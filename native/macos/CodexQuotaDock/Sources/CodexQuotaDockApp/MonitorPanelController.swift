import AppKit
import CodexQuotaDockCore
import SwiftUI

final class MonitorPanelController: NSWindowController {
    convenience init(model: NativeAppModel, touchBarController: TouchBarController?) {
        let panel = NSPanel(
            contentRect: NSRect(x: 0, y: 0, width: 380, height: 320),
            styleMask: [.borderless, .nonactivatingPanel],
            backing: .buffered,
            defer: false
        )
        panel.isMovableByWindowBackground = true
        panel.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
        panel.backgroundColor = .clear
        panel.isOpaque = false
        panel.hasShadow = true
        panel.contentView = NSHostingView(rootView: MonitorContentView(model: model))
        panel.touchBar = touchBarController?.makeTouchBar()
        self.init(window: panel)
        applySettings(model.settings)
    }

    func applySettings(_ settings: AppSettings) {
        window?.level = settings.monitorAlwaysOnTop ? .floating : .normal
    }

    func showNearStatusItem(_ item: NSStatusItem) {
        if let button = item.button, let window {
            let buttonRect = button.convert(button.bounds, to: nil)
            if let screenRect = button.window?.convertToScreen(buttonRect) {
                let origin = NSPoint(x: screenRect.midX - window.frame.width / 2, y: screenRect.minY - window.frame.height - 8)
                window.setFrameOrigin(origin)
            } else {
                window.center()
            }
            window.makeKeyAndOrderFront(nil)
        } else {
            window?.center()
            window?.makeKeyAndOrderFront(nil)
        }
    }
}
