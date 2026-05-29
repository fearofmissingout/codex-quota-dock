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
