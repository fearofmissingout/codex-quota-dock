package main

import (
	"fmt"
	"strings"
	"time"

	"github.com/fearofmissingout/codex-quota-dock/internal/display"
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

func monitorQuotaLine(row profileRow) string {
	return monitorCompactLine(row, 0, "5h: not refreshed") + "  |  " + monitorCompactLine(row, 1, "weekly: not refreshed")
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
