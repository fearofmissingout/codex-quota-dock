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
#include <bcrypt.h>
#include <winhttp.h>
#include <winsqlite/winsqlite3.h>

namespace cqd {
namespace {

constexpr const char* kVersion = "0.9.1";
constexpr const char* kCodexClientId = "app_EMoamEEZ73f0CkXaXp7hrann";
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

std::optional<std::chrono::system_clock::time_point> timePointFromSQLiteTimestamp(int64_t value) {
    if (value <= 0) return std::nullopt;
    if (value > 10'000'000'000LL) {
        return std::chrono::system_clock::time_point(std::chrono::milliseconds(value));
    }
    return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(value));
}

void addUsageAt(
    LocalUsageSummary& summary,
    std::map<std::string, UsageTotals>& byDay,
    const UsageTotals& usage,
    std::chrono::system_clock::time_point at,
    std::chrono::system_clock::time_point now
) {
    summary.total.add(usage);
    if (sameLocalDay(now, at)) summary.today.add(usage);
    if (at >= now - std::chrono::hours(24 * 7)) summary.last7Days.add(usage);
    if (at >= now - std::chrono::hours(24 * 30)) summary.last30Days.add(usage);
    byDay[localDayKey(at)].add(usage);
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
        {"quota_priority_five_hour_threshold", JsonValue(static_cast<double>(settings.quotaPriorityFiveHourThreshold))},
        {"quota_priority_mode", JsonValue(settings.quotaPriorityMode)},
        {"quota_priority_weekly_threshold", JsonValue(static_cast<double>(settings.quotaPriorityWeeklyThreshold))},
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
    settings.quotaPriorityMode = jsonBoolValue(root, "quota_priority_mode", false);
    settings.quotaPriorityFiveHourThreshold = std::clamp(jsonIntValue(root, "quota_priority_five_hour_threshold", 99), 0, 100);
    settings.quotaPriorityWeeklyThreshold = std::clamp(jsonIntValue(root, "quota_priority_weekly_threshold", 0), 0, 100);
    settings.codexLaunchPath = trim(jsonStringValue(root, "codex_launch_path"));
    return settings;
}

struct HttpResponse {
    int status = 0;
    std::string body;
};

struct ParsedUrl {
    std::wstring host;
    std::wstring path;
};

ParsedUrl parseHttpsUrl(std::string_view url) {
    constexpr std::string_view prefix = "https://";
    if (url.substr(0, prefix.size()) != prefix) {
        throw std::runtime_error("only https update URLs are supported");
    }
    std::string_view rest = url.substr(prefix.size());
    size_t slash = rest.find('/');
    std::string_view host = slash == std::string_view::npos ? rest : rest.substr(0, slash);
    std::string_view path = slash == std::string_view::npos ? std::string_view("/") : rest.substr(slash);
    if (host.empty()) throw std::runtime_error("update URL is missing host");
    return {utf8ToWide(host), utf8ToWide(path)};
}

std::string unquoteEnvValue(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 2) {
        char first = value.front();
        char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

std::map<std::string, std::string> parseEnvFileText(std::string_view envText) {
    std::map<std::string, std::string> values;
    std::istringstream input{std::string(envText)};
    std::string line;
    while (std::getline(input, line)) {
        line = trim(std::move(line));
        if (line.empty() || line[0] == '#') continue;
        constexpr std::string_view exportPrefix = "export ";
        if (line.substr(0, exportPrefix.size()) == exportPrefix) {
            line = trim(line.substr(exportPrefix.size()));
        }
        size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        std::string key = lower(trim(line.substr(0, equals)));
        std::string value = unquoteEnvValue(line.substr(equals + 1));
        if (!key.empty()) values[key] = std::move(value);
    }
    return values;
}

std::string hostWithoutPort(std::string value) {
    value = lower(trim(std::move(value)));
    if (value.size() >= 2 && value.front() == '[') {
        size_t close = value.find(']');
        if (close != std::string::npos) return value.substr(1, close - 1);
    }
    size_t colon = value.rfind(':');
    if (colon != std::string::npos && value.find(':') == colon) {
        bool port = colon + 1 < value.size() &&
            std::all_of(value.begin() + static_cast<std::ptrdiff_t>(colon + 1), value.end(), [](unsigned char c) {
                return std::isdigit(c) != 0;
            });
        if (port) value.resize(colon);
    }
    return value;
}

std::string proxyHostPortForWinHttp(std::string proxy) {
    proxy = unquoteEnvValue(std::move(proxy));
    size_t scheme = proxy.find("://");
    if (scheme != std::string::npos) proxy.erase(0, scheme + 3);
    while (proxy.rfind("//", 0) == 0) proxy.erase(0, 2);
    size_t path = proxy.find('/');
    if (path != std::string::npos) proxy.resize(path);
    size_t at = proxy.rfind('@');
    if (at != std::string::npos) proxy.erase(0, at + 1);
    return trim(std::move(proxy));
}

std::vector<std::string> splitProxyList(std::string_view value) {
    std::vector<std::string> out;
    std::string current;
    for (char ch : value) {
        if (ch == ',' || ch == ';') {
            current = trim(std::move(current));
            if (!current.empty()) out.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    current = trim(std::move(current));
    if (!current.empty()) out.push_back(current);
    return out;
}

bool hostMatchesNoProxyToken(const std::string& host, std::string token) {
    token = hostWithoutPort(std::move(token));
    if (token.empty()) return false;
    if (token == "*") return true;
    if (token.rfind("*.", 0) == 0) token.erase(0, 1);
    if (token.front() == '.') {
        std::string suffix = token.substr(1);
        return host == suffix || (host.size() > suffix.size() && host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0 &&
            host[host.size() - suffix.size() - 1] == '.');
    }
    return host == token || (host.size() > token.size() && host.compare(host.size() - token.size(), token.size(), token) == 0 &&
        host[host.size() - token.size() - 1] == '.');
}

bool noProxyMatches(std::string_view noProxy, std::string_view host) {
    std::string normalizedHost = hostWithoutPort(std::string(host));
    if (normalizedHost.empty()) return false;
    for (std::string token : splitProxyList(noProxy)) {
        if (hostMatchesNoProxyToken(normalizedHost, std::move(token))) return true;
    }
    return false;
}

std::string winHttpBypassList(std::string_view noProxy) {
    std::vector<std::string> entries;
    for (std::string token : splitProxyList(noProxy)) {
        token = trim(std::move(token));
        if (token.empty() || token == "*") continue;
        if (token.rfind("*.", 0) == 0) {
            entries.push_back(token);
        } else if (token.front() == '.') {
            entries.push_back("*" + token);
        } else {
            token = hostWithoutPort(std::move(token));
            entries.push_back(token.find(':') == std::string::npos ? token : "[" + token + "]");
        }
    }
    std::ostringstream out;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) out << ';';
        out << entries[i];
    }
    return out.str();
}

NetworkProxySettings loadCodexProxySettings(const std::wstring& host, bool https) {
    fs::path envPath = defaultCodexRoot() / L".env";
    std::string envText = readFileIfExists(envPath);
    if (envText.empty()) return {};
    return proxySettingsFromEnv(envText, wideToUtf8(host), https);
}

std::string winHttpError(std::string_view operation) {
    return std::string(operation) + " failed: " + std::to_string(GetLastError());
}

HINTERNET openHttpSession(const std::wstring& host, bool https) {
    NetworkProxySettings proxy = loadCodexProxySettings(host, https);
    std::wstring proxyName = utf8ToWide(proxy.proxy);
    std::wstring bypass = utf8ToWide(proxy.bypass);
    return WinHttpOpen(
        L"codex-quota-dock-native-windows/0.9.1",
        proxy.enabled ? WINHTTP_ACCESS_TYPE_NAMED_PROXY : WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        proxy.enabled ? proxyName.c_str() : WINHTTP_NO_PROXY_NAME,
        proxy.enabled && !bypass.empty() ? bypass.c_str() : WINHTTP_NO_PROXY_BYPASS,
        0
    );
}

HttpResponse httpsGet(
    const std::wstring& host,
    const std::wstring& path,
    const std::vector<std::pair<std::wstring, std::wstring>>& headers
) {
    HINTERNET session = openHttpSession(host, true);
    if (!session) throw std::runtime_error(winHttpError("WinHttpOpen"));
    WinHttpSetTimeouts(session, 20000, 20000, 20000, 20000);

    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        throw std::runtime_error(winHttpError("WinHttpConnect"));
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
        throw std::runtime_error(winHttpError("WinHttpOpenRequest"));
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
        throw std::runtime_error(winHttpError("quota/update HTTP request"));
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

HttpResponse httpsPost(
    const std::wstring& host,
    const std::wstring& path,
    const std::vector<std::pair<std::wstring, std::wstring>>& headers,
    std::string_view body
) {
    HINTERNET session = openHttpSession(host, true);
    if (!session) throw std::runtime_error(winHttpError("WinHttpOpen"));
    WinHttpSetTimeouts(session, 20000, 20000, 20000, 20000);

    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        throw std::runtime_error(winHttpError("WinHttpConnect"));
    }

    HINTERNET request = WinHttpOpenRequest(
        connect,
        L"POST",
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        throw std::runtime_error(winHttpError("WinHttpOpenRequest"));
    }

    for (const auto& [name, value] : headers) {
        std::wstring header = name + L": " + value;
        WinHttpAddRequestHeaders(request, header.c_str(), static_cast<DWORD>(header.size()), WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(
            request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()),
            static_cast<DWORD>(body.size()),
            0
        ) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        throw std::runtime_error(winHttpError("auth refresh HTTP request"));
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

    std::string responseBody;
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) break;
        chunk.resize(read);
        responseBody += chunk;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return HttpResponse{static_cast<int>(status), std::move(responseBody)};
}

std::string formEncode(std::string_view value) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');
    for (unsigned char ch : value) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out << static_cast<char>(ch);
        } else {
            out << '%' << std::setw(2) << static_cast<int>(ch);
        }
    }
    return out.str();
}

std::string authErrorCode(std::string_view body) {
    try {
        JsonValue root = JsonValue::parse(body);
        return jsonStringValue(root.get("error"), "code");
    } catch (...) {
        return {};
    }
}

bool sameFile(const fs::path& left, const fs::path& right) {
    std::error_code ignored;
    return fs::exists(left, ignored) && fs::exists(right, ignored) && fs::equivalent(left, right, ignored);
}

HttpResponse httpsGetUrl(
    std::string_view url,
    const std::vector<std::pair<std::wstring, std::wstring>>& headers
) {
    ParsedUrl parsed = parseHttpsUrl(url);
    return httpsGet(parsed.host, parsed.path, headers);
}

void downloadUrlToFile(std::string_view url, const fs::path& destination) {
    ParsedUrl parsed = parseHttpsUrl(url);
    fs::create_directories(destination.parent_path());
    fs::path temp = destination;
    temp += L".download";

    HINTERNET session = openHttpSession(parsed.host, true);
    if (!session) throw std::runtime_error(winHttpError("WinHttpOpen"));
    WinHttpSetTimeouts(session, 20000, 20000, 30000, 30000);

    HINTERNET connect = WinHttpConnect(session, parsed.host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        throw std::runtime_error(winHttpError("WinHttpConnect"));
    }

    HINTERNET request = WinHttpOpenRequest(
        connect,
        L"GET",
        parsed.path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        throw std::runtime_error(winHttpError("WinHttpOpenRequest"));
    }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));
    std::wstring userAgent = L"User-Agent: codex-quota-dock-native-windows";
    WinHttpAddRequestHeaders(request, userAgent.c_str(), static_cast<DWORD>(userAgent.size()), WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        throw std::runtime_error(winHttpError("update download request"));
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
    if (status < 200 || status >= 300) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        throw std::runtime_error("update download failed: http " + std::to_string(status));
    }

    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        throw std::runtime_error("cannot write update download");
    }

    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) break;
        out.write(chunk.data(), read);
    }
    out.close();

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    std::error_code ignored;
    fs::rename(temp, destination, ignored);
    if (ignored) {
        fs::remove(destination, ignored);
        fs::rename(temp, destination);
    }
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

bool isCodexProcessCandidateImpl(std::wstring_view name, std::wstring_view imagePath) {
    std::wstring value(name.begin(), name.end());
    std::transform(value.begin(), value.end(), value.begin(), ::towlower);
    if (value == L"codex.exe" || value == L"codex") return true;
    if (value == L"chatgpt.exe" || value == L"chatgpt") {
        return isWindowsAppsCodexPath(std::wstring(imagePath));
    }
    return false;
}

std::optional<fs::path> runningCodexExecutablePath() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return std::nullopt;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::optional<fs::path> result;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            std::wstring image = processImagePath(entry.th32ProcessID);
            if (isCodexProcessCandidateImpl(entry.szExeFile, image) && !image.empty()) {
                result = fs::path(image);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

class SqliteStatement {
public:
    SqliteStatement(sqlite3* db, const char* sql) : db_(db) {
        if (sqlite3_prepare_v2(db_, sql, -1, &statement_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(db_));
        }
    }

    ~SqliteStatement() {
        if (statement_) sqlite3_finalize(statement_);
    }

    SqliteStatement(const SqliteStatement&) = delete;
    SqliteStatement& operator=(const SqliteStatement&) = delete;

    sqlite3_stmt* get() const { return statement_; }

private:
    sqlite3* db_ = nullptr;
    sqlite3_stmt* statement_ = nullptr;
};

int64_t sqliteScalarInt(sqlite3* db, const char* sql, const std::vector<int64_t>& binds = {}) {
    SqliteStatement statement(db, sql);
    for (size_t i = 0; i < binds.size(); ++i) {
        sqlite3_bind_int64(statement.get(), static_cast<int>(i + 1), binds[i]);
    }
    int rc = sqlite3_step(statement.get());
    if (rc == SQLITE_ROW) return sqlite3_column_int64(statement.get(), 0);
    if (rc == SQLITE_DONE) return 0;
    throw std::runtime_error(sqlite3_errmsg(db));
}

bool sqliteTableExists(sqlite3* db, const char* table) {
    SqliteStatement statement(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?");
    sqlite3_bind_text(statement.get(), 1, table, -1, SQLITE_TRANSIENT);
    return sqlite3_step(statement.get()) == SQLITE_ROW && sqlite3_column_int64(statement.get(), 0) > 0;
}

std::vector<std::string> sqliteColumns(sqlite3* db, const char* table) {
    std::vector<std::string> columns;
    std::string sql = std::string("PRAGMA table_info(") + table + ")";
    SqliteStatement statement(db, sql.c_str());
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(statement.get(), 1);
        if (text) columns.push_back(lower(reinterpret_cast<const char*>(text)));
    }
    return columns;
}

bool hasColumn(const std::vector<std::string>& columns, const char* name) {
    return std::find(columns.begin(), columns.end(), name) != columns.end();
}

std::optional<std::string> sqliteThreadTimestampExpression(const std::vector<std::string>& columns) {
    if (hasColumn(columns, "updated_at_ms") && hasColumn(columns, "updated_at")) {
        return "CASE WHEN updated_at_ms IS NOT NULL AND updated_at_ms > 0 THEN updated_at_ms ELSE updated_at * 1000 END";
    }
    if (hasColumn(columns, "updated_at_ms")) return "updated_at_ms";
    if (hasColumn(columns, "updated_at")) return "updated_at";
    if (hasColumn(columns, "created_at_ms") && hasColumn(columns, "created_at")) {
        return "CASE WHEN created_at_ms IS NOT NULL AND created_at_ms > 0 THEN created_at_ms ELSE created_at * 1000 END";
    }
    if (hasColumn(columns, "created_at_ms")) return "created_at_ms";
    if (hasColumn(columns, "created_at")) return "created_at";
    return std::nullopt;
}

std::vector<fs::path> sqliteUsageDatabaseCandidates(const fs::path& codexRoot) {
    std::vector<fs::path> candidates;
    for (const auto& directory : {codexRoot, codexRoot / L"sqlite"}) {
        if (!fs::exists(directory) || !fs::is_directory(directory)) continue;
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (!entry.is_regular_file()) continue;
            std::wstring name = entry.path().filename().wstring();
            std::wstring lowered = name;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::towlower);
            if (lowered.rfind(L"state_", 0) == 0 && entry.path().extension() == L".sqlite") {
                candidates.push_back(entry.path());
            }
        }
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    return candidates;
}

void scanSQLiteUsageDatabase(
    const fs::path& dbPath,
    std::chrono::system_clock::time_point now,
    LocalUsageSummary& summary,
    std::map<std::string, UsageTotals>& byDay
) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(pathUtf8(dbPath).c_str(), &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr);
    if (rc != SQLITE_OK || !db) {
        if (db) sqlite3_close(db);
        return;
    }
    sqlite3_busy_timeout(db, 50);
    sqlite3_exec(db, "PRAGMA query_only=ON", nullptr, nullptr, nullptr);

    try {
        if (!sqliteTableExists(db, "threads")) {
            sqlite3_close(db);
            return;
        }
        std::vector<std::string> columns = sqliteColumns(db, "threads");
        if (!hasColumn(columns, "tokens_used")) {
            sqlite3_close(db);
            return;
        }
        auto timestampExpression = sqliteThreadTimestampExpression(columns);
        if (!timestampExpression) {
            sqlite3_close(db);
            return;
        }

        summary.sqliteDatabaseCount++;
        std::string sql = "SELECT " + *timestampExpression + ", tokens_used FROM threads WHERE tokens_used > 0";
        SqliteStatement statement(db, sql.c_str());
        while ((rc = sqlite3_step(statement.get())) == SQLITE_ROW) {
            int64_t timestamp = sqlite3_column_int64(statement.get(), 0);
            int64_t tokens = sqlite3_column_int64(statement.get(), 1);
            auto at = timePointFromSQLiteTimestamp(timestamp);
            if (!at || tokens <= 0) continue;
            UsageTotals usage;
            usage.total = tokens;
            summary.sqlite.add(usage);
            summary.sqliteThreadCount++;
            addUsageAt(summary, byDay, usage, *at, now);
        }
        if (rc != SQLITE_DONE) summary.parseErrors++;
    } catch (...) {
        summary.parseErrors++;
    }
    sqlite3_close(db);
}

void scanSQLiteUsageDatabases(
    const fs::path& codexRoot,
    std::chrono::system_clock::time_point now,
    LocalUsageSummary& summary,
    std::map<std::string, UsageTotals>& byDay
) {
    for (const auto& dbPath : sqliteUsageDatabaseCandidates(codexRoot)) {
        scanSQLiteUsageDatabase(dbPath, now, summary, byDay);
    }
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

NetworkProxySettings proxySettingsFromEnv(std::string_view envText, std::string_view host, bool https) {
    std::map<std::string, std::string> env = parseEnvFileText(envText);
    std::string noProxy;
    if (auto it = env.find("no_proxy"); it != env.end()) noProxy = it->second;
    if (!noProxy.empty() && noProxyMatches(noProxy, host)) return {};

    const char* httpsKeys[] = {"https_proxy", "all_proxy", "http_proxy", "proxy"};
    const char* httpKeys[] = {"http_proxy", "all_proxy", "https_proxy", "proxy"};
    const char** keys = https ? httpsKeys : httpKeys;
    size_t keyCount = https ? std::size(httpsKeys) : std::size(httpKeys);

    std::string proxy;
    for (size_t i = 0; i < keyCount; ++i) {
        auto it = env.find(keys[i]);
        if (it != env.end() && !trim(it->second).empty()) {
            proxy = it->second;
            break;
        }
    }
    proxy = proxyHostPortForWinHttp(std::move(proxy));
    if (proxy.empty()) return {};
    return NetworkProxySettings{true, std::move(proxy), winHttpBypassList(noProxy)};
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

std::string formatQuotaResetTime(int64_t epochSeconds) {
    if (epochSeconds <= 0) return {};
    std::time_t value = static_cast<std::time_t>(epochSeconds);
    std::tm local{};
    if (localtime_s(&local, &value) != 0) return {};
    std::ostringstream out;
    out << std::put_time(&local, "%m/%d %H:%M");
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
    metadata.idToken = jsonStringValue(tokens, "id_token");
    metadata.refreshToken = jsonStringValue(tokens, "refresh_token");
    metadata.lastRefresh = jsonStringValue(&root, "last_refresh");
    return metadata;
}

QuotaSnapshot parseQuotaPayload(std::string_view json) {
    JsonValue root = JsonValue::parse(json);

    auto mapWindow = [](const JsonValue* value, std::string label) {
        QuotaWindow window;
        window.label = std::move(label);
        if (!value || !value->isObject()) return window;
        const JsonValue* used = value->get("used_percent");
        if (!used) used = value->get("usedPercent");
        if (used && used->isNumber()) {
            int remaining = static_cast<int>(std::lround(100.0 - used->asNumber()));
            window.remainingPercent = std::clamp(remaining, 0, 100);
        }
        window.resetsAt = jsonInt64Value(value, "reset_at", 0);
        if (!window.resetsAt) window.resetsAt = jsonInt64Value(value, "resetsAt", 0);
        return window;
    };

    QuotaSnapshot snapshot;
    snapshot.planType = jsonStringValue(&root, "plan_type");
    if (snapshot.planType.empty()) snapshot.planType = jsonStringValue(&root, "planType");
    snapshot.fiveHour.label = "5h";
    snapshot.weekly.label = "weekly";

    auto classifyWindow = [](const JsonValue* value, std::string_view fallback) -> std::string {
        if (!value || !value->isObject()) return {};
        int64_t seconds = jsonInt64Value(value, "limit_window_seconds", 0);
        int64_t minutes = jsonInt64Value(value, "windowDurationMins", 0);
        if (seconds >= 17'000 && seconds <= 19'000) return "5h";
        if (minutes >= 290 && minutes <= 310) return "5h";
        if (seconds >= 600'000 && seconds <= 610'000) return "weekly";
        if (minutes >= 10'000 && minutes <= 10'160) return "weekly";
        return std::string(fallback);
    };

    auto mergeWindow = [&](const JsonValue* value, std::string_view fallback) {
        std::string label = classifyWindow(value, fallback);
        if (label == "5h" && !snapshot.fiveHour.remainingPercent) {
            snapshot.fiveHour = mapWindow(value, "5h");
        } else if (label == "weekly" && !snapshot.weekly.remainingPercent) {
            snapshot.weekly = mapWindow(value, "weekly");
        }
    };

    auto mergeSnakeRateLimit = [&](const JsonValue* rate) {
        if (!rate || !rate->isObject()) return;
        mergeWindow(rate->get("primary_window"), "5h");
        mergeWindow(rate->get("secondary_window"), "weekly");
    };

    auto mergeCamelRateLimit = [&](const JsonValue* rate) {
        if (!rate || !rate->isObject()) return;
        mergeWindow(rate->get("primary"), "5h");
        mergeWindow(rate->get("secondary"), "weekly");
    };

    mergeSnakeRateLimit(root.get("rate_limit"));
    mergeCamelRateLimit(root.get("rateLimits"));
    if (const JsonValue* byLimit = root.get("rateLimitsByLimitId"); byLimit && byLimit->isObject()) {
        for (const auto& [_, value] : byLimit->asObject()) {
            mergeCamelRateLimit(&value);
        }
    }
    if (const JsonValue* additional = root.get("additional_rate_limits"); additional && additional->isArray()) {
        for (const auto& item : additional->asArray()) {
            mergeSnakeRateLimit(item.get("rate_limit"));
        }
    }
    if (!snapshot.fiveHour.remainingPercent && !snapshot.weekly.remainingPercent) {
        throw std::runtime_error("usage payload missing quota windows");
    }
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
    int requiredIdleMinutes,
    bool quotaPriorityMode,
    int quotaPriorityFiveHourThreshold,
    int quotaPriorityWeeklyThreshold
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
    auto isQuotaPriorityRecovered = [&](const AutoSwitchCandidate& candidate) {
        int fiveHourThreshold = std::clamp(quotaPriorityFiveHourThreshold, 0, 100);
        int weeklyThreshold = std::clamp(quotaPriorityWeeklyThreshold, 0, 100);
        return candidate.autoSwitchAllowed &&
            candidate.fiveHourRemainingPercent &&
            candidate.weeklyRemainingPercent &&
            *candidate.fiveHourRemainingPercent >= fiveHourThreshold &&
            *candidate.weeklyRemainingPercent >= weeklyThreshold;
    };
    auto isHigherPriority = [&](const AutoSwitchCandidate& candidate) {
        return candidate.priority < current.priority;
    };
    auto rank = [](const AutoSwitchCandidate& left, const AutoSwitchCandidate& right) {
        if (left.priority != right.priority) return left.priority < right.priority;
        return lower(left.alias) < lower(right.alias);
    };

    std::vector<AutoSwitchCandidate> healthy;
    for (const auto& candidate : candidates) {
        if (candidate.profileId == current.profileId) continue;
        if (isHealthy(candidate)) healthy.push_back(candidate);
    }
    std::sort(healthy.begin(), healthy.end(), rank);

    AutoSwitchReason reason = AutoSwitchReason::CurrentQuotaLow;
    AutoSwitchCandidate selectedRecovered;
    const AutoSwitchCandidate* target = nullptr;
    if (isLow(current)) {
        if (!healthy.empty()) target = &healthy.front();
    } else if (quotaPriorityMode) {
        reason = AutoSwitchReason::QuotaPriorityRecovered;
        std::vector<AutoSwitchCandidate> recovered;
        for (const auto& candidate : candidates) {
            if (candidate.profileId == current.profileId) continue;
            if (isQuotaPriorityRecovered(candidate)) recovered.push_back(candidate);
        }
        std::sort(recovered.begin(), recovered.end(), rank);
        auto found = std::find_if(recovered.begin(), recovered.end(), isHigherPriority);
        if (found != recovered.end()) {
            selectedRecovered = *found;
            target = &selectedRecovered;
        }
    } else {
        reason = AutoSwitchReason::PreferredProfileRecovered;
        auto found = std::find_if(healthy.begin(), healthy.end(), isHigherPriority);
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

Profile ProfileStore::updateAutomation(const std::string& profileId, int priority, bool autoSwitchAllowed) {
    priority = std::clamp(priority, 0, 100);
    for (auto& profile : profiles_) {
        if (profile.id == profileId) {
            profile.priority = priority;
            profile.autoSwitchAllowed = autoSwitchAllowed;
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

std::string refreshAuthJson(std::string_view authJson) {
    AuthMetadata auth = parseAuthMetadata(authJson);
    if (auth.refreshToken.empty()) throw std::runtime_error("auth refresh_token is required");

    std::string body =
        "grant_type=refresh_token&client_id=" + formEncode(kCodexClientId) +
        "&refresh_token=" + formEncode(auth.refreshToken);
    std::vector<std::pair<std::wstring, std::wstring>> headers{
        {L"Content-Type", L"application/x-www-form-urlencoded"},
        {L"Accept", L"application/json"},
        {L"User-Agent", L"codex-quota-dock-native-windows"},
    };
    HttpResponse response = httpsPost(L"auth.openai.com", L"/oauth/token", headers, body);
    if (response.status != 200) {
        std::string code = authErrorCode(response.body);
        std::string detail = code.empty() ? std::to_string(response.status) : code;
        throw std::runtime_error("auth refresh failed: " + detail);
    }

    JsonValue tokenResponse = JsonValue::parse(response.body);
    std::string accessToken = jsonStringValue(&tokenResponse, "access_token");
    std::string idToken = jsonStringValue(&tokenResponse, "id_token");
    std::string refreshToken = jsonStringValue(&tokenResponse, "refresh_token", auth.refreshToken);
    if (accessToken.empty()) throw std::runtime_error("auth refresh response missing access_token");
    if (refreshToken.empty()) throw std::runtime_error("auth refresh response missing refresh_token");

    JsonValue root = JsonValue::parse(authJson);
    if (!root.isObject()) throw std::runtime_error("auth JSON must be an object");
    JsonValue::Object& object = root.asObject();
    if (!root.get("tokens") || !root.get("tokens")->isObject()) {
        object["tokens"] = JsonValue(JsonValue::Object{});
    }
    JsonValue::Object& tokens = object["tokens"].asObject();
    tokens["access_token"] = JsonValue(accessToken);
    if (!idToken.empty()) tokens["id_token"] = JsonValue(idToken);
    tokens["refresh_token"] = JsonValue(refreshToken);
    if (!auth.accountId.empty()) tokens["account_id"] = JsonValue(auth.accountId);
    object["last_refresh"] = JsonValue(nowIsoUtc());
    return root.stringify(2);
}

QuotaSnapshot fetchQuota(std::string_view authJson) {
    AuthMetadata auth = parseAuthMetadata(authJson);
    std::vector<std::pair<std::wstring, std::wstring>> headers{
        {L"Authorization", utf8ToWide("Bearer " + auth.accessToken)},
        {L"User-Agent", L"codex-quota-dock-native-windows"},
        {L"Accept", L"application/json"},
        {L"OAI-Language", L"zh-CN"},
        {L"originator", L"codex-desktop"},
    };
    if (!auth.accountId.empty()) {
        headers.push_back({L"ChatGPT-Account-Id", utf8ToWide(auth.accountId)});
    }
    std::optional<std::runtime_error> parseError;
    for (const wchar_t* path : {
             L"/backend-api/wham/usage?supports_rewardless_invites=true",
             L"/backend-api/codex/usage",
             L"/backend-api/wham/usage",
         }) {
        HttpResponse response = httpsGet(L"chatgpt.com", path, headers);
        if (response.status == 401 || response.status == 403) {
            std::string code = authErrorCode(response.body);
            throw std::runtime_error(code.empty() ? "auth expired or unauthorized" : "auth " + code);
        }
        if (response.status == 429) throw std::runtime_error("rate limited while checking quota");
        if (response.status == 200) {
            try {
                return parseQuotaPayload(response.body);
            } catch (const std::runtime_error& error) {
                parseError = std::runtime_error(error.what());
                continue;
            }
        }
        if (path == std::wstring_view(L"/backend-api/wham/usage")) {
            throw std::runtime_error("quota request failed: http " + std::to_string(response.status));
        }
    }
    if (parseError) throw *parseError;
    throw std::runtime_error("quota request failed");
}

QuotaSnapshot fetchQuotaFromAuthFile(const fs::path& authPath, const fs::path& activeAuthPath) {
    std::string profileAuth = readTextFile(authPath);
    AuthMetadata profileMetadata = parseAuthMetadata(profileAuth);

    if (!activeAuthPath.empty() && !sameFile(authPath, activeAuthPath) && fs::exists(activeAuthPath)) {
        try {
            std::string activeAuth = readTextFile(activeAuthPath);
            AuthMetadata activeMetadata = parseAuthMetadata(activeAuth, false);
            if (!profileMetadata.accountId.empty() && profileMetadata.accountId == activeMetadata.accountId) {
                QuotaSnapshot quota = fetchQuota(activeAuth);
                writeTextFileAtomic(authPath, activeAuth);
                return quota;
            }
        } catch (...) {
            // Fall back to refreshing the saved profile auth below.
        }
    }

    try {
        return fetchQuota(profileAuth);
    } catch (const std::exception& firstError) {
        std::string message = firstError.what();
        if (message.find("auth ") == std::string::npos && message.find("unauthorized") == std::string::npos) {
            throw;
        }
        std::string refreshedAuth = refreshAuthJson(profileAuth);
        writeTextFileAtomic(authPath, refreshedAuth);
        if (!activeAuthPath.empty() && !sameFile(authPath, activeAuthPath) && fs::exists(activeAuthPath)) {
            try {
                AuthMetadata activeMetadata = parseAuthMetadata(readTextFile(activeAuthPath), false);
                AuthMetadata refreshedMetadata = parseAuthMetadata(refreshedAuth);
                if (!refreshedMetadata.accountId.empty() && refreshedMetadata.accountId == activeMetadata.accountId) {
                    writeTextFileAtomic(activeAuthPath, refreshedAuth);
                }
            } catch (...) {
            }
        }
        return fetchQuota(refreshedAuth);
    }
}

UpdateCheckResult parseWindowsUpdateRelease(std::string_view releaseJson, std::string_view currentVersion) {
    UpdateCheckResult result;
    result.current = currentVersion.empty() ? std::string(kVersion) : std::string(currentVersion);
    JsonValue release = JsonValue::parse(std::string(releaseJson));
    result.latest = jsonStringValue(&release, "tag_name");
    result.releaseUrl = jsonStringValue(&release, "html_url");
    if (!isNewerVersion(result.current, result.latest)) {
        result.reason = "already up to date";
        return result;
    }
    ReleaseAsset zipAsset;
    const JsonValue* assets = release.get("assets");
    if (assets && assets->isArray()) {
        for (const auto& item : assets->asArray()) {
            std::string name = jsonStringValue(&item, "name");
            std::string lowered = lower(name);
            ReleaseAsset parsed{
                name,
                jsonStringValue(&item, "browser_download_url"),
                jsonInt64Value(&item, "size", 0),
            };
            if (lowered == "sha256sums.txt" || lowered == "checksums.txt") {
                result.checksumAsset = parsed;
            } else if ((lowered.find("native-windows-amd64") != std::string::npos ||
                        lowered.find("windows-amd64") != std::string::npos) &&
                       lowered.ends_with(".exe")) {
                result.asset = parsed;
                result.available = true;
            } else if ((lowered.find("native-windows-amd64") != std::string::npos ||
                        lowered.find("windows-amd64") != std::string::npos) &&
                       lowered.ends_with(".zip")) {
                zipAsset = parsed;
            }
        }
    }
    if (result.available) return result;
    result.reason = zipAsset.name.empty()
        ? "no matching Windows release asset"
        : "Windows automatic update requires the .exe release asset";
    return result;
}

UpdateCheckResult checkForUpdates(std::string_view currentVersion) {
    HttpResponse response = httpsGet(
        L"api.github.com",
        L"/repos/fearofmissingout/codex-quota-dock/releases/latest",
        {{L"Accept", L"application/vnd.github+json"}, {L"User-Agent", L"codex-quota-dock-native-windows"}}
    );
    if (response.status != 200) throw std::runtime_error("fetch latest release failed: http " + std::to_string(response.status));
    return parseWindowsUpdateRelease(response.body, currentVersion);
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

std::string checksumForAsset(std::string_view checksumsText, std::string_view assetName) {
    std::istringstream stream{std::string(checksumsText)};
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line.starts_with("#")) continue;
        std::istringstream row(line);
        std::string hash;
        std::string name;
        row >> hash >> name;
        if (hash.size() != 64 || name.empty()) continue;
        if (!name.empty() && name.front() == '*') name.erase(name.begin());
        if (name == assetName) return lower(hash);
    }
    return {};
}

std::string sha256HexFile(const fs::path& path) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        throw std::runtime_error("open SHA256 provider failed");
    }

    DWORD hashLength = 0;
    DWORD resultSize = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength), &resultSize, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("read SHA256 hash length failed");
    }
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("create SHA256 hash failed");
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("open file for SHA256 failed");
    }

    std::vector<unsigned char> buffer(64 * 1024);
    while (in) {
        in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        std::streamsize count = in.gcount();
        if (count > 0 && BCryptHashData(hash, buffer.data(), static_cast<ULONG>(count), 0) < 0) {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            throw std::runtime_error("SHA256 hash update failed");
        }
    }

    std::vector<unsigned char> digest(hashLength);
    if (BCryptFinishHash(hash, digest.data(), hashLength, 0) < 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("finish SHA256 hash failed");
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned char byte : digest) out << std::setw(2) << static_cast<int>(byte);
    return out.str();
}

DownloadedUpdate downloadWindowsUpdate(const UpdateCheckResult& update, const fs::path& updatesDir) {
    if (!update.available || update.asset.browserDownloadUrl.empty()) {
        throw std::runtime_error("no installable update is available");
    }
    if (update.checksumAsset.browserDownloadUrl.empty()) {
        throw std::runtime_error("release checksum is missing; open GitHub Releases and install manually");
    }

    fs::create_directories(updatesDir);
    fs::path checksumsPath = updatesDir / L"SHA256SUMS.txt";
    downloadUrlToFile(update.checksumAsset.browserDownloadUrl, checksumsPath);
    std::string expected = checksumForAsset(readTextFile(checksumsPath), update.asset.name);
    if (expected.empty()) {
        throw std::runtime_error("release checksum does not include " + update.asset.name);
    }

    fs::path packagePath = updatesDir / utf8ToWide(update.asset.name);
    downloadUrlToFile(update.asset.browserDownloadUrl, packagePath);
    std::string actual = sha256HexFile(packagePath);
    if (lower(actual) != lower(expected)) {
        throw std::runtime_error("downloaded update checksum did not match");
    }
    return {packagePath, update.asset.name, expected, actual};
}

std::wstring quoteCommandArg(std::wstring_view value) {
    std::wstring out = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') out += L'\\';
        out += ch;
    }
    out += L"\"";
    return out;
}

fs::path currentExecutablePath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    if (length == 0) throw std::runtime_error("cannot resolve current executable path");
    return fs::path(std::wstring(buffer.data(), length));
}

void launchWindowsUpdateInstaller(const DownloadedUpdate& update) {
    fs::path target = currentExecutablePath();
    fs::path helper = update.path.parent_path() / (std::wstring(L"codex-quota-dock-updater-") + std::to_wstring(GetCurrentProcessId()) + L".exe");
    std::error_code ignored;
    fs::copy_file(target, helper, fs::copy_options::overwrite_existing, ignored);
    if (ignored) throw std::runtime_error("cannot prepare updater helper: " + ignored.message());

    std::wstring command =
        quoteCommandArg(helper.wstring()) + L" --apply-update " +
        quoteCommandArg(update.path.wstring()) + L" " +
        quoteCommandArg(target.wstring()) + L" " +
        std::to_wstring(GetCurrentProcessId());

    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    std::wstring mutableCommand = command;
    if (!CreateProcessW(helper.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        throw std::runtime_error("launch updater helper failed");
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}

int runWindowsUpdateInstaller(const std::vector<std::wstring>& args) {
    if (args.size() < 5 || args[1] != L"--apply-update") return 1;
    fs::path package = args[2];
    fs::path target = args[3];
    DWORD parentPid = static_cast<DWORD>(_wtoi(args[4].c_str()));

    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    if (parent) {
        WaitForSingleObject(parent, 30000);
        CloseHandle(parent);
    }

    std::error_code copyError;
    for (int attempt = 0; attempt < 40; ++attempt) {
        copyError.clear();
        fs::copy_file(package, target, fs::copy_options::overwrite_existing, copyError);
        if (!copyError) break;
        Sleep(250);
    }
    if (copyError) {
        MessageBoxW(nullptr, utf8ToWide("Update install failed: " + copyError.message()).c_str(), L"Codex Quota Dock", MB_OK | MB_ICONERROR);
        return 2;
    }

    ShellExecuteW(nullptr, L"open", target.wstring().c_str(), nullptr, target.parent_path().wstring().c_str(), SW_SHOWNORMAL);
    std::error_code ignored;
    fs::remove(package, ignored);
    fs::remove(currentExecutablePath(), ignored);
    return 0;
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

bool isCodexProcessCandidate(std::wstring_view name, std::wstring_view imagePath) {
    return isCodexProcessCandidateImpl(name, imagePath);
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
            std::wstring image = processImagePath(entry.th32ProcessID);
            if (!isCodexProcessCandidate(entry.szExeFile, image)) continue;
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

int64_t UsageTotals::uncachedInput() const {
    return std::max<int64_t>(0, input - cachedInput);
}

int64_t UsageTotals::effectiveTotal() const {
    return std::max<int64_t>(0, total - cachedInput);
}

LocalUsageSummary scanLocalUsage(const fs::path& codexRoot) {
    LocalUsageSummary summary;
    std::map<std::string, UsageTotals> byDay;
    auto now = std::chrono::system_clock::now();

    scanSQLiteUsageDatabases(codexRoot, now, summary, byDay);
    if (summary.sqliteThreadCount > 0) {
        for (const auto& [day, usage] : byDay) {
            summary.byDay.push_back({day, usage});
        }
        return summary;
    }

    std::vector<fs::path> roots{codexRoot / L"sessions", codexRoot / L"archived_sessions"};
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
                    addUsageAt(summary, byDay, item, *at, now);
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

CodexLogActivitySummary scanCodexLogActivity(const fs::path& codexRoot, int64_t nowUnix) {
    CodexLogActivitySummary summary;
    fs::path dbPath = codexRoot / L"logs_2.sqlite";
    summary.databaseExists = fs::exists(dbPath);
    fs::path walPath = dbPath;
    walPath += L"-wal";
    if (fs::exists(walPath)) {
        std::error_code ignored;
        summary.walBytes = static_cast<int64_t>(fs::file_size(walPath, ignored));
    }
    if (!summary.databaseExists) return summary;

    if (nowUnix <= 0) {
        nowUnix = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(pathUtf8(dbPath).c_str(), &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr);
    if (rc != SQLITE_OK || !db) {
        summary.error = db ? sqlite3_errmsg(db) : "open sqlite database failed";
        if (db) sqlite3_close(db);
        return summary;
    }

    try {
        sqlite3_busy_timeout(db, 50);
        sqlite3_exec(db, "PRAGMA query_only=ON", nullptr, nullptr, nullptr);
        if (!sqliteTableExists(db, "logs")) {
            summary.error = "logs table not found";
            sqlite3_close(db);
            return summary;
        }

        summary.traceInsertBlocked = sqliteScalarInt(
            db,
            "SELECT COUNT(*) FROM sqlite_master "
            "WHERE type='trigger' AND tbl_name='logs' "
            "AND sql LIKE '%NEW.level%' AND sql LIKE '%TRACE%' AND sql LIKE '%RAISE(IGNORE)%'"
        ) > 0;
        summary.lastLogUnix = sqliteScalarInt(db, "SELECT COALESCE(MAX(ts), 0) FROM logs");
        summary.lastTraceUnix = sqliteScalarInt(db, "SELECT COALESCE(MAX(ts), 0) FROM logs WHERE level='TRACE'");
        summary.todayActiveMinutes = static_cast<int>(sqliteScalarInt(
            db,
            "SELECT COUNT(*) FROM (SELECT DISTINCT (ts / 60) AS minute FROM logs WHERE ts >= ?)",
            {nowUnix - 24 * 60 * 60}
        ));
        summary.last7DaysActiveMinutes = static_cast<int>(sqliteScalarInt(
            db,
            "SELECT COUNT(*) FROM (SELECT DISTINCT (ts / 60) AS minute FROM logs WHERE ts >= ?)",
            {nowUnix - 7 * 24 * 60 * 60}
        ));
        summary.threadCount = static_cast<int>(sqliteScalarInt(
            db,
            "SELECT COUNT(DISTINCT thread_id) FROM logs WHERE thread_id IS NOT NULL AND ts >= ?",
            {nowUnix - 7 * 24 * 60 * 60}
        ));
        summary.processCount = static_cast<int>(sqliteScalarInt(
            db,
            "SELECT COUNT(DISTINCT process_uuid) FROM logs WHERE process_uuid IS NOT NULL AND ts >= ?",
            {nowUnix - 7 * 24 * 60 * 60}
        ));
        summary.codexResponding = sqliteScalarInt(
            db,
            "SELECT COUNT(*) FROM logs WHERE ts >= ? AND ("
            "target LIKE '%responses_websocket%' OR target LIKE '%sse::responses%' OR "
            "target LIKE '%stream_events_utils%' OR module_path LIKE '%responses_websocket%' OR "
            "module_path LIKE '%sse::responses%' OR module_path LIKE '%stream_events_utils%')",
            {nowUnix - 90}
        ) > 0;

        SqliteStatement levels(db, "SELECT level, COUNT(*) FROM logs WHERE ts >= ? GROUP BY level");
        sqlite3_bind_int64(levels.get(), 1, nowUnix - 15 * 60);
        while ((rc = sqlite3_step(levels.get())) == SQLITE_ROW) {
            const unsigned char* levelText = sqlite3_column_text(levels.get(), 0);
            std::string level = levelText ? reinterpret_cast<const char*>(levelText) : "";
            int count = static_cast<int>(sqlite3_column_int64(levels.get(), 1));
            if (level == "TRACE") summary.recentTraceCount = count;
            else if (level == "DEBUG") summary.recentDebugCount = count;
            else if (level == "INFO") summary.recentInfoCount = count;
            else if (level == "WARN") summary.recentWarnCount = count;
            else if (level == "ERROR") summary.recentErrorCount = count;
        }
        if (rc != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db));
    } catch (const std::exception& error) {
        summary.error = error.what();
    }

    sqlite3_close(db);
    return summary;
}

bool shouldReuseCodexLogCache(int64_t lastScanUnix, int64_t nowUnix, int minimumIntervalSeconds) {
    if (lastScanUnix <= 0 || nowUnix <= 0) return false;
    if (minimumIntervalSeconds <= 0) return false;
    return nowUnix - lastScanUnix < minimumIntervalSeconds;
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
