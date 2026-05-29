package netproxy

import (
	"net/http"
	"testing"
)

func TestParseWindowsProxyServerUsesSingleProxyForHTTPS(t *testing.T) {
	got, ok := parseWindowsProxyServer("http://127.0.0.1:1080", "https")
	if !ok {
		t.Fatal("parseWindowsProxyServer returned ok=false")
	}
	if got.String() != "http://127.0.0.1:1080" {
		t.Fatalf("proxy=%q", got.String())
	}
}

func TestParseWindowsProxyServerAddsHTTPForBareHostPort(t *testing.T) {
	got, ok := parseWindowsProxyServer("127.0.0.1:1080", "https")
	if !ok {
		t.Fatal("parseWindowsProxyServer returned ok=false")
	}
	if got.String() != "http://127.0.0.1:1080" {
		t.Fatalf("proxy=%q", got.String())
	}
}

func TestParseWindowsProxyServerSelectsSchemeSpecificProxy(t *testing.T) {
	got, ok := parseWindowsProxyServer("http=127.0.0.1:8080;https=127.0.0.1:1080", "https")
	if !ok {
		t.Fatal("parseWindowsProxyServer returned ok=false")
	}
	if got.String() != "http://127.0.0.1:1080" {
		t.Fatalf("proxy=%q", got.String())
	}
}

func TestShouldBypassProxyForLoopbackHosts(t *testing.T) {
	for _, host := range []string{"localhost", "127.0.0.1", "::1", "[::1]"} {
		if !shouldBypassProxy(host) {
			t.Fatalf("shouldBypassProxy(%q)=false", host)
		}
	}
}

func TestWindowsProxyFromSettingsRespectsDisabledProxy(t *testing.T) {
	req, err := http.NewRequest(http.MethodGet, "https://chatgpt.com/backend-api/wham/usage", nil)
	if err != nil {
		t.Fatal(err)
	}
	got, err := proxyFromWindowsSettings(req, false, "127.0.0.1:1080")
	if err != nil {
		t.Fatalf("proxyFromWindowsSettings returned error: %v", err)
	}
	if got != nil {
		t.Fatalf("proxy=%v want nil", got)
	}
}

func TestWindowsProxyFromSettingsBypassesLocalhost(t *testing.T) {
	req, err := http.NewRequest(http.MethodGet, "http://127.0.0.1:12345/wham/usage", nil)
	if err != nil {
		t.Fatal(err)
	}
	got, err := proxyFromWindowsSettings(req, true, "127.0.0.1:1080")
	if err != nil {
		t.Fatalf("proxyFromWindowsSettings returned error: %v", err)
	}
	if got != nil {
		t.Fatalf("proxy=%v want nil", got)
	}
}

func TestWindowsProxyFromSettingsReturnsProxy(t *testing.T) {
	req, err := http.NewRequest(http.MethodGet, "https://chatgpt.com/backend-api/wham/usage", nil)
	if err != nil {
		t.Fatal(err)
	}
	got, err := proxyFromWindowsSettings(req, true, "127.0.0.1:1080")
	if err != nil {
		t.Fatalf("proxyFromWindowsSettings returned error: %v", err)
	}
	if got == nil || got.String() != "http://127.0.0.1:1080" {
		t.Fatalf("proxy=%v", got)
	}
}
