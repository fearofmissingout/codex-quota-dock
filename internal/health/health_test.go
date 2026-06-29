package health_test

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/fearofmissingout/codex-quota-dock/internal/health"
	"github.com/fearofmissingout/codex-quota-dock/internal/profile"
)

const authJSON = `{
  "auth_mode": "chatgpt",
  "tokens": {
    "id_token": "secret-id-token",
    "access_token": "secret-access-token",
    "refresh_token": "secret-refresh-token",
    "account_id": "acc_1234567890"
  },
  "last_refresh": "2026-05-29T10:25:25Z"
}`

func TestInspectAuthAndProfilesDoesNotExposeTokens(t *testing.T) {
	root := t.TempDir()
	authPath := filepath.Join(root, "auth.json")
	if err := os.WriteFile(authPath, []byte(authJSON), 0600); err != nil {
		t.Fatalf("write auth: %v", err)
	}
	store, err := profile.Open(filepath.Join(root, "store"))
	if err != nil {
		t.Fatalf("Open returned error: %v", err)
	}
	if _, err := store.ImportBytes("company", []byte(authJSON)); err != nil {
		t.Fatalf("ImportBytes returned error: %v", err)
	}

	rows := health.Inspect(authPath, store.Profiles(), true, "v0.4.0")
	text := health.Format(rows)

	for _, want := range []string{"Active auth", "Profiles", "Startup", "Version", "acc_...567890"} {
		if !strings.Contains(text, want) {
			t.Fatalf("diagnostics missing %q:\n%s", want, text)
		}
	}
	for _, secret := range []string{"secret-id-token", "secret-access-token", "secret-refresh-token"} {
		if strings.Contains(text, secret) {
			t.Fatalf("diagnostics exposed %q:\n%s", secret, text)
		}
	}
}
