import XCTest
@testable import CodexQuotaDockCore

final class QuotaClientTests: XCTestCase {
    func testParsesWhamUsedPercentAsRemaining() throws {
        let json = """
        {
          "rate_limit": {
            "primary_window": {"used_percent": 97, "limit_window_seconds": 18000, "reset_at": 1782734400},
            "secondary_window": {"used_percent": 25, "limit_window_seconds": 604800, "reset_at": 1783296000}
          }
        }
        """.data(using: .utf8)!

        let quota = try QuotaParser.parse(json)
        XCTAssertEqual(quota.fiveHour.remainingPercent, 3)
        XCTAssertEqual(quota.weekly.remainingPercent, 75)
    }

    func testFormatsQuotaResetWithMonthDayAndTime() {
        let date = Date(timeIntervalSince1970: 1_782_909_240)
        XCTAssertEqual(
            QuotaFormatting.resetText(for: date, timeZone: TimeZone(secondsFromGMT: 0)!),
            "07/01 12:34"
        )
    }
}
