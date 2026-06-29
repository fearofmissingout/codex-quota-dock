import CodexQuotaDockCore
import SwiftUI

struct SettingsContentView: View {
    @ObservedObject var model: NativeAppModel

    var body: some View {
        HSplitView {
            VStack(alignment: .leading, spacing: 12) {
                Text("Profiles")
                    .font(.headline)
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
                            }
                            Text(profile.accountSuffix.isEmpty ? "No account ID" : profile.accountSuffix)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        .tag(Optional(profile.id))
                    }
                }
                .frame(minWidth: 240)

                HStack {
                    Button("Import Current") { model.importCurrent() }
                    Button("Import File") { model.importFile() }
                }
                HStack {
                    Button("Pin") { model.togglePinnedSelectedProfile() }
                    Button("Delete") { model.deleteSelectedProfile() }
                }
            }
            .padding()
            .frame(minWidth: 280)

            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    Text("Profile Editor")
                        .font(.headline)
                    Spacer()
                    Text(model.statusMessage)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                HStack {
                    Text("Alias")
                        .frame(width: 64, alignment: .leading)
                    TextField("profile alias", text: $model.aliasEditorText)
                }

                Text("Auth JSON")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
                TextEditor(text: $model.authEditorText)
                    .font(.system(.body, design: .monospaced))
                    .frame(minHeight: 240)
                    .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.secondary.opacity(0.2)))

                HStack {
                    Button("New Profile") { model.createProfile() }
                    Button("Save Profile") { model.saveSelectedProfile() }
                    Button("Reload") { model.loadSelectedProfileEditor() }
                    Button("Switch Selected") { model.switchSelectedProfile() }
                }

                Divider()

                settingsSection
                healthSection
            }
            .padding()
            .frame(minWidth: 560)
        }
        .onAppear { model.reload() }
    }

    private var settingsSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Settings")
                .font(.headline)
            HStack {
                Picker("Poll", selection: $model.settings.pollIntervalMinutes) {
                    Text("1 min").tag(1)
                    Text("5 min").tag(5)
                    Text("10 min").tag(10)
                }
                .frame(width: 190)
                Picker("5h Alert", selection: $model.settings.fiveHourAlertThreshold) {
                    ForEach([0, 5, 10, 15, 20, 30, 40], id: \.self) { Text($0 == 0 ? "Off" : "\($0)%").tag($0) }
                }
                .frame(width: 170)
                Picker("Weekly Alert", selection: $model.settings.weeklyAlertThreshold) {
                    ForEach([0, 5, 10, 15, 20, 30, 40], id: \.self) { Text($0 == 0 ? "Off" : "\($0)%").tag($0) }
                }
                .frame(width: 190)
                Toggle("Restart Codex after switch", isOn: $model.settings.autoRestartCodex)
            }
            Button("Save Settings") { model.saveSettings() }
        }
    }

    private var healthSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("Health")
                    .font(.headline)
                Button("Refresh") { model.refreshHealth() }
            }
            ForEach(model.healthRows) { row in
                HStack(alignment: .top) {
                    Text(row.status.rawValue.uppercased())
                        .font(.caption)
                        .frame(width: 64, alignment: .leading)
                    Text(row.label)
                        .frame(width: 100, alignment: .leading)
                    Text(row.detail)
                        .foregroundStyle(.secondary)
                        .textSelection(.enabled)
                }
                .font(.caption)
            }
        }
    }
}
