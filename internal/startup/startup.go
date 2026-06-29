package startup

import (
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

const (
	DefaultAppName = "Codex Quota Dock"
	DefaultAppID   = "io.github.fearofmissingout.codex-quota-dock"
)

type Manager struct {
	AppName string
	AppID   string
}

func NewManager() Manager {
	return Manager{AppName: DefaultAppName, AppID: DefaultAppID}
}

func (m Manager) appName() string {
	if strings.TrimSpace(m.AppName) == "" {
		return DefaultAppName
	}
	return strings.TrimSpace(m.AppName)
}

func (m Manager) appID() string {
	if strings.TrimSpace(m.AppID) == "" {
		return DefaultAppID
	}
	return strings.TrimSpace(m.AppID)
}

func (m Manager) executable() (string, error) {
	path, err := os.Executable()
	if err != nil {
		return "", fmt.Errorf("resolve executable: %w", err)
	}
	return filepath.Abs(path)
}

func WindowsRunCommand(executable string) string {
	executable = strings.TrimSpace(executable)
	if strings.HasPrefix(executable, `"`) && strings.HasSuffix(executable, `"`) {
		return executable
	}
	return `"` + strings.ReplaceAll(executable, `"`, `\"`) + `"`
}

func LaunchAgentPlist(label, executable string) string {
	return fmt.Sprintf(`<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>%s</string>
	<key>ProgramArguments</key>
	<array>
		<string>%s</string>
	</array>
	<key>RunAtLoad</key>
	<true/>
</dict>
</plist>
`, xmlEscape(label), xmlEscape(executable))
}

func LinuxDesktopEntry(name, executable string) string {
	var out bytes.Buffer
	out.WriteString("[Desktop Entry]\n")
	out.WriteString("Type=Application\n")
	out.WriteString("Version=1.0\n")
	out.WriteString("Name=" + desktopEscape(name) + "\n")
	out.WriteString("Comment=Monitor Codex quota usage\n")
	out.WriteString("Exec=" + desktopEscape(executable) + "\n")
	out.WriteString("Terminal=false\n")
	out.WriteString("X-GNOME-Autostart-enabled=true\n")
	return out.String()
}

func xmlEscape(value string) string {
	var out bytes.Buffer
	for _, r := range value {
		switch r {
		case '&':
			out.WriteString("&amp;")
		case '<':
			out.WriteString("&lt;")
		case '>':
			out.WriteString("&gt;")
		case '"':
			out.WriteString("&quot;")
		case '\'':
			out.WriteString("&apos;")
		default:
			out.WriteRune(r)
		}
	}
	return out.String()
}

func desktopEscape(value string) string {
	return strings.ReplaceAll(strings.TrimSpace(value), "\n", " ")
}
