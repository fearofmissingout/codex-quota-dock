import CodexQuotaDockCore
import SwiftUI

private enum SettingsTab: String, CaseIterable, Identifiable {
    case auth = "Auth"
    case quota = "Quota"
    case usage = "Usage"
    case settings = "Settings"
    case health = "Health"
    case updates = "Updates"

    var id: String { rawValue }

    var icon: String {
        switch self {
        case .auth: "key"
        case .quota: "gauge.with.dots.needle.50percent"
        case .usage: "chart.bar"
        case .settings: "gearshape"
        case .health: "heart.text.square"
        case .updates: "arrow.up.circle"
        }
    }
}

struct SettingsContentView: View {
    @ObservedObject var model: NativeAppModel
    @State private var tab: SettingsTab = .quota

    var body: some View {
        HSplitView {
            profileSidebar
                .frame(minWidth: 300, idealWidth: 320)

            VStack(alignment: .leading, spacing: 12) {
                Picker("", selection: $tab) {
                    ForEach(SettingsTab.allCases) { item in
                        Label(item.rawValue, systemImage: item.icon).tag(item)
                    }
                }
                .pickerStyle(.segmented)
                .labelsHidden()
                .onChange(of: tab) { newValue in
                    if newValue == .usage {
                        model.refreshLocalUsage()
                    }
                }

                Group {
                    switch tab {
                    case .auth:
                        authTab
                    case .quota:
                        quotaTab
                    case .usage:
                        UsageTabView(model: model)
                    case .settings:
                        settingsTab
                    case .health:
                        healthTab
                    case .updates:
                        updatesTab
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            }
            .padding(16)
            .frame(minWidth: 620)
        }
        .frame(minWidth: 960, minHeight: 640)
        .onAppear { model.reload() }
    }

    private var profileSidebar: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Profiles")
                    .font(.headline)
                Spacer()
                Text(model.statusMessage)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }

            List(selection: Binding(
                get: { model.selectedProfileID },
                set: { model.selectProfile($0) }
            )) {
                ForEach(model.profiles) { profile in
                    VStack(alignment: .leading, spacing: 3) {
                        HStack {
                            Text(profile.alias)
                                .font(.system(size: 13, weight: .semibold))
                            if profile.pinned {
                                Image(systemName: "pin.fill").font(.caption2)
                            }
                            Text("P\(profile.priority)")
                                .font(.caption2)
                                .foregroundStyle(.secondary)
                        }
                        Text(profile.accountSuffix.isEmpty ? "No account ID" : profile.accountSuffix)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    .tag(Optional(profile.id))
                }
            }

            Grid(horizontalSpacing: 8, verticalSpacing: 8) {
                GridRow {
                    Button("Import Current") { model.importCurrent() }
                    Button("Import File") { model.importFile() }
                    Button("New Profile") { model.createProfile() }
                }
                GridRow {
                    Button("Delete") { model.deleteSelectedProfile() }
                    Button("Pin") { model.togglePinnedSelectedProfile() }
                    Button("Switch") { model.switchSelectedProfile() }
                }
                GridRow {
                    Button("Export Backup") { model.exportBackup() }
                    Button("Import Backup") { model.importBackup() }
                    Button("Restore") { model.restoreLatestBackup() }
                }
            }
            .buttonStyle(.bordered)

            HStack {
                Text("Alias")
                    .frame(width: 48, alignment: .leading)
                TextField("profile alias", text: $model.aliasEditorText)
            }

            HStack {
                Button("Save Profile") { model.saveSelectedProfile() }
                Button("Save Settings") { model.saveSettings() }
            }
            .buttonStyle(.borderedProminent)
        }
        .padding(16)
    }

    private var authTab: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Saved auth.json")
                .font(.subheadline)
                .foregroundStyle(.secondary)
            TextEditor(text: $model.authEditorText)
                .font(.system(.body, design: .monospaced))
                .scrollContentBackground(.hidden)
                .background(Color.primary.opacity(0.04))
                .clipShape(RoundedRectangle(cornerRadius: 8, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.secondary.opacity(0.18)))
            HStack {
                Stepper("Priority P\(model.priorityEditorValue) (P0 highest)", value: $model.priorityEditorValue, in: 0...100)
                Toggle("Allow auto switch", isOn: $model.autoSwitchAllowedEditorValue)
                Spacer()
                Button("Reload") { model.loadSelectedProfileEditor() }
                Button("Save Profile") { model.saveSelectedProfile() }
            }
        }
    }

    private var quotaTab: some View {
        VStack(alignment: .leading, spacing: 14) {
            if model.monitorRows.isEmpty {
                Text("No current or pinned profiles are visible in the monitor.")
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .center)
            } else {
                ForEach(model.monitorRows) { row in
                    VStack(alignment: .leading, spacing: 8) {
                        HStack {
                            Text(row.alias).font(.headline)
                            if row.isActive {
                                Text("current")
                                    .font(.caption)
                                    .padding(.horizontal, 8)
                                    .padding(.vertical, 2)
                                    .background(Capsule().fill(Color.accentColor.opacity(0.16)))
                            }
                            Spacer()
                            Text(row.accountSuffix)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        quotaLine(row.fiveHour)
                        quotaLine(row.weekly)
                    }
                    .padding(14)
                    .background(RoundedRectangle(cornerRadius: 8).fill(Color.primary.opacity(0.045)))
                    .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.secondary.opacity(0.18)))
                }
            }
        }
    }

    private func quotaLine(_ window: QuotaWindow) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text(quotaText(window))
                    .font(.callout)
                Spacer()
            }
            ProgressView(value: Double(window.remainingPercent ?? 0), total: 100)
                .tint(quotaTint(window.remainingPercent))
        }
    }

    private var settingsTab: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                GroupBox("Refresh") {
                    Picker("Refresh every", selection: $model.settings.pollIntervalMinutes) {
                        ForEach(AppSettings.allowedPollIntervalMinutes, id: \.self) { minutes in
                            Text("\(minutes) min").tag(minutes)
                        }
                    }
                    .frame(width: 260)
                }

                GroupBox("Alerts") {
                    HStack {
                        Picker("5h alert", selection: $model.settings.fiveHourAlertThreshold) {
                            ForEach([0, 5, 10, 15, 20, 30, 40], id: \.self) { Text($0 == 0 ? "Off" : "\($0)%").tag($0) }
                        }
                        Picker("Weekly alert", selection: $model.settings.weeklyAlertThreshold) {
                            ForEach([0, 5, 10, 15, 20, 30, 40], id: \.self) { Text($0 == 0 ? "Off" : "\($0)%").tag($0) }
                        }
                    }
                }

                GroupBox("Auto Switch") {
                    VStack(alignment: .leading, spacing: 10) {
                        Picker("Mode", selection: $model.settings.autoSwitchMode) {
                            Text("Off").tag(AutoSwitchMode.off)
                            Text("Notify").tag(AutoSwitchMode.notify)
                            Text("When Codex Closed").tag(AutoSwitchMode.whenCodexClosed)
                            Text("When Idle").tag(AutoSwitchMode.whenIdle)
                        }
                        Stepper("Switch away at \(model.settings.switchAwayThreshold)%", value: $model.settings.switchAwayThreshold, in: 1...50)
                        Stepper("Switch to profile above \(model.settings.switchToThreshold)%", value: $model.settings.switchToThreshold, in: 2...100)
                        Stepper("Idle for \(model.settings.autoSwitchIdleMinutes) min", value: $model.settings.autoSwitchIdleMinutes, in: 1...60)
                        Stepper("Cooldown \(model.settings.autoSwitchCooldownMinutes) min", value: $model.settings.autoSwitchCooldownMinutes, in: 1...120)
                        Toggle("Quota priority mode", isOn: $model.settings.quotaPriorityMode)
                        Stepper("Priority 5h recovers at \(model.settings.quotaPriorityFiveHourThreshold)%", value: $model.settings.quotaPriorityFiveHourThreshold, in: 0...100)
                        Stepper("Priority weekly recovers at \(model.settings.quotaPriorityWeeklyThreshold)%", value: $model.settings.quotaPriorityWeeklyThreshold, in: 0...100)
                    }
                }

                GroupBox("Codex Launch") {
                    VStack(alignment: .leading, spacing: 10) {
                        Toggle("Restart Codex after switch", isOn: $model.settings.autoRestartCodex)
                        HStack {
                            TextField("/Applications/Codex.app", text: $model.settings.codexAppPath)
                            Button("Auto Detect") { model.detectCodexAppPath() }
                        }
                    }
                }

                GroupBox("Monitor") {
                    Toggle("Keep monitor above other apps", isOn: $model.settings.monitorAlwaysOnTop)
                }

                Button("Save Settings") { model.saveSettings() }
                    .buttonStyle(.borderedProminent)
            }
            .padding(.trailing, 8)
        }
    }

    private var healthTab: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("Health").font(.headline)
                Button("Refresh") { model.refreshHealth() }
            }
            ForEach(model.healthRows) { row in
                HStack(alignment: .top, spacing: 10) {
                    Text(row.status.rawValue.uppercased())
                        .font(.caption)
                        .foregroundStyle(row.status == .ok ? .green : row.status == .warning ? .orange : .red)
                        .frame(width: 70, alignment: .leading)
                    Text(row.label)
                        .frame(width: 120, alignment: .leading)
                    Text(row.detail)
                        .foregroundStyle(.secondary)
                        .textSelection(.enabled)
                }
                .font(.callout)
            }
        }
    }

    private var updatesTab: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Updates").font(.headline)
            Text("Current version: \(NativeVersion.current)")
                .foregroundStyle(.secondary)
            HStack {
                Button(model.updateChecking ? "Checking..." : "Check Updates") { model.checkUpdates() }
                    .disabled(model.updateChecking)
                Button(model.updateDownloading ? "Downloading..." : "Download") { model.downloadUpdate() }
                    .disabled(model.updateDownloading || model.updateResult?.asset == nil || model.updateResult?.checksumAsset == nil)
                Button("Install & Restart") { model.installDownloadedUpdateAndRestart() }
                    .disabled(model.downloadedUpdate == nil)
                Button("Open Release") { model.openLatestRelease() }
                    .disabled(model.updateResult == nil)
                Button("Privacy & Security") { model.openPrivacyAndSecurity() }
            }
            .buttonStyle(.bordered)
            Text(model.updateStatusMessage)
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
            if let result = model.updateResult {
                VStack(alignment: .leading, spacing: 6) {
                    Text("Latest: \(result.latestVersion)")
                    if let asset = result.asset {
                        Text("Asset: \(asset.name)")
                        Text("Size: \(formatBytes(Int64(asset.size)))")
                    }
                    if let downloaded = model.downloadedUpdate {
                        Text("Verified: \(downloaded.actualSHA256.prefix(12))...")
                    }
                    Text(result.releaseURL)
                        .textSelection(.enabled)
                }
                .font(.callout)
                .foregroundStyle(.secondary)
                .padding(14)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(RoundedRectangle(cornerRadius: 8).fill(Color.primary.opacity(0.045)))
                .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.secondary.opacity(0.18)))
            }
            Text("Unsigned macOS builds can be blocked before this app starts. If macOS shows an Apple verification warning, open Privacy & Security and approve Codex Quota Dock there, or build locally from source.")
                .font(.caption)
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private func formatBytes(_ value: Int64) -> String {
        let units = ["B", "KB", "MB", "GB"]
        var amount = Double(max(0, value))
        var index = 0
        while amount >= 1024, index < units.count - 1 {
            amount /= 1024
            index += 1
        }
        return String(format: index == 0 ? "%.0f %@" : "%.1f %@", amount, units[index])
    }

    private func quotaText(_ window: QuotaWindow) -> String {
        guard let percent = window.remainingPercent else {
            return "\(window.label): not reported"
        }
        if let resetsAt = window.resetsAt {
            return "\(window.label): \(percent)% left, resets \(QuotaFormatting.resetText(for: resetsAt))"
        }
        return "\(window.label): \(percent)% left"
    }

    private func quotaTint(_ percent: Int?) -> Color {
        guard let percent else { return .secondary }
        if percent <= 3 { return .red }
        if percent <= 15 { return .orange }
        return .green
    }
}

private struct UsageTabView: View {
    @ObservedObject var model: NativeAppModel

    var body: some View {
        ZStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    metricGrid
                    dailyChart
                    tokenMix
                    Text("Effective usage excludes cached input. Reasoning is included in output. SQLite totals are thread-level aggregates, so their daily window is based on thread updated time.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                .padding(.trailing, 8)
            }

            if model.usageLoading {
                VStack(spacing: 10) {
                    ProgressView()
                    Text("Loading usage")
                        .font(.headline)
                    Text("Scanning local Codex history...")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                .padding(24)
                .background(.regularMaterial)
                .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
            }
        }
        .onAppear { model.refreshLocalUsage() }
    }

    private var metricGrid: some View {
        Grid(horizontalSpacing: 10, verticalSpacing: 10) {
            GridRow {
                metric("Today eff.", model.localUsageSummary.today.effectiveTotal)
                metric("7 days eff.", model.localUsageSummary.last7Days.effectiveTotal)
                metric("30 days eff.", model.localUsageSummary.last30Days.effectiveTotal)
                metric("All eff.", model.localUsageSummary.total.effectiveTotal)
                metric("SQLite", model.localUsageSummary.sqlite.effectiveTotal)
            }
        }
    }

    private func metric(_ title: String, _ value: Int64) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(title)
                .font(.caption)
                .foregroundStyle(.secondary)
            Text(formatNumber(value))
                .font(.system(size: 17, weight: .semibold, design: .rounded))
                .monospacedDigit()
        }
        .frame(maxWidth: .infinity, minHeight: 62, alignment: .leading)
        .padding(.horizontal, 12)
        .background(RoundedRectangle(cornerRadius: 8).fill(Color.primary.opacity(0.045)))
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.secondary.opacity(0.18)))
    }

    private var dailyChart: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Daily effective usage")
                .font(.headline)
            Text("Last 7 days, cached input excluded")
                .font(.caption)
                .foregroundStyle(.secondary)
            HStack(alignment: .bottom, spacing: 14) {
                let days = lastSevenDays()
                let maxTotal = max(days.map { $0.usage.effectiveTotal }.max() ?? 0, 1)
                ForEach(days) { day in
                    VStack(spacing: 6) {
                        Text(formatCompact(day.usage.effectiveTotal))
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                        RoundedRectangle(cornerRadius: 5)
                            .fill(day.id == days.last?.id ? Color.accentColor : Color.green.opacity(0.8))
                            .frame(height: max(4, CGFloat(day.usage.effectiveTotal) / CGFloat(maxTotal) * 112))
                        Text(String(day.day.suffix(5)))
                            .font(.caption2)
                    }
                    .frame(maxWidth: .infinity)
                }
            }
            .frame(height: 170)
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 8).fill(Color.primary.opacity(0.045)))
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.secondary.opacity(0.18)))
    }

    private var tokenMix: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Overall token mix").font(.headline)
            GeometryReader { proxy in
                let segments = tokenSegments()
                let total = max(segments.map(\.value).reduce(0, +), 1)
                HStack(spacing: 0) {
                    ForEach(segments, id: \.name) { segment in
                        Rectangle()
                            .fill(segment.color)
                            .frame(width: max(1, proxy.size.width * CGFloat(segment.value) / CGFloat(total)))
                    }
                }
                .clipShape(RoundedRectangle(cornerRadius: 6, style: .continuous))
            }
            .frame(height: 22)
            Grid(horizontalSpacing: 22, verticalSpacing: 6) {
                GridRow {
                    ForEach(tokenSegments(), id: \.name) { segment in
                        Text("\(segment.name) \(formatCompact(segment.value))")
                            .font(.caption)
                            .foregroundStyle(segment.color)
                    }
                }
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 8).fill(Color.primary.opacity(0.045)))
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.secondary.opacity(0.18)))
    }

    private func tokenSegments() -> [(name: String, value: Int64, color: Color)] {
        let total = model.localUsageSummary.total
        return [
            ("Input", total.uncachedInput, .blue),
            ("Cached", total.cachedInput, .green),
            ("Output", total.output, .orange),
            ("SQLite", model.localUsageSummary.sqlite.effectiveTotal, .secondary),
        ].filter { $0.value > 0 }
    }

    private func lastSevenDays() -> [LocalUsageDay] {
        let known = Dictionary(uniqueKeysWithValues: model.localUsageSummary.byDay.map { ($0.day, $0.usage) })
        let formatter = DateFormatter()
        formatter.calendar = Calendar.current
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.dateFormat = "yyyy-MM-dd"
        let start = Calendar.current.startOfDay(for: Date())
        return (0..<7).map { offset in
            let date = Calendar.current.date(byAdding: .day, value: offset - 6, to: start) ?? start
            let key = formatter.string(from: date)
            return LocalUsageDay(day: key, usage: known[key] ?? UsageTotals())
        }
    }

    private func formatNumber(_ value: Int64) -> String {
        let formatter = NumberFormatter()
        formatter.numberStyle = .decimal
        return formatter.string(from: NSNumber(value: value)) ?? "\(value)"
    }

    private func formatCompact(_ value: Int64) -> String {
        if value >= 1_000_000 {
            return String(format: value >= 10_000_000 ? "%.0fM" : "%.1fM", Double(value) / 1_000_000)
        }
        if value >= 1_000 {
            return String(format: value >= 10_000 ? "%.0fK" : "%.1fK", Double(value) / 1_000)
        }
        return "\(value)"
    }
}
