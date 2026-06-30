import Foundation

public enum ProfileStoreError: Error, LocalizedError {
    case missingAlias
    case missingProfile(String)

    public var errorDescription: String? {
        switch self {
        case .missingAlias:
            "Profile alias is required."
        case .missingProfile(let id):
            "Profile \(id) was not found."
        }
    }
}

public final class ProfileStore {
    public let configDirectory: URL
    private let fileManager: FileManager

    public init(configDirectory: URL, fileManager: FileManager = .default) {
        self.configDirectory = configDirectory
        self.fileManager = fileManager
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

    public func load() throws -> ProfileStoreFile {
        guard fileManager.fileExists(atPath: profilesFile.path) else {
            return ProfileStoreFile()
        }
        let data = try Data(contentsOf: profilesFile)
        return try JSONDecoder.codex.decode(ProfileStoreFile.self, from: data)
    }

    public func save(_ storeFile: ProfileStoreFile) throws {
        try fileManager.createDirectory(at: configDirectory, withIntermediateDirectories: true)
        try fileManager.createDirectory(at: profilesDirectory, withIntermediateDirectories: true)
        let data = try JSONEncoder.codex.encode(storeFile)
        try data.write(to: profilesFile, options: .atomic)
    }

    public func authURL(for profile: Profile) -> URL {
        profilesDirectory
            .appendingPathComponent(profile.id, isDirectory: true)
            .appendingPathComponent("auth.json")
    }

    public func authJSON(for profile: Profile) throws -> Data {
        try Data(contentsOf: authURL(for: profile))
    }

    public func profile(id: String) throws -> Profile {
        guard let profile = try load().profiles.first(where: { $0.id == id }) else {
            throw ProfileStoreError.missingProfile(id)
        }
        return profile
    }

    @discardableResult
    public func importAuth(
        alias rawAlias: String,
        authJSON: Data,
        now: Date = Date(),
        updateExistingAccount: Bool = false
    ) throws -> Profile {
        let alias = rawAlias.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !alias.isEmpty else {
            throw ProfileStoreError.missingAlias
        }

        let metadata = try AuthMetadata.parse(authJSON)
        var storeFile = try load()
        if updateExistingAccount,
           !metadata.accountID.isEmpty,
           let index = storeFile.profiles.firstIndex(where: { $0.accountID == metadata.accountID }) {
            var existing = storeFile.profiles[index]
            existing.alias = uniqueAlias(alias, profiles: storeFile.profiles, exceptID: existing.id)
            existing.accountID = metadata.accountID
            existing.accountSuffix = metadata.accountSuffix
            existing.authMode = metadata.authMode
            existing.lastRefresh = metadata.lastRefresh
            storeFile.profiles[index] = existing
            try writeAuth(authJSON, for: existing)
            try save(storeFile)
            return existing
        }

        let profile = Profile(
            id: randomProfileID(),
            alias: uniqueAlias(alias, profiles: storeFile.profiles),
            accountID: metadata.accountID,
            accountSuffix: metadata.accountSuffix,
            authMode: metadata.authMode,
            pinned: false,
            lastRefresh: metadata.lastRefresh,
            createdAt: now
        )
        storeFile.profiles.append(profile)
        try writeAuth(authJSON, for: profile)
        try save(storeFile)
        return profile
    }

    @discardableResult
    public func updateProfile(id: String, alias rawAlias: String, authJSON: Data) throws -> Profile {
        let alias = rawAlias.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !alias.isEmpty else {
            throw ProfileStoreError.missingAlias
        }
        let metadata = try AuthMetadata.parse(authJSON)
        var storeFile = try load()
        guard let index = storeFile.profiles.firstIndex(where: { $0.id == id }) else {
            throw ProfileStoreError.missingProfile(id)
        }
        var profile = storeFile.profiles[index]
        profile.alias = uniqueAlias(alias, profiles: storeFile.profiles, exceptID: id)
        profile.accountID = metadata.accountID
        profile.accountSuffix = metadata.accountSuffix
        profile.authMode = metadata.authMode
        profile.lastRefresh = metadata.lastRefresh
        storeFile.profiles[index] = profile
        try writeAuth(authJSON, for: profile)
        try save(storeFile)
        return profile
    }

    @discardableResult
    public func setPinned(profileID: String, pinned: Bool) throws -> Profile {
        var storeFile = try load()
        guard let index = storeFile.profiles.firstIndex(where: { $0.id == profileID }) else {
            throw ProfileStoreError.missingProfile(profileID)
        }
        storeFile.profiles[index].pinned = pinned
        try save(storeFile)
        return storeFile.profiles[index]
    }

    public func delete(profileID: String) throws {
        var storeFile = try load()
        guard let index = storeFile.profiles.firstIndex(where: { $0.id == profileID }) else {
            throw ProfileStoreError.missingProfile(profileID)
        }
        let profile = storeFile.profiles.remove(at: index)
        try? fileManager.removeItem(at: profilesDirectory.appendingPathComponent(profile.id, isDirectory: true))
        try save(storeFile)
    }

    @discardableResult
    public func updateAutomation(profileID: String, priority: Int, autoSwitchAllowed: Bool) throws -> Profile {
        var storeFile = try load()
        guard let index = storeFile.profiles.firstIndex(where: { $0.id == profileID }) else {
            throw ProfileStoreError.missingProfile(profileID)
        }
        storeFile.profiles[index].priority = priority
        storeFile.profiles[index].autoSwitchAllowed = autoSwitchAllowed
        try save(storeFile)
        return storeFile.profiles[index]
    }

    private func writeAuth(_ data: Data, for profile: Profile) throws {
        let directory = profilesDirectory.appendingPathComponent(profile.id, isDirectory: true)
        try fileManager.createDirectory(at: directory, withIntermediateDirectories: true)
        try data.write(to: directory.appendingPathComponent("auth.json"), options: .atomic)
    }

    private func uniqueAlias(_ alias: String, profiles: [Profile], exceptID: String? = nil) -> String {
        let lowerExisting = Set(profiles.filter { $0.id != exceptID }.map { $0.alias.lowercased() })
        guard lowerExisting.contains(alias.lowercased()) else {
            return alias
        }
        var index = 2
        while true {
            let candidate = "\(alias)-\(index)"
            if !lowerExisting.contains(candidate.lowercased()) {
                return candidate
            }
            index += 1
        }
    }

    private func randomProfileID() -> String {
        UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased()
    }
}
