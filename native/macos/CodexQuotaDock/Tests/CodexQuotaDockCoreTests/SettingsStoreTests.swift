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
          "poll_interval_minutes": 15,
          "weekly_alert_threshold": 18
        }
        """.data(using: .utf8)!.write(to: url)

        let settings = SettingsStore(url: url).load()

        XCTAssertEqual(settings.pollIntervalMinutes, 5)
        XCTAssertEqual(settings.fiveHourAlertThreshold, 0)
        XCTAssertEqual(settings.weeklyAlertThreshold, 18)
        XCTAssertTrue(settings.autoRestartCodex)
    }
}
