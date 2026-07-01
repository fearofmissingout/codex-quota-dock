import CodexQuotaDockCore
import SwiftUI

struct MonitorContentView: View {
    @ObservedObject var model: NativeAppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Image(systemName: "gauge.with.dots.needle.33percent")
                    .font(.system(size: 16, weight: .semibold))
                Text("Codex Quota")
                    .font(.system(size: 14, weight: .semibold))
                Spacer()
                Text(model.statusMessage)
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }

            if model.monitorRows.isEmpty {
                Text("No active or pinned profiles")
                    .font(.callout)
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, minHeight: 72, alignment: .center)
            } else {
                ForEach(model.monitorRows) { row in
                    MonitorRowView(
                        row: row,
                        isSelected: row.id == model.selectedProfileID,
                        fiveHourWarningThreshold: model.settings.fiveHourAlertThreshold,
                        weeklyWarningThreshold: model.settings.weeklyAlertThreshold
                    )
                    .contentShape(Rectangle())
                    .onTapGesture {
                        model.selectProfile(row.id)
                    }
                }
            }

            HStack(spacing: 8) {
                Button("Refresh") {
                    Task { await model.refreshQuotas() }
                }
                Button("Switch") {
                    model.switchSelectedProfile()
                }
                .disabled(model.selectedProfileID == nil)
                Button("Settings") {
                    model.openSettings()
                }
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.small)
        }
        .padding(14)
        .frame(width: 380)
        .background(.ultraThinMaterial)
        .clipShape(RoundedRectangle(cornerRadius: 18, style: .continuous))
    }
}

private struct MonitorRowView: View {
    let row: MonitorProfileState
    let isSelected: Bool
    let fiveHourWarningThreshold: Int
    let weeklyWarningThreshold: Int

    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            HStack {
                Text(row.alias)
                    .font(.system(size: 13, weight: .semibold))
                if row.isActive {
                    Text("current")
                        .font(.caption2)
                        .padding(.horizontal, 8)
                        .padding(.vertical, 2)
                        .background(Capsule().fill(Color.accentColor.opacity(0.16)))
                }
                if row.isPinned {
                    Image(systemName: "pin.fill")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
                Spacer()
                Text(row.accountSuffix)
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
            quotaLine(row.fiveHour)
            quotaLine(row.weekly)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
        .background(
            row.isBelow(
                fiveHourThreshold: fiveHourWarningThreshold,
                weeklyThreshold: weeklyWarningThreshold
            ) ? Color.red.opacity(0.14) : Color.primary.opacity(backgroundOpacity)
        )
        .clipShape(RoundedRectangle(cornerRadius: 9, style: .continuous))
        .overlay(
            RoundedRectangle(cornerRadius: 9, style: .continuous)
                .stroke(borderColor)
        )
    }

    private var backgroundOpacity: Double {
        if isSelected {
            return 0.12
        }
        if row.isActive {
            return 0.08
        }
        return 0.06
    }

    private var borderColor: Color {
        if isSelected {
            return Color.accentColor.opacity(0.65)
        }
        if row.isActive {
            return Color.secondary.opacity(0.24)
        }
        return Color.secondary.opacity(0.16)
    }

    private func quotaLine(_ window: QuotaWindow) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            HStack {
                Text(formatQuotaLine(window))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                Spacer(minLength: 8)
            }
            ProgressView(value: Double(window.remainingPercent ?? 0), total: 100)
                .progressViewStyle(.linear)
                .tint(tint(for: window.remainingPercent))
        }
    }

    private func formatQuotaLine(_ window: QuotaWindow) -> String {
        guard let percent = window.remainingPercent else {
            return "\(window.label): not refreshed"
        }
        if let resetsAt = window.resetsAt {
            let text = DateFormatter.localizedString(from: resetsAt, dateStyle: .none, timeStyle: .short)
            return "\(window.label): \(percent)% left, resets \(text)"
        }
        return "\(window.label): \(percent)% left"
    }

    private func tint(for percent: Int?) -> Color {
        guard let percent else { return .secondary }
        if percent <= 3 { return .red }
        if percent <= 15 { return .orange }
        return .green
    }
}
