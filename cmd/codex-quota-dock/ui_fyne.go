//go:build cgo

package main

import (
	"context"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"time"

	"github.com/fearofmissingout/codex-quota-dock/internal/auth"
	"github.com/fearofmissingout/codex-quota-dock/internal/display"
	"github.com/fearofmissingout/codex-quota-dock/internal/profile"
	"github.com/fearofmissingout/codex-quota-dock/internal/quota"
	"github.com/fearofmissingout/codex-quota-dock/internal/settings"
	"github.com/fearofmissingout/codex-quota-dock/internal/switcher"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/driver/desktop"
	"fyne.io/fyne/v2/widget"
)

type appUI struct {
	app               fyne.App
	window            fyne.Window
	store             *profile.Store
	quotaClient       quota.Client
	activeAuthPath    string
	pollingInterval   time.Duration
	selectedProfileID string

	rowsMu sync.RWMutex
	rows   []profileRow

	list          *widget.List
	activeLabel   *widget.Label
	statusLabel   *widget.Label
	details       *widget.Entry
	aliasEntry    *widget.Entry
	intervalInput *widget.Select
	pinButton     *widget.Button

	pollMu   sync.Mutex
	pollStop chan struct{}
}

func run() error {
	root, err := appDataRoot()
	if err != nil {
		return err
	}
	store, err := profile.Open(root)
	if err != nil {
		return err
	}
	activeAuthPath, err := switcher.DefaultCodexAuthPath()
	if err != nil {
		return err
	}

	a := app.NewWithID("io.github.fearofmissingout.codex-quota-dock")
	ui := &appUI{
		app:             a,
		store:           store,
		quotaClient:     quota.DefaultClient(),
		activeAuthPath:  activeAuthPath,
		pollingInterval: settings.DefaultPollingInterval(),
	}
	ui.reloadRows()
	ui.createWindow()
	ui.startPolling(ui.pollingInterval)
	ui.window.ShowAndRun()
	ui.stopPolling()
	return nil
}

func (u *appUI) createWindow() {
	u.window = u.app.NewWindow("Codex Quota Dock")
	u.window.SetMaster()
	u.window.Resize(fyne.NewSize(820, 560))

	u.activeLabel = widget.NewLabel("Active Codex account: checking...")
	u.statusLabel = widget.NewLabel("")
	u.aliasEntry = widget.NewEntry()
	u.aliasEntry.SetPlaceHolder("Alias, e.g. company or pro")
	u.intervalInput = widget.NewSelect(pollingOptions(), func(label string) {
		u.startPolling(intervalFromLabel(label))
	})
	u.intervalInput.SetSelected(intervalLabel(u.pollingInterval))
	u.details = widget.NewMultiLineEntry()
	u.details.Wrapping = fyne.TextWrapWord
	u.details.SetPlaceHolder("Select a profile to view quota details.")

	u.list = widget.NewList(
		func() int { return len(u.visibleRows()) },
		func() fyne.CanvasObject {
			title := widget.NewLabel("")
			title.TextStyle = fyne.TextStyle{Bold: true}
			line1 := widget.NewLabel("")
			line2 := widget.NewLabel("")
			return container.NewVBox(title, line1, line2)
		},
		func(id widget.ListItemID, obj fyne.CanvasObject) {
			rows := u.visibleRows()
			if id < 0 || id >= len(rows) {
				return
			}
			row := rows[id]
			box := obj.(*fyne.Container)
			title := box.Objects[0].(*widget.Label)
			line1 := box.Objects[1].(*widget.Label)
			line2 := box.Objects[2].(*widget.Label)
			title.SetText(monitorRowTitle(row, row.Profile.AccountID == u.activeAccountID()))
			line1.SetText(monitorCompactLine(row, 0, "5h: not refreshed"))
			line2.SetText(monitorCompactLine(row, 1, "weekly: not refreshed"))
		},
	)
	u.list.OnSelected = func(id widget.ListItemID) {
		rows := u.visibleRows()
		if id < 0 || id >= len(rows) {
			return
		}
		u.selectedProfileID = rows[id].Profile.ID
		u.updateDetails()
	}

	refreshSelected := widget.NewButton("Refresh Selected", u.refreshSelected)
	refreshVisible := widget.NewButton("Refresh Visible", u.refreshVisible)
	refreshAll := widget.NewButton("Refresh All", u.refreshAll)
	switchButton := widget.NewButton("Switch Selected", u.switchSelected)
	u.pinButton = widget.NewButton("Pin Selected", u.togglePinnedSelected)

	importCurrent := widget.NewButton("Import Current", u.importCurrentAuth)
	importFile := widget.NewButton("Import File", u.importAuthFile)
	top := container.NewBorder(nil, nil, nil, u.intervalInput, container.NewVBox(u.activeLabel, u.statusLabel))
	actions := container.NewVBox(
		container.NewGridWithColumns(3, refreshSelected, refreshVisible, refreshAll),
		container.NewGridWithColumns(2, switchButton, u.pinButton),
		widget.NewSeparator(),
		container.NewBorder(nil, nil, nil, container.NewHBox(importCurrent, importFile), u.aliasEntry),
	)
	left := container.NewBorder(nil, actions, nil, nil, u.list)
	right := container.NewBorder(widget.NewLabel("Details"), nil, nil, nil, u.details)
	split := container.NewHSplit(left, right)
	split.Offset = 0.48
	u.window.SetContent(container.NewBorder(top, nil, nil, nil, split))

	if desk, ok := u.app.(desktop.App); ok {
		desk.SetSystemTrayMenu(fyne.NewMenu("Codex Quota Dock",
			fyne.NewMenuItem("Show", func() {
				u.window.Show()
				u.window.RequestFocus()
			}),
			fyne.NewMenuItem("Refresh Visible", u.refreshVisible),
			fyne.NewMenuItem("Quit", func() {
				u.app.Quit()
			}),
		))
		desk.SetSystemTrayWindow(u.window)
	}

	u.updateActiveLabels()
	u.updateDetails()
	u.refreshWidgets()
}

func (u *appUI) reloadRows() {
	profiles := u.store.Profiles()
	rows := make([]profileRow, 0, len(profiles))
	for _, prof := range profiles {
		rows = append(rows, newProfileRow(prof))
	}
	u.rowsMu.Lock()
	u.rows = rows
	u.rowsMu.Unlock()
}

func (u *appUI) allRows() []profileRow {
	u.rowsMu.RLock()
	defer u.rowsMu.RUnlock()
	out := make([]profileRow, len(u.rows))
	copy(out, u.rows)
	return out
}

func (u *appUI) visibleRows() []profileRow {
	return visibleMonitorRows(u.allRows(), u.activeAccountID())
}

func (u *appUI) selectedRow() (profileRow, bool) {
	rows := u.allRows()
	if u.selectedProfileID != "" {
		if row, ok := selectedProfileRow(rows, u.selectedProfileID); ok {
			return row, true
		}
	}
	visible := visibleMonitorRows(rows, u.activeAccountID())
	if len(visible) == 0 {
		return profileRow{}, false
	}
	u.selectedProfileID = visible[0].Profile.ID
	return visible[0], true
}

func (u *appUI) refreshWidgets() {
	if u.list != nil {
		u.list.Refresh()
	}
	u.updateActiveLabels()
	u.updateDetails()
}

func (u *appUI) updateActiveLabels() {
	if u.activeLabel == nil {
		return
	}
	text := "Active Codex account: unavailable"
	if active, err := auth.Load(u.activeAuthPath); err == nil {
		if prof, ok := u.store.FindByAccountID(active.Tokens.AccountID); ok {
			text = fmt.Sprintf("Active Codex account: %s (%s)", prof.Alias, prof.AccountSuffix)
		} else {
			text = fmt.Sprintf("Active Codex account: %s", active.AccountSuffix(6))
		}
	}
	u.activeLabel.SetText(text)
	if u.statusLabel != nil {
		count := len(u.visibleRows())
		mode := intervalLabel(u.pollingInterval)
		u.statusLabel.SetText(fmt.Sprintf("%d visible profiles | refresh: %s", count, mode))
	}
}

func (u *appUI) updateDetails() {
	if u.details == nil {
		return
	}
	row, ok := u.selectedRow()
	if !ok {
		u.details.SetText("Import one or more Codex auth files, then refresh quota.")
		u.updatePinButton(false)
		return
	}
	u.details.SetText(row.Details)
	u.updatePinButton(row.Profile.Pinned)
}

func (u *appUI) updatePinButton(pinned bool) {
	if u.pinButton == nil {
		return
	}
	if pinned {
		u.pinButton.SetText("Unpin Selected")
		return
	}
	u.pinButton.SetText("Pin Selected")
}

func (u *appUI) importCurrentAuth() {
	u.importAuthPath(u.activeAuthPath)
}

func (u *appUI) importAuthFile() {
	dialog.ShowFileOpen(func(reader fyne.URIReadCloser, err error) {
		if err != nil {
			u.showError("Open auth file", err)
			return
		}
		if reader == nil {
			return
		}
		defer reader.Close()
		u.importAuthPath(localPathFromURI(reader.URI()))
	}, u.window)
}

func (u *appUI) importAuthPath(path string) {
	alias := strings.TrimSpace(u.aliasEntry.Text)
	if alias == "" {
		u.showError("Import auth", errors.New("enter an alias first, for example company or pro"))
		return
	}
	prof, err := u.store.Import(alias, path)
	if err != nil {
		u.showError("Import auth", err)
		return
	}
	u.rowsMu.Lock()
	u.rows = append(u.rows, newProfileRow(prof))
	u.rowsMu.Unlock()
	u.selectedProfileID = prof.ID
	u.aliasEntry.SetText("")
	u.refreshWidgets()
	dialog.ShowInformation("Imported", fmt.Sprintf("Imported profile %q (%s).", prof.Alias, prof.AccountSuffix), u.window)
}

func (u *appUI) refreshSelected() {
	row, ok := u.selectedRow()
	if !ok {
		u.showError("Refresh selected", errors.New("select a profile first"))
		return
	}
	go u.refreshProfile(row.Profile)
}

func (u *appUI) refreshVisible() {
	rows := u.visibleRows()
	if len(rows) == 0 {
		u.showError("Refresh visible", errors.New("import a profile first"))
		return
	}
	go func() {
		for _, row := range rows {
			u.refreshProfile(row.Profile)
			time.Sleep(500 * time.Millisecond)
		}
	}()
}

func (u *appUI) refreshAll() {
	rows := u.allRows()
	if len(rows) == 0 {
		u.showError("Refresh all", errors.New("import a profile first"))
		return
	}
	go func() {
		for _, row := range rows {
			u.refreshProfile(row.Profile)
			time.Sleep(500 * time.Millisecond)
		}
	}()
}

func (u *appUI) refreshProfile(prof profile.Profile) {
	u.updateProfileRow(prof.ID, func(row *profileRow) {
		row.Status = display.StatusText(display.StatusRefreshing)
	})
	u.withUI(u.refreshWidgets)

	authFile, err := auth.Load(u.store.AuthPath(prof.ID))
	if err != nil {
		u.setRefreshError(prof.ID, err)
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	snapshots, err := u.quotaClient.Fetch(ctx, authFile)
	if err != nil {
		u.setRefreshError(prof.ID, err)
		return
	}
	now := time.Now()
	formatted := display.FormatSnapshots(snapshots)
	u.updateProfileRow(prof.ID, func(row *profileRow) {
		row.Status = display.StatusText(display.StatusOK)
		row.Quota = formatted.Summary
		row.LastRefresh = now.Format("2006-01-02 15:04:05")
		row.Details = formatted.Details
		row.CompactTitle = formatted.CompactTitle
		row.CompactLines = formatted.CompactLines
	})
	u.withUI(u.refreshWidgets)
}

func (u *appUI) setRefreshError(profileID string, err error) {
	u.updateProfileRow(profileID, func(row *profileRow) {
		row.Status = display.StatusText(display.ClassifyError(err))
		row.Details = err.Error()
	})
	u.withUI(u.refreshWidgets)
}

func (u *appUI) updateProfileRow(profileID string, update func(*profileRow)) {
	u.rowsMu.Lock()
	defer u.rowsMu.Unlock()
	for i := range u.rows {
		if u.rows[i].Profile.ID == profileID {
			update(&u.rows[i])
			return
		}
	}
}

func (u *appUI) switchSelected() {
	row, ok := u.selectedRow()
	if !ok {
		u.showError("Switch account", errors.New("select a profile first"))
		return
	}
	u.switchProfile(row.Profile)
}

func (u *appUI) switchProfile(prof profile.Profile) {
	message := fmt.Sprintf("Switch active Codex auth to %q?\n\nCodex must be restarted after switching.", prof.Alias)
	dialog.ShowConfirm("Confirm switch", message, func(ok bool) {
		if !ok {
			return
		}
		sw := switcher.New(u.activeAuthPath, u.store.BackupsDir())
		result, err := sw.Switch(u.store.AuthPath(prof.ID))
		if err != nil {
			u.showError("Switch account", err)
			return
		}
		u.refreshWidgets()
		dialog.ShowInformation(
			"Codex auth switched",
			fmt.Sprintf("Active auth was replaced.\n\nBackup: %s\n\nPlease restart Codex for the new account to take effect.", result.BackupPath),
			u.window,
		)
	}, u.window)
}

func (u *appUI) togglePinnedSelected() {
	row, ok := u.selectedRow()
	if !ok {
		u.showError("Pin profile", errors.New("select a profile first"))
		return
	}
	updated, err := u.store.SetPinned(row.Profile.ID, !row.Profile.Pinned)
	if err != nil {
		u.showError("Pin profile", err)
		return
	}
	u.updateProfileRow(updated.ID, func(row *profileRow) {
		row.Profile = updated
	})
	u.refreshWidgets()
}

func (u *appUI) startPolling(interval time.Duration) {
	u.pollingInterval = interval
	u.stopPolling()
	u.refreshWidgets()
	if interval <= 0 {
		return
	}
	stop := make(chan struct{})
	u.pollMu.Lock()
	u.pollStop = stop
	u.pollMu.Unlock()

	go func() {
		ticker := time.NewTicker(interval)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				u.refreshVisible()
			case <-stop:
				return
			}
		}
	}()
}

func (u *appUI) stopPolling() {
	u.pollMu.Lock()
	defer u.pollMu.Unlock()
	if u.pollStop != nil {
		close(u.pollStop)
		u.pollStop = nil
	}
}

func (u *appUI) activeAccountID() string {
	active, err := auth.Load(u.activeAuthPath)
	if err != nil {
		return ""
	}
	return active.Tokens.AccountID
}

func (u *appUI) showError(title string, err error) {
	dialog.ShowError(fmt.Errorf("%s: %w", title, err), u.window)
}

func (u *appUI) withUI(fn func()) {
	fyne.Do(fn)
}

func localPathFromURI(uri fyne.URI) string {
	path := uri.Path()
	if runtime.GOOS == "windows" && len(path) >= 3 && path[0] == '/' && path[2] == ':' {
		path = path[1:]
	}
	return filepath.FromSlash(path)
}

func appDataRoot() (string, error) {
	base, err := os.UserConfigDir()
	if err != nil {
		return "", fmt.Errorf("resolve config dir: %w", err)
	}
	return filepath.Join(base, "codex-quota-dock"), nil
}
