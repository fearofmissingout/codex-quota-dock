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

        if let quota = parseQuotaPayload(root) {
            return quota
        }
        throw QuotaError.invalidResponse
    }

    private static func parseQuotaPayload(_ root: [String: Any]) -> ProfileQuota? {
        var fiveHour = QuotaWindow(label: "5h", remainingPercent: nil, resetsAt: nil)
        var weekly = QuotaWindow(label: "weekly", remainingPercent: nil, resetsAt: nil)

        if let rateLimit = root["rate_limit"] as? [String: Any] {
            mergeSnakeRateLimit(rateLimit, fiveHour: &fiveHour, weekly: &weekly)
        }
        if let rateLimits = root["rateLimits"] as? [String: Any] {
            mergeCamelRateLimit(rateLimits, fiveHour: &fiveHour, weekly: &weekly)
        }
        if let byLimit = root["rateLimitsByLimitId"] as? [String: Any] {
            for value in byLimit.values {
                if let rateLimit = value as? [String: Any] {
                    mergeCamelRateLimit(rateLimit, fiveHour: &fiveHour, weekly: &weekly)
                }
            }
        }
        if let additional = root["additional_rate_limits"] as? [[String: Any]] {
            for item in additional {
                if let rateLimit = item["rate_limit"] as? [String: Any] {
                    mergeSnakeRateLimit(rateLimit, fiveHour: &fiveHour, weekly: &weekly)
                }
            }
        }

        guard fiveHour.remainingPercent != nil || weekly.remainingPercent != nil else {
            return nil
        }
        return ProfileQuota(fiveHour: fiveHour, weekly: weekly)
    }

    private static func mergeSnakeRateLimit(
        _ rateLimit: [String: Any],
        fiveHour: inout QuotaWindow,
        weekly: inout QuotaWindow
    ) {
        mergeWindow(rateLimit["primary_window"], fallback: "5h", fiveHour: &fiveHour, weekly: &weekly)
        mergeWindow(rateLimit["secondary_window"], fallback: "weekly", fiveHour: &fiveHour, weekly: &weekly)
    }

    private static func mergeCamelRateLimit(
        _ rateLimit: [String: Any],
        fiveHour: inout QuotaWindow,
        weekly: inout QuotaWindow
    ) {
        mergeWindow(rateLimit["primary"], fallback: "5h", fiveHour: &fiveHour, weekly: &weekly)
        mergeWindow(rateLimit["secondary"], fallback: "weekly", fiveHour: &fiveHour, weekly: &weekly)
    }

    private static func mergeWindow(
        _ value: Any?,
        fallback: String,
        fiveHour: inout QuotaWindow,
        weekly: inout QuotaWindow
    ) {
        guard let label = classifyWindow(value, fallback: fallback) else { return }
        if label == "5h", fiveHour.remainingPercent == nil {
            fiveHour = parseWindow(value, label: "5h")
        } else if label == "weekly", weekly.remainingPercent == nil {
            weekly = parseWindow(value, label: "weekly")
        }
    }

    private static func classifyWindow(_ value: Any?, fallback: String) -> String? {
        guard let object = value as? [String: Any] else { return nil }
        let seconds = int64(object["limit_window_seconds"]) ?? 0
        let minutes = int64(object["windowDurationMins"]) ?? 0
        if (17_000...19_000).contains(seconds) || (290...310).contains(minutes) {
            return "5h"
        }
        if (600_000...610_000).contains(seconds) || (10_000...10_160).contains(minutes) {
            return "weekly"
        }
        return fallback
    }

    private static func parseWindow(_ value: Any?, label: String) -> QuotaWindow {
        guard let object = value as? [String: Any] else {
            return QuotaWindow(label: label, remainingPercent: nil, resetsAt: nil)
        }
        let used = double(object["used_percent"]) ?? double(object["usedPercent"])
        let remaining = used.map { clamp(Int((100 - $0).rounded())) }
        let resetSeconds = int64(object["reset_at"]) ?? int64(object["resetsAt"])
        let resetsAt = resetSeconds.map { Date(timeIntervalSince1970: TimeInterval($0)) }
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
        baseURL: URL = URL(string: "https://chatgpt.com/backend-api/wham/usage?supports_rewardless_invites=true")!,
        timeout: TimeInterval = 20
    ) {
        self.baseURL = baseURL
        self.session = ProxyConfiguration.session(timeout: timeout, host: baseURL.host)
    }

    public func fetch(authJSON: Data) async throws -> ProfileQuota {
        let metadata = try AuthMetadata.parse(authJSON)
        var request = URLRequest(url: baseURL)
        request.httpMethod = "GET"
        request.setValue("Bearer \(metadata.accessToken)", forHTTPHeaderField: "Authorization")
        request.setValue("codex-quota-dock-native", forHTTPHeaderField: "User-Agent")
        request.setValue("application/json", forHTTPHeaderField: "Accept")
        request.setValue("zh-CN", forHTTPHeaderField: "OAI-Language")
        request.setValue("codex-desktop", forHTTPHeaderField: "originator")
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
