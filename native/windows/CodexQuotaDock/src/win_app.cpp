#include "win_app.h"

#include <CommCtrl.h>
#include <Dwmapi.h>
#include <Shellapi.h>
#include <commdlg.h>

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

namespace cqd {
namespace {

constexpr wchar_t kMonitorClass[] = L"CodexQuotaDockNativeMonitor";
constexpr wchar_t kSettingsClass[] = L"CodexQuotaDockNativeSettings";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kRefreshComplete = WM_APP + 2;
constexpr UINT_PTR kPollTimer = 42;
constexpr int kVersionMajor = 0;
constexpr int kVersionMinor = 6;
constexpr int kVersionPatch = 0;
constexpr const char* kVersion = "0.6.0-dev";

enum ControlId {
    ID_PROFILE_COMBO = 1001,
    ID_REFRESH = 1002,
    ID_SWITCH = 1003,
    ID_SETTINGS = 1004,

    ID_PROFILE_LIST = 2001,
    ID_ALIAS_EDIT = 2002,
    ID_AUTH_EDIT = 2003,
    ID_IMPORT_CURRENT = 2004,
    ID_IMPORT_FILE = 2005,
    ID_NEW_PROFILE = 2006,
    ID_SAVE_PROFILE = 2007,
    ID_DELETE_PROFILE = 2008,
    ID_PIN_PROFILE = 2009,
    ID_SWITCH_PROFILE = 2010,
    ID_EXPORT_BACKUP = 2011,
    ID_IMPORT_BACKUP = 2012,
    ID_RESTORE_BACKUP = 2013,
    ID_POLL_COMBO = 2014,
    ID_FIVE_COMBO = 2015,
    ID_WEEKLY_COMBO = 2016,
    ID_AUTO_RESTART = 2017,
    ID_STARTUP = 2018,
    ID_SAVE_SETTINGS = 2019,
    ID_CHECK_UPDATES = 2020,
    ID_HEALTH_USAGE = 2021,
    ID_STATUS_TEXT = 2022,
};

std::wstring label(std::string_view value) {
    return utf8ToWide(value);
}

void addComboItem(HWND combo, const wchar_t* text, int value) {
    LRESULT index = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(index), static_cast<LPARAM>(value));
}

int selectedComboValue(HWND combo, int fallback) {
    LRESULT index = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR) return fallback;
    LRESULT value = SendMessageW(combo, CB_GETITEMDATA, static_cast<WPARAM>(index), 0);
    return value == CB_ERR ? fallback : static_cast<int>(value);
}

void selectComboValue(HWND combo, int value) {
    int count = static_cast<int>(SendMessageW(combo, CB_GETCOUNT, 0, 0));
    for (int i = 0; i < count; ++i) {
        if (static_cast<int>(SendMessageW(combo, CB_GETITEMDATA, i, 0)) == value) {
            SendMessageW(combo, CB_SETCURSEL, i, 0);
            return;
        }
    }
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
}

std::string openFile(HWND owner, const wchar_t* filter) {
    wchar_t path[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return {};
    return wideToUtf8(path);
}

std::string saveFile(HWND owner, const wchar_t* filter, const wchar_t* defaultName) {
    wchar_t path[MAX_PATH]{};
    wcscpy_s(path, defaultName);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return {};
    return wideToUtf8(path);
}

std::string formatWindow(const QuotaWindow& window) {
    std::ostringstream out;
    out << window.label << " ";
    if (window.remainingPercent) out << *window.remainingPercent << "%";
    else out << "--";
    return out.str();
}

void drawTextUtf8(HDC dc, std::string_view text, RECT rect, UINT format) {
    std::wstring wide = utf8ToWide(text);
    DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &rect, format);
}

} // namespace

int NativeWindowsApp::run(HINSTANCE instance, int showCommand) {
    instance_ = instance;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES};
    InitCommonControlsEx(&controls);

    loadState();
    createMonitorWindow(showCommand);
    createTrayIcon();
    refreshMonitorRows(false);
    SetTimer(monitor_, kPollTimer, static_cast<UINT>(settings_.pollIntervalMinutes * 60 * 1000), nullptr);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    removeTrayIcon();
    return static_cast<int>(message.wParam);
}

void NativeWindowsApp::loadState() {
    settings_ = loadSettings(configRoot());
    settings_.startAtLogin = startupEnabled();
    store_.load();
    if (!store_.profiles().empty()) selectedProfileId_ = store_.profiles().front().id;
}

void NativeWindowsApp::createMonitorWindow(int showCommand) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = MonitorProc;
    wc.hInstance = instance_;
    wc.lpszClassName = kMonitorClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassW(&wc);

    monitor_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kMonitorClass,
        L"Codex Quota Dock Native",
        WS_POPUP,
        80,
        120,
        430,
        245,
        nullptr,
        nullptr,
        instance_,
        this
    );
    SetLayeredWindowAttributes(monitor_, RGB(24, 24, 24), 240, LWA_ALPHA);
    applyWindows11Style(monitor_, true);
    ShowWindow(monitor_, showCommand);
}

void NativeWindowsApp::createSettingsWindow() {
    if (settingsWindow_) {
        ShowWindow(settingsWindow_, SW_SHOWNORMAL);
        SetForegroundWindow(settingsWindow_);
        return;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = instance_;
    wc.lpszClassName = kSettingsClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    settingsWindow_ = CreateWindowExW(
        0,
        kSettingsClass,
        L"Codex Quota Dock - Settings",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        160,
        160,
        1040,
        720,
        nullptr,
        nullptr,
        instance_,
        this
    );
    applyWindows11Style(settingsWindow_, false);
    ShowWindow(settingsWindow_, SW_SHOWNORMAL);
}

void NativeWindowsApp::createTrayIcon() {
    tray_ = {};
    tray_.cbSize = sizeof(tray_);
    tray_.hWnd = monitor_;
    tray_.uID = 1;
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tray_.uCallbackMessage = kTrayMessage;
    tray_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(tray_.szTip, L"Codex Quota Dock");
    Shell_NotifyIconW(NIM_ADD, &tray_);
}

void NativeWindowsApp::removeTrayIcon() {
    if (tray_.cbSize) Shell_NotifyIconW(NIM_DELETE, &tray_);
}

void NativeWindowsApp::applyWindows11Style(HWND hwnd, bool floating) {
    const int rounded = 2;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &rounded, sizeof(rounded));
    const int backdrop = floating ? 3 : 2;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
}

LRESULT CALLBACK NativeWindowsApp::MonitorProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    NativeWindowsApp* app = reinterpret_cast<NativeWindowsApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        app = reinterpret_cast<NativeWindowsApp*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return app ? app->handleMonitor(hwnd, message, wparam, lparam) : DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT NativeWindowsApp::handleMonitor(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 18, 166, 150, 140, hwnd, reinterpret_cast<HMENU>(ID_PROFILE_COMBO), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE, 176, 166, 74, 28, hwnd, reinterpret_cast<HMENU>(ID_REFRESH), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Switch", WS_CHILD | WS_VISIBLE, 256, 166, 70, 28, hwnd, reinterpret_cast<HMENU>(ID_SWITCH), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Config", WS_CHILD | WS_VISIBLE, 332, 166, 70, 28, hwnd, reinterpret_cast<HMENU>(ID_SETTINGS), instance_, nullptr);
        updateProfileCombo();
        return 0;
    case WM_NCHITTEST:
        return HTCAPTION;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_PROFILE_COMBO:
            if (HIWORD(wparam) == CBN_SELCHANGE) selectProfileByIndex(static_cast<int>(SendMessageW(control(ID_PROFILE_COMBO), CB_GETCURSEL, 0, 0)));
            break;
        case ID_REFRESH:
            refreshMonitorRows(true);
            break;
        case ID_SWITCH:
            switchSelectedProfile();
            break;
        case ID_SETTINGS:
            createSettingsWindow();
            break;
        }
        return 0;
    case WM_TIMER:
        if (wparam == kPollTimer) refreshMonitorRows(true);
        return 0;
    case kTrayMessage:
        if (lparam == WM_LBUTTONUP || lparam == WM_RBUTTONUP) {
            ShowWindow(hwnd, IsWindowVisible(hwnd) ? SW_HIDE : SW_SHOWNORMAL);
        }
        return 0;
    case WM_PAINT:
        paintMonitor(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK NativeWindowsApp::SettingsProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    NativeWindowsApp* app = reinterpret_cast<NativeWindowsApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        app = reinterpret_cast<NativeWindowsApp*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return app ? app->handleSettings(hwnd, message, wparam, lparam) : DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT NativeWindowsApp::handleSettings(HWND hwnd, UINT message, WPARAM wparam, LPARAM) {
    switch (message) {
    case WM_CREATE: {
        CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY, 16, 16, 240, 460, hwnd, reinterpret_cast<HMENU>(ID_PROFILE_LIST), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Import Current", WS_CHILD | WS_VISIBLE, 16, 488, 112, 28, hwnd, reinterpret_cast<HMENU>(ID_IMPORT_CURRENT), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Import File", WS_CHILD | WS_VISIBLE, 140, 488, 112, 28, hwnd, reinterpret_cast<HMENU>(ID_IMPORT_FILE), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Pin", WS_CHILD | WS_VISIBLE, 16, 524, 72, 28, hwnd, reinterpret_cast<HMENU>(ID_PIN_PROFILE), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE, 96, 524, 72, 28, hwnd, reinterpret_cast<HMENU>(ID_DELETE_PROFILE), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Switch", WS_CHILD | WS_VISIBLE, 176, 524, 76, 28, hwnd, reinterpret_cast<HMENU>(ID_SWITCH_PROFILE), instance_, nullptr);

        CreateWindowW(L"STATIC", L"Alias", WS_CHILD | WS_VISIBLE, 276, 18, 70, 22, hwnd, nullptr, instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 348, 16, 300, 24, hwnd, reinterpret_cast<HMENU>(ID_ALIAS_EDIT), instance_, nullptr);
        CreateWindowW(L"STATIC", L"Auth JSON", WS_CHILD | WS_VISIBLE, 276, 52, 120, 22, hwnd, nullptr, instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL, 276, 76, 455, 300, hwnd, reinterpret_cast<HMENU>(ID_AUTH_EDIT), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"New Profile", WS_CHILD | WS_VISIBLE, 276, 386, 100, 28, hwnd, reinterpret_cast<HMENU>(ID_NEW_PROFILE), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Save Profile", WS_CHILD | WS_VISIBLE, 384, 386, 100, 28, hwnd, reinterpret_cast<HMENU>(ID_SAVE_PROFILE), instance_, nullptr);

        CreateWindowW(L"BUTTON", L"Export Backup", WS_CHILD | WS_VISIBLE, 276, 430, 112, 28, hwnd, reinterpret_cast<HMENU>(ID_EXPORT_BACKUP), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Import Backup", WS_CHILD | WS_VISIBLE, 396, 430, 112, 28, hwnd, reinterpret_cast<HMENU>(ID_IMPORT_BACKUP), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Restore Backup", WS_CHILD | WS_VISIBLE, 516, 430, 112, 28, hwnd, reinterpret_cast<HMENU>(ID_RESTORE_BACKUP), instance_, nullptr);

        CreateWindowW(L"STATIC", L"Poll", WS_CHILD | WS_VISIBLE, 756, 18, 60, 22, hwnd, nullptr, instance_, nullptr);
        HWND poll = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 820, 16, 110, 120, hwnd, reinterpret_cast<HMENU>(ID_POLL_COMBO), instance_, nullptr);
        addComboItem(poll, L"1 min", 1); addComboItem(poll, L"5 min", 5); addComboItem(poll, L"10 min", 10);
        CreateWindowW(L"STATIC", L"5h Alert", WS_CHILD | WS_VISIBLE, 756, 54, 60, 22, hwnd, nullptr, instance_, nullptr);
        HWND five = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 820, 52, 110, 180, hwnd, reinterpret_cast<HMENU>(ID_FIVE_COMBO), instance_, nullptr);
        CreateWindowW(L"STATIC", L"Weekly", WS_CHILD | WS_VISIBLE, 756, 90, 60, 22, hwnd, nullptr, instance_, nullptr);
        HWND weekly = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 820, 88, 110, 180, hwnd, reinterpret_cast<HMENU>(ID_WEEKLY_COMBO), instance_, nullptr);
        for (int value : {0, 5, 10, 15, 20, 30, 40}) {
            std::wstring text = value == 0 ? L"Off" : std::to_wstring(value) + L"%";
            addComboItem(five, text.c_str(), value);
            addComboItem(weekly, text.c_str(), value);
        }
        CreateWindowW(L"BUTTON", L"Restart Codex after switch", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 756, 126, 220, 24, hwnd, reinterpret_cast<HMENU>(ID_AUTO_RESTART), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Start at login", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 756, 154, 160, 24, hwnd, reinterpret_cast<HMENU>(ID_STARTUP), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Save Settings", WS_CHILD | WS_VISIBLE, 756, 190, 120, 28, hwnd, reinterpret_cast<HMENU>(ID_SAVE_SETTINGS), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Check Updates", WS_CHILD | WS_VISIBLE, 884, 190, 120, 28, hwnd, reinterpret_cast<HMENU>(ID_CHECK_UPDATES), instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 756, 236, 250, 300, hwnd, reinterpret_cast<HMENU>(ID_HEALTH_USAGE), instance_, nullptr);
        CreateWindowW(L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE, 276, 632, 730, 22, hwnd, reinterpret_cast<HMENU>(ID_STATUS_TEXT), instance_, nullptr);

        selectComboValue(poll, settings_.pollIntervalMinutes);
        selectComboValue(five, settings_.fiveHourAlertThreshold);
        selectComboValue(weekly, settings_.weeklyAlertThreshold);
        SendMessageW(control(ID_AUTO_RESTART), BM_SETCHECK, settings_.autoRestartCodex ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(control(ID_STARTUP), BM_SETCHECK, settings_.startAtLogin ? BST_CHECKED : BST_UNCHECKED, 0);
        updateProfileList();
        updateHealthAndUsageText();
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_PROFILE_LIST && HIWORD(wparam) == LBN_SELCHANGE) {
            selectProfileByIndex(static_cast<int>(SendMessageW(control(ID_PROFILE_LIST), LB_GETCURSEL, 0, 0)));
            loadSelectedProfileEditor();
            return 0;
        }
        switch (LOWORD(wparam)) {
        case ID_IMPORT_CURRENT: importCurrentProfile(); break;
        case ID_IMPORT_FILE: importProfileFile(); break;
        case ID_NEW_PROFILE: newProfileFromEditor(); break;
        case ID_SAVE_PROFILE: saveSelectedProfile(); break;
        case ID_DELETE_PROFILE: deleteSelectedProfile(); break;
        case ID_PIN_PROFILE: togglePinnedProfile(); break;
        case ID_SWITCH_PROFILE: switchSelectedProfile(); break;
        case ID_EXPORT_BACKUP: exportBackupFile(); break;
        case ID_IMPORT_BACKUP: importBackupFile(); break;
        case ID_RESTORE_BACKUP: restoreLatestBackup(); break;
        case ID_SAVE_SETTINGS: saveSettingsFromControls(); break;
        case ID_CHECK_UPDATES: checkUpdates(); break;
        }
        return 0;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        settingsWindow_ = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, 0);
}

void NativeWindowsApp::saveSettingsFromControls() {
    settings_.pollIntervalMinutes = selectedComboValue(control(ID_POLL_COMBO), 5);
    settings_.fiveHourAlertThreshold = selectedComboValue(control(ID_FIVE_COMBO), 10);
    settings_.weeklyAlertThreshold = selectedComboValue(control(ID_WEEKLY_COMBO), 30);
    settings_.autoRestartCodex = SendMessageW(control(ID_AUTO_RESTART), BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings_.startAtLogin = SendMessageW(control(ID_STARTUP), BM_GETCHECK, 0, 0) == BST_CHECKED;
    try {
        saveSettings(configRoot(), settings_);
        setStartupEnabled(settings_.startAtLogin);
        KillTimer(monitor_, kPollTimer);
        SetTimer(monitor_, kPollTimer, static_cast<UINT>(settings_.pollIntervalMinutes * 60 * 1000), nullptr);
        showStatus("Settings saved.");
    } catch (const std::exception& error) {
        showError("Save settings", error);
    }
}

void NativeWindowsApp::refreshMonitorRows(bool fetchQuotaValues) {
    monitorRows_.clear();
    for (const Profile& profile : monitorProfiles()) {
        MonitorRow row;
        row.profile = profile;
        row.quota.fiveHour.label = "5h";
        row.quota.weekly.label = "weekly";
        if (fetchQuotaValues) {
            try {
                row.quota = fetchQuota(store_.readAuth(profile.id));
            } catch (const std::exception& error) {
                row.quota.error = error.what();
            }
        }
        monitorRows_.push_back(std::move(row));
    }
    status_ = fetchQuotaValues ? "Refreshed" : "Ready";
    InvalidateRect(monitor_, nullptr, TRUE);
    updateHealthAndUsageText();
}

void NativeWindowsApp::updateProfileCombo() {
    HWND combo = control(ID_PROFILE_COMBO);
    if (!combo) return;
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    int selected = 0;
    int i = 0;
    for (const auto& profile : store_.profiles()) {
        std::wstring item = utf8ToWide(profile.alias);
        LRESULT index = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
        SendMessageW(combo, CB_SETITEMDATA, index, i);
        if (profile.id == selectedProfileId_) selected = i;
        ++i;
    }
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

void NativeWindowsApp::updateProfileList() {
    HWND list = control(ID_PROFILE_LIST);
    if (!list) return;
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    int selected = 0;
    int i = 0;
    for (const auto& profile : store_.profiles()) {
        std::string text = profile.alias + (profile.pinned ? "  [pin]" : "");
        LRESULT index = SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(utf8ToWide(text).c_str()));
        SendMessageW(list, LB_SETITEMDATA, index, i);
        if (profile.id == selectedProfileId_) selected = i;
        ++i;
    }
    SendMessageW(list, LB_SETCURSEL, selected, 0);
    loadSelectedProfileEditor();
}

void NativeWindowsApp::loadSelectedProfileEditor() {
    Profile* profile = selectedProfile();
    if (!profile) {
        setControlText(ID_ALIAS_EDIT, "");
        setControlText(ID_AUTH_EDIT, "");
        return;
    }
    setControlText(ID_ALIAS_EDIT, profile->alias);
    try {
        setControlText(ID_AUTH_EDIT, store_.readAuth(profile->id));
    } catch (...) {
        setControlText(ID_AUTH_EDIT, "");
    }
}

void NativeWindowsApp::updateHealthAndUsageText() {
    HWND edit = control(ID_HEALTH_USAGE);
    if (!edit) return;
    std::ostringstream out;
    out << "Health\n";
    for (const auto& row : runHealthCheck(store_, defaultCodexAuthPath(), kVersion)) {
        out << row.status << " - " << row.label << ": " << row.detail << "\r\n";
    }
    LocalUsageSummary usage = scanLocalUsage(defaultCodexRoot());
    out << "\r\nLocal Usage\n";
    out << "Today: " << usage.today.total << " tokens\r\n";
    out << "7 days: " << usage.last7Days.total << " tokens\r\n";
    out << "30 days: " << usage.last30Days.total << " tokens\r\n";
    out << "Total: " << usage.total.total << " tokens\r\n";
    out << "Sessions: " << usage.sessionCount << "\r\n";
    if (usage.parseErrors) out << "Parse errors: " << usage.parseErrors << "\r\n";
    setControlText(ID_HEALTH_USAGE, out.str());
}

void NativeWindowsApp::paintMonitor(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);
    RECT client{};
    GetClientRect(hwnd, &client);
    HBRUSH background = CreateSolidBrush(RGB(28, 32, 38));
    FillRect(dc, &client, background);
    DeleteObject(background);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(245, 247, 250));
    RECT title{18, 14, 220, 36};
    drawTextUtf8(dc, "Codex Quota", title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(dc, RGB(172, 180, 190));
    RECT status{230, 14, 408, 36};
    drawTextUtf8(dc, status_, status, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    int y = 48;
    if (monitorRows_.empty()) {
        RECT empty{18, 68, 408, 130};
        drawTextUtf8(dc, "No active or pinned profiles", empty, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (const MonitorRow& row : monitorRows_) {
        HBRUSH rowBrush = CreateSolidBrush(row.quota.belowThreshold(settings_.fiveHourAlertThreshold, settings_.weeklyAlertThreshold) ? RGB(72, 28, 34) : RGB(42, 48, 56));
        RECT box{18, y, 408, y + 48};
        FillRect(dc, &box, rowBrush);
        DeleteObject(rowBrush);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT alias{28, y + 5, 240, y + 22};
        drawTextUtf8(dc, row.profile.alias + (row.profile.pinned ? "  pin" : ""), alias, DT_LEFT | DT_SINGLELINE);
        SetTextColor(dc, RGB(190, 199, 210));
        RECT quota{28, y + 25, 398, y + 44};
        std::string text = row.quota.error.empty()
            ? formatWindow(row.quota.fiveHour) + "    " + formatWindow(row.quota.weekly)
            : row.quota.error;
        drawTextUtf8(dc, text, quota, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += 56;
        if (y > 150) break;
    }
    EndPaint(hwnd, &ps);
}

void NativeWindowsApp::selectProfileByIndex(int index) {
    if (index < 0 || index >= static_cast<int>(store_.profiles().size())) return;
    selectedProfileId_ = store_.profiles()[static_cast<size_t>(index)].id;
    updateProfileCombo();
    updateProfileList();
}

void NativeWindowsApp::importCurrentProfile() {
    try {
        std::string auth = readTextFile(defaultCodexAuthPath());
        std::string alias = store_.suggestAlias("current", auth);
        Profile profile = store_.importAuth(alias, auth, true);
        selectedProfileId_ = profile.id;
        updateProfileCombo();
        updateProfileList();
        refreshMonitorRows(false);
        showStatus("Imported current auth.");
    } catch (const std::exception& error) {
        showError("Import current", error);
    }
}

void NativeWindowsApp::importProfileFile() {
    std::string path = openFile(settingsWindow_, L"JSON files\0*.json\0All files\0*.*\0");
    if (path.empty()) return;
    try {
        std::string auth = readTextFile(fs::path(utf8ToWide(path)));
        std::string alias = store_.suggestAlias(wideToUtf8(fs::path(utf8ToWide(path)).stem().wstring()), auth);
        Profile profile = store_.importAuth(alias, auth, true);
        selectedProfileId_ = profile.id;
        updateProfileCombo();
        updateProfileList();
        refreshMonitorRows(false);
        showStatus("Imported auth file.");
    } catch (const std::exception& error) {
        showError("Import file", error);
    }
}

void NativeWindowsApp::newProfileFromEditor() {
    try {
        Profile profile = store_.importAuth(controlText(ID_ALIAS_EDIT), controlText(ID_AUTH_EDIT), true);
        selectedProfileId_ = profile.id;
        updateProfileCombo();
        updateProfileList();
        refreshMonitorRows(false);
        showStatus("Profile created.");
    } catch (const std::exception& error) {
        showError("New profile", error);
    }
}

void NativeWindowsApp::saveSelectedProfile() {
    try {
        Profile* profile = selectedProfile();
        if (!profile) throw std::runtime_error("select a profile first");
        Profile updated = store_.updateProfile(profile->id, controlText(ID_ALIAS_EDIT), controlText(ID_AUTH_EDIT));
        selectedProfileId_ = updated.id;
        updateProfileCombo();
        updateProfileList();
        refreshMonitorRows(false);
        showStatus("Profile saved.");
    } catch (const std::exception& error) {
        showError("Save profile", error);
    }
}

void NativeWindowsApp::deleteSelectedProfile() {
    try {
        Profile* profile = selectedProfile();
        if (!profile) throw std::runtime_error("select a profile first");
        if (MessageBoxW(settingsWindow_, L"Delete selected profile?", L"Codex Quota Dock", MB_YESNO | MB_ICONWARNING) != IDYES) return;
        store_.deleteProfile(profile->id);
        selectedProfileId_ = store_.profiles().empty() ? "" : store_.profiles().front().id;
        updateProfileCombo();
        updateProfileList();
        refreshMonitorRows(false);
        showStatus("Profile deleted.");
    } catch (const std::exception& error) {
        showError("Delete profile", error);
    }
}

void NativeWindowsApp::togglePinnedProfile() {
    try {
        Profile* profile = selectedProfile();
        if (!profile) throw std::runtime_error("select a profile first");
        store_.setPinned(profile->id, !profile->pinned);
        updateProfileList();
        refreshMonitorRows(false);
    } catch (const std::exception& error) {
        showError("Pin profile", error);
    }
}

void NativeWindowsApp::switchSelectedProfile() {
    try {
        Profile* profile = selectedProfile();
        if (!profile) throw std::runtime_error("select a profile first");
        if (MessageBoxW(settingsWindow_ ? settingsWindow_ : monitor_, L"Switch Codex auth to this profile?", L"Codex Quota Dock", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
        SwitchResult result = switchAuth(defaultCodexAuthPath(), store_.authPath(profile->id), store_.backupsDir());
        std::string restart = settings_.autoRestartCodex ? restartCodex() : "Restart Codex to use the new auth.";
        std::string text = "Switched to " + profile->alias + "\nBackup: " + wideToUtf8(result.backupPath.wstring()) + "\n" + restart;
        MessageBoxW(settingsWindow_ ? settingsWindow_ : monitor_, utf8ToWide(text).c_str(), L"Codex Quota Dock", MB_OK | MB_ICONINFORMATION);
        refreshMonitorRows(false);
    } catch (const std::exception& error) {
        showError("Switch profile", error);
    }
}

void NativeWindowsApp::exportBackupFile() {
    std::string path = saveFile(settingsWindow_, L"JSON files\0*.json\0All files\0*.*\0", L"codex-quota-dock-backup.json");
    if (path.empty()) return;
    try {
        writeTextFileAtomic(fs::path(utf8ToWide(path)), exportBackup(store_, settings_));
        showStatus("Backup exported.");
    } catch (const std::exception& error) {
        showError("Export backup", error);
    }
}

void NativeWindowsApp::importBackupFile() {
    std::string path = openFile(settingsWindow_, L"JSON files\0*.json\0All files\0*.*\0");
    if (path.empty()) return;
    try {
        BackupImportSummary summary = importBackup(store_, readTextFile(fs::path(utf8ToWide(path))));
        selectedProfileId_ = store_.profiles().empty() ? "" : store_.profiles().front().id;
        updateProfileCombo();
        updateProfileList();
        refreshMonitorRows(false);
        showStatus("Backup imported: " + std::to_string(summary.created) + " created, " + std::to_string(summary.updated) + " updated.");
    } catch (const std::exception& error) {
        showError("Import backup", error);
    }
}

void NativeWindowsApp::restoreLatestBackup() {
    try {
        auto backup = latestBackup(store_.backupsDir());
        if (!backup) throw std::runtime_error("no backup found");
        restoreBackup(*backup, defaultCodexAuthPath());
        showStatus("Latest auth backup restored.");
    } catch (const std::exception& error) {
        showError("Restore backup", error);
    }
}

void NativeWindowsApp::checkUpdates() {
    try {
        UpdateCheckResult result = cqd::checkForUpdates(kVersion);
        std::string text = result.available
            ? "Update available: " + result.latest + "\nAsset: " + result.asset.name
            : "No installable update: " + result.reason;
        if (!result.releaseUrl.empty()) text += "\n\nOpen GitHub Releases?";
        int choice = MessageBoxW(settingsWindow_, utf8ToWide(text).c_str(), L"Codex Quota Dock", result.releaseUrl.empty() ? MB_OK : MB_YESNO);
        if (choice == IDYES && !result.releaseUrl.empty()) {
            ShellExecuteW(settingsWindow_, L"open", utf8ToWide(result.releaseUrl).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    } catch (const std::exception& error) {
        showError("Check updates", error);
    }
}

Profile* NativeWindowsApp::selectedProfile() {
    for (auto& profile : const_cast<std::vector<Profile>&>(store_.profiles())) {
        if (profile.id == selectedProfileId_) return &profile;
    }
    return nullptr;
}

std::string NativeWindowsApp::selectedProfileId() const {
    return selectedProfileId_;
}

std::string NativeWindowsApp::activeAccountId() const {
    try {
        return parseAuthMetadata(readTextFile(defaultCodexAuthPath()), false).accountId;
    } catch (...) {
        return {};
    }
}

std::vector<Profile> NativeWindowsApp::monitorProfiles() const {
    std::string active = activeAccountId();
    std::vector<Profile> out;
    for (const auto& profile : store_.profiles()) {
        if (profile.pinned || (!active.empty() && profile.accountId == active)) {
            out.push_back(profile);
        }
    }
    if (out.empty() && !store_.profiles().empty()) out.push_back(store_.profiles().front());
    if (out.size() > 2) out.resize(2);
    return out;
}

void NativeWindowsApp::showStatus(std::string message) {
    status_ = std::move(message);
    setControlText(ID_STATUS_TEXT, status_);
    InvalidateRect(monitor_, nullptr, TRUE);
}

void NativeWindowsApp::showError(const std::string& context, const std::exception& error) {
    std::string text = context + ": " + error.what();
    showStatus(text);
    MessageBoxW(settingsWindow_ ? settingsWindow_ : monitor_, utf8ToWide(text).c_str(), L"Codex Quota Dock", MB_OK | MB_ICONERROR);
}

std::string NativeWindowsApp::controlText(int id) const {
    HWND hwnd = control(id);
    if (!hwnd) return {};
    int len = GetWindowTextLengthW(hwnd);
    std::wstring text(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(hwnd, text.data(), len + 1);
    text.resize(static_cast<size_t>(len));
    return wideToUtf8(text);
}

void NativeWindowsApp::setControlText(int id, std::string_view text) {
    HWND hwnd = control(id);
    if (hwnd) SetWindowTextW(hwnd, utf8ToWide(text).c_str());
}

HWND NativeWindowsApp::control(int id) const {
    HWND parent = settingsWindow_ && IsWindow(settingsWindow_) ? settingsWindow_ : monitor_;
    HWND found = settingsWindow_ ? GetDlgItem(settingsWindow_, id) : nullptr;
    if (!found && monitor_) found = GetDlgItem(monitor_, id);
    if (!found && parent) found = GetDlgItem(parent, id);
    return found;
}

} // namespace cqd
