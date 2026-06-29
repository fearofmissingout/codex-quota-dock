#pragma once

#include "json.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cqd {

namespace fs = std::filesystem;

struct AuthMetadata {
    std::string authMode;
    std::string accountId;
    std::string accountSuffix;
    std::string accessToken;
    std::string lastRefresh;
};

struct Profile {
    std::string id;
    std::string alias;
    std::string accountId;
    std::string accountSuffix;
    std::string authMode;
    bool pinned = false;
    std::string lastRefresh;
    std::string createdAt;
};

struct AppSettings {
    int pollIntervalMinutes = 5;
    int fiveHourAlertThreshold = 10;
    int weeklyAlertThreshold = 30;
    bool autoRestartCodex = false;
    bool showRestartReminder = true;
    bool checkUpdatesOnStartup = true;
    bool startAtLogin = false;
};

struct QuotaWindow {
    std::string label;
    std::optional<int> remainingPercent;
    int64_t resetsAt = 0;
};

struct QuotaSnapshot {
    QuotaWindow fiveHour;
    QuotaWindow weekly;
    std::string planType;
    std::string error;

    int effectiveRemainingPercent() const;
    bool belowThreshold(int fiveHourThreshold, int weeklyThreshold) const;
};

struct SwitchResult {
    fs::path activeAuthPath;
    fs::path backupPath;
};

struct BackupImportSummary {
    int created = 0;
    int updated = 0;
    int skipped = 0;
};

struct UsageTotals {
    int64_t input = 0;
    int64_t cachedInput = 0;
    int64_t output = 0;
    int64_t reasoningOutput = 0;
    int64_t total = 0;

    void add(const UsageTotals& other);
};

struct LocalUsageSummary {
    UsageTotals total;
    UsageTotals today;
    UsageTotals last7Days;
    UsageTotals last30Days;
    int sessionCount = 0;
    int parseErrors = 0;
};

struct ReleaseAsset {
    std::string name;
    std::string browserDownloadUrl;
    int64_t size = 0;
};

struct UpdateCheckResult {
    bool available = false;
    std::string current;
    std::string latest;
    std::string reason;
    ReleaseAsset asset;
    std::string releaseUrl;
};

struct HealthRow {
    std::string status;
    std::string label;
    std::string detail;
};

std::wstring utf8ToWide(std::string_view text);
std::string wideToUtf8(std::wstring_view text);
std::string trim(std::string value);
std::string lower(std::string value);
std::string nowIsoUtc();
std::string timestampForFile();
std::string readTextFile(const fs::path& path);
void writeTextFileAtomic(const fs::path& path, std::string_view data);

fs::path configRoot();
fs::path defaultCodexAuthPath();
fs::path defaultCodexRoot();

AuthMetadata parseAuthMetadata(std::string_view authJson, bool requireAccessToken = true);
QuotaSnapshot parseQuotaPayload(std::string_view json);
AppSettings loadSettings(const fs::path& configDir);
void saveSettings(const fs::path& configDir, const AppSettings& settings);

class ProfileStore {
public:
    explicit ProfileStore(fs::path root);

    const fs::path& root() const { return root_; }
    fs::path profilesFile() const;
    fs::path profilesDir() const;
    fs::path backupsDir() const;
    fs::path authPath(const std::string& profileId) const;

    void load();
    void save() const;
    const std::vector<Profile>& profiles() const { return profiles_; }

    std::string readAuth(const std::string& profileId) const;
    Profile importAuth(std::string alias, std::string authJson, bool updateExistingAccount);
    Profile updateProfile(const std::string& profileId, std::string alias, std::string authJson);
    Profile setPinned(const std::string& profileId, bool pinned);
    void deleteProfile(const std::string& profileId);
    std::string suggestAlias(std::string prefix, std::string_view authJson) const;
    std::optional<Profile> findById(const std::string& profileId) const;
    std::optional<Profile> findByAlias(const std::string& alias) const;
    std::optional<Profile> findByAccountId(const std::string& accountId) const;

private:
    bool aliasExists(const std::string& alias, const std::string& exceptId = {}) const;
    static Profile profileFromJson(const JsonValue& value);
    static JsonValue profileToJson(const Profile& profile);

    fs::path root_;
    std::vector<Profile> profiles_;
};

SwitchResult switchAuth(const fs::path& activeAuthPath, const fs::path& profileAuthPath, const fs::path& backupDir);
std::string exportBackup(ProfileStore& store, const AppSettings& settings);
BackupImportSummary importBackup(ProfileStore& store, std::string_view backupJson);
std::optional<fs::path> latestBackup(const fs::path& backupDir);
void restoreBackup(const fs::path& backupPath, const fs::path& activeAuthPath);

QuotaSnapshot fetchQuota(std::string_view authJson);
UpdateCheckResult checkForUpdates(std::string_view currentVersion);
bool isNewerVersion(std::string_view current, std::string_view latest);

bool startupEnabled();
void setStartupEnabled(bool enabled);

std::string restartCodex();
LocalUsageSummary scanLocalUsage(const fs::path& codexRoot);
std::vector<HealthRow> runHealthCheck(ProfileStore& store, const fs::path& activeAuthPath, std::string_view version);

} // namespace cqd
