# Codex Quota Monitor

Windows desktop tool for monitoring multiple Codex ChatGPT auth profiles and switching the active Codex auth file.

## Features

- Import the current `~/.codex/auth.json` or another saved auth JSON file.
- Store multiple local profiles, such as `company` and `pro`.
- Manually refresh quota for one profile or all profiles.
- Optional automatic refresh: off, 1 minute, 5 minutes, or 10 minutes.
- Display Codex quota windows such as 5h and weekly usage, remaining percent, and reset time.
- Switch the active Codex auth file with a backup and restart reminder.

## Storage

Profile data is stored under the user config directory:

```text
%APPDATA%\codex-quota-monitor
```

Files:

- `profiles.json` stores profile metadata only.
- `profiles/<profile-id>/auth.json` stores each imported auth file.
- `backups/auth-YYYYMMDD-HHMMSS.json` stores active auth backups made before switching.

The app does not print token values in the UI or metadata.

## Usage

1. Start the app.
2. Enter an alias such as `company` or `pro`.
3. Click `Import current auth` to import the active Codex auth, or `Import auth file` to select another auth JSON.
4. Click `Refresh selected` or `Refresh all` to query quota immediately.
5. Choose an automatic refresh interval if desired.
6. Select a profile and click `Switch selected` to replace `~/.codex/auth.json`.
7. Restart Codex after switching accounts.

## Build

```powershell
go test ./...
go build ./cmd/codex-quota-monitor
```

The build creates `codex-quota-monitor.exe` in the repository root.
