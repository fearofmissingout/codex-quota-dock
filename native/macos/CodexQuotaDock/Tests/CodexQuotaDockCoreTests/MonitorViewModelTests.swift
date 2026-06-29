import XCTest
@testable import CodexQuotaDockCore

final class MonitorViewModelTests: XCTestCase {
    func testHighlightsWhenFiveHourQuotaCrossesItsThreshold() {
        let state = MonitorProfileState(
            alias: "team",
            fiveHour: QuotaWindow(label: "5h", remainingPercent: 3, resetsAt: nil),
            weekly: QuotaWindow(label: "weekly", remainingPercent: 71, resetsAt: nil),
            isActive: true,
            isPinned: true
        )

        XCTAssertEqual(state.effectiveRemainingPercent, 3)
        XCTAssertTrue(state.isBelow(fiveHourThreshold: 10, weeklyThreshold: 30))
    }

    func testHighlightsWhenWeeklyQuotaCrossesItsOwnThreshold() {
        let state = MonitorProfileState(
            alias: "team",
            fiveHour: QuotaWindow(label: "5h", remainingPercent: 42, resetsAt: nil),
            weekly: QuotaWindow(label: "weekly", remainingPercent: 24, resetsAt: nil),
            isActive: true,
            isPinned: true
        )

        XCTAssertEqual(state.effectiveRemainingPercent, 24)
        XCTAssertTrue(state.isBelow(fiveHourThreshold: 10, weeklyThreshold: 30))
        XCTAssertFalse(state.isBelow(fiveHourThreshold: 10, weeklyThreshold: 20))
    }
}
