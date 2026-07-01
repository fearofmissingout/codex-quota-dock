import Foundation

public enum AutoSwitchMode: String, Codable, CaseIterable, Equatable {
    case off
    case notify
    case whenCodexClosed = "when_codex_closed"
    case whenIdle = "when_idle"
}

public struct AppSettings: Codable, Equatable {
    public static let allowedPollIntervalMinutes = [1, 5, 10]

    public var pollIntervalMinutes: Int
    public var autoRestartCodex: Bool
    public var fiveHourAlertThreshold: Int
    public var weeklyAlertThreshold: Int
    public var autoSwitchMode: AutoSwitchMode
    public var autoSwitchIdleMinutes: Int
    public var autoSwitchCooldownMinutes: Int
    public var switchAwayThreshold: Int
    public var switchToThreshold: Int
    public var quotaPriorityMode: Bool
    public var quotaPriorityFiveHourThreshold: Int
    public var quotaPriorityWeeklyThreshold: Int
    public var codexAppPath: String
    public var monitorAlwaysOnTop: Bool

    public init(
        pollIntervalMinutes: Int,
        autoRestartCodex: Bool,
        fiveHourAlertThreshold: Int,
        weeklyAlertThreshold: Int,
        autoSwitchMode: AutoSwitchMode = .off,
        autoSwitchIdleMinutes: Int = 5,
        autoSwitchCooldownMinutes: Int = 15,
        switchAwayThreshold: Int = 5,
        switchToThreshold: Int = 30,
        quotaPriorityMode: Bool = false,
        quotaPriorityFiveHourThreshold: Int = 99,
        quotaPriorityWeeklyThreshold: Int = 0,
        codexAppPath: String = "",
        monitorAlwaysOnTop: Bool = false
    ) {
        self.pollIntervalMinutes = pollIntervalMinutes
        self.autoRestartCodex = autoRestartCodex
        self.fiveHourAlertThreshold = fiveHourAlertThreshold
        self.weeklyAlertThreshold = weeklyAlertThreshold
        self.autoSwitchMode = autoSwitchMode
        self.autoSwitchIdleMinutes = autoSwitchIdleMinutes
        self.autoSwitchCooldownMinutes = autoSwitchCooldownMinutes
        self.switchAwayThreshold = switchAwayThreshold
        self.switchToThreshold = switchToThreshold
        self.quotaPriorityMode = quotaPriorityMode
        self.quotaPriorityFiveHourThreshold = quotaPriorityFiveHourThreshold
        self.quotaPriorityWeeklyThreshold = quotaPriorityWeeklyThreshold
        self.codexAppPath = codexAppPath
        self.monitorAlwaysOnTop = monitorAlwaysOnTop
    }

    enum CodingKeys: String, CodingKey {
        case pollIntervalMinutes = "poll_interval_minutes"
        case autoRestartCodex = "auto_restart_codex"
        case fiveHourAlertThreshold = "five_hour_alert_threshold"
        case weeklyAlertThreshold = "weekly_alert_threshold"
        case autoSwitchMode = "auto_switch_mode"
        case autoSwitchIdleMinutes = "auto_switch_idle_minutes"
        case autoSwitchCooldownMinutes = "auto_switch_cooldown_minutes"
        case switchAwayThreshold = "switch_away_threshold"
        case switchToThreshold = "switch_to_threshold"
        case quotaPriorityMode = "quota_priority_mode"
        case quotaPriorityFiveHourThreshold = "quota_priority_five_hour_threshold"
        case quotaPriorityWeeklyThreshold = "quota_priority_weekly_threshold"
        case codexAppPath = "codex_app_path"
        case monitorAlwaysOnTop = "monitor_always_on_top"
    }

    public init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        pollIntervalMinutes = try values.decodeIfPresent(Int.self, forKey: .pollIntervalMinutes) ?? Self.defaults.pollIntervalMinutes
        autoRestartCodex = try values.decodeIfPresent(Bool.self, forKey: .autoRestartCodex) ?? Self.defaults.autoRestartCodex
        fiveHourAlertThreshold = try values.decodeIfPresent(Int.self, forKey: .fiveHourAlertThreshold) ?? Self.defaults.fiveHourAlertThreshold
        weeklyAlertThreshold = try values.decodeIfPresent(Int.self, forKey: .weeklyAlertThreshold) ?? Self.defaults.weeklyAlertThreshold
        autoSwitchMode = (try? values.decodeIfPresent(AutoSwitchMode.self, forKey: .autoSwitchMode)) ?? Self.defaults.autoSwitchMode
        autoSwitchIdleMinutes = try values.decodeIfPresent(Int.self, forKey: .autoSwitchIdleMinutes) ?? Self.defaults.autoSwitchIdleMinutes
        autoSwitchCooldownMinutes = try values.decodeIfPresent(Int.self, forKey: .autoSwitchCooldownMinutes) ?? Self.defaults.autoSwitchCooldownMinutes
        switchAwayThreshold = try values.decodeIfPresent(Int.self, forKey: .switchAwayThreshold) ?? Self.defaults.switchAwayThreshold
        switchToThreshold = try values.decodeIfPresent(Int.self, forKey: .switchToThreshold) ?? Self.defaults.switchToThreshold
        quotaPriorityMode = try values.decodeIfPresent(Bool.self, forKey: .quotaPriorityMode) ?? Self.defaults.quotaPriorityMode
        quotaPriorityFiveHourThreshold = try values.decodeIfPresent(Int.self, forKey: .quotaPriorityFiveHourThreshold) ?? Self.defaults.quotaPriorityFiveHourThreshold
        quotaPriorityWeeklyThreshold = try values.decodeIfPresent(Int.self, forKey: .quotaPriorityWeeklyThreshold) ?? Self.defaults.quotaPriorityWeeklyThreshold
        codexAppPath = try values.decodeIfPresent(String.self, forKey: .codexAppPath) ?? Self.defaults.codexAppPath
        monitorAlwaysOnTop = try values.decodeIfPresent(Bool.self, forKey: .monitorAlwaysOnTop) ?? Self.defaults.monitorAlwaysOnTop
    }

    public static let defaults = AppSettings(
        pollIntervalMinutes: 5,
        autoRestartCodex: false,
        fiveHourAlertThreshold: 10,
        weeklyAlertThreshold: 30,
        autoSwitchMode: .off,
        autoSwitchIdleMinutes: 5,
        autoSwitchCooldownMinutes: 15,
        switchAwayThreshold: 5,
        switchToThreshold: 30,
        quotaPriorityMode: false,
        quotaPriorityFiveHourThreshold: 99,
        quotaPriorityWeeklyThreshold: 0,
        codexAppPath: "",
        monitorAlwaysOnTop: false
    )

    public func validated() -> AppSettings {
        let away = switchAwayThreshold > 0 ? switchAwayThreshold : Self.defaults.switchAwayThreshold
        let target = switchToThreshold > away ? switchToThreshold : Self.defaults.switchToThreshold
        let idle = autoSwitchIdleMinutes > 0 ? autoSwitchIdleMinutes : Self.defaults.autoSwitchIdleMinutes
        let cooldown = autoSwitchCooldownMinutes > 0 ? autoSwitchCooldownMinutes : Self.defaults.autoSwitchCooldownMinutes
        return AppSettings(
            pollIntervalMinutes: Self.allowedPollIntervalMinutes.contains(pollIntervalMinutes)
                ? pollIntervalMinutes
                : Self.defaults.pollIntervalMinutes,
            autoRestartCodex: autoRestartCodex,
            fiveHourAlertThreshold: max(0, fiveHourAlertThreshold),
            weeklyAlertThreshold: max(0, weeklyAlertThreshold),
            autoSwitchMode: autoSwitchMode,
            autoSwitchIdleMinutes: idle,
            autoSwitchCooldownMinutes: cooldown,
            switchAwayThreshold: away,
            switchToThreshold: target,
            quotaPriorityMode: quotaPriorityMode,
            quotaPriorityFiveHourThreshold: min(100, max(0, quotaPriorityFiveHourThreshold)),
            quotaPriorityWeeklyThreshold: min(100, max(0, quotaPriorityWeeklyThreshold)),
            codexAppPath: codexAppPath.trimmingCharacters(in: .whitespacesAndNewlines),
            monitorAlwaysOnTop: monitorAlwaysOnTop
        )
    }
}
