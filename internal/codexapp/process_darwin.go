//go:build darwin

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
	out, err := exec.CommandContext(ctx, "pgrep", "-x", "Codex").Output()
	if err != nil {
		if exit, ok := err.(*exec.ExitError); ok && exit.ExitCode() == 1 {
			return nil, nil
		}
		return nil, fmt.Errorf("pgrep Codex: %w", err)
	}
	var processes []Process
	for _, line := range strings.Split(strings.TrimSpace(string(out)), "\n") {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		pid, err := strconv.Atoi(line)
		if err != nil {
			continue
		}
		executable := strings.TrimSpace(commandOutput(ctx, "ps", "-p", line, "-o", "comm="))
		processes = append(processes, Process{
			PID:        pid,
			Name:       filepath.Base(executable),
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
	if strings.TrimSpace(target.AppName) != "" {
		cmd = exec.CommandContext(ctx, "open", "-a", target.AppName)
	} else if appPath := appBundlePath(target.Executable); appPath != "" {
		cmd = exec.CommandContext(ctx, "open", appPath)
	} else if strings.TrimSpace(target.Executable) != "" {
		cmd = exec.CommandContext(ctx, target.Executable, target.Args...)
	} else {
		return ErrCodexNotFound
	}
	return cmd.Start()
}

func DefaultLaunchTarget() LaunchTarget {
	return LaunchTarget{AppName: "Codex"}
}

func commandOutput(ctx context.Context, name string, args ...string) string {
	out, err := exec.CommandContext(ctx, name, args...).Output()
	if err != nil {
		return ""
	}
	return string(out)
}

func appBundlePath(executable string) string {
	executable = strings.TrimSpace(executable)
	if executable == "" {
		return ""
	}
	marker := ".app/Contents/MacOS/"
	if index := strings.Index(executable, marker); index >= 0 {
		return executable[:index+len(".app")]
	}
	return ""
}

func errorsIsNoSuchProcess(err error) bool {
	return strings.Contains(strings.ToLower(err.Error()), "no such process")
}
