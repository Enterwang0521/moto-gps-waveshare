import Combine
import Foundation
import UIKit
import UserNotifications

/// A BLE request is only an invitation to run a user-configured shortcut.
/// iOS does not allow a background app to launch Shortcuts without user action.
@MainActor
final class ShortcutRunner: NSObject, ObservableObject {
    static let shared = ShortcutRunner()
    @Published private(set) var pendingName: String?

    private override init() {
        super.init()
        UNUserNotificationCenter.current().delegate = self
    }

    static func key(for slot: Int) -> String { "MotoGPS.Shortcut.\(slot).v1" }
    static let silentNotificationKey = "MotoGPS.Shortcut.SilentNotification.v1"

    static func url(for name: String) -> URL? {
        var components = URLComponents()
        components.scheme = "shortcuts"
        components.host = "run-shortcut"
        let allowed = CharacterSet(charactersIn: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~")
        guard let encoded = name.addingPercentEncoding(withAllowedCharacters: allowed) else { return nil }
        components.percentEncodedQuery = "name=\(encoded)"
        return components.url
    }

    func handle(slot: UInt16) -> BLECommandDisposition {
        guard (1...3).contains(slot),
              let raw = UserDefaults.standard.string(forKey: Self.key(for: Int(slot)))
        else { return .invalidState }
        let name = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !name.isEmpty else { return .invalidState }

        if UIApplication.shared.applicationState == .active {
            // Let the BLE handler send its acknowledgement before leaving the app.
            DispatchQueue.main.async { [weak self] in self?.open(name: name) }
        } else {
            let content = UNMutableNotificationContent()
            content.title = "运行快捷指令"
            content.body = "轻点以运行圆屏请求的快捷指令。"
            content.userInfo = ["shortcutName": name]
            if !UserDefaults.standard.bool(forKey: Self.silentNotificationKey) {
                content.sound = .default
            }
            let request = UNNotificationRequest(identifier: UUID().uuidString,
                                                content: content, trigger: nil)
            UNUserNotificationCenter.current().add(request, withCompletionHandler: nil)
        }
        return .accepted
    }

    func openPendingIfActive() {
        guard UIApplication.shared.applicationState == .active,
              let name = pendingName else { return }
        pendingName = nil
        open(name: name)
    }

    private func open(name: String) {
        guard let url = Self.url(for: name) else { return }
        UIApplication.shared.open(url)
    }
}

extension ShortcutRunner: UNUserNotificationCenterDelegate {
    nonisolated func userNotificationCenter(
        _ center: UNUserNotificationCenter,
        didReceive response: UNNotificationResponse,
        withCompletionHandler completionHandler: @escaping () -> Void
    ) {
        let name = response.notification.request.content.userInfo["shortcutName"] as? String
        Task { @MainActor in
            if let name, !name.isEmpty {
                self.pendingName = name
                self.openPendingIfActive()
            }
            completionHandler()
        }
    }
}
