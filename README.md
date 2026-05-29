# Codex Quota Monitor

Windows desktop tool for monitoring multiple Codex ChatGPT auth profiles and switching the active Codex auth file.

## Features

- Import the current `~/.codex/auth.json` or another saved auth JSON file.
- Store multiple local profiles, such as `company` and `pro`.
- Minimal always-on-top monitor window for the active Codex account.
- Double-click the monitor, or click `Open`, to show the larger details and configuration window.
- Manually refresh the active profile, one selected profile, or all profiles.
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
2. Use the small monitor window for the active account's 5h and weekly quota summary.
3. Double-click the monitor, or click `Open`, to show details and configuration.
4. Enter an alias such as `company` or `pro`.
5. Click `Import current` to import the active Codex auth, or `Import file` to select another auth JSON.
6. Click `Refresh selected`, `Refresh all`, or the monitor's `Refresh` button to query quota immediately.
7. Choose an automatic refresh interval if desired.
8. Select a profile and click `Switch selected` to replace `~/.codex/auth.json`.
9. Restart Codex after switching accounts.

## Build

```powershell
.\scripts\build.cmd
```

The build creates `codex-quota-monitor.exe` in the repository root. The script uses `-ldflags="-H=windowsgui"` so Windows starts it as a desktop app instead of opening a terminal window.

If you prefer PowerShell, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

Manual build:

```powershell
go test ./...
go build -ldflags="-H=windowsgui" -o codex-quota-monitor.exe ./cmd/codex-quota-monitor
```
