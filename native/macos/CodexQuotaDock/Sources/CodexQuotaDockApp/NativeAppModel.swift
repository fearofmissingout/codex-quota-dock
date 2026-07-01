import AppKit
import CodexQuotaDockCore
import Combine
import CoreGraphics
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
    @Published var quotaByProfileID: [String: ProfileQuota] = [:]
    @Published var localUsageSummary = LocalUsageSummary()
    @Published var usageLoading = false
    @Published var priorityEditorValue = 0
    @Published var autoSwitchAllowedEditorValue = true
    @Published var updateChecking = false
    @Published var updateResult: UpdateCheckResult?
    @Published var updateStatusMessage = "Ready to check for updates."

    var openSettingsHandler: (() -> Void)?
    var settingsChangedHandler: ((AppSettings) -> Void)?
    private var autoRefreshTask: Task<Void, Never>?
    private var lastAutoSwitchAt: Date?

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
        priorityEditorValue = profile.priority
        autoSwitchAllowedEditorValue = profile.autoSwitchAllowed
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
            let automation = try store.updateAutomation(
                profileID: updated.id,
                priority: priorityEditorValue,
                autoSwitchAllowed: autoSwitchAllowedEditorValue
            )
            selectedProfileID = automation.id
            reload()
            statusMessage = "Saved \(automation.alias)."
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

    func exportBackup() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.json]
        panel.nameFieldStringValue = "codex-quota-dock-backup.json"
        panel.begin { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            Task { @MainActor in
                self?.exportBackup(to: url)
            }
        }
    }

    private func exportBackup(to url: URL) {
        do {
            let data = try BackupStore.exportBackup(store: store, settings: settings)
            try data.write(to: url, options: .atomic)
            statusMessage = "Exported backup."
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func importBackup() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.json]
        panel.allowsMultipleSelection = false
        panel.begin { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            Task { @MainActor in
                self?.importBackup(from: url)
            }
        }
    }

    private func importBackup(from url: URL) {
        do {
            let data = try Data(contentsOf: url)
            let summary = try BackupStore.importBackup(into: store, data: data)
            reload()
            statusMessage = "Imported backup: \(summary.created) created, \(summary.updated) updated, \(summary.skipped) skipped."
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func restoreLatestBackup() {
        do {
            try BackupStore.restoreLatestBackup(
                from: store.backupsDirectory,
                to: paths.defaultCodexAuth
            )
            reload()
            statusMessage = "Restored latest auth backup. Restart Codex to use it."
        } catch {
            statusMessage = error.localizedDescription
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
        switchProfile(profile, showAlert: true, reason: nil)
    }

    private func switchProfile(_ profile: Profile, showAlert: Bool, reason: AutoSwitchReason?) {
        do {
            let auth = try store.authJSON(for: profile)
            let result = try switcher.switchAuth(
                activeAuth: paths.defaultCodexAuth,
                targetAuthJSON: auth,
                backupsDirectory: store.backupsDirectory
            )
            let restart = settings.autoRestartCodex ? CodexProcessService().restartCodex(appPath: settings.codexAppPath).message : "Restart Codex to use the new auth."
            lastAutoSwitchAt = Date()
            reload()
            if showAlert {
                showSwitchAlert(profile: profile, result: result, restart: restart)
            } else {
                let reasonText = reason.map { statusText(for: $0) } ?? "manual switch"
                statusMessage = "Auto switched to \(profile.alias): \(reasonText). \(restart)"
            }
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
        quotaByProfileID = quotas
        rebuildMonitorRows()
        statusMessage = "Refreshed \(DateFormatter.localizedString(from: Date(), dateStyle: .none, timeStyle: .short))"
        evaluateAutoSwitch()
    }

    func refreshLocalUsage() {
        guard !usageLoading else { return }
        usageLoading = true
        let codexRoot = paths.defaultCodexRoot
        Task { [weak self] in
            let summary = await Task.detached {
                LocalUsageScanner.scan(codexRoot: codexRoot)
            }.value
            self?.localUsageSummary = summary
            self?.usageLoading = false
        }
    }

    func saveSettings() {
        do {
            settings = settings.validated()
            try settingsStore.save(settings)
            startAutoRefresh()
            settingsChangedHandler?(settings)
            statusMessage = "Saved settings."
        } catch {
            statusMessage = error.localizedDescription
        }
    }

    func detectCodexAppPath() {
        if let path = CodexProcessService().detectCodexAppPath(configuredPath: settings.codexAppPath) {
            settings.codexAppPath = path
            statusMessage = "Detected Codex app."
        } else {
            statusMessage = "Codex app was not found. Set the path manually."
        }
    }

    func checkUpdates() {
        guard !updateChecking else { return }
        updateChecking = true
        updateStatusMessage = "Checking GitHub releases..."
        updateResult = nil

        Task { @MainActor in
            do {
                let result = try await UpdateChecker().checkLatestRelease()
                updateResult = result
                updateStatusMessage = result.message
            } catch {
                updateStatusMessage = error.localizedDescription
            }
            updateChecking = false
        }
    }

    func openLatestRelease() {
        guard let urlText = updateResult?.releaseURL,
              let url = URL(string: urlText)
        else {
            statusMessage = "Check for updates first."
            return
        }
        NSWorkspace.shared.open(url)
    }

    func openUpdateAsset() {
        guard let urlText = updateResult?.asset?.browserDownloadURL,
              let url = URL(string: urlText)
        else {
            openLatestRelease()
            return
        }
        NSWorkspace.shared.open(url)
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

    private func rebuildMonitorRows() {
        let activeAccount = activeAccountID()
        monitorRows = profiles
            .filter { shouldShowInMonitor($0, activeAccount: activeAccount) }
            .map { profile in
                let quota = quotaByProfileID[profile.id] ?? ProfileQuota(
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

    private func evaluateAutoSwitch() {
        let safeSettings = settings.validated()
        guard safeSettings.autoSwitchMode != .off else { return }
        let activeAccount = activeAccountID()
        let currentProfile = profiles.first { !$0.accountID.isEmpty && $0.accountID == activeAccount } ?? selectedProfile
        let decision = AutoSwitchPolicy.decide(
            mode: safeSettings.autoSwitchMode,
            current: currentProfile.map { autoSwitchCandidate(for: $0, activeAccount: activeAccount) },
            candidates: profiles.map { autoSwitchCandidate(for: $0, activeAccount: activeAccount) },
            context: AutoSwitchContext(
                codexRunning: CodexProcessService().isCodexRunning(),
                idleMinutes: systemIdleMinutes(),
                lastSwitchAt: lastAutoSwitchAt,
                now: Date()
            ),
            switchAwayThreshold: safeSettings.switchAwayThreshold,
            switchToThreshold: safeSettings.switchToThreshold,
            cooldownMinutes: safeSettings.autoSwitchCooldownMinutes,
            requiredIdleMinutes: safeSettings.autoSwitchIdleMinutes,
            quotaPriorityMode: safeSettings.quotaPriorityMode,
            quotaPriorityFiveHourThreshold: safeSettings.quotaPriorityFiveHourThreshold,
            quotaPriorityWeeklyThreshold: safeSettings.quotaPriorityWeeklyThreshold
        )

        switch decision {
        case .none:
            return
        case .notify(let targetID, let reason):
            if let target = profiles.first(where: { $0.id == targetID }) {
                statusMessage = "Auto switch suggested: \(target.alias) (\(statusText(for: reason)))."
            }
        case .pendingUntilIdle(let targetID, let reason):
            if let target = profiles.first(where: { $0.id == targetID }) {
                statusMessage = "Pending switch to \(target.alias) when idle (\(statusText(for: reason)))."
            }
        case .switchNow(let targetID, let reason):
            if let target = profiles.first(where: { $0.id == targetID }) {
                switchProfile(target, showAlert: false, reason: reason)
            }
        }
    }

    private func autoSwitchCandidate(for profile: Profile, activeAccount: String?) -> AutoSwitchCandidate {
        let quota = quotaByProfileID[profile.id]
        return AutoSwitchCandidate(
            profileID: profile.id,
            alias: profile.alias,
            priority: profile.priority,
            autoSwitchAllowed: profile.autoSwitchAllowed,
            fiveHourRemainingPercent: quota?.fiveHour.remainingPercent,
            weeklyRemainingPercent: quota?.weekly.remainingPercent,
            isActive: !profile.accountID.isEmpty && profile.accountID == activeAccount
        )
    }

    private func systemIdleMinutes() -> Int {
        Int(CGEventSource.secondsSinceLastEventType(.combinedSessionState, eventType: .null) / 60)
    }

    private func statusText(for reason: AutoSwitchReason) -> String {
        switch reason {
        case .currentQuotaLow:
            "quota threshold reached"
        case .preferredProfileRecovered:
            "preferred profile recovered"
        case .quotaPriorityRecovered:
            "quota priority recovered"
        }
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
