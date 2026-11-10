import SwiftUI
import UserNotifications

struct ShortcutSettingsView: View {
    @Environment(\.dismiss) private var dismiss
    @AppStorage("MotoGPS.Shortcut.1.v1") private var first = ""
    @AppStorage("MotoGPS.Shortcut.2.v1") private var second = ""
    @AppStorage("MotoGPS.Shortcut.3.v1") private var third = ""
    @AppStorage("MotoGPS.Shortcut.SilentNotification.v1") private var silentNotification = false
    @State private var notificationStatus = ""

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("🚀 出发", text: $first)
                    TextField("🏠 回家", text: $second)
                    TextField("📍 常用地点", text: $third)
                } footer: {
                    Text("先在 iPhone 的“快捷指令”App 中创建指令，再填写完全相同的名称。设备会用对应图标显示快捷按键。")
                }
                Section {
                    Toggle("静默通知", isOn: $silentNotification)
                    Button("允许通知") { requestNotifications() }
                    if !notificationStatus.isEmpty {
                        Text(notificationStatus).foregroundStyle(.secondary)
                    }
                } footer: {
                    Text("静默通知关闭提示音。App 在前台时直接打开快捷指令；后台或锁屏时仍需轻点通知，iOS 或快捷指令也可能要求解锁或授权。")
                }
            }
            .textInputAutocapitalization(.never)
            .autocorrectionDisabled()
            .navigationTitle("圆屏快捷指令")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("完成") { dismiss() } } }
        }
    }

    private func requestNotifications() {
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { allowed, _ in
            DispatchQueue.main.async {
                notificationStatus = allowed ? "通知已允许" : "请在系统设置中允许 MOTO GPS 通知"
            }
        }
    }
}
