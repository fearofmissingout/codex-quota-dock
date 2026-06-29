//go:build cgo

package main

import (
	"os"
	"path/filepath"
	"testing"
	"time"

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

func TestLatestAuthBackupPathSelectsNewestAuthBackup(t *testing.T) {
	root := t.TempDir()
	oldPath := filepath.Join(root, "auth-20260629-100000.json")
	newPath := filepath.Join(root, "auth-20260629-110000.json")
	ignoredPath := filepath.Join(root, "notes.txt")
	for _, path := range []string{oldPath, newPath, ignoredPath} {
		if err := os.WriteFile(path, []byte("{}"), 0600); err != nil {
			t.Fatalf("write %s: %v", path, err)
		}
	}
	if err := os.Chtimes(oldPath, time.Now().Add(-time.Hour), time.Now().Add(-time.Hour)); err != nil {
		t.Fatalf("chtimes old: %v", err)
	}
	if err := os.Chtimes(newPath, time.Now(), time.Now()); err != nil {
		t.Fatalf("chtimes new: %v", err)
	}

	got, err := latestAuthBackupPath(root)
	if err != nil {
		t.Fatalf("latestAuthBackupPath returned error: %v", err)
	}
	if got != newPath {
		t.Fatalf("path=%q want %q", got, newPath)
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
