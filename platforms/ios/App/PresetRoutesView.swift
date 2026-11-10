import SwiftUI

struct PresetRoutesView: View {
    @ObservedObject var model: AppModel
    @Environment(\.dismiss) private var dismiss
    @State private var name = ""
    @State private var slot = 1
    var body: some View {
        NavigationStack {
            Form {
                Section {
                    Toggle("后台待命", isOn: Binding(get: { model.presetStandby }, set: model.setPresetStandby))
                    Text("先在前台开启并允许定位，再锁屏或切到后台。圆屏与手机需保持蓝牙连接；强制退出 App 后无法保证响应。待命会持续使用定位并增加耗电。")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                if model.selectedRoutePreview != nil {
                    Section("保存当前规划路线") {
                        TextField("路线名称，如通勤、周末山路", text: $name)
                        Picker("TF 槽位", selection: $slot) {
                            ForEach(1...4, id: \.self) { Text("槽位 \($0)").tag($0) }
                        }
                        Button("保存到手机（替换此槽位）") { model.saveCurrentPreset(slot: slot, name: name) }
                            .disabled(name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || model.presetTransferProgress != nil)
                    }
                }
                if model.savedPresetRoutes.isEmpty {
                    Section("四条预设路线") {
                        Text("请先搜索终点、规划并选择路线，再打开预设路线保存。")
                    }
                }
                ForEach(model.savedPresetRoutes) { route in
                    Section("\(route.slot). \(route.name)") {
                        VStack(alignment: .leading, spacing: 8) {
                            Text("\(route.destination.name) · \(Int(route.route.totalDistanceM)) m")
                                .font(.caption).foregroundStyle(.secondary)
                            Text(model.isPresetTransferred(route) ? "已传输；以圆屏 TF 列表为准" : "尚未传到 TF 卡")
                                .font(.caption).foregroundStyle(.secondary)
                        }
                        // Each action has its own Form row and explicit button style.
                        // Automatic buttons in one row can share the row's tap action.
                        Button("传到圆屏 TF 卡") { Task { await model.transferPreset(route) } }
                            .buttonStyle(.borderless)
                            .accessibilityIdentifier("preset-transfer-\(route.slot)")
                            .disabled(model.presetTransferProgress != nil || model.isNavigationActive)
                        Button("在手机中使用") { model.startPreset(token: route.token) }
                            .buttonStyle(.borderless)
                            .accessibilityIdentifier("preset-use-\(route.slot)")
                            .disabled(model.presetTransferProgress != nil)
                    }
                }
                Section {
                    Text(model.isNavigationActive ? "当前正在导航，请先结束导航再传输路线。" : "当前未导航，传输路线不会开始导航。")
                        .font(.footnote).foregroundStyle(.secondary)
                        .accessibilityIdentifier("preset-navigation-status")
                }
                if let progress = model.presetTransferProgress {
                    Section { ProgressView("正在传输 \(Int(progress * 100))%，请保持 App 前台", value: progress) }
                }
                if let message = model.presetMessage {
                    Section { Text(message).font(.footnote).accessibilityIdentifier("preset-status-message") }
                }
                Section {
                    Text("TF 路线包含折线与转向信息，不包含离线地图。首次调取使用保存路线，偏航重算和实时路况仍需网络；离起点较远时应先检查路线是否适用。手机保留一份对应路线，重装 App 或换手机后需重新传输。")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("TF 预设路线")
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("完成") { dismiss() } } }
        }
    }
}
