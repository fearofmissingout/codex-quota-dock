//go:build !windows && !darwin && !linux && cgo

package main

import "fyne.io/fyne/v2"

func supportsBorderlessMonitorDrag() bool {
	return false
}

func configureBorderlessMonitorDrag(_ fyne.Window) {}

func startSystemWindowDrag(_ fyne.Window) {}
