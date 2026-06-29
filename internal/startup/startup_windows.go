//go:build windows

package startup

import (
	"fmt"

	"golang.org/x/sys/windows/registry"
)

const runKeyPath = `Software\Microsoft\Windows\CurrentVersion\Run`

func (m Manager) IsEnabled() (bool, error) {
	executable, err := m.executable()
	if err != nil {
		return false, err
	}
	key, err := registry.OpenKey(registry.CURRENT_USER, runKeyPath, registry.QUERY_VALUE)
	if err != nil {
		if err == registry.ErrNotExist {
			return false, nil
		}
		return false, fmt.Errorf("open startup registry: %w", err)
	}
	defer key.Close()
	value, _, err := key.GetStringValue(m.appName())
	if err != nil {
		if err == registry.ErrNotExist {
			return false, nil
		}
		return false, fmt.Errorf("read startup registry: %w", err)
	}
	return value == WindowsRunCommand(executable), nil
}

func (m Manager) SetEnabled(enabled bool) error {
	key, _, err := registry.CreateKey(registry.CURRENT_USER, runKeyPath, registry.SET_VALUE|registry.QUERY_VALUE)
	if err != nil {
		return fmt.Errorf("open startup registry: %w", err)
	}
	defer key.Close()
	if !enabled {
		if err := key.DeleteValue(m.appName()); err != nil && err != registry.ErrNotExist {
			return fmt.Errorf("delete startup registry value: %w", err)
		}
		return nil
	}
	executable, err := m.executable()
	if err != nil {
		return err
	}
	if err := key.SetStringValue(m.appName(), WindowsRunCommand(executable)); err != nil {
		return fmt.Errorf("write startup registry value: %w", err)
	}
	return nil
}
