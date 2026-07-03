# Codex Quota Dock

Language: English | [简体中文](README.zh-CN.md)

Codex Quota Dock is a native desktop monitor for people who use more than one
Codex ChatGPT auth profile. It watches 5-hour and weekly quota, keeps local
profiles organized, and can switch the active Codex `auth.json` with a backup
first.

The maintained builds are now native desktop apps:

- Windows 11: C++ / Win32
- macOS 13+: Swift / AppKit / SwiftUI, universal Intel + Apple Silicon package

The old Go/Fyne app and Linux builds remain in the repository history, but they
are no longer current release targets.

## What It Does

- Monitors multiple saved auth profiles and shows 5h / weekly quota separately.
- Shows current and pinned profiles in a compact floating monitor.
- Imports the current Codex auth file or a selected auth JSON file.
- Lets you create and edit profiles by pasting auth JSON.
- Switches `~/.codex/auth.json` or `$CODEX_HOME/auth.json` after making a backup.
- Can restart Codex after switching auth, with configurable Codex launch target.
- Supports auto-switch rules, including quota-priority mode for P0 accounts.
- Exports and imports local profile backups for moving to another machine.
- Restores the latest active-auth backup when a switch needs to be rolled back.
- Shows local usage analysis from Codex session logs / SQLite state.
- Runs health checks for active auth, profiles, startup, version, and Codex logs.
- Checks GitHub Releases for updates.
- Supports start at login on Windows and macOS.

## Download

Download the latest release from
[GitHub Releases](https://github.com/fearofmissingout/codex-quota-dock/releases).

Choose the asset for your platform:

| Platform | Asset |
| --- | --- |
| Windows 11 | `codex-quota-dock-native-windows-amd64.zip` |
| macOS 13+ | `codex-quota-dock-native-macos-universal.zip` |

Windows 10 and Linux are not maintained release targets.

## Install

### Windows 11

1. Download `codex-quota-dock-native-windows-amd64.zip`.
2. Extract the zip.
3. Run `codex-quota-dock-native.exe`.
4. Open `Config` and import your current Codex auth profile.

If Windows SmartScreen warns on first launch, review the file source and choose
to run it only if you trust the release you downloaded.

### macOS

1. Download `codex-quota-dock-native-macos-universal.zip`.
2. Extract the zip.
3. Move `Codex Quota Dock.app` to `Applications` if desired.
4. Open the app.

The macOS app is ad-hoc signed but not Apple-notarized. This project currently
does not use a paid Apple Developer ID certificate. If macOS says Apple cannot
verify the app:

1. Open `System Settings`.
2. Go to `Privacy & Security`.
3. Find the blocked `Codex Quota Dock.app` message.
4. Click `Open Anyway`, then confirm `Open`.

## Quick Start

1. Start `Codex Quota Dock`.
2. Open `Config`.
3. Click `Import Current` to import the auth file Codex is using now.
4. Add other accounts with `Import File` or `New Profile`.
5. Set aliases that are easy to recognize.
6. Pin important profiles so they stay visible in the floating monitor.
7. Click `Refresh` to fetch quota.
8. Select a profile and click `Switch` when you want Codex to use that account.

After switching auth, Codex must reload its login state. You can let the app try
to close and restart Codex automatically, or restart Codex yourself.

For a Chinese operation guide, see
[docs/user-guide.zh-CN.md](docs/user-guide.zh-CN.md).

## Quota Display

Quota is shown as two independent windows:

- `5h`: short rolling usage window
- `weekly`: weekly usage window

The app treats the lower remaining quota as the practical bottleneck when it
needs a single "can I keep using this account?" signal. Low-quota coloring and
alerts are configurable separately for 5h and weekly quota.

Quota refresh is intentionally not high-frequency. You can refresh manually, and
the polling interval can be set to 1, 5, or 10 minutes.

## Auto Switch

Auto switch is optional. The app can suggest or perform a switch when the active
profile becomes low and another saved profile is healthier.

Supported modes:

- `Off`: never auto-switch.
- `Notify`: only show a recommendation.
- `When Codex Closed`: switch only when Codex is not running.
- `When Idle`: switch when Codex is closed or the machine has been idle long enough.

Important settings:

- `Switch away`: active profile is considered low at or below this quota.
- `Switch to`: candidate profile must be at or above this quota.
- `Idle min`: required idle time for `When Idle`.
- `Cooldown min`: minimum time between automatic switches.
- `Restart Codex automatically after switching`: tries to close and reopen Codex.
- `Codex launch target`: auto-detected or manually configured Codex app path / target.

### Quota Priority Mode

Quota priority mode is for accounts that you intentionally want to consume first,
for example a company-provided account with usage expectations.

Profile priority uses lower numbers as higher priority:

- `P0` is highest priority.
- `P5` is lower priority than `P0`.

When quota priority mode is enabled, the app can switch back to the highest
priority recovered account while respecting idle and cooldown rules. A common
setup is:

- Company account: `P0`
- Personal fallback account: `P5`
- Priority 5h recovery threshold: `99%`
- Priority weekly recovery threshold: `0%`

With that setup, if the P0 account's 5h quota refreshes, the app can switch back
to P0 when it is safe to do so, instead of continuing to spend the lower-priority
fallback account.

## Local Usage Analysis

The `Usage` tab analyzes local Codex activity on this machine. It is separate
from ChatGPT quota and should not be treated as billing data.

The native apps read local Codex session files and SQLite state when available,
then summarize:

- today
- last 7 days
- last 30 days
- overall token usage
- daily usage chart data
- parsing / access issues

The scanner is designed for low-frequency UI refresh, not continuous log tailing.

## Data and Privacy

Codex Quota Dock is a local-first tool, but it does store auth files because
switching accounts requires replacing Codex's active auth file.

Profile data is stored under the current user's app config directory:

```text
Windows: %APPDATA%\codex-quota-dock
macOS:   ~/Library/Application Support/codex-quota-dock
```

Important files:

```text
profiles.json
profiles/<profile-id>/auth.json
backups/auth-YYYYMMDD-HHMMSS.json
settings.json
```

Active Codex auth is resolved as:

```text
if CODEX_HOME is set: $CODEX_HOME/auth.json
otherwise:            ~/.codex/auth.json
```

Network access:

- Quota refresh calls ChatGPT's usage endpoint with the selected auth token.
- Update checks call GitHub Releases.

Safety notes:

- Exported backups contain full auth credentials.
- Do not commit auth files, exported backups, app config folders, or `.codex`
  contents to Git.
- The app does not intentionally print token values in profile metadata,
  diagnostics, or normal UI text.
- Use this tool only with accounts you own or are authorized to use.

## Project Layout

```text
native/windows/CodexQuotaDock   Native Windows 11 app
native/macos/CodexQuotaDock     Native macOS app
assets/icon                     Shared source icons
scripts                         Build and packaging scripts
docs                            User guides, release notes, handoff docs
cmd, internal                   Legacy Go/Fyne implementation and shared history
```

## Build From Source

### Windows 11 Native

Requirements:

- Windows 11
- Visual Studio 2022 Build Tools
- Windows SDK
- CMake 3.24+

Build and test:

```powershell
.\scripts\build-windows-native.ps1 -Configuration Release -Arch x64
```

Output:

```text
dist/codex-quota-dock-native-windows-amd64.zip
```

### macOS Native

Requirements:

- macOS 13+
- Xcode Command Line Tools
- Swift 5.10+

Test and build:

```sh
cd native/macos/CodexQuotaDock
swift test
swift build
```

Package a universal `.app` zip:

```sh
# from the repository root
VERSION=0.9.0 sh ./scripts/package-macos-native.sh universal
```

Output:

```text
dist/codex-quota-dock-native-macos-universal.zip
```

## Development Workflow

Day-to-day development should start from `dev`:

```sh
git fetch origin
git checkout dev
git pull --ff-only origin dev
git checkout -b codex/<short-topic>
```

Release flow:

1. Merge tested work into `dev`.
2. Merge `dev` into `main` when preparing a release.
3. Update version constants and release notes.
4. Tag `vX.Y.Z`.
5. Let GitHub Actions publish the native Windows and macOS assets.

The workflow at `.github/workflows/build.yml` builds:

- native macOS universal package on macOS runners
- native Windows amd64 package on Windows runners
- GitHub Release assets when a `v*` tag is pushed

For a more detailed native handoff note, see
[docs/native-dev-handoff.zh-CN.md](docs/native-dev-handoff.zh-CN.md).

## Release Notes

- [v0.9.0](docs/v0.9.0-release-notes.md)
- [v0.8.0](docs/v0.8.0-release-notes.md)
- [v0.7.0](docs/v0.7.0-release-notes.md)
- [older notes](docs)

## Not Affiliated

This is an independent desktop utility for local Codex auth/profile management.
It is not an official OpenAI product.
