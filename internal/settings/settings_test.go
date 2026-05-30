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
	want := []int{0, 5, 10, 20, 30}
	if !reflect.DeepEqual(options, want) {
		t.Fatalf("QuotaAlertThresholdOptions()=%v want %v", options, want)
	}
}

func TestDefaultQuotaAlertThreshold(t *testing.T) {
	if got := settings.DefaultQuotaAlertThreshold(); got != 20 {
		t.Fatalf("DefaultQuotaAlertThreshold()=%d want 20", got)
	}
}
