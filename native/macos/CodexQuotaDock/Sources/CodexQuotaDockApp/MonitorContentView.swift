import CodexQuotaDockCore
import SwiftUI

struct MonitorContentView: View {
    @ObservedObject var model: NativeAppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Image(systemName: "gauge.with.dots.needle.33percent")
                    .font(.system(size: 16, weight: .semibold))
                Text("Codex")
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
                ForEach(model.monitorRows.prefix(2)) { row in
                    MonitorRowView(
                        row: row,
                        fiveHourWarningThreshold: model.settings.fiveHourAlertThreshold,
                        weeklyWarningThreshold: model.settings.weeklyAlertThreshold
                    )
                }
            }

            HStack(spacing: 8) {
                Button("Refresh") {
                    Task { await model.refreshQuotas() }
                }
                Button("Switch") {
                    model.switchSelectedProfile()
                }
                Button("Settings") {
                    model.openSettings()
                }
                Spacer()
            }
            .buttonStyle(.bordered)
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
    let fiveHourWarningThreshold: Int
    let weeklyWarningThreshold: Int

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text(row.alias)
                    .font(.system(size: 13, weight: .semibold))
                if row.isActive {
                    Text("active")
                        .font(.caption2)
                        .foregroundStyle(.green)
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
        .padding(8)
        .background(
            row.isBelow(
                fiveHourThreshold: fiveHourWarningThreshold,
                weeklyThreshold: weeklyWarningThreshold
            ) ? Color.red.opacity(0.14) : Color.primary.opacity(0.06)
        )
        .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
    }

    private func quotaLine(_ window: QuotaWindow) -> some View {
        HStack {
            Text(window.label)
                .font(.caption)
                .foregroundStyle(.secondary)
                .frame(width: 48, alignment: .leading)
            ProgressView(value: Double(window.remainingPercent ?? 0), total: 100)
            Text(window.remainingPercent.map { "\($0)%" } ?? "--")
                .font(.caption)
                .monospacedDigit()
                .frame(width: 40, alignment: .trailing)
        }
    }
}
