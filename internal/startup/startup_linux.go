//go:build linux

package startup

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

func (m Manager) IsEnabled() (bool, error) {
	data, err := os.ReadFile(m.desktopEntryPath())
	if errors.Is(err, os.ErrNotExist) {
		return false, nil
	}
	if err != nil {
		return false, fmt.Errorf("read autostart desktop entry: %w", err)
	}
	executable, err := m.executable()
	if err != nil {
		return false, err
	}
	return strings.Contains(string(data), "Exec="+executable), nil
}

func (m Manager) SetEnabled(enabled bool) error {
	path := m.desktopEntryPath()
	if !enabled {
		if err := os.Remove(path); err != nil && !errors.Is(err, os.ErrNotExist) {
			return fmt.Errorf("remove autostart desktop entry: %w", err)
		}
		return nil
	}
	executable, err := m.executable()
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		return fmt.Errorf("create autostart dir: %w", err)
	}
	if err := os.WriteFile(path, []byte(LinuxDesktopEntry(m.appName(), executable)), 0644); err != nil {
		return fmt.Errorf("write autostart desktop entry: %w", err)
	}
	return nil
}

func (m Manager) desktopEntryPath() string {
	base := os.Getenv("XDG_CONFIG_HOME")
	if base == "" {
		home, err := os.UserHomeDir()
		if err != nil {
			home = "."
		}
		base = filepath.Join(home, ".config")
	}
	return filepath.Join(base, "autostart", "codex-quota-dock.desktop")
}
