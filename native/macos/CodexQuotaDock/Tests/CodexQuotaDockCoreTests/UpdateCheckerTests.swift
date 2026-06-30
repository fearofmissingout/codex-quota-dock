import XCTest
@testable import CodexQuotaDockCore

final class UpdateCheckerTests: XCTestCase {
    func testParsesInstallableMacOSUniversalUpdate() throws {
        let result = try UpdateChecker.parseRelease(releaseJSON(
            tag: "v0.8.0",
            assets: [
                asset("codex-quota-dock-native-windows-x64.zip"),
                asset("codex-quota-dock-native-macos-universal.zip", url: "https://example.com/mac.zip", size: 42),
            ]
        ), currentVersion: "0.7.0")

        XCTAssertTrue(result.available)
        XCTAssertEqual(result.latestVersion, "v0.8.0")
        XCTAssertEqual(result.asset?.name, "codex-quota-dock-native-macos-universal.zip")
        XCTAssertEqual(result.asset?.browserDownloadURL, "https://example.com/mac.zip")
        XCTAssertEqual(result.asset?.size, 42)
    }

    func testIgnoresCurrentVersion() throws {
        let result = try UpdateChecker.parseRelease(releaseJSON(
            tag: "v0.8.0",
            assets: [asset("codex-quota-dock-native-macos-universal.zip")]
        ), currentVersion: "0.8.0")

        XCTAssertFalse(result.available)
        XCTAssertEqual(result.message, "Already on the latest version.")
    }

    func testReportsMissingMacAsset() throws {
        let result = try UpdateChecker.parseRelease(releaseJSON(
            tag: "v0.9.0",
            assets: [asset("codex-quota-dock-native-windows-x64.zip")]
        ), currentVersion: "0.8.0")

        XCTAssertFalse(result.available)
        XCTAssertNil(result.asset)
        XCTAssertEqual(result.message, "A newer release exists, but no macOS universal build was attached.")
    }

    private func releaseJSON(tag: String, assets: [String]) -> Data {
        """
        {
          "tag_name": "\(tag)",
          "html_url": "https://github.com/fearofmissingout/codex-quota-dock/releases/tag/\(tag)",
          "assets": [
            \(assets.joined(separator: ","))
          ]
        }
        """.data(using: .utf8)!
    }

    private func asset(_ name: String, url: String = "https://example.com/download.zip", size: Int = 1) -> String {
        """
        {
          "name": "\(name)",
          "browser_download_url": "\(url)",
          "size": \(size)
        }
        """
    }
}
