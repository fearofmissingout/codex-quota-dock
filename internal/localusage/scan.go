package localusage

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

type TokenUsage struct {
	Input           int64
	CachedInput     int64
	Output          int64
	ReasoningOutput int64
	Total           int64
}

type SwitchAttribution struct {
	At        time.Time
	ProfileID string
	AccountID string
	Alias     string
}

type Options struct {
	Now      time.Time
	Switches []SwitchAttribution
}

type Summary struct {
	Total       TokenUsage
	Today       TokenUsage
	Last7Days   TokenUsage
	Last30Days  TokenUsage
	ByProfile   map[string]TokenUsage
	ByDay       []DayUsage
	Sessions    []SessionSummary
	ParseErrors int
}

type DayUsage struct {
	Day   time.Time
	Usage TokenUsage
}

type SessionSummary struct {
	ID        string
	Path      string
	StartedAt time.Time
	LastEvent time.Time
	Usage     TokenUsage
}

type logEvent struct {
	Timestamp string          `json:"timestamp"`
	Type      string          `json:"type"`
	Payload   json.RawMessage `json:"payload"`
}

type sessionPayload struct {
	ID string `json:"id"`
}

type tokenPayload struct {
	Type string `json:"type"`
	Info struct {
		LastTokenUsage tokenUsagePayload `json:"last_token_usage"`
	} `json:"info"`
}

type tokenUsagePayload struct {
	InputTokens           int64 `json:"input_tokens"`
	CachedInputTokens     int64 `json:"cached_input_tokens"`
	OutputTokens          int64 `json:"output_tokens"`
	ReasoningOutputTokens int64 `json:"reasoning_output_tokens"`
	TotalTokens           int64 `json:"total_tokens"`
}

func Scan(root string, options Options) (Summary, error) {
	if options.Now.IsZero() {
		options.Now = time.Now()
	}
	switches := append([]SwitchAttribution(nil), options.Switches...)
	sort.Slice(switches, func(i, j int) bool {
		return switches[i].At.Before(switches[j].At)
	})

	paths, err := sessionFiles(root)
	if err != nil {
		return Summary{}, err
	}

	summary := Summary{
		ByProfile: map[string]TokenUsage{},
	}
	byDay := map[string]TokenUsage{}
	for _, path := range paths {
		session, parseErrors, err := scanFile(path, switches, options.Now, &summary, byDay)
		if err != nil {
			return Summary{}, err
		}
		summary.ParseErrors += parseErrors
		if session.ID == "" {
			session.ID = strings.TrimSuffix(filepath.Base(path), filepath.Ext(path))
		}
		if session.Usage.Total > 0 {
			summary.Sessions = append(summary.Sessions, session)
		}
	}
	summary.ByDay = sortedDays(byDay)
	sort.Slice(summary.Sessions, func(i, j int) bool {
		return summary.Sessions[i].LastEvent.After(summary.Sessions[j].LastEvent)
	})
	return summary, nil
}

func sessionFiles(root string) ([]string, error) {
	var paths []string
	for _, rel := range []string{"sessions", "archived_sessions"} {
		dir := filepath.Join(root, rel)
		if _, err := os.Stat(dir); err != nil {
			if os.IsNotExist(err) {
				continue
			}
			return nil, fmt.Errorf("stat %s: %w", dir, err)
		}
		err := filepath.WalkDir(dir, func(path string, entry os.DirEntry, err error) error {
			if err != nil {
				return err
			}
			if entry.IsDir() || !strings.EqualFold(filepath.Ext(path), ".jsonl") {
				return nil
			}
			paths = append(paths, path)
			return nil
		})
		if err != nil {
			return nil, fmt.Errorf("scan %s: %w", dir, err)
		}
	}
	sort.Strings(paths)
	return paths, nil
}

func scanFile(path string, switches []SwitchAttribution, now time.Time, summary *Summary, byDay map[string]TokenUsage) (SessionSummary, int, error) {
	file, err := os.Open(path)
	if err != nil {
		return SessionSummary{}, 0, fmt.Errorf("open session log: %w", err)
	}
	defer file.Close()

	var session SessionSummary
	session.Path = path
	parseErrors := 0
	scanner := bufio.NewScanner(file)
	scanner.Buffer(make([]byte, 64*1024), 16*1024*1024)
	for scanner.Scan() {
		var event logEvent
		if err := json.Unmarshal(scanner.Bytes(), &event); err != nil {
			parseErrors++
			continue
		}
		at, err := time.Parse(time.RFC3339Nano, event.Timestamp)
		if err != nil {
			parseErrors++
			continue
		}
		if event.Type == "session_meta" {
			var payload sessionPayload
			if err := json.Unmarshal(event.Payload, &payload); err == nil && payload.ID != "" {
				session.ID = payload.ID
				session.StartedAt = at
			}
			continue
		}
		var payload tokenPayload
		if err := json.Unmarshal(event.Payload, &payload); err != nil || payload.Type != "token_count" {
			continue
		}
		usage := mapUsage(payload.Info.LastTokenUsage)
		if usage.Total == 0 && usage.Input == 0 && usage.Output == 0 {
			continue
		}
		session.Usage = session.Usage.Add(usage)
		session.LastEvent = at
		summary.Total = summary.Total.Add(usage)
		addWindows(summary, now, at, usage)
		key := profileAt(switches, at)
		summary.ByProfile[key] = summary.ByProfile[key].Add(usage)
		dayKey := at.Local().Format("2006-01-02")
		byDay[dayKey] = byDay[dayKey].Add(usage)
	}
	if err := scanner.Err(); err != nil {
		return SessionSummary{}, parseErrors, fmt.Errorf("read session log: %w", err)
	}
	return session, parseErrors, nil
}

func mapUsage(usage tokenUsagePayload) TokenUsage {
	total := usage.TotalTokens
	if total == 0 {
		total = usage.InputTokens + usage.OutputTokens
	}
	return TokenUsage{
		Input:           usage.InputTokens,
		CachedInput:     usage.CachedInputTokens,
		Output:          usage.OutputTokens,
		ReasoningOutput: usage.ReasoningOutputTokens,
		Total:           total,
	}
}

func addWindows(summary *Summary, now, at time.Time, usage TokenUsage) {
	if sameLocalDay(now, at) {
		summary.Today = summary.Today.Add(usage)
	}
	if !at.Before(now.AddDate(0, 0, -7)) {
		summary.Last7Days = summary.Last7Days.Add(usage)
	}
	if !at.Before(now.AddDate(0, 0, -30)) {
		summary.Last30Days = summary.Last30Days.Add(usage)
	}
}

func sameLocalDay(a, b time.Time) bool {
	ay, am, ad := a.Local().Date()
	by, bm, bd := b.Local().Date()
	return ay == by && am == bm && ad == bd
}

func profileAt(switches []SwitchAttribution, at time.Time) string {
	profileID := ""
	for _, item := range switches {
		if item.At.After(at) {
			break
		}
		profileID = item.ProfileID
	}
	return profileID
}

func sortedDays(byDay map[string]TokenUsage) []DayUsage {
	keys := make([]string, 0, len(byDay))
	for key := range byDay {
		keys = append(keys, key)
	}
	sort.Strings(keys)
	days := make([]DayUsage, 0, len(keys))
	for _, key := range keys {
		day, _ := time.ParseInLocation("2006-01-02", key, time.Local)
		days = append(days, DayUsage{Day: day, Usage: byDay[key]})
	}
	return days
}

func (u TokenUsage) Add(other TokenUsage) TokenUsage {
	return TokenUsage{
		Input:           u.Input + other.Input,
		CachedInput:     u.CachedInput + other.CachedInput,
		Output:          u.Output + other.Output,
		ReasoningOutput: u.ReasoningOutput + other.ReasoningOutput,
		Total:           u.Total + other.Total,
	}
}
