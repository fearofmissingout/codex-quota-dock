package backup_test

import (
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fearofmissingout/codex-quota-dock/internal/backup"
	"github.com/fearofmissingout/codex-quota-dock/internal/profile"
)

const authJSON = `{
  "auth_mode": "chatgpt",
  "tokens": {
    "id_token": "test-id-token",
    "access_token": "test-access-token",
    "refresh_token": "test-refresh-token",
    "account_id": "acc_1234567890"
  },
  "last_refresh": "2026-05-29T10:25:25Z"
}`

const secondAuthJSON = `{
  "auth_mode": "chatgpt",
  "tokens": {
    "id_token": "second-id-token",
    "access_token": "second-access-token",
    "refresh_token": "second-refresh-token",
    "account_id": "acc_5555555555"
  },
  "last_refresh": "2026-05-29T10:25:25Z"
}`

func TestExportImportRoundTripProfilesAndSettings(t *testing.T) {
	root := t.TempDir()
	source, err := profile.Open(filepath.Join(root, "source"))
	if err != nil {
		t.Fatalf("Open source returned error: %v", err)
	}
	company, err := source.ImportBytes("company", []byte(authJSON))
	if err != nil {
		t.Fatalf("ImportBytes company returned error: %v", err)
	}
	if _, err := source.SetPinned(company.ID, true); err != nil {
		t.Fatalf("SetPinned returned error: %v", err)
	}
	if _, err := source.ImportBytes("pro", []byte(secondAuthJSON)); err != nil {
		t.Fatalf("ImportBytes pro returned error: %v", err)
	}

	data, err := backup.Export(source, backup.Settings{
		PollingInterval:          "5 minutes",
		FiveHourAlertThreshold:   10,
		WeeklyAlertThreshold:     30,
		AutoRestartAfterSwitch:   true,
		ShowRestartReminder:      false,
		CheckForUpdatesOnStartup: true,
		StartAtLogin:             true,
	})
	if err != nil {
		t.Fatalf("Export returned error: %v", err)
	}
	if !strings.Contains(string(data), "test-access-token") {
		t.Fatalf("backup should contain auth credentials for migration")
	}

	target, err := profile.Open(filepath.Join(root, "target"))
	if err != nil {
		t.Fatalf("Open target returned error: %v", err)
	}
	summary, err := backup.Import(target, data)
	if err != nil {
		t.Fatalf("Import returned error: %v", err)
	}
	if summary.Created != 2 || summary.Updated != 0 {
		t.Fatalf("summary=%+v want 2 created", summary)
	}
	if summary.Settings.PollingInterval != "5 minutes" || !summary.Settings.StartAtLogin {
		t.Fatalf("settings=%+v not restored", summary.Settings)
	}
	profiles := target.Profiles()
	if len(profiles) != 2 {
		t.Fatalf("profiles=%+v want 2", profiles)
	}
	if profiles[0].Alias != "company" || !profiles[0].Pinned {
		t.Fatalf("first profile=%+v want pinned company", profiles[0])
	}
	saved, err := target.ReadAuth(profiles[0].ID)
	if err != nil {
		t.Fatalf("ReadAuth returned error: %v", err)
	}
	if !strings.Contains(string(saved), "test-refresh-token") {
		t.Fatalf("restored auth missing token")
	}
}

func TestImportBackupUpdatesExistingAccountID(t *testing.T) {
	root := t.TempDir()
	store, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	existing, err := store.ImportBytes("old", []byte(authJSON))
	if err != nil {
		t.Fatalf("ImportBytes returned error: %v", err)
	}
	doc := backup.Document{
		Version: 1,
		Profiles: []backup.Profile{
			{Alias: "new", Pinned: true, AuthJSON: json.RawMessage(authJSON)},
		},
	}
	data, err := json.Marshal(doc)
	if err != nil {
		t.Fatalf("marshal backup: %v", err)
	}

	summary, err := backup.Import(store, data)
	if err != nil {
		t.Fatalf("Import returned error: %v", err)
	}
	if summary.Created != 0 || summary.Updated != 1 {
		t.Fatalf("summary=%+v want one updated", summary)
	}
	profiles := store.Profiles()
	if len(profiles) != 1 {
		t.Fatalf("profiles=%+v want one existing profile", profiles)
	}
	if profiles[0].ID != existing.ID || profiles[0].Alias != "new" || !profiles[0].Pinned {
		t.Fatalf("profile=%+v want updated existing pinned profile", profiles[0])
	}
}

func TestImportBackupRejectsMissingAuthJSON(t *testing.T) {
	store, err := profile.Open(t.TempDir())
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	path := filepath.Join(t.TempDir(), "backup.json")
	if err := os.WriteFile(path, []byte(`{"version":1,"profiles":[{"alias":"empty"}]}`), 0600); err != nil {
		t.Fatalf("write backup: %v", err)
	}
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read backup: %v", err)
	}

	if _, err := backup.Import(store, data); err == nil {
		t.Fatal("Import returned nil error for missing auth JSON")
	}
}
