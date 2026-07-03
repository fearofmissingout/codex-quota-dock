import AppKit
import CodexQuotaDockCore

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var statusItem: NSStatusItem!
    private var monitor: MonitorPanelController!
    private var settings: SettingsWindowController!
    private var touchBarController: TouchBarController!
    private var model: NativeAppModel!

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)

        model = NativeAppModel(paths: AppPaths())
        model.openSettingsHandler = { [weak self] in self?.openSettings() }
        touchBarController = TouchBarController(model: model)
        monitor = MonitorPanelController(model: model, touchBarController: touchBarController)
        settings = SettingsWindowController(model: model, touchBarController: touchBarController)
        model.settingsChangedHandler = { [weak self] settings in
            self?.monitor.applySettings(settings)
        }
        NSApp.touchBar = touchBarController.makeTouchBar()

        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        statusItem.button?.image = NSImage(systemSymbolName: "gauge.with.dots.needle.33percent", accessibilityDescription: "Codex Quota Dock")
        statusItem.button?.action = #selector(toggleMonitor)
        statusItem.button?.target = self
        statusItem.menu = makeMenu()

        model.reload()
        model.startAutoRefresh()
        showTrustReminderIfNeeded()
    }

    private func makeMenu() -> NSMenu {
        let menu = NSMenu()
        menu.addItem(item("Show Monitor", #selector(showMonitor)))
        menu.addItem(item("Refresh", #selector(refresh), key: "r"))
        menu.addItem(item("Settings...", #selector(openSettings), key: ","))
        menu.addItem(.separator())
        menu.addItem(item("Quit", #selector(quit), key: "q"))
        return menu
    }

    private func item(_ title: String, _ action: Selector, key: String = "") -> NSMenuItem {
        let item = NSMenuItem(title: title, action: action, keyEquivalent: key)
        item.target = self
        return item
    }

    @objc private func toggleMonitor() {
        showMonitor()
    }

    @objc private func showMonitor() {
        monitor.showNearStatusItem(statusItem)
    }

    @objc private func refresh() {
        Task { await model.refreshQuotas() }
    }

    @objc private func openSettings() {
        settings.show()
    }

    @objc private func quit() {
        NSApp.terminate(nil)
    }

    private func showTrustReminderIfNeeded() {
        let appURL = Bundle.main.bundleURL
        let key = "macTrustReminderShown.\(NativeVersion.current).\(appURL.path)"
        guard !UserDefaults.standard.bool(forKey: key), appHasQuarantineAttribute(appURL) else {
            return
        }

        let alert = NSAlert()
        alert.messageText = "macOS may require approval for this build"
        alert.informativeText = "Codex Quota Dock is not notarized with an Apple Developer account. If macOS blocks this app, approve it in Privacy & Security, or build it locally from source."
        alert.addButton(withTitle: "Open Privacy & Security")
        alert.addButton(withTitle: "Later")
        UserDefaults.standard.set(true, forKey: key)
        if alert.runModal() == .alertFirstButtonReturn,
           let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?General") {
            NSWorkspace.shared.open(url)
        }
    }

    private func appHasQuarantineAttribute(_ appURL: URL) -> Bool {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/xattr")
        process.arguments = ["-p", "com.apple.quarantine", appURL.path]
        process.standardOutput = Pipe()
        process.standardError = Pipe()
        do {
            try process.run()
            process.waitUntilExit()
            return process.terminationStatus == 0
        } catch {
            return false
        }
    }
}
