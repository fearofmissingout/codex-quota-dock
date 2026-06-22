package codexapp

import (
	"context"
	"errors"
	"fmt"
	"os"
	"path"
	"sort"
	"strings"
	"time"
)

var ErrCodexNotFound = errors.New("codex app was not found")

type Process struct {
	PID         int
	ParentPID   int
	Name        string
	Executable  string
	CommandLine string
	AppID       string
}

type LaunchTarget struct {
	Executable string
	Args       []string
	AppName    string
	AppID      string
}

type Result struct {
	Stopped  []Process
	Launched LaunchTarget
}

type Restarter struct {
	CurrentPID     int
	Fallback       LaunchTarget
	AfterStopDelay time.Duration
	List           func(context.Context) ([]Process, error)
	Stop           func(context.Context, Process) error
	Start          func(context.Context, LaunchTarget) error
}

func DefaultRestarter() Restarter {
	return Restarter{
		CurrentPID:     os.Getpid(),
		Fallback:       DefaultLaunchTarget(),
		AfterStopDelay: 700 * time.Millisecond,
		List:           ListProcesses,
		Stop:           StopProcess,
		Start:          StartCodex,
	}
}

func (r Restarter) Restart(ctx context.Context) (Result, error) {
	if r.List == nil {
		r.List = ListProcesses
	}
	if r.Stop == nil {
		r.Stop = StopProcess
	}
	if r.Start == nil {
		r.Start = StartCodex
	}

	processes, err := r.List(ctx)
	if err != nil {
		return Result{}, fmt.Errorf("list Codex processes: %w", err)
	}
	candidates := codexProcesses(processes, r.CurrentPID)
	target := launchTargetFor(candidates, r.Fallback)
	if target.empty() {
		return Result{}, ErrCodexNotFound
	}

	result := Result{Stopped: make([]Process, 0, len(candidates)), Launched: target}
	var stopErrors []error
	for _, proc := range candidates {
		if err := r.Stop(ctx, proc); err != nil {
			stopErrors = append(stopErrors, fmt.Errorf("stop pid %d: %w", proc.PID, err))
			continue
		}
		result.Stopped = append(result.Stopped, proc)
	}
	if len(stopErrors) > 0 {
		return result, errors.Join(stopErrors...)
	}
	if len(result.Stopped) > 0 && r.AfterStopDelay > 0 {
		timer := time.NewTimer(r.AfterStopDelay)
		select {
		case <-ctx.Done():
			timer.Stop()
			return result, ctx.Err()
		case <-timer.C:
		}
	}
	if err := r.Start(ctx, target); err != nil {
		return result, fmt.Errorf("start Codex: %w", err)
	}
	return result, nil
}

func codexProcesses(processes []Process, currentPID int) []Process {
	filtered := make([]Process, 0, len(processes))
	for _, proc := range processes {
		if isCodexProcess(proc, currentPID) {
			filtered = append(filtered, proc)
		}
	}
	out := scopedPackagedCodexProcesses(filtered)
	if len(out) == 0 {
		out = filtered
	}
	sort.SliceStable(out, func(i, j int) bool {
		return out[i].PID < out[j].PID
	})
	return out
}

func scopedPackagedCodexProcesses(processes []Process) []Process {
	appIDs := map[string]bool{}
	for _, proc := range processes {
		if proc.AppID != "" {
			appIDs[proc.AppID] = true
		}
	}
	if len(appIDs) == 0 {
		return nil
	}
	for appID := range appIDs {
		scoped := packageProcessTree(processes, appID)
		if len(scoped) > 0 {
			return scoped
		}
	}
	return nil
}

func packageProcessTree(processes []Process, appID string) []Process {
	byPID := map[int]Process{}
	roots := map[int]bool{}
	for _, proc := range processes {
		if proc.AppID != appID {
			continue
		}
		byPID[proc.PID] = proc
		if isDesktopRootProcess(proc) {
			roots[proc.PID] = true
		}
	}
	if len(roots) == 0 {
		return nil
	}
	var out []Process
	for _, proc := range processes {
		if proc.AppID != appID {
			continue
		}
		if roots[proc.PID] || descendsFrom(proc, byPID, roots) {
			out = append(out, proc)
		}
	}
	return out
}

func descendsFrom(proc Process, byPID map[int]Process, roots map[int]bool) bool {
	seen := map[int]bool{}
	parentPID := proc.ParentPID
	for parentPID > 0 && !seen[parentPID] {
		if roots[parentPID] {
			return true
		}
		seen[parentPID] = true
		parent, ok := byPID[parentPID]
		if !ok {
			return false
		}
		parentPID = parent.ParentPID
	}
	return false
}

func isCodexProcess(proc Process, currentPID int) bool {
	if proc.PID <= 0 || (currentPID > 0 && proc.PID == currentPID) {
		return false
	}
	identity := strings.ToLower(proc.Name + " " + proc.Executable)
	if strings.Contains(identity, "codex-quota-dock") || strings.Contains(identity, "quota-dock") {
		return false
	}
	name := cleanProcessBase(proc.Name)
	exe := cleanProcessBase(proc.Executable)
	return name == "codex" || exe == "codex"
}

func launchTargetFor(processes []Process, fallback LaunchTarget) LaunchTarget {
	for _, proc := range processes {
		if proc.AppID != "" && isDesktopRootProcess(proc) {
			return LaunchTarget{Executable: proc.Executable, AppName: "Codex", AppID: proc.AppID}
		}
	}
	for _, proc := range processes {
		if proc.AppID != "" {
			return LaunchTarget{Executable: proc.Executable, AppName: "Codex", AppID: proc.AppID}
		}
	}
	for _, proc := range processes {
		if strings.TrimSpace(proc.Executable) == "" || !isDesktopRootProcess(proc) {
			continue
		}
		return LaunchTarget{Executable: proc.Executable, AppName: "Codex"}
	}
	for _, proc := range processes {
		if strings.TrimSpace(proc.Executable) == "" {
			continue
		}
		target := LaunchTarget{Executable: proc.Executable}
		if cleanProcessBase(proc.Name) == "codex" || cleanProcessBase(proc.Executable) == "codex" {
			target.AppName = "Codex"
		}
		return target
	}
	return fallback
}

func isDesktopRootProcess(proc Process) bool {
	executable := strings.ReplaceAll(strings.ToLower(strings.TrimSpace(proc.Executable)), `\`, "/")
	commandLine := strings.ToLower(proc.CommandLine)
	if strings.Contains(executable, "/resources/") {
		return false
	}
	if strings.Contains(commandLine, " --type=") || strings.Contains(commandLine, " app-server") {
		return false
	}
	return cleanProcessBase(proc.Name) == "codex" || cleanProcessBase(proc.Executable) == "codex"
}

func cleanProcessBase(value string) string {
	value = strings.TrimSpace(strings.Trim(value, `"`))
	if value == "" {
		return ""
	}
	value = strings.ReplaceAll(value, `\`, "/")
	base := strings.ToLower(path.Base(value))
	return strings.TrimSuffix(base, ".exe")
}

func (t LaunchTarget) empty() bool {
	return strings.TrimSpace(t.Executable) == "" && strings.TrimSpace(t.AppName) == "" && strings.TrimSpace(t.AppID) == ""
}
