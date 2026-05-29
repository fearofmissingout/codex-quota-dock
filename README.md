# Codex Quota Dock

Cross-platform desktop tool for monitoring multiple Codex ChatGPT auth profiles and switching the active Codex auth file.

This branch uses [Fyne](https://fyne.io/) so it can target Windows, macOS, and Linux from the same Go UI code. The Windows-only Walk/Mica floating window implementation stays on the Windows branch.

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

## Usage

1. Start the app.
2. Enter an alias such as `company` or `pro`.
3. Click `Import Current` to import the active Codex auth, or `Import File` to select another auth JSON.
4. Select a profile to view its quota detail.
5. Click `Refresh Selected`, `Refresh Visible`, or `Refresh All` to query quota immediately.
6. Pin profiles you want to keep in the compact list.
7. Choose an automatic refresh interval if desired.
8. Select a profile and click `Switch Selected` to replace the active Codex auth file.
9. Restart Codex after switching accounts.

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
