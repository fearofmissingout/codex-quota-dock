package display

import (
	"errors"
	"fmt"
	"strings"
	"time"

	"codex-quota-monitor/internal/quota"
)

type Status string

const (
	StatusNotRefreshed    Status = "not refreshed"
	StatusOK              Status = "ok"
	StatusRefreshing      Status = "refreshing"
	StatusUnauthorized    Status = "auth expired"
	StatusRateLimited     Status = "check limited"
	StatusInvalidResponse Status = "bad response"
	StatusError           Status = "error"
)

type FormattedSnapshots struct {
	CompactTitle string
	CompactLines []string
	Summary      string
	Details      string
}

func FormatSnapshots(snapshots []quota.Snapshot) FormattedSnapshots {
	if len(snapshots) == 0 {
		return FormattedSnapshots{
			CompactTitle: "Codex",
			CompactLines: []string{
				"5h: quota unavailable",
				"weekly: quota unavailable",
			},
			Summary: "quota unavailable",
			Details: "No quota windows were returned.",
		}
	}

	lines := make([]string, 0, len(snapshots)*4)
	compact := []string{}
	summary := []string{}
	title := "Codex"

	for _, snapshot := range snapshots {
		name := snapshot.LimitName
		if name == "" {
			name = snapshot.LimitID
		}
		if name == "" {
			name = "codex"
		}
		if snapshot.LimitID == "codex" || strings.EqualFold(name, "codex") {
			title = "Codex"
		}

		header := name
		if snapshot.PlanType != "" {
			header = fmt.Sprintf("%s (%s)", name, snapshot.PlanType)
		}
		lines = append(lines, header)
		if snapshot.RateLimitReachedType != "" {
			lines = append(lines, "  reached: "+snapshot.RateLimitReachedType)
		}
		if snapshot.Primary != nil {
			line := formatWindow("primary", snapshot.Primary)
			lines = append(lines, "  "+line)
			if isCodexSnapshot(snapshot) {
				compact = append(compact, formatCompactWindow(snapshot.Primary))
				summary = append(summary, line)
			}
		}
		if snapshot.Secondary != nil {
			line := formatWindow("secondary", snapshot.Secondary)
			lines = append(lines, "  "+line)
			if isCodexSnapshot(snapshot) {
				compact = append(compact, formatCompactWindow(snapshot.Secondary))
				summary = append(summary, line)
			}
		}
		if snapshot.Credits != nil && snapshot.Credits.HasCredits {
			balance := snapshot.Credits.Balance
			if balance == "" && snapshot.Credits.Unlimited {
				balance = "unlimited"
			}
			lines = append(lines, "  credits: "+balance)
		}
	}

	if len(compact) == 0 {
		compact = []string{"5h: quota unavailable", "weekly: quota unavailable"}
	}
	for len(compact) < 2 {
		compact = append(compact, "usage: quota unavailable")
	}
	if len(summary) == 0 {
		summary = append(summary, "quota unavailable")
	}

	return FormattedSnapshots{
		CompactTitle: title,
		CompactLines: compact,
		Summary:      strings.Join(summary, "; "),
		Details:      strings.Join(lines, "\r\n"),
	}
}

func StatusText(status Status) string {
	return string(status)
}

func ClassifyError(err error) Status {
	switch {
	case errors.Is(err, quota.ErrUnauthorized):
		return StatusUnauthorized
	case errors.Is(err, quota.ErrRateLimited):
		return StatusRateLimited
	case errors.Is(err, quota.ErrInvalidResponse):
		return StatusInvalidResponse
	default:
		return StatusError
	}
}

func isCodexSnapshot(snapshot quota.Snapshot) bool {
	return snapshot.LimitID == "codex" || strings.EqualFold(snapshot.LimitName, "codex")
}

func formatCompactWindow(window *quota.Window) string {
	label := quota.WindowLabel(window.WindowMinutes)
	return fmt.Sprintf("%s: %.1f%% left, resets %s", label, quota.RemainingPercent(window.UsedPercent), formatReset(window.ResetsAt))
}

func formatWindow(prefix string, window *quota.Window) string {
	label := quota.WindowLabel(window.WindowMinutes)
	return fmt.Sprintf("%s %s: %.1f%% used, %.1f%% left, resets %s", prefix, label, window.UsedPercent, quota.RemainingPercent(window.UsedPercent), formatReset(window.ResetsAt))
}

func formatReset(resetsAt int64) string {
	if resetsAt <= 0 {
		return "-"
	}
	return time.Unix(resetsAt, 0).Local().Format("2006-01-02 15:04")
}
