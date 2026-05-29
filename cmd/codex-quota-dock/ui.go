package main

import (
	"context"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/fearofmissingout/codex-quota-dock/internal/auth"
	"github.com/fearofmissingout/codex-quota-dock/internal/display"
	"github.com/fearofmissingout/codex-quota-dock/internal/profile"
	"github.com/fearofmissingout/codex-quota-dock/internal/quota"
	"github.com/fearofmissingout/codex-quota-dock/internal/settings"
	"github.com/fearofmissingout/codex-quota-dock/internal/switcher"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
	"github.com/lxn/win"
)

type profileRow struct {
	Profile      profile.Profile
	Status       string
	Quota        string
	LastRefresh  string
	Details      string
	CompactTitle string
	CompactLines []string
}

type profileModel struct {
	walk.TableModelBase
	rows []profileRow
}

func (m *profileModel) RowCount() int {
	return len(m.rows)
}

func (m *profileModel) Value(row, col int) interface{} {
	item := m.rows[row]
	switch col {
	case 0:
		return item.Profile.Alias
	case 1:
		return item.Profile.AuthMode
	case 2:
		if item.Profile.AccountSuffix == "" {
			return "-"
		}
		return item.Profile.AccountSuffix
	case 3:
		return item.Status
	case 4:
		return item.Quota
	case 5:
		return item.LastRefresh
	default:
		return ""
	}
}

func (m *profileModel) setRows(rows []profileRow) {
	m.rows = rows
	m.PublishRowsReset()
}

func (m *profileModel) row(index int) (profileRow, bool) {
	if index < 0 || index >= len(m.rows) {
		return profileRow{}, false
	}
	return m.rows[index], true
}

func (m *profileModel) rowByProfileID(profileID string) (profileRow, bool) {
	for _, row := range m.rows {
		if row.Profile.ID == profileID {
			return row, true
		}
	}
	return profileRow{}, false
}

func (m *profileModel) update(profileID string, update func(*profileRow)) {
	for i := range m.rows {
		if m.rows[i].Profile.ID == profileID {
			update(&m.rows[i])
			m.PublishRowChanged(i)
			return
		}
	}
}

type appUI struct {
	mw               *walk.MainWindow
	detailDialog     *walk.Dialog
	table            *walk.TableView
	details          *walk.TextEdit
	detailActive     *walk.Label
	aliasEdit        *walk.LineEdit
	intervalBox      *walk.ComboBox
	monitorTitle     *walk.TextLabel
	monitorLine1     *walk.TextLabel
	monitorLine2     *walk.TextLabel
	monitorStatus    *walk.TextLabel
	model            *profileModel
	store            *profile.Store
	quotaClient      quota.Client
	activeAuthPath   string
	pollingInterval  time.Duration
	lastMonitorClick time.Time

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

	ui := &appUI{
		model:           &profileModel{},
		store:           store,
		quotaClient:     quota.DefaultClient(),
		activeAuthPath:  activeAuthPath,
		pollingInterval: settings.DefaultPollingInterval(),
	}
	ui.reloadRows()

	if err := ui.createMonitorWindow(); err != nil {
		return err
	}
	ui.updateActiveLabels()
	ui.updateMonitor()
	ui.startPolling(ui.pollingInterval)
	ui.mw.Run()
	ui.stopPolling()
	return nil
}

func (u *appUI) createMonitorWindow() error {
	if err := (MainWindow{
		AssignTo:    &u.mw,
		Title:       "Codex Monitor",
		Size:        Size{Width: 318, Height: 150},
		MinSize:     Size{Width: 300, Height: 142},
		MaxSize:     Size{Width: 390, Height: 190},
		Layout:      VBox{Margins: Margins{Left: 14, Top: 12, Right: 14, Bottom: 12}, Spacing: 8},
		Background:  SolidColorBrush{Color: walk.RGB(246, 247, 249)},
		OnMouseDown: u.handleMonitorMouseDown,
		Children: []Widget{
			Composite{
				Layout:      VBox{MarginsZero: true, Spacing: 3},
				OnMouseDown: u.handleMonitorMouseDown,
				Children: []Widget{
					TextLabel{
						AssignTo:    &u.monitorTitle,
						Text:        "Codex",
						TextColor:   walk.RGB(28, 31, 36),
						Font:        Font{Family: "Segoe UI", PointSize: 13, Bold: true},
						OnMouseDown: u.handleMonitorMouseDown,
					},
					TextLabel{
						AssignTo:    &u.monitorLine1,
						Text:        "5h: not refreshed",
						TextColor:   walk.RGB(55, 65, 81),
						Font:        Font{Family: "Segoe UI", PointSize: 9},
						OnMouseDown: u.handleMonitorMouseDown,
					},
					TextLabel{
						AssignTo:    &u.monitorLine2,
						Text:        "weekly: not refreshed",
						TextColor:   walk.RGB(55, 65, 81),
						Font:        Font{Family: "Segoe UI", PointSize: 9},
						OnMouseDown: u.handleMonitorMouseDown,
					},
					TextLabel{
						AssignTo:    &u.monitorStatus,
						Text:        "active: checking",
						TextColor:   walk.RGB(101, 112, 128),
						Font:        Font{Family: "Segoe UI", PointSize: 8},
						OnMouseDown: u.handleMonitorMouseDown,
					},
				},
			},
			Composite{
				Layout: HBox{MarginsZero: true, Spacing: 6},
				Children: []Widget{
					PushButton{Text: "Refresh", MaxSize: Size{Width: 78}, OnClicked: u.refreshActive},
					PushButton{Text: "Open", MaxSize: Size{Width: 62}, OnClicked: u.openDetailsWindow},
					HSpacer{},
					PushButton{Text: "Close", MaxSize: Size{Width: 60}, OnClicked: func() { _ = u.mw.Close() }},
				},
			},
		},
	}).Create(); err != nil {
		return err
	}
	u.styleFloatingMonitor()
	return nil
}

func (u *appUI) openDetailsWindow() {
	if u.detailDialog != nil && !u.detailDialog.IsDisposed() {
		u.detailDialog.Show()
		_ = u.detailDialog.Activate()
		return
	}
	if err := u.createDetailsWindow(); err != nil {
		u.errorBox("Open details", err)
		return
	}
	u.updateActiveLabels()
	u.showSelectedDetails()
	u.detailDialog.Show()
	_ = u.detailDialog.Activate()
}

func (u *appUI) createDetailsWindow() error {
	intervals := []string{"Off", "1 minute", "5 minutes", "10 minutes"}
	if err := (Dialog{
		AssignTo: &u.detailDialog,
		Title:    "Codex Quota Details",
		Size:     Size{Width: 1000, Height: 660},
		MinSize:  Size{Width: 820, Height: 520},
		Layout:   VBox{Margins: Margins{Left: 12, Top: 12, Right: 12, Bottom: 12}, Spacing: 9},
		Children: []Widget{
			Composite{
				Layout: HBox{MarginsZero: true, Spacing: 8},
				Children: []Widget{
					Label{AssignTo: &u.detailActive, Text: "Active Codex account: checking..."},
					HSpacer{},
					Label{Text: "Auto refresh"},
					ComboBox{
						AssignTo:     &u.intervalBox,
						Model:        intervals,
						CurrentIndex: intervalIndex(u.pollingInterval),
						OnCurrentIndexChanged: func() {
							u.startPolling(u.selectedInterval())
						},
					},
				},
			},
			Composite{
				Layout: HBox{MarginsZero: true, Spacing: 7},
				Children: []Widget{
					Label{Text: "Alias"},
					LineEdit{AssignTo: &u.aliasEdit, ToolTipText: "Example: company or pro", MinSize: Size{Width: 150}},
					PushButton{Text: "Import current", OnClicked: u.importCurrentAuth},
					PushButton{Text: "Import file", OnClicked: u.importAuthFile},
					PushButton{Text: "Refresh selected", OnClicked: u.refreshSelected},
					PushButton{Text: "Refresh all", OnClicked: u.refreshAll},
					PushButton{Text: "Switch selected", OnClicked: u.switchSelected},
				},
			},
			TableView{
				AssignTo:         &u.table,
				AlternatingRowBG: true,
				ColumnsOrderable: true,
				Model:            u.model,
				Columns: []TableViewColumn{
					{Title: "Alias", Width: 160},
					{Title: "Auth", Width: 90},
					{Title: "Account", Width: 100},
					{Title: "Status", Width: 140},
					{Title: "Quota", Width: 280},
					{Title: "Last refresh", Width: 170},
				},
				OnCurrentIndexChanged: u.showSelectedDetails,
			},
			TextEdit{
				AssignTo: &u.details,
				ReadOnly: true,
				MinSize:  Size{Height: 190},
				Text:     "Import one or more Codex auth files, then refresh quota.",
			},
		},
	}).Create(u.mw); err != nil {
		return err
	}
	u.detailDialog.Closing().Attach(func(canceled *bool, reason walk.CloseReason) {
		u.detailDialog = nil
		u.table = nil
		u.details = nil
		u.detailActive = nil
		u.aliasEdit = nil
		u.intervalBox = nil
	})
	return nil
}

func (u *appUI) reloadRows() {
	rows := make([]profileRow, 0, len(u.store.Profiles()))
	for _, prof := range u.store.Profiles() {
		rows = append(rows, profileRow{
			Profile:      prof,
			Status:       display.StatusText(display.StatusNotRefreshed),
			Quota:        "-",
			LastRefresh:  "-",
			Details:      "No quota data yet. Click Refresh selected or Refresh all.",
			CompactTitle: "Codex",
			CompactLines: []string{"5h: not refreshed", "weekly: not refreshed"},
		})
	}
	u.model.setRows(rows)
	u.updateMonitor()
}

func (u *appUI) selectedInterval() time.Duration {
	if u.intervalBox == nil {
		return u.pollingInterval
	}
	switch u.intervalBox.CurrentIndex() {
	case 1:
		return time.Minute
	case 2:
		return 5 * time.Minute
	case 3:
		return 10 * time.Minute
	default:
		return 0
	}
}

func (u *appUI) startPolling(interval time.Duration) {
	u.pollingInterval = interval
	u.stopPolling()
	if interval <= 0 || u.mw == nil {
		u.updateMonitor()
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
				u.refreshAll()
			case <-stop:
				return
			}
		}
	}()
	u.updateMonitor()
}

func (u *appUI) stopPolling() {
	u.pollMu.Lock()
	defer u.pollMu.Unlock()
	if u.pollStop != nil {
		close(u.pollStop)
		u.pollStop = nil
	}
}

func (u *appUI) importCurrentAuth() {
	u.importAuthPath(u.activeAuthPath)
}

func (u *appUI) importAuthFile() {
	dlg := walk.FileDialog{
		Title:  "Select Codex auth.json",
		Filter: "JSON files (*.json)|*.json|All files (*.*)|*.*",
	}
	ok, err := dlg.ShowOpen(u.messageOwner())
	if err != nil {
		u.errorBox("Open auth file", err)
		return
	}
	if !ok {
		return
	}
	u.importAuthPath(dlg.FilePath)
}

func (u *appUI) importAuthPath(path string) {
	if u.aliasEdit == nil {
		u.errorBox("Import auth", errors.New("open details before importing auth files"))
		return
	}
	alias := strings.TrimSpace(u.aliasEdit.Text())
	if alias == "" {
		u.errorBox("Import auth", errors.New("enter an alias first, for example company or pro"))
		return
	}
	prof, err := u.store.Import(alias, path)
	if err != nil {
		u.errorBox("Import auth", err)
		return
	}
	u.reloadRows()
	u.updateActiveLabels()
	_ = u.aliasEdit.SetText("")
	walk.MsgBox(u.messageOwner(), "Imported", fmt.Sprintf("Imported profile %q (%s).", prof.Alias, prof.AccountSuffix), walk.MsgBoxOK|walk.MsgBoxIconInformation)
}

func (u *appUI) refreshActive() {
	prof, ok := u.activeProfile()
	if !ok {
		u.openDetailsWindow()
		return
	}
	go u.refreshProfile(prof)
}

func (u *appUI) refreshSelected() {
	row, ok := u.selectedRow()
	if !ok {
		u.errorBox("Refresh selected", errors.New("select a profile first"))
		return
	}
	go u.refreshProfile(row.Profile)
}

func (u *appUI) refreshAll() {
	profiles := u.store.Profiles()
	go func() {
		for _, prof := range profiles {
			u.refreshProfile(prof)
			time.Sleep(500 * time.Millisecond)
		}
	}()
}

func (u *appUI) refreshProfile(prof profile.Profile) {
	u.withUI(func() {
		u.model.update(prof.ID, func(row *profileRow) {
			row.Status = display.StatusText(display.StatusRefreshing)
		})
		u.updateMonitor()
	})

	authFile, err := auth.Load(u.store.AuthPath(prof.ID))
	if err != nil {
		u.setRefreshError(prof.ID, err)
		return
	}
	snapshots, err := u.quotaClient.Fetch(context.Background(), authFile)
	if err != nil {
		u.setRefreshError(prof.ID, err)
		return
	}
	now := time.Now()
	formatted := display.FormatSnapshots(snapshots)
	u.withUI(func() {
		u.model.update(prof.ID, func(row *profileRow) {
			row.Status = display.StatusText(display.StatusOK)
			row.Quota = formatted.Summary
			row.LastRefresh = now.Format("2006-01-02 15:04:05")
			row.Details = formatted.Details
			row.CompactTitle = formatted.CompactTitle
			row.CompactLines = formatted.CompactLines
		})
		u.showSelectedDetails()
		u.updateMonitor()
	})
}

func (u *appUI) setRefreshError(profileID string, err error) {
	u.withUI(func() {
		u.model.update(profileID, func(row *profileRow) {
			row.Status = display.StatusText(display.ClassifyError(err))
			row.Details = err.Error()
		})
		u.showSelectedDetails()
		u.updateMonitor()
	})
}

func (u *appUI) switchSelected() {
	row, ok := u.selectedRow()
	if !ok {
		u.errorBox("Switch account", errors.New("select a profile first"))
		return
	}
	message := fmt.Sprintf("Switch active Codex auth to %q?\n\nCodex must be restarted after switching.", row.Profile.Alias)
	if walk.MsgBox(u.messageOwner(), "Confirm switch", message, walk.MsgBoxYesNo|walk.MsgBoxIconQuestion) != walk.DlgCmdYes {
		return
	}
	sw := switcher.New(u.activeAuthPath, u.store.BackupsDir())
	result, err := sw.Switch(u.store.AuthPath(row.Profile.ID))
	if err != nil {
		u.errorBox("Switch account", err)
		return
	}
	u.updateActiveLabels()
	u.updateMonitor()
	walk.MsgBox(
		u.messageOwner(),
		"Codex auth switched",
		fmt.Sprintf("Active auth was replaced.\n\nBackup: %s\n\nPlease restart Codex for the new account to take effect.", result.BackupPath),
		walk.MsgBoxOK|walk.MsgBoxIconInformation,
	)
}

func (u *appUI) selectedRow() (profileRow, bool) {
	if u.table == nil {
		return profileRow{}, false
	}
	return u.model.row(u.table.CurrentIndex())
}

func (u *appUI) showSelectedDetails() {
	if u.details == nil {
		return
	}
	row, ok := u.selectedRow()
	if !ok {
		_ = u.details.SetText("Select a profile to see quota details.")
		return
	}
	_ = u.details.SetText(row.Details)
}

func (u *appUI) updateActiveLabels() {
	text := "Active Codex account: unavailable"
	active, err := auth.Load(u.activeAuthPath)
	if err == nil {
		if prof, ok := u.store.FindByAccountID(active.Tokens.AccountID); ok {
			text = fmt.Sprintf("Active Codex account: %s (%s)", prof.Alias, prof.AccountSuffix)
		} else {
			text = fmt.Sprintf("Active Codex account: %s", active.AccountSuffix(6))
		}
	}
	if u.detailActive != nil {
		_ = u.detailActive.SetText(text)
	}
}

func (u *appUI) updateMonitor() {
	if u.monitorTitle == nil {
		return
	}

	title := "Codex"
	line1 := "5h: not refreshed"
	line2 := "weekly: not refreshed"
	status := "active: unavailable"

	if active, err := auth.Load(u.activeAuthPath); err == nil {
		status = "active: " + active.AccountSuffix(6)
		if prof, ok := u.store.FindByAccountID(active.Tokens.AccountID); ok {
			title = prof.Alias
			status = "active: " + prof.AccountSuffix
			if row, ok := u.model.rowByProfileID(prof.ID); ok {
				if len(row.CompactLines) > 0 {
					line1 = row.CompactLines[0]
				}
				if len(row.CompactLines) > 1 {
					line2 = row.CompactLines[1]
				}
				status = row.Status
				if row.LastRefresh != "-" {
					status += " | " + row.LastRefresh[11:]
				}
			}
		}
	}
	if u.pollingInterval <= 0 {
		status += " | manual"
	}

	_ = u.monitorTitle.SetText(title)
	_ = u.monitorLine1.SetText(line1)
	_ = u.monitorLine2.SetText(line2)
	_ = u.monitorStatus.SetText(status)
}

func (u *appUI) activeProfile() (profile.Profile, bool) {
	active, err := auth.Load(u.activeAuthPath)
	if err != nil {
		return profile.Profile{}, false
	}
	return u.store.FindByAccountID(active.Tokens.AccountID)
}

func (u *appUI) handleMonitorMouseDown(x, y int, button walk.MouseButton) {
	if button != walk.LeftButton {
		return
	}
	now := time.Now()
	open, nextClick := monitorClickAction(u.lastMonitorClick, now)
	u.lastMonitorClick = nextClick
	if open {
		u.openDetailsWindow()
		return
	}
	u.beginMonitorDrag()
}

func (u *appUI) styleFloatingMonitor() {
	if u.mw == nil {
		return
	}
	hwnd := u.mw.Handle()
	style := uint32(win.GetWindowLong(hwnd, win.GWL_STYLE))
	style &^= uint32(win.WS_CAPTION | win.WS_THICKFRAME | win.WS_SYSMENU | win.WS_MINIMIZEBOX | win.WS_MAXIMIZEBOX)
	style |= uint32(win.WS_POPUP)
	win.SetWindowLong(hwnd, win.GWL_STYLE, int32(style))
	win.SetWindowPos(hwnd, win.HWND_TOPMOST, 0, 0, 0, 0, win.SWP_NOMOVE|win.SWP_NOSIZE|win.SWP_FRAMECHANGED|win.SWP_SHOWWINDOW)
}

func (u *appUI) beginMonitorDrag() {
	if u.mw == nil {
		return
	}
	win.ReleaseCapture()
	win.SendMessage(u.mw.Handle(), win.WM_NCLBUTTONDOWN, uintptr(win.HTCAPTION), 0)
}

func (u *appUI) messageOwner() walk.Form {
	if u.detailDialog != nil && !u.detailDialog.IsDisposed() {
		return u.detailDialog
	}
	return u.mw
}

func (u *appUI) withUI(fn func()) {
	if u.mw == nil {
		return
	}
	u.mw.Synchronize(fn)
}

func (u *appUI) errorBox(title string, err error) {
	walk.MsgBox(u.messageOwner(), title, err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
}

func intervalIndex(interval time.Duration) int {
	switch interval {
	case time.Minute:
		return 1
	case 5 * time.Minute:
		return 2
	case 10 * time.Minute:
		return 3
	default:
		return 0
	}
}

func monitorClickAction(lastClick, now time.Time) (bool, time.Time) {
	if !lastClick.IsZero() && now.Sub(lastClick) <= 450*time.Millisecond {
		return true, time.Time{}
	}
	return false, now
}

func appDataRoot() (string, error) {
	base, err := os.UserConfigDir()
	if err != nil {
		return "", fmt.Errorf("resolve config dir: %w", err)
	}
	return filepath.Join(base, "codex-quota-dock"), nil
}
