import Foundation
import SQLite3

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

    public var uncachedInput: Int64 {
        max(0, input - cachedInput)
    }

    public var effectiveTotal: Int64 {
        max(0, total - cachedInput)
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
    public var sqlite: UsageTotals
    public var byDay: [LocalUsageDay]
    public var sessionCount: Int
    public var sqliteThreadCount: Int
    public var sqliteDatabaseCount: Int
    public var parseErrors: Int

    public init(
        today: UsageTotals = UsageTotals(),
        last7Days: UsageTotals = UsageTotals(),
        last30Days: UsageTotals = UsageTotals(),
        total: UsageTotals = UsageTotals(),
        sqlite: UsageTotals = UsageTotals(),
        byDay: [LocalUsageDay] = [],
        sessionCount: Int = 0,
        sqliteThreadCount: Int = 0,
        sqliteDatabaseCount: Int = 0,
        parseErrors: Int = 0
    ) {
        self.today = today
        self.last7Days = last7Days
        self.last30Days = last30Days
        self.total = total
        self.sqlite = sqlite
        self.byDay = byDay
        self.sessionCount = sessionCount
        self.sqliteThreadCount = sqliteThreadCount
        self.sqliteDatabaseCount = sqliteDatabaseCount
        self.parseErrors = parseErrors
    }
}

public enum LocalUsageScanner {
    public static func scan(codexRoot: URL, now: Date = Date(), fileManager: FileManager = .default) -> LocalUsageSummary {
        var summary = LocalUsageSummary()
        var byDay: [String: UsageTotals] = [:]
        scanSQLiteDatabases(codexRoot: codexRoot, now: now, summary: &summary, byDay: &byDay, fileManager: fileManager)
        if summary.sqliteThreadCount > 0 {
            summary.byDay = byDay
                .map { LocalUsageDay(day: $0.key, usage: $0.value) }
                .sorted { $0.day < $1.day }
            return summary
        }

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
                add(item, at: at, now: now, summary: &summary, byDay: &byDay)
            } catch {
                summary.parseErrors += 1
            }
        }
    }

    private static func scanSQLiteDatabases(
        codexRoot: URL,
        now: Date,
        summary: inout LocalUsageSummary,
        byDay: inout [String: UsageTotals],
        fileManager: FileManager
    ) {
        for url in sqliteDatabaseCandidates(codexRoot: codexRoot, fileManager: fileManager) {
            scanSQLiteDatabase(url, now: now, summary: &summary, byDay: &byDay)
        }
    }

    private static func sqliteDatabaseCandidates(codexRoot: URL, fileManager: FileManager) -> [URL] {
        var candidates: [URL] = []
        for directory in [
            codexRoot,
            codexRoot.appendingPathComponent("sqlite", isDirectory: true),
        ] {
            guard let urls = try? fileManager.contentsOfDirectory(
                at: directory,
                includingPropertiesForKeys: [.isRegularFileKey],
                options: [.skipsHiddenFiles]
            ) else {
                continue
            }
            candidates.append(contentsOf: urls.filter { isSQLiteDatabaseURL($0) })
        }
        return Array(Set(candidates)).sorted { $0.path < $1.path }
    }

    private static func isSQLiteDatabaseURL(_ url: URL) -> Bool {
        let name = url.lastPathComponent.lowercased()
        let ext = url.pathExtension.lowercased()
        guard ["sqlite", "sqlite3", "db", "db3"].contains(ext) else {
            return false
        }
        return !name.hasSuffix("-wal") && !name.hasSuffix("-shm") && !name.hasSuffix("-journal")
    }

    private static func scanSQLiteDatabase(
        _ url: URL,
        now: Date,
        summary: inout LocalUsageSummary,
        byDay: inout [String: UsageTotals]
    ) {
        var db: OpaquePointer?
        guard sqlite3_open_v2(url.path, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nil) == SQLITE_OK, let db else {
            if db != nil {
                sqlite3_close(db)
            }
            return
        }
        defer { sqlite3_close(db) }

        let columns = sqliteColumns(in: db, table: "threads")
        guard columns.contains("tokens_used") else {
            return
        }
        guard let timestampExpression = sqliteThreadTimestampExpression(columns: columns) else {
            return
        }

        summary.sqliteDatabaseCount += 1
        let sql = "SELECT \(timestampExpression), tokens_used FROM threads WHERE tokens_used > 0"
        var statement: OpaquePointer?
        guard sqlite3_prepare_v2(db, sql, -1, &statement, nil) == SQLITE_OK, let statement else {
            summary.parseErrors += 1
            return
        }
        defer { sqlite3_finalize(statement) }

        while sqlite3_step(statement) == SQLITE_ROW {
            let timestamp = sqlite3_column_int64(statement, 0)
            let tokens = sqlite3_column_int64(statement, 1)
            guard tokens > 0, let at = date(fromSQLiteTimestamp: timestamp) else {
                continue
            }
            let usage = UsageTotals(total: tokens)
            summary.sqlite.add(usage)
            summary.sqliteThreadCount += 1
            add(usage, at: at, now: now, summary: &summary, byDay: &byDay)
        }
    }

    private static func sqliteColumns(in db: OpaquePointer, table: String) -> Set<String> {
        var columns = Set<String>()
        var statement: OpaquePointer?
        guard sqlite3_prepare_v2(db, "PRAGMA table_info(\(table))", -1, &statement, nil) == SQLITE_OK, let statement else {
            return columns
        }
        defer { sqlite3_finalize(statement) }

        while sqlite3_step(statement) == SQLITE_ROW {
            if let pointer = sqlite3_column_text(statement, 1) {
                columns.insert(String(cString: pointer).lowercased())
            }
        }
        return columns
    }

    private static func sqliteThreadTimestampExpression(columns: Set<String>) -> String? {
        if columns.contains("updated_at_ms"), columns.contains("updated_at") {
            return "CASE WHEN updated_at_ms IS NOT NULL AND updated_at_ms > 0 THEN updated_at_ms ELSE updated_at * 1000 END"
        }
        if columns.contains("updated_at_ms") {
            return "updated_at_ms"
        }
        if columns.contains("updated_at") {
            return "updated_at"
        }
        if columns.contains("created_at_ms"), columns.contains("created_at") {
            return "CASE WHEN created_at_ms IS NOT NULL AND created_at_ms > 0 THEN created_at_ms ELSE created_at * 1000 END"
        }
        if columns.contains("created_at_ms") {
            return "created_at_ms"
        }
        if columns.contains("created_at") {
            return "created_at"
        }
        return nil
    }

    private static func date(fromSQLiteTimestamp value: Int64) -> Date? {
        guard value > 0 else {
            return nil
        }
        if value > 10_000_000_000 {
            return Date(timeIntervalSince1970: TimeInterval(value) / 1000)
        }
        return Date(timeIntervalSince1970: TimeInterval(value))
    }

    private static func add(
        _ usage: UsageTotals,
        at: Date,
        now: Date,
        summary: inout LocalUsageSummary,
        byDay: inout [String: UsageTotals]
    ) {
        summary.total.add(usage)
        if Calendar.current.isDate(at, inSameDayAs: now) {
            summary.today.add(usage)
        }
        if at >= now.addingTimeInterval(-7 * 24 * 60 * 60) {
            summary.last7Days.add(usage)
        }
        if at >= now.addingTimeInterval(-30 * 24 * 60 * 60) {
            summary.last30Days.add(usage)
        }
        var dayTotal = byDay[dayKey(at)] ?? UsageTotals()
        dayTotal.add(usage)
        byDay[dayKey(at)] = dayTotal
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
