package codexapp_test

import (
	"context"
	"errors"
	"reflect"
	"testing"

	"github.com/fearofmissingout/codex-quota-dock/internal/codexapp"
)

func TestRestartStopsCodexAndStartsCapturedExecutable(t *testing.T) {
	processes := []codexapp.Process{
		{PID: 10, Name: "Codex.exe", Executable: `C:\Users\me\AppData\Local\Programs\Codex\Codex.exe`},
	}
	var stopped []int
	var started codexapp.LaunchTarget
	restarter := codexapp.Restarter{
		CurrentPID: 99,
		List: func(context.Context) ([]codexapp.Process, error) {
			return processes, nil
		},
		Stop: func(_ context.Context, proc codexapp.Process) error {
			stopped = append(stopped, proc.PID)
			return nil
		},
		Start: func(_ context.Context, target codexapp.LaunchTarget) error {
			started = target
			return nil
		},
	}

	result, err := restarter.Restart(context.Background())

	if err != nil {
		t.Fatalf("Restart returned error: %v", err)
	}
	if !reflect.DeepEqual(stopped, []int{10}) {
		t.Fatalf("stopped=%v want [10]", stopped)
	}
	if started.Executable != processes[0].Executable {
		t.Fatalf("started=%+v want executable %q", started, processes[0].Executable)
	}
	if len(result.Stopped) != 1 || result.Stopped[0].PID != 10 {
		t.Fatalf("result=%+v want stopped process", result)
	}
}

func TestRestartSkipsCurrentProcessAndQuotaDock(t *testing.T) {
	processes := []codexapp.Process{
		{PID: 12, Name: "Codex Quota Dock.exe", Executable: `C:\tools\codex-quota-dock.exe`},
		{PID: 13, Name: "codex-quota-dock-windows-amd64.exe", Executable: `C:\tools\codex-quota-dock-windows-amd64.exe`},
		{PID: 99, Name: "Codex.exe", Executable: `C:\Codex\Codex.exe`},
		{PID: 14, Name: "Codex.exe", Executable: `C:\Codex\Codex.exe`},
	}
	var stopped []int
	restarter := codexapp.Restarter{
		CurrentPID: 99,
		List: func(context.Context) ([]codexapp.Process, error) {
			return processes, nil
		},
		Stop: func(_ context.Context, proc codexapp.Process) error {
			stopped = append(stopped, proc.PID)
			return nil
		},
		Start: func(context.Context, codexapp.LaunchTarget) error {
			return nil
		},
	}

	_, err := restarter.Restart(context.Background())

	if err != nil {
		t.Fatalf("Restart returned error: %v", err)
	}
	if !reflect.DeepEqual(stopped, []int{14}) {
		t.Fatalf("stopped=%v want only the external Codex process", stopped)
	}
}

func TestRestartScopesPackagedCodexToDesktopProcessTree(t *testing.T) {
	const appID = "OpenAI.Codex_2p2nqsd0c76g0!App"
	processes := []codexapp.Process{
		{PID: 7, ParentPID: 10, Name: "Codex.exe", Executable: `C:\Program Files\WindowsApps\OpenAI.Codex_26.616.6631.0_x64__2p2nqsd0c76g0\app\Codex.exe`, CommandLine: `"Codex.exe" --type=renderer`, AppID: appID},
		{PID: 10, ParentPID: 100, Name: "Codex.exe", Executable: `C:\Program Files\WindowsApps\OpenAI.Codex_26.616.6631.0_x64__2p2nqsd0c76g0\app\Codex.exe`, CommandLine: `"Codex.exe"`, AppID: appID},
		{PID: 12, ParentPID: 10, Name: "codex.exe", Executable: `C:\Program Files\WindowsApps\OpenAI.Codex_26.616.6631.0_x64__2p2nqsd0c76g0\app\resources\codex.exe`, CommandLine: `"codex.exe" app-server`, AppID: appID},
		{PID: 20, ParentPID: 12, Name: "powershell.exe", Executable: `C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe`, CommandLine: `powershell -Command codex-quota-dock-autorestart-preview.exe`},
		{PID: 21, ParentPID: 20, Name: "codex-quota-dock-autorestart-preview.exe", Executable: `C:\tools\codex-quota-dock-autorestart-preview.exe`},
		{PID: 30, ParentPID: 200, Name: "codex.exe", Executable: `C:\Users\me\bin\codex.exe`, CommandLine: `"codex.exe" resume`},
	}
	var stopped []int
	var started codexapp.LaunchTarget
	restarter := codexapp.Restarter{
		CurrentPID: 21,
		List: func(context.Context) ([]codexapp.Process, error) {
			return processes, nil
		},
		Stop: func(_ context.Context, proc codexapp.Process) error {
			stopped = append(stopped, proc.PID)
			return nil
		},
		Start: func(_ context.Context, target codexapp.LaunchTarget) error {
			started = target
			return nil
		},
	}

	_, err := restarter.Restart(context.Background())

	if err != nil {
		t.Fatalf("Restart returned error: %v", err)
	}
	if !reflect.DeepEqual(stopped, []int{7, 10, 12}) {
		t.Fatalf("stopped=%v want only packaged Codex desktop process tree", stopped)
	}
	if started.AppID != appID {
		t.Fatalf("started=%+v want packaged app id %q", started, appID)
	}
}

func TestRestartReturnsNotFoundWithoutCodexOrFallback(t *testing.T) {
	restarter := codexapp.Restarter{
		CurrentPID: 99,
		List: func(context.Context) ([]codexapp.Process, error) {
			return []codexapp.Process{{PID: 13, Name: "codex-quota-dock.exe"}}, nil
		},
		Stop: func(context.Context, codexapp.Process) error {
			t.Fatal("Stop should not be called")
			return nil
		},
		Start: func(context.Context, codexapp.LaunchTarget) error {
			t.Fatal("Start should not be called")
			return nil
		},
	}

	_, err := restarter.Restart(context.Background())

	if !errors.Is(err, codexapp.ErrCodexNotFound) {
		t.Fatalf("err=%v want ErrCodexNotFound", err)
	}
}

func TestRestartUsesFallbackWhenCodexIsNotRunning(t *testing.T) {
	fallback := codexapp.LaunchTarget{AppName: "Codex"}
	var started codexapp.LaunchTarget
	restarter := codexapp.Restarter{
		CurrentPID: 99,
		Fallback:   fallback,
		List: func(context.Context) ([]codexapp.Process, error) {
			return nil, nil
		},
		Stop: func(context.Context, codexapp.Process) error {
			t.Fatal("Stop should not be called")
			return nil
		},
		Start: func(_ context.Context, target codexapp.LaunchTarget) error {
			started = target
			return nil
		},
	}

	result, err := restarter.Restart(context.Background())

	if err != nil {
		t.Fatalf("Restart returned error: %v", err)
	}
	if !reflect.DeepEqual(started, fallback) {
		t.Fatalf("started=%+v want fallback %+v", started, fallback)
	}
	if len(result.Stopped) != 0 {
		t.Fatalf("stopped=%+v want none", result.Stopped)
	}
}
