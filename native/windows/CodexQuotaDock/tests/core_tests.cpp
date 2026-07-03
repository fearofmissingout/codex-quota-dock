#include "core.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <regex>
#include <stdexcept>

#include <winsqlite/winsqlite3.h>

namespace fs = std::filesystem;

namespace {

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

fs::path tempRoot(const wchar_t* name) {
    fs::path root = fs::temp_directory_path() / name;
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root);
    return root;
}

std::string authJson(std::string accessToken = "fixture-access-token", std::string accountId = "acct_1234567890") {
    return std::string(R"({
  "auth_mode": "chatgpt",
  "tokens": {
    "access_token": ")") + accessToken + R"(",
    "refresh_token": "fixture-refresh-token",
    "account_id": ")" + accountId + R"("
  },
  "last_refresh": "2026-06-29T00:00:00Z"
})";
}

void execSql(sqlite3* db, const char* sql) {
    char* error = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        std::string message = error ? error : "sqlite exec failed";
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

void testJsonRoundTrip() {
    cqd::JsonValue value = cqd::JsonValue::parse(R"({"name":"codex","items":[1,true,null],"escaped":"a\nb"})");
    expect(value.get("name")->asString() == "codex", "json string field");
    expect(value.get("items")->asArray().size() == 3, "json array field");
    std::string text = value.stringify(2);
    expect(text.find("\"escaped\"") != std::string::npos, "json stringify keeps keys");
}

void testAuthMetadata() {
    cqd::AuthMetadata metadata = cqd::parseAuthMetadata(authJson());
    expect(metadata.authMode == "chatgpt", "auth mode");
    expect(metadata.accessToken == "fixture-access-token", "auth access token");
    expect(metadata.accountId == "acct_1234567890", "auth account id");
    expect(metadata.accountSuffix == "567890", "auth suffix");
}

void testProfileStoreImportUpdateDelete() {
    fs::path root = tempRoot(L"cqd-native-profile-test");
    cqd::ProfileStore store(root);
    store.load();

    cqd::Profile created = store.importAuth("team", authJson(), false);
    expect(created.alias == "team", "profile alias");
    expect(created.accountSuffix == "567890", "profile suffix");
    expect(fs::exists(store.authPath(created.id)), "profile auth exists");

    cqd::Profile updated = store.updateProfile(created.id, "team-pro", authJson("updated-token", "acct_9999999999"));
    expect(updated.alias == "team-pro", "updated alias");
    expect(updated.accountSuffix == "999999", "updated suffix");
    expect(store.readAuth(created.id).find("updated-token") != std::string::npos, "updated auth persisted");

    cqd::Profile pinned = store.setPinned(created.id, true);
    expect(pinned.pinned, "pin persisted");

    cqd::Profile automated = store.updateAutomation(created.id, 10, false);
    expect(automated.priority == 10, "automation priority persisted");
    expect(!automated.autoSwitchAllowed, "automation allow flag persisted");

    cqd::ProfileStore reloaded(root);
    reloaded.load();
    expect(reloaded.profiles().size() == 1, "profile metadata reload");
    expect(reloaded.profiles()[0].pinned, "profile pin reload");
    expect(reloaded.profiles()[0].priority == 10, "profile priority reload");
    expect(!reloaded.profiles()[0].autoSwitchAllowed, "profile auto switch flag reload");

    reloaded.deleteProfile(created.id);
    expect(reloaded.profiles().empty(), "profile delete metadata");
    expect(!fs::exists(store.authPath(created.id)), "profile delete auth");
}

void testQuotaParser() {
    std::string payload = R"({
  "plan_type": "pro",
  "rate_limit": {
    "primary_window": {"used_percent": 97, "limit_window_seconds": 18000, "reset_at": 1782734400},
    "secondary_window": {"used_percent": 25, "limit_window_seconds": 604800, "reset_at": 1783296000}
  }
})";
    cqd::QuotaSnapshot snapshot = cqd::parseQuotaPayload(payload);
    expect(snapshot.fiveHour.remainingPercent && *snapshot.fiveHour.remainingPercent == 3, "5h remaining");
    expect(snapshot.weekly.remainingPercent && *snapshot.weekly.remainingPercent == 75, "weekly remaining");
    expect(snapshot.belowThreshold(10, 30), "threshold check");
}

void testFormatsQuotaResetWithMonthDayAndTime() {
    std::string reset = cqd::formatQuotaResetTime(1'782'909'240);
    expect(std::regex_match(reset, std::regex(R"(\d{2}/\d{2} \d{2}:\d{2})")), "reset time includes month day and time");
    expect(cqd::formatQuotaResetTime(0).empty(), "empty reset time for missing epoch");
}

void testBackupAndSwitch() {
    fs::path root = tempRoot(L"cqd-native-backup-test");
    cqd::ProfileStore store(root / L"config");
    store.load();
    cqd::Profile profile = store.importAuth("team", authJson(), false);
    cqd::AppSettings settings;
    std::string backup = cqd::exportBackup(store, settings);
    expect(backup.find("\"auth_json\"") != std::string::npos, "backup contains auth json");

    cqd::ProfileStore imported(root / L"imported");
    imported.load();
    cqd::BackupImportSummary summary = cqd::importBackup(imported, backup);
    expect(summary.created == 1, "backup import created");

    fs::path active = root / L".codex" / L"auth.json";
    cqd::writeTextFileAtomic(active, authJson("old-token", "acct_old"));
    cqd::SwitchResult result = cqd::switchAuth(active, store.authPath(profile.id), store.backupsDir());
    expect(fs::exists(result.backupPath), "switch backup exists");
    expect(cqd::readTextFile(active).find("fixture-access-token") != std::string::npos, "active auth replaced");
}

void testSettingsAndVersion() {
    fs::path root = tempRoot(L"cqd-native-settings-test");
    cqd::AppSettings settings;
    settings.pollIntervalMinutes = 10;
    settings.fiveHourAlertThreshold = 15;
    settings.autoSwitchMode = cqd::AutoSwitchMode::WhenIdle;
    settings.switchAwayThreshold = 4;
    settings.switchToThreshold = 35;
    settings.autoSwitchIdleMinutes = 7;
    settings.autoSwitchCooldownMinutes = 19;
    settings.autoRestartCodex = true;
    settings.quotaPriorityMode = true;
    settings.quotaPriorityFiveHourThreshold = 99;
    settings.quotaPriorityWeeklyThreshold = 0;
    settings.codexLaunchPath = "C:\\custom\\Codex.exe";
    cqd::saveSettings(root, settings);
    cqd::AppSettings loaded = cqd::loadSettings(root);
    expect(loaded.pollIntervalMinutes == 10, "settings poll interval");
    expect(loaded.fiveHourAlertThreshold == 15, "settings 5h threshold");
    expect(loaded.autoSwitchMode == cqd::AutoSwitchMode::WhenIdle, "settings auto switch mode");
    expect(loaded.switchAwayThreshold == 4, "settings switch away threshold");
    expect(loaded.switchToThreshold == 35, "settings switch to threshold");
    expect(loaded.autoSwitchIdleMinutes == 7, "settings idle minutes");
    expect(loaded.autoSwitchCooldownMinutes == 19, "settings cooldown minutes");
    expect(loaded.autoRestartCodex, "settings auto restart codex");
    expect(loaded.quotaPriorityMode, "settings quota priority mode");
    expect(loaded.quotaPriorityFiveHourThreshold == 99, "settings quota priority 5h");
    expect(loaded.quotaPriorityWeeklyThreshold == 0, "settings quota priority weekly");
    expect(loaded.codexLaunchPath == "C:\\custom\\Codex.exe", "settings codex launch path");
    expect(cqd::isNewerVersion("0.5.0", "v0.6.0"), "version newer");
    expect(!cqd::isNewerVersion("0.6.0", "v0.6.0"), "version equal");
}

void testUpdateReleaseParsingAndChecksums() {
    std::string release = R"({
      "tag_name": "v0.9.0",
      "html_url": "https://github.com/fearofmissingout/codex-quota-dock/releases/tag/v0.9.0",
      "assets": [
        {
          "name": "codex-quota-dock-native-windows-amd64.zip",
          "browser_download_url": "https://example.com/windows.zip",
          "size": 100
        },
        {
          "name": "codex-quota-dock-native-windows-amd64.exe",
          "browser_download_url": "https://example.com/windows.exe",
          "size": 42
        },
        {
          "name": "SHA256SUMS.txt",
          "browser_download_url": "https://example.com/SHA256SUMS.txt",
          "size": 256
        }
      ]
    })";
    cqd::UpdateCheckResult result = cqd::parseWindowsUpdateRelease(release, "0.8.0");
    expect(result.available, "new Windows update is installable");
    expect(result.asset.name == "codex-quota-dock-native-windows-amd64.exe", "Windows auto update prefers exe asset");
    expect(result.checksumAsset.name == "SHA256SUMS.txt", "checksum asset detected");

    std::string checksum = cqd::checksumForAsset(R"SUM(
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa  codex-quota-dock-native-windows-amd64.zip
bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb *codex-quota-dock-native-windows-amd64.exe
)SUM", "codex-quota-dock-native-windows-amd64.exe");
    expect(checksum == std::string(64, 'b'), "checksum parser handles starred filename");

    fs::path root = tempRoot(L"cqd-native-update-test");
    fs::path file = root / L"payload.txt";
    cqd::writeTextFileAtomic(file, "abc");
    expect(cqd::sha256HexFile(file) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256 file hash");
}

void testCodexProcessSelectionAndLaunchTarget() {
    expect(cqd::isCodexProcessName(L"Codex.exe"), "Codex.exe is a Codex process");
    expect(cqd::isCodexProcessName(L"codex.exe"), "codex.exe is a Codex process");
    expect(!cqd::isCodexProcessName(L"codex-quota-dock-native.exe"), "quota dock must not kill itself");
    expect(!cqd::isCodexProcessName(L"node_repl.exe"), "node repl is not Codex app");

    cqd::AppSettings settings;
    settings.codexLaunchPath = "C:\\custom\\Codex.exe";
    cqd::CodexLaunchTarget target = cqd::codexLaunchTarget(settings);
    expect(target.kind == cqd::CodexLaunchKind::ExecutablePath, "manual executable path wins");
    expect(target.value == L"C:\\custom\\Codex.exe", "manual executable path preserved");

    settings.codexLaunchPath = "OpenAI.Codex_2p2nqsd0c76g0!App";
    target = cqd::codexLaunchTarget(settings);
    expect(target.kind == cqd::CodexLaunchKind::AppUserModelId, "manual app id is supported");
}

void testLocalUsageEffectiveTotals() {
    cqd::UsageTotals totals;
    totals.input = 100;
    totals.cachedInput = 80;
    totals.output = 25;
    totals.reasoningOutput = 7;
    totals.total = 125;
    expect(totals.uncachedInput() == 20, "uncached input excludes cached input");
    expect(totals.effectiveTotal() == 45, "effective total excludes cached input without double-counting reasoning");

    fs::path root = tempRoot(L"cqd-native-local-usage-test");
    fs::path sessions = root / L"sessions" / L"2026" / L"07" / L"01";
    fs::create_directories(sessions);
    cqd::writeTextFileAtomic(sessions / L"fixture.jsonl", R"JSONL(
{"timestamp":"2026-07-01T08:00:00Z","payload":{"type":"token_count","info":{"last_token_usage":{"input_tokens":100,"cached_input_tokens":80,"output_tokens":25,"reasoning_output_tokens":7,"total_tokens":125}}}}
{"timestamp":"2026-07-01T08:01:00Z","payload":{"type":"token_count","info":{"last_token_usage":{"input_tokens":50,"cached_input_tokens":10,"output_tokens":5,"reasoning_output_tokens":1,"total_tokens":55}}}}
)JSONL");

    cqd::LocalUsageSummary summary = cqd::scanLocalUsage(root);
    expect(summary.total.total == 180, "raw local usage total keeps cached input");
    expect(summary.total.cachedInput == 90, "cached input remains available");
    expect(summary.total.effectiveTotal() == 90, "summary effective total excludes cached input");
    expect(summary.byDay.size() == 1, "daily usage has one day");
    expect(summary.byDay[0].usage.effectiveTotal() == 90, "daily effective total excludes cached input");
}

void testAutoSwitchPolicy() {
    cqd::AutoSwitchCandidate current{
        "current",
        "current",
        0,
        true,
        2,
        90,
        true,
    };
    cqd::AutoSwitchCandidate backup{
        "backup",
        "backup",
        5,
        true,
        80,
        80,
        false,
    };
    cqd::AutoSwitchDecision decision = cqd::decideAutoSwitch(
        cqd::AutoSwitchMode::WhenCodexClosed,
        current,
        {current, backup},
        cqd::AutoSwitchContext{false, 0, 0, 1000},
        5,
        30,
        15,
        5
    );
    expect(decision.action == cqd::AutoSwitchAction::SwitchNow, "closed Codex can switch now");
    expect(decision.targetProfileId == "backup", "switches to healthy target");

    decision = cqd::decideAutoSwitch(
        cqd::AutoSwitchMode::WhenIdle,
        current,
        {current, backup},
        cqd::AutoSwitchContext{true, 1, 0, 1000},
        5,
        30,
        15,
        5
    );
    expect(decision.action == cqd::AutoSwitchAction::PendingUntilIdle, "running Codex waits for idle");

    decision = cqd::decideAutoSwitch(
        cqd::AutoSwitchMode::WhenIdle,
        current,
        {current, backup},
        cqd::AutoSwitchContext{true, 10, 995, 1000},
        5,
        30,
        15,
        5
    );
    expect(decision.action == cqd::AutoSwitchAction::None, "cooldown blocks switching");

    cqd::AutoSwitchCandidate pro{
        "pro",
        "pro",
        5,
        true,
        80,
        80,
        true,
    };
    cqd::AutoSwitchCandidate team{
        "team",
        "team",
        0,
        true,
        99,
        0,
        false,
    };
    decision = cqd::decideAutoSwitch(
        cqd::AutoSwitchMode::WhenIdle,
        pro,
        {pro, team},
        cqd::AutoSwitchContext{true, 3, 0, 1000},
        10,
        30,
        15,
        3,
        true,
        99,
        0
    );
    expect(decision.action == cqd::AutoSwitchAction::SwitchNow, "quota priority switches back when highest priority recovers");
    expect(decision.targetProfileId == "team", "quota priority selects P0 profile");
    expect(decision.reason == cqd::AutoSwitchReason::QuotaPriorityRecovered, "quota priority reason");

    cqd::AutoSwitchCandidate leo{
        "leo",
        "leo",
        5,
        true,
        99,
        99,
        false,
    };
    cqd::AutoSwitchCandidate teamCurrent{
        "team",
        "team",
        0,
        true,
        63,
        80,
        true,
    };
    decision = cqd::decideAutoSwitch(
        cqd::AutoSwitchMode::WhenIdle,
        teamCurrent,
        {teamCurrent, leo},
        cqd::AutoSwitchContext{true, 10, 0, 1000},
        10,
        30,
        15,
        3,
        true,
        99,
        0
    );
    expect(decision.action == cqd::AutoSwitchAction::None, "quota priority does not leave P0 for lower priority");
}

void testCodexLogObserverAggregatesAndThrottle() {
    fs::path root = tempRoot(L"cqd-native-log-observer-test");
    fs::path dbPath = root / L"logs_2.sqlite";
    sqlite3* db = nullptr;
    expect(sqlite3_open16(dbPath.c_str(), &db) == SQLITE_OK, "open temp sqlite logs db");
    execSql(db, R"SQL(
        CREATE TABLE logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ts INTEGER NOT NULL,
            ts_nanos INTEGER NOT NULL,
            level TEXT NOT NULL,
            target TEXT NOT NULL,
            feedback_log_body TEXT,
            module_path TEXT,
            file TEXT,
            line INTEGER,
            thread_id TEXT,
            process_uuid TEXT,
            estimated_bytes INTEGER NOT NULL DEFAULT 0
        );
        INSERT INTO logs (ts, ts_nanos, level, target, module_path, thread_id, process_uuid, estimated_bytes)
        VALUES
          (1000, 0, 'INFO', 'codex_api::endpoint::responses_websocket', 'codex_api::endpoint::responses_websocket', 't1', 'p1', 100),
          (990, 0, 'DEBUG', 'codex_core::stream_events_utils', 'codex_core::stream_events_utils', 't1', 'p1', 120),
          (980, 0, 'WARN', 'codex_core_plugins::manifest', 'codex_core_plugins::manifest', 't1', 'p1', 130),
          (970, 0, 'ERROR', 'codex_core::responses_retry', 'codex_core::responses_retry', 't2', 'p2', 140),
          (900, 0, 'TRACE', 'codex_api::sse::responses', 'codex_api::sse::responses', 't2', 'p2', 150),
          (800, 0, 'INFO', 'other', 'other', 't3', 'p2', 90);
        CREATE TRIGGER codex_ignore_trace_logs
        BEFORE INSERT ON logs
        WHEN NEW.level = 'TRACE'
        BEGIN
          SELECT RAISE(IGNORE);
        END;
    )SQL");
    sqlite3_close(db);

    cqd::CodexLogActivitySummary summary = cqd::scanCodexLogActivity(root, 1000);
    expect(summary.databaseExists, "log database exists");
    expect(summary.traceInsertBlocked, "trace trigger detected");
    expect(summary.codexResponding, "recent response logs mark Codex as responding");
    expect(summary.recentWarnCount == 1, "recent warn count");
    expect(summary.recentErrorCount == 1, "recent error count");
    expect(summary.recentTraceCount == 1, "recent trace count");
    expect(summary.threadCount == 3, "thread count");
    expect(summary.processCount == 2, "process count");
    expect(summary.todayActiveMinutes >= 3, "active minutes bucketed by minute");
    expect(cqd::shouldReuseCodexLogCache(100, 130, 60), "reuse cache inside minimum interval");
    expect(!cqd::shouldReuseCodexLogCache(100, 161, 60), "refresh cache after minimum interval");
}

} // namespace

int main() {
    try {
        testJsonRoundTrip();
        testAuthMetadata();
        testProfileStoreImportUpdateDelete();
        testQuotaParser();
        testFormatsQuotaResetWithMonthDayAndTime();
        testBackupAndSwitch();
        testSettingsAndVersion();
        testUpdateReleaseParsingAndChecksums();
        testCodexProcessSelectionAndLaunchTarget();
        testLocalUsageEffectiveTotals();
        testAutoSwitchPolicy();
        testCodexLogObserverAggregatesAndThrottle();
        std::cout << "cqd_native_tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "cqd_native_tests failed: " << error.what() << "\n";
        return 1;
    }
}
