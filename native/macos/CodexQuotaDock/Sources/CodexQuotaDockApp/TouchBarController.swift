import AppKit
import CodexQuotaDockCore
import Combine

@MainActor
final class TouchBarController: NSObject, NSTouchBarDelegate {
    private enum ItemID {
        static let alias = NSTouchBarItem.Identifier("com.codexquotadock.touchbar.alias")
        static let fiveHour = NSTouchBarItem.Identifier("com.codexquotadock.touchbar.fiveHour")
        static let weekly = NSTouchBarItem.Identifier("com.codexquotadock.touchbar.weekly")
        static let refresh = NSTouchBarItem.Identifier("com.codexquotadock.touchbar.refresh")
        static let switchProfile = NSTouchBarItem.Identifier("com.codexquotadock.touchbar.switch")
    }

    private let model: NativeAppModel
    private var cancellables: Set<AnyCancellable> = []
    private let aliasLabel = NSTextField(labelWithString: "Codex Quota")
    private let fiveHourLabel = NSTextField(labelWithString: "5h --")
    private let weeklyLabel = NSTextField(labelWithString: "weekly --")

    init(model: NativeAppModel) {
        self.model = model
        super.init()
        aliasLabel.font = .systemFont(ofSize: 13, weight: .semibold)
        fiveHourLabel.font = .monospacedDigitSystemFont(ofSize: 12, weight: .medium)
        weeklyLabel.font = .monospacedDigitSystemFont(ofSize: 12, weight: .medium)
        model.$monitorRows
            .receive(on: RunLoop.main)
            .sink { [weak self] _ in self?.updateLabels() }
            .store(in: &cancellables)
        updateLabels()
    }

    func makeTouchBar() -> NSTouchBar {
        let touchBar = NSTouchBar()
        touchBar.delegate = self
        touchBar.defaultItemIdentifiers = [
            ItemID.alias,
            ItemID.fiveHour,
            ItemID.weekly,
            .flexibleSpace,
            ItemID.refresh,
            ItemID.switchProfile,
        ]
        return touchBar
    }

    func touchBar(_ touchBar: NSTouchBar, makeItemForIdentifier identifier: NSTouchBarItem.Identifier) -> NSTouchBarItem? {
        switch identifier {
        case ItemID.alias:
            return customItem(identifier: identifier, view: aliasLabel)
        case ItemID.fiveHour:
            return customItem(identifier: identifier, view: fiveHourLabel)
        case ItemID.weekly:
            return customItem(identifier: identifier, view: weeklyLabel)
        case ItemID.refresh:
            return customItem(identifier: identifier, view: NSButton(title: "Refresh", target: self, action: #selector(refresh)))
        case ItemID.switchProfile:
            return customItem(identifier: identifier, view: NSButton(title: "Switch", target: self, action: #selector(switchProfile)))
        default:
            return nil
        }
    }

    private func customItem(identifier: NSTouchBarItem.Identifier, view: NSView) -> NSTouchBarItem {
        let item = NSCustomTouchBarItem(identifier: identifier)
        item.view = view
        return item
    }

    private func updateLabels() {
        let row = model.monitorRows.first(where: { $0.isActive }) ?? model.monitorRows.first
        aliasLabel.stringValue = row?.alias ?? "Codex Quota"
        fiveHourLabel.stringValue = format(row?.fiveHour, fallback: "5h --")
        weeklyLabel.stringValue = format(row?.weekly, fallback: "weekly --")
    }

    private func format(_ window: QuotaWindow?, fallback: String) -> String {
        guard let window else { return fallback }
        guard let percent = window.remainingPercent else { return "\(window.label) --" }
        return "\(window.label) \(percent)%"
    }

    @objc private func refresh() {
        Task { await model.refreshQuotas() }
    }

    @objc private func switchProfile() {
        model.switchSelectedProfile()
    }
}
