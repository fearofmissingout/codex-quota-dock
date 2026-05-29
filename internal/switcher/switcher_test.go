package switcher_test

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fearofmissingout/codex-quota-dock/internal/switcher"
)

func TestSwitchBacksUpCurrentAuthAndReplacesIt(t *testing.T) {
	root := t.TempDir()
	active := filepath.Join(root, ".codex", "auth.json")
	profileAuth := filepath.Join(root, "profile-auth.json")
	backups := filepath.Join(root, "backups")
	mustWrite(t, active, "current-auth")
	mustWrite(t, profileAuth, "profile-auth")

	sw := switcher.New(active, backups)
	result, err := sw.Switch(profileAuth)
	if err != nil {
		t.Fatalf("Switch returned error: %v", err)
	}
	activeData, err := os.ReadFile(active)
	if err != nil {
		t.Fatalf("read active auth: %v", err)
	}
	if string(activeData) != "profile-auth" {
		t.Fatalf("active auth=%q want profile-auth", activeData)
	}
	backupData, err := os.ReadFile(result.BackupPath)
	if err != nil {
		t.Fatalf("read backup auth: %v", err)
	}
	if string(backupData) != "current-auth" {
		t.Fatalf("backup auth=%q want current-auth", backupData)
	}
}

func TestSwitchDoesNotReplaceWhenBackupFails(t *testing.T) {
	root := t.TempDir()
	active := filepath.Join(root, ".codex", "auth.json")
	profileAuth := filepath.Join(root, "profile-auth.json")
	backupsAsFile := filepath.Join(root, "backups")
	mustWrite(t, active, "current-auth")
	mustWrite(t, profileAuth, "profile-auth")
	mustWrite(t, backupsAsFile, "not-a-dir")

	sw := switcher.New(active, backupsAsFile)
	if _, err := sw.Switch(profileAuth); err == nil {
		t.Fatal("Switch returned nil error when backup dir is a file")
	}
	activeData, err := os.ReadFile(active)
	if err != nil {
		t.Fatalf("read active auth: %v", err)
	}
	if string(activeData) != "current-auth" {
		t.Fatalf("active auth changed after backup failure: %q", activeData)
	}
}

func TestSwitchRemovesTemporaryFile(t *testing.T) {
	root := t.TempDir()
	active := filepath.Join(root, ".codex", "auth.json")
	profileAuth := filepath.Join(root, "profile-auth.json")
	backups := filepath.Join(root, "backups")
	mustWrite(t, active, "current-auth")
	mustWrite(t, profileAuth, "profile-auth")

	sw := switcher.New(active, backups)
	if _, err := sw.Switch(profileAuth); err != nil {
		t.Fatalf("Switch returned error: %v", err)
	}
	entries, err := os.ReadDir(filepath.Dir(active))
	if err != nil {
		t.Fatalf("read active dir: %v", err)
	}
	for _, entry := range entries {
		if strings.Contains(entry.Name(), ".tmp") {
			t.Fatalf("temporary file remained: %s", entry.Name())
		}
	}
}

func TestDefaultCodexAuthPath(t *testing.T) {
	path, err := switcher.DefaultCodexAuthPath()
	if err != nil {
		t.Fatalf("DefaultCodexAuthPath returned error: %v", err)
	}
	if !strings.HasSuffix(filepath.ToSlash(path), "/.codex/auth.json") {
		t.Fatalf("path=%q", path)
	}
}

func TestDefaultCodexAuthPathUsesCodexHome(t *testing.T) {
	root := t.TempDir()
	t.Setenv("CODEX_HOME", root)

	path, err := switcher.DefaultCodexAuthPath()
	if err != nil {
		t.Fatalf("DefaultCodexAuthPath returned error: %v", err)
	}
	if path != filepath.Join(root, "auth.json") {
		t.Fatalf("path=%q want CODEX_HOME auth", path)
	}
}

func mustWrite(t *testing.T, path, data string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0o700); err != nil {
		t.Fatalf("mkdir: %v", err)
	}
	if err := os.WriteFile(path, []byte(data), 0o600); err != nil {
		t.Fatalf("write %s: %v", path, err)
	}
}
