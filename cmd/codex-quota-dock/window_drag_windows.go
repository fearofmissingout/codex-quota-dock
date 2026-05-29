//go:build windows && cgo

package main

import (
	"syscall"
	"unsafe"

	"fyne.io/fyne/v2"
	fynedriver "fyne.io/fyne/v2/driver"
)

const (
	swpNoSize     = 0x0001
	swpNoZOrder   = 0x0004
	swpNoActivate = 0x0010
)

type winRect struct {
	Left   int32
	Top    int32
	Right  int32
	Bottom int32
}

var (
	user32Proc    = syscall.NewLazyDLL("user32.dll")
	getWindowRect = user32Proc.NewProc("GetWindowRect")
	setWindowPos  = user32Proc.NewProc("SetWindowPos")
)

func supportsBorderlessMonitorDrag() bool {
	return true
}

func moveWindowBy(win fyne.Window, delta fyne.Delta) {
	if win == nil {
		return
	}
	dx, dy := dragDeltaPixels(delta, win.Canvas().Scale())
	if dx == 0 && dy == 0 {
		return
	}
	native, ok := win.(fynedriver.NativeWindow)
	if !ok {
		return
	}
	native.RunNative(func(context any) {
		winContext, ok := context.(fynedriver.WindowsWindowContext)
		if !ok || winContext.HWND == 0 {
			return
		}
		var rect winRect
		okCode, _, _ := getWindowRect.Call(winContext.HWND, uintptr(unsafe.Pointer(&rect)))
		if okCode == 0 {
			return
		}
		setWindowPos.Call(
			winContext.HWND,
			0,
			uintptr(int32(dx)+rect.Left),
			uintptr(int32(dy)+rect.Top),
			0,
			0,
			uintptr(swpNoSize|swpNoZOrder|swpNoActivate),
		)
	})
}
