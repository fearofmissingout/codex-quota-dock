//go:build darwin

package startup

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

func (m Manager) IsEnabled() (bool, error) {
	data, err := os.ReadFile(m.launchAgentPath())
	if errors.Is(err, os.ErrNotExist) {
		return false, nil
	}
	if err != nil {
		return false, fmt.Errorf("read launch agent: %w", err)
	}
	executable, err := m.executable()
	if err != nil {
		return false, err
	}
	text := string(data)
	return strings.Contains(text, m.appID()) && strings.Contains(text, executable), nil
}

func (m Manager) SetEnabled(enabled bool) error {
	path := m.launchAgentPath()
	if !enabled {
		if err := os.Remove(path); err != nil && !errors.Is(err, os.ErrNotExist) {
			return fmt.Errorf("remove launch agent: %w", err)
		}
		return nil
	}
	executable, err := m.executable()
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		return fmt.Errorf("create launch agents dir: %w", err)
	}
	if err := os.WriteFile(path, []byte(LaunchAgentPlist(m.appID(), executable)), 0644); err != nil {
		return fmt.Errorf("write launch agent: %w", err)
	}
	return nil
}

func (m Manager) launchAgentPath() string {
	home, err := os.UserHomeDir()
	if err != nil {
		home = "."
	}
	return filepath.Join(home, "Library", "LaunchAgents", m.appID()+".plist")
}
