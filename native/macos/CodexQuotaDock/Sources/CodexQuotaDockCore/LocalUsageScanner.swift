import Foundation

public struct UsageTotals: Equatable, Sendable {
    public var input: Int64
    public var cachedInput: Int64
    public var output: Int64
    public var reasoningOutput: Int64
    public var total: Int64

    public init(input: Int64 = 0, cachedInput: Int64 = 0, output: Int64 = 0, reasoningOutput: Int64 = 0, total: Int64 = 0) {
        self.input = input
        self.cachedInput = cachedInput
        self.output = output
        self.reasoningOutput = reasoningOutput
        self.total = total
    }

    public mutating func add(_ other: UsageTotals) {
        input += other.input
        cachedInput += other.cachedInput
        output += other.output
        reasoningOutput += other.reasoningOutput
        total += other.total
    }
}

public struct LocalUsageDay: Equatable, Identifiable, Sendable {
    public var id: String { day }
    public let day: String
    public let usage: UsageTotals

    public init(day: String, usage: UsageTotals) {
        self.day = day
        self.usage = usage
    }
}

public struct LocalUsageSummary: Equatable, Sendable {
    public var today: UsageTotals
    public var last7Days: UsageTotals
    public var last30Days: UsageTotals
    public var total: UsageTotals
    public var byDay: [LocalUsageDay]
    public var sessionCount: Int
    public var parseErrors: Int

    public init(
        today: UsageTotals = UsageTotals(),
        last7Days: UsageTotals = UsageTotals(),
        last30Days: UsageTotals = UsageTotals(),
        total: UsageTotals = UsageTotals(),
        byDay: [LocalUsageDay] = [],
        sessionCount: Int = 0,
        parseErrors: Int = 0
    ) {
        self.today = today
        self.last7Days = last7Days
        self.last30Days = last30Days
        self.total = total
        self.byDay = byDay
        self.sessionCount = sessionCount
        self.parseErrors = parseErrors
    }
}

public enum LocalUsageScanner {
    public static func scan(codexRoot: URL, now: Date = Date(), fileManager: FileManager = .default) -> LocalUsageSummary {
        var summary = LocalUsageSummary()
        var byDay: [String: UsageTotals] = [:]
        for rootName in ["sessions", "archived_sessions"] {
            let root = codexRoot.appendingPathComponent(rootName, isDirectory: true)
            guard let enumerator = fileManager.enumerator(at: root, includingPropertiesForKeys: [.isRegularFileKey]) else {
                continue
            }
            for case let url as URL in enumerator {
                guard url.pathExtension == "jsonl",
                      ((try? url.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) ?? false)
                else {
                    continue
                }
                summary.sessionCount += 1
                scanFile(url, now: now, summary: &summary, byDay: &byDay)
            }
        }
        summary.byDay = byDay
            .map { LocalUsageDay(day: $0.key, usage: $0.value) }
            .sorted { $0.day < $1.day }
        return summary
    }

    private static func scanFile(_ url: URL, now: Date, summary: inout LocalUsageSummary, byDay: inout [String: UsageTotals]) {
        guard let text = try? String(contentsOf: url, encoding: .utf8) else {
            summary.parseErrors += 1
            return
        }
        for line in text.split(separator: "\n", omittingEmptySubsequences: true) {
            do {
                guard let event = try JSONSerialization.jsonObject(with: Data(line.utf8)) as? [String: Any],
                      let timestamp = event["timestamp"] as? String,
                      let at = parseDate(timestamp),
                      let payload = event["payload"] as? [String: Any],
                      payload["type"] as? String == "token_count",
                      let info = payload["info"] as? [String: Any],
                      let usage = info["last_token_usage"] as? [String: Any]
                else {
                    continue
                }
                let item = totals(from: usage)
                summary.total.add(item)
                if Calendar.current.isDate(at, inSameDayAs: now) {
                    summary.today.add(item)
                }
                if at >= now.addingTimeInterval(-7 * 24 * 60 * 60) {
                    summary.last7Days.add(item)
                }
                if at >= now.addingTimeInterval(-30 * 24 * 60 * 60) {
                    summary.last30Days.add(item)
                }
                var dayTotal = byDay[dayKey(at)] ?? UsageTotals()
                dayTotal.add(item)
                byDay[dayKey(at)] = dayTotal
            } catch {
                summary.parseErrors += 1
            }
        }
    }

    private static func parseDate(_ text: String) -> Date? {
        CodexJSONCoding.iso8601WithFractionalSeconds.date(from: text) ?? CodexJSONCoding.iso8601.date(from: text)
    }

    private static func totals(from usage: [String: Any]) -> UsageTotals {
        let input = int64(usage["input_tokens"])
        let output = int64(usage["output_tokens"])
        return UsageTotals(
            input: input,
            cachedInput: int64(usage["cached_input_tokens"]),
            output: output,
            reasoningOutput: int64(usage["reasoning_output_tokens"]),
            total: int64(usage["total_tokens"], fallback: input + output)
        )
    }

    private static func int64(_ value: Any?, fallback: Int64 = 0) -> Int64 {
        if let value = value as? Int64 { return value }
        if let value = value as? Int { return Int64(value) }
        if let value = value as? Double { return Int64(value) }
        if let value = value as? String, let parsed = Int64(value) { return parsed }
        return fallback
    }

    private static func dayKey(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.calendar = Calendar.current
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.dateFormat = "yyyy-MM-dd"
        return formatter.string(from: date)
    }
}
