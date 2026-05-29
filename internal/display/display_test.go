package display_test

import (
	"strings"
	"testing"

	"github.com/fearofmissingout/codex-quota-dock/internal/display"
	"github.com/fearofmissingout/codex-quota-dock/internal/quota"
)

func TestFormatSnapshotsBuildsCompactAndDetails(t *testing.T) {
	snapshots := []quota.Snapshot{
		{
			LimitID:  "codex",
			PlanType: "pro",
			Primary: &quota.Window{
				UsedPercent:   40,
				WindowMinutes: 300,
				ResetsAt:      1780026000,
			},
			Secondary: &quota.Window{
				UsedPercent:   75,
				WindowMinutes: 10080,
				ResetsAt:      1780630800,
			},
		},
	}

	formatted := display.FormatSnapshots(snapshots)

	if formatted.CompactTitle != "Codex" {
		t.Fatalf("CompactTitle=%q want Codex", formatted.CompactTitle)
	}
	if !strings.Contains(formatted.CompactLines[0], "5h") || !strings.Contains(formatted.CompactLines[0], "60.0% left") {
		t.Fatalf("first compact line=%q", formatted.CompactLines[0])
	}
	if !strings.Contains(formatted.CompactLines[1], "weekly") || !strings.Contains(formatted.CompactLines[1], "25.0% left") {
		t.Fatalf("second compact line=%q", formatted.CompactLines[1])
	}
	if !strings.Contains(formatted.Details, "codex (pro)") {
		t.Fatalf("details=%q", formatted.Details)
	}
}

func TestFormatSnapshotsHandlesEmpty(t *testing.T) {
	formatted := display.FormatSnapshots(nil)
	if formatted.Summary != "quota unavailable" {
		t.Fatalf("Summary=%q", formatted.Summary)
	}
	if formatted.Details == "" {
		t.Fatal("Details should explain empty quota data")
	}
}

func TestFormatErrorStatus(t *testing.T) {
	if got := display.StatusText(display.StatusUnauthorized); got != "auth expired" {
		t.Fatalf("StatusText=%q", got)
	}
}
