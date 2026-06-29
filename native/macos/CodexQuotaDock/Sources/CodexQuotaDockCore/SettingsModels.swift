import Foundation

public struct AppSettings: Codable, Equatable {
    public static let allowedPollIntervalMinutes = [1, 5, 10]

    public var pollIntervalMinutes: Int
    public var autoRestartCodex: Bool
    public var fiveHourAlertThreshold: Int
    public var weeklyAlertThreshold: Int

    public init(
        pollIntervalMinutes: Int,
        autoRestartCodex: Bool,
        fiveHourAlertThreshold: Int,
        weeklyAlertThreshold: Int
    ) {
        self.pollIntervalMinutes = pollIntervalMinutes
        self.autoRestartCodex = autoRestartCodex
        self.fiveHourAlertThreshold = fiveHourAlertThreshold
        self.weeklyAlertThreshold = weeklyAlertThreshold
    }

    enum CodingKeys: String, CodingKey {
        case pollIntervalMinutes = "poll_interval_minutes"
        case autoRestartCodex = "auto_restart_codex"
        case fiveHourAlertThreshold = "five_hour_alert_threshold"
        case weeklyAlertThreshold = "weekly_alert_threshold"
    }

    public static let defaults = AppSettings(
        pollIntervalMinutes: 5,
        autoRestartCodex: false,
        fiveHourAlertThreshold: 10,
        weeklyAlertThreshold: 30
    )

    public func validated() -> AppSettings {
        AppSettings(
            pollIntervalMinutes: Self.allowedPollIntervalMinutes.contains(pollIntervalMinutes)
                ? pollIntervalMinutes
                : Self.defaults.pollIntervalMinutes,
            autoRestartCodex: autoRestartCodex,
            fiveHourAlertThreshold: max(0, fiveHourAlertThreshold),
            weeklyAlertThreshold: max(0, weeklyAlertThreshold)
        )
    }
}
