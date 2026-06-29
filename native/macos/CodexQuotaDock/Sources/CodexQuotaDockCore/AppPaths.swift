import Foundation

public struct AppPaths: Equatable {
    public let homeDirectory: URL
    public let environment: [String: String]

    public init(
        homeDirectory: URL = FileManager.default.homeDirectoryForCurrentUser,
        environment: [String: String] = ProcessInfo.processInfo.environment
    ) {
        self.homeDirectory = homeDirectory
        self.environment = environment
    }

    public var configDirectory: URL {
        homeDirectory
            .appendingPathComponent("Library", isDirectory: true)
            .appendingPathComponent("Application Support", isDirectory: true)
            .appendingPathComponent("codex-quota-dock", isDirectory: true)
    }

    public var profilesFile: URL {
        configDirectory.appendingPathComponent("profiles.json")
    }

    public var profilesDirectory: URL {
        configDirectory.appendingPathComponent("profiles", isDirectory: true)
    }

    public var backupsDirectory: URL {
        configDirectory.appendingPathComponent("backups", isDirectory: true)
    }

    public var settingsFile: URL {
        configDirectory.appendingPathComponent("settings.json")
    }

    public var switchHistoryFile: URL {
        configDirectory.appendingPathComponent("auth_switch_history.json")
    }

    public var defaultCodexAuth: URL {
        if let codexHome = environment["CODEX_HOME"], !codexHome.isEmpty {
            return URL(fileURLWithPath: codexHome).appendingPathComponent("auth.json")
        }
        return homeDirectory
            .appendingPathComponent(".codex", isDirectory: true)
            .appendingPathComponent("auth.json")
    }
}
