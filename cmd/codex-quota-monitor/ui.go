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

	"codex-quota-monitor/internal/auth"
	"codex-quota-monitor/internal/profile"
	"codex-quota-monitor/internal/quota"
	"codex-quota-monitor/internal/settings"
	"codex-quota-monitor/internal/switcher"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

type profileRow struct {
	Profile     profile.Profile
	Status      string
	Quota       string
	LastRefresh string
	Details     string
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
	mw             *walk.MainWindow
	table          *walk.TableView
	details        *walk.TextEdit
	activeLabel    *walk.Label
	aliasEdit      *walk.LineEdit
	intervalBox    *walk.ComboBox
	model          *profileModel
	store          *profile.Store
	quotaClient    quota.Client
	activeAuthPath string

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
		model:          &profileModel{},
		store:          store,
		quotaClient:    quota.DefaultClient(),
		activeAuthPath: activeAuthPath,
	}
	ui.reloadRows()

	if err := ui.createWindow(); err != nil {
		return err
	}
	ui.refreshActiveLabel()
	ui.startPolling(settings.DefaultPollingInterval())
	ui.mw.Run()
	ui.stopPolling()
	return nil
}

func (u *appUI) createWindow() error {
	intervals := []string{"Off", "1 minute", "5 minutes", "10 minutes"}

	return MainWindow{
		AssignTo: &u.mw,
		Title:    "Codex Quota Monitor",
		Size:     Size{Width: 980, Height: 640},
		Layout:   VBox{Margins: Margins{Left: 10, Top: 10, Right: 10, Bottom: 10}},
		Children: []Widget{
			Composite{
				Layout: HBox{},
				Children: []Widget{
					Label{AssignTo: &u.activeLabel, Text: "Active Codex account: checking..."},
					HSpacer{},
					Label{Text: "Auto refresh"},
					ComboBox{
						AssignTo:     &u.intervalBox,
						Model:        intervals,
						CurrentIndex: 2,
						OnCurrentIndexChanged: func() {
							u.startPolling(u.selectedInterval())
						},
					},
				},
			},
			Composite{
				Layout: HBox{},
				Children: []Widget{
					Label{Text: "Alias"},
					LineEdit{AssignTo: &u.aliasEdit, ToolTipText: "Example: company or pro"},
					PushButton{Text: "Import current auth", OnClicked: u.importCurrentAuth},
					PushButton{Text: "Import auth file", OnClicked: u.importAuthFile},
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
					{Title: "Alias", Width: 150},
					{Title: "Auth", Width: 90},
					{Title: "Account", Width: 90},
					{Title: "Status", Width: 150},
					{Title: "Quota", Width: 220},
					{Title: "Last refresh", Width: 170},
				},
				OnCurrentIndexChanged: u.showSelectedDetails,
			},
			TextEdit{
				AssignTo: &u.details,
				ReadOnly: true,
				MinSize:  Size{Height: 180},
				Text:     "Import one or more Codex auth files, then use Refresh selected or Refresh all.",
			},
		},
	}.Create()
}

func (u *appUI) reloadRows() {
	rows := make([]profileRow, 0, len(u.store.Profiles()))
	for _, prof := range u.store.Profiles() {
		rows = append(rows, profileRow{
			Profile:     prof,
			Status:      "not refreshed",
			Quota:       "-",
			LastRefresh: "-",
			Details:     "No quota data yet. Click Refresh selected or Refresh all.",
		})
	}
	u.model.setRows(rows)
}

func (u *appUI) selectedInterval() time.Duration {
	if u.intervalBox == nil {
		return settings.DefaultPollingInterval()
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
	u.stopPolling()
	if interval <= 0 || u.mw == nil {
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
	ok, err := dlg.ShowOpen(u.mw)
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
	u.refreshActiveLabel()
	_ = u.aliasEdit.SetText("")
	walk.MsgBox(u.mw, "Imported", fmt.Sprintf("Imported profile %q (%s).", prof.Alias, prof.AccountSuffix), walk.MsgBoxOK|walk.MsgBoxIconInformation)
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
			row.Status = "refreshing"
		})
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
	now := time.Now().Format("2006-01-02 15:04:05")
	summary, details := formatSnapshots(snapshots)
	u.withUI(func() {
		u.model.update(prof.ID, func(row *profileRow) {
			row.Status = "ok"
			row.Quota = summary
			row.LastRefresh = now
			row.Details = details
		})
		u.showSelectedDetails()
	})
}

func (u *appUI) setRefreshError(profileID string, err error) {
	u.withUI(func() {
		u.model.update(profileID, func(row *profileRow) {
			row.Status = classifyError(err)
			row.Details = err.Error()
		})
		u.showSelectedDetails()
	})
}

func (u *appUI) switchSelected() {
	row, ok := u.selectedRow()
	if !ok {
		u.errorBox("Switch account", errors.New("select a profile first"))
		return
	}
	message := fmt.Sprintf("Switch active Codex auth to %q?\n\nCodex must be restarted after switching.", row.Profile.Alias)
	if walk.MsgBox(u.mw, "Confirm switch", message, walk.MsgBoxYesNo|walk.MsgBoxIconQuestion) != walk.DlgCmdYes {
		return
	}
	sw := switcher.New(u.activeAuthPath, u.store.BackupsDir())
	result, err := sw.Switch(u.store.AuthPath(row.Profile.ID))
	if err != nil {
		u.errorBox("Switch account", err)
		return
	}
	u.refreshActiveLabel()
	walk.MsgBox(
		u.mw,
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
	row, ok := u.selectedRow()
	if !ok {
		_ = u.details.SetText("Select a profile to see quota details.")
		return
	}
	_ = u.details.SetText(row.Details)
}

func (u *appUI) refreshActiveLabel() {
	active, err := auth.Load(u.activeAuthPath)
	if err != nil {
		_ = u.activeLabel.SetText("Active Codex account: unavailable")
		return
	}
	if prof, ok := u.store.FindByAccountID(active.Tokens.AccountID); ok {
		_ = u.activeLabel.SetText(fmt.Sprintf("Active Codex account: %s (%s)", prof.Alias, prof.AccountSuffix))
		return
	}
	_ = u.activeLabel.SetText(fmt.Sprintf("Active Codex account: %s", active.AccountSuffix(6)))
}

func (u *appUI) withUI(fn func()) {
	if u.mw == nil {
		return
	}
	u.mw.Synchronize(fn)
}

func (u *appUI) errorBox(title string, err error) {
	walk.MsgBox(u.mw, title, err.Error(), walk.MsgBoxOK|walk.MsgBoxIconError)
}

func formatSnapshots(snapshots []quota.Snapshot) (string, string) {
	if len(snapshots) == 0 {
		return "quota unavailable", "No quota windows were returned."
	}

	lines := make([]string, 0, len(snapshots)*4)
	summaryParts := []string{}
	for _, snapshot := range snapshots {
		name := snapshot.LimitName
		if name == "" {
			name = snapshot.LimitID
		}
		if name == "" {
			name = "codex"
		}
		lines = append(lines, fmt.Sprintf("%s (%s)", name, snapshot.PlanType))
		if snapshot.RateLimitReachedType != "" {
			lines = append(lines, "  reached: "+snapshot.RateLimitReachedType)
		}
		if snapshot.Primary != nil {
			line := formatWindow("primary", snapshot.Primary)
			lines = append(lines, "  "+line)
			if snapshot.LimitID == "codex" {
				summaryParts = append(summaryParts, line)
			}
		}
		if snapshot.Secondary != nil {
			line := formatWindow("secondary", snapshot.Secondary)
			lines = append(lines, "  "+line)
			if snapshot.LimitID == "codex" {
				summaryParts = append(summaryParts, line)
			}
		}
		if snapshot.Credits != nil && snapshot.Credits.HasCredits {
			balance := snapshot.Credits.Balance
			if balance == "" && snapshot.Credits.Unlimited {
				balance = "unlimited"
			}
			lines = append(lines, "  credits: "+balance)
		}
	}
	if len(summaryParts) == 0 {
		summaryParts = append(summaryParts, "quota unavailable")
	}
	return strings.Join(summaryParts, "; "), strings.Join(lines, "\r\n")
}

func formatWindow(prefix string, window *quota.Window) string {
	label := quota.WindowLabel(window.WindowMinutes)
	reset := "-"
	if window.ResetsAt > 0 {
		reset = time.Unix(window.ResetsAt, 0).Local().Format("2006-01-02 15:04")
	}
	return fmt.Sprintf("%s %s: %.1f%% used, %.1f%% left, resets %s", prefix, label, window.UsedPercent, quota.RemainingPercent(window.UsedPercent), reset)
}

func classifyError(err error) string {
	switch {
	case errors.Is(err, quota.ErrUnauthorized):
		return "auth expired"
	case errors.Is(err, quota.ErrRateLimited):
		return "check limited"
	case errors.Is(err, quota.ErrInvalidResponse):
		return "bad response"
	default:
		return "error"
	}
}

func appDataRoot() (string, error) {
	base, err := os.UserConfigDir()
	if err != nil {
		return "", fmt.Errorf("resolve config dir: %w", err)
	}
	return filepath.Join(base, "codex-quota-monitor"), nil
}
