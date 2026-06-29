import XCTest
@testable import CodexQuotaDockCore

final class AuthSwitcherTests: XCTestCase {
    func testSwitchCreatesBackupBeforeReplacingActiveAuth() throws {
        let root = try TemporaryDirectory()
        let active = root.url.appendingPathComponent("auth.json")
        let backups = root.url.appendingPathComponent("backups", isDirectory: true)
        try FileManager.default.createDirectory(at: backups, withIntermediateDirectories: true)
        try oldAuth.write(to: active)

        let switcher = AuthSwitcher(fileManager: .default)
        let result = try switcher.switchAuth(
            activeAuth: active,
            targetAuthJSON: newAuth,
            backupsDirectory: backups,
            now: Date(timeIntervalSince1970: 1)
        )

        XCTAssertEqual(try Data(contentsOf: active), newAuth)
        XCTAssertNotNil(result.backupURL)
        XCTAssertEqual(try Data(contentsOf: result.backupURL!), oldAuth)
    }

    private var oldAuth: Data {
        authJSON(accountID: "old", accessToken: "old-token")
    }

    private var newAuth: Data {
        authJSON(accountID: "new", accessToken: "new-token")
    }

    private func authJSON(accountID: String, accessToken: String) -> Data {
        #"{"tokens":{"access_token":"\#(accessToken)","account_id":"\#(accountID)"}}"#.data(using: .utf8)!
    }
}
