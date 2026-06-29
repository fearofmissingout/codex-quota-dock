# Codex Quota Dock

Cross-platform desktop tool for monitoring multiple Codex ChatGPT auth profiles, switching the active Codex auth file, and keeping local profile backup/export workflows simple.

The stable app uses [Fyne](https://fyne.io/) so one Go codebase can target Windows, macOS, and Linux. Starting in v0.5.0, the repository also includes a native macOS Swift/AppKit preview. Starting in v0.6.0, it includes a Windows 11 native C++ preview.

## Features

- Import the current `~/.codex/auth.json` or a selected auth JSON file.
- Create and edit profiles by pasting auth JSON directly.
- Monitor current and pinned accounts in a compact floating window.
- Show separate 5h and weekly quota rows with remaining percent, reset time, and low-quota coloring.
- Configure separate 5h and weekly low-quota thresholds.
- Switch the active Codex auth file with a backup and optional Codex restart.
- Export/import local profile backups for moving to another machine.
- Restore the latest auth backup if a switch needs to be rolled back.
- Start at login on Windows, macOS, and Linux.
- Check GitHub Releases for updates and install after user confirmation.
- Show health diagnostics for auth/profile/startup/version state.
- Native macOS preview app built with Swift/AppKit, sharing the same local profile storage.
- Native Windows 11 preview app built with C++/Win32, sharing the same local profile storage.

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

The app does not print token values in profile metadata, diagnostics, or normal UI text.

## Download

Download the latest build from the [GitHub Releases](https://github.com/fearofmissingout/codex-quota-dock/releases) page.

Choose the file for your platform:

- Windows 11 native preview: `codex-quota-dock-native-windows-amd64.zip`
- Windows Fyne fallback: `codex-quota-dock-windows-amd64.zip`
- macOS Apple Silicon native preview: `codex-quota-dock-native-macos-arm64.zip`
- macOS Intel native preview: `codex-quota-dock-native-macos-x86_64.zip`
- macOS Apple Silicon Fyne fallback: `codex-quota-dock-macos-arm64.zip`
- macOS Intel Fyne fallback: `codex-quota-dock-macos-amd64.zip`
- Linux: `codex-quota-dock-linux-amd64.zip`

For macOS, prefer the native preview package if available. The older Go/Fyne macOS package remains available as a fallback while the native app reaches feature parity.

For Windows 11, prefer the native preview package if available. Windows 10 is not a target for the native preview; use the Fyne fallback if you need older Windows support.

On macOS, check your architecture with:

```sh
uname -m
```

`arm64` means Apple Silicon. `x86_64` means Intel.

## macOS Gatekeeper

The macOS builds are packaged as `.app` bundles and ad-hoc signed, but they are not Apple-notarized because this project does not currently use a paid Apple Developer ID certificate. If macOS says Apple cannot verify the app:

1. Open `System Settings`.
2. Go to `Privacy & Security`.
3. Find the blocked `Codex Quota Dock.app` message.
4. Click `Open Anyway`, then confirm `Open`.

## User Guide

For a concise Chinese operation guide, see [docs/user-guide.zh-CN.md](docs/user-guide.zh-CN.md).

## Safety Notes

- The app stores auth files locally on your machine.
- Exported backup files contain full auth credentials.
- Do not commit auth files, backups, generated app config folders, or `.codex` contents to Git.
- Switching creates a backup before replacing the active Codex auth.
- Existing Codex windows need to restart after switching auth. The app can do this automatically after confirmation, or you can restart Codex manually.

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

PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

macOS/Linux:

```sh
./scripts/build.sh
```

Without a C compiler, `CGO_ENABLED=0 go test ./...` can still verify the non-GUI/core path through the no-CGO fallback. Building the real desktop UI requires CGO.

### Native macOS Preview

The Swift/AppKit preview lives in `native/macos/CodexQuotaDock` and requires macOS with Xcode Command Line Tools:

```sh
cd native/macos/CodexQuotaDock
swift test
swift build
```

Package a native `.app` zip:

```sh
VERSION=0.5.0 ./scripts/package-macos-native.sh arm64
VERSION=0.5.0 ./scripts/package-macos-native.sh x86_64
```

The native app uses the same local profile directory: `~/Library/Application Support/codex-quota-dock`.

### Native Windows 11 Preview

The C++/Win32 preview lives in `native/windows/CodexQuotaDock` and requires Visual Studio 2022 Build Tools with the Windows SDK:

```powershell
.\scripts\build-windows-native.ps1
```

The native Windows app uses the same local profile directory: `%APPDATA%\codex-quota-dock`.

## Release Artifacts

The GitHub Actions workflow `Build desktop artifacts` builds the downloadable assets listed in the Download section. macOS artifacts are packaged as `.app` bundles with an `.icns` icon and ad-hoc signed with `codesign --sign -`; the native macOS preview is built with Swift Package Manager on macOS 14 runners.
