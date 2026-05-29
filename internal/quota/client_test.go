package quota_test

import (
	"context"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"codex-quota-monitor/internal/auth"
	"codex-quota-monitor/internal/quota"
)

func TestClientSendsAuthHeadersAndParsesResponse(t *testing.T) {
	var sawAuthorization string
	var sawAccountID string
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/wham/usage" {
			t.Fatalf("path=%q want /wham/usage", r.URL.Path)
		}
		sawAuthorization = r.Header.Get("Authorization")
		sawAccountID = r.Header.Get("ChatGPT-Account-Id")
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(usagePayload))
	}))
	defer server.Close()

	client := quota.NewClient(server.URL, "/wham/usage")
	got, err := client.Fetch(context.Background(), auth.File{
		Tokens: auth.Tokens{AccessToken: "token-123", AccountID: "account-123"},
	})
	if err != nil {
		t.Fatalf("Fetch returned error: %v", err)
	}
	if sawAuthorization != "Bearer token-123" {
		t.Fatalf("Authorization=%q", sawAuthorization)
	}
	if sawAccountID != "account-123" {
		t.Fatalf("ChatGPT-Account-Id=%q", sawAccountID)
	}
	if len(got) != 2 || got[0].LimitID != "codex" {
		t.Fatalf("snapshots=%+v", got)
	}
}

func TestClientMapsUnauthorized(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Error(w, "nope", http.StatusUnauthorized)
	}))
	defer server.Close()

	client := quota.NewClient(server.URL, "/wham/usage")
	_, err := client.Fetch(context.Background(), auth.File{Tokens: auth.Tokens{AccessToken: "token"}})
	if !errors.Is(err, quota.ErrUnauthorized) {
		t.Fatalf("err=%v want ErrUnauthorized", err)
	}
}

func TestClientMapsRateLimited(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Error(w, "slow down", http.StatusTooManyRequests)
	}))
	defer server.Close()

	client := quota.NewClient(server.URL, "/wham/usage")
	_, err := client.Fetch(context.Background(), auth.File{Tokens: auth.Tokens{AccessToken: "token"}})
	if !errors.Is(err, quota.ErrRateLimited) {
		t.Fatalf("err=%v want ErrRateLimited", err)
	}
}

func TestClientReportsInvalidJSON(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_, _ = w.Write([]byte(`{"plan_type":`))
	}))
	defer server.Close()

	client := quota.NewClient(server.URL, "/wham/usage")
	_, err := client.Fetch(context.Background(), auth.File{Tokens: auth.Tokens{AccessToken: "token"}})
	if !errors.Is(err, quota.ErrInvalidResponse) {
		t.Fatalf("err=%v want ErrInvalidResponse", err)
	}
}
