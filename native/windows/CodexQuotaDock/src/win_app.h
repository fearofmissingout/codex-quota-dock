#pragma once

#include "core.h"

#include <Windows.h>
#include <shellapi.h>

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
    void saveSettingsFromControls();
    void refreshMonitorRows(bool fetchQuotaValues);
    void updateProfileList();
    void loadSelectedProfileEditor();
    void updateQuotaDetailsText();
    void updateLocalUsageText();
    void startLocalUsageLoad();
    void updateHealthText();
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

    Profile* selectedProfile();
    std::string selectedProfileId() const;
    std::string activeAccountId() const;
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
    LocalUsageSummary usageSummary_{};
    std::string selectedProfileId_;
    std::string status_ = "Ready";
    int settingsTab_ = 0;
    int hoverMonitorRow_ = -1;
    int usageSpinnerFrame_ = 0;
    bool trackingMonitorMouse_ = false;
    bool usageLoaded_ = false;
    bool usageLoading_ = false;
    bool healthLoaded_ = false;
};

} // namespace cqd
