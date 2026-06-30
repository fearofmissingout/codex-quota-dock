#include "core.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>

#include <Windows.h>
#include <ShlObj.h>
#include <Shellapi.h>
#include <TlHelp32.h>
#include <winhttp.h>

namespace cqd {
namespace {

constexpr const char* kVersion = "0.7.0";
constexpr const wchar_t* kStartupValueName = L"Codex Quota Dock";
constexpr const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

std::string jsonStringValue(const JsonValue* object, std::string_view key, std::string fallback = {}) {
    if (!object) return fallback;
    const JsonValue* value = object->get(key);
    return value ? value->asString(std::move(fallback)) : fallback;
}

bool jsonBoolValue(const JsonValue* object, std::string_view key, bool fallback = false) {
    if (!object) return fallback;
    const JsonValue* value = object->get(key);
    return value ? value->asBool(fallback) : fallback;
}

int jsonIntValue(const JsonValue* object, std::string_view key, int fallback = 0) {
    if (!object) return fallback;
    const JsonValue* value = object->get(key);
    return value && value->isNumber() ? static_cast<int>(value->asNumber()) : fallback;
}

int64_t jsonInt64Value(const JsonValue* object, std::string_view key, int64_t fallback = 0) {
    if (!object) return fallback;
    const JsonValue* value = object->get(key);
    return value && value->isNumber() ? static_cast<int64_t>(value->asNumber()) : fallback;
}

std::optional<std::chrono::system_clock::time_point> parseIsoTimestamp(std::string_view value) {
    if (value.size() < 19) return std::nullopt;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    std::string head(value.substr(0, 19));
    if (sscanf_s(head.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second) != 6) {
        return std::nullopt;
    }
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    std::time_t utc = _mkgmtime(&tm);
    if (utc == static_cast<std::time_t>(-1)) return std::nullopt;
    return std::chrono::system_clock::from_time_t(utc);
}

std::tm localTm(std::chrono::system_clock::time_point value) {
    std::time_t time = std::chrono::system_clock::to_time_t(value);
    std::tm local{};
    localtime_s(&local, &time);
    return local;
}

std::string localDayKey(std::chrono::system_clock::time_point value) {
    std::tm local = localTm(value);
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d");
    return out.str();
}

bool sameLocalDay(std::chrono::system_clock::time_point left, std::chrono::system_clock::time_point right) {
    std::tm a = localTm(left);
    std::tm b = localTm(right);
    return a.tm_year == b.tm_year && a.tm_mon == b.tm_mon && a.tm_mday == b.tm_mday;
}

std::string accountSuffix(std::string_view accountId) {
    if (accountId.size() <= 6) return std::string(accountId);
    return std::string(accountId.substr(accountId.size() - 6));
}

std::string randomHexId() {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 255);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) {
        out << std::setw(2) << dist(rd);
    }
    return out.str();
}

fs::path knownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &raw)) && raw) {
        fs::path out(raw);
        CoTaskMemFree(raw);
        return out;
    }
    return {};
}

std::string getenvString(const wchar_t* name) {
    DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0) return {};
    std::wstring buffer(needed, L'\0');
    DWORD written = GetEnvironmentVariableW(name, buffer.data(), needed);
    if (written == 0) return {};
    buffer.resize(written);
    return wideToUtf8(buffer);
}

std::string readFileIfExists(const fs::path& path) {
    if (!fs::exists(path)) return {};
    return readTextFile(path);
}

std::string pathUtf8(const fs::path& path) {
    return wideToUtf8(path.wstring());
}

std::string normalizedAutoSwitchMode(AutoSwitchMode mode) {
    switch (mode) {
    case AutoSwitchMode::Notify:
        return "notify";
    case AutoSwitchMode::WhenCodexClosed:
        return "when_codex_closed";
    case AutoSwitchMode::WhenIdle:
        return "when_idle";
    case AutoSwitchMode::Off:
    default:
        return "off";
    }
}

JsonValue::Object settingsJson(const AppSettings& settings) {
    return {
        {"auto_restart_codex", JsonValue(settings.autoRestartCodex)},
        {"auto_switch_cooldown_minutes", JsonValue(static_cast<double>(settings.autoSwitchCooldownMinutes))},
        {"auto_switch_idle_minutes", JsonValue(static_cast<double>(settings.autoSwitchIdleMinutes))},
        {"auto_switch_mode", JsonValue(normalizedAutoSwitchMode(settings.autoSwitchMode))},
        {"check_updates_on_startup", JsonValue(settings.checkUpdatesOnStartup)},
        {"codex_launch_path", JsonValue(settings.codexLaunchPath)},
        {"five_hour_alert_threshold", JsonValue(static_cast<double>(settings.fiveHourAlertThreshold))},
        {"poll_interval_minutes", JsonValue(static_cast<double>(settings.pollIntervalMinutes))},
        {"show_restart_reminder", JsonValue(settings.showRestartReminder)},
        {"start_at_login", JsonValue(settings.startAtLogin)},
        {"switch_away_threshold", JsonValue(static_cast<double>(settings.switchAwayThreshold))},
        {"switch_to_threshold", JsonValue(static_cast<double>(settings.switchToThreshold))},
        {"weekly_alert_threshold", JsonValue(static_cast<double>(settings.weeklyAlertThreshold))},
    };
}

AppSettings settingsFromJson(const JsonValue& value) {
    AppSettings settings;
    const JsonValue* root = &value;
    settings.pollIntervalMinutes = jsonIntValue(root, "poll_interval_minutes", 5);
    if (settings.pollIntervalMinutes != 1 && settings.pollIntervalMinutes != 5 && settings.pollIntervalMinutes != 10) {
        settings.pollIntervalMinutes = 5;
    }
    settings.fiveHourAlertThreshold = std::max(0, jsonIntValue(root, "five_hour_alert_threshold", 10));
    settings.weeklyAlertThreshold = std::max(0, jsonIntValue(root, "weekly_alert_threshold", 30));
    settings.autoRestartCodex = jsonBoolValue(root, "auto_restart_codex", false);
    settings.showRestartReminder = jsonBoolValue(root, "show_restart_reminder", true);
    settings.checkUpdatesOnStartup = jsonBoolValue(root, "check_updates_on_startup", true);
    settings.startAtLogin = jsonBoolValue(root, "start_at_login", false);
    settings.autoSwitchMode = autoSwitchModeFromString(jsonStringValue(root, "auto_switch_mode", "off"));
    settings.autoSwitchIdleMinutes = std::max(1, jsonIntValue(root, "auto_switch_idle_minutes", 5));
    settings.autoSwitchCooldownMinutes = std::max(1, jsonIntValue(root, "auto_switch_cooldown_minutes", 15));
    settings.switchAwayThreshold = std::max(1, jsonIntValue(root, "switch_away_threshold", 5));
    settings.switchToThreshold = jsonIntValue(root, "switch_to_threshold", 30);
    if (settings.switchToThreshold <= settings.switchAwayThreshold) settings.switchToThreshold = 30;
    settings.codexLaunchPath = trim(jsonStringValue(root, "codex_launch_path"));
    return settings;
}

struct HttpResponse {
    int status = 0;
    std::string body;
};

HttpResponse httpsGet(
    const std::wstring& host,
    const std::wstring& path,
    const std::vector<std::pair<std::wstring, std::wstring>>& headers
) {
    HINTERNET session = WinHttpOpen(
        L"codex-quota-dock-native-windows/0.6",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!session) throw std::runtime_error("WinHttpOpen failed");
    WinHttpSetTimeouts(session, 20000, 20000, 20000, 20000);

    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        throw std::runtime_error("WinHttpConnect failed");
    }

    HINTERNET request = WinHttpOpenRequest(
        connect,
        L"GET",
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        throw std::runtime_error("WinHttpOpenRequest failed");
    }

    for (const auto& [name, value] : headers) {
        std::wstring header = name + L": " + value;
        WinHttpAddRequestHeaders(request, header.c_str(), static_cast<DWORD>(header.size()), WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        throw std::runtime_error("quota/update HTTP request failed");
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );

    std::string body;
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) break;
        chunk.resize(read);
        body += chunk;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return HttpResponse{static_cast<int>(status), std::move(body)};
}

std::vector<int> versionParts(std::string_view version) {
    std::vector<int> out;
    std::regex part("(\\d+)");
    std::string text(version);
    auto begin = std::sregex_iterator(text.begin(), text.end(), part);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end && out.size() < 3; ++it) {
        out.push_back(std::stoi((*it)[1].str()));
    }
    while (out.size() < 3) out.push_back(0);
    return out;
}

bool containsInsensitive(std::wstring value, std::wstring needle) {
    std::transform(value.begin(), value.end(), value.begin(), ::towlower);
    std::transform(needle.begin(), needle.end(), needle.begin(), ::towlower);
    return value.find(needle) != std::wstring::npos;
}

std::wstring processImagePath(DWORD processId) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return {};
    wchar_t path[32768]{};
    DWORD size = static_cast<DWORD>(sizeof(path) / sizeof(path[0]));
    std::wstring out;
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        out.assign(path, size);
    }
    CloseHandle(process);
    return out;
}

bool isWindowsAppsCodexPath(const std::wstring& path) {
    return containsInsensitive(path, L"\\WindowsApps\\OpenAI.Codex_");
}

std::optional<fs::path> runningCodexExecutablePath() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return std::nullopt;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::optional<fs::path> result;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (!isCodexProcessName(entry.szExeFile)) continue;
            std::wstring image = processImagePath(entry.th32ProcessID);
            if (!image.empty()) {
                result = fs::path(image);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

} // namespace

std::wstring utf8ToWide(std::string_view text) {
    if (text.empty()) return {};
    int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring out(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), count);
    return out;
}

std::string wideToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string out(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), count, nullptr, nullptr);
    return out;
}

std::string trim(std::string value) {
    auto isSpace = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char c) { return !isSpace(static_cast<unsigned char>(c)); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char c) { return !isSpace(static_cast<unsigned char>(c)); }).base(), value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string nowIsoUtc() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_s(&tm, &t);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::string timestampForFile() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_s(&tm, &t);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return out.str();
}

std::string readTextFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("read file failed: " + pathUtf8(path));
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

void writeTextFileAtomic(const fs::path& path, std::string_view data) {
    fs::create_directories(path.parent_path());
    fs::path tmp = path;
    tmp += L".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("write file failed: " + pathUtf8(tmp));
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    std::error_code ignored;
    fs::remove(path, ignored);
    fs::rename(tmp, path);
}

fs::path configRoot() {
    fs::path roaming = knownFolder(FOLDERID_RoamingAppData);
    if (roaming.empty()) {
        roaming = fs::path(utf8ToWide(getenvString(L"APPDATA")));
    }
    if (roaming.empty()) {
        roaming = fs::current_path();
    }
    return roaming / L"codex-quota-dock";
}

fs::path defaultCodexRoot() {
    std::string codexHome = getenvString(L"CODEX_HOME");
    if (!codexHome.empty()) return fs::path(utf8ToWide(codexHome));
    fs::path profile = knownFolder(FOLDERID_Profile);
    if (profile.empty()) {
        profile = fs::path(utf8ToWide(getenvString(L"USERPROFILE")));
    }
    return profile / L".codex";
}

fs::path defaultCodexAuthPath() {
    return defaultCodexRoot() / L"auth.json";
}

AuthMetadata parseAuthMetadata(std::string_view authJson, bool requireAccessToken) {
    JsonValue root = JsonValue::parse(authJson);
    if (!root.isObject()) throw std::runtime_error("auth JSON must be an object");
    const JsonValue* tokens = root.get("tokens");
    std::string accessToken = jsonStringValue(tokens, "access_token");
    if (accessToken.empty()) accessToken = jsonStringValue(&root, "access_token");
    if (requireAccessToken && accessToken.empty()) {
        throw std::runtime_error("auth tokens.access_token is required");
    }
    std::string accountId = jsonStringValue(tokens, "account_id");
    if (accountId.empty()) accountId = jsonStringValue(&root, "OPENAI_ACCOUNT_ID");
    if (accountId.empty()) accountId = jsonStringValue(&root, "account_id");
    AuthMetadata metadata;
    metadata.authMode = jsonStringValue(&root, "auth_mode", "chatgpt");
    metadata.accountId = accountId;
    metadata.accountSuffix = accountSuffix(accountId);
    metadata.accessToken = accessToken;
    metadata.lastRefresh = jsonStringValue(&root, "last_refresh");
    return metadata;
}

QuotaSnapshot parseQuotaPayload(std::string_view json) {
    JsonValue root = JsonValue::parse(json);
    const JsonValue* rate = root.get("rate_limit");
    if (!rate) throw std::runtime_error("usage payload missing rate_limit");

    auto mapWindow = [](const JsonValue* value, std::string label) {
        QuotaWindow window;
        window.label = std::move(label);
        if (!value || !value->isObject()) return window;
        const JsonValue* used = value->get("used_percent");
        if (used && used->isNumber()) {
            int remaining = static_cast<int>(std::lround(100.0 - used->asNumber()));
            window.remainingPercent = std::clamp(remaining, 0, 100);
        }
        window.resetsAt = jsonInt64Value(value, "reset_at", 0);
        return window;
    };

    QuotaSnapshot snapshot;
    snapshot.planType = jsonStringValue(&root, "plan_type");
    snapshot.fiveHour = mapWindow(rate->get("primary_window"), "5h");
    snapshot.weekly = mapWindow(rate->get("secondary_window"), "weekly");
    return snapshot;
}

int QuotaSnapshot::effectiveRemainingPercent() const {
    int best = 101;
    if (fiveHour.remainingPercent) best = std::min(best, *fiveHour.remainingPercent);
    if (weekly.remainingPercent) best = std::min(best, *weekly.remainingPercent);
    return best == 101 ? -1 : best;
}

bool QuotaSnapshot::belowThreshold(int fiveHourThreshold, int weeklyThreshold) const {
    return (fiveHourThreshold > 0 && fiveHour.remainingPercent && *fiveHour.remainingPercent <= fiveHourThreshold) ||
        (weeklyThreshold > 0 && weekly.remainingPercent && *weekly.remainingPercent <= weeklyThreshold);
}

AppSettings loadSettings(const fs::path& configDir) {
    fs::path path = configDir / L"settings.json";
    if (!fs::exists(path)) return AppSettings{};
    return settingsFromJson(JsonValue::parse(readTextFile(path)));
}

void saveSettings(const fs::path& configDir, const AppSettings& settings) {
    writeTextFileAtomic(configDir / L"settings.json", JsonValue(settingsJson(settings)).stringify(2));
}

std::string autoSwitchModeToString(AutoSwitchMode mode) {
    return normalizedAutoSwitchMode(mode);
}

AutoSwitchMode autoSwitchModeFromString(std::string_view value) {
    std::string normalized = lower(trim(std::string(value)));
    if (normalized == "notify") return AutoSwitchMode::Notify;
    if (normalized == "when_codex_closed") return AutoSwitchMode::WhenCodexClosed;
    if (normalized == "when_idle") return AutoSwitchMode::WhenIdle;
    return AutoSwitchMode::Off;
}

AutoSwitchDecision decideAutoSwitch(
    AutoSwitchMode mode,
    const AutoSwitchCandidate& current,
    const std::vector<AutoSwitchCandidate>& candidates,
    const AutoSwitchContext& context,
    int switchAwayThreshold,
    int switchToThreshold,
    int cooldownMinutes,
    int requiredIdleMinutes
) {
    if (mode == AutoSwitchMode::Off || current.profileId.empty()) return {};
    if (context.lastSwitchUnix > 0 && context.nowUnix > context.lastSwitchUnix) {
        int64_t elapsed = context.nowUnix - context.lastSwitchUnix;
        if (elapsed < static_cast<int64_t>(std::max(1, cooldownMinutes)) * 60) return {};
    }

    auto isLow = [&](const AutoSwitchCandidate& candidate) {
        if (switchAwayThreshold <= 0) return false;
        return (candidate.fiveHourRemainingPercent && *candidate.fiveHourRemainingPercent <= switchAwayThreshold) ||
            (candidate.weeklyRemainingPercent && *candidate.weeklyRemainingPercent <= switchAwayThreshold);
    };
    auto isHealthy = [&](const AutoSwitchCandidate& candidate) {
        return candidate.autoSwitchAllowed &&
            candidate.fiveHourRemainingPercent &&
            candidate.weeklyRemainingPercent &&
            *candidate.fiveHourRemainingPercent >= switchToThreshold &&
            *candidate.weeklyRemainingPercent >= switchToThreshold;
    };

    std::vector<AutoSwitchCandidate> healthy;
    for (const auto& candidate : candidates) {
        if (candidate.profileId == current.profileId) continue;
        if (isHealthy(candidate)) healthy.push_back(candidate);
    }
    std::sort(healthy.begin(), healthy.end(), [](const AutoSwitchCandidate& left, const AutoSwitchCandidate& right) {
        if (left.priority != right.priority) return left.priority > right.priority;
        return lower(left.alias) < lower(right.alias);
    });

    AutoSwitchReason reason = AutoSwitchReason::CurrentQuotaLow;
    const AutoSwitchCandidate* target = nullptr;
    if (isLow(current)) {
        if (!healthy.empty()) target = &healthy.front();
    } else {
        reason = AutoSwitchReason::PreferredProfileRecovered;
        auto found = std::find_if(healthy.begin(), healthy.end(), [&](const AutoSwitchCandidate& candidate) {
            return candidate.priority > current.priority;
        });
        if (found != healthy.end()) target = &*found;
    }
    if (!target) return {};

    AutoSwitchDecision decision;
    decision.targetProfileId = target->profileId;
    decision.reason = reason;
    switch (mode) {
    case AutoSwitchMode::Notify:
        decision.action = AutoSwitchAction::Notify;
        return decision;
    case AutoSwitchMode::WhenCodexClosed:
        decision.action = context.codexRunning ? AutoSwitchAction::PendingUntilIdle : AutoSwitchAction::SwitchNow;
        return decision;
    case AutoSwitchMode::WhenIdle:
        decision.action = (!context.codexRunning || context.idleMinutes >= std::max(1, requiredIdleMinutes))
            ? AutoSwitchAction::SwitchNow
            : AutoSwitchAction::PendingUntilIdle;
        return decision;
    case AutoSwitchMode::Off:
    default:
        return {};
    }
}

ProfileStore::ProfileStore(fs::path root) : root_(std::move(root)) {}

fs::path ProfileStore::profilesFile() const { return root_ / L"profiles.json"; }
fs::path ProfileStore::profilesDir() const { return root_ / L"profiles"; }
fs::path ProfileStore::backupsDir() const { return root_ / L"backups"; }
fs::path ProfileStore::authPath(const std::string& profileId) const { return profilesDir() / utf8ToWide(profileId) / L"auth.json"; }

void ProfileStore::load() {
    fs::create_directories(profilesDir());
    fs::create_directories(backupsDir());
    profiles_.clear();
    if (!fs::exists(profilesFile())) return;
    JsonValue root = JsonValue::parse(readTextFile(profilesFile()));
    const JsonValue* profiles = root.get("profiles");
    if (!profiles || !profiles->isArray()) return;
    for (const auto& value : profiles->asArray()) {
        profiles_.push_back(profileFromJson(value));
    }
}

void ProfileStore::save() const {
    JsonValue::Array profiles;
    for (const auto& profile : profiles_) {
        profiles.push_back(profileToJson(profile));
    }
    JsonValue::Object root{{"profiles", JsonValue(std::move(profiles))}};
    writeTextFileAtomic(profilesFile(), JsonValue(std::move(root)).stringify(2));
}

std::string ProfileStore::readAuth(const std::string& profileId) const {
    return readTextFile(authPath(profileId));
}

Profile ProfileStore::importAuth(std::string alias, std::string authJson, bool updateExistingAccount) {
    alias = trim(std::move(alias));
    if (alias.empty()) throw std::runtime_error("profile alias is required");
    AuthMetadata metadata = parseAuthMetadata(authJson);
    if (updateExistingAccount && !metadata.accountId.empty()) {
        for (auto& existing : profiles_) {
            if (existing.accountId == metadata.accountId) {
                return updateProfile(existing.id, alias, std::move(authJson));
            }
        }
    }
    if (aliasExists(alias)) throw std::runtime_error("profile alias already exists");
    Profile profile;
    profile.id = randomHexId();
    profile.alias = std::move(alias);
    profile.accountId = metadata.accountId;
    profile.accountSuffix = metadata.accountSuffix;
    profile.authMode = metadata.authMode;
    profile.lastRefresh = metadata.lastRefresh;
    profile.createdAt = nowIsoUtc();
    writeTextFileAtomic(authPath(profile.id), authJson);
    profiles_.push_back(profile);
    save();
    return profile;
}

Profile ProfileStore::updateProfile(const std::string& profileId, std::string alias, std::string authJson) {
    alias = trim(std::move(alias));
    if (alias.empty()) throw std::runtime_error("profile alias is required");
    if (aliasExists(alias, profileId)) throw std::runtime_error("profile alias already exists");
    AuthMetadata metadata = parseAuthMetadata(authJson);
    for (auto& profile : profiles_) {
        if (profile.id == profileId) {
            profile.alias = alias;
            profile.accountId = metadata.accountId;
            profile.accountSuffix = metadata.accountSuffix;
            profile.authMode = metadata.authMode;
            profile.lastRefresh = metadata.lastRefresh;
            writeTextFileAtomic(authPath(profile.id), authJson);
            save();
            return profile;
        }
    }
    throw std::runtime_error("profile not found");
}

Profile ProfileStore::setPinned(const std::string& profileId, bool pinned) {
    for (auto& profile : profiles_) {
        if (profile.id == profileId) {
            profile.pinned = pinned;
            save();
            return profile;
        }
    }
    throw std::runtime_error("profile not found");
}

void ProfileStore::deleteProfile(const std::string& profileId) {
    auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const Profile& profile) { return profile.id == profileId; });
    if (it == profiles_.end()) throw std::runtime_error("profile not found");
    std::error_code ignored;
    fs::remove_all(profilesDir() / utf8ToWide(profileId), ignored);
    profiles_.erase(it);
    save();
}

std::string ProfileStore::suggestAlias(std::string prefix, std::string_view authJson) const {
    prefix = trim(std::move(prefix));
    if (prefix.empty()) prefix = "profile";
    AuthMetadata metadata = parseAuthMetadata(authJson);
    std::string base = metadata.accountSuffix.empty() ? prefix : prefix + "-" + metadata.accountSuffix;
    std::string candidate = base;
    for (int i = 2; aliasExists(candidate); ++i) {
        candidate = base + "-" + std::to_string(i);
    }
    return candidate;
}

std::optional<Profile> ProfileStore::findById(const std::string& profileId) const {
    auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const Profile& profile) { return profile.id == profileId; });
    if (it == profiles_.end()) return std::nullopt;
    return *it;
}

std::optional<Profile> ProfileStore::findByAlias(const std::string& alias) const {
    std::string want = lower(alias);
    auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const Profile& profile) { return lower(profile.alias) == want; });
    if (it == profiles_.end()) return std::nullopt;
    return *it;
}

std::optional<Profile> ProfileStore::findByAccountId(const std::string& accountId) const {
    auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const Profile& profile) { return profile.accountId == accountId; });
    if (it == profiles_.end()) return std::nullopt;
    return *it;
}

bool ProfileStore::aliasExists(const std::string& alias, const std::string& exceptId) const {
    std::string want = lower(alias);
    return std::any_of(profiles_.begin(), profiles_.end(), [&](const Profile& profile) {
        return profile.id != exceptId && lower(profile.alias) == want;
    });
}

Profile ProfileStore::profileFromJson(const JsonValue& value) {
    Profile profile;
    profile.id = jsonStringValue(&value, "id");
    profile.alias = jsonStringValue(&value, "alias");
    profile.accountId = jsonStringValue(&value, "account_id");
    profile.accountSuffix = jsonStringValue(&value, "account_suffix");
    profile.authMode = jsonStringValue(&value, "auth_mode");
    profile.pinned = jsonBoolValue(&value, "pinned", false);
    profile.priority = jsonIntValue(&value, "priority", 0);
    profile.autoSwitchAllowed = jsonBoolValue(&value, "auto_switch_allowed", true);
    profile.lastRefresh = jsonStringValue(&value, "last_refresh");
    profile.createdAt = jsonStringValue(&value, "created_at");
    return profile;
}

JsonValue ProfileStore::profileToJson(const Profile& profile) {
    JsonValue::Object object{
        {"account_id", JsonValue(profile.accountId)},
        {"account_suffix", JsonValue(profile.accountSuffix)},
        {"alias", JsonValue(profile.alias)},
        {"auth_mode", JsonValue(profile.authMode)},
        {"created_at", JsonValue(profile.createdAt)},
        {"id", JsonValue(profile.id)},
        {"last_refresh", JsonValue(profile.lastRefresh)},
    };
    if (profile.pinned) object["pinned"] = JsonValue(true);
    if (profile.priority != 0) object["priority"] = JsonValue(static_cast<double>(profile.priority));
    if (!profile.autoSwitchAllowed) object["auto_switch_allowed"] = JsonValue(false);
    return JsonValue(std::move(object));
}

SwitchResult switchAuth(const fs::path& activeAuthPath, const fs::path& profileAuthPath, const fs::path& backupDir) {
    fs::create_directories(activeAuthPath.parent_path());
    fs::create_directories(backupDir);
    SwitchResult result{activeAuthPath, backupDir / utf8ToWide("auth-" + timestampForFile() + ".json")};
    if (fs::exists(activeAuthPath)) {
        fs::copy_file(activeAuthPath, result.backupPath, fs::copy_options::overwrite_existing);
    }
    std::string replacement = readTextFile(profileAuthPath);
    writeTextFileAtomic(activeAuthPath, replacement);
    return result;
}

std::string exportBackup(ProfileStore& store, const AppSettings& settings) {
    JsonValue::Array profiles;
    for (const auto& profile : store.profiles()) {
        JsonValue::Object item{
            {"account_id", JsonValue(profile.accountId)},
            {"account_suffix", JsonValue(profile.accountSuffix)},
            {"alias", JsonValue(profile.alias)},
            {"auth_json", JsonValue(JsonValue::parse(store.readAuth(profile.id)))},
            {"auth_mode", JsonValue(profile.authMode)},
        };
        if (profile.pinned) item["pinned"] = JsonValue(true);
        profiles.push_back(JsonValue(std::move(item)));
    }
    JsonValue::Object root{
        {"exported_at", JsonValue(nowIsoUtc())},
        {"profiles", JsonValue(std::move(profiles))},
        {"settings", JsonValue(settingsJson(settings))},
        {"version", JsonValue(1.0)},
    };
    return JsonValue(std::move(root)).stringify(2);
}

BackupImportSummary importBackup(ProfileStore& store, std::string_view backupJson) {
    JsonValue root = JsonValue::parse(backupJson);
    if (jsonIntValue(&root, "version", 0) != 1) throw std::runtime_error("unsupported backup version");
    const JsonValue* profiles = root.get("profiles");
    if (!profiles || !profiles->isArray()) throw std::runtime_error("backup missing profiles");
    BackupImportSummary summary;
    for (const auto& item : profiles->asArray()) {
        std::string alias = jsonStringValue(&item, "alias", "profile");
        const JsonValue* authJson = item.get("auth_json");
        if (!authJson) throw std::runtime_error("backup profile missing auth_json");
        std::string authText = authJson->stringify(2);
        AuthMetadata metadata = parseAuthMetadata(authText);
        if (auto existing = store.findByAccountId(metadata.accountId); existing) {
            Profile updated = store.updateProfile(existing->id, alias, authText);
            store.setPinned(updated.id, jsonBoolValue(&item, "pinned", false));
            summary.updated++;
        } else {
            Profile created = store.importAuth(alias, authText, false);
            if (jsonBoolValue(&item, "pinned", false)) store.setPinned(created.id, true);
            summary.created++;
        }
    }
    return summary;
}

std::optional<fs::path> latestBackup(const fs::path& backupDir) {
    if (!fs::exists(backupDir)) return std::nullopt;
    std::optional<fs::path> best;
    for (const auto& entry : fs::directory_iterator(backupDir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != L".json") continue;
        if (!best || entry.path().filename().wstring() > best->filename().wstring()) {
            best = entry.path();
        }
    }
    return best;
}

void restoreBackup(const fs::path& backupPath, const fs::path& activeAuthPath) {
    writeTextFileAtomic(activeAuthPath, readTextFile(backupPath));
}

QuotaSnapshot fetchQuota(std::string_view authJson) {
    AuthMetadata auth = parseAuthMetadata(authJson);
    std::vector<std::pair<std::wstring, std::wstring>> headers{
        {L"Authorization", utf8ToWide("Bearer " + auth.accessToken)},
        {L"User-Agent", L"codex-quota-dock-native-windows"},
    };
    if (!auth.accountId.empty()) {
        headers.push_back({L"ChatGPT-Account-Id", utf8ToWide(auth.accountId)});
    }
    HttpResponse response = httpsGet(L"chatgpt.com", L"/backend-api/wham/usage", headers);
    if (response.status == 401 || response.status == 403) throw std::runtime_error("auth expired or unauthorized");
    if (response.status == 429) throw std::runtime_error("rate limited while checking quota");
    if (response.status != 200) throw std::runtime_error("quota request failed: http " + std::to_string(response.status));
    return parseQuotaPayload(response.body);
}

UpdateCheckResult checkForUpdates(std::string_view currentVersion) {
    UpdateCheckResult result;
    result.current = currentVersion.empty() ? std::string(kVersion) : std::string(currentVersion);
    HttpResponse response = httpsGet(
        L"api.github.com",
        L"/repos/fearofmissingout/codex-quota-dock/releases/latest",
        {{L"Accept", L"application/vnd.github+json"}, {L"User-Agent", L"codex-quota-dock-native-windows"}}
    );
    if (response.status != 200) throw std::runtime_error("fetch latest release failed: http " + std::to_string(response.status));
    JsonValue release = JsonValue::parse(response.body);
    result.latest = jsonStringValue(&release, "tag_name");
    result.releaseUrl = jsonStringValue(&release, "html_url");
    if (!isNewerVersion(result.current, result.latest)) {
        result.reason = "already up to date";
        return result;
    }
    const JsonValue* assets = release.get("assets");
    if (assets && assets->isArray()) {
        for (const auto& item : assets->asArray()) {
            std::string name = jsonStringValue(&item, "name");
            std::string lowered = lower(name);
            if (lowered.find("native-windows-amd64") == std::string::npos &&
                lowered.find("windows-amd64") == std::string::npos) {
                continue;
            }
            result.asset.name = name;
            result.asset.browserDownloadUrl = jsonStringValue(&item, "browser_download_url");
            result.asset.size = jsonInt64Value(&item, "size", 0);
            result.available = true;
            return result;
        }
    }
    result.reason = "no matching Windows release asset";
    return result;
}

bool isNewerVersion(std::string_view current, std::string_view latest) {
    auto c = versionParts(current);
    auto l = versionParts(latest);
    for (size_t i = 0; i < c.size(); ++i) {
        if (l[i] != c[i]) return l[i] > c[i];
    }
    return lower(std::string(current)).find("dev") != std::string::npos &&
        lower(std::string(latest)).find("dev") == std::string::npos;
}

bool startupEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
    wchar_t value[MAX_PATH * 2]{};
    DWORD size = sizeof(value);
    DWORD type = 0;
    LONG rc = RegQueryValueExW(key, kStartupValueName, nullptr, &type, reinterpret_cast<LPBYTE>(value), &size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS && type == REG_SZ && wcslen(value) > 0;
}

void setStartupEnabled(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        throw std::runtime_error("open startup registry failed");
    }
    if (!enabled) {
        RegDeleteValueW(key, kStartupValueName);
        RegCloseKey(key);
        return;
    }
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring command = L"\"" + std::wstring(exe) + L"\"";
    RegSetValueExW(key, kStartupValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
}

bool isCodexProcessName(std::wstring_view name) {
    std::wstring value(name.begin(), name.end());
    std::transform(value.begin(), value.end(), value.begin(), ::towlower);
    return value == L"codex.exe" || value == L"codex";
}

CodexLaunchTarget detectCodexLaunchTarget() {
    constexpr const wchar_t* appUserModelId = L"OpenAI.Codex_2p2nqsd0c76g0!App";
    if (auto running = runningCodexExecutablePath(); running) {
        std::wstring image = running->wstring();
        if (isWindowsAppsCodexPath(image)) {
            return {CodexLaunchKind::AppUserModelId, appUserModelId};
        }
        if (fs::exists(*running)) {
            return {CodexLaunchKind::ExecutablePath, image};
        }
    }

    std::vector<fs::path> executableCandidates{
        fs::path(utf8ToWide(getenvString(L"LOCALAPPDATA"))) / L"Programs" / L"Codex" / L"Codex.exe",
        fs::path(utf8ToWide(getenvString(L"LOCALAPPDATA"))) / L"Programs" / L"codex" / L"Codex.exe",
        fs::path(utf8ToWide(getenvString(L"LOCALAPPDATA"))) / L"OpenAI" / L"Codex" / L"Codex.exe",
        fs::path(utf8ToWide(getenvString(L"PROGRAMFILES"))) / L"Codex" / L"Codex.exe",
    };
    for (const auto& candidate : executableCandidates) {
        if (!candidate.empty() && fs::exists(candidate)) {
            return {CodexLaunchKind::ExecutablePath, candidate.wstring()};
        }
    }
    return {CodexLaunchKind::AppUserModelId, appUserModelId};
}

CodexLaunchTarget codexLaunchTarget(const AppSettings& settings) {
    std::string configured = trim(settings.codexLaunchPath);
    if (configured.empty()) return detectCodexLaunchTarget();
    std::string lowered = lower(configured);
    if (lowered.rfind("codex:", 0) == 0) {
        return {CodexLaunchKind::Protocol, utf8ToWide(configured)};
    }
    if (configured.find('!') != std::string::npos && configured.find('\\') == std::string::npos && configured.find('/') == std::string::npos) {
        return {CodexLaunchKind::AppUserModelId, utf8ToWide(configured)};
    }
    return {CodexLaunchKind::ExecutablePath, utf8ToWide(configured)};
}

std::string restartCodex() {
    return restartCodex(AppSettings{});
}

std::string restartCodex(const AppSettings& settings) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return "Could not inspect Codex processes.";
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    int stopped = 0;
    DWORD self = GetCurrentProcessId();
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == self) continue;
            if (!isCodexProcessName(entry.szExeFile)) continue;
            HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
            if (process) {
                if (TerminateProcess(process, 0)) stopped++;
                CloseHandle(process);
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    CodexLaunchTarget target = codexLaunchTarget(settings);
    HINSTANCE launched = nullptr;
    switch (target.kind) {
    case CodexLaunchKind::ExecutablePath:
        launched = ShellExecuteW(nullptr, L"open", target.value.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case CodexLaunchKind::Protocol:
        launched = ShellExecuteW(nullptr, L"open", target.value.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case CodexLaunchKind::AppUserModelId:
    default: {
        std::wstring appFolder = L"shell:AppsFolder\\" + target.value;
        launched = ShellExecuteW(nullptr, L"open", appFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    }
    }
    if (reinterpret_cast<intptr_t>(launched) <= 32) {
        return "Switched auth. Please restart Codex manually.";
    }
    return "Stopped " + std::to_string(stopped) + " Codex process(es) and requested restart.";
}

void UsageTotals::add(const UsageTotals& other) {
    input += other.input;
    cachedInput += other.cachedInput;
    output += other.output;
    reasoningOutput += other.reasoningOutput;
    total += other.total;
}

LocalUsageSummary scanLocalUsage(const fs::path& codexRoot) {
    LocalUsageSummary summary;
    std::map<std::string, UsageTotals> byDay;
    std::vector<fs::path> roots{codexRoot / L"sessions", codexRoot / L"archived_sessions"};
    auto now = std::chrono::system_clock::now();
    for (const auto& root : roots) {
        if (!fs::exists(root)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file() || entry.path().extension() != L".jsonl") continue;
            summary.sessionCount++;
            std::ifstream in(entry.path());
            std::string line;
            while (std::getline(in, line)) {
                try {
                    JsonValue event = JsonValue::parse(line);
                    auto at = parseIsoTimestamp(jsonStringValue(&event, "timestamp"));
                    if (!at) {
                        summary.parseErrors++;
                        continue;
                    }
                    std::string payloadType;
                    const JsonValue* payload = event.get("payload");
                    if (payload) payloadType = jsonStringValue(payload, "type");
                    if (payloadType != "token_count") continue;
                    const JsonValue* info = payload ? payload->get("info") : nullptr;
                    const JsonValue* usage = info ? info->get("last_token_usage") : nullptr;
                    if (!usage) continue;
                    UsageTotals item;
                    item.input = jsonInt64Value(usage, "input_tokens", 0);
                    item.cachedInput = jsonInt64Value(usage, "cached_input_tokens", 0);
                    item.output = jsonInt64Value(usage, "output_tokens", 0);
                    item.reasoningOutput = jsonInt64Value(usage, "reasoning_output_tokens", 0);
                    item.total = jsonInt64Value(usage, "total_tokens", item.input + item.output);
                    summary.total.add(item);
                    if (sameLocalDay(now, *at)) summary.today.add(item);
                    if (*at >= now - std::chrono::hours(24 * 7)) summary.last7Days.add(item);
                    if (*at >= now - std::chrono::hours(24 * 30)) summary.last30Days.add(item);
                    byDay[localDayKey(*at)].add(item);
                } catch (...) {
                    summary.parseErrors++;
                }
            }
        }
    }
    for (const auto& [day, usage] : byDay) {
        summary.byDay.push_back({day, usage});
    }
    return summary;
}

std::vector<HealthRow> runHealthCheck(ProfileStore& store, const fs::path& activeAuthPath, std::string_view version) {
    std::vector<HealthRow> rows;
    try {
        parseAuthMetadata(readFileIfExists(activeAuthPath), false);
        rows.push_back({"ok", "Active auth", pathUtf8(activeAuthPath)});
    } catch (...) {
        rows.push_back({"warning", "Active auth", "missing or not parseable: " + pathUtf8(activeAuthPath)});
    }
    rows.push_back({"ok", "Profiles", std::to_string(store.profiles().size()) + " saved profile(s)"});
    rows.push_back({startupEnabled() ? "ok" : "info", "Startup", startupEnabled() ? "enabled" : "disabled"});
    rows.push_back({"ok", "Version", version.empty() ? std::string(kVersion) : std::string(version)});
    return rows;
}

} // namespace cqd
