//go:build cgo

package main

import (
	"testing"

	fynetest "fyne.io/fyne/v2/test"
)

func TestDialogWindowForPrefersExplicitParent(t *testing.T) {
	a := fynetest.NewApp()
	defer a.Quit()

	config := a.NewWindow("config")
	monitor := a.NewWindow("monitor")
	ui := &appUI{
		monitorWindow: monitor,
		configWindow:  config,
	}

	got := ui.dialogWindowFor(monitor)
	if got != monitor {
		t.Fatal("dialogWindowFor should use the window that triggered the action")
	}
}
