import Foundation

public enum QuotaError: Error, LocalizedError {
    case invalidResponse
    case unauthorized(Int)
    case requestFailed(Int)

    public var errorDescription: String? {
        switch self {
        case .invalidResponse:
            "Quota response could not be parsed."
        case .unauthorized(let code):
            "Quota request was unauthorized (HTTP \(code))."
        case .requestFailed(let code):
            "Quota request failed (HTTP \(code))."
        }
    }
}

public struct QuotaWindow: Equatable {
    public let label: String
    public let remainingPercent: Int?
    public let resetsAt: Date?

    public init(label: String, remainingPercent: Int?, resetsAt: Date?) {
        self.label = label
        self.remainingPercent = remainingPercent
        self.resetsAt = resetsAt
    }
}

public enum QuotaFormatting {
    public static func resetText(for date: Date, timeZone: TimeZone = .current) -> String {
        var calendar = Calendar(identifier: .gregorian)
        calendar.timeZone = timeZone
        let components = calendar.dateComponents([.month, .day, .hour, .minute], from: date)
        return String(
            format: "%02d/%02d %02d:%02d",
            components.month ?? 0,
            components.day ?? 0,
            components.hour ?? 0,
            components.minute ?? 0
        )
    }
}

public struct ProfileQuota: Equatable {
    public let fiveHour: QuotaWindow
    public let weekly: QuotaWindow

    public init(fiveHour: QuotaWindow, weekly: QuotaWindow) {
        self.fiveHour = fiveHour
        self.weekly = weekly
    }
}

public enum QuotaParser {
    public static func parse(_ data: Data) throws -> ProfileQuota {
        let object = try JSONSerialization.jsonObject(with: data)
        guard let root = object as? [String: Any] else {
            throw QuotaError.invalidResponse
        }

        if let wham = parseWhamUsage(root) {
            return wham
        }
        throw QuotaError.invalidResponse
    }

    private static func parseWhamUsage(_ root: [String: Any]) -> ProfileQuota? {
        guard let rateLimit = root["rate_limit"] as? [String: Any] else { return nil }
        let primary = parseWindow(rateLimit["primary_window"], label: "5h")
        let secondary = parseWindow(rateLimit["secondary_window"], label: "weekly")
        return ProfileQuota(fiveHour: primary, weekly: secondary)
    }

    private static func parseWindow(_ value: Any?, label: String) -> QuotaWindow {
        guard let object = value as? [String: Any] else {
            return QuotaWindow(label: label, remainingPercent: nil, resetsAt: nil)
        }
        let used = double(object["used_percent"])
        let remaining = used.map { clamp(Int((100 - $0).rounded())) }
        let resetsAt = int64(object["reset_at"]).map { Date(timeIntervalSince1970: TimeInterval($0)) }
        return QuotaWindow(label: label, remainingPercent: remaining, resetsAt: resetsAt)
    }

    private static func clamp(_ value: Int) -> Int {
        min(100, max(0, value))
    }

    private static func int64(_ value: Any?) -> Int64? {
        if let number = value as? Int64 { return number }
        if let number = value as? Int { return Int64(number) }
        if let number = value as? Double { return Int64(number) }
        if let text = value as? String { return Int64(text) }
        return nil
    }

    private static func double(_ value: Any?) -> Double? {
        if let number = value as? Double { return number }
        if let number = value as? Int { return Double(number) }
        if let text = value as? String { return Double(text) }
        return nil
    }
}

public final class QuotaClient {
    private let baseURL: URL
    private let session: URLSession

    public init(
        baseURL: URL = URL(string: "https://chatgpt.com/backend-api/wham/usage")!,
        timeout: TimeInterval = 20
    ) {
        self.baseURL = baseURL
        let config = URLSessionConfiguration.ephemeral
        config.timeoutIntervalForRequest = timeout
        config.timeoutIntervalForResource = timeout
        self.session = URLSession(configuration: config)
    }

    public func fetch(authJSON: Data) async throws -> ProfileQuota {
        let metadata = try AuthMetadata.parse(authJSON)
        var request = URLRequest(url: baseURL)
        request.httpMethod = "GET"
        request.setValue("Bearer \(metadata.accessToken)", forHTTPHeaderField: "Authorization")
        request.setValue("codex-quota-dock-native", forHTTPHeaderField: "User-Agent")
        if !metadata.accountID.isEmpty {
            request.setValue(metadata.accountID, forHTTPHeaderField: "ChatGPT-Account-Id")
        }

        let (data, response) = try await session.data(for: request)
        guard let http = response as? HTTPURLResponse else {
            throw QuotaError.invalidResponse
        }
        switch http.statusCode {
        case 200:
            return try QuotaParser.parse(data)
        case 401, 403:
            throw QuotaError.unauthorized(http.statusCode)
        default:
            throw QuotaError.requestFailed(http.statusCode)
        }
    }
}
