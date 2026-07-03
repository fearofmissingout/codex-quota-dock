import XCTest
@testable import CodexQuotaDockCore

final class UpdateCheckerTests: XCTestCase {
    func testParsesInstallableMacOSUniversalUpdate() throws {
        let result = try UpdateChecker.parseRelease(releaseJSON(
            tag: "v0.8.0",
            assets: [
                asset("codex-quota-dock-native-windows-x64.zip"),
                asset("codex-quota-dock-native-macos-universal.zip", url: "https://example.com/mac.zip", size: 42),
                asset("SHA256SUMS.txt", url: "https://example.com/SHA256SUMS.txt", size: 256),
            ]
        ), currentVersion: "0.7.0")

        XCTAssertTrue(result.available)
        XCTAssertEqual(result.latestVersion, "v0.8.0")
        XCTAssertEqual(result.asset?.name, "codex-quota-dock-native-macos-universal.zip")
        XCTAssertEqual(result.asset?.browserDownloadURL, "https://example.com/mac.zip")
        XCTAssertEqual(result.asset?.size, 42)
        XCTAssertEqual(result.checksumAsset?.name, "SHA256SUMS.txt")
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

    func testReportsMissingChecksumAsset() throws {
        let result = try UpdateChecker.parseRelease(releaseJSON(
            tag: "v0.9.0",
            assets: [asset("codex-quota-dock-native-macos-universal.zip")]
        ), currentVersion: "0.8.0")

        XCTAssertTrue(result.available)
        XCTAssertNil(result.checksumAsset)
        XCTAssertEqual(result.message, "A newer macOS build is available, but automatic install needs SHA256SUMS.txt.")
    }

    func testParsesChecksumForAsset() throws {
        let checksum = UpdateChecker.checksum(
            for: "codex-quota-dock-native-macos-universal.zip",
            in: """
            aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa  codex-quota-dock-native-windows-amd64.exe
            bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb *codex-quota-dock-native-macos-universal.zip
            """
        )

        XCTAssertEqual(checksum, String(repeating: "b", count: 64))
    }

    func testComputesSHA256ForUpdatePackage() throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let file = directory.appendingPathComponent("payload.txt")
        try Data("abc".utf8).write(to: file)

        XCTAssertEqual(
            try UpdateInstaller.sha256Hex(for: file),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        )
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
