//go:build linux || darwin

package updater

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
)

func LaunchInstaller(stageDir, cacheRoot string) error {
	candidate, err := FindInstallCandidate(stageDir, runtime.GOOS)
	if err != nil {
		return err
	}
	current, err := os.Executable()
	if err != nil {
		return fmt.Errorf("resolve current executable: %w", err)
	}
	scriptPath := filepath.Join(cacheRoot, "updates", "install-update.sh")
	if err := os.MkdirAll(filepath.Dir(scriptPath), 0755); err != nil {
		return err
	}
	script := fmt.Sprintf(`#!/bin/sh
set -eu
pid_to_wait=%d
source_path=%q
target_path=%q
while kill -0 "$pid_to_wait" 2>/dev/null; do
  sleep 0.2
done
cp "$source_path" "$target_path"
chmod +x "$target_path"
"$target_path" >/dev/null 2>&1 &
`, os.Getpid(), candidate, current)
	if err := os.WriteFile(scriptPath, []byte(script), 0755); err != nil {
		return fmt.Errorf("write installer script: %w", err)
	}
	cmd := exec.Command("/bin/sh", scriptPath)
	if err := cmd.Start(); err != nil {
		return fmt.Errorf("launch installer: %w", err)
	}
	return nil
}
