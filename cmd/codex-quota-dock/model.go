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
	Metrics     []localUsageMetric
	Profiles    []localUsageProfileRow
	Sessions    []localUsageSessionRow
	Daily       []localUsageDailyBar
	Overall     []localUsageOverallSegment
	Attribution string
	Warning     string
	Note        string
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

type localUsageDailyBar struct {
	Label   string
	Value   string
	Usage   string
	Ratio   float64
	IsToday bool
}

type localUsageOverallSegment struct {
	Name  string
	Value string
	Share string
	Ratio float64
}

type lowQuotaAlert struct {
	Key          string
	ProfileAlias string
	Window       string
	PercentLeft  float64
	Threshold    int
	Severity     quotaAlertSeverity
	Line         string
	Title        string
	Body         string
}

type quotaAlertSeverity string

const (
	quotaAlertWarning   quotaAlertSeverity = "warning"
	quotaAlertCritical  quotaAlertSeverity = "critical"
	quotaAlertExhausted quotaAlertSeverity = "exhausted"
)

type quotaAlertThresholds struct {
	FiveHour int
	Weekly   int
}

type lowQuotaNotification struct {
	Title string
	Body  string
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

func evaluateLowQuotaAlerts(row profileRow, thresholds quotaAlertThresholds, active map[string]bool) ([]lowQuotaAlert, map[string]bool) {
	next := map[string]bool{}
	for key, value := range active {
		next[key] = value
	}
	var alerts []lowQuotaAlert
	for _, line := range row.CompactLines {
		window := quotaLineWindow(line)
		if window == "" {
			continue
		}
		threshold := thresholds.forWindow(window)
		baseKey := lowQuotaAlertBaseKey(row.Profile.ID, window)
		percent, ok := quotaRuleProgress(line)
		if !ok {
			clearLowQuotaAlertState(next, baseKey)
			continue
		}
		if threshold <= 0 {
			clearLowQuotaAlertState(next, baseKey)
			continue
		}
		if percent > float64(threshold) {
			clearLowQuotaAlertState(next, baseKey)
			continue
		}
		severity := lowQuotaSeverity(percent)
		if hasLowQuotaSeverity(next, baseKey, severity) {
			continue
		}
		key := lowQuotaAlertKey(baseKey, severity)
		alert := lowQuotaAlert{
			Key:          key,
			ProfileAlias: row.Profile.Alias,
			Window:       window,
			PercentLeft:  percent,
			Threshold:    threshold,
			Severity:     severity,
			Line:         line,
			Title:        fmt.Sprintf("%s %s quota %s", row.Profile.Alias, window, severity),
			Body:         fmt.Sprintf("%s %s: %.1f%% left. %s", window, severity, percent, line),
		}
		alerts = append(alerts, alert)
		next[key] = true
	}
	return alerts, next
}

func lowQuotaSeverity(percent float64) quotaAlertSeverity {
	if percent <= 0 {
		return quotaAlertExhausted
	}
	if percent <= 5 {
		return quotaAlertCritical
	}
	return quotaAlertWarning
}

func lowQuotaSeverityRank(severity quotaAlertSeverity) int {
	switch severity {
	case quotaAlertExhausted:
		return 3
	case quotaAlertCritical:
		return 2
	case quotaAlertWarning:
		return 1
	default:
		return 0
	}
}

func lowQuotaAlertBaseKey(profileID, window string) string {
	return profileID + "\x00" + window
}

func lowQuotaAlertKey(baseKey string, severity quotaAlertSeverity) string {
	return baseKey + "\x00" + string(severity)
}

func clearLowQuotaAlertState(active map[string]bool, baseKey string) {
	prefix := baseKey + "\x00"
	for key := range active {
		if strings.HasPrefix(key, prefix) {
			delete(active, key)
		}
	}
}

func hasLowQuotaSeverity(active map[string]bool, baseKey string, severity quotaAlertSeverity) bool {
	for _, existing := range []quotaAlertSeverity{quotaAlertWarning, quotaAlertCritical, quotaAlertExhausted} {
		if lowQuotaSeverityRank(existing) >= lowQuotaSeverityRank(severity) && active[lowQuotaAlertKey(baseKey, existing)] {
			return true
		}
	}
	return false
}

func (q quotaAlertThresholds) forWindow(window string) int {
	switch strings.ToLower(strings.TrimSpace(window)) {
	case "5h":
		return q.FiveHour
	case "weekly":
		return q.Weekly
	default:
		return 0
	}
}

func lowQuotaNotificationFor(alerts []lowQuotaAlert) (lowQuotaNotification, bool) {
	if len(alerts) == 0 {
		return lowQuotaNotification{}, false
	}
	sort.SliceStable(alerts, func(i, j int) bool {
		left := lowQuotaSeverityRank(alerts[i].Severity)
		right := lowQuotaSeverityRank(alerts[j].Severity)
		if left != right {
			return left > right
		}
		return alerts[i].Window < alerts[j].Window
	})
	if len(alerts) == 1 {
		alert := alerts[0]
		return lowQuotaNotification{
			Title: alert.Title,
			Body:  alert.Body,
		}, true
	}
	alias := alerts[0].ProfileAlias
	parts := make([]string, 0, len(alerts))
	for _, alert := range alerts {
		parts = append(parts, fmt.Sprintf("%s %s at %.1f%% left", alert.Window, alert.Severity, alert.PercentLeft))
	}
	return lowQuotaNotification{
		Title: fmt.Sprintf("%s has %d quota windows low", alias, len(alerts)),
		Body:  strings.Join(parts, "; "),
	}, true
}

func quotaLineWindow(line string) string {
	before, _, ok := strings.Cut(line, ":")
	if !ok {
		return ""
	}
	return strings.TrimSpace(before)
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

func quotaAlertThresholdOptions() []string {
	return []string{"Off", "5%", "10%", "15%", "20%", "30%", "40%"}
}

func quotaAlertThresholdLabel(threshold int) string {
	switch threshold {
	case 5:
		return "5%"
	case 10:
		return "10%"
	case 15:
		return "15%"
	case 20:
		return "20%"
	case 30:
		return "30%"
	case 40:
		return "40%"
	default:
		return "Off"
	}
}

func quotaAlertThresholdFromLabel(label string) int {
	switch label {
	case "5%":
		return 5
	case "10%":
		return 10
	case "15%":
		return 15
	case "20%":
		return 20
	case "30%":
		return 30
	case "40%":
		return 40
	default:
		return 0
	}
}

func quotaAlertThresholdSummary(thresholds quotaAlertThresholds) string {
	return fmt.Sprintf("5h %s, weekly %s", quotaAlertThresholdLabel(thresholds.FiveHour), quotaAlertThresholdLabel(thresholds.Weekly))
}

func formatLocalUsageSummary(summary localusage.Summary, profiles []profile.Profile) string {
	view := buildLocalUsageView(summary, profiles)
	aliases := map[string]string{"": "Unknown / before tracking"}
	for profileID, alias := range summary.ProfileAliases {
		if profileID != "" && alias != "" {
			aliases[profileID] = alias
		}
	}
	for _, prof := range profiles {
		aliases[prof.ID] = prof.Alias
	}
	lines := []string{
		"Local Codex token usage",
		"",
		"Windows",
		"  Today: " + formatTokenUsage(summary.Today),
		"  Last 7 days: " + formatTokenUsage(summary.Last7Days),
		"  Last 30 days: " + formatTokenUsage(summary.Last30Days),
		"  All local sessions: " + formatTokenUsage(summary.Total),
		"",
		"By profile",
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
	if len(profileIDs) == 0 {
		lines = append(lines, "  No local token events found.")
	}
	for _, profileID := range profileIDs {
		usage := summary.ByProfile[profileID]
		lines = append(lines, "  "+localUsageAlias(aliases, profileID)+": "+formatTokenUsage(usage)+" ("+formatPercentShare(usage.Total, summary.Total.Total)+")")
	}

	if len(summary.Sessions) > 0 {
		lines = append(lines, "", "Recent sessions")
		limit := len(summary.Sessions)
		if limit > 8 {
			limit = 8
		}
		for _, session := range summary.Sessions[:limit] {
			lines = append(lines, fmt.Sprintf("  %s  %s  %s", formatLocalTime(session.LastEvent), shortSessionID(session.ID), formatTokenUsage(session.Usage)))
		}
	}

	if summary.ParseErrors > 0 {
		lines = append(lines, "", fmt.Sprintf("parse warnings: %d malformed session lines were skipped", summary.ParseErrors))
	}
	lines = append(lines, "", view.Attribution, view.Note)
	return strings.Join(lines, "\r\n")
}

func buildLocalUsageView(summary localusage.Summary, profiles []profile.Profile) localUsageView {
	view := localUsageView{
		Metrics: []localUsageMetric{
			{Title: "Today", Value: formatInt(summary.Today.Total), Detail: formatTokenUsageCompact(summary.Today)},
			{Title: "Last 7 days", Value: formatInt(summary.Last7Days.Total), Detail: formatTokenUsageCompact(summary.Last7Days)},
			{Title: "Last 30 days", Value: formatInt(summary.Last30Days.Total), Detail: formatTokenUsageCompact(summary.Last30Days)},
			{Title: "All local sessions", Value: formatInt(summary.Total.Total), Detail: formatTokenUsageCompact(summary.Total)},
		},
		Attribution: "Account attribution uses this app's auth switch history. Codex session logs do not include a stable account id, so older local usage stays Unknown / before tracking.",
		Note:        "Website quota is account-wide across devices. Local usage here only counts token events written by Codex on this machine.",
	}

	aliases := map[string]string{"": "Unknown / before tracking"}
	for profileID, alias := range summary.ProfileAliases {
		if profileID != "" && alias != "" {
			aliases[profileID] = alias
		}
	}
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
			Usage:  formatTokenCount(usage.Total),
			Share:  formatPercentShare(usage.Total, summary.Total.Total),
			Detail: formatTokenUsageCompact(usage),
		})
	}

	limit := len(summary.Sessions)
	if limit > 8 {
		limit = 8
	}
	for _, session := range summary.Sessions[:limit] {
		view.Sessions = append(view.Sessions, localUsageSessionRow{
			Name:   shortSessionID(session.ID),
			Usage:  formatTokenUsageCompact(session.Usage),
			Detail: formatLocalTime(session.LastEvent),
		})
	}
	if summary.ParseErrors > 0 {
		view.Warning = fmt.Sprintf("parse warnings: %d malformed session lines were skipped", summary.ParseErrors)
	}
	view.Daily = buildDailyUsageBars(summary)
	view.Overall = buildOverallSegments(summary.Total)
	return view
}

func buildDailyUsageBars(summary localusage.Summary) []localUsageDailyBar {
	now := summary.Now
	if now.IsZero() {
		now = time.Now()
		if len(summary.ByDay) > 0 {
			now = summary.ByDay[len(summary.ByDay)-1].Day
		}
	}
	byDay := map[string]localusage.TokenUsage{}
	for _, day := range summary.ByDay {
		byDay[day.Day.Local().Format("2006-01-02")] = day.Usage
	}
	const days = 7
	bars := make([]localUsageDailyBar, 0, days)
	maxTotal := int64(0)
	start := dayStart(now).AddDate(0, 0, -(days - 1))
	for i := 0; i < days; i++ {
		day := start.AddDate(0, 0, i)
		usage := byDay[day.Format("2006-01-02")]
		if usage.Total > maxTotal {
			maxTotal = usage.Total
		}
		bars = append(bars, localUsageDailyBar{
			Label:   day.Format("01/02"),
			Value:   formatInt(usage.Total),
			Usage:   formatTokenUsage(usage),
			IsToday: sameDisplayDay(day, now),
		})
	}
	for i := range bars {
		if maxTotal > 0 {
			day := start.AddDate(0, 0, i)
			usage := byDay[day.Format("2006-01-02")]
			bars[i].Ratio = float64(usage.Total) / float64(maxTotal)
		}
	}
	return bars
}

func buildOverallSegments(usage localusage.TokenUsage) []localUsageOverallSegment {
	uncachedInput := usage.Input - usage.CachedInput
	if uncachedInput < 0 {
		uncachedInput = 0
	}
	parts := []struct {
		name  string
		value int64
	}{
		{name: "Input", value: uncachedInput},
		{name: "Cached", value: usage.CachedInput},
		{name: "Output", value: usage.Output},
		{name: "Reasoning", value: usage.ReasoningOutput},
	}
	total := int64(0)
	for _, part := range parts {
		total += part.value
	}
	out := make([]localUsageOverallSegment, 0, len(parts))
	for _, part := range parts {
		ratio := 0.0
		if total > 0 {
			ratio = float64(part.value) / float64(total)
		}
		out = append(out, localUsageOverallSegment{
			Name:  part.name,
			Value: formatInt(part.value),
			Share: formatPercentShare(part.value, total),
			Ratio: ratio,
		})
	}
	return out
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

func dayStart(value time.Time) time.Time {
	local := value.Local()
	year, month, day := local.Date()
	return time.Date(year, month, day, 0, 0, 0, 0, local.Location())
}

func sameDisplayDay(a, b time.Time) bool {
	ay, am, ad := a.Local().Date()
	by, bm, bd := b.Local().Date()
	return ay == by && am == bm && ad == bd
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

func formatTokenUsageCompact(usage localusage.TokenUsage) string {
	return fmt.Sprintf(
		"in %s / cached %s / out %s / reason %s",
		formatInt(usage.Input),
		formatInt(usage.CachedInput),
		formatInt(usage.Output),
		formatInt(usage.ReasoningOutput),
	)
}

func formatTokenCount(value int64) string {
	return formatInt(value) + " tokens"
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
