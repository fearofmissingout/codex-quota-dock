package main

import (
	"fmt"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/fearofmissingout/codex-quota-dock/internal/display"
	"github.com/fearofmissingout/codex-quota-dock/internal/localusage"
	"github.com/fearofmissingout/codex-quota-dock/internal/profile"
)

type profileRow struct {
	Profile      profile.Profile
	Status       string
	Quota        string
	LastRefresh  string
	Details      string
	CompactTitle string
	CompactLines []string
}

type switchReminderCopy struct {
	DialogTitle string
	Heading     string
	Summary     string
	Restart     string
	BackupLabel string
	BackupPath  string
	Footer      string
}

type localUsageView struct {
	Metrics  []localUsageMetric
	Profiles []localUsageProfileRow
	Sessions []localUsageSessionRow
	Warning  string
	Note     string
}

type localUsageMetric struct {
	Title  string
	Value  string
	Detail string
}

type localUsageProfileRow struct {
	Name   string
	Usage  string
	Share  string
	Detail string
}

type localUsageSessionRow struct {
	Name   string
	Usage  string
	Detail string
}

var leftPercentPattern = regexp.MustCompile(`([0-9]+(?:\.[0-9]+)?)%\s+left`)

func newProfileRow(prof profile.Profile) profileRow {
	return profileRow{
		Profile:      prof,
		Status:       display.StatusText(display.StatusNotRefreshed),
		Quota:        "-",
		LastRefresh:  "-",
		Details:      "No quota data yet. Refresh this profile to see quota details.",
		CompactTitle: "Codex",
		CompactLines: []string{"5h: not refreshed", "weekly: not refreshed"},
	}
}

func visibleMonitorRows(rows []profileRow, activeAccountID string) []profileRow {
	hasPinned := false
	for _, row := range rows {
		if row.Profile.Pinned {
			hasPinned = true
			break
		}
	}
	if !hasPinned {
		out := make([]profileRow, len(rows))
		copy(out, rows)
		return out
	}

	out := make([]profileRow, 0, len(rows))
	seen := map[string]bool{}
	for _, row := range rows {
		if row.Profile.AccountID == activeAccountID {
			out = append(out, row)
			seen[row.Profile.ID] = true
			break
		}
	}
	for _, row := range rows {
		if row.Profile.Pinned && !seen[row.Profile.ID] {
			out = append(out, row)
			seen[row.Profile.ID] = true
		}
	}
	return out
}

func selectedMonitorProfile(rows []profileRow, profileID string) (profile.Profile, bool) {
	for _, row := range rows {
		if row.Profile.ID == profileID {
			return row.Profile, true
		}
	}
	return profile.Profile{}, false
}

func selectedProfileRow(rows []profileRow, profileID string) (profileRow, bool) {
	for _, row := range rows {
		if row.Profile.ID == profileID {
			return row, true
		}
	}
	return profileRow{}, false
}

func normalizedMonitorSelection(rows []profileRow, selectedID, activeAccountID string) string {
	for _, row := range rows {
		if row.Profile.ID == selectedID {
			return selectedID
		}
	}
	for _, row := range rows {
		if row.Profile.AccountID == activeAccountID {
			return row.Profile.ID
		}
	}
	if len(rows) == 0 {
		return ""
	}
	return rows[0].Profile.ID
}

func monitorClickAction(lastClick, now time.Time) (bool, time.Time) {
	if !lastClick.IsZero() && now.Sub(lastClick) <= 450*time.Millisecond {
		return true, time.Time{}
	}
	return false, now
}

func monitorRowTitle(row profileRow, active bool) string {
	parts := []string{row.Profile.Alias}
	if active {
		parts = append(parts, "current")
	}
	if row.Profile.Pinned {
		parts = append(parts, "pinned")
	}
	if row.Status != "" && row.Status != display.StatusText(display.StatusOK) {
		parts = append(parts, row.Status)
	}
	return strings.Join(parts, "  ")
}

func monitorCompactLine(row profileRow, index int, fallback string) string {
	if index >= 0 && index < len(row.CompactLines) && strings.TrimSpace(row.CompactLines[index]) != "" {
		return row.CompactLines[index]
	}
	return fallback
}

func monitorQuotaLines(row profileRow) (string, string) {
	return monitorCompactLine(row, 0, "5h: not refreshed"), monitorCompactLine(row, 1, "weekly: not refreshed")
}

func monitorWindowHeight(rowCount int) int {
	if rowCount < 1 {
		rowCount = 1
	}
	height := 104 + rowCount*64
	if height < 150 {
		height = 150
	}
	if height > 380 {
		height = 380
	}
	return height
}

func quotaRuleProgress(line string) (float64, bool) {
	match := leftPercentPattern.FindStringSubmatch(line)
	if len(match) != 2 {
		return 0, false
	}
	value, err := strconv.ParseFloat(match[1], 64)
	if err != nil {
		return 0, false
	}
	if value < 0 {
		value = 0
	}
	if value > 100 {
		value = 100
	}
	return value, true
}

func newSwitchReminderCopy(alias, backupPath string) switchReminderCopy {
	if strings.TrimSpace(alias) == "" {
		alias = "selected profile"
	}
	return switchReminderCopy{
		DialogTitle: "Codex auth switched",
		Heading:     fmt.Sprintf("Switched to %q", alias),
		Summary:     "The active Codex auth file has been replaced. Running Codex windows may keep using the previous session until they restart.",
		Restart:     "Restart Codex to apply this account.",
		BackupLabel: "Backup saved before switching",
		BackupPath:  backupPath,
		Footer:      "You can turn off this reminder from Settings.",
	}
}

func pollingOptions() []string {
	return []string{"Off", "1 minute", "5 minutes", "10 minutes"}
}

func intervalLabel(interval time.Duration) string {
	switch interval {
	case time.Minute:
		return "1 minute"
	case 5 * time.Minute:
		return "5 minutes"
	case 10 * time.Minute:
		return "10 minutes"
	default:
		return "Off"
	}
}

func intervalFromLabel(label string) time.Duration {
	switch label {
	case "1 minute":
		return time.Minute
	case "5 minutes":
		return 5 * time.Minute
	case "10 minutes":
		return 10 * time.Minute
	default:
		return 0
	}
}

func formatLocalUsageSummary(summary localusage.Summary, profiles []profile.Profile) string {
	view := buildLocalUsageView(summary, profiles)
	lines := []string{
		"Local Codex token usage",
		"",
		"Windows",
	}
	for _, metric := range view.Metrics {
		lines = append(lines, "  "+metric.Title+": "+metric.Detail)
	}
	lines = append(lines,
		"",
		"By profile",
	)
	if len(view.Profiles) == 0 {
		lines = append(lines, "  No local token events found.")
	}
	for _, row := range view.Profiles {
		lines = append(lines, "  "+row.Name+": "+row.Detail+" ("+row.Share+")")
	}

	if len(view.Sessions) > 0 {
		lines = append(lines, "", "Recent sessions")
		for _, session := range view.Sessions {
			lines = append(lines, fmt.Sprintf("  %s  %s  %s", session.Detail, session.Name, session.Usage))
		}
	}

	if view.Warning != "" {
		lines = append(lines, "", view.Warning)
	}
	lines = append(lines, "", view.Note)
	return strings.Join(lines, "\r\n")
}

func buildLocalUsageView(summary localusage.Summary, profiles []profile.Profile) localUsageView {
	view := localUsageView{
		Metrics: []localUsageMetric{
			{Title: "Today", Value: formatInt(summary.Today.Total), Detail: formatTokenUsage(summary.Today)},
			{Title: "Last 7 days", Value: formatInt(summary.Last7Days.Total), Detail: formatTokenUsage(summary.Last7Days)},
			{Title: "Last 30 days", Value: formatInt(summary.Last30Days.Total), Detail: formatTokenUsage(summary.Last30Days)},
			{Title: "All local sessions", Value: formatInt(summary.Total.Total), Detail: formatTokenUsage(summary.Total)},
		},
		Note: "Historical profile attribution starts after this app records an auth switch. Older local usage is shown as Unknown / before tracking.",
	}

	aliases := map[string]string{"": "Unknown / before tracking"}
	for _, prof := range profiles {
		aliases[prof.ID] = prof.Alias
	}
	profileIDs := make([]string, 0, len(summary.ByProfile))
	for profileID := range summary.ByProfile {
		profileIDs = append(profileIDs, profileID)
	}
	sort.Slice(profileIDs, func(i, j int) bool {
		left := summary.ByProfile[profileIDs[i]]
		right := summary.ByProfile[profileIDs[j]]
		if left.Total != right.Total {
			return left.Total > right.Total
		}
		return localUsageAlias(aliases, profileIDs[i]) < localUsageAlias(aliases, profileIDs[j])
	})
	for _, profileID := range profileIDs {
		usage := summary.ByProfile[profileID]
		view.Profiles = append(view.Profiles, localUsageProfileRow{
			Name:   localUsageAlias(aliases, profileID),
			Usage:  formatInt(usage.Total),
			Share:  formatPercentShare(usage.Total, summary.Total.Total),
			Detail: formatTokenUsage(usage),
		})
	}

	limit := len(summary.Sessions)
	if limit > 8 {
		limit = 8
	}
	for _, session := range summary.Sessions[:limit] {
		view.Sessions = append(view.Sessions, localUsageSessionRow{
			Name:   shortSessionID(session.ID),
			Usage:  formatTokenUsage(session.Usage),
			Detail: formatLocalTime(session.LastEvent),
		})
	}
	if summary.ParseErrors > 0 {
		view.Warning = fmt.Sprintf("parse warnings: %d malformed session lines were skipped", summary.ParseErrors)
	}
	return view
}

func localUsageAlias(aliases map[string]string, profileID string) string {
	alias := aliases[profileID]
	if alias == "" {
		return profileID
	}
	return alias
}

func formatPercentShare(part, total int64) string {
	if total <= 0 {
		return "0.0%"
	}
	return fmt.Sprintf("%.1f%%", float64(part)*100/float64(total))
}

func shortSessionID(id string) string {
	if len(id) > 12 {
		return id[:12]
	}
	if id == "" {
		return "-"
	}
	return id
}

func formatTokenUsage(usage localusage.TokenUsage) string {
	return fmt.Sprintf(
		"%s total (input: %s, cached input: %s, output: %s, reasoning: %s)",
		formatInt(usage.Total),
		formatInt(usage.Input),
		formatInt(usage.CachedInput),
		formatInt(usage.Output),
		formatInt(usage.ReasoningOutput),
	)
}

func formatInt(value int64) string {
	text := fmt.Sprintf("%d", value)
	if len(text) <= 3 {
		return text
	}
	var parts []string
	for len(text) > 3 {
		parts = append([]string{text[len(text)-3:]}, parts...)
		text = text[:len(text)-3]
	}
	parts = append([]string{text}, parts...)
	return strings.Join(parts, ",")
}

func formatLocalTime(value time.Time) string {
	if value.IsZero() {
		return "-"
	}
	return value.Local().Format("2006-01-02 15:04")
}
