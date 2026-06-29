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
    void applyWindows11Style(HWND hwnd, bool floating);

    void loadState();
    void saveSettingsFromControls();
    void refreshMonitorRows(bool fetchQuotaValues);
    void updateProfileCombo();
    void updateProfileList();
    void loadSelectedProfileEditor();
    void updateHealthAndUsageText();
    void paintMonitor(HWND hwnd);

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
    ProfileStore store_{configRoot()};
    AppSettings settings_{};
    std::vector<MonitorRow> monitorRows_;
    std::string selectedProfileId_;
    std::string status_ = "Ready";
};

} // namespace cqd
