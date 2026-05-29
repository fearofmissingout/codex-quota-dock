package quota_test

import (
	"testing"

	"codex-quota-monitor/internal/quota"
)

const usagePayload = `{
  "plan_type": "pro",
  "rate_limit": {
    "allowed": true,
    "limit_reached": false,
    "primary_window": {
      "used_percent": 42,
      "limit_window_seconds": 18000,
      "reset_after_seconds": 3000,
      "reset_at": 1780026000
    },
    "secondary_window": {
      "used_percent": 95.5,
      "limit_window_seconds": 604800,
      "reset_after_seconds": 86400,
      "reset_at": 1780630800
    }
  },
  "additional_rate_limits": [
    {
      "limit_name": "codex_other",
      "metered_feature": "codex_other",
      "rate_limit": {
        "allowed": true,
        "limit_reached": false,
        "primary_window": {
          "used_percent": 88,
          "limit_window_seconds": 1800,
          "reset_after_seconds": 600,
          "reset_at": 1780023600
        }
      }
    }
  ]
}`

func TestParsePayloadMapsWindows(t *testing.T) {
	snapshots, err := quota.ParsePayload([]byte(usagePayload))
	if err != nil {
		t.Fatalf("ParsePayload returned error: %v", err)
	}
	if len(snapshots) != 2 {
		t.Fatalf("len(snapshots)=%d want 2", len(snapshots))
	}
	codex := snapshots[0]
	if codex.PlanType != "pro" {
		t.Fatalf("PlanType=%q want pro", codex.PlanType)
	}
	if codex.LimitID != "codex" {
		t.Fatalf("LimitID=%q want codex", codex.LimitID)
	}
	if codex.Primary == nil || codex.Primary.UsedPercent != 42 {
		t.Fatalf("Primary=%+v", codex.Primary)
	}
	if codex.Primary.WindowMinutes != 300 {
		t.Fatalf("Primary.WindowMinutes=%d want 300", codex.Primary.WindowMinutes)
	}
	if codex.Secondary == nil || codex.Secondary.WindowMinutes != 10080 {
		t.Fatalf("Secondary=%+v", codex.Secondary)
	}
	if codex.Secondary.ResetsAt != 1780630800 {
		t.Fatalf("Secondary.ResetsAt=%d", codex.Secondary.ResetsAt)
	}
}

func TestParsePayloadMapsAdditionalLimits(t *testing.T) {
	snapshots, err := quota.ParsePayload([]byte(usagePayload))
	if err != nil {
		t.Fatalf("ParsePayload returned error: %v", err)
	}
	extra := snapshots[1]
	if extra.LimitID != "codex_other" {
		t.Fatalf("LimitID=%q want codex_other", extra.LimitID)
	}
	if extra.LimitName != "codex_other" {
		t.Fatalf("LimitName=%q want codex_other", extra.LimitName)
	}
	if extra.Primary == nil || extra.Primary.WindowMinutes != 30 {
		t.Fatalf("Primary=%+v", extra.Primary)
	}
}

func TestWindowLabels(t *testing.T) {
	cases := map[int]string{
		300:   "5h",
		1440:  "daily",
		10080: "weekly",
		43200: "monthly",
		525600: "annual",
		30:    "usage",
	}
	for minutes, want := range cases {
		if got := quota.WindowLabel(minutes); got != want {
			t.Fatalf("WindowLabel(%d)=%q want %q", minutes, got, want)
		}
	}
}

func TestRemainingPercentClamps(t *testing.T) {
	cases := map[float64]float64{
		42:    58,
		0:     100,
		100:   0,
		120:   0,
		-12.5: 100,
	}
	for used, want := range cases {
		if got := quota.RemainingPercent(used); got != want {
			t.Fatalf("RemainingPercent(%v)=%v want %v", used, got, want)
		}
	}
}
