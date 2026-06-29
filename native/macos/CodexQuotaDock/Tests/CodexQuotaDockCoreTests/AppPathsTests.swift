import XCTest
@testable import CodexQuotaDockCore

final class AppPathsTests: XCTestCase {
    func testDefaultConfigDirectoryUsesApplicationSupport() throws {
        let paths = AppPaths(
            homeDirectory: URL(fileURLWithPath: "/Users/tester"),
            environment: [:]
        )

        XCTAssertEqual(
            paths.configDirectory.path,
            "/Users/tester/Library/Application Support/codex-quota-dock"
        )
        XCTAssertEqual(paths.defaultCodexAuth.path, "/Users/tester/.codex/auth.json")
    }

    func testCodexHomeOverridesDefaultAuthPath() throws {
        let paths = AppPaths(
            homeDirectory: URL(fileURLWithPath: "/Users/tester"),
            environment: ["CODEX_HOME": "/tmp/custom-codex"]
        )

        XCTAssertEqual(paths.defaultCodexAuth.path, "/tmp/custom-codex/auth.json")
    }
}
