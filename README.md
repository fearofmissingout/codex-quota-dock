# Codex Quota Dock

Native desktop tool for monitoring multiple Codex ChatGPT auth profiles, switching the active Codex auth file, and keeping local profile backup/export workflows simple.

Current releases publish only the native Windows 11 app and the native macOS app. The older Go/Fyne builds remain in repository history, but are no longer release targets.

## Features

- Import the current `~/.codex/auth.json` or a selected auth JSON file.
- Create and edit profiles by pasting auth JSON directly.
- Monitor current and pinned accounts in a compact floating window.
- Show separate 5h and weekly quota rows with remaining percent, reset time, and low-quota coloring.
- Configure separate 5h and weekly low-quota thresholds.
- Switch the active Codex auth file with a backup and optional Codex restart.
- Export/import local profile backups for moving to another machine.
- Restore the latest auth backup if a switch needs to be rolled back.
- Start at login on Windows and macOS.
- Check GitHub Releases for updates and install after user confirmation.
- Show health diagnostics for auth/profile/startup/version state.
- Native macOS app built with Swift/AppKit, sharing the same local profile storage.
- Native Windows 11 app built with C++/Win32, sharing the same local profile storage.

## Storage

Profile data is stored under the user config directory:

```text
Windows: %APPDATA%\codex-quota-dock
macOS:   ~/Library/Application Support/codex-quota-dock
```

Files:

- `profiles.json` stores profile metadata only.
- `profiles/<profile-id>/auth.json` stores each imported auth file.
- `backups/auth-YYYYMMDD-HHMMSS.json` stores active auth backups made before switching.

The app does not print token values in profile metadata, diagnostics, or normal UI text.

## Download

Download the latest build from the [GitHub Releases](https://github.com/fearofmissingout/codex-quota-dock/releases) page.

Choose the file for your platform:

- Windows 11 native: `codex-quota-dock-native-windows-amd64.zip`
- macOS universal native: `codex-quota-dock-native-macos-universal.zip`

The maintained desktop builds are the native Windows 11 app and the native macOS app. The older Go/Fyne Windows, macOS, and Linux builds are no longer published in new releases.

Windows 10 and Linux are not current release targets.

The macOS universal package runs natively on Apple Silicon and Intel Macs.

## macOS Gatekeeper

The macOS builds are packaged as `.app` bundles and ad-hoc signed, but they are not Apple-notarized because this project does not currently use a paid Apple Developer ID certificate. If macOS says Apple cannot verify the app:

1. Open `System Settings`.
2. Go to `Privacy & Security`.
3. Find the blocked `Codex Quota Dock.app` message.
4. Click `Open Anyway`, then confirm `Open`.

## User Guide

For a concise Chinese operation guide, see [docs/user-guide.zh-CN.md](docs/user-guide.zh-CN.md).

For native desktop development handoff and branch rules, see [docs/native-dev-handoff.zh-CN.md](docs/native-dev-handoff.zh-CN.md).

## Safety Notes

- The app stores auth files locally on your machine.
- Exported backup files contain full auth credentials.
- Do not commit auth files, backups, generated app config folders, or `.codex` contents to Git.
- Switching creates a backup before replacing the active Codex auth.
- Existing Codex windows need to restart after switching auth. The app can do this automatically after confirmation, or you can restart Codex manually.

## Build

### Native macOS

The Swift/AppKit app lives in `native/macos/CodexQuotaDock` and requires macOS with Xcode Command Line Tools:

```sh
cd native/macos/CodexQuotaDock
swift test
swift build
```

Package a native `.app` zip:

```sh
VERSION=0.7.0 sh ./scripts/package-macos-native.sh universal
```

The native app uses the same local profile directory: `~/Library/Application Support/codex-quota-dock`.

### Native Windows 11

The C++/Win32 app lives in `native/windows/CodexQuotaDock` and requires Visual Studio 2022 Build Tools with the Windows SDK:

```powershell
.\scripts\build-windows-native.ps1
```

The native Windows app uses the same local profile directory: `%APPDATA%\codex-quota-dock`.

## Release Artifacts

The GitHub Actions workflow `Build desktop artifacts` builds the downloadable assets listed in the Download section. macOS artifacts are packaged as `.app` bundles with an `.icns` icon and ad-hoc signed with `codesign --sign -`; the native macOS app is built with Swift Package Manager on macOS 14 runners.
