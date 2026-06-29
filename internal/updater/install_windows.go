//go:build windows

package updater

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
)

func LaunchInstaller(stageDir, cacheRoot string) error {
	candidate, err := FindInstallCandidate(stageDir, "windows")
	if err != nil {
		return err
	}
	current, err := os.Executable()
	if err != nil {
		return fmt.Errorf("resolve current executable: %w", err)
	}
	scriptPath := filepath.Join(cacheRoot, "updates", "install-update.ps1")
	if err := os.MkdirAll(filepath.Dir(scriptPath), 0755); err != nil {
		return err
	}
	script := fmt.Sprintf(`$ErrorActionPreference = "Stop"
$pidToWait = %d
$source = %q
$target = %q
try { Wait-Process -Id $pidToWait -Timeout 30 } catch {}
Start-Sleep -Milliseconds 350
Copy-Item -LiteralPath $source -Destination $target -Force
Start-Process -FilePath $target
`, os.Getpid(), candidate, current)
	if err := os.WriteFile(scriptPath, []byte(script), 0600); err != nil {
		return fmt.Errorf("write installer script: %w", err)
	}
	cmd := exec.Command("powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-WindowStyle", "Hidden", "-File", scriptPath)
	if err := cmd.Start(); err != nil {
		return fmt.Errorf("launch installer: %w", err)
	}
	return nil
}
