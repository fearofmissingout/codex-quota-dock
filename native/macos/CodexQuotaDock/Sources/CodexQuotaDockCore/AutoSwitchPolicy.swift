import Foundation

public struct AutoSwitchCandidate: Equatable {
    public let profileID: String
    public let alias: String
    public let priority: Int
    public let autoSwitchAllowed: Bool
    public let fiveHourRemainingPercent: Int?
    public let weeklyRemainingPercent: Int?
    public let isActive: Bool

    public init(
        profileID: String,
        alias: String,
        priority: Int,
        autoSwitchAllowed: Bool,
        fiveHourRemainingPercent: Int?,
        weeklyRemainingPercent: Int?,
        isActive: Bool
    ) {
        self.profileID = profileID
        self.alias = alias
        self.priority = priority
        self.autoSwitchAllowed = autoSwitchAllowed
        self.fiveHourRemainingPercent = fiveHourRemainingPercent
        self.weeklyRemainingPercent = weeklyRemainingPercent
        self.isActive = isActive
    }
}

public struct AutoSwitchContext: Equatable {
    public let codexRunning: Bool
    public let idleMinutes: Int
    public let lastSwitchAt: Date?
    public let now: Date

    public init(codexRunning: Bool, idleMinutes: Int, lastSwitchAt: Date?, now: Date) {
        self.codexRunning = codexRunning
        self.idleMinutes = idleMinutes
        self.lastSwitchAt = lastSwitchAt
        self.now = now
    }
}

public enum AutoSwitchReason: Equatable {
    case currentQuotaLow
    case preferredProfileRecovered
    case quotaPriorityRecovered
}

public enum AutoSwitchDecision: Equatable {
    case none
    case notify(targetProfileID: String, reason: AutoSwitchReason)
    case pendingUntilIdle(targetProfileID: String, reason: AutoSwitchReason)
    case switchNow(targetProfileID: String, reason: AutoSwitchReason)
}

public enum AutoSwitchPolicy {
    public static func decide(
        mode: AutoSwitchMode,
        current: AutoSwitchCandidate?,
        candidates: [AutoSwitchCandidate],
        context: AutoSwitchContext,
        switchAwayThreshold: Int,
        switchToThreshold: Int,
        cooldownMinutes: Int,
        requiredIdleMinutes: Int,
        quotaPriorityMode: Bool = false,
        quotaPriorityFiveHourThreshold: Int = 99,
        quotaPriorityWeeklyThreshold: Int = 0
    ) -> AutoSwitchDecision {
        guard mode != .off, let current else { return .none }
        guard !isInCooldown(context: context, cooldownMinutes: cooldownMinutes) else { return .none }

        let healthyCandidates = candidates
            .filter { $0.profileID != current.profileID }
            .filter { isHealthy($0, threshold: switchToThreshold) }
            .sorted(by: rank)

        let target: AutoSwitchCandidate?
        let reason: AutoSwitchReason
        if isLow(current, threshold: switchAwayThreshold) {
            target = healthyCandidates.first
            reason = .currentQuotaLow
        } else if quotaPriorityMode {
            target = candidates
                .filter { $0.profileID != current.profileID }
                .filter { isQuotaPriorityRecovered($0, fiveHourThreshold: quotaPriorityFiveHourThreshold, weeklyThreshold: quotaPriorityWeeklyThreshold) }
                .sorted(by: rank)
                .first { isHigherPriority($0, than: current) }
            reason = .quotaPriorityRecovered
        } else {
            target = healthyCandidates.first { isHigherPriority($0, than: current) }
            reason = .preferredProfileRecovered
        }

        guard let target else { return .none }
        return gatedDecision(mode: mode, targetProfileID: target.profileID, reason: reason, context: context, requiredIdleMinutes: requiredIdleMinutes)
    }

    private static func isInCooldown(context: AutoSwitchContext, cooldownMinutes: Int) -> Bool {
        guard let lastSwitchAt = context.lastSwitchAt else { return false }
        return context.now.timeIntervalSince(lastSwitchAt) < TimeInterval(max(1, cooldownMinutes) * 60)
    }

    private static func isLow(_ candidate: AutoSwitchCandidate, threshold: Int) -> Bool {
        guard threshold > 0 else { return false }
        return [candidate.fiveHourRemainingPercent, candidate.weeklyRemainingPercent]
            .compactMap { $0 }
            .contains { $0 <= threshold }
    }

    private static func isHealthy(_ candidate: AutoSwitchCandidate, threshold: Int) -> Bool {
        guard candidate.autoSwitchAllowed else { return false }
        guard let fiveHour = candidate.fiveHourRemainingPercent,
              let weekly = candidate.weeklyRemainingPercent
        else {
            return false
        }
        return fiveHour >= threshold && weekly >= threshold
    }

    private static func isQuotaPriorityRecovered(_ candidate: AutoSwitchCandidate, fiveHourThreshold: Int, weeklyThreshold: Int) -> Bool {
        guard candidate.autoSwitchAllowed else { return false }
        guard let fiveHour = candidate.fiveHourRemainingPercent,
              let weekly = candidate.weeklyRemainingPercent
        else {
            return false
        }
        return fiveHour >= min(100, max(0, fiveHourThreshold)) && weekly >= min(100, max(0, weeklyThreshold))
    }

    private static func isHigherPriority(_ candidate: AutoSwitchCandidate, than current: AutoSwitchCandidate) -> Bool {
        candidate.priority < current.priority
    }

    private static func rank(_ lhs: AutoSwitchCandidate, _ rhs: AutoSwitchCandidate) -> Bool {
        if lhs.priority != rhs.priority {
            return lhs.priority < rhs.priority
        }
        return lhs.alias.localizedCaseInsensitiveCompare(rhs.alias) == .orderedAscending
    }

    private static func gatedDecision(
        mode: AutoSwitchMode,
        targetProfileID: String,
        reason: AutoSwitchReason,
        context: AutoSwitchContext,
        requiredIdleMinutes: Int
    ) -> AutoSwitchDecision {
        switch mode {
        case .off:
            return .none
        case .notify:
            return .notify(targetProfileID: targetProfileID, reason: reason)
        case .whenCodexClosed:
            return context.codexRunning
                ? .pendingUntilIdle(targetProfileID: targetProfileID, reason: reason)
                : .switchNow(targetProfileID: targetProfileID, reason: reason)
        case .whenIdle:
            if !context.codexRunning || context.idleMinutes >= max(1, requiredIdleMinutes) {
                return .switchNow(targetProfileID: targetProfileID, reason: reason)
            }
            return .pendingUntilIdle(targetProfileID: targetProfileID, reason: reason)
        }
    }
}
