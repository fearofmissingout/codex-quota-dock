package main

import (
	"testing"
	"time"

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

func TestMonitorQuotaLineCombinesFiveHourAndWeeklyUsage(t *testing.T) {
	row := profileRow{
		CompactLines: []string{
			"5h: 88.0% left, resets 19:07",
			"weekly: 24.0% left, resets Sunday",
		},
	}

	got := monitorQuotaLine(row)

	want := "5h: 88.0% left, resets 19:07  |  weekly: 24.0% left, resets Sunday"
	if got != want {
		t.Fatalf("line=%q want %q", got, want)
	}
}

func TestIntervalLabelsRoundTrip(t *testing.T) {
	for _, interval := range []time.Duration{0, time.Minute, 5 * time.Minute, 10 * time.Minute} {
		if got := intervalFromLabel(intervalLabel(interval)); got != interval {
			t.Fatalf("round trip=%s want %s", got, interval)
		}
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
