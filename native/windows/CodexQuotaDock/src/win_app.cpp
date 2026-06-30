#include "win_app.h"

#include <CommCtrl.h>
#include <Dwmapi.h>
#include <Shellapi.h>
#include <Uxtheme.h>
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
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

namespace cqd {
namespace {

constexpr wchar_t kMonitorClass[] = L"CodexQuotaDockNativeMonitor";
constexpr wchar_t kSettingsClass[] = L"CodexQuotaDockNativeSettings";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kUsageLoadedMessage = WM_APP + 2;
constexpr UINT_PTR kPollTimer = 42;
constexpr UINT_PTR kUsageAnimationTimer = 43;
constexpr const char* kVersion = "0.7.0";
constexpr int kAppIconResourceId = 1;
constexpr int kTabIconResourceIds[] = {10, 11, 12, 13, 14, 15};
constexpr int kMonitorWidth = 372;
constexpr int kMonitorHeaderHeight = 36;
constexpr int kMonitorRowHeight = 78;
constexpr int kMonitorActionHeight = 46;
constexpr int kMonitorMinHeight = 164;
constexpr int kMonitorMaxHeight = 420;

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
    ID_TAB_AUTH = 2023,
    ID_DETAILS_EDIT = 2024,
    ID_USAGE_EDIT = 2025,
    ID_HEALTH_EDIT = 2026,
    ID_ALIAS_LABEL = 2027,
    ID_AUTH_LABEL = 2028,
    ID_POLL_LABEL = 2029,
    ID_FIVE_LABEL = 2030,
    ID_WEEKLY_LABEL = 2031,
    ID_UPDATE_STATUS = 2032,
    ID_TAB_QUOTA = 2033,
    ID_TAB_USAGE = 2034,
    ID_TAB_SETTINGS = 2035,
    ID_TAB_HEALTH = 2036,
    ID_TAB_UPDATES = 2037,
    ID_TRAY_TOGGLE = 3001,
    ID_TRAY_CONFIG = 3002,
    ID_TRAY_EXIT = 3003,
};

struct Theme {
    bool dark = true;
    BYTE monitorAlpha = 244;
    COLORREF windowBackground = RGB(243, 246, 250);
    COLORREF monitorBackground = RGB(243, 246, 250);
    COLORREF panel = RGB(255, 255, 255);
    COLORREF card = RGB(250, 251, 253);
    COLORREF cardHover = RGB(244, 248, 252);
    COLORREF cardSelected = RGB(226, 241, 255);
    COLORREF cardWarning = RGB(255, 241, 224);
    COLORREF border = RGB(214, 220, 228);
    COLORREF borderStrong = RGB(186, 198, 211);
    COLORREF text = RGB(32, 35, 40);
    COLORREF muted = RGB(92, 102, 115);
    COLORREF subtle = RGB(124, 134, 148);
    COLORREF accent = RGB(0, 120, 212);
    COLORREF accentSoft = RGB(208, 232, 255);
    COLORREF success = RGB(15, 123, 92);
    COLORREF warning = RGB(194, 126, 30);
    COLORREF danger = RGB(205, 54, 54);
    COLORREF control = RGB(255, 255, 255);
    COLORREF controlBorder = RGB(201, 208, 217);
    COLORREF barTrack = RGB(218, 225, 234);
};

bool systemUsesLightTheme() {
    DWORD value = 1;
    DWORD size = sizeof(value);
    LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size
    );
    return status != ERROR_SUCCESS || value != 0;
}

Theme currentTheme(bool refresh = false) {
    static bool cached = false;
    static Theme cachedTheme;
    if (cached && !refresh) return cachedTheme;

    Theme theme;
    theme.dark = !systemUsesLightTheme();
    if (!theme.dark) {
        cachedTheme = theme;
        cached = true;
        return cachedTheme;
    }

    theme.monitorAlpha = 238;
    theme.windowBackground = RGB(24, 28, 35);
    theme.monitorBackground = RGB(24, 28, 35);
    theme.panel = RGB(31, 36, 44);
    theme.card = RGB(38, 44, 53);
    theme.cardHover = RGB(45, 53, 64);
    theme.cardSelected = RGB(34, 72, 100);
    theme.cardWarning = RGB(78, 44, 42);
    theme.border = RGB(66, 75, 88);
    theme.borderStrong = RGB(88, 103, 120);
    theme.text = RGB(245, 247, 250);
    theme.muted = RGB(190, 199, 210);
    theme.subtle = RGB(142, 153, 168);
    theme.accent = RGB(96, 181, 255);
    theme.accentSoft = RGB(30, 74, 102);
    theme.success = RGB(82, 199, 151);
    theme.warning = RGB(235, 172, 74);
    theme.danger = RGB(238, 82, 83);
    theme.control = RGB(31, 36, 44);
    theme.controlBorder = RGB(76, 86, 100);
    theme.barTrack = RGB(67, 76, 90);
    cachedTheme = theme;
    cached = true;
    return cachedTheme;
}

void fillRect(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void drawRoundRect(HDC dc, RECT rect, int radius, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void drawTextUtf8(HDC dc, std::string_view text, RECT rect, UINT format);

int settingsTabIndexFromControlId(int id) {
    switch (id) {
    case ID_TAB_AUTH: return 0;
    case ID_TAB_QUOTA: return 1;
    case ID_TAB_USAGE: return 2;
    case ID_TAB_SETTINGS: return 3;
    case ID_TAB_HEALTH: return 4;
    case ID_TAB_UPDATES: return 5;
    default: return -1;
    }
}

void drawPill(HDC dc, std::string_view text, RECT rect, const Theme& theme, bool accent) {
    COLORREF fill = accent ? theme.accentSoft : theme.cardHover;
    COLORREF border = accent ? theme.accent : theme.border;
    drawRoundRect(dc, rect, 12, fill, border);
    SetTextColor(dc, accent ? theme.accent : theme.muted);
    drawTextUtf8(dc, text, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void drawUsageGlyph(HDC dc, RECT rect, const Theme& theme, bool selected) {
    COLORREF color = selected ? theme.accent : theme.muted;
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, brush);

    int width = rect.right - rect.left;
    int bottom = rect.bottom - 2;
    int barWidth = std::max(2, width / 5);
    RECT first{rect.left + 1, bottom - 5, rect.left + 1 + barWidth, bottom};
    RECT second{rect.left + 1 + barWidth + 2, bottom - 9, rect.left + 1 + barWidth * 2 + 2, bottom};
    RECT third{rect.left + 1 + (barWidth + 2) * 2, bottom - 13, rect.left + 1 + (barWidth + 2) * 2 + barWidth, bottom};
    RoundRect(dc, first.left, first.top, first.right, first.bottom, 2, 2);
    RoundRect(dc, second.left, second.top, second.right, second.bottom, 2, 2);
    RoundRect(dc, third.left, third.top, third.right, third.bottom, 2, 2);

    MoveToEx(dc, rect.left, rect.bottom - 1, nullptr);
    LineTo(dc, rect.right, rect.bottom - 1);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void drawTabGlyph(HDC dc, int tabIndex, RECT rect, const Theme& theme, bool selected) {
    if (tabIndex == 2) {
        drawUsageGlyph(dc, rect, theme, selected);
        return;
    }
    COLORREF color = selected ? theme.accent : theme.muted;
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    int midY = (rect.top + rect.bottom) / 2;

    switch (tabIndex) {
    case 0:
        Ellipse(dc, rect.left + 1, midY - 4, rect.left + 9, midY + 4);
        MoveToEx(dc, rect.left + 9, midY, nullptr);
        LineTo(dc, rect.right - 1, midY);
        LineTo(dc, rect.right - 1, midY + 4);
        break;
    case 1:
        Arc(dc, rect.left + 1, rect.top + 3, rect.right - 1, rect.bottom + 8, rect.left + 2, midY, rect.right - 2, midY);
        MoveToEx(dc, (rect.left + rect.right) / 2, midY + 3, nullptr);
        LineTo(dc, rect.right - 4, rect.top + 5);
        break;
    case 3:
        for (int i = 0; i < 3; ++i) {
            int y = rect.top + 3 + i * 5;
            MoveToEx(dc, rect.left + 1, y, nullptr);
            LineTo(dc, rect.right - 1, y);
            SelectObject(dc, brush);
            int knob = rect.left + 4 + (i == 1 ? 6 : 0);
            Ellipse(dc, knob - 2, y - 2, knob + 3, y + 3);
            SelectObject(dc, GetStockObject(NULL_BRUSH));
        }
        break;
    case 4:
        MoveToEx(dc, rect.left + 2, midY, nullptr);
        LineTo(dc, rect.left + 7, rect.bottom - 3);
        LineTo(dc, rect.right - 1, rect.top + 3);
        break;
    case 5:
        Arc(dc, rect.left + 2, rect.top + 2, rect.right - 2, rect.bottom - 2, rect.right - 4, rect.top + 4, rect.left + 3, rect.bottom - 4);
        MoveToEx(dc, rect.right - 5, rect.top + 3, nullptr);
        LineTo(dc, rect.right - 1, rect.top + 3);
        LineTo(dc, rect.right - 1, rect.top + 7);
        break;
    default:
        SelectObject(dc, brush);
        Ellipse(dc, rect.left + 4, rect.top + 4, rect.right - 4, rect.bottom - 4);
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        break;
    }

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

HFONT createSegoeFont(int pointSize, int weight) {
    HDC screen = GetDC(nullptr);
    int height = -MulDiv(pointSize, GetDeviceCaps(screen, LOGPIXELSY), 72);
    ReleaseDC(nullptr, screen);
    return CreateFontW(
        height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );
}

HWND createCommandButton(HWND parent, int id, const wchar_t* text, int x, int y, int width, int height, HINSTANCE instance) {
    return CreateWindowW(
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x,
        y,
        width,
        height,
        parent,
        reinterpret_cast<HMENU>(id),
        instance,
        nullptr
    );
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

std::string formatResetTime(int64_t epochSeconds) {
    if (epochSeconds <= 0) return {};
    std::time_t value = static_cast<std::time_t>(epochSeconds);
    std::tm local{};
    if (localtime_s(&local, &value) != 0) return {};
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << local.tm_hour << ":" << std::setw(2) << local.tm_min;
    return out.str();
}

std::string formatNumber(int64_t value) {
    bool negative = value < 0;
    uint64_t remaining = static_cast<uint64_t>(negative ? -value : value);
    std::string digits = std::to_string(remaining);
    for (int insertAt = static_cast<int>(digits.size()) - 3; insertAt > 0; insertAt -= 3) {
        digits.insert(static_cast<size_t>(insertAt), ",");
    }
    return negative ? "-" + digits : digits;
}

std::string formatCompactNumber(int64_t value) {
    if (value >= 1000000) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(value >= 10000000 ? 0 : 1) << static_cast<double>(value) / 1000000.0 << "M";
        return out.str();
    }
    if (value >= 1000) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(value >= 10000 ? 0 : 1) << static_cast<double>(value) / 1000.0 << "K";
        return out.str();
    }
    return std::to_string(value);
}

std::string formatDayKey(std::time_t value) {
    std::tm local{};
    localtime_s(&local, &value);
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d");
    return out.str();
}

std::string formatDayLabel(std::time_t value) {
    std::tm local{};
    localtime_s(&local, &value);
    std::ostringstream out;
    out << std::put_time(&local, "%m/%d");
    return out.str();
}

std::string percentShare(int64_t part, int64_t total) {
    if (total <= 0) return "0.0%";
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << (static_cast<double>(part) * 100.0 / static_cast<double>(total)) << "%";
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

void drawQuotaBar(HDC dc, const QuotaWindow& window, int threshold, RECT rect, const Theme& theme) {
    drawRoundRect(dc, rect, 4, theme.barTrack, theme.barTrack);
    if (!window.remainingPercent) return;
    int percent = std::clamp(*window.remainingPercent, 0, 100);
    RECT fill = rect;
    fill.right = fill.left + ((rect.right - rect.left) * percent / 100);
    COLORREF color = theme.success;
    if (threshold > 0 && percent <= threshold) color = percent <= 3 ? theme.danger : theme.warning;
    drawRoundRect(dc, fill, 4, color, color);
}

void drawLoadingSpinner(HDC dc, POINT center, int frame, const Theme& theme) {
    static constexpr POINT offsets[] = {
        {0, -12}, {8, -8}, {12, 0}, {8, 8}, {0, 12}, {-8, 8}, {-12, 0}, {-8, -8},
    };
    for (int i = 0; i < 8; ++i) {
        int phase = (i - frame) % 8;
        if (phase < 0) phase += 8;
        COLORREF color = phase == 0 ? theme.accent : (phase <= 2 ? theme.borderStrong : theme.border);
        HBRUSH brush = CreateSolidBrush(color);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HPEN pen = CreatePen(PS_SOLID, 1, color);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        int size = phase == 0 ? 5 : 4;
        int x = center.x + offsets[i].x;
        int y = center.y + offsets[i].y;
        Ellipse(dc, x - size / 2, y - size / 2, x + size / 2 + 1, y + size / 2 + 1);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
    }
}

} // namespace

int NativeWindowsApp::run(HINSTANCE instance, int showCommand) {
    instance_ = instance;
    createVisualResources();
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
    destroyVisualResources();
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
    wc.hIcon = appIcon_;
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
    SendMessageW(monitor_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIcon_));
    SendMessageW(monitor_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appSmallIcon_ ? appSmallIcon_ : appIcon_));
    SetLayeredWindowAttributes(monitor_, RGB(24, 24, 24), currentTheme().monitorAlpha, LWA_ALPHA);
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
    wc.hIcon = appIcon_;
    wc.hbrBackground = nullptr;
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
    SendMessageW(settingsWindow_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIcon_));
    SendMessageW(settingsWindow_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appSmallIcon_ ? appSmallIcon_ : appIcon_));
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
    tray_.hIcon = appSmallIcon_ ? appSmallIcon_ : appIcon_;
    wcscpy_s(tray_.szTip, L"Codex Quota Dock");
    Shell_NotifyIconW(NIM_ADD, &tray_);
}

void NativeWindowsApp::removeTrayIcon() {
    if (tray_.cbSize) Shell_NotifyIconW(NIM_DELETE, &tray_);
}

void NativeWindowsApp::showTrayMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, ID_TRAY_TOGGLE, IsWindowVisible(monitor_) ? L"Hide Monitor" : L"Show Monitor");
    AppendMenuW(menu, MF_STRING, ID_TRAY_CONFIG, L"Config");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(monitor_);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, cursor.x, cursor.y, 0, monitor_, nullptr);
    DestroyMenu(menu);
    if (command == ID_TRAY_TOGGLE) {
        ShowWindow(monitor_, IsWindowVisible(monitor_) ? SW_HIDE : SW_SHOWNORMAL);
    } else if (command == ID_TRAY_CONFIG) {
        createSettingsWindow();
    } else if (command == ID_TRAY_EXIT) {
        if (settingsWindow_) DestroyWindow(settingsWindow_);
        DestroyWindow(monitor_);
    }
}

void NativeWindowsApp::createVisualResources() {
    Theme theme = currentTheme(true);
    appIcon_ = LoadIconW(instance_, MAKEINTRESOURCEW(kAppIconResourceId));
    appSmallIcon_ = reinterpret_cast<HICON>(LoadImageW(
        instance_,
        MAKEINTRESOURCEW(kAppIconResourceId),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR
    ));
    for (int i = 0; i < 6; ++i) {
        tabIcons_[i] = reinterpret_cast<HICON>(LoadImageW(
            instance_,
            MAKEINTRESOURCEW(kTabIconResourceIds[i]),
            IMAGE_ICON,
            16,
            16,
            LR_DEFAULTCOLOR
        ));
    }
    uiFont_ = createSegoeFont(9, FW_NORMAL);
    titleFont_ = createSegoeFont(10, FW_SEMIBOLD);
    smallFont_ = createSegoeFont(8, FW_NORMAL);
    settingsBackgroundBrush_ = CreateSolidBrush(theme.panel);
    controlBackgroundBrush_ = CreateSolidBrush(theme.control);
}

void NativeWindowsApp::destroyVisualResources() {
    if (uiFont_) DeleteObject(uiFont_);
    if (titleFont_) DeleteObject(titleFont_);
    if (smallFont_) DeleteObject(smallFont_);
    if (settingsBackgroundBrush_) DeleteObject(settingsBackgroundBrush_);
    if (controlBackgroundBrush_) DeleteObject(controlBackgroundBrush_);
    if (appSmallIcon_) DestroyIcon(appSmallIcon_);
    for (HICON& icon : tabIcons_) {
        if (icon) DestroyIcon(icon);
        icon = nullptr;
    }
    uiFont_ = nullptr;
    titleFont_ = nullptr;
    smallFont_ = nullptr;
    settingsBackgroundBrush_ = nullptr;
    controlBackgroundBrush_ = nullptr;
    appIcon_ = nullptr;
    appSmallIcon_ = nullptr;
}

void NativeWindowsApp::applyWindows11Style(HWND hwnd, bool floating) {
    Theme theme = currentTheme();
    const int rounded = 2;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &rounded, sizeof(rounded));
    const BOOL dark = theme.dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    const int backdrop = floating ? 3 : 2;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    COLORREF border = theme.border;
    COLORREF caption = theme.windowBackground;
    COLORREF text = theme.text;
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &text, sizeof(text));
}

void NativeWindowsApp::styleWindowControls(HWND parent) {
    Theme theme = currentTheme();
    const int ids[] = {
        ID_REFRESH, ID_SWITCH, ID_SETTINGS,
        ID_TAB_AUTH, ID_TAB_QUOTA, ID_TAB_USAGE, ID_TAB_SETTINGS, ID_TAB_HEALTH, ID_TAB_UPDATES,
        ID_PROFILE_LIST, ID_ALIAS_EDIT, ID_AUTH_EDIT, ID_IMPORT_CURRENT, ID_IMPORT_FILE, ID_NEW_PROFILE,
        ID_SAVE_PROFILE, ID_DELETE_PROFILE, ID_PIN_PROFILE, ID_SWITCH_PROFILE, ID_EXPORT_BACKUP,
        ID_IMPORT_BACKUP, ID_RESTORE_BACKUP, ID_POLL_COMBO, ID_FIVE_COMBO, ID_WEEKLY_COMBO,
        ID_AUTO_RESTART, ID_STARTUP, ID_SAVE_SETTINGS, ID_CHECK_UPDATES, ID_DETAILS_EDIT,
        ID_USAGE_EDIT, ID_HEALTH_EDIT
    };
    for (int id : ids) {
        HWND child = GetDlgItem(parent, id);
        if (!child) child = control(id);
        if (!child) continue;
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
        SetWindowTheme(child, theme.dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    }
    for (int id : {ID_ALIAS_LABEL, ID_AUTH_LABEL, ID_POLL_LABEL, ID_FIVE_LABEL, ID_WEEKLY_LABEL, ID_UPDATE_STATUS, ID_STATUS_TEXT}) {
        HWND child = GetDlgItem(parent, id);
        if (child) SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    }
    for (int id : {ID_ALIAS_EDIT, ID_AUTH_EDIT, ID_DETAILS_EDIT, ID_USAGE_EDIT, ID_HEALTH_EDIT}) {
        HWND child = GetDlgItem(parent, id);
        if (child) SendMessageW(child, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
    }
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

    const int tabIds[] = {ID_TAB_AUTH, ID_TAB_QUOTA, ID_TAB_USAGE, ID_TAB_SETTINGS, ID_TAB_HEALTH, ID_TAB_UPDATES};
    int tabInset = 8;
    int tabGap = 6;
    int tabHeight = 30;
    int tabY = 26;
    int tabWidth = std::max(68, (rightWidth - tabInset * 2 - tabGap * 5) / 6);
    for (int i = 0; i < 6; ++i) {
        MoveWindow(control(tabIds[i]), rightX + tabInset + i * (tabWidth + tabGap), tabY, tabWidth, tabHeight, TRUE);
    }
    int contentInset = 18;
    int contentX = rightX + contentInset;
    int contentY = tabY + tabHeight + 12;
    int contentW = rightWidth - contentInset * 2;
    int contentH = bottom - contentY - 10;
    MoveWindow(control(ID_AUTH_LABEL), contentX, contentY, 220, 20, TRUE);
    MoveWindow(control(ID_AUTH_EDIT), contentX, contentY + 24, contentW, contentH - 24, TRUE);
    MoveWindow(control(ID_DETAILS_EDIT), contentX, contentY, contentW, contentH, TRUE);
    MoveWindow(control(ID_USAGE_EDIT), contentX, contentY, contentW, contentH, TRUE);
    MoveWindow(control(ID_HEALTH_EDIT), contentX, contentY, contentW, contentH, TRUE);

    MoveWindow(control(ID_POLL_LABEL), contentX, contentY, 110, 22, TRUE);
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
    if (settingsTab_ == 0) {
        loadSelectedProfileEditor();
    } else if (settingsTab_ == 1) {
        updateQuotaDetailsText();
    } else if (settingsTab_ == 2 && !usageLoaded_) {
        startLocalUsageLoad();
    } else if (settingsTab_ == 4 && !healthLoaded_) {
        setControlText(ID_HEALTH_EDIT, "Running health checks...");
        updateHealthText();
        healthLoaded_ = true;
    }
    for (int id : {ID_TAB_AUTH, ID_TAB_QUOTA, ID_TAB_USAGE, ID_TAB_SETTINGS, ID_TAB_HEALTH, ID_TAB_UPDATES}) {
        HWND tab = GetDlgItem(settingsWindow_, id);
        if (tab) InvalidateRect(tab, nullptr, TRUE);
    }
    InvalidateRect(settingsWindow_, nullptr, FALSE);
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
        createCommandButton(hwnd, ID_REFRESH, L"Refresh", 10, 0, 104, 28, instance_);
        createCommandButton(hwnd, ID_SWITCH, L"Switch", 124, 0, 104, 28, instance_);
        createCommandButton(hwnd, ID_SETTINGS, L"Config", 238, 0, 104, 28, instance_);
        styleWindowControls(hwnd);
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
    case WM_MOUSEMOVE: {
        int row = monitorRowIndexAt(GET_Y_LPARAM(lparam));
        if (row != hoverMonitorRow_) {
            hoverMonitorRow_ = row;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        if (!trackingMonitorMouse_) {
            TRACKMOUSEEVENT event{sizeof(event), TME_LEAVE, hwnd, 0};
            trackingMonitorMouse_ = TrackMouseEvent(&event) == TRUE;
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        trackingMonitorMouse_ = false;
        hoverMonitorRow_ = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
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
        case ID_TRAY_TOGGLE:
            ShowWindow(hwnd, IsWindowVisible(hwnd) ? SW_HIDE : SW_SHOWNORMAL);
            break;
        case ID_TRAY_CONFIG:
            createSettingsWindow();
            break;
        case ID_TRAY_EXIT:
            if (settingsWindow_) DestroyWindow(settingsWindow_);
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    case WM_TIMER:
        if (wparam == kPollTimer) refreshMonitorRows(true);
        return 0;
    case kTrayMessage:
        if (lparam == WM_LBUTTONUP) {
            ShowWindow(hwnd, IsWindowVisible(hwnd) ? SW_HIDE : SW_SHOWNORMAL);
        } else if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU) {
            showTrayMenu();
        }
        return 0;
    case WM_PAINT:
        paintMonitor(hwnd);
        return 0;
    case WM_DRAWITEM:
        if (drawOwnerButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam))) return TRUE;
        break;
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
        if (app) app->settingsWindow_ = hwnd;
    }
    return app ? app->handleSettings(hwnd, message, wparam, lparam) : DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT NativeWindowsApp::handleSettings(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        settingsWindow_ = hwnd;
        CreateWindowW(L"STATIC", L"Profiles", WS_CHILD | WS_VISIBLE, 16, 18, 180, 22, hwnd, nullptr, instance_, nullptr);
        CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 16, 64, 326, 232, hwnd, reinterpret_cast<HMENU>(ID_PROFILE_LIST), instance_, nullptr);
        createCommandButton(hwnd, ID_IMPORT_CURRENT, L"Import Current", 16, 304, 102, 28, instance_);
        createCommandButton(hwnd, ID_IMPORT_FILE, L"Import File", 126, 304, 102, 28, instance_);
        createCommandButton(hwnd, ID_NEW_PROFILE, L"New Profile", 236, 304, 102, 28, instance_);
        createCommandButton(hwnd, ID_DELETE_PROFILE, L"Delete", 16, 338, 102, 28, instance_);
        createCommandButton(hwnd, ID_PIN_PROFILE, L"Pin", 126, 338, 102, 28, instance_);
        createCommandButton(hwnd, ID_SWITCH_PROFILE, L"Switch", 236, 338, 102, 28, instance_);
        createCommandButton(hwnd, ID_EXPORT_BACKUP, L"Export", 16, 378, 102, 28, instance_);
        createCommandButton(hwnd, ID_IMPORT_BACKUP, L"Import", 126, 378, 102, 28, instance_);
        createCommandButton(hwnd, ID_RESTORE_BACKUP, L"Restore", 236, 378, 102, 28, instance_);
        CreateWindowW(L"STATIC", L"Alias", WS_CHILD | WS_VISIBLE, 16, 424, 50, 22, hwnd, reinterpret_cast<HMENU>(ID_ALIAS_LABEL), instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 70, 422, 272, 24, hwnd, reinterpret_cast<HMENU>(ID_ALIAS_EDIT), instance_, nullptr);
        createCommandButton(hwnd, ID_SAVE_PROFILE, L"Save Profile", 16, 456, 154, 28, instance_);
        createCommandButton(hwnd, ID_SAVE_SETTINGS, L"Save Settings", 184, 456, 154, 28, instance_);

        createCommandButton(hwnd, ID_TAB_AUTH, L"Auth", 358, 64, 86, 30, instance_);
        createCommandButton(hwnd, ID_TAB_QUOTA, L"Quota", 450, 64, 86, 30, instance_);
        createCommandButton(hwnd, ID_TAB_USAGE, L"Usage", 542, 64, 86, 30, instance_);
        createCommandButton(hwnd, ID_TAB_SETTINGS, L"Settings", 634, 64, 86, 30, instance_);
        createCommandButton(hwnd, ID_TAB_HEALTH, L"Health", 726, 64, 86, 30, instance_);
        createCommandButton(hwnd, ID_TAB_UPDATES, L"Updates", 818, 64, 86, 30, instance_);
        settingsTab_ = 1;
        CreateWindowW(L"STATIC", L"Saved auth.json", WS_CHILD | WS_VISIBLE, 370, 104, 220, 20, hwnd, reinterpret_cast<HMENU>(ID_AUTH_LABEL), instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL, 370, 128, 560, 400, hwnd, reinterpret_cast<HMENU>(ID_AUTH_EDIT), instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 370, 104, 560, 424, hwnd, reinterpret_cast<HMENU>(ID_DETAILS_EDIT), instance_, nullptr);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 370, 104, 560, 424, hwnd, reinterpret_cast<HMENU>(ID_USAGE_EDIT), instance_, nullptr);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 370, 104, 560, 424, hwnd, reinterpret_cast<HMENU>(ID_HEALTH_EDIT), instance_, nullptr);

        CreateWindowW(L"STATIC", L"Refresh every", WS_CHILD | WS_VISIBLE, 370, 104, 110, 22, hwnd, reinterpret_cast<HMENU>(ID_POLL_LABEL), instance_, nullptr);
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
        createCommandButton(hwnd, ID_CHECK_UPDATES, L"Check Updates", 370, 104, 140, 28, instance_);
        CreateWindowW(L"STATIC", L"Current version: 0.7.0", WS_CHILD | WS_VISIBLE, 370, 146, 560, 80, hwnd, reinterpret_cast<HMENU>(ID_UPDATE_STATUS), instance_, nullptr);
        CreateWindowW(L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE, 16, 590, 920, 22, hwnd, reinterpret_cast<HMENU>(ID_STATUS_TEXT), instance_, nullptr);

        styleWindowControls(hwnd);
        SendMessageW(control(ID_PROFILE_LIST), LB_SETITEMHEIGHT, 0, 26);
        selectComboValue(poll, settings_.pollIntervalMinutes);
        selectComboValue(five, settings_.fiveHourAlertThreshold);
        selectComboValue(weekly, settings_.weeklyAlertThreshold);
        SendMessageW(control(ID_AUTO_RESTART), BM_SETCHECK, settings_.autoRestartCodex ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(control(ID_STARTUP), BM_SETCHECK, settings_.startAtLogin ? BST_CHECKED : BST_UNCHECKED, 0);
        updateProfileList();
        updateQuotaDetailsText();
        setControlText(ID_USAGE_EDIT, "Open this tab to calculate local usage.");
        setControlText(ID_HEALTH_EDIT, "Open this tab to run health checks.");
        updateSettingsTabVisibility();
        layoutSettingsWindow();
        return 0;
    }
    case WM_SIZE:
        layoutSettingsWindow();
        return 0;
    case WM_ERASEBKGND:
        paintSettingsBackground(hwnd, reinterpret_cast<HDC>(wparam));
        return 1;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        Theme theme = currentTheme();
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, theme.text);
        SetBkColor(dc, theme.panel);
        return reinterpret_cast<LRESULT>(settingsBackgroundBrush_);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        Theme theme = currentTheme();
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, theme.text);
        SetBkColor(dc, theme.control);
        return reinterpret_cast<LRESULT>(controlBackgroundBrush_);
    }
    case WM_COMMAND:
        if (int tab = settingsTabIndexFromControlId(LOWORD(wparam)); tab >= 0) {
            settingsTab_ = tab;
            updateSettingsTabVisibility();
            return 0;
        }
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
    case WM_TIMER:
        if (wparam == kUsageAnimationTimer) {
            ++usageSpinnerFrame_;
            HWND panel = control(ID_USAGE_EDIT);
            if (panel) InvalidateRect(panel, nullptr, FALSE);
            return 0;
        }
        break;
    case kUsageLoadedMessage: {
        auto* summary = reinterpret_cast<LocalUsageSummary*>(lparam);
        if (summary) {
            usageSummary_ = std::move(*summary);
            delete summary;
        }
        usageLoaded_ = true;
        usageLoading_ = false;
        KillTimer(hwnd, kUsageAnimationTimer);
        HWND panel = control(ID_USAGE_EDIT);
        if (panel) InvalidateRect(panel, nullptr, TRUE);
        return 0;
    }
    case WM_DRAWITEM:
        if (drawOwnerTab(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam))) return TRUE;
        if (drawOwnerListBox(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam))) return TRUE;
        if (drawOwnerUsagePanel(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam))) return TRUE;
        if (drawOwnerButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam))) return TRUE;
        break;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kUsageAnimationTimer);
        usageLoading_ = false;
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
    updateQuotaDetailsText();
    if (settingsTab_ == 2 && usageLoaded_) startLocalUsageLoad();
    if (settingsTab_ == 4 && healthLoaded_) updateHealthText();
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
    InvalidateRect(list, nullptr, TRUE);
}

void NativeWindowsApp::loadSelectedProfileEditor() {
    Profile* profile = selectedProfile();
    if (!profile) {
        setControlText(ID_ALIAS_EDIT, "");
        setControlText(ID_AUTH_EDIT, "");
        return;
    }
    setControlText(ID_ALIAS_EDIT, profile->alias);
    if (settingsTab_ != 0) {
        setControlText(ID_AUTH_EDIT, "Open the Auth JSON tab to edit this profile auth.");
        return;
    }
    try {
        setControlText(ID_AUTH_EDIT, store_.readAuth(profile->id));
    } catch (...) {
        setControlText(ID_AUTH_EDIT, "");
    }
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
    startLocalUsageLoad();
}

void NativeWindowsApp::startLocalUsageLoad() {
    if (!settingsWindow_ || usageLoading_) return;
    usageLoading_ = true;
    usageSpinnerFrame_ = 0;
    SetTimer(settingsWindow_, kUsageAnimationTimer, 120, nullptr);
    HWND panel = control(ID_USAGE_EDIT);
    if (panel) InvalidateRect(panel, nullptr, TRUE);
    HWND target = settingsWindow_;
    std::thread([target]() {
        auto* summary = new LocalUsageSummary(scanLocalUsage(defaultCodexRoot()));
        if (!PostMessageW(target, kUsageLoadedMessage, 0, reinterpret_cast<LPARAM>(summary))) {
            delete summary;
        }
    }).detach();
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
    Theme theme = currentTheme();
    fillRect(dc, client, theme.monitorBackground);
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, titleFont_ ? titleFont_ : GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(dc, theme.text);
    RECT title{14, 8, 146, 28};
    drawTextUtf8(dc, "Codex Quota", title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(dc, smallFont_ ? smallFont_ : GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(dc, theme.subtle);
    RECT status{150, 8, client.right - 14, 28};
    std::ostringstream statusText;
    statusText << monitorRows_.size() << "  " << status_;
    drawTextUtf8(dc, statusText.str(), status, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, uiFont_ ? uiFont_ : GetStockObject(DEFAULT_GUI_FONT));
    int y = kMonitorHeaderHeight;
    int bottomLimit = std::max(y, static_cast<int>(client.bottom) - kMonitorActionHeight - 4);
    if (monitorRows_.empty()) {
        SetTextColor(dc, theme.muted);
        RECT empty{18, y + 12, client.right - 18, bottomLimit};
        drawTextUtf8(dc, "No profiles yet. Open Config to import auth.", empty, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    }

    std::string active = activeAccountId();
    for (size_t i = 0; i < monitorRows_.size(); ++i) {
        const MonitorRow& row = monitorRows_[i];
        if (y + kMonitorRowHeight - 8 > bottomLimit) break;
        bool selected = row.profile.id == selectedProfileId_;
        bool hovered = static_cast<int>(i) == hoverMonitorRow_;
        bool activeProfile = !active.empty() && row.profile.accountId == active;
        bool warning = row.quota.belowThreshold(settings_.fiveHourAlertThreshold, settings_.weeklyAlertThreshold);
        COLORREF boxColor = theme.card;
        if (warning) boxColor = theme.cardWarning;
        if (hovered) boxColor = theme.cardHover;
        if (selected) boxColor = theme.cardSelected;
        RECT box{12, y, client.right - 12, y + kMonitorRowHeight - 8};
        drawRoundRect(dc, box, 14, boxColor, selected ? theme.accent : theme.border);
        if (selected) {
            RECT accent{box.left + 1, box.top + 10, box.left + 4, box.bottom - 10};
            drawRoundRect(dc, accent, 4, theme.accent, theme.accent);
        }
        int savedDc = SaveDC(dc);
        IntersectClipRect(dc, box.left + 1, box.top + 1, box.right - 1, box.bottom - 1);

        int contentLeft = box.left + 12;
        int contentRight = box.right - 10;

        SelectObject(dc, titleFont_ ? titleFont_ : GetStockObject(DEFAULT_GUI_FONT));
        SetTextColor(dc, theme.text);
        std::string name = row.profile.alias;
        RECT alias{contentLeft, y + 7, box.right - 116, y + 25};
        drawTextUtf8(dc, name, alias, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(dc, smallFont_ ? smallFont_ : GetStockObject(DEFAULT_GUI_FONT));
        int pillRight = contentRight;
        if (row.profile.pinned) {
            RECT pill{pillRight - 48, y + 7, pillRight, y + 25};
            drawPill(dc, "pin", pill, theme, false);
            pillRight -= 54;
        }
        if (activeProfile) {
            RECT pill{pillRight - 68, y + 7, pillRight, y + 25};
            drawPill(dc, "current", pill, theme, true);
        }

        SelectObject(dc, uiFont_ ? uiFont_ : GetStockObject(DEFAULT_GUI_FONT));
        if (!row.quota.error.empty()) {
            SetTextColor(dc, theme.warning);
            RECT error{contentLeft, y + 31, contentRight, y + 56};
            drawTextUtf8(dc, row.quota.error, error, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            SetTextColor(dc, theme.muted);
            RECT fiveText{contentLeft, y + 27, contentRight, y + 41};
            drawTextUtf8(dc, formatQuotaLine(row.quota.fiveHour, "5h: not refreshed"), fiveText, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT fiveBar{contentLeft, y + 42, contentRight, y + 45};
            drawQuotaBar(dc, row.quota.fiveHour, settings_.fiveHourAlertThreshold, fiveBar, theme);

            RECT weeklyText{contentLeft, y + 48, contentRight, y + 62};
            drawTextUtf8(dc, formatQuotaLine(row.quota.weekly, "weekly: not refreshed"), weeklyText, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT weeklyBar{contentLeft, y + 63, contentRight, y + 66};
            drawQuotaBar(dc, row.quota.weekly, settings_.weeklyAlertThreshold, weeklyBar, theme);
        }
        RestoreDC(dc, savedDc);
        y += kMonitorRowHeight;
    }
    SelectObject(dc, oldFont);
    EndPaint(hwnd, &ps);
}

void NativeWindowsApp::paintSettingsBackground(HWND hwnd, HDC dc) {
    RECT client{};
    GetClientRect(hwnd, &client);
    Theme theme = currentTheme();
    fillRect(dc, client, theme.windowBackground);
    HGDIOBJ oldFont = SelectObject(dc, titleFont_ ? titleFont_ : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT);

    int margin = 10;
    int leftWidth = 340;
    int rightX = margin + leftWidth + 8;
    RECT leftPanel{margin, 8, margin + leftWidth, client.bottom - 42};
    RECT rightPanel{rightX, 8, client.right - margin, client.bottom - 42};
    drawRoundRect(dc, leftPanel, 16, theme.panel, theme.border);
    drawRoundRect(dc, rightPanel, 16, theme.panel, theme.border);

    RECT footer{16, client.bottom - 31, client.right - 16, client.bottom - 10};
    SelectObject(dc, smallFont_ ? smallFont_ : GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(dc, theme.subtle);
    drawTextUtf8(dc, "Native Windows preview", footer, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);
}

bool NativeWindowsApp::drawOwnerButton(const DRAWITEMSTRUCT& item) {
    if (item.CtlType != ODT_BUTTON || !item.hwndItem) return false;
    Theme theme = currentTheme();
    bool pressed = (item.itemState & ODS_SELECTED) != 0;
    bool hot = (item.itemState & ODS_HOTLIGHT) != 0;
    bool disabled = (item.itemState & ODS_DISABLED) != 0;
    bool focused = (item.itemState & ODS_FOCUS) != 0;
    COLORREF fill = theme.control;
    COLORREF border = focused ? theme.accent : theme.controlBorder;
    COLORREF text = disabled ? theme.subtle : theme.text;
    if (hot) fill = theme.cardHover;
    if (pressed) fill = theme.cardSelected;

    RECT background = item.rcItem;
    fillRect(item.hDC, background, GetParent(item.hwndItem) == monitor_ ? theme.monitorBackground : theme.panel);
    RECT rect = item.rcItem;
    rect.right -= 1;
    rect.bottom -= 1;
    drawRoundRect(item.hDC, rect, 10, fill, border);
    wchar_t textBuffer[128]{};
    GetWindowTextW(item.hwndItem, textBuffer, static_cast<int>(sizeof(textBuffer) / sizeof(textBuffer[0])));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, text);
    HGDIOBJ oldFont = SelectObject(item.hDC, uiFont_ ? uiFont_ : GetStockObject(DEFAULT_GUI_FONT));
    DrawTextW(item.hDC, textBuffer, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(item.hDC, oldFont);
    return true;
}

bool NativeWindowsApp::drawOwnerTab(const DRAWITEMSTRUCT& item) {
    int tabIndex = settingsTabIndexFromControlId(item.CtlID);
    if (item.CtlType != ODT_BUTTON || tabIndex < 0 || !item.hwndItem) return false;
    Theme theme = currentTheme();
    RECT background = item.rcItem;
    fillRect(item.hDC, background, theme.panel);
    RECT rect = item.rcItem;
    rect.right -= 1;
    rect.bottom -= 1;
    bool selected = tabIndex == settingsTab_;
    bool hot = (item.itemState & ODS_HOTLIGHT) != 0;
    COLORREF fill = selected ? theme.cardSelected : (hot ? theme.cardHover : theme.panel);
    COLORREF border = selected ? theme.accent : theme.border;
    drawRoundRect(item.hDC, rect, 10, fill, border);

    wchar_t text[64]{};
    GetWindowTextW(item.hwndItem, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, selected ? theme.text : theme.muted);
    HGDIOBJ oldFont = SelectObject(item.hDC, uiFont_ ? uiFont_ : GetStockObject(DEFAULT_GUI_FONT));
    RECT textRect = rect;
    int centerY = (rect.top + rect.bottom) / 2;
    RECT iconRect{rect.left + 10, centerY - 8, rect.left + 26, centerY + 8};
    if (tabIndex >= 0 && tabIndex < 6 && tabIcons_[tabIndex]) {
        DrawIconEx(item.hDC, iconRect.left, iconRect.top, tabIcons_[tabIndex], 16, 16, 0, nullptr, DI_NORMAL);
    } else {
        drawTabGlyph(item.hDC, tabIndex, iconRect, theme, selected);
    }
    textRect.left += 24;
    textRect.right -= 4;
    DrawTextW(item.hDC, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(item.hDC, oldFont);
    return true;
}

bool NativeWindowsApp::drawOwnerListBox(const DRAWITEMSTRUCT& item) {
    if (item.CtlType != ODT_LISTBOX || item.CtlID != ID_PROFILE_LIST) return false;
    Theme theme = currentTheme();
    RECT rect = item.rcItem;
    fillRect(item.hDC, rect, theme.panel);
    if (item.itemID == static_cast<UINT>(-1)) return true;

    bool selected = (item.itemState & ODS_SELECTED) != 0;
    bool focused = (item.itemState & ODS_FOCUS) != 0;
    RECT itemRect = rect;
    itemRect.left += 4;
    itemRect.right -= 4;
    itemRect.top += 2;
    itemRect.bottom -= 2;
    COLORREF fill = selected ? theme.cardSelected : theme.panel;
    COLORREF border = focused ? theme.accent : fill;
    drawRoundRect(item.hDC, itemRect, 8, fill, border);

    int textLen = static_cast<int>(SendMessageW(item.hwndItem, LB_GETTEXTLEN, item.itemID, 0));
    std::wstring text(static_cast<size_t>(std::max(0, textLen)) + 1, L'\0');
    SendMessageW(item.hwndItem, LB_GETTEXT, item.itemID, reinterpret_cast<LPARAM>(text.data()));
    text.resize(static_cast<size_t>(std::max(0, textLen)));

    RECT textRect = itemRect;
    textRect.left += 8;
    textRect.right -= 8;
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, selected ? theme.text : theme.muted);
    HGDIOBJ oldFont = SelectObject(item.hDC, uiFont_ ? uiFont_ : GetStockObject(DEFAULT_GUI_FONT));
    DrawTextW(item.hDC, text.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(item.hDC, oldFont);
    return true;
}

bool NativeWindowsApp::drawOwnerUsagePanel(const DRAWITEMSTRUCT& item) {
    if (item.CtlType != ODT_STATIC || item.CtlID != ID_USAGE_EDIT) return false;
    Theme theme = currentTheme();
    RECT rect = item.rcItem;
    fillRect(item.hDC, rect, theme.panel);
    SetBkMode(item.hDC, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(item.hDC, uiFont_ ? uiFont_ : GetStockObject(DEFAULT_GUI_FONT));

    if (usageLoading_ && !usageLoaded_) {
        RECT card{
            rect.left + (rect.right - rect.left) / 2 - 150,
            rect.top + (rect.bottom - rect.top) / 2 - 54,
            rect.left + (rect.right - rect.left) / 2 + 150,
            rect.top + (rect.bottom - rect.top) / 2 + 54
        };
        drawRoundRect(item.hDC, card, 12, theme.card, theme.border);
        POINT center{card.left + 42, card.top + 54};
        drawLoadingSpinner(item.hDC, center, usageSpinnerFrame_, theme);
        SelectObject(item.hDC, titleFont_ ? titleFont_ : GetStockObject(DEFAULT_GUI_FONT));
        SetTextColor(item.hDC, theme.text);
        RECT title{card.left + 72, card.top + 30, card.right - 18, card.top + 50};
        drawTextUtf8(item.hDC, "Loading usage", title, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(item.hDC, smallFont_ ? smallFont_ : GetStockObject(DEFAULT_GUI_FONT));
        SetTextColor(item.hDC, theme.subtle);
        RECT sub{card.left + 72, card.top + 54, card.right - 18, card.top + 76};
        drawTextUtf8(item.hDC, "Scanning local Codex history...", sub, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(item.hDC, oldFont);
        return true;
    }

    if (!usageLoaded_) {
        SetTextColor(item.hDC, theme.muted);
        drawTextUtf8(item.hDC, "Open this tab to calculate local usage.", rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(item.hDC, oldFont);
        return true;
    }

    const int gap = 10;
    const int cardHeight = 62;
    const int cardWidth = (rect.right - rect.left - gap * 3) / 4;
    struct Metric {
        const char* title;
        int64_t value;
    };
    Metric metrics[] = {
        {"Today", usageSummary_.today.total},
        {"7 days", usageSummary_.last7Days.total},
        {"30 days", usageSummary_.last30Days.total},
        {"All", usageSummary_.total.total},
    };
    for (int i = 0; i < 4; ++i) {
        RECT card{rect.left + i * (cardWidth + gap), rect.top, rect.left + i * (cardWidth + gap) + cardWidth, rect.top + cardHeight};
        drawRoundRect(item.hDC, card, 12, theme.card, theme.border);
        RECT title{card.left + 10, card.top + 8, card.right - 10, card.top + 26};
        SelectObject(item.hDC, smallFont_ ? smallFont_ : GetStockObject(DEFAULT_GUI_FONT));
        SetTextColor(item.hDC, theme.subtle);
        drawTextUtf8(item.hDC, metrics[i].title, title, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT value{card.left + 10, card.top + 27, card.right - 10, card.bottom - 8};
        SelectObject(item.hDC, titleFont_ ? titleFont_ : GetStockObject(DEFAULT_GUI_FONT));
        SetTextColor(item.hDC, theme.text);
        drawTextUtf8(item.hDC, formatNumber(metrics[i].value), value, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    RECT daily{rect.left, rect.top + cardHeight + 12, rect.right, rect.top + cardHeight + 204};
    drawRoundRect(item.hDC, daily, 12, theme.card, theme.border);
    RECT dailyTitle{daily.left + 14, daily.top + 10, daily.right - 14, daily.top + 30};
    SelectObject(item.hDC, titleFont_ ? titleFont_ : GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(item.hDC, theme.text);
    drawTextUtf8(item.hDC, "Daily usage", dailyTitle, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(item.hDC, smallFont_ ? smallFont_ : GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(item.hDC, theme.subtle);
    RECT dailySub{daily.left + 14, daily.top + 30, daily.right - 14, daily.top + 48};
    drawTextUtf8(item.hDC, "Last 7 days on this machine", dailySub, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    auto usageForDay = [&](const std::string& key) {
        for (const auto& day : usageSummary_.byDay) {
            if (day.day == key) return day.usage;
        }
        return UsageTotals{};
    };
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    std::time_t todayStart = std::mktime(&local);
    UsageTotals bars[7]{};
    int64_t maxTotal = 0;
    for (int i = 0; i < 7; ++i) {
        std::time_t dayTime = todayStart - static_cast<std::time_t>((6 - i) * 24 * 60 * 60);
        bars[i] = usageForDay(formatDayKey(dayTime));
        maxTotal = std::max(maxTotal, bars[i].total);
    }
    RECT chart{daily.left + 18, daily.top + 56, daily.right - 18, daily.bottom - 28};
    int slot = (chart.right - chart.left) / 7;
    int barMaxHeight = chart.bottom - chart.top - 24;
    for (int i = 0; i < 7; ++i) {
        std::time_t dayTime = todayStart - static_cast<std::time_t>((6 - i) * 24 * 60 * 60);
        int x = chart.left + i * slot;
        int barWidth = std::max(12, slot / 2);
        int barHeight = maxTotal > 0 ? static_cast<int>(static_cast<double>(bars[i].total) / static_cast<double>(maxTotal) * barMaxHeight) : 3;
        barHeight = std::clamp(barHeight, 3, barMaxHeight);
        RECT bar{x + (slot - barWidth) / 2, chart.top + barMaxHeight - barHeight, x + (slot + barWidth) / 2, chart.top + barMaxHeight};
        bool today = i == 6;
        drawRoundRect(item.hDC, bar, 6, today ? theme.accent : theme.success, today ? theme.accent : theme.success);
        RECT value{x, chart.top, x + slot, chart.top + 18};
        SetTextColor(item.hDC, theme.muted);
        drawTextUtf8(item.hDC, formatCompactNumber(bars[i].total), value, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT label{x, chart.bottom - 16, x + slot, chart.bottom};
        drawTextUtf8(item.hDC, formatDayLabel(dayTime), label, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    RECT overall{rect.left, daily.bottom + 12, rect.right, rect.bottom - 34};
    drawRoundRect(item.hDC, overall, 12, theme.card, theme.border);
    RECT overallTitle{overall.left + 14, overall.top + 10, overall.right - 14, overall.top + 30};
    SelectObject(item.hDC, titleFont_ ? titleFont_ : GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(item.hDC, theme.text);
    drawTextUtf8(item.hDC, "Overall token mix", overallTitle, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    int64_t uncachedInput = std::max<int64_t>(0, usageSummary_.total.input - usageSummary_.total.cachedInput);
    struct Segment {
        const char* name;
        int64_t value;
        COLORREF color;
    };
    Segment segments[] = {
        {"Input", uncachedInput, RGB(65, 143, 220)},
        {"Cached", usageSummary_.total.cachedInput, RGB(75, 178, 121)},
        {"Output", usageSummary_.total.output, RGB(210, 143, 64)},
        {"Reasoning", usageSummary_.total.reasoningOutput, RGB(177, 99, 214)},
    };
    int64_t mixTotal = 0;
    for (const auto& segment : segments) mixTotal += segment.value;
    RECT mixBar{overall.left + 14, overall.top + 42, overall.right - 14, overall.top + 64};
    drawRoundRect(item.hDC, mixBar, 8, theme.barTrack, theme.barTrack);
    int x = mixBar.left;
    for (const auto& segment : segments) {
        int width = mixTotal > 0 ? static_cast<int>((mixBar.right - mixBar.left) * static_cast<double>(segment.value) / static_cast<double>(mixTotal)) : 0;
        if (width <= 0 && segment.value > 0) width = 2;
        RECT part{x, mixBar.top, std::min<LONG>(mixBar.right, static_cast<LONG>(x + width)), mixBar.bottom};
        if (part.right > part.left) drawRoundRect(item.hDC, part, 8, segment.color, segment.color);
        x += width;
    }
    SelectObject(item.hDC, smallFont_ ? smallFont_ : GetStockObject(DEFAULT_GUI_FONT));
    for (int i = 0; i < 4; ++i) {
        int col = i % 2;
        int row = i / 2;
        RECT label{overall.left + 14 + col * ((overall.right - overall.left) / 2), overall.top + 74 + row * 18, overall.left + 14 + (col + 1) * ((overall.right - overall.left) / 2) - 10, overall.top + 90 + row * 18};
        SetTextColor(item.hDC, segments[i].color);
        std::string text = std::string(segments[i].name) + "  " + formatCompactNumber(segments[i].value) + "  " + percentShare(segments[i].value, mixTotal);
        drawTextUtf8(item.hDC, text, label, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    if (usageLoading_) {
        POINT center{rect.right - 72, rect.top + 31};
        drawLoadingSpinner(item.hDC, center, usageSpinnerFrame_, theme);
        RECT loadingText{rect.right - 58, rect.top + 20, rect.right - 8, rect.top + 42};
        SetTextColor(item.hDC, theme.subtle);
        drawTextUtf8(item.hDC, "Updating", loadingText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    SelectObject(item.hDC, smallFont_ ? smallFont_ : GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(item.hDC, theme.subtle);
    RECT note{rect.left + 4, rect.bottom - 26, rect.right - 4, rect.bottom};
    std::string noteText = "Local usage only counts Codex token events on this machine.";
    if (usageSummary_.parseErrors > 0) noteText += " Parse warnings: " + std::to_string(usageSummary_.parseErrors) + ".";
    drawTextUtf8(item.hDC, noteText, note, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

    SelectObject(item.hDC, oldFont);
    return true;
}

int NativeWindowsApp::monitorRowIndexAt(int y) const {
    int rowIndex = (y - kMonitorHeaderHeight) / kMonitorRowHeight;
    if (rowIndex < 0 || rowIndex >= static_cast<int>(monitorRows_.size())) return -1;
    int rowTop = kMonitorHeaderHeight + rowIndex * kMonitorRowHeight;
    if (y < rowTop || y > rowTop + kMonitorRowHeight - 8) return -1;
    return rowIndex;
}

void NativeWindowsApp::selectMonitorRowAt(int y) {
    int rowIndex = monitorRowIndexAt(y);
    if (rowIndex < 0) return;
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
