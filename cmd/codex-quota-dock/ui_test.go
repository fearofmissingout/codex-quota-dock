package main

import (
	"testing"
	"time"

	"github.com/fearofmissingout/codex-quota-dock/internal/profile"
)

func TestMonitorClickActionOpensOnSecondClickWithinThreshold(t *testing.T) {
	first := time.Date(2026, 5, 29, 10, 0, 0, 0, time.UTC)
	open, next := monitorClickAction(time.Time{}, first)
	if open {
		t.Fatal("first click should not open details")
	}
	if !next.Equal(first) {
		t.Fatalf("next=%v want %v", next, first)
	}

	open, next = monitorClickAction(next, first.Add(250*time.Millisecond))
	if !open {
		t.Fatal("second click within threshold should open details")
	}
	if !next.IsZero() {
		t.Fatalf("next=%v want zero after opening", next)
	}
}

func TestMonitorClickActionDoesNotOpenAfterThreshold(t *testing.T) {
	first := time.Date(2026, 5, 29, 10, 0, 0, 0, time.UTC)
	open, next := monitorClickAction(first, first.Add(600*time.Millisecond))
	if open {
		t.Fatal("slow second click should not open details")
	}
	if !next.Equal(first.Add(600 * time.Millisecond)) {
		t.Fatalf("next=%v want slow click time", next)
	}
}

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

func testProfile(alias, accountID string, pinned bool) profile.Profile {
	return profile.Profile{
		ID:        alias,
		Alias:     alias,
		AccountID: accountID,
		Pinned:    pinned,
	}
}
