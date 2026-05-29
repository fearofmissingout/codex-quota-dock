//go:build windows && cgo

package main

import (
	"syscall"

	"fyne.io/fyne/v2"
	fynedriver "fyne.io/fyne/v2/driver"
)

const (
	htCaption       = 2
	wmNCLButtonDown = 0x00A1
)

var (
	user32Proc     = syscall.NewLazyDLL("user32.dll")
	releaseCapture = user32Proc.NewProc("ReleaseCapture")
	sendMessageW   = user32Proc.NewProc("SendMessageW")
)

func supportsBorderlessMonitorDrag() bool {
	return true
}

func startSystemWindowDrag(win fyne.Window) {
	if win == nil {
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
		releaseCapture.Call()
		sendMessageW.Call(winContext.HWND, wmNCLButtonDown, htCaption, 0)
	})
}
