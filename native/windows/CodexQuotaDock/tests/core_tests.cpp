#include "core.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>

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

    cqd::ProfileStore reloaded(root);
    reloaded.load();
    expect(reloaded.profiles().size() == 1, "profile metadata reload");
    expect(reloaded.profiles()[0].pinned, "profile pin reload");

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
    cqd::saveSettings(root, settings);
    cqd::AppSettings loaded = cqd::loadSettings(root);
    expect(loaded.pollIntervalMinutes == 10, "settings poll interval");
    expect(loaded.fiveHourAlertThreshold == 15, "settings 5h threshold");
    expect(cqd::isNewerVersion("0.5.0", "v0.6.0"), "version newer");
    expect(!cqd::isNewerVersion("0.6.0", "v0.6.0"), "version equal");
}

} // namespace

int main() {
    try {
        testJsonRoundTrip();
        testAuthMetadata();
        testProfileStoreImportUpdateDelete();
        testQuotaParser();
        testBackupAndSwitch();
        testSettingsAndVersion();
        std::cout << "cqd_native_tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "cqd_native_tests failed: " << error.what() << "\n";
        return 1;
    }
}
