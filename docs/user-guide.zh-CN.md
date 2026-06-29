# Codex Quota Dock 操作手册

## 首次启动

1. 从 GitHub Releases 下载对应平台的压缩包并解压。
2. 启动 `Codex Quota Dock`。
3. 打开配置窗口，点击 `Import Current` 导入当前 Codex 使用的 auth。
4. 检查自动生成的 alias，需要时可以修改。
5. 其他账号可以通过 `Import File` 导入 auth JSON，或用 `New Profile` 粘贴 auth JSON 新建。
6. 需要长期显示在悬浮窗里的账号，选中后点击 `Pin`。

## Auth 路径

`Import Current` 和切换功能会读取当前用户的 Codex auth 路径：

- 如果设置了 `CODEX_HOME`，使用 `CODEX_HOME/auth.json`。
- 否则使用 `~/.codex/auth.json`。

## 切换账号

1. 在悬浮窗或配置窗口选择目标 profile。
2. 点击 `Switch` 或 `Switch Selected`。
3. 工具会先备份当前 active auth，再替换成目标 profile 的 auth。
4. 如果开启了自动重启，工具会尝试关闭并重新打开 Codex。
5. 切换结果会显示 active auth 路径、备份路径和重启结果。

切换 auth 后，Codex 需要重新加载登录状态。自动重启失败时，请手动重启 Codex。

## 备份迁移

配置窗口提供：

- `Export Backup`：导出 profiles、alias、pinned 状态、auth JSON 和基础设置。
- `Import Backup`：在另一台机器上一键导入备份。
- `Restore Backup`：恢复最近一次 switch 前保存的 active auth。

备份文件包含完整 auth 凭据，不要提交到 GitHub，也不要发给别人。

## 更新和开机自启

- `Check Updates` 会检查 GitHub 最新 release。
- `Download and Install` 会下载匹配当前平台的包，退出后替换当前程序并重启。
- `Check for updates on startup` 每天最多自动检查一次，不会自动安装。
- `Start at login` 会把当前程序加入当前用户的开机自启。

## 用量和健康检查

- `Local Usage` 展示本机 Codex session 日志里的 token 用量，不等同于 ChatGPT 网站 quota。
- `Health` 展示 active auth、profile 数量、开机自启和版本状态。
- 诊断信息会脱敏账号 ID，不会显示 auth token。
