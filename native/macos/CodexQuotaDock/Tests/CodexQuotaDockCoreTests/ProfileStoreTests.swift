import XCTest
@testable import CodexQuotaDockCore

final class ProfileStoreTests: XCTestCase {
    func testSavesAndLoadsProfileMetadataAndAuthJSON() throws {
        let root = try TemporaryDirectory()
        let store = ProfileStore(configDirectory: root.url)

        let auth = authJSON(accountID: "acct_123", accessToken: "token-123")
        let profile = try store.importAuth(alias: "team", authJSON: auth, now: Date(timeIntervalSince1970: 1))

        let loaded = try store.load()
        XCTAssertEqual(loaded.profiles.map(\.alias), ["team"])
        XCTAssertEqual(try store.authJSON(for: profile), auth)
    }

    func testImportUpdatesExistingAccountWhenRequested() throws {
        let root = try TemporaryDirectory()
        let store = ProfileStore(configDirectory: root.url)
        _ = try store.importAuth(alias: "team", authJSON: authJSON(accountID: "acct_123", accessToken: "old"), now: Date(timeIntervalSince1970: 1))
        _ = try store.importAuth(alias: "team-new", authJSON: authJSON(accountID: "acct_123", accessToken: "fresh"), now: Date(timeIntervalSince1970: 2), updateExistingAccount: true)

        let loaded = try store.load()
        XCTAssertEqual(loaded.profiles.count, 1)
        XCTAssertEqual(loaded.profiles[0].alias, "team-new")
    }

    func testDeleteRemovesProfileAndAuth() throws {
        let root = try TemporaryDirectory()
        let store = ProfileStore(configDirectory: root.url)
        let profile = try store.importAuth(alias: "team", authJSON: authJSON(accountID: "acct_123", accessToken: "token"))
        let authURL = store.authURL(for: profile)

        try store.delete(profileID: profile.id)

        XCTAssertFalse(FileManager.default.fileExists(atPath: authURL.path))
        XCTAssertEqual(try store.load().profiles.count, 0)
    }

    func testUpdatesPriorityAndAutoSwitchFlag() throws {
        let root = try TemporaryDirectory()
        let store = ProfileStore(configDirectory: root.url)
        let profile = try store.importAuth(alias: "team", authJSON: authJSON(accountID: "acct_123", accessToken: "token"))

        let updated = try store.updateAutomation(profileID: profile.id, priority: 10, autoSwitchAllowed: false)

        XCTAssertEqual(updated.priority, 10)
        XCTAssertFalse(updated.autoSwitchAllowed)
        let loaded = try store.load().profiles[0]
        XCTAssertEqual(loaded.priority, 10)
        XCTAssertFalse(loaded.autoSwitchAllowed)
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
