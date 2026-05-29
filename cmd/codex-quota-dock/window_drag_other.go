//go:build !windows && cgo

package main

import "fyne.io/fyne/v2"

func supportsBorderlessMonitorDrag() bool {
	return false
}

func startSystemWindowDrag(_ fyne.Window) {}
