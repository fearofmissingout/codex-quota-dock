# Codex Quota Monitor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Go Windows desktop tool that monitors multiple Codex auth profiles, shows quota windows, and switches the active Codex auth file safely.

**Architecture:** Core logic is split into small testable packages for auth parsing, profile storage, quota fetching/parsing, polling configuration, and auth switching. The Windows desktop UI in `cmd/codex-quota-monitor` uses `walk` and calls these packages through simple services.

**Tech Stack:** Go 1.24, standard library HTTP/JSON/filesystem APIs, `github.com/lxn/walk` for Windows UI, `go test` for core behavior.

---

### Task 1: Project Skeleton And Polling Settings

**Files:**
- Create: `go.mod`
- Create: `internal/settings/settings.go`
- Create: `internal/settings/settings_test.go`

- [ ] **Step 1: Write failing tests for polling settings**

```go
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
```

- [ ] **Step 2: Run red test**

Run: `go test ./internal/settings`

Expected: fail because package does not exist.

- [ ] **Step 3: Implement settings package**

```go
package settings

import "time"

func DefaultPollingInterval() time.Duration { return 5 * time.Minute }

func PollingOptions() []time.Duration {
	return []time.Duration{0, time.Minute, 5 * time.Minute, 10 * time.Minute}
}
```

- [ ] **Step 4: Run green test**

Run: `go test ./internal/settings`

Expected: pass.

### Task 2: Auth Parsing And Redaction

**Files:**
- Create: `internal/auth/auth.go`
- Create: `internal/auth/auth_test.go`

- [ ] **Step 1: Write failing tests**

Cover:
- Parse valid Codex auth JSON.
- Reject missing access token.
- Redact access, refresh, and ID token values.
- Return account ID suffix for display.

- [ ] **Step 2: Run red test**

Run: `go test ./internal/auth`

Expected: fail because package does not exist.

- [ ] **Step 3: Implement auth package**

Define `File`, `Tokens`, `Parse`, `Load`, `Validate`, `AccountSuffix`, and `Redacted`.

- [ ] **Step 4: Run green test**

Run: `go test ./internal/auth`

Expected: pass.

### Task 3: Quota Payload Parsing And Labels

**Files:**
- Create: `internal/quota/types.go`
- Create: `internal/quota/parse.go`
- Create: `internal/quota/labels.go`
- Create: `internal/quota/parse_test.go`

- [ ] **Step 1: Write failing tests**

Cover:
- Parse `plan_type`, primary window, secondary window, reset timestamp.
- Parse additional rate limits.
- Map 300 minutes to `5h`.
- Map 10080 minutes to `weekly`.
- Clamp remaining percent to `0..100`.

- [ ] **Step 2: Run red test**

Run: `go test ./internal/quota`

Expected: fail because package does not exist.

- [ ] **Step 3: Implement quota parsing**

Define `StatusPayload`, `RateLimitDetails`, `WindowSnapshot`, `Snapshot`, `Window`, `ParsePayload`, `WindowLabel`, and `RemainingPercent`.

- [ ] **Step 4: Run green test**

Run: `go test ./internal/quota`

Expected: pass.

### Task 4: Quota HTTP Client

**Files:**
- Create: `internal/quota/client.go`
- Create: `internal/quota/client_test.go`

- [ ] **Step 1: Write failing tests with `httptest.Server`**

Cover:
- Sends `Authorization: Bearer <access token>`.
- Sends `ChatGPT-Account-Id`.
- Parses a 200 response.
- Maps 401/403 to unauthorized error.
- Maps 429 to rate-limited error.
- Reports invalid JSON.

- [ ] **Step 2: Run red test**

Run: `go test ./internal/quota`

Expected: fail because client does not exist.

- [ ] **Step 3: Implement client**

Define `Client`, `Fetch`, configurable base URL and path, timeout, and typed errors.

- [ ] **Step 4: Run green test**

Run: `go test ./internal/quota`

Expected: pass.

### Task 5: Profile Store

**Files:**
- Create: `internal/profile/store.go`
- Create: `internal/profile/store_test.go`

- [ ] **Step 1: Write failing tests**

Cover:
- Import auth file into profile directory.
- Reject duplicate alias.
- Persist and reload `profiles.json`.
- Detect active profile by account ID.
- Do not store token values in metadata.

- [ ] **Step 2: Run red test**

Run: `go test ./internal/profile`

Expected: fail because package does not exist.

- [ ] **Step 3: Implement profile store**

Define `Profile`, `Store`, `Import`, `Load`, `Save`, `Profiles`, `AuthPath`, and `FindActiveByAccountID`.

- [ ] **Step 4: Run green test**

Run: `go test ./internal/profile`

Expected: pass.

### Task 6: Auth Switcher

**Files:**
- Create: `internal/switcher/switcher.go`
- Create: `internal/switcher/switcher_test.go`

- [ ] **Step 1: Write failing tests**

Cover:
- Backup current auth before replacement.
- Replace active auth with selected profile auth.
- Do not replace active auth when backup fails.
- Use a temporary file during replacement.

- [ ] **Step 2: Run red test**

Run: `go test ./internal/switcher`

Expected: fail because package does not exist.

- [ ] **Step 3: Implement switcher**

Define `Switcher`, `Switch`, `Result`, `DefaultCodexAuthPath`, and safe copy/rename helpers.

- [ ] **Step 4: Run green test**

Run: `go test ./internal/switcher`

Expected: pass.

### Task 7: Windows Desktop UI

**Files:**
- Create: `cmd/codex-quota-monitor/main.go`
- Create: `cmd/codex-quota-monitor/ui.go`

- [ ] **Step 1: Implement UI shell**

Use `walk` to render:
- Active account label.
- Polling interval combo box with off/1/5/10 minutes.
- Profile table.
- Quota details text area.
- Import current auth, import auth file, refresh selected, refresh all, switch selected buttons.

- [ ] **Step 2: Wire UI actions**

Wire:
- Import current `~/.codex/auth.json`.
- Import selected auth file.
- Manual refresh.
- Refresh all.
- Switch selected profile with confirmation and restart-required dialog.
- Timer-based polling using the configured interval.

- [ ] **Step 3: Build UI**

Run: `go build ./cmd/codex-quota-monitor`

Expected: pass on Windows.

### Task 8: Verification And Polish

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Add README**

Document:
- What the tool does.
- Where profiles are stored.
- How to import accounts.
- How polling works.
- How to switch active Codex auth.
- Restart Codex requirement.

- [ ] **Step 2: Run full tests**

Run: `go test ./...`

Expected: pass.

- [ ] **Step 3: Run build**

Run: `go build ./cmd/codex-quota-monitor`

Expected: pass.

- [ ] **Step 4: Commit implementation**

Run:

```bash
git add .
git commit -m "feat: add codex quota monitor"
```
