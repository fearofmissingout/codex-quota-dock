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

const updatedAuthJSON = `{
  "auth_mode": "chatgpt",
  "tokens": {
    "id_token": "updated-id-token",
    "access_token": "updated-access-token",
    "refresh_token": "updated-refresh-token",
    "account_id": "acc_9999999999"
  },
  "last_refresh": "2026-05-29T11:11:11Z"
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

func TestImportBytesCreatesProfileWithoutSourceFile(t *testing.T) {
	root := t.TempDir()
	store, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}

	prof, err := store.ImportBytes("current", []byte(authJSON))
	if err != nil {
		t.Fatalf("ImportBytes returned error: %v", err)
	}
	if prof.Alias != "current" {
		t.Fatalf("Alias=%q want current", prof.Alias)
	}
	if prof.AccountSuffix != "567890" {
		t.Fatalf("AccountSuffix=%q want 567890", prof.AccountSuffix)
	}
	saved, err := store.ReadAuth(prof.ID)
	if err != nil {
		t.Fatalf("ReadAuth returned error: %v", err)
	}
	if !strings.Contains(string(saved), "test-refresh-token") {
		t.Fatalf("saved auth missing token")
	}
}

func TestSuggestAliasUsesAccountSuffixAndAvoidsDuplicates(t *testing.T) {
	root := t.TempDir()
	store, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	if _, err := store.ImportBytes("current-567890", []byte(authJSON)); err != nil {
		t.Fatalf("ImportBytes returned error: %v", err)
	}

	got, err := store.SuggestAlias("current", []byte(authJSON))
	if err != nil {
		t.Fatalf("SuggestAlias returned error: %v", err)
	}
	if got != "current-567890-2" {
		t.Fatalf("alias=%q want current-567890-2", got)
	}
}

func TestUpdateByAccountIDUpdatesExistingProfile(t *testing.T) {
	root := t.TempDir()
	store, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	prof, err := store.ImportBytes("company", []byte(authJSON))
	if err != nil {
		t.Fatalf("ImportBytes returned error: %v", err)
	}

	updated, ok, err := store.UpdateByAccountID("company-new", []byte(authJSON))
	if err != nil {
		t.Fatalf("UpdateByAccountID returned error: %v", err)
	}
	if !ok {
		t.Fatal("UpdateByAccountID returned ok=false for existing account")
	}
	if updated.ID != prof.ID {
		t.Fatalf("ID=%q want existing %q", updated.ID, prof.ID)
	}
	if updated.Alias != "company-new" {
		t.Fatalf("Alias=%q want company-new", updated.Alias)
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

func TestDeleteProfileRemovesAuthAndPersistsMetadata(t *testing.T) {
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
	authPath := store.AuthPath(prof.ID)

	if err := store.Delete(prof.ID); err != nil {
		t.Fatalf("Delete returned error: %v", err)
	}
	if _, err := os.Stat(authPath); !os.IsNotExist(err) {
		t.Fatalf("auth path still exists or stat failed with unexpected error: %v", err)
	}

	reloaded, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("reopen store: %v", err)
	}
	if len(reloaded.Profiles()) != 0 {
		t.Fatalf("profiles=%+v want deleted profile removed", reloaded.Profiles())
	}
}

func TestDeleteProfileRejectsUnknownProfile(t *testing.T) {
	store, err := profile.Open(t.TempDir())
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	if err := store.Delete("missing"); err == nil {
		t.Fatal("Delete returned nil error for unknown profile")
	}
}

func TestUpdateProfileAuthAndAliasPersists(t *testing.T) {
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

	updated, err := store.Update(prof.ID, "pro", []byte(updatedAuthJSON))
	if err != nil {
		t.Fatalf("Update returned error: %v", err)
	}
	if updated.Alias != "pro" {
		t.Fatalf("Alias=%q want pro", updated.Alias)
	}
	if updated.AccountSuffix != "999999" {
		t.Fatalf("AccountSuffix=%q want updated suffix", updated.AccountSuffix)
	}

	copied, err := store.ReadAuth(prof.ID)
	if err != nil {
		t.Fatalf("ReadAuth returned error: %v", err)
	}
	if !strings.Contains(string(copied), "updated-access-token") {
		t.Fatalf("auth file was not updated: %s", copied)
	}

	reloaded, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("reopen store: %v", err)
	}
	profiles := reloaded.Profiles()
	if len(profiles) != 1 || profiles[0].Alias != "pro" || profiles[0].AccountSuffix != "999999" {
		t.Fatalf("profiles=%+v want updated metadata", profiles)
	}
}

func TestUpdateProfileRejectsDuplicateAlias(t *testing.T) {
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
	pro, err := store.Import("pro", source)
	if err != nil {
		t.Fatalf("second import returned error: %v", err)
	}

	if _, err := store.Update(pro.ID, "company", []byte(authJSON)); err == nil {
		t.Fatal("Update returned nil error for duplicate alias")
	}
}

func TestUpdateProfileRejectsInvalidAuthWithoutChangingSavedAuth(t *testing.T) {
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

	if _, err := store.Update(prof.ID, "renamed", []byte(`{"tokens":{}}`)); err == nil {
		t.Fatal("Update returned nil error for invalid auth")
	}

	saved, err := store.ReadAuth(prof.ID)
	if err != nil {
		t.Fatalf("ReadAuth returned error: %v", err)
	}
	if !strings.Contains(string(saved), "test-access-token") {
		t.Fatalf("saved auth changed after rejected update: %s", saved)
	}
	if got := store.Profiles()[0].Alias; got != "company" {
		t.Fatalf("Alias=%q want unchanged company", got)
	}
}
