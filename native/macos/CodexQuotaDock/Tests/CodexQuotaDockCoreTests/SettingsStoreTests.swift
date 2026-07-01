import XCTest
@testable import CodexQuotaDockCore

final class SettingsStoreTests: XCTestCase {
    func testLoadFallsBackToAllowedPollInterval() throws {
        let root = try TemporaryDirectory()
        let url = root.url.appendingPathComponent("settings.json")
        try """
        {
          "auto_restart_codex": true,
          "five_hour_alert_threshold": -2,
          "monitor_always_on_top": true,
          "poll_interval_minutes": 15,
          "weekly_alert_threshold": 18
        }
        """.data(using: .utf8)!.write(to: url)

        let settings = SettingsStore(url: url).load()

        XCTAssertEqual(settings.pollIntervalMinutes, 5)
        XCTAssertEqual(settings.fiveHourAlertThreshold, 0)
        XCTAssertEqual(settings.weeklyAlertThreshold, 18)
        XCTAssertTrue(settings.autoRestartCodex)
        XCTAssertTrue(settings.monitorAlwaysOnTop)
    }

    func testLoadFallsBackToSafeAutoSwitchDefaults() throws {
        let root = try TemporaryDirectory()
        let url = root.url.appendingPathComponent("settings.json")
        try """
        {
          "auto_switch_mode": "force",
          "auto_switch_idle_minutes": 0,
          "auto_switch_cooldown_minutes": -5,
          "quota_priority_mode": true,
          "quota_priority_five_hour_threshold": 101,
          "quota_priority_weekly_threshold": -1,
          "switch_away_threshold": -1,
          "switch_to_threshold": 3
        }
        """.data(using: .utf8)!.write(to: url)

        let settings = SettingsStore(url: url).load()

        XCTAssertEqual(settings.autoSwitchMode, .off)
        XCTAssertEqual(settings.autoSwitchIdleMinutes, 5)
        XCTAssertEqual(settings.autoSwitchCooldownMinutes, 15)
        XCTAssertEqual(settings.switchAwayThreshold, 5)
        XCTAssertEqual(settings.switchToThreshold, 30)
        XCTAssertTrue(settings.quotaPriorityMode)
        XCTAssertEqual(settings.quotaPriorityFiveHourThreshold, 100)
        XCTAssertEqual(settings.quotaPriorityWeeklyThreshold, 0)
        XCTAssertEqual(settings.codexAppPath, "")
        XCTAssertFalse(settings.monitorAlwaysOnTop)
    }

    func testSavesCodexAppPath() throws {
        let root = try TemporaryDirectory()
        let url = root.url.appendingPathComponent("settings.json")
        let store = SettingsStore(url: url)
        var settings = AppSettings.defaults
        settings.codexAppPath = "/Applications/Codex.app"
        settings.monitorAlwaysOnTop = true
        settings.quotaPriorityMode = true
        settings.quotaPriorityFiveHourThreshold = 99
        settings.quotaPriorityWeeklyThreshold = 0

        try store.save(settings)

        XCTAssertEqual(store.load().codexAppPath, "/Applications/Codex.app")
        XCTAssertTrue(store.load().monitorAlwaysOnTop)
        XCTAssertTrue(store.load().quotaPriorityMode)
        XCTAssertEqual(store.load().quotaPriorityFiveHourThreshold, 99)
        XCTAssertEqual(store.load().quotaPriorityWeeklyThreshold, 0)
    }
}
