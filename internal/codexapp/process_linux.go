//go:build linux

package codexapp

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
)

func ListProcesses(ctx context.Context) ([]Process, error) {
	entries, err := os.ReadDir("/proc")
	if err != nil {
		return nil, fmt.Errorf("read /proc: %w", err)
	}
	var processes []Process
	for _, entry := range entries {
		if !entry.IsDir() {
			continue
		}
		pid, err := strconv.Atoi(entry.Name())
		if err != nil {
			continue
		}
		select {
		case <-ctx.Done():
			return nil, ctx.Err()
		default:
		}
		procDir := filepath.Join("/proc", entry.Name())
		name := strings.TrimSpace(readText(filepath.Join(procDir, "comm")))
		executable, _ := os.Readlink(filepath.Join(procDir, "exe"))
		if name == "" && executable == "" {
			continue
		}
		processes = append(processes, Process{
			PID:        pid,
			Name:       name,
			Executable: executable,
		})
	}
	return processes, nil
}

func StopProcess(ctx context.Context, proc Process) error {
	process, err := os.FindProcess(proc.PID)
	if err != nil {
		return err
	}
	if err := process.Signal(syscall.SIGTERM); err != nil && !errorsIsNoSuchProcess(err) {
		return err
	}
	return nil
}

func StartCodex(ctx context.Context, target LaunchTarget) error {
	var cmd *exec.Cmd
	if strings.TrimSpace(target.Executable) != "" {
		cmd = exec.CommandContext(ctx, target.Executable, target.Args...)
	} else if strings.TrimSpace(target.AppName) != "" {
		cmd = exec.CommandContext(ctx, target.AppName)
	} else {
		return ErrCodexNotFound
	}
	return cmd.Start()
}

func DefaultLaunchTarget() LaunchTarget {
	return LaunchTarget{}
}

func readText(path string) string {
	data, err := os.ReadFile(path)
	if err != nil {
		return ""
	}
	return string(data)
}

func errorsIsNoSuchProcess(err error) bool {
	return strings.Contains(strings.ToLower(err.Error()), "no such process")
}
