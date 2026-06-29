package updater

import (
	"archive/zip"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"time"
)

const DefaultRepo = "fearofmissingout/codex-quota-dock"

type Asset struct {
	Name               string `json:"name"`
	BrowserDownloadURL string `json:"browser_download_url"`
	Size               int64  `json:"size"`
}

type Release struct {
	TagName string  `json:"tag_name"`
	Name    string  `json:"name"`
	Body    string  `json:"body"`
	Assets  []Asset `json:"assets"`
}

type CheckResult struct {
	Available bool
	Current   string
	Latest    string
	Release   Release
	Asset     Asset
	Reason    string
}

type Client struct {
	HTTPClient *http.Client
	BaseURL    string
}

func DefaultClient() Client {
	return Client{HTTPClient: &http.Client{Timeout: 20 * time.Second}, BaseURL: "https://api.github.com"}
}

func (c Client) FetchLatest(ctx context.Context, repo string) (Release, error) {
	repo = strings.Trim(repo, "/")
	if repo == "" {
		repo = DefaultRepo
	}
	baseURL := strings.TrimRight(c.BaseURL, "/")
	if baseURL == "" {
		baseURL = "https://api.github.com"
	}
	httpClient := c.HTTPClient
	if httpClient == nil {
		httpClient = http.DefaultClient
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, baseURL+"/repos/"+repo+"/releases/latest", nil)
	if err != nil {
		return Release{}, err
	}
	req.Header.Set("Accept", "application/vnd.github+json")
	req.Header.Set("User-Agent", "codex-quota-dock")
	resp, err := httpClient.Do(req)
	if err != nil {
		return Release{}, fmt.Errorf("fetch latest release: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return Release{}, fmt.Errorf("fetch latest release: %s", resp.Status)
	}
	var release Release
	if err := json.NewDecoder(resp.Body).Decode(&release); err != nil {
		return Release{}, fmt.Errorf("parse latest release: %w", err)
	}
	return release, nil
}

func Check(current string, release Release, goos, goarch string) CheckResult {
	result := CheckResult{Current: current, Latest: release.TagName, Release: release}
	if !IsNewer(current, release.TagName) {
		result.Reason = "already up to date"
		return result
	}
	asset, ok := SelectAsset(release, goos, goarch)
	if !ok {
		result.Reason = "no matching release asset"
		return result
	}
	result.Available = true
	result.Asset = asset
	return result
}

func CheckRuntime(current string, release Release) CheckResult {
	return Check(current, release, runtime.GOOS, runtime.GOARCH)
}

func SelectAsset(release Release, goos, goarch string) (Asset, bool) {
	platform := goos
	if platform == "darwin" {
		platform = "macos"
	}
	want := platform + "-" + goarch + ".zip"
	for _, asset := range release.Assets {
		if strings.Contains(strings.ToLower(asset.Name), strings.ToLower(want)) {
			return asset, true
		}
	}
	return Asset{}, false
}

func IsNewer(current, latest string) bool {
	c := parseVersion(current)
	l := parseVersion(latest)
	for i := 0; i < len(c) && i < len(l); i++ {
		if l[i] != c[i] {
			return l[i] > c[i]
		}
	}
	if strings.Contains(strings.ToLower(current), "dev") && !strings.Contains(strings.ToLower(latest), "dev") {
		return true
	}
	return false
}

var versionPartPattern = regexp.MustCompile(`\d+`)

func parseVersion(value string) [3]int {
	var out [3]int
	parts := versionPartPattern.FindAllString(value, 3)
	for i, part := range parts {
		n, _ := strconv.Atoi(part)
		out[i] = n
	}
	return out
}

func DownloadAndStage(ctx context.Context, client *http.Client, asset Asset, cacheRoot string) (string, error) {
	if strings.TrimSpace(asset.BrowserDownloadURL) == "" {
		return "", fmt.Errorf("release asset %q has no download URL", asset.Name)
	}
	if client == nil {
		client = http.DefaultClient
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, asset.BrowserDownloadURL, nil)
	if err != nil {
		return "", err
	}
	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("download update: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return "", fmt.Errorf("download update: %s", resp.Status)
	}
	stageDir := filepath.Join(cacheRoot, "updates", strings.TrimSuffix(asset.Name, ".zip"))
	if err := os.RemoveAll(stageDir); err != nil {
		return "", fmt.Errorf("clean update stage: %w", err)
	}
	if err := os.MkdirAll(stageDir, 0755); err != nil {
		return "", fmt.Errorf("create update stage: %w", err)
	}
	zipPath := filepath.Join(stageDir, asset.Name)
	out, err := os.Create(zipPath)
	if err != nil {
		return "", fmt.Errorf("create update zip: %w", err)
	}
	if _, err := io.Copy(out, resp.Body); err != nil {
		_ = out.Close()
		return "", fmt.Errorf("write update zip: %w", err)
	}
	if err := out.Close(); err != nil {
		return "", fmt.Errorf("close update zip: %w", err)
	}
	if err := unzip(zipPath, stageDir); err != nil {
		return "", err
	}
	return stageDir, nil
}

func unzip(zipPath, destDir string) error {
	reader, err := zip.OpenReader(zipPath)
	if err != nil {
		return fmt.Errorf("open update zip: %w", err)
	}
	defer reader.Close()
	for _, file := range reader.File {
		target := filepath.Join(destDir, file.Name)
		cleanDest := filepath.Clean(destDir) + string(os.PathSeparator)
		if !strings.HasPrefix(filepath.Clean(target), cleanDest) {
			return fmt.Errorf("update zip contains unsafe path: %s", file.Name)
		}
		if file.FileInfo().IsDir() {
			if err := os.MkdirAll(target, 0755); err != nil {
				return err
			}
			continue
		}
		if err := os.MkdirAll(filepath.Dir(target), 0755); err != nil {
			return err
		}
		src, err := file.Open()
		if err != nil {
			return err
		}
		dst, err := os.OpenFile(target, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, file.Mode())
		if err != nil {
			_ = src.Close()
			return err
		}
		_, copyErr := io.Copy(dst, src)
		closeErr := dst.Close()
		_ = src.Close()
		if copyErr != nil {
			return copyErr
		}
		if closeErr != nil {
			return closeErr
		}
	}
	return nil
}
