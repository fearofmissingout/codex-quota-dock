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
    std::string idToken;
    std::string refreshToken;
    std::string lastRefresh;
};

struct Profile {
    std::string id;
    std::string alias;
    std::string accountId;
    std::string accountSuffix;
    std::string authMode;
    bool pinned = false;
    int priority = 0;
    bool autoSwitchAllowed = true;
    std::string lastRefresh;
    std::string createdAt;
};

enum class AutoSwitchMode {
    Off = 0,
    Notify = 1,
    WhenCodexClosed = 2,
    WhenIdle = 3,
};

struct AppSettings {
    int pollIntervalMinutes = 5;
    int fiveHourAlertThreshold = 10;
    int weeklyAlertThreshold = 30;
    bool autoRestartCodex = false;
    bool showRestartReminder = true;
    bool checkUpdatesOnStartup = true;
    bool startAtLogin = false;
    AutoSwitchMode autoSwitchMode = AutoSwitchMode::Off;
    int autoSwitchIdleMinutes = 5;
    int autoSwitchCooldownMinutes = 15;
    int switchAwayThreshold = 5;
    int switchToThreshold = 30;
    bool quotaPriorityMode = false;
    int quotaPriorityFiveHourThreshold = 99;
    int quotaPriorityWeeklyThreshold = 0;
    std::string codexLaunchPath;
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
    int64_t uncachedInput() const;
    int64_t effectiveTotal() const;
};

struct LocalUsageDay {
    std::string day;
    UsageTotals usage;
};

struct LocalUsageSummary {
    UsageTotals total;
    UsageTotals today;
    UsageTotals last7Days;
    UsageTotals last30Days;
    UsageTotals sqlite;
    std::vector<LocalUsageDay> byDay;
    int sessionCount = 0;
    int sqliteThreadCount = 0;
    int sqliteDatabaseCount = 0;
    int parseErrors = 0;
};

struct CodexLogActivitySummary {
    bool databaseExists = false;
    bool traceInsertBlocked = false;
    bool codexResponding = false;
    int64_t walBytes = 0;
    int64_t lastLogUnix = 0;
    int64_t lastTraceUnix = 0;
    int recentTraceCount = 0;
    int recentDebugCount = 0;
    int recentInfoCount = 0;
    int recentWarnCount = 0;
    int recentErrorCount = 0;
    int todayActiveMinutes = 0;
    int last7DaysActiveMinutes = 0;
    int threadCount = 0;
    int processCount = 0;
    std::string error;
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
    ReleaseAsset checksumAsset;
    std::string releaseUrl;
};

struct DownloadedUpdate {
    fs::path path;
    std::string assetName;
    std::string expectedSha256;
    std::string actualSha256;
};

struct NetworkProxySettings {
    bool enabled = false;
    std::string proxy;
    std::string bypass;
};

struct HealthRow {
    std::string status;
    std::string label;
    std::string detail;
};

enum class AutoSwitchAction {
    None = 0,
    Notify = 1,
    PendingUntilIdle = 2,
    SwitchNow = 3,
};

enum class AutoSwitchReason {
    CurrentQuotaLow = 0,
    PreferredProfileRecovered = 1,
    QuotaPriorityRecovered = 2,
};

struct AutoSwitchCandidate {
    std::string profileId;
    std::string alias;
    int priority = 0;
    bool autoSwitchAllowed = true;
    std::optional<int> fiveHourRemainingPercent;
    std::optional<int> weeklyRemainingPercent;
    bool isActive = false;
};

struct AutoSwitchContext {
    bool codexRunning = false;
    int idleMinutes = 0;
    int64_t lastSwitchUnix = 0;
    int64_t nowUnix = 0;
};

struct AutoSwitchDecision {
    AutoSwitchAction action = AutoSwitchAction::None;
    std::string targetProfileId;
    AutoSwitchReason reason = AutoSwitchReason::CurrentQuotaLow;
};

enum class CodexLaunchKind {
    AppUserModelId = 0,
    ExecutablePath = 1,
    Protocol = 2,
};

struct CodexLaunchTarget {
    CodexLaunchKind kind = CodexLaunchKind::AppUserModelId;
    std::wstring value;
};

std::wstring utf8ToWide(std::string_view text);
std::string wideToUtf8(std::wstring_view text);
std::string trim(std::string value);
std::string lower(std::string value);
std::string nowIsoUtc();
std::string timestampForFile();
std::string formatQuotaResetTime(int64_t epochSeconds);
std::string readTextFile(const fs::path& path);
void writeTextFileAtomic(const fs::path& path, std::string_view data);

fs::path configRoot();
fs::path defaultCodexAuthPath();
fs::path defaultCodexRoot();

AuthMetadata parseAuthMetadata(std::string_view authJson, bool requireAccessToken = true);
QuotaSnapshot parseQuotaPayload(std::string_view json);
AppSettings loadSettings(const fs::path& configDir);
void saveSettings(const fs::path& configDir, const AppSettings& settings);
std::string autoSwitchModeToString(AutoSwitchMode mode);
AutoSwitchMode autoSwitchModeFromString(std::string_view value);

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
    Profile updateAutomation(const std::string& profileId, int priority, bool autoSwitchAllowed);
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
QuotaSnapshot fetchQuotaFromAuthFile(const fs::path& authPath, const fs::path& activeAuthPath = {});
NetworkProxySettings proxySettingsFromEnv(std::string_view envText, std::string_view host, bool https);
UpdateCheckResult checkForUpdates(std::string_view currentVersion);
UpdateCheckResult parseWindowsUpdateRelease(std::string_view releaseJson, std::string_view currentVersion);
bool isNewerVersion(std::string_view current, std::string_view latest);
std::string checksumForAsset(std::string_view checksumsText, std::string_view assetName);
std::string sha256HexFile(const fs::path& path);
DownloadedUpdate downloadWindowsUpdate(const UpdateCheckResult& update, const fs::path& updatesDir);
void launchWindowsUpdateInstaller(const DownloadedUpdate& update);
int runWindowsUpdateInstaller(const std::vector<std::wstring>& args);

bool startupEnabled();
void setStartupEnabled(bool enabled);

bool isCodexProcessName(std::wstring_view name);
bool isCodexProcessCandidate(std::wstring_view name, std::wstring_view imagePath);
CodexLaunchTarget detectCodexLaunchTarget();
CodexLaunchTarget codexLaunchTarget(const AppSettings& settings);
std::string restartCodex();
std::string restartCodex(const AppSettings& settings);
AutoSwitchDecision decideAutoSwitch(
    AutoSwitchMode mode,
    const AutoSwitchCandidate& current,
    const std::vector<AutoSwitchCandidate>& candidates,
    const AutoSwitchContext& context,
    int switchAwayThreshold,
    int switchToThreshold,
    int cooldownMinutes,
    int requiredIdleMinutes,
    bool quotaPriorityMode = false,
    int quotaPriorityFiveHourThreshold = 99,
    int quotaPriorityWeeklyThreshold = 0
);
LocalUsageSummary scanLocalUsage(const fs::path& codexRoot);
CodexLogActivitySummary scanCodexLogActivity(const fs::path& codexRoot, int64_t nowUnix = 0);
bool shouldReuseCodexLogCache(int64_t lastScanUnix, int64_t nowUnix, int minimumIntervalSeconds = 60);
std::vector<HealthRow> runHealthCheck(ProfileStore& store, const fs::path& activeAuthPath, std::string_view version);

} // namespace cqd
