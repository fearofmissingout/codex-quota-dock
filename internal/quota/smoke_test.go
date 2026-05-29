package quota_test

import (
	"context"
	"os"
	"testing"
	"time"

	"github.com/fearofmissingout/codex-quota-dock/internal/auth"
	"github.com/fearofmissingout/codex-quota-dock/internal/quota"
)

func TestSmokeFetchWithAuthFile(t *testing.T) {
	path := os.Getenv("CODEX_QUOTA_SMOKE_AUTH_PATH")
	if path == "" {
		t.Skip("set CODEX_QUOTA_SMOKE_AUTH_PATH to run a real quota fetch")
	}

	authFile, err := auth.Load(path)
	if err != nil {
		t.Fatalf("load auth file: %v", err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	snapshots, err := quota.DefaultClient().Fetch(ctx, authFile)
	if err != nil {
		t.Fatalf("fetch quota: %v", err)
	}
	if len(snapshots) == 0 {
		t.Fatal("fetch quota returned no snapshots")
	}
	for _, snapshot := range snapshots {
		t.Logf("limit=%s plan=%s primary=%s secondary=%s", snapshot.LimitID, snapshot.PlanType, smokeWindow(snapshot.Primary), smokeWindow(snapshot.Secondary))
	}
}

func smokeWindow(window *quota.Window) string {
	if window == nil {
		return "none"
	}
	return quota.WindowLabel(window.WindowMinutes)
}
