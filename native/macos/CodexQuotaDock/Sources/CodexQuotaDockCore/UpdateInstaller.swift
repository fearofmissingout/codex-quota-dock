import CryptoKit
import Foundation

public struct DownloadedUpdate: Equatable {
    public let packageURL: URL
    public let assetName: String
    public let expectedSHA256: String
    public let actualSHA256: String
}

public enum UpdateInstallerError: Error, LocalizedError {
    case noInstallableAsset
    case missingChecksumAsset
    case invalidDownloadURL
    case missingChecksum(String)
    case checksumMismatch(expected: String, actual: String)

    public var errorDescription: String? {
        switch self {
        case .noInstallableAsset:
            "No installable update asset is available."
        case .missingChecksumAsset:
            "The release does not include SHA256SUMS.txt, so automatic install is disabled."
        case .invalidDownloadURL:
            "The update download URL is not valid."
        case .missingChecksum(let asset):
            "SHA256SUMS.txt does not include \(asset)."
        case .checksumMismatch:
            "The downloaded update checksum did not match."
        }
    }
}

public struct UpdateInstaller {
    public init() {}

    public func download(_ update: UpdateCheckResult, to directory: URL) async throws -> DownloadedUpdate {
        guard let asset = update.asset else {
            throw UpdateInstallerError.noInstallableAsset
        }
        guard let checksumAsset = update.checksumAsset else {
            throw UpdateInstallerError.missingChecksumAsset
        }
        guard let assetURL = URL(string: asset.browserDownloadURL),
              let checksumURL = URL(string: checksumAsset.browserDownloadURL)
        else {
            throw UpdateInstallerError.invalidDownloadURL
        }

        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)

        let session = ProxyConfiguration.session(timeout: 30)
        let (checksumData, _) = try await session.data(from: checksumURL)
        let checksumText = String(decoding: checksumData, as: UTF8.self)
        guard let expected = UpdateChecker.checksum(for: asset.name, in: checksumText) else {
            throw UpdateInstallerError.missingChecksum(asset.name)
        }

        let (temporaryURL, _) = try await session.download(from: assetURL)
        let packageURL = directory.appendingPathComponent(asset.name)
        if FileManager.default.fileExists(atPath: packageURL.path) {
            try FileManager.default.removeItem(at: packageURL)
        }
        try FileManager.default.moveItem(at: temporaryURL, to: packageURL)

        let actual = try Self.sha256Hex(for: packageURL)
        guard actual.lowercased() == expected.lowercased() else {
            throw UpdateInstallerError.checksumMismatch(expected: expected, actual: actual)
        }

        return DownloadedUpdate(
            packageURL: packageURL,
            assetName: asset.name,
            expectedSHA256: expected,
            actualSHA256: actual
        )
    }

    public static func sha256Hex(for url: URL) throws -> String {
        let data = try Data(contentsOf: url)
        let digest = SHA256.hash(data: data)
        return digest.map { String(format: "%02x", $0) }.joined()
    }
}
