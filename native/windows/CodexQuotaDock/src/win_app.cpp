#include "win_app.h"

#include <CommCtrl.h>
#include <Dwmapi.h>
#include <Shellapi.h>
#include <commdlg.h>
#include <windowsx.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <initializer_list>
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
constexpr UINT_PTR kPollTimer = 42;
constexpr const char* kVersion = "0.6.1-dev";
constexpr int kMonitorWidth = 360;
constexpr int kMonitorHeaderHeight = 38;
constexpr int kMonitorRowHeight = 64;
constexpr int kMonitorActionHeight = 44;
constexpr int kMonitorMinHeight = 150;
constexpr int kMonitorMaxHeight = 380;

enum ControlId {
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
    ID_SETTINGS_TAB = 2023,
    ID_DETAILS_EDIT = 2024,
    ID_USAGE_EDIT = 2025,
    ID_HEALTH_EDIT = 2026,
    ID_ALIAS_LABEL = 2027,
    ID_AUTH_LABEL = 2028,
    ID_POLL_LABEL = 2029,
    ID_FIVE_LABEL = 2030,
    ID_WEEKLY_LABEL = 2031,
    ID_UPDATE_STATUS = 2032,
};

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

std::string formatResetTime(int64_t epochSeconds) {
    if (epochSeconds <= 0) return {};
    std::time_t value = static_cast<std::time_t>(epochSeconds);
    std::tm local{};
    if (localtime_s(&local, &value) != 0) return {};
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << local.tm_hour << ":" << std::setw(2) << local.tm_min;
    return out.str();
}

std::string formatQuotaLine(const QuotaWindow& window, std::string_view fallback) {
    if (!window.remainingPercent) return std::string(fallback);
    std::ostringstream out;
    out << window.label << ": " << *window.remainingPercent << "% left";
    std::string reset = formatResetTime(window.resetsAt);
    if (!reset.empty()) out << ", resets " << reset;
    return out.str();
}

int monitorWindowHeightForRows(size_t rowCount) {
    if (rowCount < 1) rowCount = 1;
    int height = kMonitorHeaderHeight + static_cast<int>(rowCount) * kMonitorRowHeight + kMonitorActionHeight;
    return std::clamp(height, kMonitorMinHeight, kMonitorMaxHeight);
}

void showControls(HWND parent, bool visible, std::initializer_list<int> ids) {
    for (int id : ids) {
        HWND child = GetDlgItem(parent, id);
        if (child) ShowWindow(child, visible ? SW_SHOW : SW_HIDE);
    }
}

void drawTextUtf8(HDC dc, std::string_view text, RECT rect, UINT format) {
    std::wstring wide = utf8ToWide(text);
    DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &rect, format);
}

void drawQuotaBar(HDC dc, const QuotaWindow& window, int threshold, RECT rect) {
    HBRUSH track = CreateSolidBrush(RGB(66, 73, 84));
    FillRect(dc, &rect, track);
    DeleteObject(track);
    if (!window.remainingPercent) return;
    int percent = std::clamp(*window.remainingPercent, 0, 100);
    RECT fill = rect;
    fill.right = fill.left + ((rect.right - rect.left) * percent / 100);
    COLORREF color = RGB(66, 168, 128);
    if (threshold > 0 && percent <= threshold) color = percent <= 3 ? RGB(238, 82, 83) : RGB(235, 168, 74);
    HBRUSH fillBrush = CreateSolidBrush(color);
    FillRect(dc, &fill, fillBrush);
    DeleteObject(fillBrush);
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
    wc.style = CS_DBLCLKS;
    RegisterClassW(&wc);

    monitor_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kMonitorClass,
        L"Codex Quota Dock Native",
        WS_POPUP,
        80,
        120,
        kMonitorWidth,
        monitorWindowHeightForRows(2),
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
        980,
        640,
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

void NativeWindowsApp::layoutMonitorWindow() {
    if (!monitor_) return;
    RECT client{};
    GetClientRect(monitor_, &client);
    int y = std::max(104, static_cast<int>(client.bottom) - 36);
    int gap = 8;
    int width = (client.right - 20 - gap * 2) / 3;
    MoveWindow(control(ID_REFRESH), 10, y, width, 28, TRUE);
    MoveWindow(control(ID_SWITCH), 10 + width + gap, y, width, 28, TRUE);
    MoveWindow(control(ID_SETTINGS), 10 + (width + gap) * 2, y, width, 28, TRUE);
}

void NativeWindowsApp::resizeMonitorWindow() {
    if (!monitor_) return;
    int height = monitorWindowHeightForRows(monitorRows_.size());
    SetWindowPos(monitor_, nullptr, 0, 0, kMonitorWidth, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    layoutMonitorWindow();
}

void NativeWindowsApp::layoutSettingsWindow() {
    if (!settingsWindow_) return;
    RECT client{};
    GetClientRect(settingsWindow_, &client);
    int width = std::max(760, static_cast<int>(client.right));
    int height = std::max(500, static_cast<int>(client.bottom));
    int margin = 16;
    int leftWidth = 326;
    int rightX = margin + leftWidth + 16;
    int rightWidth = width - rightX - margin;
    int bottom = height - 44;

    MoveWindow(control(ID_PROFILE_LIST), margin, 64, leftWidth, 232, TRUE);
    MoveWindow(control(ID_IMPORT_CURRENT), margin, 304, 102, 28, TRUE);
    MoveWindow(control(ID_IMPORT_FILE), margin + 110, 304, 102, 28, TRUE);
    MoveWindow(control(ID_NEW_PROFILE), margin + 220, 304, 102, 28, TRUE);
    MoveWindow(control(ID_DELETE_PROFILE), margin, 338, 102, 28, TRUE);
    MoveWindow(control(ID_PIN_PROFILE), margin + 110, 338, 102, 28, TRUE);
    MoveWindow(control(ID_SWITCH_PROFILE), margin + 220, 338, 102, 28, TRUE);
    MoveWindow(control(ID_EXPORT_BACKUP), margin, 378, 102, 28, TRUE);
    MoveWindow(control(ID_IMPORT_BACKUP), margin + 110, 378, 102, 28, TRUE);
    MoveWindow(control(ID_RESTORE_BACKUP), margin + 220, 378, 102, 28, TRUE);
    MoveWindow(control(ID_ALIAS_LABEL), margin, 424, 50, 22, TRUE);
    MoveWindow(control(ID_ALIAS_EDIT), margin + 54, 422, leftWidth - 54, 24, TRUE);
    MoveWindow(control(ID_SAVE_PROFILE), margin, 456, 154, 28, TRUE);
    MoveWindow(control(ID_SAVE_SETTINGS), margin + 168, 456, 154, 28, TRUE);

    MoveWindow(control(ID_SETTINGS_TAB), rightX, 64, rightWidth, bottom - 70, TRUE);
    int contentX = rightX + 12;
    int contentY = 104;
    int contentW = rightWidth - 24;
    int contentH = bottom - contentY - 10;
    MoveWindow(control(ID_AUTH_LABEL), contentX, contentY, 220, 20, TRUE);
    MoveWindow(control(ID_AUTH_EDIT), contentX, contentY + 24, contentW, contentH - 24, TRUE);
    MoveWindow(control(ID_DETAILS_EDIT), contentX, contentY, contentW, contentH, TRUE);
    MoveWindow(control(ID_USAGE_EDIT), contentX, contentY, contentW, contentH, TRUE);
    MoveWindow(control(ID_HEALTH_EDIT), contentX, contentY, contentW, contentH, TRUE);

    MoveWindow(control(ID_POLL_LABEL), contentX, contentY, 90, 22, TRUE);
    MoveWindow(control(ID_POLL_COMBO), contentX + 120, contentY, 140, 120, TRUE);
    MoveWindow(control(ID_FIVE_LABEL), contentX, contentY + 38, 90, 22, TRUE);
    MoveWindow(control(ID_FIVE_COMBO), contentX + 120, contentY + 38, 140, 180, TRUE);
    MoveWindow(control(ID_WEEKLY_LABEL), contentX, contentY + 76, 90, 22, TRUE);
    MoveWindow(control(ID_WEEKLY_COMBO), contentX + 120, contentY + 76, 140, 180, TRUE);
    MoveWindow(control(ID_AUTO_RESTART), contentX, contentY + 124, 280, 24, TRUE);
    MoveWindow(control(ID_STARTUP), contentX, contentY + 154, 180, 24, TRUE);
    MoveWindow(control(ID_CHECK_UPDATES), contentX, contentY, 140, 28, TRUE);
    MoveWindow(control(ID_UPDATE_STATUS), contentX, contentY + 42, contentW, 80, TRUE);
    MoveWindow(control(ID_STATUS_TEXT), margin, height - 30, width - margin * 2, 22, TRUE);
}

void NativeWindowsApp::updateSettingsTabVisibility() {
    if (!settingsWindow_) return;
    showControls(settingsWindow_, settingsTab_ == 0, {ID_AUTH_LABEL, ID_AUTH_EDIT});
    showControls(settingsWindow_, settingsTab_ == 1, {ID_DETAILS_EDIT});
    showControls(settingsWindow_, settingsTab_ == 2, {ID_USAGE_EDIT});
    showControls(settingsWindow_, settingsTab_ == 3, {ID_POLL_LABEL, ID_POLL_COMBO, ID_FIVE_LABEL, ID_FIVE_COMBO, ID_WEEKLY_LABEL, ID_WEEKLY_COMBO, ID_AUTO_RESTART, ID_STARTUP});
    showControls(settingsWindow_, settingsTab_ == 4, {ID_HEALTH_EDIT});
    showControls(settingsWindow_, settingsTab_ == 5, {ID_CHECK_UPDATES, ID_UPDATE_STATUS});
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
        CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE, 10, 0, 104, 28, hwnd, reinterpret_cast<HMENU>(ID_REFRESH), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Switch", WS_CHILD | WS_VISIBLE, 124, 0, 104, 28, hwnd, reinterpret_cast<HMENU>(ID_SWITCH), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Config", WS_CHILD | WS_VISIBLE, 238, 0, 104, 28, hwnd, reinterpret_cast<HMENU>(ID_SETTINGS), instance_, nullptr);
        layoutMonitorWindow();
        return 0;
    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd, &point);
        RECT client{};
        GetClientRect(hwnd, &client);
        if (point.y >= kMonitorHeaderHeight && point.y < client.bottom) return HTCLIENT;
        return HTCAPTION;
    }
    case WM_SIZE:
        layoutMonitorWindow();
        return 0;
    case WM_LBUTTONDOWN:
        selectMonitorRowAt(GET_Y_LPARAM(lparam));
        return 0;
    case WM_LBUTTONDBLCLK:
        selectMonitorRowAt(GET_Y_LPARAM(lparam));
        createSettingsWindow();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
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

LRESULT NativeWindowsApp::handleSettings(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"Profiles", WS_CHILD | WS_VISIBLE, 16, 18, 180, 22, hwnd, nullptr, instance_, nullptr);
        CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY, 16, 64, 326, 232, hwnd, reinterpret_cast<HMENU>(ID_PROFILE_LIST), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Import Current", WS_CHILD | WS_VISIBLE, 16, 304, 102, 28, hwnd, reinterpret_cast<HMENU>(ID_IMPORT_CURRENT), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Import File", WS_CHILD | WS_VISIBLE, 126, 304, 102, 28, hwnd, reinterpret_cast<HMENU>(ID_IMPORT_FILE), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"New Profile", WS_CHILD | WS_VISIBLE, 236, 304, 102, 28, hwnd, reinterpret_cast<HMENU>(ID_NEW_PROFILE), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE, 16, 338, 102, 28, hwnd, reinterpret_cast<HMENU>(ID_DELETE_PROFILE), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Pin", WS_CHILD | WS_VISIBLE, 126, 338, 102, 28, hwnd, reinterpret_cast<HMENU>(ID_PIN_PROFILE), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Switch", WS_CHILD | WS_VISIBLE, 236, 338, 102, 28, hwnd, reinterpret_cast<HMENU>(ID_SWITCH_PROFILE), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Export", WS_CHILD | WS_VISIBLE, 16, 378, 102, 28, hwnd, reinterpret_cast<HMENU>(ID_EXPORT_BACKUP), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Import", WS_CHILD | WS_VISIBLE, 126, 378, 102, 28, hwnd, reinterpret_cast<HMENU>(ID_IMPORT_BACKUP), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Restore", WS_CHILD | WS_VISIBLE, 236, 378, 102, 28, hwnd, reinterpret_cast<HMENU>(ID_RESTORE_BACKUP), instance_, nullptr);
        CreateWindowW(L"STATIC", L"Alias", WS_CHILD | WS_VISIBLE, 16, 424, 50, 22, hwnd, reinterpret_cast<HMENU>(ID_ALIAS_LABEL), instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 70, 422, 272, 24, hwnd, reinterpret_cast<HMENU>(ID_ALIAS_EDIT), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Save Profile", WS_CHILD | WS_VISIBLE, 16, 456, 154, 28, hwnd, reinterpret_cast<HMENU>(ID_SAVE_PROFILE), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Save Settings", WS_CHILD | WS_VISIBLE, 184, 456, 154, 28, hwnd, reinterpret_cast<HMENU>(ID_SAVE_SETTINGS), instance_, nullptr);

        HWND tabs = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 358, 64, 600, 520, hwnd, reinterpret_cast<HMENU>(ID_SETTINGS_TAB), instance_, nullptr);
        for (const wchar_t* name : {L"Auth JSON", L"Quota", L"Usage", L"Settings", L"Health", L"Updates"}) {
            TCITEMW item{};
            item.mask = TCIF_TEXT;
            item.pszText = const_cast<wchar_t*>(name);
            TabCtrl_InsertItem(tabs, TabCtrl_GetItemCount(tabs), &item);
        }
        CreateWindowW(L"STATIC", L"Saved auth.json", WS_CHILD | WS_VISIBLE, 370, 104, 220, 20, hwnd, reinterpret_cast<HMENU>(ID_AUTH_LABEL), instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL, 370, 128, 560, 400, hwnd, reinterpret_cast<HMENU>(ID_AUTH_EDIT), instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 370, 104, 560, 424, hwnd, reinterpret_cast<HMENU>(ID_DETAILS_EDIT), instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 370, 104, 560, 424, hwnd, reinterpret_cast<HMENU>(ID_USAGE_EDIT), instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 370, 104, 560, 424, hwnd, reinterpret_cast<HMENU>(ID_HEALTH_EDIT), instance_, nullptr);

        CreateWindowW(L"STATIC", L"Poll", WS_CHILD | WS_VISIBLE, 370, 104, 90, 22, hwnd, reinterpret_cast<HMENU>(ID_POLL_LABEL), instance_, nullptr);
        HWND poll = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 490, 104, 140, 120, hwnd, reinterpret_cast<HMENU>(ID_POLL_COMBO), instance_, nullptr);
        addComboItem(poll, L"1 min", 1); addComboItem(poll, L"5 min", 5); addComboItem(poll, L"10 min", 10);
        CreateWindowW(L"STATIC", L"5h alert", WS_CHILD | WS_VISIBLE, 370, 142, 90, 22, hwnd, reinterpret_cast<HMENU>(ID_FIVE_LABEL), instance_, nullptr);
        HWND five = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 490, 142, 140, 180, hwnd, reinterpret_cast<HMENU>(ID_FIVE_COMBO), instance_, nullptr);
        CreateWindowW(L"STATIC", L"Weekly alert", WS_CHILD | WS_VISIBLE, 370, 180, 90, 22, hwnd, reinterpret_cast<HMENU>(ID_WEEKLY_LABEL), instance_, nullptr);
        HWND weekly = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 490, 180, 140, 180, hwnd, reinterpret_cast<HMENU>(ID_WEEKLY_COMBO), instance_, nullptr);
        for (int value : {0, 5, 10, 15, 20, 30, 40}) {
            std::wstring text = value == 0 ? L"Off" : std::to_wstring(value) + L"%";
            addComboItem(five, text.c_str(), value);
            addComboItem(weekly, text.c_str(), value);
        }
        CreateWindowW(L"BUTTON", L"Restart Codex automatically after switching", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 370, 228, 300, 24, hwnd, reinterpret_cast<HMENU>(ID_AUTO_RESTART), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Start at login", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 370, 258, 180, 24, hwnd, reinterpret_cast<HMENU>(ID_STARTUP), instance_, nullptr);
        CreateWindowW(L"BUTTON", L"Check Updates", WS_CHILD | WS_VISIBLE, 370, 104, 140, 28, hwnd, reinterpret_cast<HMENU>(ID_CHECK_UPDATES), instance_, nullptr);
        CreateWindowW(L"STATIC", L"Current version: 0.6.1", WS_CHILD | WS_VISIBLE, 370, 146, 560, 80, hwnd, reinterpret_cast<HMENU>(ID_UPDATE_STATUS), instance_, nullptr);
        CreateWindowW(L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE, 16, 590, 920, 22, hwnd, reinterpret_cast<HMENU>(ID_STATUS_TEXT), instance_, nullptr);

        selectComboValue(poll, settings_.pollIntervalMinutes);
        selectComboValue(five, settings_.fiveHourAlertThreshold);
        selectComboValue(weekly, settings_.weeklyAlertThreshold);
        SendMessageW(control(ID_AUTO_RESTART), BM_SETCHECK, settings_.autoRestartCodex ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(control(ID_STARTUP), BM_SETCHECK, settings_.startAtLogin ? BST_CHECKED : BST_UNCHECKED, 0);
        updateProfileList();
        updateHealthAndUsageText();
        updateSettingsTabVisibility();
        layoutSettingsWindow();
        return 0;
    }
    case WM_SIZE:
        layoutSettingsWindow();
        return 0;
    case WM_NOTIFY: {
        auto* notify = reinterpret_cast<NMHDR*>(lparam);
        if (notify && notify->idFrom == ID_SETTINGS_TAB && notify->code == TCN_SELCHANGE) {
            settingsTab_ = TabCtrl_GetCurSel(control(ID_SETTINGS_TAB));
            updateSettingsTabVisibility();
            return 0;
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ID_PROFILE_LIST && HIWORD(wparam) == LBN_SELCHANGE) {
            selectProfileByIndex(static_cast<int>(SendMessageW(control(ID_PROFILE_LIST), LB_GETCURSEL, 0, 0)));
            loadSelectedProfileEditor();
            updateQuotaDetailsText();
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
    return DefWindowProcW(hwnd, message, wparam, lparam);
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
    bool selectedVisible = std::any_of(monitorRows_.begin(), monitorRows_.end(), [&](const MonitorRow& row) { return row.profile.id == selectedProfileId_; });
    if (!selectedVisible && !monitorRows_.empty()) {
        std::string active = activeAccountId();
        selectedProfileId_ = monitorRows_.front().profile.id;
        for (const auto& row : monitorRows_) {
            if (!active.empty() && row.profile.accountId == active) {
                selectedProfileId_ = row.profile.id;
                break;
            }
        }
    }
    status_ = fetchQuotaValues ? "Refreshed" : "Ready";
    resizeMonitorWindow();
    updateProfileList();
    InvalidateRect(monitor_, nullptr, TRUE);
    updateHealthAndUsageText();
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
    updateQuotaDetailsText();
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
    updateQuotaDetailsText();
    updateLocalUsageText();
    updateHealthText();
}

void NativeWindowsApp::updateQuotaDetailsText() {
    HWND edit = control(ID_DETAILS_EDIT);
    if (!edit) return;
    std::ostringstream out;
    Profile* selected = selectedProfile();
    if (!selected) {
        out << "Select a profile to see quota details.\r\n";
    } else {
        out << selected->alias << "\r\n";
        out << "Account: " << (selected->accountSuffix.empty() ? selected->accountId : selected->accountSuffix) << "\r\n\r\n";
        bool found = false;
        for (const auto& row : monitorRows_) {
            if (row.profile.id != selected->id) continue;
            found = true;
            if (!row.quota.error.empty()) {
                out << row.quota.error << "\r\n";
            } else {
                out << formatQuotaLine(row.quota.fiveHour, "5h: not refreshed") << "\r\n";
                out << formatQuotaLine(row.quota.weekly, "weekly: not refreshed") << "\r\n";
                if (!row.quota.planType.empty()) out << "\r\nPlan: " << row.quota.planType << "\r\n";
            }
            break;
        }
        if (!found) out << "This profile is not visible in the monitor. Pin it or make it current to show quota here.\r\n";
    }
    setControlText(ID_DETAILS_EDIT, out.str());
}

void NativeWindowsApp::updateLocalUsageText() {
    HWND edit = control(ID_USAGE_EDIT);
    if (!edit) return;
    LocalUsageSummary usage = scanLocalUsage(defaultCodexRoot());
    std::ostringstream out;
    out << "Local Usage\r\n";
    out << "Today: " << usage.today.total << " tokens\r\n";
    out << "Last 7 days: " << usage.last7Days.total << " tokens\r\n";
    out << "Last 30 days: " << usage.last30Days.total << " tokens\r\n";
    out << "All local sessions: " << usage.total.total << " tokens\r\n";
    out << "Sessions: " << usage.sessionCount << "\r\n";
    if (usage.parseErrors) out << "Parse errors: " << usage.parseErrors << "\r\n";
    out << "\r\nWebsite quota is account-wide. Local usage only counts Codex session logs on this machine.\r\n";
    setControlText(ID_USAGE_EDIT, out.str());
}

void NativeWindowsApp::updateHealthText() {
    HWND edit = control(ID_HEALTH_EDIT);
    if (!edit) return;
    std::ostringstream out;
    out << "Health\r\n";
    for (const auto& row : runHealthCheck(store_, defaultCodexAuthPath(), kVersion)) {
        out << row.status << " - " << row.label << ": " << row.detail << "\r\n";
    }
    setControlText(ID_HEALTH_EDIT, out.str());
}

void NativeWindowsApp::paintMonitor(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);
    RECT client{};
    GetClientRect(hwnd, &client);
    HBRUSH background = CreateSolidBrush(RGB(25, 28, 34));
    FillRect(dc, &client, background);
    DeleteObject(background);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(244, 246, 248));
    RECT title{12, 10, 140, 30};
    drawTextUtf8(dc, "Codex", title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(dc, RGB(174, 184, 196));
    RECT status{150, 10, client.right - 12, 30};
    std::ostringstream statusText;
    statusText << monitorRows_.size() << " | " << status_;
    drawTextUtf8(dc, statusText.str(), status, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    int y = kMonitorHeaderHeight;
    int bottomLimit = std::max(y, static_cast<int>(client.bottom) - kMonitorActionHeight - 4);
    if (monitorRows_.empty()) {
        RECT empty{12, y + 12, client.right - 12, bottomLimit};
        drawTextUtf8(dc, "No profiles yet. Open Config to import auth.", empty, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    }

    std::string active = activeAccountId();
    for (const MonitorRow& row : monitorRows_) {
        if (y + kMonitorRowHeight - 6 > bottomLimit) break;
        bool selected = row.profile.id == selectedProfileId_;
        bool activeProfile = !active.empty() && row.profile.accountId == active;
        bool warning = row.quota.belowThreshold(settings_.fiveHourAlertThreshold, settings_.weeklyAlertThreshold);
        COLORREF boxColor = selected ? RGB(42, 75, 96) : (warning ? RGB(72, 35, 38) : RGB(38, 44, 52));
        HBRUSH rowBrush = CreateSolidBrush(boxColor);
        RECT box{10, y, client.right - 10, y + kMonitorRowHeight - 8};
        FillRect(dc, &box, rowBrush);
        DeleteObject(rowBrush);
        SetTextColor(dc, RGB(255, 255, 255));
        std::string name = row.profile.alias;
        if (activeProfile) name += "  current";
        if (row.profile.pinned) name += "  pinned";
        RECT alias{20, y + 5, client.right - 20, y + 21};
        drawTextUtf8(dc, name, alias, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (!row.quota.error.empty()) {
            SetTextColor(dc, RGB(235, 168, 74));
            RECT error{20, y + 27, client.right - 20, y + 50};
            drawTextUtf8(dc, row.quota.error, error, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            SetTextColor(dc, RGB(199, 207, 216));
            RECT fiveText{20, y + 24, client.right - 20, y + 39};
            drawTextUtf8(dc, formatQuotaLine(row.quota.fiveHour, "5h: not refreshed"), fiveText, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT fiveBar{20, y + 40, client.right - 20, y + 43};
            drawQuotaBar(dc, row.quota.fiveHour, settings_.fiveHourAlertThreshold, fiveBar);

            RECT weeklyText{20, y + 44, client.right - 20, y + 59};
            drawTextUtf8(dc, formatQuotaLine(row.quota.weekly, "weekly: not refreshed"), weeklyText, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT weeklyBar{20, y + 60, client.right - 20, y + 63};
            drawQuotaBar(dc, row.quota.weekly, settings_.weeklyAlertThreshold, weeklyBar);
        }
        y += kMonitorRowHeight;
    }
    EndPaint(hwnd, &ps);
}

void NativeWindowsApp::selectMonitorRowAt(int y) {
    int rowIndex = (y - kMonitorHeaderHeight) / kMonitorRowHeight;
    if (rowIndex < 0 || rowIndex >= static_cast<int>(monitorRows_.size())) return;
    selectedProfileId_ = monitorRows_[static_cast<size_t>(rowIndex)].profile.id;
    updateProfileList();
    updateQuotaDetailsText();
    InvalidateRect(monitor_, nullptr, TRUE);
}

void NativeWindowsApp::selectProfileByIndex(int index) {
    if (index < 0 || index >= static_cast<int>(store_.profiles().size())) return;
    selectedProfileId_ = store_.profiles()[static_cast<size_t>(index)].id;
    updateProfileList();
}

void NativeWindowsApp::importCurrentProfile() {
    try {
        std::string auth = readTextFile(defaultCodexAuthPath());
        std::string alias = store_.suggestAlias("current", auth);
        Profile profile = store_.importAuth(alias, auth, true);
        selectedProfileId_ = profile.id;
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
    bool hasPinned = std::any_of(store_.profiles().begin(), store_.profiles().end(), [](const Profile& profile) { return profile.pinned; });
    if (!hasPinned) return store_.profiles();

    std::vector<Profile> out;
    std::string seen;
    for (const auto& profile : store_.profiles()) {
        if (!active.empty() && profile.accountId == active) {
            out.push_back(profile);
            seen = profile.id;
            break;
        }
    }
    for (const auto& profile : store_.profiles()) {
        if (profile.pinned && profile.id != seen) out.push_back(profile);
    }
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
