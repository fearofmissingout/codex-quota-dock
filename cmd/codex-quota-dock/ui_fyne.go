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
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/driver/desktop"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

const prefShowRestartReminder = "show_restart_reminder_after_switch"

type splashDriver interface {
	CreateSplashWindow() fyne.Window
}

type appUI struct {
	app             fyne.App
	monitorWindow   fyne.Window
	configWindow    fyne.Window
	store           *profile.Store
	quotaClient     quota.Client
	activeAuthPath  string
	pollingInterval time.Duration

	selectedProfileID       string
	selectedMonitorID       string
	lastMonitorClick        time.Time
	lastMonitorClickProfile string
	syncingSelection        bool
	editorProfileID         string

	rowsMu sync.RWMutex
	rows   []profileRow

	monitorList   *widget.List
	monitorHeader *widget.Label
	monitorStatus *widget.Label

	configList    *widget.List
	activeLabel   *widget.Label
	statusLabel   *widget.Label
	details       *widget.Entry
	authEntry     *widget.Entry
	aliasEntry    *widget.Entry
	intervalInput *widget.Select
	pinButton     *widget.Button
	restartCheck  *widget.Check

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
	ui.createMonitorWindow()
	ui.startPolling(ui.pollingInterval)
	ui.monitorWindow.ShowAndRun()
	ui.stopPolling()
	return nil
}

func (u *appUI) createMonitorWindow() {
	u.monitorWindow = u.newMonitorWindow()
	u.monitorWindow.SetMaster()
	u.monitorWindow.SetFixedSize(true)
	u.monitorWindow.Resize(fyne.NewSize(360, float32(monitorWindowHeight(2))))

	u.monitorHeader = widget.NewLabel("Codex")
	u.monitorHeader.TextStyle = fyne.TextStyle{Bold: true}
	u.monitorStatus = widget.NewLabel("manual")

	u.monitorList = widget.NewList(
		func() int { return len(u.visibleRows()) },
		func() fyne.CanvasObject {
			title := canvas.NewText("", theme.ForegroundColor())
			title.TextStyle = fyne.TextStyle{Bold: true}
			title.TextSize = 12
			fiveHour := canvas.NewText("", theme.ForegroundColor())
			fiveHour.TextSize = 11
			weekly := canvas.NewText("", theme.ForegroundColor())
			weekly.TextSize = 11
			return container.NewVBox(title, fiveHour, weekly)
		},
		func(id widget.ListItemID, obj fyne.CanvasObject) {
			rows := u.visibleRows()
			if id < 0 || id >= len(rows) {
				return
			}
			row := rows[id]
			box := obj.(*fyne.Container)
			title := box.Objects[0].(*canvas.Text)
			fiveHour := box.Objects[1].(*canvas.Text)
			weekly := box.Objects[2].(*canvas.Text)
			active := row.Profile.AccountID == u.activeAccountID()
			selected := row.Profile.ID == u.selectedMonitorID
			prefix := "  "
			if selected {
				prefix = ">"
			}
			fiveHourText, weeklyText := monitorQuotaLines(row)
			title.Text = prefix + " " + monitorRowTitle(row, active)
			fiveHour.Text = "   " + fiveHourText
			weekly.Text = "   " + weeklyText
			title.Refresh()
			fiveHour.Refresh()
			weekly.Refresh()
		},
	)
	u.monitorList.OnSelected = func(id widget.ListItemID) {
		if u.syncingSelection {
			return
		}
		rows := u.visibleRows()
		if id < 0 || id >= len(rows) {
			return
		}
		u.handleMonitorSelection(rows[id].Profile.ID)
	}

	refreshButton := widget.NewButtonWithIcon("Refresh", theme.ViewRefreshIcon(), u.refreshVisible)
	switchButton := widget.NewButtonWithIcon("Switch", theme.LoginIcon(), u.switchMonitorSelected)
	configButton := widget.NewButtonWithIcon("Config", theme.SettingsIcon(), u.openConfigWindow)
	refreshButton.Importance = widget.LowImportance
	switchButton.Importance = widget.LowImportance
	configButton.Importance = widget.LowImportance

	header := container.NewBorder(nil, nil, nil, u.monitorStatus, u.monitorHeader)
	actions := container.NewGridWithColumns(3, refreshButton, switchButton, configButton)
	content := container.NewBorder(header, actions, nil, nil, u.monitorList)
	dragSurface := newWindowDragSurface(func() {
		startSystemWindowDrag(u.monitorWindow)
	})
	u.monitorWindow.SetContent(container.NewPadded(container.NewMax(content, dragSurface)))

	if desk, ok := u.app.(desktop.App); ok {
		desk.SetSystemTrayMenu(fyne.NewMenu("Codex Quota Dock",
			fyne.NewMenuItem("Show Monitor", func() {
				u.monitorWindow.Show()
				u.monitorWindow.RequestFocus()
			}),
			fyne.NewMenuItem("Open Config", u.openConfigWindow),
			fyne.NewMenuItem("Refresh Visible", u.refreshVisible),
			fyne.NewMenuItem("Quit", func() {
				u.app.Quit()
			}),
		))
		desk.SetSystemTrayWindow(u.monitorWindow)
	}

	u.refreshWidgets()
}

func (u *appUI) newMonitorWindow() fyne.Window {
	if driver, ok := u.app.Driver().(splashDriver); ok && supportsBorderlessMonitorDrag() {
		w := driver.CreateSplashWindow()
		w.SetTitle("Codex Quota Dock")
		configureBorderlessMonitorDrag(w)
		return w
	}
	return u.app.NewWindow("Codex Quota Dock")
}

func (u *appUI) openConfigWindow() {
	if u.configWindow == nil {
		u.createConfigWindow()
	}
	u.refreshWidgets()
	u.configWindow.Show()
	u.configWindow.RequestFocus()
}

func (u *appUI) createConfigWindow() {
	u.configWindow = u.app.NewWindow("Codex Quota Settings")
	u.configWindow.Resize(fyne.NewSize(980, 640))
	u.configWindow.SetCloseIntercept(func() {
		u.configWindow.Hide()
	})

	u.activeLabel = widget.NewLabel("Active Codex account: checking...")
	u.statusLabel = widget.NewLabel("")
	u.aliasEntry = widget.NewEntry()
	u.aliasEntry.SetPlaceHolder("Alias, e.g. company or pro")
	u.intervalInput = widget.NewSelect(pollingOptions(), func(label string) {
		u.startPolling(intervalFromLabel(label))
	})
	u.intervalInput.SetSelected(intervalLabel(u.pollingInterval))
	u.restartCheck = widget.NewCheck("Show restart reminder after switching", func(enabled bool) {
		u.app.Preferences().SetBool(prefShowRestartReminder, enabled)
	})
	u.restartCheck.SetChecked(u.showRestartReminder())
	u.details = widget.NewMultiLineEntry()
	u.details.Wrapping = fyne.TextWrapWord
	u.details.SetPlaceHolder("Quota details will appear after refresh.")
	u.authEntry = widget.NewMultiLineEntry()
	u.authEntry.Wrapping = fyne.TextWrapOff
	u.authEntry.SetPlaceHolder("Select a profile to edit its saved auth.json content.")

	u.configList = widget.NewList(
		func() int { return len(u.allRows()) },
		func() fyne.CanvasObject {
			title := widget.NewLabel("")
			title.TextStyle = fyne.TextStyle{Bold: true}
			line1 := widget.NewLabel("")
			line2 := widget.NewLabel("")
			return container.NewVBox(title, line1, line2)
		},
		func(id widget.ListItemID, obj fyne.CanvasObject) {
			rows := u.allRows()
			if id < 0 || id >= len(rows) {
				return
			}
			row := rows[id]
			box := obj.(*fyne.Container)
			title := box.Objects[0].(*widget.Label)
			line1 := box.Objects[1].(*widget.Label)
			line2 := box.Objects[2].(*widget.Label)
			title.SetText(monitorRowTitle(row, row.Profile.AccountID == u.activeAccountID()))
			line1.SetText(row.Quota)
			line2.SetText(fmt.Sprintf("status: %s | refreshed: %s", row.Status, row.LastRefresh))
		},
	)
	u.configList.OnSelected = func(id widget.ListItemID) {
		if u.syncingSelection {
			return
		}
		rows := u.allRows()
		if id < 0 || id >= len(rows) {
			return
		}
		u.selectedProfileID = rows[id].Profile.ID
		u.selectedMonitorID = rows[id].Profile.ID
		u.loadSelectedProfileEditor()
		u.refreshWidgets()
	}

	refreshSelected := widget.NewButtonWithIcon("Refresh Selected", theme.ViewRefreshIcon(), u.refreshSelected)
	refreshAll := widget.NewButton("Refresh All", u.refreshAll)
	switchButton := widget.NewButtonWithIcon("Switch Selected", theme.LoginIcon(), u.switchSelected)
	u.pinButton = widget.NewButton("Pin Selected", u.togglePinnedSelected)
	saveButton := widget.NewButtonWithIcon("Save Profile", theme.DocumentSaveIcon(), u.saveSelectedProfile)
	reloadButton := widget.NewButton("Reload Editor", u.loadSelectedProfileEditor)
	importCurrent := widget.NewButton("Import Current", u.importCurrentAuth)
	importFile := widget.NewButtonWithIcon("Import File", theme.FolderOpenIcon(), u.importAuthFile)

	top := container.NewBorder(nil, nil, nil, u.intervalInput, container.NewVBox(u.activeLabel, u.statusLabel))
	aliasForm := container.NewBorder(nil, nil, widget.NewLabel("Alias"), nil, u.aliasEntry)
	actions := container.NewVBox(
		container.NewGridWithColumns(4, saveButton, reloadButton, importCurrent, importFile),
		container.NewGridWithColumns(4, refreshSelected, refreshAll, switchButton, u.pinButton),
		u.restartCheck,
		widget.NewSeparator(),
		aliasForm,
	)
	left := container.NewBorder(nil, actions, nil, nil, u.configList)
	editorTabs := container.NewAppTabs(
		container.NewTabItem("Auth JSON", u.authEntry),
		container.NewTabItem("Quota Details", u.details),
	)
	right := container.NewBorder(widget.NewLabel("Profile Editor"), nil, nil, nil, editorTabs)
	split := container.NewHSplit(left, right)
	split.Offset = 0.48
	u.configWindow.SetContent(container.NewBorder(top, nil, nil, nil, split))
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
	if u.selectedMonitorID != "" {
		if row, ok := selectedProfileRow(rows, u.selectedMonitorID); ok {
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

func (u *appUI) selectedMonitorRow() (profileRow, bool) {
	rows := u.visibleRows()
	u.selectedMonitorID = normalizedMonitorSelection(rows, u.selectedMonitorID, u.activeAccountID())
	if u.selectedMonitorID == "" {
		return profileRow{}, false
	}
	return selectedProfileRow(rows, u.selectedMonitorID)
}

func (u *appUI) handleMonitorSelection(profileID string) {
	if profileID != u.lastMonitorClickProfile {
		u.lastMonitorClick = time.Time{}
	}
	u.selectedMonitorID = profileID
	u.selectedProfileID = profileID
	u.lastMonitorClickProfile = profileID
	open, nextClick := monitorClickAction(u.lastMonitorClick, time.Now())
	u.lastMonitorClick = nextClick
	u.refreshWidgets()
	if open {
		u.openConfigWindow()
	}
}

func (u *appUI) refreshWidgets() {
	u.syncMonitorSelection()
	if u.monitorList != nil {
		rows := u.visibleRows()
		u.monitorList.Refresh()
		u.selectListProfile(u.monitorList, rows, u.selectedMonitorID)
		u.resizeMonitorWindow(len(rows))
	}
	if u.configList != nil {
		u.configList.Refresh()
		u.selectListProfile(u.configList, u.allRows(), u.selectedProfileID)
	}
	u.updateActiveLabels()
	u.updateDetails()
}

func (u *appUI) resizeMonitorWindow(rowCount int) {
	if u.monitorWindow == nil {
		return
	}
	u.monitorWindow.Resize(fyne.NewSize(360, float32(monitorWindowHeight(rowCount))))
}

func (u *appUI) syncMonitorSelection() {
	rows := u.visibleRows()
	u.selectedMonitorID = normalizedMonitorSelection(rows, u.selectedMonitorID, u.activeAccountID())
	if u.selectedProfileID == "" {
		u.selectedProfileID = u.selectedMonitorID
	}
}

func (u *appUI) selectListProfile(list *widget.List, rows []profileRow, profileID string) {
	u.syncingSelection = true
	defer func() {
		u.syncingSelection = false
	}()
	for i, row := range rows {
		if row.Profile.ID == profileID {
			list.Select(widget.ListItemID(i))
			return
		}
	}
	list.UnselectAll()
}

func (u *appUI) updateActiveLabels() {
	text := "Active Codex account: unavailable"
	if active, err := auth.Load(u.activeAuthPath); err == nil {
		if prof, ok := u.store.FindByAccountID(active.Tokens.AccountID); ok {
			text = fmt.Sprintf("Active Codex account: %s (%s)", prof.Alias, prof.AccountSuffix)
		} else {
			text = fmt.Sprintf("Active Codex account: %s", active.AccountSuffix(6))
		}
	}
	visibleCount := len(u.visibleRows())
	refreshMode := intervalLabel(u.pollingInterval)
	if u.monitorHeader != nil {
		u.monitorHeader.SetText("Codex")
	}
	if u.monitorStatus != nil {
		u.monitorStatus.SetText(fmt.Sprintf("%d | %s", visibleCount, refreshMode))
	}
	if u.activeLabel != nil {
		u.activeLabel.SetText(text)
	}
	if u.statusLabel != nil {
		u.statusLabel.SetText(fmt.Sprintf("%d visible profiles | refresh: %s", visibleCount, refreshMode))
	}
}

func (u *appUI) updateDetails() {
	if u.details == nil {
		return
	}
	row, ok := u.selectedRow()
	if !ok {
		u.details.SetText("Import one or more Codex auth files, then refresh quota.")
		if u.aliasEntry != nil {
			u.aliasEntry.SetText("")
		}
		if u.authEntry != nil {
			u.authEntry.SetText("")
		}
		u.editorProfileID = ""
		u.updatePinButton(false)
		return
	}
	if u.authEntry != nil && u.editorProfileID != row.Profile.ID {
		u.loadSelectedProfileEditor()
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

func (u *appUI) loadSelectedProfileEditor() {
	if u.aliasEntry == nil || u.authEntry == nil {
		return
	}
	row, ok := u.selectedRow()
	if !ok {
		u.aliasEntry.SetText("")
		u.authEntry.SetText("")
		u.editorProfileID = ""
		return
	}
	data, err := u.store.ReadAuth(row.Profile.ID)
	if err != nil {
		u.showError("Load auth editor", err)
		return
	}
	u.aliasEntry.SetText(row.Profile.Alias)
	u.authEntry.SetText(string(data))
	u.editorProfileID = row.Profile.ID
}

func (u *appUI) saveSelectedProfile() {
	if u.aliasEntry == nil || u.authEntry == nil {
		u.openConfigWindow()
		return
	}
	row, ok := u.selectedRow()
	if !ok {
		u.showError("Save profile", errors.New("select a profile first"))
		return
	}
	alias := strings.TrimSpace(u.aliasEntry.Text)
	authText := strings.TrimSpace(u.authEntry.Text)
	if authText == "" {
		u.showError("Save profile", errors.New("auth JSON is required"))
		return
	}
	updated, err := u.store.Update(row.Profile.ID, alias, []byte(authText))
	if err != nil {
		u.showError("Save profile", err)
		return
	}
	u.updateProfileRow(updated.ID, func(row *profileRow) {
		*row = newProfileRow(updated)
	})
	u.selectedProfileID = updated.ID
	u.selectedMonitorID = updated.ID
	u.editorProfileID = updated.ID
	u.refreshWidgets()
	dialog.ShowInformation("Saved", fmt.Sprintf("Saved profile %q.", updated.Alias), u.dialogWindow())
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
	}, u.dialogWindow())
}

func (u *appUI) importAuthPath(path string) {
	if u.aliasEntry == nil {
		u.openConfigWindow()
		u.showError("Import auth", errors.New("enter an alias in the config window first"))
		return
	}
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
	u.selectedMonitorID = prof.ID
	u.editorProfileID = ""
	u.refreshWidgets()
	dialog.ShowInformation("Imported", fmt.Sprintf("Imported profile %q (%s).", prof.Alias, prof.AccountSuffix), u.dialogWindow())
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
		u.openConfigWindow()
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
		u.openConfigWindow()
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
	u.switchProfile(row.Profile, u.configWindow)
}

func (u *appUI) switchMonitorSelected() {
	row, ok := u.selectedMonitorRow()
	if !ok {
		u.openConfigWindow()
		return
	}
	u.switchProfile(row.Profile, u.monitorWindow)
}

func (u *appUI) switchProfile(prof profile.Profile, parent fyne.Window) {
	parent = u.dialogWindowFor(parent)
	message := fmt.Sprintf("Switch active Codex auth to %q?\n\nCodex must be restarted after switching.", prof.Alias)
	dialog.ShowConfirm("Confirm switch", message, func(ok bool) {
		if !ok {
			return
		}
		sw := switcher.New(u.activeAuthPath, u.store.BackupsDir())
		result, err := sw.Switch(u.store.AuthPath(prof.ID))
		if err != nil {
			u.showErrorIn("Switch account", err, parent)
			return
		}
		u.refreshWidgets()
		if u.showRestartReminder() {
			u.showSwitchReminderDialog(prof, result.BackupPath, parent)
		} else if u.statusLabel != nil {
			u.statusLabel.SetText("Auth switched. Restart Codex to use the new account.")
		}
	}, parent)
}

func (u *appUI) showSwitchReminderDialog(prof profile.Profile, backupPath string, parent fyne.Window) {
	copy := newSwitchReminderCopy(prof.Alias, backupPath)

	heading := canvas.NewText(copy.Heading, theme.ForegroundColor())
	heading.TextSize = 18
	heading.TextStyle = fyne.TextStyle{Bold: true}

	summary := widget.NewLabel(copy.Summary)
	summary.Wrapping = fyne.TextWrapWord

	restart := canvas.NewText(copy.Restart, theme.PrimaryColor())
	restart.TextSize = 14
	restart.TextStyle = fyne.TextStyle{Bold: true}

	backupLabel := widget.NewLabel(copy.BackupLabel)
	backupLabel.TextStyle = fyne.TextStyle{Bold: true}

	backupEntry := widget.NewMultiLineEntry()
	backupEntry.Wrapping = fyne.TextWrapOff
	backupEntry.SetMinRowsVisible(2)
	backupEntry.SetText(copy.BackupPath)

	copyStatus := widget.NewLabel("")
	copyStatus.Wrapping = fyne.TextWrapWord
	copyBackup := widget.NewButtonWithIcon("Copy backup path", theme.ContentCopyIcon(), func() {
		u.app.Clipboard().SetContent(copy.BackupPath)
		copyStatus.SetText("Copied.")
	})
	copyBackup.Importance = widget.LowImportance

	footer := widget.NewLabel(copy.Footer)
	footer.Wrapping = fyne.TextWrapWord
	footer.TextStyle = fyne.TextStyle{Italic: true}

	content := container.NewVBox(
		heading,
		summary,
		restart,
		widget.NewSeparator(),
		backupLabel,
		backupEntry,
		container.NewBorder(nil, nil, nil, copyStatus, copyBackup),
		footer,
	)
	reminder := dialog.NewCustom(copy.DialogTitle, "OK", content, u.dialogWindowFor(parent))
	reminder.Resize(fyne.NewSize(560, 320))
	reminder.Show()
}

func (u *appUI) showRestartReminder() bool {
	return u.app.Preferences().BoolWithFallback(prefShowRestartReminder, true)
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

func (u *appUI) dialogWindow() fyne.Window {
	if u.configWindow != nil {
		return u.configWindow
	}
	return u.monitorWindow
}

func (u *appUI) dialogWindowFor(parent fyne.Window) fyne.Window {
	if parent != nil {
		return parent
	}
	return u.dialogWindow()
}

func (u *appUI) showError(title string, err error) {
	u.showErrorIn(title, err, nil)
}

func (u *appUI) showErrorIn(title string, err error, parent fyne.Window) {
	dialog.ShowError(fmt.Errorf("%s: %w", title, err), u.dialogWindowFor(parent))
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
