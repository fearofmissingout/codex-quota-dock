package macosapp

import (
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func TestPackageCreatesMacOSAppBundle(t *testing.T) {
	root := t.TempDir()
	binary := filepath.Join(root, "codex-quota-dock")
	if err := os.WriteFile(binary, []byte("fake binary"), 0644); err != nil {
		t.Fatalf("write binary: %v", err)
	}
	icon := filepath.Join(root, "AppIcon.icns")
	if err := os.WriteFile(icon, []byte("fake icon"), 0644); err != nil {
		t.Fatalf("write icon: %v", err)
	}
	appPath := filepath.Join(root, "Codex Quota Dock.app")

	if err := Package(Options{
		BinaryPath:     binary,
		AppPath:        appPath,
		AppName:        "Codex Quota Dock",
		ExecutableName: "codex-quota-dock",
		BundleID:       "io.github.fearofmissingout.codex-quota-dock",
		Version:        "0.3.1",
		Build:          "test-build",
		IconPath:       icon,
	}); err != nil {
		t.Fatalf("Package returned error: %v", err)
	}

	copiedBinary := filepath.Join(appPath, "Contents", "MacOS", "codex-quota-dock")
	data, err := os.ReadFile(copiedBinary)
	if err != nil {
		t.Fatalf("read app binary: %v", err)
	}
	if string(data) != "fake binary" {
		t.Fatalf("app binary=%q want copied binary", data)
	}
	if runtime.GOOS != "windows" {
		info, err := os.Stat(copiedBinary)
		if err != nil {
			t.Fatalf("stat app binary: %v", err)
		}
		if info.Mode()&0111 == 0 {
			t.Fatalf("app binary mode=%v want executable bits", info.Mode())
		}
	}

	plist := readText(t, filepath.Join(appPath, "Contents", "Info.plist"))
	for _, want := range []string{
		"<key>CFBundleIdentifier</key>",
		"<string>io.github.fearofmissingout.codex-quota-dock</string>",
		"<key>CFBundleExecutable</key>",
		"<string>codex-quota-dock</string>",
		"<key>CFBundleShortVersionString</key>",
		"<string>0.3.1</string>",
		"<key>CFBundleIconFile</key>",
		"<string>AppIcon.icns</string>",
	} {
		if !strings.Contains(plist, want) {
			t.Fatalf("Info.plist missing %q:\n%s", want, plist)
		}
	}

	if got := readText(t, filepath.Join(appPath, "Contents", "Resources", "AppIcon.icns")); got != "fake icon" {
		t.Fatalf("copied icon=%q want fake icon", got)
	}

	if got := readText(t, filepath.Join(appPath, "Contents", "PkgInfo")); got != "APPL????" {
		t.Fatalf("PkgInfo=%q want APPL????", got)
	}
}

func TestPackageRejectsMissingBinary(t *testing.T) {
	root := t.TempDir()

	err := Package(Options{
		BinaryPath:     filepath.Join(root, "missing"),
		AppPath:        filepath.Join(root, "Codex Quota Dock.app"),
		AppName:        "Codex Quota Dock",
		ExecutableName: "codex-quota-dock",
		BundleID:       "io.github.fearofmissingout.codex-quota-dock",
		Version:        "0.3.1",
		Build:          "test-build",
	})

	if err == nil {
		t.Fatal("Package returned nil error for missing binary")
	}
}

func readText(t *testing.T, path string) string {
	t.Helper()
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read %s: %v", path, err)
	}
	return string(data)
}
