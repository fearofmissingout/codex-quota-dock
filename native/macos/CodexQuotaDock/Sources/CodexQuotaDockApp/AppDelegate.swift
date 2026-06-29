import AppKit
import CodexQuotaDockCore

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var statusItem: NSStatusItem!
    private var monitor: MonitorPanelController!
    private var settings: SettingsWindowController!
    private var model: NativeAppModel!

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)

        model = NativeAppModel(paths: AppPaths())
        model.openSettingsHandler = { [weak self] in self?.openSettings() }
        monitor = MonitorPanelController(model: model)
        settings = SettingsWindowController(model: model)

        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        statusItem.button?.image = NSImage(systemSymbolName: "gauge.with.dots.needle.33percent", accessibilityDescription: "Codex Quota Dock")
        statusItem.button?.action = #selector(toggleMonitor)
        statusItem.button?.target = self
        statusItem.menu = makeMenu()

        model.reload()
        model.startAutoRefresh()
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
}
