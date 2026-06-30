import CoreFoundation
import Foundation

public struct BackupImportSummary: Equatable {
    public let created: Int
    public let updated: Int
    public let skipped: Int

    public init(created: Int = 0, updated: Int = 0, skipped: Int = 0) {
        self.created = created
        self.updated = updated
        self.skipped = skipped
    }
}

public enum BackupStoreError: Error, LocalizedError {
    case invalidVersion
    case invalidJSON
    case missingBackup

    public var errorDescription: String? {
        switch self {
        case .invalidVersion:
            "Backup version is not supported."
        case .invalidJSON:
            "Backup JSON is not valid."
        case .missingBackup:
            "No auth backup was found."
        }
    }
}

public enum BackupStore {
    public static func exportBackup(store: ProfileStore, settings: AppSettings, now: Date = Date()) throws -> Data {
        let profiles = try store.load().profiles.map { profile in
            let data = try store.authJSON(for: profile)
            let object = try JSONSerialization.jsonObject(with: data)
            return BackupProfile(
                alias: profile.alias,
                accountID: profile.accountID,
                accountSuffix: profile.accountSuffix,
                authMode: profile.authMode,
                pinned: profile.pinned,
                priority: profile.priority,
                autoSwitchAllowed: profile.autoSwitchAllowed,
                authJSON: try JSONValue(any: object)
            )
        }
        let backup = BackupFile(
            version: 1,
            exportedAt: now,
            profiles: profiles,
            settings: try settingsJSON(settings.validated())
        )
        return try JSONEncoder.codex.encode(backup)
    }

    @discardableResult
    public static func importBackup(into store: ProfileStore, data: Data) throws -> BackupImportSummary {
        let backup = try JSONDecoder.codex.decode(BackupFile.self, from: data)
        guard Int(backup.version) == 1 else {
            throw BackupStoreError.invalidVersion
        }

        var created = 0
        var updated = 0
        var skipped = 0

        for backupProfile in backup.profiles {
            let authData = try JSONEncoder.codex.encode(backupProfile.authJSON)
            guard (try? AuthMetadata.parse(authData)) != nil else {
                skipped += 1
                continue
            }

            let existing = try store.load().profiles.first { profile in
                !profile.accountID.isEmpty && profile.accountID == backupProfile.accountID
            }

            if let existing {
                let profile = try store.updateProfile(
                    id: existing.id,
                    alias: backupProfile.alias,
                    authJSON: authData
                )
                try applyMetadata(backupProfile, to: profile, in: store)
                updated += 1
            } else {
                let profile = try store.importAuth(
                    alias: backupProfile.alias,
                    authJSON: authData,
                    updateExistingAccount: false
                )
                try applyMetadata(backupProfile, to: profile, in: store)
                created += 1
            }
        }

        return BackupImportSummary(created: created, updated: updated, skipped: skipped)
    }

    public static func latestBackup(in backupsDirectory: URL, fileManager: FileManager = .default) -> URL? {
        guard let urls = try? fileManager.contentsOfDirectory(
            at: backupsDirectory,
            includingPropertiesForKeys: [.isRegularFileKey],
            options: [.skipsHiddenFiles]
        ) else {
            return nil
        }

        return urls
            .filter { $0.pathExtension.lowercased() == "json" }
            .sorted { $0.lastPathComponent < $1.lastPathComponent }
            .last
    }

    public static func restoreLatestBackup(
        from backupsDirectory: URL,
        to activeAuth: URL,
        fileManager: FileManager = .default
    ) throws {
        guard let latest = latestBackup(in: backupsDirectory, fileManager: fileManager) else {
            throw BackupStoreError.missingBackup
        }
        try fileManager.createDirectory(
            at: activeAuth.deletingLastPathComponent(),
            withIntermediateDirectories: true
        )
        if fileManager.fileExists(atPath: activeAuth.path) {
            try fileManager.removeItem(at: activeAuth)
        }
        try fileManager.copyItem(at: latest, to: activeAuth)
    }

    private static func applyMetadata(_ backupProfile: BackupProfile, to profile: Profile, in store: ProfileStore) throws {
        try store.setPinned(profileID: profile.id, pinned: backupProfile.pinned ?? profile.pinned)
        try store.updateAutomation(
            profileID: profile.id,
            priority: backupProfile.priority ?? profile.priority,
            autoSwitchAllowed: backupProfile.autoSwitchAllowed ?? profile.autoSwitchAllowed
        )
    }

    private static func settingsJSON(_ settings: AppSettings) throws -> JSONValue {
        let data = try JSONEncoder.codex.encode(settings)
        let object = try JSONSerialization.jsonObject(with: data)
        return try JSONValue(any: object)
    }
}

private struct BackupFile: Codable {
    let version: Double
    let exportedAt: Date
    let profiles: [BackupProfile]
    let settings: JSONValue?

    enum CodingKeys: String, CodingKey {
        case version
        case exportedAt = "exported_at"
        case profiles
        case settings
    }
}

private struct BackupProfile: Codable {
    let alias: String
    let accountID: String
    let accountSuffix: String
    let authMode: String
    let pinned: Bool?
    let priority: Int?
    let autoSwitchAllowed: Bool?
    let authJSON: JSONValue

    enum CodingKeys: String, CodingKey {
        case alias
        case accountID = "account_id"
        case accountSuffix = "account_suffix"
        case authMode = "auth_mode"
        case pinned
        case priority
        case autoSwitchAllowed = "auto_switch_allowed"
        case authJSON = "auth_json"
    }
}

private enum JSONValue: Codable, Equatable {
    case object([String: JSONValue])
    case array([JSONValue])
    case string(String)
    case number(Double)
    case bool(Bool)
    case null

    init(any value: Any) throws {
        switch value {
        case let dictionary as [String: Any]:
            self = .object(try dictionary.mapValues { try JSONValue(any: $0) })
        case let array as [Any]:
            self = .array(try array.map { try JSONValue(any: $0) })
        case let string as String:
            self = .string(string)
        case let number as NSNumber:
            if CFGetTypeID(number) == CFBooleanGetTypeID() {
                self = .bool(number.boolValue)
            } else {
                self = .number(number.doubleValue)
            }
        case _ as NSNull:
            self = .null
        default:
            throw BackupStoreError.invalidJSON
        }
    }

    init(from decoder: Decoder) throws {
        if let object = try? decoder.container(keyedBy: DynamicCodingKey.self) {
            var values: [String: JSONValue] = [:]
            for key in object.allKeys {
                values[key.stringValue] = try object.decode(JSONValue.self, forKey: key)
            }
            self = .object(values)
            return
        }

        if var array = try? decoder.unkeyedContainer() {
            var values: [JSONValue] = []
            while !array.isAtEnd {
                values.append(try array.decode(JSONValue.self))
            }
            self = .array(values)
            return
        }

        let single = try decoder.singleValueContainer()
        if single.decodeNil() {
            self = .null
        } else if let bool = try? single.decode(Bool.self) {
            self = .bool(bool)
        } else if let number = try? single.decode(Double.self) {
            self = .number(number)
        } else if let string = try? single.decode(String.self) {
            self = .string(string)
        } else {
            throw BackupStoreError.invalidJSON
        }
    }

    func encode(to encoder: Encoder) throws {
        switch self {
        case let .object(values):
            var container = encoder.container(keyedBy: DynamicCodingKey.self)
            for (key, value) in values {
                try container.encode(value, forKey: DynamicCodingKey(stringValue: key))
            }
        case let .array(values):
            var container = encoder.unkeyedContainer()
            for value in values {
                try container.encode(value)
            }
        case let .string(value):
            var container = encoder.singleValueContainer()
            try container.encode(value)
        case let .number(value):
            var container = encoder.singleValueContainer()
            try container.encode(value)
        case let .bool(value):
            var container = encoder.singleValueContainer()
            try container.encode(value)
        case .null:
            var container = encoder.singleValueContainer()
            try container.encodeNil()
        }
    }
}

private struct DynamicCodingKey: CodingKey {
    let stringValue: String
    let intValue: Int?

    init(stringValue: String) {
        self.stringValue = stringValue
        intValue = nil
    }

    init(intValue: Int) {
        stringValue = "\(intValue)"
        self.intValue = intValue
    }
}
