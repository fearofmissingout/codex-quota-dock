package main

import (
	"flag"
	"fmt"
	"os"

	"github.com/fearofmissingout/codex-quota-dock/internal/macosapp"
)

func main() {
	var opts macosapp.Options
	flag.StringVar(&opts.BinaryPath, "binary", "", "Path to the built macOS executable")
	flag.StringVar(&opts.AppPath, "app", "", "Output .app bundle path")
	flag.StringVar(&opts.AppName, "name", "Codex Quota Dock", "macOS app display name")
	flag.StringVar(&opts.ExecutableName, "executable", "codex-quota-dock", "Executable name inside Contents/MacOS")
	flag.StringVar(&opts.BundleID, "bundle-id", "io.github.fearofmissingout.codex-quota-dock", "macOS bundle identifier")
	flag.StringVar(&opts.Version, "version", "0.3.1", "CFBundleShortVersionString")
	flag.StringVar(&opts.Build, "build", "dev", "CFBundleVersion")
	flag.Parse()

	if err := macosapp.Package(opts); err != nil {
		fmt.Fprintf(os.Stderr, "package macOS app: %v\n", err)
		os.Exit(1)
	}
}
