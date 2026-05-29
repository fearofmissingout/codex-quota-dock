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
