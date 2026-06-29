import Foundation
import XCTest
@testable import CodexQuotaDockCore

final class ContractFixtureTests: XCTestCase {
    func testDecodesExistingProfilesFixture() throws {
        let data = try fixture("profiles.json")
        let store = try JSONDecoder.codex.decode(ProfileStoreFile.self, from: data)

        XCTAssertEqual(store.profiles.count, 2)
        XCTAssertEqual(store.profiles[0].alias, "team")
        XCTAssertEqual(store.profiles[0].accountID, "acct_team_123456")
        XCTAssertEqual(store.profiles[0].accountSuffix, "123456")
        XCTAssertTrue(store.profiles[0].pinned)
    }

    private func fixture(_ name: String) throws -> Data {
        var cursor = URL(fileURLWithPath: #filePath)
        while cursor.path != "/" {
            let candidate = cursor
                .deletingLastPathComponent()
                .appendingPathComponent("testdata/contracts")
                .appendingPathComponent(name)
            if FileManager.default.fileExists(atPath: candidate.path) {
                return try Data(contentsOf: candidate)
            }
            cursor.deleteLastPathComponent()
        }
        throw CocoaError(.fileNoSuchFile)
    }
}
