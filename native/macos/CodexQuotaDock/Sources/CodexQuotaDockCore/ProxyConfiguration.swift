import Foundation

public struct NetworkProxySettings: Equatable {
    public let enabled: Bool
    public let proxyHost: String
    public let proxyPort: Int
    public let exceptions: [String]

    public init(enabled: Bool = false, proxyHost: String = "", proxyPort: Int = 0, exceptions: [String] = []) {
        self.enabled = enabled
        self.proxyHost = proxyHost
        self.proxyPort = proxyPort
        self.exceptions = exceptions
    }
}

public enum ProxyConfiguration {
    public static func settings(fromEnvText text: String, host: String? = nil, isHTTPS: Bool = true) -> NetworkProxySettings {
        let env = parseEnv(text)
        let noProxy = env["no_proxy"] ?? ""
        if let host, noProxyMatches(noProxy, host: host) {
            return NetworkProxySettings()
        }

        let keys = isHTTPS
            ? ["https_proxy", "all_proxy", "http_proxy", "proxy"]
            : ["http_proxy", "all_proxy", "https_proxy", "proxy"]
        guard let rawProxy = keys.compactMap({ env[$0] }).first(where: { !$0.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }),
              let parsed = parseProxy(rawProxy)
        else {
            return NetworkProxySettings()
        }
        return NetworkProxySettings(
            enabled: true,
            proxyHost: parsed.host,
            proxyPort: parsed.port,
            exceptions: exceptions(fromNoProxy: noProxy)
        )
    }

    public static func session(timeout: TimeInterval, codexRoot: URL = AppPaths().defaultCodexRoot, host: String? = nil) -> URLSession {
        let configuration = URLSessionConfiguration.ephemeral
        configuration.timeoutIntervalForRequest = timeout
        configuration.timeoutIntervalForResource = timeout

        let envURL = codexRoot.appendingPathComponent(".env")
        if let text = try? String(contentsOf: envURL, encoding: .utf8) {
            let settings = settings(fromEnvText: text, host: host, isHTTPS: true)
            if settings.enabled {
                configuration.connectionProxyDictionary = proxyDictionary(from: settings)
            }
        }
        return URLSession(configuration: configuration)
    }

    private static func proxyDictionary(from settings: NetworkProxySettings) -> [AnyHashable: Any] {
        var dictionary: [AnyHashable: Any] = [
            "HTTPEnable": 1,
            "HTTPProxy": settings.proxyHost,
            "HTTPPort": settings.proxyPort,
            "HTTPSEnable": 1,
            "HTTPSProxy": settings.proxyHost,
            "HTTPSPort": settings.proxyPort,
        ]
        if !settings.exceptions.isEmpty {
            dictionary["ExceptionsList"] = settings.exceptions
        }
        return dictionary
    }

    private static func parseEnv(_ text: String) -> [String: String] {
        var values: [String: String] = [:]
        for rawLine in text.split(whereSeparator: \.isNewline) {
            var line = String(rawLine).trimmingCharacters(in: .whitespacesAndNewlines)
            if line.isEmpty || line.hasPrefix("#") {
                continue
            }
            if line.hasPrefix("export ") {
                line = String(line.dropFirst("export ".count)).trimmingCharacters(in: .whitespacesAndNewlines)
            }
            guard let equals = line.firstIndex(of: "=") else {
                continue
            }
            let key = String(line[..<equals]).trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
            let rawValue = String(line[line.index(after: equals)...]).trimmingCharacters(in: .whitespacesAndNewlines)
            values[key] = unquote(rawValue)
        }
        return values
    }

    private static func unquote(_ value: String) -> String {
        guard value.count >= 2,
              let first = value.first,
              let last = value.last,
              (first == "\"" && last == "\"") || (first == "'" && last == "'")
        else {
            return value
        }
        return String(value.dropFirst().dropLast())
    }

    private static func parseProxy(_ value: String) -> (host: String, port: Int)? {
        var text = unquote(value.trimmingCharacters(in: .whitespacesAndNewlines))
        if !text.contains("://") {
            text = "http://\(text)"
        }
        guard let components = URLComponents(string: text),
              let host = components.host,
              !host.isEmpty
        else {
            return nil
        }
        return (host, components.port ?? 80)
    }

    private static func splitNoProxy(_ value: String) -> [String] {
        value
            .split { $0 == "," || $0 == ";" }
            .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
            .filter { !$0.isEmpty }
    }

    private static func hostWithoutPort(_ value: String) -> String {
        var text = value.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        if text.hasPrefix("["), let close = text.firstIndex(of: "]") {
            return String(text[text.index(after: text.startIndex)..<close])
        }
        if let colon = text.lastIndex(of: ":"),
           text[..<colon].contains(":") == false,
           text[text.index(after: colon)...].allSatisfy(\.isNumber) {
            text = String(text[..<colon])
        }
        return text
    }

    private static func noProxyMatches(_ noProxy: String, host: String) -> Bool {
        let normalizedHost = hostWithoutPort(host)
        guard !normalizedHost.isEmpty else {
            return false
        }
        return splitNoProxy(noProxy).contains { rawToken in
            var token = hostWithoutPort(rawToken)
            if token.isEmpty {
                return false
            }
            if token == "*" {
                return true
            }
            if token.hasPrefix("*.") {
                token.removeFirst()
            }
            if token.hasPrefix(".") {
                let suffix = String(token.dropFirst())
                return normalizedHost == suffix || normalizedHost.hasSuffix(".\(suffix)")
            }
            return normalizedHost == token || normalizedHost.hasSuffix(".\(token)")
        }
    }

    private static func exceptions(fromNoProxy noProxy: String) -> [String] {
        splitNoProxy(noProxy).compactMap { rawToken in
            let token = rawToken.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !token.isEmpty, token != "*" else {
                return nil
            }
            if token.hasPrefix("*.") {
                return token
            }
            if token.hasPrefix(".") {
                return "*\(token)"
            }
            return hostWithoutPort(token)
        }
    }
}
