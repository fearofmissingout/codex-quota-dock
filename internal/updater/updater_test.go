package updater_test

import (
	"context"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"testing"

	"github.com/fearofmissingout/codex-quota-dock/internal/updater"
)

func TestCompareVersions(t *testing.T) {
	cases := []struct {
		current string
		latest  string
		want    bool
	}{
		{"v0.3.2", "v0.4.0", true},
		{"0.4.0", "v0.4.0", false},
		{"v0.4.1", "v0.4.0", false},
		{"v0.4.0-dev", "v0.4.0", true},
	}
	for _, tc := range cases {
		if got := updater.IsNewer(tc.current, tc.latest); got != tc.want {
			t.Fatalf("IsNewer(%q,%q)=%v want %v", tc.current, tc.latest, got, tc.want)
		}
	}
}

func TestFindInstallCandidateForWindowsArtifact(t *testing.T) {
	root := t.TempDir()
	path := filepath.Join(root, "codex-quota-dock-windows-amd64.exe")
	if err := os.WriteFile(path, []byte("exe"), 0755); err != nil {
		t.Fatalf("write candidate: %v", err)
	}

	got, err := updater.FindInstallCandidate(root, "windows")
	if err != nil {
		t.Fatalf("FindInstallCandidate returned error: %v", err)
	}
	if got != path {
		t.Fatalf("candidate=%q want %q", got, path)
	}
}

func TestSelectAssetForPlatform(t *testing.T) {
	release := updater.Release{
		TagName: "v0.4.0",
		Assets: []updater.Asset{
			{Name: "codex-quota-dock-windows-amd64.zip", BrowserDownloadURL: "https://example.com/windows.zip"},
			{Name: "codex-quota-dock-linux-amd64.zip", BrowserDownloadURL: "https://example.com/linux.zip"},
			{Name: "codex-quota-dock-macos-amd64.zip", BrowserDownloadURL: "https://example.com/macos-intel.zip"},
			{Name: "codex-quota-dock-macos-arm64.zip", BrowserDownloadURL: "https://example.com/macos-arm.zip"},
		},
	}

	asset, ok := updater.SelectAsset(release, "darwin", "arm64")
	if !ok {
		t.Fatal("SelectAsset returned false")
	}
	if asset.Name != "codex-quota-dock-macos-arm64.zip" {
		t.Fatalf("asset=%+v want macos arm64", asset)
	}
}

func TestCheckFindsAvailableUpdate(t *testing.T) {
	release := updater.Release{
		TagName: "v0.4.0",
		Assets:  []updater.Asset{{Name: "codex-quota-dock-windows-amd64.zip", BrowserDownloadURL: "https://example.com/windows.zip"}},
	}

	check := updater.Check("v0.3.2", release, "windows", "amd64")

	if !check.Available {
		t.Fatalf("check=%+v want update available", check)
	}
	if check.Asset.Name != "codex-quota-dock-windows-amd64.zip" {
		t.Fatalf("asset=%+v want windows asset", check.Asset)
	}
}

func TestClientFetchLatestRelease(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/repos/fearofmissingout/codex-quota-dock/releases/latest" {
			t.Fatalf("path=%s", r.URL.Path)
		}
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(`{
		  "tag_name": "v0.4.0",
		  "name": "v0.4.0",
		  "body": "notes",
		  "assets": [
		    {"name":"codex-quota-dock-linux-amd64.zip","browser_download_url":"https://example.com/linux.zip","size":123}
		  ]
		}`))
	}))
	defer server.Close()

	client := updater.Client{HTTPClient: server.Client(), BaseURL: server.URL}
	release, err := client.FetchLatest(context.Background(), "fearofmissingout/codex-quota-dock")
	if err != nil {
		t.Fatalf("FetchLatest returned error: %v", err)
	}
	if release.TagName != "v0.4.0" || len(release.Assets) != 1 || release.Assets[0].Size != 123 {
		t.Fatalf("release=%+v", release)
	}
}
