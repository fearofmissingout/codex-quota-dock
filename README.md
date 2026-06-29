# Codex Quota Dock

Cross-platform desktop tool for monitoring multiple Codex ChatGPT auth profiles, switching the active Codex auth file, and keeping the dock app easy to install, migrate, and update.

The stable app uses [Fyne](https://fyne.io/) so one Go codebase can target Windows, macOS, and Linux. Starting in v0.5.0, the repository also includes a native macOS Swift/AppKit preview for Mac users who want a more native menu bar and floating-panel experience.

## Features

- Import the current `~/.codex/auth.json` or a selected auth JSON file without pre-filling a separate alias field.
- Create a new profile by pasting auth JSON directly.
- Store multiple local profiles, such as `company`, `team`, and `pro`.
- Export and import a full local backup for moving profiles to another machine.
- Restore the latest auth backup if a switch needs to be rolled back.
- Compact floating monitor for the current and pinned Codex accounts.
- Separate 5h and weekly quota rows with remaining percent, reset time, and low-quota coloring.
- Optional low-quota notifications with separate 5h and weekly thresholds.
- Switch the active Codex auth file with a backup, optional automatic Codex restart, and a readable result reminder.
- Start at login on Windows, macOS, and Linux.
- Check GitHub Releases for updates, download the matching asset, and install after user confirmation.
- Health diagnostics for auth/profile/startup/version state.
- App, taskbar, tray, and macOS bundle icon assets.
- Native macOS preview app built with Swift/AppKit, sharing the same local profile storage.

## Storage

Profile data is stored under the user config directory:

```text
Windows: %APPDATA%\codex-quota-dock
macOS:   ~/Library/Application Support/codex-quota-dock
Linux:   $XDG_CONFIG_HOME/codex-quota-dock or ~/.config/codex-quota-dock
```

Files:

- `profiles.json` stores profile metadata only.
- `profiles/<profile-id>/auth.json` stores each imported auth file.
- `backups/auth-YYYYMMDD-HHMMSS.json` stores active auth backups made before switching.

The app does not print token values in profile metadata, diagnostics, or normal UI text.

## Download

Download the latest build from the [GitHub Releases](https://github.com/fearofmissingout/codex-quota-dock/releases) page.

Choose the file for your platform:

- Windows: `codex-quota-dock-windows-amd64.zip`
- macOS Apple Silicon native preview: `codex-quota-dock-native-macos-arm64.zip`
- macOS Intel native preview: `codex-quota-dock-native-macos-x86_64.zip`
- macOS Apple Silicon: `codex-quota-dock-macos-arm64.zip`
- macOS Intel: `codex-quota-dock-macos-amd64.zip`
- Linux: `codex-quota-dock-linux-amd64.zip`

For macOS, prefer the native preview package if available. The older Go/Fyne macOS package remains available as a fallback while the native app reaches feature parity.

Unzip the package and run the app or executable inside. On macOS, the zip contains `Codex Quota Dock.app`. On Linux, you may need to mark the file as executable:

```sh
chmod +x ./codex-quota-dock-*
```

### macOS Gatekeeper

The macOS builds are packaged as `.app` bundles and ad-hoc signed, but they are not Apple-notarized because this project does not currently use a paid Apple Developer ID certificate. If macOS says Apple cannot verify the app:

1. Open `System Settings`.
2. Go to `Privacy & Security`.
3. Find the blocked `Codex Quota Dock.app` message.
4. Click `Open Anyway`, then confirm `Open`.

Choose the package that matches your Mac:

- M-series / Apple Silicon Mac: `codex-quota-dock-macos-arm64.zip`
- Intel Mac: `codex-quota-dock-macos-amd64.zip`
- Native preview on M-series / Apple Silicon Mac: `codex-quota-dock-native-macos-arm64.zip`
- Native preview on Intel Mac: `codex-quota-dock-native-macos-x86_64.zip`

Check your Mac architecture with:

```sh
uname -m
```

`arm64` means Apple Silicon. `x86_64` means Intel.

## 操作手册

### 首次启动

1. 下载对应平台的 release 包并解压。
2. 启动 `Codex Quota Dock`。
3. 首次没有 profile 时，设置窗口会自动打开；也可以在悬浮窗点击 `Config` 手动打开。
4. 点击 `Import Current` 导入当前 Codex 正在使用的 auth。
5. 确认或修改自动生成的 alias。
6. 如果还有其他账号，点击 `Import File` 导入保存好的 auth JSON，或点击 `New Profile` 粘贴 auth JSON 新建。
7. 需要常驻显示的账号可以选中后点击 `Pin`。

### 导入账号

`Import Current` 会读取当前 Codex auth 路径：

1. 如果设置了 `CODEX_HOME`，使用 `CODEX_HOME/auth.json`。
2. 否则使用当前用户 home 下的 `~/.codex/auth.json`。

导入时不再要求先填写 alias。工具会根据账号后缀自动生成默认 alias，例如 `current-567890`。如果这个账号已经存在，会提示更新现有 profile 或作为副本导入。

### 切换账号

1. 在悬浮窗或设置窗口选择目标 profile。
2. 点击 `Switch` 或 `Switch Selected`。
3. 确认切换。
4. 工具会先备份当前 active auth，再替换为目标 profile 的 auth。
5. 如果启用了自动重启，工具会关闭正在运行的 Codex 窗口并重新打开 Codex。

确认弹窗会提示可能关闭 Codex 窗口，未保存输入可能丢失。切换结果弹窗会显示备份路径和重启状态。

### 备份迁移

设置窗口提供：

- `Export Backup`：导出所有 profiles、alias、pinned 状态、auth JSON 和基础设置。
- `Import Backup`：在另一台机器上一键导入备份，遇到相同账号会更新现有 profile。
- `Restore Backup`：把最近一次 switch 前保存的 active auth 备份恢复回 Codex 当前 auth 路径。

备份文件包含完整 auth 凭据，相当于账号登录凭证。不要提交到 GitHub，不要发给别人，不要放进公开网盘。

### 更新和自启

`Updates` 页可以：

- 查看当前版本。
- 点击 `Check Updates` 检查 GitHub latest release。
- 有新版本时点击 `Download and Install`，工具会下载对应平台 zip，退出后替换当前程序并重启。
- 勾选 `Check for updates on startup`，启动时每天最多自动检查一次，但不会自动安装。

设置窗口里的 `Start at login` 会把当前程序加入当前用户的开机自启：

- Windows: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
- macOS: `~/Library/LaunchAgents`
- Linux: `~/.config/autostart`

更新后会继续使用当前安装路径，开机自启设置会保持指向当前程序。

### 用量分析和健康检查

`Local Usage` 页展示本机 Codex session 日志中的 token 用量。它是本机使用分析，不等同于 ChatGPT 网站账号 quota。

`Health` 页展示：

- 当前 auth 是否存在并可解析。
- 保存的 profile 数量。
- 开机自启是否启用。
- 当前程序版本。

诊断内容会脱敏账号 ID，不会显示 auth token。

## Safety Notes

- The app stores auth files locally on your machine.
- Exported backup files contain full auth credentials.
- Do not commit auth files, backups, generated app config folders, or `.codex` contents to Git.
- Switching creates a backup before replacing the active Codex auth.
- Existing Codex windows need to restart after switching auth. The app can do this automatically after confirmation, or you can restart Codex manually.

## Build

Fyne desktop builds require CGO and a C compiler.

Prerequisites:

- Windows: install MinGW-w64, TDM-GCC, MSYS2, or another GCC toolchain and make sure `gcc` is in `PATH`.
- macOS: install Xcode Command Line Tools.
- Linux: install `gcc` and the desktop/OpenGL development packages required by Fyne for your distribution.

Windows:

```powershell
.\scripts\build.cmd
```

PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

macOS/Linux:

```sh
./scripts/build.sh
```

The build scripts generate icon assets, run tests, compile stripped release-style binaries, and on Windows generate a `.syso` icon resource for the taskbar executable icon.

Without a C compiler, `CGO_ENABLED=0 go test ./...` can still verify the non-GUI/core path through the no-CGO fallback. Building the real desktop UI requires CGO.

### Native macOS Preview

The Swift/AppKit preview lives in `native/macos/CodexQuotaDock` and requires macOS with Xcode Command Line Tools:

```sh
cd native/macos/CodexQuotaDock
swift test
swift build
```

Package a native `.app` zip:

```sh
VERSION=0.5.0 ./scripts/package-macos-native.sh arm64
VERSION=0.5.0 ./scripts/package-macos-native.sh x86_64
```

The native app uses the same local profile directory: `~/Library/Application Support/codex-quota-dock`.

## Release Artifacts

The GitHub Actions workflow `Build desktop artifacts` builds downloadable artifacts for:

- Windows amd64: `codex-quota-dock-windows-amd64.exe`
- macOS amd64: `Codex Quota Dock.app`
- macOS arm64: `Codex Quota Dock.app`
- Linux amd64: `codex-quota-dock-linux-amd64`
- Native macOS arm64 preview: `Codex Quota Dock.app`
- Native macOS x86_64 preview: `Codex Quota Dock.app`

Windows and Linux are compiled on native hosted runners. macOS artifacts are built on the macOS 14 hosted runner, packaged as `.app` bundles, given an `.icns` app icon, and ad-hoc signed with `codesign --sign -`. Native macOS preview artifacts are built with Swift Package Manager on macOS 14 runners.
