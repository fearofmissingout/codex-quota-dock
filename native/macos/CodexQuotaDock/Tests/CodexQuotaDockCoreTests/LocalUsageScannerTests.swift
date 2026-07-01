import XCTest
@testable import CodexQuotaDockCore

final class LocalUsageScannerTests: XCTestCase {
    func testScansTokenCountEventsIntoSummaryWindows() throws {
        let root = try TemporaryDirectory()
        let sessions = root.url.appendingPathComponent("sessions", isDirectory: true)
        try FileManager.default.createDirectory(at: sessions, withIntermediateDirectories: true)
        try """
        {"timestamp":"2026-06-30T02:00:00Z","payload":{"type":"token_count","info":{"last_token_usage":{"input_tokens":10,"cached_input_tokens":4,"output_tokens":5,"reasoning_output_tokens":2,"total_tokens":15}}}}
        {"timestamp":"2026-06-25T02:00:00Z","payload":{"type":"token_count","info":{"last_token_usage":{"input_tokens":30,"cached_input_tokens":10,"output_tokens":7,"reasoning_output_tokens":3,"total_tokens":37}}}}
        {"timestamp":"2026-05-01T02:00:00Z","payload":{"type":"other"}}
        {bad json}
        """.data(using: .utf8)!.write(to: sessions.appendingPathComponent("fixture.jsonl"))

        let summary = LocalUsageScanner.scan(
            codexRoot: root.url,
            now: Date(timeIntervalSince1970: 1_782_820_800)
        )

        XCTAssertEqual(summary.sessionCount, 1)
        XCTAssertEqual(summary.parseErrors, 1)
        XCTAssertEqual(summary.today.total, 15)
        XCTAssertEqual(summary.last7Days.total, 52)
        XCTAssertEqual(summary.last30Days.total, 52)
        XCTAssertEqual(summary.total.input, 40)
        XCTAssertEqual(summary.total.cachedInput, 14)
        XCTAssertEqual(summary.total.output, 12)
        XCTAssertEqual(summary.total.reasoningOutput, 5)
        XCTAssertEqual(summary.byDay.map(\.day), ["2026-06-25", "2026-06-30"])
    }

    func testScansSQLiteThreadTokenTotals() throws {
        let root = try TemporaryDirectory()
        let database = root.url.appendingPathComponent("state_5.sqlite")
        let now = Date(timeIntervalSince1970: 1_782_820_800)
        let today = Int64(now.timeIntervalSince1970)
        let older = Int64(now.addingTimeInterval(-8 * 24 * 60 * 60).timeIntervalSince1970)
        try runSQLite(database: database, sql: """
        CREATE TABLE threads (
          id TEXT PRIMARY KEY,
          updated_at INTEGER NOT NULL,
          tokens_used INTEGER NOT NULL DEFAULT 0
        );
        INSERT INTO threads (id, updated_at, tokens_used) VALUES ('thread-today', \(today), 100);
        INSERT INTO threads (id, updated_at, tokens_used) VALUES ('thread-older', \(older), 25);
        INSERT INTO threads (id, updated_at, tokens_used) VALUES ('thread-empty', \(today), 0);
        """)

        let summary = LocalUsageScanner.scan(codexRoot: root.url, now: now)

        XCTAssertEqual(summary.sqliteDatabaseCount, 1)
        XCTAssertEqual(summary.sqliteThreadCount, 2)
        XCTAssertEqual(summary.sqlite.total, 125)
        XCTAssertEqual(summary.total.total, 125)
        XCTAssertEqual(summary.today.total, 100)
        XCTAssertEqual(summary.last7Days.total, 100)
        XCTAssertEqual(summary.last30Days.total, 125)
        XCTAssertEqual(summary.byDay.map(\.day), [dayString(for: Date(timeIntervalSince1970: TimeInterval(older))), dayString(for: now)])
    }

    private func runSQLite(database: URL, sql: String) throws {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/sqlite3")
        process.arguments = [database.path, sql]
        try process.run()
        process.waitUntilExit()
        XCTAssertEqual(process.terminationStatus, 0)
    }

    private func dayString(for date: Date) -> String {
        let formatter = DateFormatter()
        formatter.calendar = Calendar.current
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.dateFormat = "yyyy-MM-dd"
        return formatter.string(from: date)
    }
}
