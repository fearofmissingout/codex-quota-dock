# Native Auto Switch And Restart Paths Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add reliable Windows auto-switching and cross-platform configurable Codex restart/launch behavior.

**Architecture:** Keep restart selection and auto-switch decisions in testable core code, then wire minimal native UI controls on Windows and macOS. Windows must not kill `codex-quota-dock-native.exe` while restarting Codex, and both platforms must support automatic detection plus a manually configured Codex path or launch identifier.

**Tech Stack:** C++17/Win32/CMake tests for Windows native; Swift/AppKit/Swift Package tests for macOS native.

---

### Task 1: Windows Core Restart Target And Auto-Switch Policy

**Files:**
- Modify: `native/windows/CodexQuotaDock/src/core.h`
- Modify: `native/windows/CodexQuotaDock/src/core.cpp`
- Modify: `native/windows/CodexQuotaDock/tests/core_tests.cpp`

- [ ] **Step 1: Write failing Windows core tests**

Add tests that prove:

```cpp
expect(!cqd::isCodexProcessName(L"codex-quota-dock-native.exe"), "quota dock must not be killed");
expect(cqd::isCodexProcessName(L"Codex.exe"), "Codex.exe should be killed");
expect(cqd::isCodexProcessName(L"codex.exe"), "codex.exe should be killed");

cqd::AppSettings settings;
settings.codexLaunchPath = "C:\\custom\\Codex.exe";
expect(cqd::codexLaunchTarget(settings).value.find(L"C:\\custom\\Codex.exe") != std::wstring::npos, "manual path wins");

cqd::AutoSwitchCandidate current{"current", "current", 0, true, 2, 90, true};
cqd::AutoSwitchCandidate backup{"backup", "backup", 10, true, 80, 80, false};
cqd::AutoSwitchDecision decision = cqd::decideAutoSwitch(
    cqd::AutoSwitchMode::WhenCodexClosed,
    current,
    {current, backup},
    cqd::AutoSwitchContext{false, 0, 0, 1000},
    5,
    20,
    15,
    5
);
expect(decision.action == cqd::AutoSwitchAction::SwitchNow, "closed Codex can switch now");
expect(decision.targetProfileId == "backup", "switches to healthy higher priority profile");
```

- [ ] **Step 2: Run failing tests**

Run:

```powershell
.\scripts\build-windows-native.ps1 -Configuration Release -Arch x64
```

Expected: compile failure for missing functions/types.

- [ ] **Step 3: Implement minimal core behavior**

Add:

- `AppSettings.codexLaunchPath`
- `AppSettings.autoSwitchMode`
- `AppSettings.switchAwayThreshold`
- `AppSettings.switchToThreshold`
- `AppSettings.autoSwitchIdleMinutes`
- `AppSettings.autoSwitchCooldownMinutes`
- `Profile.priority`
- `Profile.autoSwitchAllowed`
- `isCodexProcessName`
- `codexLaunchTarget`
- `decideAutoSwitch`

- [ ] **Step 4: Run tests until green**

Run:

```powershell
.\scripts\build-windows-native.ps1 -Configuration Release -Arch x64
```

Expected: tests pass and Windows zip builds.

### Task 2: Windows UI Wiring

**Files:**
- Modify: `native/windows/CodexQuotaDock/src/win_app.h`
- Modify: `native/windows/CodexQuotaDock/src/win_app.cpp`

- [ ] **Step 1: Add Settings controls**

Add controls for:

- auto switch mode
- switch away threshold
- switch to threshold
- idle minutes
- cooldown minutes
- Codex launch path text box
- auto detect Codex path button

- [ ] **Step 2: Wire timer auto-switch**

After quota refresh, call `evaluateAutoSwitch()`. It should:

- identify active profile from `~/.codex/auth.json`
- choose the best healthy candidate
- respect cooldown
- switch immediately only in safe modes
- call `restartCodex(settings_)` only if auto restart is enabled

### Task 3: macOS Configurable Restart Target

**Files:**
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/SettingsModels.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/CodexProcessService.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/NativeAppModel.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/SettingsContentView.swift`
- Modify: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/SettingsStoreTests.swift`

- [ ] **Step 1: Add tests**

Test that settings persist `codex_app_path` and that default remains empty.

- [ ] **Step 2: Implement settings and restart**

Use configured `.app` path first. If empty, try bundle identifier, `/Applications/Codex.app`, and indexed running app URL.

- [ ] **Step 3: Add minimal UI**

Add text field and `Auto Detect` button in Settings tab.

### Task 4: Verification And Docs

**Files:**
- Modify: `docs/native-dev-handoff.zh-CN.md`

- [ ] **Step 1: Verify Windows build**

Run Windows native build/test command.

- [ ] **Step 2: Verify macOS build via CI or note local limitation**

On Windows, Swift/AppKit tests cannot be run locally. Push/check GitHub Actions when ready.

- [ ] **Step 3: Update handoff doc**

Record the new Windows/macOS restart settings and auto-switch behavior.
