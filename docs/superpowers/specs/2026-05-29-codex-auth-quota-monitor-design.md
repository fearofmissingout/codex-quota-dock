# Codex Auth Quota Monitor Design

## Goal

Build a Windows-friendly Go desktop tool that monitors one or more Codex ChatGPT auth profiles, shows their Codex quota windows such as 5h and weekly remaining usage, and lets the user switch the active Codex auth file with one click.

The user has two accounts:

- A company/team/enterprise account that should be preferred for KPI and normal work.
- A Pro account that should be used when the company quota is low or exhausted.

The current manual workflow is replacing the Codex auth file. The tool will make that workflow visible, safer, and faster.

## Evidence And External Contracts

The local Codex auth file is `~/.codex/auth.json`. It contains:

- `auth_mode`
- `tokens.id_token`
- `tokens.access_token`
- `tokens.refresh_token`
- `tokens.account_id`
- `last_refresh`

The official Codex source shows that quota data is fetched from usage endpoints:

- Codex API style: `GET /api/codex/usage`
- ChatGPT backend style: `GET /wham/usage`

The response includes `plan_type`, `rate_limit`, optional `additional_rate_limits`, optional `credits`, and optional `rate_limit_reached_type`.

The primary and secondary quota windows include:

- `used_percent`
- `limit_window_seconds`
- `reset_after_seconds`
- `reset_at`

The tool will model these fields directly instead of scraping UI text.

## Recommended Approach

Use Go with the Windows-native `walk` desktop UI library.

Reasons:

- The target environment is Windows, and the local machine does not have a C toolchain available.
- `walk` builds Windows desktop UI from Go without requiring the Fyne/OpenGL desktop toolchain.
- The project needs simple forms, lists, progress bars, refresh buttons, dialogs, and file pickers, which `walk` handles well.
- The core business logic can be tested without launching the UI.

The app will be local-only. It will not upload auth files, tokens, quota data, or account metadata to any third-party service.

## User Experience

The main window shows:

- Active Codex account at the top.
- A profile list with aliases such as `company` and `pro`.
- Each profile's plan type, account ID suffix, last refresh time, request status, and quota cards.
- Quota cards for each returned limit bucket, especially the Codex bucket.
- Manual `Refresh` and `Refresh All` controls.
- A `Switch to this account` button per profile.

Quota display:

- Show used percent and remaining percent.
- Show reset time in local time.
- Label common windows as `5h`, `daily`, `weekly`, `monthly`, or `annual` using the same approximate-duration logic as Codex.
- Mark stale data when the last successful quota fetch is older than the configured polling interval.
- Mark exhausted windows clearly when `used_percent` is 100 or `rate_limit_reached_type` is present.

Switching flow:

1. User clicks `Switch to this account`.
2. The app asks for confirmation and names the target alias.
3. The app backs up the current `~/.codex/auth.json`.
4. The app atomically replaces `~/.codex/auth.json` with the chosen profile auth.
5. The app shows a success dialog telling the user to restart Codex for the new auth to take effect.

## Polling And Refresh Policy

Manual refresh is the primary interaction.

Automatic polling is a low-frequency safety net:

- Default interval: 5 minutes.
- Configurable options: off, 1 minute, 5 minutes, 10 minutes.
- The app refreshes profiles sequentially with short spacing between requests to avoid bursts.
- Manual refresh cancels or supersedes any pending refresh for the same profile.

The 5-minute default keeps the display useful while avoiding rapid polling. The 1-minute option is available for short periods when the user wants closer monitoring, and manual refresh remains the primary interaction.

## Data Storage

Application data lives under a user-scoped config directory:

- Windows default: `%APPDATA%\codex-quota-monitor`

Files:

- `profiles.json`: profile metadata, aliases, active flags, polling settings, and non-secret display cache.
- `profiles/<profile-id>/auth.json`: copied Codex auth for that profile.
- `backups/auth-YYYYMMDD-HHMMSS.json`: backups made before switching active auth.

The app never stores tokens in logs. UI and logs only display:

- Profile alias.
- Plan type.
- Account ID suffix, such as the final 6 characters.
- Error category.
- Last refresh timestamp.

## Modules

### Auth Module

Responsibilities:

- Parse Codex auth JSON.
- Validate that ChatGPT token fields are present.
- Redact token-bearing structures for logging and UI.
- Import an auth file into a profile.
- Identify the active Codex account by comparing the current auth account ID to stored profiles.

Public concepts:

- `AuthFile`
- `TokenSet`
- `Profile`
- `ProfileStore`

### Quota Module

Responsibilities:

- Build authenticated requests using `Authorization: Bearer <access_token>`.
- Send `ChatGPT-Account-Id` when `tokens.account_id` is present.
- Fetch usage data from the configured base URL.
- Parse the Codex usage payload into a stable internal snapshot.
- Convert windows into display labels, used percent, remaining percent, and reset times.

Default base URL:

- `https://chatgpt.com/backend-api`

Default path:

- `/wham/usage`

The implementation keeps the endpoint configurable from the first version so advanced users can select Codex API style URLs such as `/api/codex/usage`.

### Switcher Module

Responsibilities:

- Resolve current Codex auth path, defaulting to `~/.codex/auth.json`.
- Backup the current active auth before switching.
- Write replacement auth through a temporary file and atomic rename.
- Preserve readable file permissions.
- Return a result that the UI can present as a restart-required dialog.

Failure behavior:

- If backup fails, do not switch.
- If writing the replacement fails, keep the original active auth.
- If atomic rename fails after backup, report the error and leave the backup path visible.

### UI Module

Responsibilities:

- Render profile list and quota cards.
- Provide import, refresh, refresh all, edit alias, remove profile, and switch actions.
- Show confirmation and success/error dialogs.
- Keep UI responsive while quota requests run in background goroutines.

The first version will keep the UI practical and dense rather than decorative.

## Error Handling

Quota fetch errors:

- `401` or `403`: show `auth expired or unauthorized`.
- `429`: show `rate limited while checking quota`.
- Network timeout: show `network error`.
- Invalid JSON: show `unexpected usage response`.
- Missing quota fields: show `quota unavailable`.

Profile import errors:

- Invalid JSON.
- Missing `tokens.access_token`.
- Missing or empty profile alias.
- Duplicate alias.

Switch errors:

- Missing target profile auth.
- Current auth path missing and parent directory cannot be created.
- Backup failure.
- Replacement write failure.

All errors should be visible in the UI without exposing token values.

## Security And Privacy

The tool handles high-value auth tokens, so the design is intentionally conservative:

- Do not print tokens to stdout, logs, dialogs, or test output.
- Do not include auth profile directories in git.
- Keep profile auth files in the user's app data directory.
- Back up active auth before every switch.
- Do not automatically switch accounts based on quota in the first version.
- Require explicit user action for account switching.

## Testing Strategy

Use Go unit tests for non-UI logic:

- Auth JSON parsing succeeds for Codex ChatGPT auth.
- Auth parsing rejects missing access tokens.
- Redaction removes token values.
- Usage payload parsing maps primary and secondary windows.
- Window label conversion maps approximate 300 minutes to `5h` and approximate 10080 minutes to `weekly`.
- Remaining percent is `100 - used_percent`, clamped to `0..100`.
- Reset timestamps format in local time.
- Profile import copies auth and writes metadata.
- Switcher backs up current auth before replacement.
- Switcher does not replace active auth if backup fails.
- Switcher writes through a temporary file.

Use a local HTTP test server for quota requests:

- Verify `Authorization` header.
- Verify `ChatGPT-Account-Id` header.
- Verify handling for 200, 401, 429, and malformed JSON.

UI verification:

- `go test ./...` must pass.
- `go build ./cmd/codex-quota-monitor` must produce the desktop executable.
- Manual smoke test launches the app, imports a profile, refreshes quota, and opens the switch confirmation dialog.

## Initial Scope

Included:

- Go module and Windows desktop app using `walk`.
- Multi-profile auth storage.
- Import current or selected auth file.
- Manual refresh and low-frequency polling.
- Quota display for primary and secondary windows.
- One-click auth switching with backup and restart prompt.
- Focused tests for core logic.

Not included in the first version:

- Automatic account switching.
- Editing or refreshing OAuth tokens.
- Uploading telemetry.
- System tray background daemon.
- macOS/Linux packaging.
- Password-protected encrypted profile storage.

## Open Decisions Resolved

- Polling interval: default 5 minutes, configurable to off, 1, 5, or 10 minutes.
- Switching behavior: explicit user action only.
- Auth target path: default `~/.codex/auth.json` for the first version.
- UI framework: Windows-native `walk`.
- Quota source: direct usage endpoint polling, not UI scraping.
