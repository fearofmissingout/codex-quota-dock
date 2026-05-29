# Codex Quota Dock

Cross-platform desktop tool for monitoring multiple Codex ChatGPT auth profiles and switching the active Codex auth file.

The app uses [Fyne](https://fyne.io/) so it can target Windows, macOS, and Linux from the same Go UI code.

## Features

- Import the current `~/.codex/auth.json` or another saved auth JSON file.
- Store multiple local profiles, such as `company` and `pro`.
- Compact desktop window for the current and pinned Codex accounts.
- Borderless draggable monitor on Windows, macOS, and Linux/X11. Linux Wayland or unsupported desktops fall back to a normal title bar so the window remains movable.
- System tray menu for showing the window, refreshing visible profiles, and quitting.
- Manually refresh the selected profile, visible profiles, or all profiles.
- Optional automatic refresh: off, 1 minute, 5 minutes, or 10 minutes.
- Display Codex quota windows such as 5h and weekly usage, remaining percent, and reset time.
- Switch the active Codex auth file with a backup and a readable restart reminder, including a copyable backup path.

## Storage

Profile data is stored under the user config directory:

```text
Windows: %APPDATA%\codex-quota-dock
macOS:   ~/Library/Application Support/codex-quota-dock
Linux:   $XDG_CONFIG_HOME/codex-quota-dock or ~/.config/codex-quota-dock
```

Files:

- `profiles.json` stores profile metadata only.
- `profiles/<profile-id>/auth.json` stores each imported auth file.
- `backups/auth-YYYYMMDD-HHMMSS.json` stores active auth backups made before switching.

The app does not print token values in the UI or metadata.

## Codex Auth Path

When switching profiles, the active Codex auth file is resolved in this order:

1. `CODEX_HOME/auth.json`, when `CODEX_HOME` is set.
2. `~/.codex/auth.json`, using the current user's home directory.

The app does not scan arbitrary folders for auth files. Profiles are imported explicitly from the active Codex auth file or from a file the user selects.

## Download

Download the latest build from the [GitHub Releases](https://github.com/fearofmissingout/codex-quota-dock/releases) page.

Choose the file for your platform:

- Windows: `codex-quota-dock-windows-amd64.zip`
- macOS Apple Silicon: `codex-quota-dock-macos-arm64.zip`
- Linux: `codex-quota-dock-linux-amd64.zip`

Unzip the package and run the executable inside. On macOS or Linux, you may need to mark the file as executable:

```sh
chmod +x ./codex-quota-dock-*
```

## User Guide

### First Run

1. Start `codex-quota-dock`.
2. Click `Config` in the floating monitor to open the settings window.
3. In the `Alias` field, enter a short name for the account, such as `company`, `team`, or `pro`.
4. Click `Import Current` to import the current Codex auth from `CODEX_HOME/auth.json` or `~/.codex/auth.json`.
5. Repeat the same steps for every Codex account you want to monitor.

Use `Import File` when you already have another saved auth JSON file and want to add it manually. The app only imports files you explicitly select.

### Floating Monitor

The small floating window is designed for daily monitoring.

- It shows the active profile and any pinned profiles.
- Each profile shows quota information on separate lines, including `5h` and `weekly` windows when available.
- Click `Refresh` to query the visible profiles immediately.
- Select a profile and click `Switch` to make it the active Codex auth profile.
- Click `Config` to open the full settings window.
- Double-click a profile to open the settings window with that profile selected.

The monitor is borderless and draggable on Windows, macOS, and Linux/X11. On unsupported Linux desktop sessions, it falls back to a normal movable window title bar.

### Settings Window

The settings window is where you manage profiles and detailed quota data.

- Select a profile from the list to view quota details.
- Edit the alias in the `Alias` field.
- Edit the saved auth JSON in the auth text box if you need to update a profile manually.
- Click `Save Profile` after changing the alias or auth content.
- Click `Refresh Selected`, `Refresh Visible`, or `Refresh All` to update quota data.
- Click `Pin` to keep a profile visible in the floating monitor.
- Use the refresh interval selector to choose `off`, `1 minute`, `5 minutes`, or `10 minutes`.

### Switching Accounts

Switching a profile replaces the active Codex auth file with the selected saved profile.

1. Select the profile you want to use.
2. Click `Switch` in the floating monitor, or `Switch Selected` in the settings window.
3. Confirm the switch.
4. The app creates a timestamped backup of the previous active auth file.
5. Restart Codex so existing Codex windows reload the new auth file.

The restart reminder includes the backup path. You can disable this reminder from the settings window, but Codex still needs to be restarted after an auth switch.

### Refresh Behavior

Manual refresh is usually enough for normal use. If you enable automatic refresh, prefer the slowest interval that works for you.

- `off`: no background polling.
- `1 minute`: useful when you are actively comparing accounts.
- `5 minutes`: balanced for regular monitoring.
- `10 minutes`: lowest background traffic.

Quota requests use the imported auth token for each profile. If a request times out or fails, the profile remains stored and you can retry with `Refresh`.

### Safety Notes

- The app stores auth files locally on your machine.
- Do not commit auth files, backups, or app config directories to Git.
- Switching creates a backup before replacing the active Codex auth.
- The app does not reload a running Codex session. Restart Codex after switching.
- If `CODEX_HOME` is set, switching targets `CODEX_HOME/auth.json`; otherwise it targets `~/.codex/auth.json`.

## Build

Fyne desktop builds require CGO and a C compiler.

Prerequisites:

- Windows: install MinGW-w64, TDM-GCC, MSYS2, or another GCC toolchain and make sure `gcc` is in `PATH`.
- macOS: install Xcode Command Line Tools.
- Linux: install `gcc` and the desktop/OpenGL development packages required by Fyne for your distribution.

Windows:

```powershell
.\scripts\build.cmd
```

If you prefer PowerShell, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

macOS/Linux:

```sh
./scripts/build.sh
```

Manual Windows build:

```powershell
go test ./...
$env:CGO_ENABLED = "1"
go build -o codex-quota-dock.exe ./cmd/codex-quota-dock
```

Manual macOS/Linux build:

```sh
go test ./...
CGO_ENABLED=1 go build -o codex-quota-dock ./cmd/codex-quota-dock
```

Without a C compiler, `go test ./...` can still verify the non-GUI/core path through the no-CGO fallback. Building the real desktop UI requires CGO.

## Release Artifacts

The GitHub Actions workflow `Build desktop artifacts` builds downloadable artifacts for:

- Windows amd64: `codex-quota-dock-windows-amd64.exe`
- macOS amd64: `codex-quota-dock-macos-amd64`
- macOS arm64: `codex-quota-dock-macos-arm64`
- Linux amd64: `codex-quota-dock-linux-amd64`

Each platform is compiled on its native runner so the CGO desktop dependencies match the target OS. Local generated folders such as `dist/` and `fyne-cross/` are ignored by Git.
