package macosapp

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
)

type Options struct {
	BinaryPath     string
	AppPath        string
	AppName        string
	ExecutableName string
	BundleID       string
	Version        string
	Build          string
	IconPath       string
}

func Package(opts Options) error {
	if err := validateOptions(opts); err != nil {
		return err
	}
	info, err := os.Stat(opts.BinaryPath)
	if err != nil {
		return fmt.Errorf("stat binary: %w", err)
	}
	if info.IsDir() {
		return fmt.Errorf("binary path is a directory: %s", opts.BinaryPath)
	}

	contents := filepath.Join(opts.AppPath, "Contents")
	macosDir := filepath.Join(contents, "MacOS")
	resourcesDir := filepath.Join(contents, "Resources")
	if err := os.RemoveAll(opts.AppPath); err != nil {
		return fmt.Errorf("remove existing app bundle: %w", err)
	}
	if err := os.MkdirAll(macosDir, 0755); err != nil {
		return fmt.Errorf("create MacOS directory: %w", err)
	}
	if err := os.MkdirAll(resourcesDir, 0755); err != nil {
		return fmt.Errorf("create Resources directory: %w", err)
	}

	targetBinary := filepath.Join(macosDir, opts.ExecutableName)
	if err := copyFile(targetBinary, opts.BinaryPath, 0755); err != nil {
		return fmt.Errorf("copy app binary: %w", err)
	}
	if err := os.WriteFile(filepath.Join(contents, "Info.plist"), []byte(infoPlist(opts)), 0644); err != nil {
		return fmt.Errorf("write Info.plist: %w", err)
	}
	if strings.TrimSpace(opts.IconPath) != "" {
		if err := copyFile(filepath.Join(resourcesDir, filepath.Base(opts.IconPath)), opts.IconPath, 0644); err != nil {
			return fmt.Errorf("copy app icon: %w", err)
		}
	}
	if err := os.WriteFile(filepath.Join(contents, "PkgInfo"), []byte("APPL????"), 0644); err != nil {
		return fmt.Errorf("write PkgInfo: %w", err)
	}
	return nil
}

func validateOptions(opts Options) error {
	missing := []string{}
	if strings.TrimSpace(opts.BinaryPath) == "" {
		missing = append(missing, "BinaryPath")
	}
	if strings.TrimSpace(opts.AppPath) == "" {
		missing = append(missing, "AppPath")
	}
	if strings.TrimSpace(opts.AppName) == "" {
		missing = append(missing, "AppName")
	}
	if strings.TrimSpace(opts.ExecutableName) == "" {
		missing = append(missing, "ExecutableName")
	}
	if strings.TrimSpace(opts.BundleID) == "" {
		missing = append(missing, "BundleID")
	}
	if strings.TrimSpace(opts.Version) == "" {
		missing = append(missing, "Version")
	}
	if strings.TrimSpace(opts.Build) == "" {
		missing = append(missing, "Build")
	}
	if len(missing) > 0 {
		return fmt.Errorf("missing required options: %s", strings.Join(missing, ", "))
	}
	return nil
}

func copyFile(dst, src string, mode os.FileMode) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()

	out, err := os.OpenFile(dst, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, mode)
	if err != nil {
		return err
	}
	if _, err := io.Copy(out, in); err != nil {
		_ = out.Close()
		return err
	}
	if err := out.Close(); err != nil {
		return err
	}
	return os.Chmod(dst, mode)
}

func infoPlist(opts Options) string {
	return fmt.Sprintf(`<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>en</string>
	<key>CFBundleDisplayName</key>
	<string>%s</string>
	<key>CFBundleExecutable</key>
	<string>%s</string>
	<key>CFBundleIdentifier</key>
	<string>%s</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
%s	<key>CFBundleName</key>
	<string>%s</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>%s</string>
	<key>CFBundleVersion</key>
	<string>%s</string>
	<key>LSApplicationCategoryType</key>
	<string>public.app-category.productivity</string>
	<key>NSHighResolutionCapable</key>
	<true/>
</dict>
</plist>
`, xmlEscape(opts.AppName), xmlEscape(opts.ExecutableName), xmlEscape(opts.BundleID), iconPlistEntry(opts.IconPath), xmlEscape(opts.AppName), xmlEscape(opts.Version), xmlEscape(opts.Build))
}

func iconPlistEntry(iconPath string) string {
	if strings.TrimSpace(iconPath) == "" {
		return ""
	}
	return fmt.Sprintf("\t<key>CFBundleIconFile</key>\n\t<string>%s</string>\n", xmlEscape(filepath.Base(iconPath)))
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
