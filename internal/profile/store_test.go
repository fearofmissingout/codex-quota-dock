package profile_test

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

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

func TestImportCopiesAuthAndPersistsMetadata(t *testing.T) {
	root := t.TempDir()
	source := filepath.Join(root, "source-auth.json")
	if err := os.WriteFile(source, []byte(authJSON), 0o600); err != nil {
		t.Fatalf("write source auth: %v", err)
	}

	store, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	prof, err := store.Import("company", source)
	if err != nil {
		t.Fatalf("Import returned error: %v", err)
	}
	if prof.Alias != "company" {
		t.Fatalf("Alias=%q", prof.Alias)
	}
	if prof.AccountSuffix != "567890" {
		t.Fatalf("AccountSuffix=%q", prof.AccountSuffix)
	}
	copied, err := os.ReadFile(store.AuthPath(prof.ID))
	if err != nil {
		t.Fatalf("read copied auth: %v", err)
	}
	if !strings.Contains(string(copied), "test-access-token") {
		t.Fatalf("copied auth missing token")
	}

	reloaded, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("reopen store: %v", err)
	}
	if len(reloaded.Profiles()) != 1 {
		t.Fatalf("profiles=%+v", reloaded.Profiles())
	}
}

func TestImportRejectsDuplicateAlias(t *testing.T) {
	root := t.TempDir()
	source := filepath.Join(root, "source-auth.json")
	if err := os.WriteFile(source, []byte(authJSON), 0o600); err != nil {
		t.Fatalf("write source auth: %v", err)
	}
	store, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	if _, err := store.Import("company", source); err != nil {
		t.Fatalf("first import returned error: %v", err)
	}
	if _, err := store.Import("company", source); err == nil {
		t.Fatal("duplicate alias import returned nil error")
	}
}

func TestMetadataDoesNotStoreTokens(t *testing.T) {
	root := t.TempDir()
	source := filepath.Join(root, "source-auth.json")
	if err := os.WriteFile(source, []byte(authJSON), 0o600); err != nil {
		t.Fatalf("write source auth: %v", err)
	}
	store, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	if _, err := store.Import("company", source); err != nil {
		t.Fatalf("Import returned error: %v", err)
	}
	metadata, err := os.ReadFile(filepath.Join(root, "store", "profiles.json"))
	if err != nil {
		t.Fatalf("read profiles metadata: %v", err)
	}
	for _, secret := range []string{"test-id-token", "test-access-token", "test-refresh-token"} {
		if strings.Contains(string(metadata), secret) {
			t.Fatalf("metadata exposed %q: %s", secret, metadata)
		}
	}
}

func TestFindActiveByAccountID(t *testing.T) {
	root := t.TempDir()
	source := filepath.Join(root, "source-auth.json")
	if err := os.WriteFile(source, []byte(authJSON), 0o600); err != nil {
		t.Fatalf("write source auth: %v", err)
	}
	store, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	want, err := store.Import("company", source)
	if err != nil {
		t.Fatalf("Import returned error: %v", err)
	}
	got, ok := store.FindByAccountID("acc_1234567890")
	if !ok {
		t.Fatal("FindByAccountID returned false")
	}
	if got.ID != want.ID {
		t.Fatalf("ID=%q want %q", got.ID, want.ID)
	}
}

func TestSetPinnedPersistsProfileFavorite(t *testing.T) {
	root := t.TempDir()
	source := filepath.Join(root, "source-auth.json")
	if err := os.WriteFile(source, []byte(authJSON), 0o600); err != nil {
		t.Fatalf("write source auth: %v", err)
	}
	store, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	prof, err := store.Import("company", source)
	if err != nil {
		t.Fatalf("Import returned error: %v", err)
	}

	updated, err := store.SetPinned(prof.ID, true)
	if err != nil {
		t.Fatalf("SetPinned returned error: %v", err)
	}
	if !updated.Pinned {
		t.Fatalf("Pinned=false after SetPinned")
	}

	reloaded, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("reopen store: %v", err)
	}
	profiles := reloaded.Profiles()
	if len(profiles) != 1 || !profiles[0].Pinned {
		t.Fatalf("profiles=%+v want pinned profile", profiles)
	}
}

func TestSetPinnedRejectsUnknownProfile(t *testing.T) {
	store, err := profile.Open(t.TempDir())
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	if _, err := store.SetPinned("missing", true); err == nil {
		t.Fatal("SetPinned returned nil error for unknown profile")
	}
}
