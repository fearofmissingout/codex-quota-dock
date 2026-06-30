import AppKit
import SwiftUI

final class SettingsWindowController: NSWindowController {
    convenience init(model: NativeAppModel, touchBarController: TouchBarController?) {
        let controller = NSHostingController(rootView: SettingsContentView(model: model))
        let window = NSWindow(contentViewController: controller)
        window.title = "Codex Quota Dock Settings"
        window.setContentSize(NSSize(width: 980, height: 640))
        window.minSize = NSSize(width: 960, height: 640)
        window.styleMask.insert([.titled, .closable, .miniaturizable, .resizable])
        window.touchBar = touchBarController?.makeTouchBar()
        self.init(window: window)
    }

    func show() {
        window?.center()
        window?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
}
