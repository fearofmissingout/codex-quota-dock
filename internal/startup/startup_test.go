package startup_test

import (
	"strings"
	"testing"

	"github.com/fearofmissingout/codex-quota-dock/internal/startup"
)

func TestQuoteWindowsRunCommand(t *testing.T) {
	got := startup.WindowsRunCommand(`C:\Program Files\Codex Quota Dock\codex-quota-dock.exe`)
	want := `"C:\Program Files\Codex Quota Dock\codex-quota-dock.exe"`
	if got != want {
		t.Fatalf("command=%q want %q", got, want)
	}
}

func TestLaunchAgentPlistContainsExecutableAndLabel(t *testing.T) {
	got := startup.LaunchAgentPlist("io.github.fearofmissingout.codex-quota-dock", "/Applications/Codex Quota Dock.app/Contents/MacOS/codex-quota-dock")

	for _, want := range []string{
		"io.github.fearofmissingout.codex-quota-dock",
		"/Applications/Codex Quota Dock.app/Contents/MacOS/codex-quota-dock",
		"<key>RunAtLoad</key>",
		"<true/>",
	} {
		if !strings.Contains(got, want) {
			t.Fatalf("plist missing %q:\n%s", want, got)
		}
	}
}

func TestLinuxDesktopEntryContainsExecutable(t *testing.T) {
	got := startup.LinuxDesktopEntry("Codex Quota Dock", "/opt/codex-quota-dock/codex-quota-dock")

	for _, want := range []string{
		"[Desktop Entry]",
		"Type=Application",
		"Name=Codex Quota Dock",
		"Exec=/opt/codex-quota-dock/codex-quota-dock",
		"X-GNOME-Autostart-enabled=true",
	} {
		if !strings.Contains(got, want) {
			t.Fatalf("desktop entry missing %q:\n%s", want, got)
		}
	}
}
