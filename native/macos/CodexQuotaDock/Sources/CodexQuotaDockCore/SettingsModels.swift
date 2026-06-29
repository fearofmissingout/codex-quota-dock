import Foundation

public struct AppSettings: Codable, Equatable {
    public var pollIntervalMinutes: Int
    public var showRestartReminder: Bool
    public var autoRestartCodex: Bool
    public var fiveHourAlertThreshold: Int
    public var weeklyAlertThreshold: Int
    public var checkUpdatesOnStartup: Bool
    public var lastUpdateCheck: Date?

    public init(
        pollIntervalMinutes: Int,
        showRestartReminder: Bool,
        autoRestartCodex: Bool,
        fiveHourAlertThreshold: Int,
        weeklyAlertThreshold: Int,
        checkUpdatesOnStartup: Bool,
        lastUpdateCheck: Date?
    ) {
        self.pollIntervalMinutes = pollIntervalMinutes
        self.showRestartReminder = showRestartReminder
        self.autoRestartCodex = autoRestartCodex
        self.fiveHourAlertThreshold = fiveHourAlertThreshold
        self.weeklyAlertThreshold = weeklyAlertThreshold
        self.checkUpdatesOnStartup = checkUpdatesOnStartup
        self.lastUpdateCheck = lastUpdateCheck
    }

    enum CodingKeys: String, CodingKey {
        case pollIntervalMinutes = "poll_interval_minutes"
        case showRestartReminder = "show_restart_reminder"
        case autoRestartCodex = "auto_restart_codex"
        case fiveHourAlertThreshold = "five_hour_alert_threshold"
        case weeklyAlertThreshold = "weekly_alert_threshold"
        case checkUpdatesOnStartup = "check_updates_on_startup"
        case lastUpdateCheck = "last_update_check"
    }

    public static let defaults = AppSettings(
        pollIntervalMinutes: 5,
        showRestartReminder: true,
        autoRestartCodex: false,
        fiveHourAlertThreshold: 10,
        weeklyAlertThreshold: 30,
        checkUpdatesOnStartup: true,
        lastUpdateCheck: nil
    )
}
