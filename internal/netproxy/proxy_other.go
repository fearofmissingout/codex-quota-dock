//go:build !windows

package netproxy

func windowsProxySettings() (bool, string) {
	return false, ""
}
