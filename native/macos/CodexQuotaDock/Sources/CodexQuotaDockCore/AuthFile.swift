import Foundation

public enum AuthFileError: Error, LocalizedError {
    case invalidJSON
    case missingAccessToken

    public var errorDescription: String? {
        switch self {
        case .invalidJSON:
            "Auth JSON is not a valid object."
        case .missingAccessToken:
            "Auth JSON is missing tokens.access_token."
        }
    }
}

public struct AuthMetadata: Equatable {
    public let authMode: String
    public let accountID: String
    public let accountSuffix: String
    public let accessToken: String
    public let lastRefresh: String

    public static func parse(_ data: Data, requireAccessToken: Bool = true) throws -> AuthMetadata {
        let object = try JSONSerialization.jsonObject(with: data)
        guard let root = object as? [String: Any] else {
            throw AuthFileError.invalidJSON
        }
        let tokens = root["tokens"] as? [String: Any] ?? [:]
        let accessToken = string(tokens["access_token"]) ?? string(root["access_token"]) ?? ""
        if requireAccessToken && accessToken.isEmpty {
            throw AuthFileError.missingAccessToken
        }
        let accountID = string(tokens["account_id"])
            ?? string(root["OPENAI_ACCOUNT_ID"])
            ?? string(root["account_id"])
            ?? ""
        let authMode = string(root["auth_mode"]) ?? "chatgpt"
        let lastRefresh = string(root["last_refresh"]) ?? ""
        return AuthMetadata(
            authMode: authMode,
            accountID: accountID,
            accountSuffix: suffix(accountID, length: 6),
            accessToken: accessToken,
            lastRefresh: lastRefresh
        )
    }

    private static func string(_ value: Any?) -> String? {
        guard let text = value as? String else { return nil }
        return text
    }

    private static func suffix(_ value: String, length: Int) -> String {
        guard length > 0, value.count > length else { return value }
        return String(value.suffix(length))
    }
}
