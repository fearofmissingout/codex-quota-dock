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
