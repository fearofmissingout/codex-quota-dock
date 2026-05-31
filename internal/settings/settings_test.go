package settings_test

import (
	"reflect"
	"testing"
	"time"

	"github.com/fearofmissingout/codex-quota-dock/internal/settings"
)

func TestPollingOptions(t *testing.T) {
	options := settings.PollingOptions()
	want := []time.Duration{0, time.Minute, 5 * time.Minute, 10 * time.Minute}
	if !reflect.DeepEqual(options, want) {
		t.Fatalf("PollingOptions()=%v want %v", options, want)
	}
}

func TestDefaultPollingInterval(t *testing.T) {
	if got := settings.DefaultPollingInterval(); got != 5*time.Minute {
		t.Fatalf("DefaultPollingInterval()=%v want 5m", got)
	}
}

func TestQuotaAlertThresholdOptions(t *testing.T) {
	options := settings.QuotaAlertThresholdOptions()
	want := []int{0, 5, 10, 15, 20, 30, 40}
	if !reflect.DeepEqual(options, want) {
		t.Fatalf("QuotaAlertThresholdOptions()=%v want %v", options, want)
	}
}

func TestWindowSpecificQuotaAlertDefaults(t *testing.T) {
	if got := settings.DefaultFiveHourQuotaAlertThreshold(); got != 10 {
		t.Fatalf("DefaultFiveHourQuotaAlertThreshold()=%d want 10", got)
	}
	if got := settings.DefaultWeeklyQuotaAlertThreshold(); got != 30 {
		t.Fatalf("DefaultWeeklyQuotaAlertThreshold()=%d want 30", got)
	}
}
