import XCTest
@testable import CodexQuotaDockCore

final class QuotaClientTests: XCTestCase {
    func testParsesFiveHourAndWeeklyRemainingFromSimpleShape() throws {
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
}
