# Codex Quota Dock 原生版开发交接

这份文档给后续在 macOS 机器上的 Codex 接手调试和开发用。目标是减少聊天上下文依赖，让每次开发都能从 GitHub 上的最新进度开始。

## 当前定位

Codex Quota Dock 是一个本地桌面工具，用来管理多个 Codex ChatGPT auth profile，监控 5h / weekly quota，并把选中的 profile 切换成当前 `~/.codex/auth.json`。

从 `v0.7.0` 开始，后续只维护两个原生版本：

- Windows 11 native: C++ / Win32，目录 `native/windows/CodexQuotaDock`
- macOS native: Swift / AppKit / SwiftUI，目录 `native/macos/CodexQuotaDock`

旧 Go / Fyne 版本和 Linux 版本不再作为发布目标。除非明确要迁移逻辑，不要继续投入旧版本。

## 分支规范

- `main`：稳定发布分支，只放已经 release 或准备立即 release 的内容。
- `dev`：日常集成分支，后续开发统一从这里开始。
- `codex/<topic>`：单个任务分支，从 `dev` 拉出，完成后合并回 `dev`。

每次开发开始前先同步：

```sh
git fetch origin
git checkout dev
git pull --ff-only origin dev
git checkout -b codex/<short-topic>
```

如果已经在任务分支上继续开发：

```sh
git fetch origin
git checkout dev
git pull --ff-only origin dev
git checkout codex/<short-topic>
git merge --ff-only dev
```

如果不能 fast-forward，先停下来查看差异，不要强推或重置用户改动。

## 发布规范

发布时从 `dev` 合并到 `main`，打 tag，并只上传原生产物：

- `codex-quota-dock-native-windows-amd64.zip`
- `codex-quota-dock-native-macos-universal.zip`

版本号需要同时更新：

- `native/macos/CodexQuotaDock/Sources/CodexQuotaDockCore/NativeVersion.swift`
- `native/windows/CodexQuotaDock/src/core.cpp`
- `native/windows/CodexQuotaDock/src/win_app.cpp`
- `.github/workflows/build.yml`
- `scripts/package-macos-native.sh`
- 对应的 `docs/vX.Y.Z-release-notes.md`

macOS 包使用 universal 产物，同时支持 Apple Silicon 和 Intel Mac：

```sh
VERSION=0.7.0 sh ./scripts/package-macos-native.sh universal
```

GitHub Actions workflow 是 `.github/workflows/build.yml`。它应该只构建 native macOS universal 和 native Windows amd64。

## 本地数据与安全

不要提交任何真实 auth、profile、backup、`.codex` 内容。

运行时数据位置：

```text
Windows: %APPDATA%\codex-quota-dock
macOS:   ~/Library/Application Support/codex-quota-dock
Codex:   ~/.codex/auth.json
```

本项目明文存储 auth JSON，设计上追求简单和可迁移，不引入系统钥匙串或复杂加密组件。导出文件包含完整 auth，必须提醒用户自行保管。

## macOS 代码地图

- `AppDelegate.swift`：菜单栏入口、status item、monitor/settings/touch bar 初始化。
- `MonitorPanelController.swift`：悬浮监控窗，当前使用 `NSPanel`、`.borderless`、`.nonactivatingPanel`、`.floating`。
- `MonitorContentView.swift`：悬浮窗内容，显示 profile、quota、refresh/switch/config。
- `SettingsContentView.swift`：配置窗口 UI，包含 Auth / Quota / Usage / Settings / Health / Updates tabs。
- `NativeAppModel.swift`：macOS UI 状态、profile 操作、quota 刷新、auto switch、usage 扫描。
- `TouchBarController.swift`：Touch Bar quota 展示和 Refresh/Switch action。
- `ProfileStore.swift`：profile metadata 和每个 profile 的 auth JSON 存储。
- `AuthSwitcher.swift`：替换 `~/.codex/auth.json` 并创建 backup。
- `QuotaClient.swift`：请求 `https://chatgpt.com/backend-api/wham/usage`。
- `LocalUsageScanner.swift`：扫描本机 Codex session JSONL 的 token usage。

Windows 原生版是当前功能参考，尤其是配置导入导出和更新检查：

- `native/windows/CodexQuotaDock/src/core.cpp`
- `native/windows/CodexQuotaDock/src/win_app.cpp`

## 当前 macOS 已知差距

这些是从 `v0.7.0` 后需要优先补齐的点。

1. 配置导入/导出/恢复缺失

macOS Settings 现在有 `Import Current`、`Import File`、`New Profile`、`Delete`、`Pin`、`Switch`、`Save Profile`，但还没有 Windows 版的 `Export`、`Import`、`Restore`。

建议做法：

- 在 macOS core 里实现和 Windows 相同的 backup JSON schema。
- 导出内容包含 profiles、settings、version、exported_at。
- 导入时按 account_id 更新已有 profile，否则创建新 profile。
- 恢复 latest backup 时只恢复当前 active `~/.codex/auth.json`。
- Settings 左侧按钮布局和 Windows 对齐，降低跨平台差异。

2. 悬浮监控窗一直置顶

`MonitorPanelController.swift` 当前设置了：

```swift
panel.level = .floating
panel.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
```

这会让监控窗盖在其他 app 上，影响正常使用。

建议做法：

- 增加设置项 `monitorAlwaysOnTop`，默认 `false`。
- 默认使用 `.normal` 或更温和的层级，仅点击菜单栏图标时显示。
- 如果用户开启置顶，再设置 `.floating`。
- 可以增加 `Pin monitor` 语义，未 pin 时点击其他 app 后自动隐藏。
- 保留可拖拽和毛玻璃视觉，但不要让默认行为打断用户工作流。

3. Touch Bar 没有显示内容

当前 `AppDelegate.swift` 里使用：

```swift
NSApp.touchBar = touchBarController.makeTouchBar()
```

但 app 是 accessory app，主 monitor 又是 non-activating panel。Touch Bar 通常需要 app 激活且有可响应窗口，所以内容可能不会出现。

建议做法：

- 在有 Touch Bar 的 Intel Mac 上实机验证，不要只靠 CI。
- 尝试把 touch bar 绑定到 Settings window 或对应 `NSHostingView`，而不是只挂到 `NSApp.touchBar`。
- 打开 settings 时激活 app：`NSApp.activate(ignoringOtherApps: true)`。
- Touch Bar 内容先保持简单：active alias、5h、weekly、Refresh、Switch。
- 没有物理 Touch Bar 的 Mac 不应显示错误状态。

4. 自动更新未补齐

macOS `Updates` tab 现在只是占位文案，没有真正检查 GitHub release 或安装更新。Windows 有 `checkForUpdates` 参考实现。

建议做法：

- 先做最小正确版本：检查 GitHub latest release，匹配 `codex-quota-dock-native-macos-universal.zip`。
- 如果发现新版本，显示版本号、asset 名称、大小，并提供打开 GitHub Releases 的按钮。
- 自动下载安装和替换 `.app` 可以后置，因为 macOS Gatekeeper、quarantine、自替换退出流程更复杂。
- 不要引入 Sparkle 等新依赖，除非手动更新体验已经稳定且确实需要。

## macOS 下个版本建议顺序

推荐先做这些，不要一次把架构拉太大：

1. 补齐 macOS 配置导入/导出/恢复，并和 Windows backup JSON 互通。
2. 优化 monitor 默认非置顶行为，加 `Always on top` 或 `Pin monitor` 设置。
3. 在 Touch Bar 真机上调通显示，把 touch bar 挂到可激活窗口。
4. 实现 macOS update check：检查 release、匹配 universal asset、打开下载页。
5. 最后再做 UI 细节 polish 和 release notes。

## macOS 验证命令

在 macOS 上运行：

```sh
cd native/macos/CodexQuotaDock
swift test
swift build
```

打包：

```sh
cd ../../..
VERSION=0.7.0 sh ./scripts/package-macos-native.sh universal
```

手工验证清单：

- 首次启动不会创建或提交真实 auth。
- `Import Current` 能读取 `~/.codex/auth.json`。
- `Import File` 能导入一个 auth JSON。
- profile alias / auth JSON 可编辑并保存。
- `Switch` 会备份当前 active auth，并提示重启或自动重启 Codex。
- monitor 默认不会一直盖住其他 app。
- 开启置顶后才保持 above other apps。
- Touch Bar 在支持的机器上显示 active profile 和 quota。
- Updates tab 能检查当前 release，并找到 macOS universal asset。

## 与 Windows 的功能对齐规则

Windows 原生版目前是功能参照，但 macOS 不需要照搬 Win32 UI 代码。对齐的是用户能力和术语：

- profile 操作按钮名称一致。
- backup export/import/restore 文件格式一致。
- quota 显示口径一致：5h 和 weekly 分开展示，低额度按各自阈值提醒。
- switch 行为一致：先备份，再替换 active auth，再提示或重启 Codex。
- release asset 命名一致，避免 update checker 找错文件。

如果 Windows 和 macOS 某个功能行为不同，需要在 release notes 里说明原因。

## 交接给 macOS Codex 的首条提示

可以直接把下面这段发给 macOS 机器上的 Codex：

```text
请先阅读 docs/native-dev-handoff.zh-CN.md。先 git fetch origin && git checkout dev && git pull --ff-only origin dev，然后从 dev 新建 codex/macos-parity-fixes 分支。优先处理 macOS 配置导入导出恢复、悬浮窗默认非置顶、Touch Bar 不显示、Updates tab 未接 GitHub release 检查这四个问题。不要提交任何 auth.json、profiles、backups 或 .codex 内容。
```
