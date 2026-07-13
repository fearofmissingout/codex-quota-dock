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

    func testClassifiesQuotaWindowsByDuration() throws {
        let json = """
        {
          "plan_type": "pro",
          "rate_limit": {
            "primary_window": {"used_percent": 1, "limit_window_seconds": 604800, "reset_at": 1784511322},
            "secondary_window": null
          },
          "additional_rate_limits": [
            {
              "limit_name": "GPT-5.3-Codex-Spark",
              "metered_feature": "codex_bengalfox",
              "rate_limit": {
                "primary_window": {"used_percent": 0, "limit_window_seconds": 604800, "reset_at": 1784514384},
                "secondary_window": null
              }
            }
          ]
        }
        """.data(using: .utf8)!

        let quota = try QuotaParser.parse(json)
        XCTAssertNil(quota.fiveHour.remainingPercent)
        XCTAssertEqual(quota.weekly.remainingPercent, 99)
    }

    func testParsesAppServerRateLimitsPayload() throws {
        let json = """
        {
          "rateLimits": {
            "planType": "pro",
            "limitId": "codex",
            "primary": {"usedPercent": 12, "windowDurationMins": 300, "resetsAt": 1782734400},
            "secondary": {"usedPercent": 33, "windowDurationMins": 10080, "resetsAt": 1783296000}
          },
          "rateLimitsByLimitId": {
            "codex_bengalfox": {
              "planType": "pro",
              "limitId": "codex_bengalfox",
              "limitName": "GPT-5.3-Codex-Spark",
              "primary": {"usedPercent": 5, "windowDurationMins": 10080, "resetsAt": 1784514384}
            }
          }
        }
        """.data(using: .utf8)!

        let quota = try QuotaParser.parse(json)
        XCTAssertEqual(quota.fiveHour.remainingPercent, 88)
        XCTAssertEqual(quota.weekly.remainingPercent, 67)
    }

    func testFormatsQuotaResetWithMonthDayAndTime() {
        let date = Date(timeIntervalSince1970: 1_782_909_240)
        XCTAssertEqual(
            QuotaFormatting.resetText(for: date, timeZone: TimeZone(secondsFromGMT: 0)!),
            "07/01 12:34"
        )
    }
}
