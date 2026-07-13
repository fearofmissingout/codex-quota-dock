#pragma once

#include "core.h"

#include <Windows.h>
#include <shellapi.h>

#include <map>
#include <string>
#include <vector>

namespace cqd {

class NativeWindowsApp {
public:
    int run(HINSTANCE instance, int showCommand);

private:
    struct MonitorRow {
        Profile profile;
        QuotaSnapshot quota;
    };

    static LRESULT CALLBACK MonitorProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK SettingsProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    LRESULT handleMonitor(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handleSettings(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    void createMonitorWindow(int showCommand);
    void createSettingsWindow();
    void createTrayIcon();
    void removeTrayIcon();
    void showTrayMenu();
    void createVisualResources();
    void destroyVisualResources();
    void applyWindows11Style(HWND hwnd, bool floating);
    void styleWindowControls(HWND parent);
    void layoutMonitorWindow();
    void resizeMonitorWindow();
    void layoutSettingsWindow();
    void updateSettingsTabVisibility();

    void loadState();
    void syncSettingsFromControls();
    void saveSettingsFromControls(bool announce = true);
    void startQuotaRefresh();
    void refreshMonitorRows(bool fetchQuotaValues);
    void updateProfileList();
    void loadSelectedProfileEditor();
    void updateQuotaDetailsText();
    void updateLocalUsageText();
    void startLocalUsageLoad();
    void updateHealthText();
    void refreshCodexLogActivity(bool force = false);
    void evaluateAutoSwitch();
    void paintMonitor(HWND hwnd);
    void paintSettingsBackground(HWND hwnd, HDC dc);
    bool drawOwnerButton(const DRAWITEMSTRUCT& item);
    bool drawOwnerTab(const DRAWITEMSTRUCT& item);
    bool drawOwnerListBox(const DRAWITEMSTRUCT& item);
    bool drawOwnerUsagePanel(const DRAWITEMSTRUCT& item);
    int monitorRowIndexAt(int y) const;
    void selectMonitorRowAt(int y);

    void selectProfileByIndex(int index);
    void importCurrentProfile();
    void importProfileFile();
    void newProfileFromEditor();
    void saveSelectedProfile();
    void deleteSelectedProfile();
    void togglePinnedProfile();
    void switchSelectedProfile();
    void exportBackupFile();
    void importBackupFile();
    void restoreLatestBackup();
    void checkUpdates();
    void detectCodexPath();

    Profile* selectedProfile();
    std::string selectedProfileId() const;
    std::string activeAccountId() const;
    bool isCodexRunning() const;
    int systemIdleMinutes() const;
    std::vector<Profile> monitorProfiles() const;
    void showStatus(std::string message);
    void showError(const std::string& context, const std::exception& error);

    std::string controlText(int id) const;
    void setControlText(int id, std::string_view text);
    HWND control(int id) const;

    HINSTANCE instance_ = nullptr;
    HWND monitor_ = nullptr;
    HWND settingsWindow_ = nullptr;
    NOTIFYICONDATAW tray_{};
    HICON appIcon_ = nullptr;
    HICON appSmallIcon_ = nullptr;
    HICON tabIcons_[6]{};
    HFONT uiFont_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    HBRUSH settingsBackgroundBrush_ = nullptr;
    HBRUSH controlBackgroundBrush_ = nullptr;
    ProfileStore store_{configRoot()};
    AppSettings settings_{};
    std::vector<MonitorRow> monitorRows_;
    std::map<std::string, QuotaSnapshot> quotaByProfileId_;
    LocalUsageSummary usageSummary_{};
    CodexLogActivitySummary logActivitySummary_{};
    std::string selectedProfileId_;
    std::string status_ = "Ready";
    int settingsTab_ = 0;
    int hoverMonitorRow_ = -1;
    int usageSpinnerFrame_ = 0;
    bool trackingMonitorMouse_ = false;
    bool usageLoaded_ = false;
    bool usageLoading_ = false;
    bool quotaRefreshLoading_ = false;
    bool healthLoaded_ = false;
    int64_t lastAutoSwitchUnix_ = 0;
    int64_t lastCodexLogScanUnix_ = 0;
};

} // namespace cqd
