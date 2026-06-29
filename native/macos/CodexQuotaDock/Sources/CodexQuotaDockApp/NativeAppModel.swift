import AppKit
import CodexQuotaDockCore
import Combine
import Foundation
import UniformTypeIdentifiers

@MainActor
final class NativeAppModel: ObservableObject {
    let paths: AppPaths
    let store: ProfileStore
    let settingsStore: SettingsStore
    let switcher: AuthSwitcher
    let quotaClient: QuotaClient

    @Published var profiles: [Profile] = []
    @Published var monitorRows: [MonitorProfileState] = []
    @Published var selectedProfileID: String?
    @Published var aliasEditorText = ""
    @Published var authEditorText = ""
    @Published var statusMessage = "Ready"
    @Published var healthRows: [HealthRow] = []
    @Published var settings: AppSettings

    var openSettingsHandler: (() -> Void)?
    private var autoRefreshTask: Task<Void, Never>?

    init(paths: AppPaths) {
        self.paths = paths
        self.store = ProfileStore(configDirectory: paths.configDirectory)
        self.settingsStore = SettingsStore(url: paths.settingsFile)
        self.switcher = AuthSwitcher()
        self.quotaClient = QuotaClient()
        self.settings = settingsStore.load()
    }

    deinit {
        autoRefreshTask?.cancel()
    }

    var selectedProfile: Profile? {
        guard let selectedProfileID else { return nil }
        return profiles.first { $0.id == selectedProfileID }
    }

    func reload() {
        do {
            profiles = try store.load().profiles.sorted { $0.alias.localizedCaseInsensitiveCompare($1.alias) == .orderedAscending }
            if selectedProfileID == nil {
                selectedProfileID = profiles.first?.id
            }
            loadSelectedProfileEditor()
            refreshHealth()
            rebuildMonitorRows()
            statusMessage = profiles.isEmpty ? "No profiles yet" : "Loaded \(profiles.count) profile(s)"
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func selectProfile(_ id: String?) {
        selectedProfileID = id
        loadSelectedProfileEditor()
    }

    func loadSelectedProfileEditor() {
        guard let profile = selectedProfile else {
            aliasEditorText = ""
            authEditorText = ""
            return
        }
        aliasEditorText = profile.alias
        do {
            authEditorText = String(data: try store.authJSON(for: profile), encoding: .utf8) ?? ""
        } catch {
            authEditorText = ""
            statusMessage = error.localizedDescription
        }
    }

    func saveSelectedProfile() {
        guard let profile = selectedProfile else {
            statusMessage = "Select a profile first."
            return
        }
        guard let data = authEditorText.data(using: .utf8) else {
            statusMessage = "Auth JSON must be UTF-8 text."
            return
        }
        do {
            let updated = try store.updateProfile(id: profile.id, alias: aliasEditorText, authJSON: data)
            selectedProfileID = updated.id
            reload()
            statusMessage = "Saved \(updated.alias)."
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func createProfile() {
        guard let data = authEditorText.data(using: .utf8) else {
            statusMessage = "Auth JSON must be UTF-8 text."
            return
        }
        do {
            let created = try store.importAuth(alias: aliasEditorText.isEmpty ? "profile" : aliasEditorText, authJSON: data, updateExistingAccount: true)
            selectedProfileID = created.id
            reload()
            statusMessage = "Created \(created.alias)."
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func importCurrent() {
        do {
            let data = try Data(contentsOf: paths.defaultCodexAuth)
            let metadata = try AuthMetadata.parse(data)
            let alias = metadata.accountSuffix.isEmpty ? "current" : "current-\(metadata.accountSuffix)"
            let profile = try store.importAuth(alias: alias, authJSON: data, updateExistingAccount: true)
            selectedProfileID = profile.id
            reload()
            statusMessage = "Imported \(profile.alias)."
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func importFile() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.json]
        panel.allowsMultipleSelection = false
        panel.begin { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            Task { @MainActor in
                self?.importAuthFile(url)
            }
        }
    }

    private func importAuthFile(_ url: URL) {
        do {
            let data = try Data(contentsOf: url)
            let metadata = try AuthMetadata.parse(data)
            let basename = url.deletingPathExtension().lastPathComponent
            let alias = metadata.accountSuffix.isEmpty ? basename : "\(basename)-\(metadata.accountSuffix)"
            let profile = try store.importAuth(alias: alias, authJSON: data, updateExistingAccount: true)
            selectedProfileID = profile.id
            reload()
            statusMessage = "Imported \(profile.alias)."
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func deleteSelectedProfile() {
        guard let profile = selectedProfile else {
            statusMessage = "Select a profile first."
            return
        }
        do {
            try store.delete(profileID: profile.id)
            selectedProfileID = nil
            reload()
            statusMessage = "Deleted \(profile.alias)."
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func togglePinnedSelectedProfile() {
        guard let profile = selectedProfile else {
            statusMessage = "Select a profile first."
            return
        }
        do {
            let updated = try store.setPinned(profileID: profile.id, pinned: !profile.pinned)
            selectedProfileID = updated.id
            reload()
            statusMessage = updated.pinned ? "Pinned \(updated.alias)." : "Unpinned \(updated.alias)."
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func switchSelectedProfile() {
        guard let profile = selectedProfile else {
            statusMessage = "Select a profile first."
            return
        }
        do {
            let auth = try store.authJSON(for: profile)
            let result = try switcher.switchAuth(
                activeAuth: paths.defaultCodexAuth,
                targetAuthJSON: auth,
                backupsDirectory: store.backupsDirectory
            )
            let restart = settings.autoRestartCodex ? CodexProcessService().restartCodex().message : "Restart Codex to use the new auth."
            reload()
            showSwitchAlert(profile: profile, result: result, restart: restart)
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func openSettings() {
        openSettingsHandler?()
    }

    func refreshHealth() {
        healthRows = HealthCheck().run(paths: paths, store: store)
    }

    func refreshQuotas() async {
        let currentProfiles = profiles
        statusMessage = "Refreshing quota..."
        var quotas: [String: ProfileQuota] = [:]
        for profile in currentProfiles {
            guard shouldShowInMonitor(profile) else { continue }
            do {
                let auth = try store.authJSON(for: profile)
                quotas[profile.id] = try await quotaClient.fetch(authJSON: auth)
            } catch {
                quotas[profile.id] = ProfileQuota(
                    fiveHour: QuotaWindow(label: "5h", remainingPercent: nil, resetsAt: nil),
                    weekly: QuotaWindow(label: "weekly", remainingPercent: nil, resetsAt: nil)
                )
            }
        }
        rebuildMonitorRows(quotas: quotas)
        statusMessage = "Refreshed \(DateFormatter.localizedString(from: Date(), dateStyle: .none, timeStyle: .short))"
    }

    func saveSettings() {
        do {
            settings = settings.validated()
            try settingsStore.save(settings)
            startAutoRefresh()
            statusMessage = "Saved settings."
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func startAutoRefresh() {
        autoRefreshTask?.cancel()
        autoRefreshTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                await self.refreshQuotas()
                let minutes = await MainActor.run {
                    self.settings.validated().pollIntervalMinutes
                }
                let nanoseconds = UInt64(minutes) * 60 * 1_000_000_000
                try? await Task.sleep(nanoseconds: nanoseconds)
            }
        }
    }

    private func rebuildMonitorRows(quotas: [String: ProfileQuota] = [:]) {
        let activeAccount = activeAccountID()
        monitorRows = profiles
            .filter { shouldShowInMonitor($0, activeAccount: activeAccount) }
            .map { profile in
                let quota = quotas[profile.id] ?? ProfileQuota(
                    fiveHour: QuotaWindow(label: "5h", remainingPercent: nil, resetsAt: nil),
                    weekly: QuotaWindow(label: "weekly", remainingPercent: nil, resetsAt: nil)
                )
                return MonitorProfileState(
                    id: profile.id,
                    alias: profile.alias,
                    accountSuffix: profile.accountSuffix,
                    fiveHour: quota.fiveHour,
                    weekly: quota.weekly,
                    isActive: profile.accountID == activeAccount,
                    isPinned: profile.pinned
                )
            }
    }

    private func shouldShowInMonitor(_ profile: Profile, activeAccount: String? = nil) -> Bool {
        profile.pinned || (!profile.accountID.isEmpty && profile.accountID == (activeAccount ?? activeAccountID()))
    }

    private func activeAccountID() -> String? {
        guard let data = try? Data(contentsOf: paths.defaultCodexAuth),
              let metadata = try? AuthMetadata.parse(data, requireAccessToken: false),
              !metadata.accountID.isEmpty
        else {
            return nil
        }
        return metadata.accountID
    }

    private func showSwitchAlert(profile: Profile, result: AuthSwitchResult, restart: String) {
        let alert = NSAlert()
        alert.messageText = "Switched to \(profile.alias)"
        var lines = ["Active auth: \(result.activeAuthURL.path)", restart]
        if let backupURL = result.backupURL {
            lines.append("Backup: \(backupURL.path)")
        }
        alert.informativeText = lines.joined(separator: "\n")
        alert.runModal()
    }
}
