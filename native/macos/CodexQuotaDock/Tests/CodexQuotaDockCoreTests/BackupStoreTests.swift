import XCTest
@testable import CodexQuotaDockCore

final class BackupStoreTests: XCTestCase {
    func testExportsAndImportsProfilesWithMetadata() throws {
        let sourceRoot = try TemporaryDirectory()
        let source = ProfileStore(configDirectory: sourceRoot.url)
        let profile = try source.importAuth(
            alias: "team",
            authJSON: authJSON(accountID: "acct_123", accessToken: "token-123")
        )
        try source.setPinned(profileID: profile.id, pinned: true)
        try source.updateAutomation(profileID: profile.id, priority: 20, autoSwitchAllowed: false)

        var settings = AppSettings.defaults
        settings.autoRestartCodex = true
        let data = try BackupStore.exportBackup(
            store: source,
            settings: settings,
            now: Date(timeIntervalSince1970: 1)
        )

        let text = String(data: data, encoding: .utf8)!
        XCTAssertTrue(text.contains("\"auth_json\""))
        XCTAssertTrue(text.contains("\"profiles\""))

        let destinationRoot = try TemporaryDirectory()
        let destination = ProfileStore(configDirectory: destinationRoot.url)
        let summary = try BackupStore.importBackup(into: destination, data: data)

        XCTAssertEqual(summary, BackupImportSummary(created: 1, updated: 0, skipped: 0))
        let loaded = try destination.load().profiles[0]
        XCTAssertEqual(loaded.alias, "team")
        XCTAssertEqual(loaded.accountID, "acct_123")
        XCTAssertTrue(loaded.pinned)
        XCTAssertEqual(loaded.priority, 20)
        XCTAssertFalse(loaded.autoSwitchAllowed)
        XCTAssertTrue(String(data: try destination.authJSON(for: loaded), encoding: .utf8)!.contains("token-123"))
    }

    func testImportUpdatesExistingAccount() throws {
        let root = try TemporaryDirectory()
        let store = ProfileStore(configDirectory: root.url)
        _ = try store.importAuth(
            alias: "old-team",
            authJSON: authJSON(accountID: "acct_123", accessToken: "old-token")
        )

        let sourceRoot = try TemporaryDirectory()
        let source = ProfileStore(configDirectory: sourceRoot.url)
        _ = try source.importAuth(
            alias: "new-team",
            authJSON: authJSON(accountID: "acct_123", accessToken: "fresh-token")
        )
        let data = try BackupStore.exportBackup(store: source, settings: .defaults)

        let summary = try BackupStore.importBackup(into: store, data: data)

        XCTAssertEqual(summary, BackupImportSummary(created: 0, updated: 1, skipped: 0))
        let profiles = try store.load().profiles
        XCTAssertEqual(profiles.count, 1)
        XCTAssertEqual(profiles[0].alias, "new-team")
        XCTAssertTrue(String(data: try store.authJSON(for: profiles[0]), encoding: .utf8)!.contains("fresh-token"))
    }

    func testImportsWindowsBackupWithFloatingPointSettings() throws {
        let root = try TemporaryDirectory()
        let store = ProfileStore(configDirectory: root.url)
        let data = """
        {
          "exported_at": "2026-07-01T00:00:00Z",
          "profiles": [
            {
              "account_id": "acct_win",
              "account_suffix": "ct_win",
              "alias": "win-team",
              "auth_mode": "chatgpt",
              "auth_json": {
                "auth_mode": "chatgpt",
                "tokens": {
                  "access_token": "win-token",
                  "refresh_token": "fixture-refresh",
                  "account_id": "acct_win"
                }
              }
            }
          ],
          "settings": {
            "poll_interval_minutes": 5.0,
            "switch_away_threshold": 5.0,
            "switch_to_threshold": 30.0
          },
          "version": 1.0
        }
        """.data(using: .utf8)!

        let summary = try BackupStore.importBackup(into: store, data: data)

        XCTAssertEqual(summary, BackupImportSummary(created: 1, updated: 0, skipped: 0))
        XCTAssertEqual(try store.load().profiles[0].alias, "win-team")
    }

    func testRestoreLatestBackupCopiesNewestJsonFile() throws {
        let root = try TemporaryDirectory()
        let backups = root.url.appendingPathComponent("backups", isDirectory: true)
        let active = root.url.appendingPathComponent("auth.json")
        try FileManager.default.createDirectory(at: backups, withIntermediateDirectories: true)
        try "old".data(using: .utf8)!.write(to: active)
        try "older".data(using: .utf8)!.write(to: backups.appendingPathComponent("auth-backup-20260101.json"))
        try "newer".data(using: .utf8)!.write(to: backups.appendingPathComponent("auth-backup-20260201.json"))

        try BackupStore.restoreLatestBackup(from: backups, to: active)

        XCTAssertEqual(String(data: try Data(contentsOf: active), encoding: .utf8), "newer")
    }

    private func authJSON(accountID: String, accessToken: String) -> Data {
        """
        {
          "auth_mode": "chatgpt",
          "tokens": {
            "access_token": "\(accessToken)",
            "refresh_token": "fixture-refresh",
            "account_id": "\(accountID)"
          },
          "last_refresh": "2026-06-29T00:00:00Z"
        }
        """.data(using: .utf8)!
    }
}
