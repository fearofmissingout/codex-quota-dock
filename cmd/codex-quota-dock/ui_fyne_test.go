//go:build cgo

package main

import (
	"testing"

	"fyne.io/fyne/v2"
	fynetest "fyne.io/fyne/v2/test"
	"fyne.io/fyne/v2/widget"
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

func TestAuthJSONEditorPanelHasInlineSaveControls(t *testing.T) {
	a := fynetest.NewApp()
	defer a.Quit()

	ui := &appUI{app: a, authEntry: widget.NewMultiLineEntry()}

	panel := ui.newAuthJSONEditorPanel()

	if !containsButtonText(panel, "Save Auth JSON") {
		t.Fatalf("auth JSON panel does not expose an inline save button")
	}
	if !containsButtonText(panel, "Reload JSON") {
		t.Fatalf("auth JSON panel does not expose an inline reload button")
	}
}

func containsButtonText(obj fyne.CanvasObject, text string) bool {
	if button, ok := obj.(*widget.Button); ok && button.Text == text {
		return true
	}
	if container, ok := obj.(*fyne.Container); ok {
		for _, child := range container.Objects {
			if containsButtonText(child, text) {
				return true
			}
		}
	}
	return false
}
