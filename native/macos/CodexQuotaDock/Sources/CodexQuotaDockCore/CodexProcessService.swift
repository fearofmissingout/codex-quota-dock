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
        let appURL = codexAppURL(configuredPath: appPath)
        for app in candidates {
            app.terminate()
        }
        let closedCount = waitForTermination(candidates, timeout: 5)
        guard candidates.isEmpty || closedCount == candidates.count else {
            return RestartResult(
                closedCount: closedCount,
                reopened: false,
                message: "Codex quit was requested, but the old process did not exit yet."
            )
        }
        let reopened = reopenCodex(appURL: appURL)
        let message: String
        if reopened, candidates.isEmpty {
            message = "Codex launch requested."
        } else if reopened {
            message = "Codex restart requested."
        } else if candidates.isEmpty {
            message = "Codex was not running, and automatic launch was not available."
        } else {
            message = "Codex was closed, but automatic reopen was not available."
        }
        return RestartResult(
            closedCount: closedCount,
            reopened: reopened,
            message: message
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

    private func reopenCodex(appURL: URL?) -> Bool {
        guard let appURL else {
            return false
        }
        NSWorkspace.shared.open(appURL)
        return true
    }

    private func waitForTermination(_ applications: [NSRunningApplication], timeout: TimeInterval) -> Int {
        guard !applications.isEmpty else {
            return 0
        }
        let deadline = Date().addingTimeInterval(timeout)
        while Date() < deadline {
            let closedCount = applications.filter(\.isTerminated).count
            if closedCount == applications.count {
                return closedCount
            }
            _ = RunLoop.current.run(mode: .default, before: Date().addingTimeInterval(0.05))
        }
        return applications.filter(\.isTerminated).count
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
