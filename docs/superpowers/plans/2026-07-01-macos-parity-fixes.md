# macOS Parity Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring the native macOS app closer to the native Windows feature set for backup migration, monitor pinning, Touch Bar visibility, and update checks.

**Architecture:** Keep the Swift core small and file-based, matching the existing Windows backup JSON shape without introducing new dependencies. AppKit-only behavior stays in `CodexQuotaDockApp`, while portable data transforms and GitHub release parsing stay in `CodexQuotaDockCore`.

**Tech Stack:** Swift Package Manager, SwiftUI, AppKit, Foundation `URLSession`, local JSON files.

**Current status:** Implemented from a Windows workstation on 2026-07-01. `swift` is not installed on this machine, so macOS `swift test` and Touch Bar verification remain pending for a Mac or CI runner.

---

### Task 1: Backup Export, Import, And Restore

**Files:**
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/BackupStore.swift`
- Test: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/BackupStoreTests.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/NativeAppModel.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/SettingsContentView.swift`

- [x] **Step 1: Write failing tests**

Add tests that create a temp profile store, export backup JSON containing profiles/settings/auth JSON, import it into a second store, and restore the latest active auth backup.

- [ ] **Step 2: Run tests to verify failure**

Run on macOS:

```sh
cd native/macos/CodexQuotaDock
swift test --filter BackupStoreTests
```

Expected before implementation: compile failure because `BackupStore` does not exist.

- [x] **Step 3: Implement BackupStore**

Add `BackupStore.exportBackup(store:settings:)`, `BackupStore.importBackup(into:data:)`, `BackupStore.latestBackup(in:)`, and `BackupStore.restoreLatestBackup(from:to:)` using the existing profile store and auth switch backup directory.

- [x] **Step 4: Wire UI**

Add `Export`, `Import`, and `Restore` buttons next to macOS profile actions. Use `NSSavePanel`/`NSOpenPanel`, call the model methods, and keep status messages non-modal.

- [ ] **Step 5: Verify**

Run:

```sh
cd native/macos/CodexQuotaDock
swift test --filter BackupStoreTests
swift test
```

Expected: all tests pass.

### Task 2: Monitor Pinning

**Files:**
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/SettingsModels.swift`
- Modify: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/SettingsStoreTests.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/MonitorPanelController.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/NativeAppModel.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/SettingsContentView.swift`

- [x] **Step 1: Write failing test**

Extend `SettingsStoreTests` to assert `monitor_always_on_top` decodes, saves, and defaults to `false`.

- [x] **Step 2: Implement setting**

Add `monitorAlwaysOnTop` to `AppSettings`, validation, and Settings UI toggle.

- [x] **Step 3: Apply window level**

Default the monitor panel to `.normal`; set `.floating` only when `monitorAlwaysOnTop` is true. Re-apply when settings are saved.

- [ ] **Step 4: Verify**

Run:

```sh
cd native/macos/CodexQuotaDock
swift test --filter SettingsStoreTests
```

Expected: all settings tests pass.

### Task 3: Touch Bar Binding

**Files:**
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/AppDelegate.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/SettingsWindowController.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/MonitorPanelController.swift`

- [x] **Step 1: Bind Touch Bar to active windows**

Pass `TouchBarController` into the settings and monitor controllers and assign `window.touchBar = touchBarController.makeTouchBar()` for both windows, while keeping `NSApp.touchBar` as a fallback.

- [ ] **Step 2: Verify manually on a Touch Bar Mac**

Run the app on a Touch Bar-capable Mac, open Settings, and confirm alias / 5h / weekly / Refresh / Switch are visible.

### Task 4: macOS Update Check

**Files:**
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/UpdateChecker.swift`
- Test: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/UpdateCheckerTests.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/NativeAppModel.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/SettingsContentView.swift`

- [x] **Step 1: Write failing parser tests**

Test that a GitHub latest-release JSON payload selects `codex-quota-dock-native-macos-universal.zip` and ignores old/current versions.

- [x] **Step 2: Implement update checker**

Add a minimal GitHub release checker using `URLSession` and parser-only tests. It reports latest version, asset name, asset size, release URL, and download URL.

- [x] **Step 3: Wire Updates tab**

Replace placeholder text with `Check Updates`, status text, and an `Open Releases` button using `NSWorkspace.shared.open`.

- [ ] **Step 4: Verify**

Run:

```sh
cd native/macos/CodexQuotaDock
swift test --filter UpdateCheckerTests
swift test
```

Expected: all tests pass.
