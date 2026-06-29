import Foundation

public final class SettingsStore {
    private let url: URL
    private let fileManager: FileManager

    public init(url: URL, fileManager: FileManager = .default) {
        self.url = url
        self.fileManager = fileManager
    }

    public func load() -> AppSettings {
        guard let data = try? Data(contentsOf: url),
              let settings = try? JSONDecoder.codex.decode(AppSettings.self, from: data)
        else {
            return .defaults
        }
        return settings.validated()
    }

    public func save(_ settings: AppSettings) throws {
        try fileManager.createDirectory(
            at: url.deletingLastPathComponent(),
            withIntermediateDirectories: true
        )
        let data = try JSONEncoder.codex.encode(settings.validated())
        try data.write(to: url, options: .atomic)
    }
}
