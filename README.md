# Codex Quota Dock

Cross-platform desktop tool for monitoring multiple Codex ChatGPT auth profiles and switching the active Codex auth file.

The app uses [Fyne](https://fyne.io/) so it can target Windows, macOS, and Linux from the same Go UI code.

## Features

- Import the current `~/.codex/auth.json` or another saved auth JSON file.
- Store multiple local profiles, such as `company` and `pro`.
- Compact desktop window for the current and pinned Codex accounts.
- Borderless draggable monitor on Windows, macOS, and Linux/X11. Linux Wayland or unsupported desktops fall back to a normal title bar so the window remains movable.
- System tray menu for showing the window, refreshing visible profiles, and quitting.
- Manually refresh the selected profile, visible profiles, or all profiles.
- Optional automatic refresh: off, 1 minute, 5 minutes, or 10 minutes.
- Display Codex quota windows such as 5h and weekly usage, remaining percent, and reset time.
- Switch the active Codex auth file with a backup and a readable restart reminder, including a copyable backup path.

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

The app does not print token values in the UI or metadata.

## Codex Auth Path

When switching profiles, the active Codex auth file is resolved in this order:

1. `CODEX_HOME/auth.json`, when `CODEX_HOME` is set.
2. `~/.codex/auth.json`, using the current user's home directory.

The app does not scan arbitrary folders for auth files. Profiles are imported explicitly from the active Codex auth file or from a file the user selects.

## Download

Download the latest build from the [GitHub Releases](https://github.com/fearofmissingout/codex-quota-dock/releases) page.

Choose the file for your platform:

- Windows: `codex-quota-dock-windows-amd64.zip`
- macOS Apple Silicon: `codex-quota-dock-macos-arm64.zip`
- Linux: `codex-quota-dock-linux-amd64.zip`

Unzip the package and run the executable inside. On macOS or Linux, you may need to mark the file as executable:

```sh
chmod +x ./codex-quota-dock-*
```

## 操作手册

### 1. 准备账号 auth

Codex Quota Dock 通过本地 auth 文件管理多个 Codex 账号。你可以准备：

- 当前正在使用的 Codex auth：`CODEX_HOME/auth.json` 或 `~/.codex/auth.json`。
- 其他账号的 auth 备份文件：例如你手动保存的团队账号、Pro 账号 auth JSON。

不要把 auth 文件、备份文件、配置目录提交到 Git。README 和示例里也不要粘贴 token、cookie、refresh token 或完整 auth 内容。

### 2. 首次启动

1. 下载对应平台的 release 包并解压。
2. 启动 `codex-quota-dock`。
3. 在悬浮窗点击 `Config` 打开配置窗口。
4. 在 `Alias` 输入一个容易识别的名字，例如 `team`、`company`、`pro`。
5. 点击 `Import Current` 导入当前 Codex auth，或点击 `Import File` 选择另一个 auth JSON 文件。
6. 为每个账号重复添加一次。
7. 需要常驻悬浮窗显示的账号，选中后点击 `Pin`。

### 3. 悬浮窗日常使用

悬浮窗用于快速查看当前账号和收藏账号的额度。

- 顶部可以选择 profile，并点击 `Switch` 一键切换当前 Codex auth。
- 点击 `Refresh` 会立即刷新当前悬浮窗里可见账号的额度。
- 点击 `Config` 打开完整配置窗口。
- 双击账号行也可以进入配置窗口。
- 每个账号会分两行展示 `5h` 和 `weekly` quota，避免 reset 时间被截断。
- quota 行背景会像电量条一样显示剩余额度比例：剩余高于 20% 偏绿色，低于 20% 偏红色。

悬浮窗默认是无边框、可拖拽的小窗。Windows、macOS、Linux/X11 会尽量使用全局拖拽；Linux Wayland 或不支持无边框拖动的桌面环境会回退到普通标题栏，保证窗口仍然可移动。

### 4. 配置窗口

配置窗口用于管理账号、刷新策略和详细信息。

- `Alias`：修改当前 profile 的显示名。
- Auth 文本框：查看或更新当前 profile 保存的 auth JSON。
- `Save Profile`：保存 alias 或 auth 内容修改。
- `Delete`：删除当前选中的 profile，以及本工具保存的 auth 副本；不会删除 Codex 正在使用的 active auth 文件。
- `Refresh Selected`：只刷新当前选中的账号。
- `Refresh Visible`：刷新当前悬浮窗显示的账号。
- `Refresh All`：刷新所有已保存账号。
- `Pin`：把账号固定显示在悬浮窗。
- 自动刷新：可选 `off`、`1 minute`、`5 minutes`、`10 minutes`。日常建议用手动刷新或 5/10 分钟，避免不必要的请求。
- 重启提醒：切换 auth 后默认提示重启 Codex，也可以在配置里关闭提醒。

### 5. 切换 Codex 账号

切换账号时，工具会把选中的 profile auth 写入当前 Codex 使用的 auth 路径。

1. 在悬浮窗或配置窗口选择目标 profile。
2. 点击 `Switch` 或 `Switch Selected`。
3. 确认切换。
4. 工具会先备份当前 active auth，再替换为目标 profile 的 auth。
5. 按提示重启 Codex 客户端，让 Codex 重新读取新 auth。

目标 auth 路径解析规则：

1. 如果设置了 `CODEX_HOME`，使用 `CODEX_HOME/auth.json`。
2. 否则使用当前用户 home 下的 `~/.codex/auth.json`。

工具不会扫描任意目录，也不会自动搜集 auth。所有 profile 都来自你主动导入的当前 auth 或文件。

### 6. 本机 Codex 用量分析

配置窗口里的 `Local Usage` 页面用于查看本机 Codex session 日志中的 token 用量。

- 统计来源是本机 `sessions` 和 `archived_sessions` 日志。
- 使用增量 token 事件统计本机用量，避免重复累计总量。
- 展示今日、近 7 天、近 30 天、总量，以及按 profile 归属的用量。
- `Daily usage` 图表展示最近 7 天每天在本机消耗了多少 token。
- `Overall token mix` 展示输入、缓存输入、输出、推理输出的大致构成。
- 账号归属依赖本工具记录的 auth switch history。Codex session 日志里通常没有稳定的账号标识，所以应用开始记录切换历史之前的用量会显示为 `Unknown / before tracking`。
- 之后通过本工具切换 profile 的记录，会用于把新的本机用量归属到对应账号。

这个页面是本机 Codex 使用量分析，不等同于 ChatGPT 网站账号总 quota。网站 quota 是账号级别，本机统计是当前机器上的 Codex token 消耗。

### 7. 常见问题

`fetch quota` 超时：
网络、ChatGPT 后端响应或本地代理都可能导致超时。可以稍后点击 `Refresh` 重试。profile 不会因为刷新失败被删除。

切换后 Codex 没有变化：
Codex 运行中的窗口通常不会自动 reload auth。请重启 Codex 客户端。

没有 GCC 导致无法本地编译：
Fyne 桌面 UI 需要 CGO 和 C 编译器。Windows 可安装 MinGW-w64、TDM-GCC 或 MSYS2；macOS 安装 Xcode Command Line Tools；Linux 安装发行版对应的 gcc 和桌面/OpenGL 依赖。

悬浮窗不能无边框拖动：
某些 Linux Wayland 桌面不支持当前拖拽方案，会自动回退到普通窗口标题栏。

## User Guide

### First Run

1. Start `codex-quota-dock`.
2. Click `Config` in the floating monitor to open the settings window.
3. In the `Alias` field, enter a short name for the account, such as `company`, `team`, or `pro`.
4. Click `Import Current` to import the current Codex auth from `CODEX_HOME/auth.json` or `~/.codex/auth.json`.
5. Repeat the same steps for every Codex account you want to monitor.

Use `Import File` when you already have another saved auth JSON file and want to add it manually. The app only imports files you explicitly select.

### Floating Monitor

The small floating window is designed for daily monitoring.

- It shows the active profile and any pinned profiles.
- Each profile shows quota information on separate lines, including `5h` and `weekly` windows when available.
- Click `Refresh` to query the visible profiles immediately.
- Select a profile and click `Switch` to make it the active Codex auth profile.
- Click `Config` to open the full settings window.
- Double-click a profile to open the settings window with that profile selected.

The monitor is borderless and draggable on Windows, macOS, and Linux/X11. On unsupported Linux desktop sessions, it falls back to a normal movable window title bar.

### Settings Window

The settings window is where you manage profiles and detailed quota data.

- Select a profile from the list to view quota details.
- Edit the alias in the `Alias` field.
- Edit the saved auth JSON in the auth text box if you need to update a profile manually.
- Click `Save Profile` after changing the alias or auth content.
- Click `Refresh Selected`, `Refresh Visible`, or `Refresh All` to update quota data.
- Click `Pin` to keep a profile visible in the floating monitor.
- Use the refresh interval selector to choose `off`, `1 minute`, `5 minutes`, or `10 minutes`.

### Switching Accounts

Switching a profile replaces the active Codex auth file with the selected saved profile.

1. Select the profile you want to use.
2. Click `Switch` in the floating monitor, or `Switch Selected` in the settings window.
3. Confirm the switch.
4. The app creates a timestamped backup of the previous active auth file.
5. Restart Codex so existing Codex windows reload the new auth file.

The restart reminder includes the backup path. You can disable this reminder from the settings window, but Codex still needs to be restarted after an auth switch.

### Refresh Behavior

Manual refresh is usually enough for normal use. If you enable automatic refresh, prefer the slowest interval that works for you.

- `off`: no background polling.
- `1 minute`: useful when you are actively comparing accounts.
- `5 minutes`: balanced for regular monitoring.
- `10 minutes`: lowest background traffic.

Quota requests use the imported auth token for each profile. If a request times out or fails, the profile remains stored and you can retry with `Refresh`.

### Safety Notes

- The app stores auth files locally on your machine.
- Do not commit auth files, backups, or app config directories to Git.
- Switching creates a backup before replacing the active Codex auth.
- The app does not reload a running Codex session. Restart Codex after switching.
- If `CODEX_HOME` is set, switching targets `CODEX_HOME/auth.json`; otherwise it targets `~/.codex/auth.json`.

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

If you prefer PowerShell, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

macOS/Linux:

```sh
./scripts/build.sh
```

Manual Windows build:

```powershell
go test ./...
$env:CGO_ENABLED = "1"
go build -o codex-quota-dock.exe ./cmd/codex-quota-dock
```

Manual macOS/Linux build:

```sh
go test ./...
CGO_ENABLED=1 go build -o codex-quota-dock ./cmd/codex-quota-dock
```

Without a C compiler, `go test ./...` can still verify the non-GUI/core path through the no-CGO fallback. Building the real desktop UI requires CGO.

## Release Artifacts

The GitHub Actions workflow `Build desktop artifacts` builds downloadable artifacts for:

- Windows amd64: `codex-quota-dock-windows-amd64.exe`
- macOS amd64: `codex-quota-dock-macos-amd64`
- macOS arm64: `codex-quota-dock-macos-arm64`
- Linux amd64: `codex-quota-dock-linux-amd64`

Each platform is compiled on its native runner so the CGO desktop dependencies match the target OS. Local generated folders such as `dist/` and `fyne-cross/` are ignored by Git.
