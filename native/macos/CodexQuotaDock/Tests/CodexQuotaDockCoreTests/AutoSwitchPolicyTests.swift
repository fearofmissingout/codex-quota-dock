import XCTest
@testable import CodexQuotaDockCore

final class AutoSwitchPolicyTests: XCTestCase {
    func testNotifiesWhenCurrentQuotaCrossesThresholdInNotifyMode() {
        let decision = AutoSwitchPolicy.decide(
            mode: .notify,
            current: candidate("team", priority: 10, fiveHour: 2, weekly: 80, active: true),
            candidates: [candidate("pro", priority: 1, fiveHour: 90, weekly: 90)],
            context: AutoSwitchContext(codexRunning: true, idleMinutes: 0, lastSwitchAt: nil, now: Date(timeIntervalSince1970: 100)),
            switchAwayThreshold: 5,
            switchToThreshold: 30,
            cooldownMinutes: 15,
            requiredIdleMinutes: 5
        )

        XCTAssertEqual(decision, .notify(targetProfileID: "pro", reason: .currentQuotaLow))
    }

    func testSwitchesWhenCodexIsClosedAndCurrentQuotaIsLow() {
        let decision = AutoSwitchPolicy.decide(
            mode: .whenCodexClosed,
            current: candidate("team", priority: 10, fiveHour: 2, weekly: 80, active: true),
            candidates: [candidate("pro", priority: 1, fiveHour: 90, weekly: 90)],
            context: AutoSwitchContext(codexRunning: false, idleMinutes: 0, lastSwitchAt: nil, now: Date(timeIntervalSince1970: 100)),
            switchAwayThreshold: 5,
            switchToThreshold: 30,
            cooldownMinutes: 15,
            requiredIdleMinutes: 5
        )

        XCTAssertEqual(decision, .switchNow(targetProfileID: "pro", reason: .currentQuotaLow))
    }

    func testWaitsForIdleWhenPreferredProfileRecovers() {
        let decision = AutoSwitchPolicy.decide(
            mode: .whenIdle,
            current: candidate("backup", priority: 1, fiveHour: 80, weekly: 80, active: true),
            candidates: [candidate("team", priority: 10, fiveHour: 35, weekly: 40)],
            context: AutoSwitchContext(codexRunning: true, idleMinutes: 2, lastSwitchAt: nil, now: Date(timeIntervalSince1970: 100)),
            switchAwayThreshold: 5,
            switchToThreshold: 30,
            cooldownMinutes: 15,
            requiredIdleMinutes: 5
        )

        XCTAssertEqual(decision, .pendingUntilIdle(targetProfileID: "team", reason: .preferredProfileRecovered))
    }

    func testCooldownBlocksSwitching() {
        let decision = AutoSwitchPolicy.decide(
            mode: .whenIdle,
            current: candidate("team", priority: 10, fiveHour: 1, weekly: 80, active: true),
            candidates: [candidate("pro", priority: 1, fiveHour: 90, weekly: 90)],
            context: AutoSwitchContext(
                codexRunning: true,
                idleMinutes: 20,
                lastSwitchAt: Date(timeIntervalSince1970: 90),
                now: Date(timeIntervalSince1970: 100)
            ),
            switchAwayThreshold: 5,
            switchToThreshold: 30,
            cooldownMinutes: 15,
            requiredIdleMinutes: 5
        )

        XCTAssertEqual(decision, .none)
    }

    private func candidate(
        _ id: String,
        priority: Int,
        fiveHour: Int,
        weekly: Int,
        active: Bool = false
    ) -> AutoSwitchCandidate {
        AutoSwitchCandidate(
            profileID: id,
            alias: id,
            priority: priority,
            autoSwitchAllowed: true,
            fiveHourRemainingPercent: fiveHour,
            weeklyRemainingPercent: weekly,
            isActive: active
        )
    }
}
