import XCTest
@testable import CodexQuotaDockCore

final class HealthCheckTests: XCTestCase {
    func testReportsMissingAuthAndProfileCount() throws {
        let root = try TemporaryDirectory()
        let paths = AppPaths(homeDirectory: root.url, environment: [:])
        let store = ProfileStore(configDirectory: paths.configDirectory)

        let rows = HealthCheck().run(paths: paths, store: store)

        XCTAssertTrue(rows.contains { $0.label == "Active auth" && $0.status == .warning })
        XCTAssertTrue(rows.contains { $0.label == "Profiles" && $0.detail.contains("0") })
    }
}
