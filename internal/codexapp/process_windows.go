//go:build windows

package codexapp

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
)

func ListProcesses(ctx context.Context) ([]Process, error) {
	const script = `Get-CimInstance Win32_Process -Filter "Name LIKE '%Codex%'" | Select-Object ProcessId,ParentProcessId,Name,ExecutablePath,CommandLine | ConvertTo-Json -Compress`
	cmd := hiddenCommandContext(ctx, "powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script)
	var stderr bytes.Buffer
	cmd.Stderr = &stderr
	out, err := cmd.Output()
	if err != nil {
		return nil, fmt.Errorf("query processes: %w: %s", err, strings.TrimSpace(stderr.String()))
	}
	return parseWindowsProcesses(out)
}

func StopProcess(ctx context.Context, proc Process) error {
	cmd := hiddenCommandContext(ctx, "taskkill.exe", windowsTaskkillArgs(proc.PID)...)
	out, err := cmd.CombinedOutput()
	if err == nil {
		return nil
	}
	if exists, existsErr := windowsProcessExists(ctx, proc.PID); existsErr == nil && !exists {
		return nil
	}
	return fmt.Errorf("taskkill: %w: %s", err, strings.TrimSpace(string(out)))
}

func StartCodex(ctx context.Context, target LaunchTarget) error {
	var cmd *exec.Cmd
	if strings.TrimSpace(target.AppID) != "" {
		return startPackagedCodexApp(ctx, target.AppID)
	} else if strings.TrimSpace(target.Executable) != "" {
		cmd = hiddenCommandContext(ctx, target.Executable, target.Args...)
	} else if strings.TrimSpace(target.AppName) != "" {
		if appID := findWindowsCodexAppID(ctx); appID != "" {
			return startPackagedCodexApp(ctx, appID)
		} else {
			cmd = hiddenCommandContext(ctx, "powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", "Start-Process -FilePath "+quotePowerShell(target.AppName))
		}
	} else {
		return ErrCodexNotFound
	}
	return cmd.Start()
}

func startPackagedCodexApp(ctx context.Context, appID string) error {
	cmd := hiddenCommandContext(ctx, "powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", packagedCodexActivationScript(appID))
	out, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("activate packaged Codex app: %w: %s", err, strings.TrimSpace(string(out)))
	}
	return nil
}

func DefaultLaunchTarget() LaunchTarget {
	if appID := findWindowsCodexAppID(context.Background()); appID != "" {
		return LaunchTarget{AppName: "Codex", AppID: appID}
	}
	for _, candidate := range commonWindowsCodexPaths() {
		if info, err := os.Stat(candidate); err == nil && !info.IsDir() {
			return LaunchTarget{Executable: candidate, AppName: "Codex"}
		}
	}
	return LaunchTarget{AppName: "Codex"}
}

type windowsProcessJSON struct {
	ProcessID       int    `json:"ProcessId"`
	ParentProcessID int    `json:"ParentProcessId"`
	Name            string `json:"Name"`
	ExecutablePath  string `json:"ExecutablePath"`
	CommandLine     string `json:"CommandLine"`
}

func parseWindowsProcesses(out []byte) ([]Process, error) {
	text := strings.TrimSpace(string(out))
	if text == "" || text == "null" {
		return nil, nil
	}
	var many []windowsProcessJSON
	if err := json.Unmarshal([]byte(text), &many); err == nil {
		return windowsProcessesFromJSON(many), nil
	}
	var one windowsProcessJSON
	if err := json.Unmarshal([]byte(text), &one); err != nil {
		return nil, fmt.Errorf("parse process json: %w", err)
	}
	return windowsProcessesFromJSON([]windowsProcessJSON{one}), nil
}

func windowsProcessesFromJSON(items []windowsProcessJSON) []Process {
	processes := make([]Process, 0, len(items))
	for _, item := range items {
		processes = append(processes, Process{
			PID:         item.ProcessID,
			ParentPID:   item.ParentProcessID,
			Name:        item.Name,
			Executable:  item.ExecutablePath,
			CommandLine: item.CommandLine,
			AppID:       windowsAppIDFromPath(item.ExecutablePath),
		})
	}
	return processes
}

func windowsTaskkillArgs(pid int) []string {
	return []string{"/PID", strconv.Itoa(pid), "/F"}
}

func commonWindowsCodexPaths() []string {
	var paths []string
	if local := os.Getenv("LOCALAPPDATA"); local != "" {
		paths = append(paths,
			filepath.Join(local, "Programs", "Codex", "Codex.exe"),
			filepath.Join(local, "Codex", "Codex.exe"),
		)
	}
	if programFiles := os.Getenv("ProgramFiles"); programFiles != "" {
		paths = append(paths, filepath.Join(programFiles, "Codex", "Codex.exe"))
	}
	if programFilesX86 := os.Getenv("ProgramFiles(x86)"); programFilesX86 != "" {
		paths = append(paths, filepath.Join(programFilesX86, "Codex", "Codex.exe"))
	}
	return paths
}

func windowsProcessExists(ctx context.Context, pid int) (bool, error) {
	script := fmt.Sprintf(`if (Get-Process -Id %d -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }`, pid)
	cmd := hiddenCommandContext(ctx, "powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script)
	err := cmd.Run()
	if err == nil {
		return true, nil
	}
	if exit, ok := err.(*exec.ExitError); ok && exit.ExitCode() == 1 {
		return false, nil
	}
	return false, err
}

func findWindowsCodexAppID(ctx context.Context) string {
	const script = `(Get-StartApps | Where-Object { $_.Name -eq 'Codex' -or $_.AppID -like '*Codex*' } | Select-Object -First 1 -ExpandProperty AppID)`
	cmd := hiddenCommandContext(ctx, "powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script)
	out, err := cmd.Output()
	if err != nil {
		return ""
	}
	return strings.TrimSpace(string(out))
}

func packagedCodexActivationScript(appID string) string {
	return fmt.Sprintf(`
$ErrorActionPreference = 'Stop'
$appId = %s

function Get-CodexPackageProcess {
  Get-CimInstance Win32_Process |
    Where-Object {
      $_.Name -like '*Codex*' -and
      $_.ExecutablePath -like '*\WindowsApps\OpenAI.Codex_*'
    }
}

function Test-CodexDesktopRoot {
  $root = Get-CodexPackageProcess |
    Where-Object {
      $_.Name -eq 'Codex.exe' -and
      $_.ExecutablePath -like '*\WindowsApps\OpenAI.Codex_*' -and
      $_.ExecutablePath -notlike '*\resources\*' -and
      $_.CommandLine -notmatch ' --type='
    } |
    Select-Object -First 1
  return $null -ne $root
}

function Wait-ForCodexPackageExit {
  for ($i = 0; $i -lt 20; $i++) {
    if (-not (Get-CodexPackageProcess | Select-Object -First 1)) {
      return
    }
    Start-Sleep -Milliseconds 500
  }
}

Wait-ForCodexPackageExit
for ($attempt = 0; $attempt -lt 5; $attempt++) {
  Start-Process -FilePath 'explorer.exe' -ArgumentList ('shell:AppsFolder\' + $appId)
  Start-Sleep -Seconds 2
  if (Test-CodexDesktopRoot) {
    exit 0
  }
}
throw 'Codex did not reopen after activation attempts.'
`, quotePowerShell(appID))
}

func windowsAppIDFromPath(executablePath string) string {
	parts := strings.Split(filepath.Clean(executablePath), string(filepath.Separator))
	for _, part := range parts {
		if !strings.Contains(part, "__") {
			continue
		}
		beforePublisher, publisher, ok := strings.Cut(part, "__")
		if !ok || publisher == "" {
			continue
		}
		packageName, _, ok := strings.Cut(beforePublisher, "_")
		if !ok || packageName == "" {
			continue
		}
		if !strings.EqualFold(packageName, "OpenAI.Codex") {
			continue
		}
		return packageName + "_" + publisher + "!App"
	}
	return ""
}

func hiddenCommandContext(ctx context.Context, name string, args ...string) *exec.Cmd {
	cmd := exec.CommandContext(ctx, name, args...)
	cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
	return cmd
}

func quotePowerShell(value string) string {
	return "'" + strings.ReplaceAll(value, "'", "''") + "'"
}
