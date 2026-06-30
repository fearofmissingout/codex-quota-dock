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
}
