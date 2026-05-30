package main

import (
	"strings"
	"testing"
	"time"

	"github.com/fearofmissingout/codex-quota-dock/internal/localusage"
	"github.com/fearofmissingout/codex-quota-dock/internal/profile"
)

func TestVisibleMonitorRowsShowsActiveAndPinnedProfiles(t *testing.T) {
	rows := []profileRow{
		{Profile: testProfile("pro", "acc_pro", true)},
		{Profile: testProfile("company", "acc_company", false)},
		{Profile: testProfile("spare", "acc_spare", false)},
	}

	got := visibleMonitorRows(rows, "acc_company")

	if len(got) != 2 {
		t.Fatalf("rows=%+v want active and pinned", got)
	}
	if got[0].Profile.Alias != "company" || got[1].Profile.Alias != "pro" {
		t.Fatalf("rows=%+v want company then pro", got)
	}
}

func TestVisibleMonitorRowsFallsBackToAllProfilesWithoutPins(t *testing.T) {
	rows := []profileRow{
		{Profile: testProfile("company", "acc_company", false)},
		{Profile: testProfile("pro", "acc_pro", false)},
	}

	got := visibleMonitorRows(rows, "acc_company")

	if len(got) != 2 {
		t.Fatalf("rows=%+v want all profiles", got)
	}
}

func TestSelectedMonitorProfileReturnsSelectedProfile(t *testing.T) {
	rows := []profileRow{
		{Profile: testProfile("company", "acc_company", false)},
		{Profile: testProfile("pro", "acc_pro", false)},
	}

	got, ok := selectedMonitorProfile(rows, "pro")

	if !ok {
		t.Fatal("selectedMonitorProfile returned false")
	}
	if got.Alias != "pro" {
		t.Fatalf("profile=%+v want pro", got)
	}
}

func TestSelectedMonitorProfileRejectsMissingSelection(t *testing.T) {
	rows := []profileRow{
		{Profile: testProfile("company", "acc_company", false)},
	}

	if _, ok := selectedMonitorProfile(rows, "missing"); ok {
		t.Fatal("selectedMonitorProfile returned true for missing profile")
	}
}

func TestNormalizedMonitorSelectionPrefersExistingSelection(t *testing.T) {
	rows := []profileRow{
		{Profile: testProfile("company", "acc_company", false)},
		{Profile: testProfile("pro", "acc_pro", false)},
	}

	got := normalizedMonitorSelection(rows, "pro", "acc_company")

	if got != "pro" {
		t.Fatalf("selection=%q want existing pro", got)
	}
}

func TestNormalizedMonitorSelectionFallsBackToActiveAccount(t *testing.T) {
	rows := []profileRow{
		{Profile: testProfile("company", "acc_company", false)},
		{Profile: testProfile("pro", "acc_pro", false)},
	}

	got := normalizedMonitorSelection(rows, "missing", "acc_company")

	if got != "company" {
		t.Fatalf("selection=%q want active company", got)
	}
}

func TestMonitorClickActionOpensOnDoubleClick(t *testing.T) {
	first := time.Date(2026, 5, 29, 10, 0, 0, 0, time.UTC)
	second := first.Add(300 * time.Millisecond)

	open, nextClick := monitorClickAction(time.Time{}, first)
	if open {
		t.Fatal("first click opened details")
	}
	if !nextClick.Equal(first) {
		t.Fatalf("nextClick=%s want first click time", nextClick)
	}

	open, nextClick = monitorClickAction(nextClick, second)
	if !open {
		t.Fatal("second click did not open details")
	}
	if !nextClick.IsZero() {
		t.Fatalf("nextClick=%s want reset after double click", nextClick)
	}
}

func TestMonitorQuotaLinesSplitFiveHourAndWeeklyUsage(t *testing.T) {
	row := profileRow{
		CompactLines: []string{
			"5h: 88.0% left, resets 19:07",
			"weekly: 24.0% left, resets Sunday",
		},
	}

	gotFiveHour, gotWeekly := monitorQuotaLines(row)

	if gotFiveHour != "5h: 88.0% left, resets 19:07" {
		t.Fatalf("fiveHour=%q", gotFiveHour)
	}
	if gotWeekly != "weekly: 24.0% left, resets Sunday" {
		t.Fatalf("weekly=%q", gotWeekly)
	}
}

func TestMonitorWindowHeightFitsTwoThreeLineProfiles(t *testing.T) {
	if got := monitorWindowHeight(2); got != 232 {
		t.Fatalf("height=%d want 232", got)
	}
}

func TestQuotaRuleProgressParsesRemainingPercent(t *testing.T) {
	got, ok := quotaRuleProgress("5h: 18.5% left, resets 19:07")
	if !ok {
		t.Fatal("quotaRuleProgress returned false")
	}
	if got != 18.5 {
		t.Fatalf("progress=%f want 18.5", got)
	}
}

func TestQuotaRuleProgressRejectsUnavailableLine(t *testing.T) {
	if _, ok := quotaRuleProgress("weekly: quota unavailable"); ok {
		t.Fatal("quotaRuleProgress returned true for unavailable line")
	}
}

func TestLowQuotaAlertsTriggerOnceUntilRecovered(t *testing.T) {
	row := profileRow{
		Profile: testProfile("company", "acc_company", false),
		CompactLines: []string{
			"5h: 18.5% left, resets 19:07",
			"weekly: 24.0% left, resets Sunday",
		},
	}
	state := map[string]bool{}

	alerts, next := evaluateLowQuotaAlerts(row, 20, state)

	if len(alerts) != 1 {
		t.Fatalf("alerts=%+v want one low 5h alert", alerts)
	}
	if alerts[0].ProfileAlias != "company" || alerts[0].Window != "5h" || alerts[0].PercentLeft != 18.5 {
		t.Fatalf("alert=%+v want company 5h at 18.5", alerts[0])
	}
	if !next[alerts[0].Key] {
		t.Fatalf("state=%+v want alert key marked active", next)
	}

	alerts, next = evaluateLowQuotaAlerts(row, 20, next)
	if len(alerts) != 0 {
		t.Fatalf("alerts=%+v want duplicate low alert suppressed", alerts)
	}

	recovered := row
	recovered.CompactLines = []string{"5h: 40.0% left, resets 20:00"}
	alerts, next = evaluateLowQuotaAlerts(recovered, 20, next)
	if len(alerts) != 0 {
		t.Fatalf("alerts=%+v want no alert after recovery", alerts)
	}
	if next["company\x005h"] {
		t.Fatalf("state=%+v want 5h recovery to clear active alert", next)
	}

	alerts, next = evaluateLowQuotaAlerts(row, 20, next)
	if len(alerts) != 1 {
		t.Fatalf("alerts=%+v want low alert after recovery", alerts)
	}
}

func TestLowQuotaAlertsRespectOffThreshold(t *testing.T) {
	row := profileRow{
		Profile:      testProfile("company", "acc_company", false),
		CompactLines: []string{"5h: 2.0% left, resets 19:07"},
	}

	alerts, next := evaluateLowQuotaAlerts(row, 0, map[string]bool{"company\x005h": true})

	if len(alerts) != 0 {
		t.Fatalf("alerts=%+v want alerts disabled", alerts)
	}
	if len(next) != 0 {
		t.Fatalf("state=%+v want disabled alerts to clear active state", next)
	}
}

func TestSwitchReminderCopyEmphasizesRestartAndBackup(t *testing.T) {
	got := newSwitchReminderCopy("company", `C:\CodexQuotaDock\backups\auth.json`)

	if got.DialogTitle != "Codex auth switched" {
		t.Fatalf("DialogTitle=%q", got.DialogTitle)
	}
	if !strings.Contains(got.Heading, "company") {
		t.Fatalf("Heading=%q, want alias", got.Heading)
	}
	if !strings.Contains(got.Restart, "Restart Codex") {
		t.Fatalf("Restart=%q, want restart instruction", got.Restart)
	}
	if !strings.Contains(got.BackupLabel, "Backup") {
		t.Fatalf("BackupLabel=%q, want backup label", got.BackupLabel)
	}
	if got.BackupPath == "" {
		t.Fatal("BackupPath is empty")
	}
}

func TestIntervalLabelsRoundTrip(t *testing.T) {
	for _, interval := range []time.Duration{0, time.Minute, 5 * time.Minute, 10 * time.Minute} {
		if got := intervalFromLabel(intervalLabel(interval)); got != interval {
			t.Fatalf("round trip=%s want %s", got, interval)
		}
	}
}

func TestQuotaAlertThresholdLabelsRoundTrip(t *testing.T) {
	for _, threshold := range []int{0, 5, 10, 20, 30} {
		if got := quotaAlertThresholdFromLabel(quotaAlertThresholdLabel(threshold)); got != threshold {
			t.Fatalf("round trip=%d want %d", got, threshold)
		}
	}
}

func TestFormatLocalUsageSummaryIncludesWindowsAndProfiles(t *testing.T) {
	summary := localusage.Summary{
		Today:      localusage.TokenUsage{Input: 10, CachedInput: 3, Output: 5, ReasoningOutput: 2, Total: 15},
		Last7Days:  localusage.TokenUsage{Total: 40},
		Last30Days: localusage.TokenUsage{Total: 80},
		Total:      localusage.TokenUsage{Total: 100},
		ByProfile: map[string]localusage.TokenUsage{
			"company": {Total: 90},
			"":        {Total: 10},
		},
		ParseErrors: 2,
	}
	profiles := []profile.Profile{testProfile("company", "acc_company", false)}

	got := formatLocalUsageSummary(summary, profiles)

	for _, want := range []string{"Today", "Last 7 days", "company", "Unknown / before tracking", "parse warnings: 2", "cached input: 3"} {
		if !strings.Contains(got, want) {
			t.Fatalf("summary missing %q:\n%s", want, got)
		}
	}
}

func TestBuildLocalUsageViewSummarizesUsageForDetailPanel(t *testing.T) {
	now := time.Date(2026, 5, 29, 9, 30, 0, 0, time.Local)
	summary := localusage.Summary{
		Now:        now,
		Today:      localusage.TokenUsage{Total: 15},
		Last7Days:  localusage.TokenUsage{Total: 40},
		Last30Days: localusage.TokenUsage{Total: 80},
		Total:      localusage.TokenUsage{Input: 60, CachedInput: 10, Output: 35, ReasoningOutput: 5, Total: 100},
		ByProfile: map[string]localusage.TokenUsage{
			"pro":     {Total: 20, Input: 12, Output: 8},
			"company": {Total: 70, Input: 40, Output: 30},
			"":        {Total: 10},
		},
		Sessions: []localusage.SessionSummary{
			{ID: "session-newest", LastEvent: now, Usage: localusage.TokenUsage{Total: 30}},
			{ID: "session-older", LastEvent: now.Add(-time.Hour), Usage: localusage.TokenUsage{Total: 20}},
		},
		ByDay: []localusage.DayUsage{
			{Day: now.AddDate(0, 0, -2), Usage: localusage.TokenUsage{Total: 10}},
			{Day: now.AddDate(0, 0, -1), Usage: localusage.TokenUsage{Total: 40}},
			{Day: now, Usage: localusage.TokenUsage{Total: 20}},
		},
		ParseErrors: 2,
	}
	profiles := []profile.Profile{
		testProfile("company", "acc_company", false),
		testProfile("pro", "acc_pro", false),
	}

	got := buildLocalUsageView(summary, profiles)

	if len(got.Metrics) != 4 {
		t.Fatalf("metrics=%+v want 4 windows", got.Metrics)
	}
	if got.Metrics[0].Title != "Today" || got.Metrics[0].Value != "15" {
		t.Fatalf("first metric=%+v", got.Metrics[0])
	}
	if len(got.Profiles) != 3 {
		t.Fatalf("profiles=%+v want company, pro, unknown", got.Profiles)
	}
	if got.Profiles[0].Name != "company" || !strings.Contains(got.Profiles[0].Share, "70.0%") {
		t.Fatalf("first profile=%+v want company sorted by usage share", got.Profiles[0])
	}
	if got.Profiles[2].Name != "Unknown / before tracking" {
		t.Fatalf("last profile=%+v want unknown attribution last by usage", got.Profiles[2])
	}
	if len(got.Sessions) != 2 || got.Sessions[0].Name != "session-newe" {
		t.Fatalf("sessions=%+v want recent sessions with trimmed ids", got.Sessions)
	}
	if !strings.Contains(got.Warning, "2 malformed") {
		t.Fatalf("warning=%q want parse warning", got.Warning)
	}
	if !strings.Contains(got.Note, "Local usage") {
		t.Fatalf("note=%q want local usage scope note", got.Note)
	}
	if len(got.Daily) != 7 {
		t.Fatalf("daily=%+v want 7 day window", got.Daily)
	}
	if got.Daily[5].Value != "40" || got.Daily[5].Ratio != 1 {
		t.Fatalf("daily max bar=%+v want prior day as max", got.Daily[5])
	}
	if len(got.Overall) != 4 {
		t.Fatalf("overall=%+v want four token mix segments", got.Overall)
	}
	if got.Overall[0].Name != "Input" || got.Overall[1].Name != "Cached" {
		t.Fatalf("overall=%+v want input/cached split first", got.Overall)
	}
	if !strings.Contains(got.Attribution, "switch history") {
		t.Fatalf("attribution=%q want switch-history explanation", got.Attribution)
	}
}

func TestBuildLocalUsageViewUsesCompactUIText(t *testing.T) {
	summary := localusage.Summary{
		Today: localusage.TokenUsage{
			Input: 900, CachedInput: 300, Output: 120, ReasoningOutput: 40, Total: 1020,
		},
		Total: localusage.TokenUsage{
			Input: 900, CachedInput: 300, Output: 120, ReasoningOutput: 40, Total: 1020,
		},
		ByProfile: map[string]localusage.TokenUsage{
			"company": {Input: 900, CachedInput: 300, Output: 120, ReasoningOutput: 40, Total: 1020},
		},
		Sessions: []localusage.SessionSummary{
			{ID: "session-one", LastEvent: time.Date(2026, 5, 29, 10, 0, 0, 0, time.Local), Usage: localusage.TokenUsage{Input: 100, Output: 20, Total: 120}},
		},
	}
	profiles := []profile.Profile{testProfile("company", "acc_company", false)}

	got := buildLocalUsageView(summary, profiles)

	for _, text := range []string{got.Metrics[0].Detail, got.Profiles[0].Detail, got.Sessions[0].Usage} {
		if strings.Contains(text, "cached input:") || strings.Contains(text, "reasoning:") {
			t.Fatalf("ui text is too verbose: %q", text)
		}
		if !strings.Contains(text, "in ") || !strings.Contains(text, "out ") {
			t.Fatalf("ui text missing compact input/output summary: %q", text)
		}
	}
	if got.Profiles[0].Usage != "1,020 tokens" {
		t.Fatalf("profile usage=%q want token unit", got.Profiles[0].Usage)
	}
}

func testProfile(alias, accountID string, pinned bool) profile.Profile {
	return profile.Profile{
		ID:        alias,
		Alias:     alias,
		AccountID: accountID,
		Pinned:    pinned,
	}
}
