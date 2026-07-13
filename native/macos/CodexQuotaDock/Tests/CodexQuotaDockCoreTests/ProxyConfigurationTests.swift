import XCTest
@testable import CodexQuotaDockCore

final class ProxyConfigurationTests: XCTestCase {
    func testProxySettingsFromDotEnv() {
        let env = """
        HTTP_PROXY=http://127.0.0.1:10808
        HTTPS_PROXY=http://127.0.0.1:10809
        NO_PROXY=localhost,127.0.0.1,.internal
        """

        let settings = ProxyConfiguration.settings(fromEnvText: env, host: "chatgpt.com", isHTTPS: true)
        XCTAssertTrue(settings.enabled)
        XCTAssertEqual(settings.proxyHost, "127.0.0.1")
        XCTAssertEqual(settings.proxyPort, 10809)
        XCTAssertEqual(settings.exceptions, ["localhost", "127.0.0.1", "*.internal"])

        let bypassed = ProxyConfiguration.settings(fromEnvText: env, host: "api.internal", isHTTPS: true)
        XCTAssertFalse(bypassed.enabled)
    }
}
