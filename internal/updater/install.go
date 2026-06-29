package updater

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

func FindInstallCandidate(stageDir, goos string) (string, error) {
	var matches []string
	err := filepath.WalkDir(stageDir, func(path string, entry os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if entry.IsDir() {
			return nil
		}
		name := strings.ToLower(entry.Name())
		switch goos {
		case "windows":
			if name == "codex-quota-dock-windows-amd64.exe" || name == "codex-quota-dock.exe" {
				matches = append(matches, path)
			}
		case "darwin":
			if name == "codex-quota-dock" && strings.Contains(filepath.ToSlash(path), "Codex Quota Dock.app/Contents/MacOS/") {
				matches = append(matches, path)
			}
		default:
			if name == "codex-quota-dock-linux-amd64" || name == "codex-quota-dock" {
				matches = append(matches, path)
			}
		}
		return nil
	})
	if err != nil {
		return "", err
	}
	if len(matches) == 0 {
		return "", fmt.Errorf("no install candidate found in %s", stageDir)
	}
	return matches[0], nil
}
