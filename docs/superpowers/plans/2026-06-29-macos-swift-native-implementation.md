# macOS Swift Native Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a native macOS Swift/AppKit preview edition while preserving the current Go/Fyne app as the stable cross-platform edition.

**Architecture:** Create a Swift Package based macOS app under `native/macos/CodexQuotaDock` with domain, application, and AppKit UI layers. Keep JSON/config compatibility with the existing Go app by sharing file layout and adding contract fixtures. Use GitHub Actions macOS runners as the authoritative build/test environment.

**Tech Stack:** Swift 5.10+, AppKit, XCTest, Swift Package Manager, GitHub Actions macOS runners, existing Go/Fyne app for cross-platform releases.

---

## File Structure

- Create `native/macos/CodexQuotaDock/Package.swift`: Swift package manifest for app executable and tests.
- Create `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp`: App entry point and AppKit UI.
- Create `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore`: domain models, stores, quota client, auth switcher, Codex process service.
- Create `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests`: XCTest tests for JSON contracts and domain behavior.
- Generate native macOS app icons at package time from `assets/icon/codex-quota-dock.png`; do not add a second icon source in the MVP.
- Create `scripts/package-macos-native.sh`: build executable, assemble `.app`, ad-hoc sign, zip.
- Modify `.github/workflows/build.yml`: add native macOS preview build jobs.
- Modify `README.md`: document the native macOS preview and keep Go/Fyne build docs.
- Create `docs/v0.5.0-release-notes.md`: release notes for the preview.
- Create `testdata/contracts`: JSON fixtures shared by Go tests and Swift tests.

## Task 1: Swift Package Skeleton

**Files:**
- Create: `native/macos/CodexQuotaDock/Package.swift`
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/main.swift`
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/AppPaths.swift`
- Create: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/AppPathsTests.swift`

- [ ] **Step 1: Write the first XCTest**

Create `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/AppPathsTests.swift`:

```swift
import XCTest
@testable import CodexQuotaDockCore

final class AppPathsTests: XCTestCase {
    func testDefaultConfigDirectoryUsesApplicationSupport() throws {
        let paths = AppPaths(
            homeDirectory: URL(fileURLWithPath: "/Users/tester"),
            environment: [:]
        )

        XCTAssertEqual(
            paths.configDirectory.path,
            "/Users/tester/Library/Application Support/codex-quota-dock"
        )
        XCTAssertEqual(paths.defaultCodexAuth.path, "/Users/tester/.codex/auth.json")
    }

    func testCodexHomeOverridesDefaultAuthPath() throws {
        let paths = AppPaths(
            homeDirectory: URL(fileURLWithPath: "/Users/tester"),
            environment: ["CODEX_HOME": "/tmp/custom-codex"]
        )

        XCTAssertEqual(paths.defaultCodexAuth.path, "/tmp/custom-codex/auth.json")
    }
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run on macOS:

```sh
cd native/macos/CodexQuotaDock
swift test --filter AppPathsTests
```

Expected: build failure because `Package.swift` and `AppPaths` do not exist.

- [ ] **Step 3: Add the Swift package manifest**

Create `native/macos/CodexQuotaDock/Package.swift`:

```swift
// swift-tools-version: 5.10
import PackageDescription

let package = Package(
    name: "CodexQuotaDock",
    platforms: [.macOS(.v13)],
    products: [
        .executable(name: "CodexQuotaDock", targets: ["CodexQuotaDockApp"]),
        .library(name: "CodexQuotaDockCore", targets: ["CodexQuotaDockCore"])
    ],
    targets: [
        .target(name: "CodexQuotaDockCore"),
        .executableTarget(
            name: "CodexQuotaDockApp",
            dependencies: ["CodexQuotaDockCore"]
        ),
        .testTarget(
            name: "CodexQuotaDockCoreTests",
            dependencies: ["CodexQuotaDockCore"]
        )
    ]
)
```

- [ ] **Step 4: Add `AppPaths`**

Create `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/AppPaths.swift`:

```swift
import Foundation

public struct AppPaths: Equatable {
    public let homeDirectory: URL
    public let environment: [String: String]

    public init(homeDirectory: URL = FileManager.default.homeDirectoryForCurrentUser,
                environment: [String: String] = ProcessInfo.processInfo.environment) {
        self.homeDirectory = homeDirectory
        self.environment = environment
    }

    public var configDirectory: URL {
        homeDirectory
            .appendingPathComponent("Library", isDirectory: true)
            .appendingPathComponent("Application Support", isDirectory: true)
            .appendingPathComponent("codex-quota-dock", isDirectory: true)
    }

    public var profilesFile: URL {
        configDirectory.appendingPathComponent("profiles.json")
    }

    public var profilesDirectory: URL {
        configDirectory.appendingPathComponent("profiles", isDirectory: true)
    }

    public var backupsDirectory: URL {
        configDirectory.appendingPathComponent("backups", isDirectory: true)
    }

    public var settingsFile: URL {
        configDirectory.appendingPathComponent("settings.json")
    }

    public var defaultCodexAuth: URL {
        if let codexHome = environment["CODEX_HOME"], !codexHome.isEmpty {
            return URL(fileURLWithPath: codexHome).appendingPathComponent("auth.json")
        }
        return homeDirectory
            .appendingPathComponent(".codex", isDirectory: true)
            .appendingPathComponent("auth.json")
    }
}
```

- [ ] **Step 5: Add a minimal app entry point**

Create `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/main.swift`:

```swift
import AppKit
import CodexQuotaDockCore

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
        let alert = NSAlert()
        alert.messageText = "Codex Quota Dock Native"
        alert.informativeText = "Native macOS preview shell is installed."
        alert.runModal()
        NSApp.terminate(nil)
    }
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.run()
```

- [ ] **Step 6: Verify and commit**

Run on macOS:

```sh
cd native/macos/CodexQuotaDock
swift test --filter AppPathsTests
swift build
```

Expected: tests pass and executable builds.

Commit:

```sh
git add native/macos/CodexQuotaDock
git commit -m "feat: scaffold native macos swift app"
```

## Task 2: Contract Fixtures For Existing Go Data

**Files:**
- Create: `testdata/contracts/profiles.json`
- Create: `testdata/contracts/native-settings.json`
- Create: `testdata/contracts/profile-auth.json`
- Modify: `internal/profile/store_test.go`
- Create: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/ContractFixtureTests.swift`
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/ProfileModels.swift`
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/SettingsModels.swift`

- [ ] **Step 1: Add shared JSON fixtures**

Create `testdata/contracts/profiles.json`:

```json
{
  "profiles": [
    {
      "id": "profile-team",
      "alias": "team",
      "account_id": "acct_team_123456",
      "account_suffix": "123456",
      "auth_mode": "chatgpt",
      "pinned": true,
      "last_refresh": "2026-06-29T00:00:00Z",
      "created_at": "2026-06-29T00:00:00Z"
    },
    {
      "id": "profile-pro",
      "alias": "pro",
      "account_id": "acct_pro_987654",
      "account_suffix": "987654",
      "auth_mode": "chatgpt",
      "pinned": false,
      "last_refresh": "2026-06-29T00:00:00Z",
      "created_at": "2026-06-29T00:00:00Z"
    }
  ]
}
```

Create `testdata/contracts/native-settings.json`. This is a new Swift-native settings file; the existing Go app does not currently persist standalone settings:

```json
{
  "poll_interval_minutes": 5,
  "show_restart_reminder": true,
  "auto_restart_codex": false,
  "five_hour_alert_threshold": 10,
  "weekly_alert_threshold": 10,
  "check_updates_on_startup": true,
  "last_update_check": "2026-06-29T00:00:00Z"
}
```

Create `testdata/contracts/profile-auth.json`:

```json
{
  "OPENAI_ACCOUNT_ID": "acct_team_123456",
  "tokens": {
    "access_token": "fixture-access-token",
    "refresh_token": "fixture-refresh-token"
  }
}
```

- [ ] **Step 2: Add Swift fixture decoding test**

Create `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/ContractFixtureTests.swift`:

```swift
import Foundation
import XCTest
@testable import CodexQuotaDockCore

final class ContractFixtureTests: XCTestCase {
    func testDecodesExistingProfilesFixture() throws {
        let data = try fixture("profiles.json")
        let store = try JSONDecoder.codex.decode(ProfileStoreFile.self, from: data)

        XCTAssertEqual(store.profiles.count, 2)
        XCTAssertEqual(store.profiles[0].alias, "team")
        XCTAssertEqual(store.profiles[0].accountID, "acct_team_123456")
        XCTAssertTrue(store.profiles[0].pinned)
    }

    func testDecodesExistingSettingsFixture() throws {
        let data = try fixture("native-settings.json")
        let settings = try JSONDecoder.codex.decode(AppSettings.self, from: data)

        XCTAssertEqual(settings.pollIntervalMinutes, 5)
        XCTAssertEqual(settings.fiveHourAlertThreshold, 10)
        XCTAssertEqual(settings.weeklyAlertThreshold, 10)
    }

    private func fixture(_ name: String) throws -> Data {
        let packageRoot = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
        return try Data(contentsOf: packageRoot
            .appendingPathComponent("testdata/contracts")
            .appendingPathComponent(name))
    }
}
```

- [ ] **Step 3: Add Swift JSON models**

Create `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/ProfileModels.swift`:

```swift
import Foundation

public struct ProfileStoreFile: Codable, Equatable {
    public var profiles: [Profile]
}

public struct Profile: Codable, Equatable, Identifiable {
    public var id: String
    public var alias: String
    public var accountID: String
    public var accountSuffix: String
    public var authMode: String
    public var pinned: Bool
    public var lastRefresh: String
    public var createdAt: Date

    enum CodingKeys: String, CodingKey {
        case id
        case alias
        case accountID = "account_id"
        case accountSuffix = "account_suffix"
        case authMode = "auth_mode"
        case pinned
        case lastRefresh = "last_refresh"
        case createdAt = "created_at"
    }
}

public extension JSONDecoder {
    static var codex: JSONDecoder {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return decoder
    }
}

public extension JSONEncoder {
    static var codex: JSONEncoder {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        return encoder
    }
}
```

Create `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/SettingsModels.swift`:

```swift
import Foundation

public struct AppSettings: Codable, Equatable {
    public var pollIntervalMinutes: Int
    public var showRestartReminder: Bool
    public var autoRestartCodex: Bool
    public var fiveHourAlertThreshold: Int
    public var weeklyAlertThreshold: Int
    public var checkUpdatesOnStartup: Bool
    public var lastUpdateCheck: Date?

    enum CodingKeys: String, CodingKey {
        case pollIntervalMinutes = "poll_interval_minutes"
        case showRestartReminder = "show_restart_reminder"
        case autoRestartCodex = "auto_restart_codex"
        case fiveHourAlertThreshold = "five_hour_alert_threshold"
        case weeklyAlertThreshold = "weekly_alert_threshold"
        case checkUpdatesOnStartup = "check_updates_on_startup"
        case lastUpdateCheck = "last_update_check"
    }

    public static let defaults = AppSettings(
        pollIntervalMinutes: 5,
        showRestartReminder: true,
        autoRestartCodex: false,
        fiveHourAlertThreshold: 10,
        weeklyAlertThreshold: 10,
        checkUpdatesOnStartup: true,
        lastUpdateCheck: nil
    )
}
```

- [ ] **Step 4: Add Go fixture compatibility assertions**

Modify the existing Go profile tests so `go test ./internal/profile` reads `testdata/contracts/profiles.json`. Add a test that decodes the fixture and asserts the alias, account suffix, auth mode, pinned flag, and created timestamp.

Expected Go test command:

```sh
go test ./internal/profile
```

- [ ] **Step 5: Verify and commit**

Run:

```sh
go test ./internal/profile
cd native/macos/CodexQuotaDock && swift test --filter ContractFixtureTests
```

Commit:

```sh
git add testdata/contracts internal/profile/store_test.go native/macos/CodexQuotaDock
git commit -m "test: add shared config contract fixtures"
```

## Task 3: Native Profile Store And Auth Switcher

**Files:**
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/ProfileStore.swift`
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/AuthSwitcher.swift`
- Create: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/ProfileStoreTests.swift`
- Create: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/AuthSwitcherTests.swift`

- [ ] **Step 1: Write profile store tests**

Create `ProfileStoreTests.swift` with tests for load, save, import, delete, and duplicate account update:

```swift
import XCTest
@testable import CodexQuotaDockCore

final class ProfileStoreTests: XCTestCase {
    func testSavesAndLoadsProfileMetadataAndAuthJSON() throws {
        let root = try TemporaryDirectory()
        let store = ProfileStore(configDirectory: root.url)

        let auth = #"{"OPENAI_ACCOUNT_ID":"acct_123"}"#.data(using: .utf8)!
        let profile = try store.importAuth(alias: "team", authJSON: auth, now: Date(timeIntervalSince1970: 1))

        let loaded = try store.load()
        XCTAssertEqual(loaded.profiles.map(\.alias), ["team"])
        XCTAssertEqual(try store.authJSON(for: profile), auth)
    }

    func testImportUpdatesExistingAccountWhenRequested() throws {
        let root = try TemporaryDirectory()
        let store = ProfileStore(configDirectory: root.url)
        _ = try store.importAuth(alias: "team", authJSON: #"{"OPENAI_ACCOUNT_ID":"acct_123"}"#.data(using: .utf8)!, now: Date(timeIntervalSince1970: 1))
        _ = try store.importAuth(alias: "team-new", authJSON: #"{"OPENAI_ACCOUNT_ID":"acct_123","fresh":true}"#.data(using: .utf8)!, now: Date(timeIntervalSince1970: 2), updateExistingAccount: true)

        let loaded = try store.load()
        XCTAssertEqual(loaded.profiles.count, 1)
        XCTAssertEqual(loaded.profiles[0].alias, "team-new")
    }
}
```

- [ ] **Step 2: Write auth switcher test**

Create `AuthSwitcherTests.swift`:

```swift
import XCTest
@testable import CodexQuotaDockCore

final class AuthSwitcherTests: XCTestCase {
    func testSwitchCreatesBackupBeforeReplacingActiveAuth() throws {
        let root = try TemporaryDirectory()
        let active = root.url.appendingPathComponent("auth.json")
        let backups = root.url.appendingPathComponent("backups", isDirectory: true)
        try FileManager.default.createDirectory(at: backups, withIntermediateDirectories: true)
        try #"{"OPENAI_ACCOUNT_ID":"old"}"#.write(to: active, atomically: true, encoding: .utf8)

        let switcher = AuthSwitcher(fileManager: .default)
        let result = try switcher.switchAuth(
            activeAuth: active,
            targetAuthJSON: #"{"OPENAI_ACCOUNT_ID":"new"}"#.data(using: .utf8)!,
            backupsDirectory: backups,
            now: Date(timeIntervalSince1970: 1)
        )

        XCTAssertEqual(try String(contentsOf: active), #"{"OPENAI_ACCOUNT_ID":"new"}"#)
        XCTAssertTrue(FileManager.default.fileExists(atPath: result.backupURL.path))
        XCTAssertEqual(try String(contentsOf: result.backupURL), #"{"OPENAI_ACCOUNT_ID":"old"}"#)
    }
}
```

- [ ] **Step 3: Implement `ProfileStore`**

Add a store that:

- Creates `profiles/` directories as needed.
- Extracts account ID from `OPENAI_ACCOUNT_ID`.
- Writes auth JSON under `profiles/<profile-id>/auth.json`.
- Writes `profiles.json` atomically.
- Keeps duplicate alias behavior deterministic by suffixing `-2`, `-3`, and so on.

- [ ] **Step 4: Implement `AuthSwitcher`**

Add a switcher that:

- Creates the backup directory.
- Backs up active auth only when the active file exists.
- Writes target auth atomically to the active auth path.
- Returns backup URL and active auth URL for the UI alert.

- [ ] **Step 5: Verify and commit**

Run on macOS:

```sh
cd native/macos/CodexQuotaDock
swift test --filter ProfileStoreTests
swift test --filter AuthSwitcherTests
```

Commit:

```sh
git add native/macos/CodexQuotaDock
git commit -m "feat: add native profile store and auth switcher"
```

## Task 4: Quota Client And View Model State

**Files:**
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/QuotaClient.swift`
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/MonitorViewModel.swift`
- Create: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/QuotaClientTests.swift`
- Create: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/MonitorViewModelTests.swift`

- [ ] **Step 1: Write quota parsing tests**

Create `QuotaClientTests.swift` with sample available and exhausted responses:

```swift
import XCTest
@testable import CodexQuotaDockCore

final class QuotaClientTests: XCTestCase {
    func testParsesFiveHourAndWeeklyRemaining() throws {
        let json = """
        {
          "usage": {
            "gpt-5": {
              "limits": {
                "five_hour": {"remaining": 42, "resets_at": "2026-06-29T12:00:00Z"},
                "weekly": {"remaining": 88, "resets_at": "2026-07-06T00:00:00Z"}
              }
            }
          }
        }
        """.data(using: .utf8)!

        let quota = try QuotaParser.parse(json)
        XCTAssertEqual(quota.fiveHour.remainingPercent, 42)
        XCTAssertEqual(quota.weekly.remainingPercent, 88)
    }
}
```

- [ ] **Step 2: Write monitor view model tests**

Create `MonitorViewModelTests.swift`:

```swift
import XCTest
@testable import CodexQuotaDockCore

final class MonitorViewModelTests: XCTestCase {
    func testHighlightsLowestRemainingQuota() {
        let state = MonitorProfileState(
            alias: "team",
            fiveHour: QuotaWindow(label: "5h", remainingPercent: 3, resetsAt: nil),
            weekly: QuotaWindow(label: "weekly", remainingPercent: 71, resetsAt: nil),
            isActive: true,
            isPinned: true
        )

        XCTAssertEqual(state.effectiveRemainingPercent, 3)
        XCTAssertTrue(state.isBelow(threshold: 10))
    }
}
```

- [ ] **Step 3: Implement quota types**

Create types:

```swift
public struct QuotaWindow: Equatable {
    public let label: String
    public let remainingPercent: Int?
    public let resetsAt: Date?
}

public struct ProfileQuota: Equatable {
    public let fiveHour: QuotaWindow
    public let weekly: QuotaWindow
}
```

- [ ] **Step 4: Implement parser and client**

Add:

- `QuotaParser.parse(_ data: Data) throws -> ProfileQuota`
- `QuotaClient.fetch(authJSON: Data) async throws -> ProfileQuota`

The client should set the same auth headers the Go client uses and a timeout no longer than 20 seconds.

- [ ] **Step 5: Verify and commit**

Run:

```sh
cd native/macos/CodexQuotaDock
swift test --filter QuotaClientTests
swift test --filter MonitorViewModelTests
```

Commit:

```sh
git add native/macos/CodexQuotaDock
git commit -m "feat: add native quota state model"
```

## Task 5: AppKit Menu Bar And Floating Monitor

**Files:**
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/AppDelegate.swift`
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/MonitorPanelController.swift`
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/MonitorView.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/main.swift`

- [ ] **Step 1: Replace the alert app with a menu bar app**

Move startup into `AppDelegate.swift`:

```swift
import AppKit
import CodexQuotaDockCore

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var statusItem: NSStatusItem!
    private var monitor: MonitorPanelController!

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
        monitor = MonitorPanelController()
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        statusItem.button?.image = NSImage(systemSymbolName: "gauge.with.dots.needle.33percent", accessibilityDescription: "Codex Quota Dock")
        statusItem.menu = makeMenu()
    }

    private func makeMenu() -> NSMenu {
        let menu = NSMenu()
        menu.addItem(NSMenuItem(title: "Show Monitor", action: #selector(showMonitor), keyEquivalent: ""))
        menu.addItem(NSMenuItem(title: "Refresh", action: #selector(refresh), keyEquivalent: "r"))
        menu.addItem(.separator())
        menu.addItem(NSMenuItem(title: "Quit", action: #selector(quit), keyEquivalent: "q"))
        return menu
    }

    @objc private func showMonitor() { monitor.showNearStatusItem(statusItem) }
    @objc private func refresh() { monitor.refresh() }
    @objc private func quit() { NSApp.terminate(nil) }
}
```

- [ ] **Step 2: Add a borderless draggable monitor panel**

Create `MonitorPanelController.swift`:

```swift
import AppKit

final class MonitorPanelController: NSWindowController {
    convenience init() {
        let panel = NSPanel(
            contentRect: NSRect(x: 0, y: 0, width: 360, height: 180),
            styleMask: [.borderless, .nonactivatingPanel],
            backing: .buffered,
            defer: false
        )
        panel.isMovableByWindowBackground = true
        panel.level = .floating
        panel.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
        panel.backgroundColor = .clear
        panel.isOpaque = false
        panel.contentView = MonitorView()
        self.init(window: panel)
    }

    func showNearStatusItem(_ item: NSStatusItem) {
        window?.center()
        window?.makeKeyAndOrderFront(nil)
    }

    func refresh() {
        (window?.contentView as? MonitorView)?.setStatus("Refreshing...")
    }
}
```

- [ ] **Step 3: Add the visual effect monitor view**

Create `MonitorView.swift` with `NSVisualEffectView`, labels for active profile, 5h quota, weekly quota, and three buttons: Refresh, Switch, Settings.

- [ ] **Step 4: Verify and commit**

Run on macOS:

```sh
cd native/macos/CodexQuotaDock
swift build
swift run CodexQuotaDock
```

Manual expected result: menu bar icon appears; Show Monitor opens a translucent draggable panel.

Commit:

```sh
git add native/macos/CodexQuotaDock
git commit -m "feat: add native macos menu bar monitor"
```

## Task 6: Settings Window And Profile Editing

**Files:**
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/SettingsWindowController.swift`
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/ProfileListView.swift`
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/AuthEditorView.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/AppDelegate.swift`

- [ ] **Step 1: Add settings menu item**

Add Settings to the status menu:

```swift
menu.addItem(NSMenuItem(title: "Settings...", action: #selector(openSettings), keyEquivalent: ","))
```

Add a `SettingsWindowController` property and `openSettings()` action.

- [ ] **Step 2: Implement profile list UI**

Use `NSTableView` or `NSOutlineView` with columns:

- Alias
- Account suffix
- Active
- Pinned

Rows should update from `ProfileStore.load()`.

- [ ] **Step 3: Implement auth JSON editor**

Use `NSTextView` inside `NSScrollView` for auth JSON. Add Save and Reload buttons. Saving writes to the selected profile auth path through `ProfileStore`.

- [ ] **Step 4: Implement profile actions**

Wire:

- Import Current
- Import File
- New Profile
- Delete
- Pin
- Switch

Switch must call `AuthSwitcher` and then show a native `NSAlert` with backup path and restart status.

- [ ] **Step 5: Verify and commit**

Run:

```sh
cd native/macos/CodexQuotaDock
swift build
```

Manual expected result: settings opens as a normal titled window; profile actions operate on a temporary app config directory during testing and on real config during manual smoke testing.

Commit:

```sh
git add native/macos/CodexQuotaDock
git commit -m "feat: add native macos profile settings"
```

## Task 7: Codex Restart And Health Checks

**Files:**
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/CodexProcessService.swift`
- Create: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/HealthCheck.swift`
- Create: `native/macos/CodexQuotaDock/Tests/CodexQuotaDockCoreTests/HealthCheckTests.swift`
- Modify: `native/macos/CodexQuotaDock/Sources/CodexQuotaDockApp/SettingsWindowController.swift`

- [ ] **Step 1: Write health tests**

Create tests for missing auth, readable auth, profile count, and restart availability model.

- [ ] **Step 2: Implement Codex process service**

Use `NSWorkspace.shared.runningApplications` to find likely Codex apps by bundle identifier or localized name. Terminate existing instances and reopen with `NSWorkspace.shared.openApplication`.

- [ ] **Step 3: Add health section to settings**

Show:

- Active auth path
- Active auth parse status
- Saved profile count
- App version
- Codex restart availability

Do not display auth tokens.

- [ ] **Step 4: Verify and commit**

Run:

```sh
cd native/macos/CodexQuotaDock
swift test --filter HealthCheckTests
swift build
```

Commit:

```sh
git add native/macos/CodexQuotaDock
git commit -m "feat: add native macos codex restart health"
```

## Task 8: macOS Native Packaging And CI

**Files:**
- Create: `scripts/package-macos-native.sh`
- Modify: `.github/workflows/build.yml`
- Modify: `assets/icon/codex-quota-dock.png`

- [ ] **Step 1: Add package script**

Create `scripts/package-macos-native.sh`:

```sh
#!/usr/bin/env sh
set -eu

ARCH="${1:-arm64}"
VERSION="${VERSION:-0.5.0-dev}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP_ROOT="$ROOT/dist/native-macos-$ARCH"
APP="$APP_ROOT/Codex Quota Dock.app"
BIN="$APP/Contents/MacOS/CodexQuotaDock"

rm -rf "$APP_ROOT"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

cd "$ROOT/native/macos/CodexQuotaDock"
swift build -c release --arch "$ARCH"
cp ".build/$ARCH-apple-macosx/release/CodexQuotaDock" "$BIN"

ICONSET="$APP_ROOT/AppIcon.iconset"
mkdir -p "$ICONSET"
sips -z 16 16 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_16x16.png" >/dev/null
sips -z 32 32 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_16x16@2x.png" >/dev/null
sips -z 32 32 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_32x32.png" >/dev/null
sips -z 64 64 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_32x32@2x.png" >/dev/null
sips -z 128 128 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_128x128.png" >/dev/null
sips -z 256 256 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_128x128@2x.png" >/dev/null
sips -z 256 256 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_256x256.png" >/dev/null
sips -z 512 512 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_256x256@2x.png" >/dev/null
sips -z 512 512 "$ROOT/assets/icon/codex-quota-dock.png" --out "$ICONSET/icon_512x512.png" >/dev/null
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/AppIcon.icns"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleExecutable</key><string>CodexQuotaDock</string>
  <key>CFBundleIdentifier</key><string>io.github.fearofmissingout.codex-quota-dock.native</string>
  <key>CFBundleName</key><string>Codex Quota Dock</string>
  <key>CFBundleDisplayName</key><string>Codex Quota Dock</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>LSUIElement</key><true/>
  <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST

codesign --force --deep --sign - "$APP"
ditto -c -k --sequesterRsrc --keepParent "$APP" "$ROOT/dist/codex-quota-dock-native-macos-$ARCH.zip"
```

- [ ] **Step 2: Add workflow jobs**

Modify `.github/workflows/build.yml` to add a native macOS matrix:

```yaml
  build-native-macos:
    name: Build native macOS ${{ matrix.arch }}
    runs-on: macos-14
    strategy:
      fail-fast: false
      matrix:
        include:
          - arch: arm64
          - arch: x86_64
    steps:
      - uses: actions/checkout@v4
      - name: Test native macOS app
        run: |
          cd native/macos/CodexQuotaDock
          swift test
      - name: Package native macOS app
        env:
          VERSION: "0.5.0"
        run: ./scripts/package-macos-native.sh "${{ matrix.arch }}"
      - uses: actions/upload-artifact@v4
        with:
          name: codex-quota-dock-native-macos-${{ matrix.arch }}
          path: dist/codex-quota-dock-native-macos-${{ matrix.arch }}.zip
          if-no-files-found: error
```

- [ ] **Step 3: Verify and commit**

Run on macOS:

```sh
./scripts/package-macos-native.sh arm64
```

Run through GitHub Actions on the feature branch and require both native macOS artifacts to pass before release.

Commit:

```sh
git add scripts/package-macos-native.sh .github/workflows/build.yml native/macos/CodexQuotaDock
git commit -m "build: package native macos preview app"
```

## Task 9: Documentation, Release Notes, And Smoke Checklist

**Files:**
- Modify: `README.md`
- Create: `docs/v0.5.0-release-notes.md`
- Create: `docs/v0.5.0-native-macos-smoke-checklist.md`

- [ ] **Step 1: Update README**

Document:

- Native macOS preview is recommended for Mac users.
- Go/Fyne macOS build remains available as fallback during preview.
- Native preview keeps auth files as local plaintext.
- Gatekeeper flow remains Privacy & Security allow-open because builds are ad-hoc signed and not notarized.

- [ ] **Step 2: Add release notes**

Create release notes that list:

- Native macOS preview app.
- Compatible profile/auth storage.
- Menu bar monitor.
- Native settings/profile editor.
- Known preview limitations: no native auto-install update and no local usage charts.

- [ ] **Step 3: Add smoke checklist**

Create checklist with:

- Download correct arch zip.
- Open `.app` after Gatekeeper approval.
- Import Current.
- Add New Profile.
- Refresh quota.
- Switch profile.
- Confirm backup file exists.
- Confirm Codex restart behavior.
- Quit from menu bar.

- [ ] **Step 4: Verify and commit**

Run:

```sh
git diff --check
GITHUB_TOKEN_PATTERN='gh''p_'
OPENAI_KEY_PATTERN='sk-''[A-Za-z0-9_-]{20,}'
rg -n "${GITHUB_TOKEN_PATTERN}|${OPENAI_KEY_PATTERN}" README.md docs native/macos testdata
```

Do not allow real account aliases, GitHub tokens, or OpenAI API keys in committed files.

Commit:

```sh
git add README.md docs native/macos testdata
git commit -m "docs: document native macos preview"
```

## Task 10: Preview Release Validation

**Files:**
- Modify only if validation finds a defect.

- [ ] **Step 1: Run Go validation**

Run:

```sh
CGO_ENABLED=0 go test ./...
```

Expected: all Go tests pass.

- [ ] **Step 2: Run native macOS validation**

Run on macOS:

```sh
cd native/macos/CodexQuotaDock
swift test
swift build -c release
```

Expected: all Swift tests pass and release build succeeds.

- [ ] **Step 3: Run packaging validation**

Run on macOS:

```sh
VERSION=0.5.0 ./scripts/package-macos-native.sh arm64
VERSION=0.5.0 ./scripts/package-macos-native.sh x86_64
```

Expected: both zip files are created under `dist/`.

- [ ] **Step 4: Run GitHub Actions validation**

Push the feature branch and watch the workflow:

```sh
git push -u origin codex/v0.5.0-macos-native
gh run list --repo fearofmissingout/codex-quota-dock --branch codex/v0.5.0-macos-native --limit 5
```

Expected: Go/Fyne jobs and native macOS jobs complete successfully.

- [ ] **Step 5: Manual macOS smoke test**

Run the checklist in `docs/v0.5.0-native-macos-smoke-checklist.md` on a real Mac before tagging v0.5.0.

- [ ] **Step 6: Commit validation fixes**

If validation required fixes:

```sh
git add .
git commit -m "fix: stabilize native macos preview"
```

If no fixes were required, do not create an empty commit.
