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

    public func restartCodex() -> RestartResult {
        #if canImport(AppKit)
        let candidates = runningCodexApplications()
        for app in candidates {
            app.terminate()
        }
        let reopened = reopenCodex()
        return RestartResult(
            closedCount: candidates.count,
            reopened: reopened,
            message: reopened ? "Codex restart requested." : "Codex was closed, but automatic reopen was not available."
        )
        #else
        return RestartResult(closedCount: 0, reopened: false, message: "Codex restart is only available on macOS.")
        #endif
    }

    #if canImport(AppKit)
    private func runningCodexApplications() -> [NSRunningApplication] {
        NSWorkspace.shared.runningApplications.filter { app in
            let name = (app.localizedName ?? "").lowercased()
            let bundleID = (app.bundleIdentifier ?? "").lowercased()
            return name.contains("codex") || bundleID.contains("codex")
        }
    }

    private func reopenCodex() -> Bool {
        let workspace = NSWorkspace.shared
        if let appURL = workspace.urlForApplication(withBundleIdentifier: "com.openai.codex") {
            workspace.open(appURL)
            return true
        }
        let appURL = URL(fileURLWithPath: "/Applications/Codex.app")
        if FileManager.default.fileExists(atPath: appURL.path) {
            workspace.open(appURL)
            return true
        }
        return false
    }
    #endif
}
