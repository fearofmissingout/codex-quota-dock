package auth_test

import (
	"strings"
	"testing"

	"codex-quota-monitor/internal/auth"
)

const validAuthJSON = `{
  "auth_mode": "chatgpt",
  "tokens": {
    "id_token": "id-secret",
    "access_token": "access-secret",
    "refresh_token": "refresh-secret",
    "account_id": "acc_1234567890"
  },
  "last_refresh": "2026-05-29T10:25:25Z"
}`

func TestParseValidCodexAuth(t *testing.T) {
	file, err := auth.Parse([]byte(validAuthJSON))
	if err != nil {
		t.Fatalf("Parse returned error: %v", err)
	}
	if file.AuthMode != "chatgpt" {
		t.Fatalf("AuthMode=%q want chatgpt", file.AuthMode)
	}
	if file.Tokens.AccessToken != "access-secret" {
		t.Fatalf("AccessToken not parsed")
	}
	if file.Tokens.AccountID != "acc_1234567890" {
		t.Fatalf("AccountID=%q", file.Tokens.AccountID)
	}
}

func TestValidateRejectsMissingAccessToken(t *testing.T) {
	_, err := auth.Parse([]byte(`{"auth_mode":"chatgpt","tokens":{"account_id":"acc_123"}}`))
	if err == nil {
		t.Fatal("Parse returned nil error for missing access token")
	}
	if !strings.Contains(err.Error(), "access_token") {
		t.Fatalf("error=%q want access_token mention", err.Error())
	}
}

func TestAccountSuffix(t *testing.T) {
	file, err := auth.Parse([]byte(validAuthJSON))
	if err != nil {
		t.Fatalf("Parse returned error: %v", err)
	}
	if got := file.AccountSuffix(6); got != "567890" {
		t.Fatalf("AccountSuffix=%q want 567890", got)
	}
}

func TestRedactedDoesNotExposeTokens(t *testing.T) {
	file, err := auth.Parse([]byte(validAuthJSON))
	if err != nil {
		t.Fatalf("Parse returned error: %v", err)
	}
	redacted := file.Redacted()
	text := redacted.String()
	for _, secret := range []string{"id-secret", "access-secret", "refresh-secret"} {
		if strings.Contains(text, secret) {
			t.Fatalf("redacted output exposed %q: %s", secret, text)
		}
	}
	if !strings.Contains(text, "acc_1234567890") {
		t.Fatalf("redacted output should keep account id for local matching: %s", text)
	}
}
