package netproxy

import (
	"net"
	"net/http"
	"net/url"
	"strings"
)

// Proxy returns a proxy URL from Go's environment settings, then falls back to
// the Windows WinINET proxy used by many desktop proxy clients.
func Proxy(req *http.Request) (*url.URL, error) {
	if req == nil || req.URL == nil {
		return nil, nil
	}
	if shouldBypassProxy(req.URL.Hostname()) {
		return nil, nil
	}

	proxyURL, err := http.ProxyFromEnvironment(req)
	if err != nil || proxyURL != nil {
		return proxyURL, err
	}

	enabled, proxyServer := windowsProxySettings()
	return proxyFromWindowsSettings(req, enabled, proxyServer)
}

func proxyFromWindowsSettings(req *http.Request, enabled bool, proxyServer string) (*url.URL, error) {
	if req == nil || req.URL == nil || !enabled {
		return nil, nil
	}
	if shouldBypassProxy(req.URL.Hostname()) {
		return nil, nil
	}
	proxyURL, ok := parseWindowsProxyServer(proxyServer, req.URL.Scheme)
	if !ok {
		return nil, nil
	}
	return proxyURL, nil
}

func parseWindowsProxyServer(raw string, scheme string) (*url.URL, bool) {
	raw = strings.TrimSpace(raw)
	if raw == "" {
		return nil, false
	}

	scheme = strings.ToLower(strings.TrimSpace(scheme))
	selected := ""
	for _, entry := range strings.Split(raw, ";") {
		entry = strings.TrimSpace(entry)
		if entry == "" {
			continue
		}
		key, value, hasKey := strings.Cut(entry, "=")
		if !hasKey {
			selected = entry
			break
		}
		key = strings.ToLower(strings.TrimSpace(key))
		value = strings.TrimSpace(value)
		if key == scheme {
			selected = value
			break
		}
		if selected == "" && key == "http" {
			selected = value
		}
	}
	if selected == "" {
		return nil, false
	}
	if !strings.Contains(selected, "://") {
		selected = "http://" + selected
	}
	proxyURL, err := url.Parse(selected)
	if err != nil || proxyURL.Host == "" {
		return nil, false
	}
	return proxyURL, true
}

func shouldBypassProxy(host string) bool {
	host = strings.Trim(strings.ToLower(strings.TrimSpace(host)), "[]")
	if host == "" || host == "localhost" {
		return true
	}
	ip := net.ParseIP(host)
	return ip != nil && ip.IsLoopback()
}
