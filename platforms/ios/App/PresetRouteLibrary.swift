import Foundation
import MotoNavigationCore

enum PresetRouteError: LocalizedError {
    case message(String)
    var errorDescription: String? { if case let .message(text) = self { return text }; return nil }
}
struct SavedPresetRoute: Codable, Identifiable {
    let token: UInt32
    let slot: Int
    let name: String
    let origin: WGS84Point
    let destination: PlaceSearchResult
    let route: RoutePlan
    var id: UInt32 { token }
}
@MainActor
final class PresetRouteLibrary {
    private struct State: Codable {
        var records: [SavedPresetRoute] = []
        var transferred: [Int: UInt32] = [:]
    }
    private var state = State()
    private var loadFailure: String?
    private let url: URL
    var current: [SavedPresetRoute] {
        (1...4).compactMap { slot in state.records.last { $0.slot == slot } }
    }
    init() {
        url = URL.applicationSupportDirectory.appendingPathComponent("PresetRoutes-v1.json")
        if FileManager.default.fileExists(atPath: url.path) {
            do { state = try JSONDecoder().decode(State.self, from: Data(contentsOf: url)) }
            catch { loadFailure = "已保存路线无法读取，原文件已保留：\(error.localizedDescription)" }
        }
    }
    func find(token: UInt32) -> SavedPresetRoute? { state.records.first { $0.token == token } }
    func isTransferred(_ route: SavedPresetRoute) -> Bool { state.transferred[route.slot] == route.token }
    private func persist(_ next: State) throws {
        if let loadFailure { throw PresetRouteError.message(loadFailure) }
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try JSONEncoder().encode(next).write(to: url, options: .atomic)
        try FileManager.default.setAttributes([.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication], ofItemAtPath: url.path)
        state = next
    }
    func save(slot: Int, name: String, origin: WGS84Point, destination: PlaceSearchResult,
              route: RoutePlan) throws -> SavedPresetRoute {
        let name = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard (1...4).contains(slot), !name.isEmpty, name.utf8.count <= 63 else {
            throw PresetRouteError.message("路线名称需为 1～63 字节，最多约 21 个汉字")
        }
        var token = UInt32.random(in: 1...UInt32.max)
        while find(token: token) != nil { token = UInt32.random(in: 1...UInt32.max) }
        let saved = SavedPresetRoute(token: token, slot: slot, name: name, origin: origin,
                                     destination: destination, route: RoutePlan(routeID: route.routeID,
            provider: route.provider, coordinateSystem: route.coordinateSystem,
            generatedAtMs: route.generatedAtMs, totalDistanceM: route.totalDistanceM,
            totalDurationS: route.totalDurationS, polyline: route.polyline,
            maneuvers: route.maneuvers, traffic: []))
        guard try JSONEncoder().encode(saved).count <= 262_144 else {
            throw PresetRouteError.message("路线超过 256 KiB，请缩短路线后重试")
        }
        var next = state
        // Keep the previous generation so a failed TF replacement remains selectable.
        let previous = next.records.first { $0.slot == slot && $0.token == next.transferred[slot] }
            ?? next.records.last { $0.slot == slot }
        next.records.removeAll { $0.slot == slot }
        if let previous { next.records.append(previous) }
        next.records.append(saved)
        try persist(next)
        return saved
    }
    func transferred(_ route: SavedPresetRoute) throws {
        var next = state; next.transferred[route.slot] = route.token; try persist(next)
    }
    nonisolated static func crc32(_ data: Data) -> UInt32 {
        var crc: UInt32 = 0xffff_ffff
        for byte in data {
            crc ^= UInt32(byte)
            for _ in 0..<8 { crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xedb8_8320 : 0) }
        }
        return ~crc
    }
}
