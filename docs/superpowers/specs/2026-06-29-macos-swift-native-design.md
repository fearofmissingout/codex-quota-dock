# macOS Swift Native Design

## Summary

Codex Quota Dock v0.5.0 will introduce a native macOS edition built with Swift and AppKit while keeping the existing Go/Fyne edition as the stable cross-platform release for Windows, Linux, and fallback macOS users.

The native macOS edition is not a rewrite of every feature at once. It is a focused replacement for the macOS desktop shell: menu bar presence, floating quota monitor, profile management, auth switching, and Codex restart. Existing local file formats remain the compatibility boundary so users can move between the Go/Fyne app and the Swift app without migration steps.

## Goals

- Provide a macOS-native user experience for the floating monitor, settings window, menu bar item, notifications, and app bundle.
- Keep the existing app data layout compatible:
  - `profiles.json`
  - `profiles/<profile-id>/auth.json`
  - `backups/auth-YYYYMMDD-HHMMSS.json`
- Treat `settings.json` as a new v0.5 native settings file. If it is absent, use the same defaults as the Go app.
- Keep auth files as local plaintext files. Do not introduce Keychain or system encryption in v0.5.0.
- Build and test the Swift app on real macOS through GitHub Actions macOS runners.
- Ship separate macOS Intel and Apple Silicon `.app` zip artifacts.
- Preserve the current Go/Fyne release path for Windows and Linux.

## Non-Goals

- Do not rewrite Windows or Linux in v0.5.0.
- Do not split the repository before the macOS MVP has user feedback.
- Do not introduce a background daemon or privileged helper.
- Do not implement full automatic self-update for the Swift app in v0.5.0. The native app may check GitHub Releases and open the release page.
- Do not implement local usage charts in the first Swift MVP. Quota monitoring, profile switching, and profile editing come first.
- Do not change the auth switching semantics: switching still backs up the active Codex auth file before replacement.

## Architecture

The repository will contain a new macOS app under `native/macos/CodexQuotaDock`. It will be a Swift Package based macOS app using Swift, AppKit, and a small amount of SwiftUI only where it simplifies forms.

The app is split into three layers:

1. **Domain layer**
   - Owns JSON models, profile store, settings store, auth path resolution, backup handling, quota API calls, and Codex process restart.
   - Contains no AppKit view code.
   - Tested with XCTest.

2. **Application layer**
   - Owns refresh scheduling, selected profile state, pinned profiles, switch workflows, and error presentation models.
   - Coordinates domain services and emits view state.
   - Tested with XCTest using fake stores and fake quota clients.

3. **UI layer**
   - Owns `NSStatusItem`, floating `NSPanel`, settings `NSWindow`, profile editor controls, and native alerts.
   - Uses `NSVisualEffectView` for the monitor background and standard AppKit controls for forms and lists.
   - Verified on macOS via Actions build and manual smoke tests on a real Mac.

## Data Compatibility

The Swift app must read the existing Go app config directory:

```text
~/Library/Application Support/codex-quota-dock
```

The profile store remains compatible with the Go `internal/profile` package. The Swift model names can be idiomatic, but the encoded JSON keys must match the existing files: `id`, `alias`, `account_id`, `account_suffix`, `auth_mode`, `pinned`, `last_refresh`, and `created_at`.

The Go/Fyne app does not currently persist a standalone settings file. The Swift app may add:

```text
settings.json
```

This file is a forward-compatible native settings file. The Swift app must run correctly when it is missing. The Go/Fyne app may ignore it until a later compatibility pass.

Auth path resolution follows the existing README behavior:

1. If `CODEX_HOME` is set, use `$CODEX_HOME/auth.json`.
2. Otherwise use `~/.codex/auth.json`.

Backup export/import must not be part of the first Swift MVP unless the core profile editor is already stable. Reading existing backups and creating switch backups is required.

## macOS User Experience

The native app starts as a menu bar utility. It should not show a Dock icon by default after launch unless settings are open.

Primary surfaces:

- **Menu bar item**
  - Shows app icon.
  - Opens the floating monitor.
  - Offers Refresh, Settings, and Quit.

- **Floating monitor**
  - Borderless `NSPanel`.
  - Movable by dragging anywhere on the monitor.
  - Uses `NSVisualEffectView` with the current system appearance.
  - Shows active profile plus pinned profiles.
  - Shows 5h and weekly quota on separate lines.
  - Provides Refresh, Switch, and Settings controls.

- **Settings window**
  - Standard titled macOS window.
  - Profile list with active/pinned markers.
  - Alias editor.
  - Auth JSON editor.
  - Import Current, Import File, New Profile, Delete, Save, Pin, Switch.
  - Polling interval and quota threshold settings.
  - Health section with auth path, profile count, current version, and Codex restart availability.

## Quota Behavior

Quota requests continue to use ChatGPT's backend usage endpoint with the selected profile auth. The app should avoid aggressive polling:

- Manual refresh is always available.
- Polling choices are 1, 5, and 10 minutes.
- Default remains 5 minutes unless existing settings say otherwise.

Threshold notifications apply to both the 5h and weekly limits. The effective availability is the stricter of the two because either exhausted window can block use. The UI should make this visible by showing both rules and highlighting the lowest remaining usable quota.

## Auth Switching

Switching a profile performs the same sequence as the Go app:

1. Resolve active Codex auth path.
2. Read the current active auth file if present.
3. Write a timestamped backup under the app backup directory.
4. Replace active Codex auth with the selected profile auth.
5. Optionally restart Codex.
6. Show a clear result alert with backup path and restart result.

The Swift app may use `NSWorkspace` and process inspection to restart Codex. If Codex cannot be restarted, the app must still complete the auth switch and tell the user to restart Codex manually.

## Build And Release

GitHub Actions will add a macOS native build job:

- Build on `macos-14` or newer.
- Produce Apple Silicon and Intel artifacts.
- Ad-hoc sign the `.app` with `codesign --sign -`.
- Zip with `ditto --sequesterRsrc --keepParent`.
- Upload artifacts named:
  - `codex-quota-dock-native-macos-arm64.zip`
  - `codex-quota-dock-native-macos-amd64.zip`

The existing Go/Fyne macOS artifact can remain during the preview period. Release notes must clearly label the Swift app as the native preview until it reaches feature parity.

## Testing Strategy

Local Windows development can edit Swift files and validate repository structure, but it cannot prove AppKit behavior. The authoritative checks for Swift are:

- `swift test` on macOS Actions for domain and application layer tests.
- `xcodebuild` or `swift build` on macOS Actions for app compilation.
- Manual smoke testing on a real Mac before a stable release.

Required XCTest coverage:

- Existing profile JSON can decode.
- Swift profile writes remain readable by the Go app.
- Auth path resolution honors `CODEX_HOME`.
- Switching creates a backup before replacing auth.
- Quota response parsing handles available, exhausted, and missing fields.
- Polling interval and threshold settings round-trip.

Manual macOS smoke tests:

- App opens from the zipped `.app` after allowing it in Privacy & Security.
- Menu bar icon appears.
- Floating monitor opens, drags smoothly, and respects system light/dark mode.
- Import Current creates a profile.
- Switch replaces `~/.codex/auth.json` and creates a backup.
- Optional Codex restart closes and reopens Codex when available.

## Rollout

v0.5.0 is a native macOS preview. The release should keep Go/Fyne artifacts for all platforms and add the native macOS artifacts as the recommended Mac download.

v0.5.1 can add native update installation and usage charts after the MVP has real feedback.

v0.6.0 can start the Windows native C++ edition after the macOS boundaries prove stable.
