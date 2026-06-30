# macOS Native Parity And Auto Switch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring the macOS native Swift app closer to the Windows native experience, add Touch Bar quota controls, and introduce a conservative automatic auth switching policy.

**Architecture:** Keep business logic in `CodexQuotaDockCore` and UI in `CodexQuotaDockApp`. Add testable auto-switch policy logic first, then wire it into `NativeAppModel`, SwiftUI settings, monitor UI, and AppKit Touch Bar support.

**Tech Stack:** Swift 5.10, SwiftUI, AppKit, XCTest, existing GitHub Actions macOS native build.

---

### Task 1: Core Auto Switch Settings And Profile Priority

**Files:**
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/SettingsModels.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/ProfileModels.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/ProfileStore.swift`
- Test: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/SettingsStoreTests.swift`
- Test: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/ProfileStoreTests.swift`

- [ ] Add `AutoSwitchMode` with `off`, `notify`, `whenCodexClosed`, and `whenIdle`.
- [ ] Extend `AppSettings` with auto-switch thresholds, idle minutes, cooldown minutes, and mode.
- [ ] Extend `Profile` with `priority` and `autoSwitchAllowed`, preserving backwards-compatible decode defaults.
- [ ] Add profile store mutation methods for priority and auto-switch allowed flag.
- [ ] Add XCTest coverage for defaults and persistence.

### Task 2: Core Auto Switch Decision Engine

**Files:**
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/AutoSwitchPolicy.swift`
- Create: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/AutoSwitchPolicyTests.swift`

- [ ] Add pure decision types for current profile, candidate profiles, quota health, idle state, and cooldown.
- [ ] Return `none`, `notify`, `switchNow`, or `pendingUntilIdle`.
- [ ] Test switch-away when current profile crosses either threshold.
- [ ] Test preferred profile switch-back only after both quota windows recover above switch-to threshold.
- [ ] Test cooldown blocks repeated switches.
- [ ] Test `whenCodexClosed` and `whenIdle` safety gates.

### Task 3: macOS Usage Summary

**Files:**
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/LocalUsageScanner.swift`
- Create: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/LocalUsageScannerTests.swift`

- [ ] Scan local Codex history files under the Codex root using tolerant JSON line parsing.
- [ ] Return today, 7 days, 30 days, all-time totals, and seven daily buckets.
- [ ] Add tests with temporary fixture files containing token usage payloads.

### Task 4: Native App Model Wiring

**Files:**
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/NativeAppModel.swift`

- [ ] Track quota results by profile ID.
- [ ] Track `localUsageSummary`, `usageLoading`, and selected settings tab.
- [ ] Refresh local usage asynchronously.
- [ ] Evaluate auto-switch decisions after quota refresh.
- [ ] Execute safe automatic switch only for approved modes and show status for notify/pending states.

### Task 5: macOS UI Parity

**Files:**
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/MonitorContentView.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/SettingsContentView.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/MonitorPanelController.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/SettingsWindowController.swift`

- [ ] Update floating monitor to show all visible monitor profiles with two-line quota rows and progress bars.
- [ ] Replace Settings right side with segmented tabs: Auth, Quota, Usage, Settings, Health, Updates.
- [ ] Add priority and auto-switch controls without making the common flow more complex.
- [ ] Add Usage cards, daily bars, overall mix bar, and loading state.

### Task 6: Touch Bar Support

**Files:**
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/TouchBarController.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/AppDelegate.swift`

- [ ] Provide `NSTouchBar` items for active profile alias, 5h, weekly, Refresh, and Switch.
- [ ] Bind Touch Bar labels to `NativeAppModel` updates.
- [ ] Keep Touch Bar optional; app remains unchanged on Macs without Touch Bar.

### Task 7: Verification And Packaging

**Files:**
- Modify only if build scripts need adjustment.

- [ ] Run Swift tests in GitHub Actions for macOS.
- [ ] Package native macOS artifacts for arm64 and x86_64.
- [ ] Download artifacts if needed and report paths.
