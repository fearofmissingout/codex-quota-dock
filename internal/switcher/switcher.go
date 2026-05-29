package switcher

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"time"
)

type Switcher struct {
	activeAuthPath string
	backupDir      string
}

type Result struct {
	ActiveAuthPath string
	BackupPath     string
}

func New(activeAuthPath, backupDir string) Switcher {
	return Switcher{activeAuthPath: activeAuthPath, backupDir: backupDir}
}

func DefaultCodexAuthPath() (string, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return "", fmt.Errorf("resolve home dir: %w", err)
	}
	return filepath.Join(home, ".codex", "auth.json"), nil
}

func (s Switcher) Switch(profileAuthPath string) (Result, error) {
	if err := os.MkdirAll(filepath.Dir(s.activeAuthPath), 0o700); err != nil {
		return Result{}, fmt.Errorf("create active auth dir: %w", err)
	}
	if err := os.MkdirAll(s.backupDir, 0o700); err != nil {
		return Result{}, fmt.Errorf("create backup dir: %w", err)
	}

	backupPath := filepath.Join(s.backupDir, "auth-"+time.Now().Format("20060102-150405")+".json")
	if _, err := os.Stat(s.activeAuthPath); err == nil {
		if err := copyFile(s.activeAuthPath, backupPath, 0o600); err != nil {
			return Result{}, fmt.Errorf("backup current auth: %w", err)
		}
	} else if err != nil && !os.IsNotExist(err) {
		return Result{}, fmt.Errorf("stat active auth: %w", err)
	}

	tmpPath := s.activeAuthPath + ".tmp"
	if err := copyFile(profileAuthPath, tmpPath, 0o600); err != nil {
		_ = os.Remove(tmpPath)
		return Result{}, fmt.Errorf("write replacement auth: %w", err)
	}
	if err := replaceFile(tmpPath, s.activeAuthPath); err != nil {
		_ = os.Remove(tmpPath)
		return Result{}, fmt.Errorf("replace active auth: %w", err)
	}

	return Result{ActiveAuthPath: s.activeAuthPath, BackupPath: backupPath}, nil
}

func replaceFile(source, dest string) error {
	if err := os.Rename(source, dest); err == nil {
		return nil
	}
	if err := os.Remove(dest); err != nil && !os.IsNotExist(err) {
		return err
	}
	return os.Rename(source, dest)
}

func copyFile(source, dest string, mode os.FileMode) error {
	in, err := os.Open(source)
	if err != nil {
		return err
	}
	defer in.Close()

	out, err := os.OpenFile(dest, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, mode)
	if err != nil {
		return err
	}
	if _, err := io.Copy(out, in); err != nil {
		_ = out.Close()
		return err
	}
	return out.Close()
}
