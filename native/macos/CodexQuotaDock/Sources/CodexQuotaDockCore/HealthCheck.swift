import Foundation

public enum HealthStatus: String, Codable, Equatable {
    case ok
    case warning
    case error
}

public struct HealthRow: Identifiable, Equatable {
    public let id: String
    public let status: HealthStatus
    public let label: String
    public let detail: String

    public init(id: String = UUID().uuidString, status: HealthStatus, label: String, detail: String) {
        self.id = id
        self.status = status
        self.label = label
        self.detail = detail
    }
}

public struct HealthCheck {
    public let fileManager: FileManager

    public init(fileManager: FileManager = .default) {
        self.fileManager = fileManager
    }

    public func run(paths: AppPaths, store: ProfileStore) -> [HealthRow] {
        var rows: [HealthRow] = []
        if fileManager.fileExists(atPath: paths.defaultCodexAuth.path) {
            do {
                _ = try AuthMetadata.parse(Data(contentsOf: paths.defaultCodexAuth))
                rows.append(HealthRow(status: .ok, label: "Active auth", detail: paths.defaultCodexAuth.path))
            } catch {
                rows.append(HealthRow(status: .error, label: "Active auth", detail: error.localizedDescription))
            }
        } else {
            rows.append(HealthRow(status: .warning, label: "Active auth", detail: "Missing at \(paths.defaultCodexAuth.path)"))
        }

        do {
            let profiles = try store.load().profiles
            rows.append(HealthRow(status: .ok, label: "Profiles", detail: "\(profiles.count) saved profile(s)"))
        } catch {
            rows.append(HealthRow(status: .error, label: "Profiles", detail: error.localizedDescription))
        }

        rows.append(HealthRow(status: .ok, label: "Version", detail: NativeVersion.current))
        return rows
    }
}
