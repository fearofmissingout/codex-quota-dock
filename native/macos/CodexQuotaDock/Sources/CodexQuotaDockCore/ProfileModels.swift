import Foundation

public struct ProfileStoreFile: Codable, Equatable {
    public var profiles: [Profile]

    public init(profiles: [Profile] = []) {
        self.profiles = profiles
    }
}

public struct Profile: Codable, Equatable, Identifiable {
    public var id: String
    public var alias: String
    public var accountID: String
    public var accountSuffix: String
    public var authMode: String
    public var pinned: Bool
    public var lastRefresh: String
    public var createdAt: Date

    public init(
        id: String,
        alias: String,
        accountID: String,
        accountSuffix: String,
        authMode: String,
        pinned: Bool = false,
        lastRefresh: String = "",
        createdAt: Date
    ) {
        self.id = id
        self.alias = alias
        self.accountID = accountID
        self.accountSuffix = accountSuffix
        self.authMode = authMode
        self.pinned = pinned
        self.lastRefresh = lastRefresh
        self.createdAt = createdAt
    }

    enum CodingKeys: String, CodingKey {
        case id
        case alias
        case accountID = "account_id"
        case accountSuffix = "account_suffix"
        case authMode = "auth_mode"
        case pinned
        case lastRefresh = "last_refresh"
        case createdAt = "created_at"
    }

    public init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        id = try values.decode(String.self, forKey: .id)
        alias = try values.decode(String.self, forKey: .alias)
        accountID = try values.decodeIfPresent(String.self, forKey: .accountID) ?? ""
        accountSuffix = try values.decodeIfPresent(String.self, forKey: .accountSuffix) ?? ""
        authMode = try values.decodeIfPresent(String.self, forKey: .authMode) ?? "chatgpt"
        pinned = try values.decodeIfPresent(Bool.self, forKey: .pinned) ?? false
        lastRefresh = try values.decodeIfPresent(String.self, forKey: .lastRefresh) ?? ""
        createdAt = try values.decode(Date.self, forKey: .createdAt)
    }
}
