//go:build windows

package codexapp

import (
	"strconv"
	"strings"
	"testing"
)

func TestWindowsTaskkillArgsDoNotKillProcessTree(t *testing.T) {
	args := windowsTaskkillArgs(1234)

	for _, arg := range args {
		if arg == "/T" {
			t.Fatalf("taskkill args include /T and would kill the launcher process tree: %v", args)
		}
	}
	if len(args) < 3 || args[0] != "/PID" || args[1] != strconv.Itoa(1234) {
		t.Fatalf("taskkill args=%v want /PID 1234 ...", args)
	}
}

func TestWindowsAppIDFromPackagedCodexPath(t *testing.T) {
	path := `C:\Program Files\WindowsApps\OpenAI.Codex_26.616.6631.0_x64__2p2nqsd0c76g0\app\Codex.exe`

	got := windowsAppIDFromPath(path)

	if got != "OpenAI.Codex_2p2nqsd0c76g0!App" {
		t.Fatalf("appID=%q", got)
	}
}

func TestPackagedCodexActivationScriptWaitsRetriesAndVerifies(t *testing.T) {
	script := packagedCodexActivationScript("OpenAI.Codex_2p2nqsd0c76g0!App")

	for _, want := range []string{
		"Wait-ForCodexPackageExit",
		"Start-Process -FilePath 'explorer.exe'",
		"shell:AppsFolder\\",
		"Test-CodexDesktopRoot",
		"throw 'Codex did not reopen after activation attempts.'",
	} {
		if !strings.Contains(script, want) {
			t.Fatalf("activation script missing %q:\n%s", want, script)
		}
	}
}
