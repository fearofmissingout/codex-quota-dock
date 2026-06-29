import Foundation

public struct MonitorProfileState: Identifiable, Equatable {
    public let id: String
    public let alias: String
    public let accountSuffix: String
    public let fiveHour: QuotaWindow
    public let weekly: QuotaWindow
    public let isActive: Bool
    public let isPinned: Bool

    public init(
        id: String = UUID().uuidString,
        alias: String,
        accountSuffix: String = "",
        fiveHour: QuotaWindow,
        weekly: QuotaWindow,
        isActive: Bool,
        isPinned: Bool
    ) {
        self.id = id
        self.alias = alias
        self.accountSuffix = accountSuffix
        self.fiveHour = fiveHour
        self.weekly = weekly
        self.isActive = isActive
        self.isPinned = isPinned
    }

    public var effectiveRemainingPercent: Int? {
        [fiveHour.remainingPercent, weekly.remainingPercent].compactMap { $0 }.min()
    }

    public func isBelow(threshold: Int) -> Bool {
        guard threshold > 0, let effectiveRemainingPercent else {
            return false
        }
        return effectiveRemainingPercent <= threshold
    }
}
