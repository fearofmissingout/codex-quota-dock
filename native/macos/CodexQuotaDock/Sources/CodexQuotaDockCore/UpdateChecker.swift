import Foundation

public struct ReleaseAssetInfo: Equatable {
    public let name: String
    public let browserDownloadURL: String
    public let size: Int
}

public struct UpdateCheckResult: Equatable {
    public let available: Bool
    public let currentVersion: String
    public let latestVersion: String
    public let releaseURL: String
    public let asset: ReleaseAssetInfo?
    public let checksumAsset: ReleaseAssetInfo?
    public let message: String
}

public enum UpdateCheckerError: Error, LocalizedError {
    case invalidResponse
    case invalidRelease

    public var errorDescription: String? {
        switch self {
        case .invalidResponse:
            "GitHub did not return a valid release response."
        case .invalidRelease:
            "Latest GitHub release JSON is not valid."
        }
    }
}

public struct UpdateChecker {
    public static let latestReleaseAPI = URL(
        string: "https://api.github.com/repos/fearofmissingout/codex-quota-dock/releases/latest"
    )!

    private let currentVersion: String

    public init(currentVersion: String = NativeVersion.current) {
        self.currentVersion = currentVersion
    }

    public func checkLatestRelease() async throws -> UpdateCheckResult {
        var request = URLRequest(url: Self.latestReleaseAPI)
        request.timeoutInterval = 12
        request.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
        request.setValue("CodexQuotaDock/\(currentVersion)", forHTTPHeaderField: "User-Agent")

        let session = ProxyConfiguration.session(timeout: 12, host: Self.latestReleaseAPI.host)
        let (data, response) = try await session.data(for: request)
        guard let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode) else {
            throw UpdateCheckerError.invalidResponse
        }
        return try Self.parseRelease(data, currentVersion: currentVersion)
    }

    public static func parseRelease(_ data: Data, currentVersion: String) throws -> UpdateCheckResult {
        let release = try JSONDecoder().decode(GitHubRelease.self, from: data)
        let asset = release.assets.first { asset in
            let name = asset.name.lowercased()
            return name.contains("native-macos-universal") && name.hasSuffix(".zip")
        }
        let checksumAsset = release.assets.first { asset in
            let name = asset.name.lowercased()
            return name == "sha256sums.txt" || name == "checksums.txt"
        }
        let isNewer = isNewerVersion(current: currentVersion, latest: release.tagName)
        let message: String
        if !isNewer {
            message = "Already on the latest version."
        } else if asset == nil {
            message = "A newer release exists, but no macOS universal build was attached."
        } else if checksumAsset == nil {
            message = "A newer macOS build is available, but automatic install needs SHA256SUMS.txt."
        } else {
            message = "A newer macOS build is available."
        }

        return UpdateCheckResult(
            available: isNewer && asset != nil,
            currentVersion: currentVersion,
            latestVersion: release.tagName,
            releaseURL: release.htmlURL,
            asset: asset.map {
                ReleaseAssetInfo(
                    name: $0.name,
                    browserDownloadURL: $0.browserDownloadURL,
                    size: $0.size
                )
            },
            checksumAsset: checksumAsset.map {
                ReleaseAssetInfo(
                    name: $0.name,
                    browserDownloadURL: $0.browserDownloadURL,
                    size: $0.size
                )
            },
            message: message
        )
    }

    public static func checksum(for assetName: String, in checksumsText: String) -> String? {
        for rawLine in checksumsText.split(whereSeparator: \.isNewline) {
            let line = String(rawLine).trimmingCharacters(in: .whitespacesAndNewlines)
            if line.isEmpty || line.hasPrefix("#") {
                continue
            }
            let parts = line.split(whereSeparator: \.isWhitespace)
            guard parts.count >= 2 else {
                continue
            }
            let hash = String(parts[0]).lowercased()
            var name = String(parts[1])
            if name.hasPrefix("*") {
                name.removeFirst()
            }
            if hash.count == 64 && name == assetName {
                return hash
            }
        }
        return nil
    }

    static func isNewerVersion(current: String, latest: String) -> Bool {
        let currentParts = versionParts(current)
        let latestParts = versionParts(latest)
        let count = max(currentParts.count, latestParts.count)

        for index in 0..<count {
            let left = index < currentParts.count ? currentParts[index] : 0
            let right = index < latestParts.count ? latestParts[index] : 0
            if right > left {
                return true
            }
            if right < left {
                return false
            }
        }
        return false
    }

    private static func versionParts(_ version: String) -> [Int] {
        version
            .trimmingCharacters(in: CharacterSet(charactersIn: "vV"))
            .split { !$0.isNumber }
            .map { Int($0) ?? 0 }
    }
}

private struct GitHubRelease: Decodable {
    let tagName: String
    let htmlURL: String
    let assets: [GitHubReleaseAsset]

    enum CodingKeys: String, CodingKey {
        case tagName = "tag_name"
        case htmlURL = "html_url"
        case assets
    }
}

private struct GitHubReleaseAsset: Decodable {
    let name: String
    let browserDownloadURL: String
    let size: Int

    enum CodingKeys: String, CodingKey {
        case name
        case browserDownloadURL = "browser_download_url"
        case size
    }
}
