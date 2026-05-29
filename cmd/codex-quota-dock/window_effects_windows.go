package main

import (
	"syscall"
	"unsafe"

	"github.com/lxn/win"
)

const (
	lwaAlpha                    = 0x00000002
	dwmwaUseImmersiveDarkMode   = 20
	dwmwaSystemBackdropType     = 38
	dwmSystemBackdropAcrylic    = 3
	dwmSystemBackdropMainWindow = 2
)

var (
	user32SetLayeredWindowAttributes = syscall.NewLazyDLL("user32.dll").NewProc("SetLayeredWindowAttributes")
	dwmapiSetWindowAttribute         = syscall.NewLazyDLL("dwmapi.dll").NewProc("DwmSetWindowAttribute")
)

func applyFloatingWindowEffects(hwnd win.HWND, theme monitorTheme) {
	exStyle := uint32(win.GetWindowLong(hwnd, win.GWL_EXSTYLE))
	exStyle |= win.WS_EX_LAYERED
	win.SetWindowLong(hwnd, win.GWL_EXSTYLE, int32(exStyle))
	user32SetLayeredWindowAttributes.Call(uintptr(hwnd), 0, uintptr(theme.alpha), lwaAlpha)

	dark := int32(0)
	if theme.dark {
		dark = 1
	}
	setDwmIntAttribute(hwnd, dwmwaUseImmersiveDarkMode, dark)

	backdrop := int32(dwmSystemBackdropAcrylic)
	if !theme.dark {
		backdrop = dwmSystemBackdropMainWindow
	}
	setDwmIntAttribute(hwnd, dwmwaSystemBackdropType, backdrop)
}

func setDwmIntAttribute(hwnd win.HWND, attr uint32, value int32) {
	dwmapiSetWindowAttribute.Call(
		uintptr(hwnd),
		uintptr(attr),
		uintptr(unsafe.Pointer(&value)),
		unsafe.Sizeof(value),
	)
}
