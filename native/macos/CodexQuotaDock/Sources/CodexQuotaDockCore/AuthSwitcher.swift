import Foundation

public struct AuthSwitchResult: Equatable {
    public let activeAuthURL: URL
    public let backupURL: URL?
}

public final class AuthSwitcher {
    private let fileManager: FileManager

    public init(fileManager: FileManager = .default) {
        self.fileManager = fileManager
    }

    public func switchAuth(
        activeAuth: URL,
        targetAuthJSON: Data,
        backupsDirectory: URL,
        now: Date = Date()
    ) throws -> AuthSwitchResult {
        try AuthMetadata.parse(targetAuthJSON)
        try fileManager.createDirectory(
            at: activeAuth.deletingLastPathComponent(),
            withIntermediateDirectories: true
        )
        try fileManager.createDirectory(at: backupsDirectory, withIntermediateDirectories: true)

        var backupURL: URL?
        if fileManager.fileExists(atPath: activeAuth.path) {
            let backup = backupsDirectory.appendingPathComponent("auth-\(Self.timestamp(now)).json")
            if fileManager.fileExists(atPath: backup.path) {
                try fileManager.removeItem(at: backup)
            }
            try fileManager.copyItem(at: activeAuth, to: backup)
            backupURL = backup
        }

        try targetAuthJSON.write(to: activeAuth, options: .atomic)
        return AuthSwitchResult(activeAuthURL: activeAuth, backupURL: backupURL)
    }

    private static func timestamp(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.calendar = Calendar(identifier: .gregorian)
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone(secondsFromGMT: 0)
        formatter.dateFormat = "yyyyMMdd-HHmmss"
        return formatter.string(from: date)
    }
}
