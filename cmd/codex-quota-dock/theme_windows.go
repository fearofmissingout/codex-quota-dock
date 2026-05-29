package main

import (
	"github.com/lxn/walk"
	"golang.org/x/sys/windows/registry"
)

func currentMonitorTheme() monitorTheme {
	if systemUsesDarkMode() {
		return monitorTheme{
			dark:               true,
			background:         walk.RGB(28, 29, 32),
			rowBackground:      walk.RGB(39, 41, 46),
			activeBackground:   walk.RGB(38, 57, 84),
			selectedBackground: walk.RGB(48, 75, 116),
			text:               walk.RGB(238, 242, 248),
			muted:              walk.RGB(174, 184, 196),
			accent:             walk.RGB(116, 169, 255),
			alpha:              232,
		}
	}
	return monitorTheme{
		background:         walk.RGB(244, 247, 251),
		rowBackground:      walk.RGB(255, 255, 255),
		activeBackground:   walk.RGB(226, 238, 255),
		selectedBackground: walk.RGB(214, 228, 255),
		text:               walk.RGB(24, 29, 38),
		muted:              walk.RGB(86, 99, 118),
		accent:             walk.RGB(45, 108, 255),
		alpha:              238,
	}
}

func systemUsesDarkMode() bool {
	key, err := registry.OpenKey(registry.CURRENT_USER, `Software\Microsoft\Windows\CurrentVersion\Themes\Personalize`, registry.QUERY_VALUE)
	if err != nil {
		return false
	}
	defer key.Close()

	lightTheme, _, err := key.GetIntegerValue("AppsUseLightTheme")
	return err == nil && lightTheme == 0
}
