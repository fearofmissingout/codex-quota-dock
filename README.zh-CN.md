# Codex Quota Dock

语言：[English](README.md) | 简体中文

Codex Quota Dock 是一个原生桌面监控工具，适合同时使用多个 Codex
ChatGPT auth profile 的用户。它可以监控 5 小时和每周额度，管理本地账号
配置，并在切换当前 Codex `auth.json` 前自动创建备份。

当前维护的版本是原生桌面应用：

- Windows 11：C++ / Win32
- macOS 13+：Swift / AppKit / SwiftUI，universal 包同时支持 Intel 和 Apple Silicon

旧的 Go/Fyne 应用和 Linux 构建仍保留在仓库历史里，但不再作为当前发布目标。

## 功能

- 监控多个已保存 auth profile，并分别展示 5h / weekly quota。
- 在简洁的悬浮监控窗里显示当前账号和 pinned 账号。
- 导入当前 Codex auth 文件，或导入指定 auth JSON 文件。
- 支持通过粘贴 auth JSON 新建和编辑 profile。
- 切换 `~/.codex/auth.json` 或 `$CODEX_HOME/auth.json`，切换前先备份。
- 切换 auth 后可自动重启 Codex，并支持配置 Codex 启动目标。
- 支持自动切换规则，包括用于 P0 账号的额度优先模式。
- 支持导出和导入本地 profile 备份，方便迁移到其他机器。
- 支持恢复最近一次切换前的 active auth 备份。
- 从 Codex session 日志 / SQLite 状态里分析本地用量。
- 提供 active auth、profile、开机自启、版本和 Codex 日志健康检查。
- 检查 GitHub Releases 更新。
- 支持 Windows 和 macOS 开机自启。

## 下载

从 [GitHub Releases](https://github.com/fearofmissingout/codex-quota-dock/releases)
下载最新版本。

按平台选择文件：

| 平台 | 文件 |
| --- | --- |
| Windows 11 | `codex-quota-dock-native-windows-amd64.zip` |
| macOS 13+ | `codex-quota-dock-native-macos-universal.zip` |

Windows 10 和 Linux 不是当前维护的发布目标。

## 安装

### Windows 11

1. 下载 `codex-quota-dock-native-windows-amd64.zip`。
2. 解压 zip。
3. 运行 `codex-quota-dock-native.exe`。
4. 打开 `Config`，导入当前 Codex 正在使用的 auth profile。

如果 Windows SmartScreen 首次启动时提醒，请确认文件来源可信后再选择运行。

### macOS

1. 下载 `codex-quota-dock-native-macos-universal.zip`。
2. 解压 zip。
3. 可选：把 `Codex Quota Dock.app` 移动到 `Applications`。
4. 打开应用。

macOS 应用使用 ad-hoc 签名，但没有 Apple notarization。项目目前没有使用付费
Apple Developer ID 证书。如果 macOS 提示无法验证应用：

1. 打开 `System Settings`。
2. 进入 `Privacy & Security`。
3. 找到被拦截的 `Codex Quota Dock.app` 提示。
4. 点击 `Open Anyway`，再确认 `Open`。

## 快速开始

1. 启动 `Codex Quota Dock`。
2. 打开 `Config`。
3. 点击 `Import Current`，导入 Codex 当前正在使用的 auth 文件。
4. 用 `Import File` 或 `New Profile` 添加其他账号。
5. 设置容易识别的 alias。
6. 将重要 profile 设为 pinned，让它们显示在悬浮监控窗里。
7. 点击 `Refresh` 拉取 quota。
8. 选中 profile 后点击 `Switch`，让 Codex 使用该账号。

切换 auth 后，Codex 需要重新加载登录状态。你可以让工具尝试自动关闭并重启
Codex，也可以手动重启 Codex。

更简洁的中文操作手册见
[docs/user-guide.zh-CN.md](docs/user-guide.zh-CN.md)。

## Quota 展示

Quota 会分成两个独立窗口展示：

- `5h`：短周期滚动用量窗口
- `weekly`：每周用量窗口

当工具需要判断“这个账号还能不能继续用”时，会把剩余额度更低的窗口当作实际
瓶颈。低额度颜色和提醒阈值可以分别为 5h 和 weekly 配置。

Quota 刷新不会高频轮询。你可以手动刷新，也可以把轮询间隔设置为 1、5 或
10 分钟。

## 自动切换

自动切换是可选功能。当当前 profile 额度较低，且另一个保存的 profile 更健康
时，工具可以提示或执行切换。

支持的模式：

- `Off`：不自动切换。
- `Notify`：只显示建议。
- `When Codex Closed`：仅在 Codex 未运行时切换。
- `When Idle`：Codex 已关闭，或系统空闲足够久时切换。

重要设置：

- `Switch away`：当前 profile 低于或等于该额度时视为需要切走。
- `Switch to`：候选 profile 必须高于或等于该额度。
- `Idle min`：`When Idle` 所需的空闲分钟数。
- `Cooldown min`：两次自动切换之间的最小间隔。
- `Restart Codex automatically after switching`：切换后尝试关闭并重新打开 Codex。
- `Codex launch target`：自动探测或手动配置 Codex 应用路径 / 启动目标。

### 额度优先模式

额度优先模式适合你想优先消耗某个账号的情况，例如公司提供、有用量要求的账号。

profile priority 数字越小优先级越高：

- `P0` 是最高优先级。
- `P5` 比 `P0` 优先级低。

开启额度优先模式后，工具会在满足 idle 和 cooldown 规则时，切回已经恢复额度的
最高优先级账号。一个常见配置是：

- 公司账号：`P0`
- 个人兜底账号：`P5`
- Priority 5h recovery threshold：`99%`
- Priority weekly recovery threshold：`0%`

这样配置后，如果 P0 账号的 5h quota 恢复，工具会在安全时切回 P0，而不是继续
消耗低优先级的兜底账号。

## 本地用量分析

`Usage` tab 会分析这台机器上的本地 Codex 活动。它和 ChatGPT quota 是两回事，
不要把它当作账单数据。

原生应用会在可用时读取本地 Codex session 文件和 SQLite 状态，并汇总：

- 今天
- 最近 7 天
- 最近 30 天
- overall token usage
- 每日用量图表数据
- 解析或访问问题

扫描器设计为低频 UI 刷新使用，不会持续高频 tail 日志。

## 数据和隐私

Codex Quota Dock 是 local-first 工具，但因为切换账号必须替换 Codex 的 active
auth 文件，所以它会在本地保存 auth 文件。

profile 数据存储在当前用户的应用配置目录：

```text
Windows: %APPDATA%\codex-quota-dock
macOS:   ~/Library/Application Support/codex-quota-dock
```

重要文件：

```text
profiles.json
profiles/<profile-id>/auth.json
backups/auth-YYYYMMDD-HHMMSS.json
settings.json
```

active Codex auth 的解析规则：

```text
if CODEX_HOME is set: $CODEX_HOME/auth.json
otherwise:            ~/.codex/auth.json
```

网络访问：

- Quota 刷新会用选中的 auth token 请求 ChatGPT usage endpoint。
- 更新检查会请求 GitHub Releases。

安全提醒：

- 导出的备份包含完整 auth 凭据。
- 不要把 auth 文件、导出的备份、应用配置目录或 `.codex` 内容提交到 Git。
- 工具不会有意在 profile metadata、诊断信息或普通 UI 文案里打印 token 值。
- 只在你拥有或被授权使用的账号上使用本工具。

## 项目结构

```text
native/windows/CodexQuotaDock   Windows 11 原生应用
native/macos/CodexQuotaDock     macOS 原生应用
assets/icon                     共享图标源文件
scripts                         构建和打包脚本
docs                            操作手册、release notes、交接文档
cmd, internal                   旧 Go/Fyne 实现和共享历史
```

## 从源码构建

### Windows 11 原生版

要求：

- Windows 11
- Visual Studio 2022 Build Tools
- Windows SDK
- CMake 3.24+

构建和测试：

```powershell
.\scripts\build-windows-native.ps1 -Configuration Release -Arch x64
```

输出：

```text
dist/codex-quota-dock-native-windows-amd64.zip
```

### macOS 原生版

要求：

- macOS 13+
- Xcode Command Line Tools
- Swift 5.10+

测试和构建：

```sh
cd native/macos/CodexQuotaDock
swift test
swift build
```

打包 universal `.app` zip：

```sh
# from the repository root
VERSION=0.8.0 sh ./scripts/package-macos-native.sh universal
```

输出：

```text
dist/codex-quota-dock-native-macos-universal.zip
```

## 开发流程

日常开发从 `dev` 开始：

```sh
git fetch origin
git checkout dev
git pull --ff-only origin dev
git checkout -b codex/<short-topic>
```

发布流程：

1. 将测试过的改动合入 `dev`。
2. 准备发布时将 `dev` 合入 `main`。
3. 更新版本常量和 release notes。
4. 打 `vX.Y.Z` tag。
5. 让 GitHub Actions 发布原生 Windows 和 macOS 产物。

`.github/workflows/build.yml` 会构建：

- macOS runner 上的 native macOS universal package
- Windows runner 上的 native Windows amd64 package
- 推送 `v*` tag 时发布 GitHub Release assets

更详细的原生开发交接说明见
[docs/native-dev-handoff.zh-CN.md](docs/native-dev-handoff.zh-CN.md)。

## Release Notes

- [v0.8.0](docs/v0.8.0-release-notes.md)
- [v0.7.0](docs/v0.7.0-release-notes.md)
- [older notes](docs)

## 非官方项目

这是一个独立的本地 Codex auth/profile 管理工具，不是 OpenAI 官方产品。
