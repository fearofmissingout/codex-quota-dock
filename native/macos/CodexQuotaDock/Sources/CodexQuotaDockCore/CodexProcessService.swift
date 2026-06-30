import Foundation

#if canImport(AppKit)
import AppKit
#endif

public struct RestartResult: Equatable {
    public let closedCount: Int
    public let reopened: Bool
    public let message: String
}

public final class CodexProcessService {
    public init() {}

    public func isCodexRunning() -> Bool {
        #if canImport(AppKit)
        !runningCodexApplications().isEmpty
        #else
        false
        #endif
    }

    public func restartCodex(appPath: String = "") -> RestartResult {
        #if canImport(AppKit)
        let candidates = runningCodexApplications()
        for app in candidates {
            app.terminate()
        }
        let reopened = reopenCodex(appPath: appPath)
        return RestartResult(
            closedCount: candidates.count,
            reopened: reopened,
            message: reopened ? "Codex restart requested." : "Codex was closed, but automatic reopen was not available."
        )
        #else
        return RestartResult(closedCount: 0, reopened: false, message: "Codex restart is only available on macOS.")
        #endif
    }

    public func detectCodexAppPath(configuredPath: String = "") -> String? {
        #if canImport(AppKit)
        if let url = codexAppURL(configuredPath: configuredPath) {
            return url.path
        }
        return nil
        #else
        return nil
        #endif
    }

    #if canImport(AppKit)
    private func runningCodexApplications() -> [NSRunningApplication] {
        NSWorkspace.shared.runningApplications.filter { app in
            let name = (app.localizedName ?? "").lowercased()
            let bundleID = (app.bundleIdentifier ?? "").lowercased()
            return name == "codex" || bundleID == "com.openai.codex" || bundleID.hasPrefix("com.openai.codex.")
        }
    }

    private func reopenCodex(appPath: String) -> Bool {
        guard let appURL = codexAppURL(configuredPath: appPath) else {
            return false
        }
        NSWorkspace.shared.open(appURL)
        return true
    }

    private func codexAppURL(configuredPath: String) -> URL? {
        let workspace = NSWorkspace.shared
        let trimmed = configuredPath.trimmingCharacters(in: .whitespacesAndNewlines)
        if !trimmed.isEmpty {
            let url = URL(fileURLWithPath: trimmed)
            if FileManager.default.fileExists(atPath: url.path) {
                return url
            }
        }
        if let appURL = workspace.urlForApplication(withBundleIdentifier: "com.openai.codex") {
            return appURL
        }
        if let running = runningCodexApplications().first?.bundleURL {
            return running
        }
        let appURL = URL(fileURLWithPath: "/Applications/Codex.app")
        if FileManager.default.fileExists(atPath: appURL.path) {
            return appURL
        }
        return nil
    }
    #endif
}
