import Foundation
import XCTest
@testable import MOTO_GPS

@MainActor
final class ShortcutRunnerTests: XCTestCase {
    func testShortcutNameIsOneEscapedURLParameter() throws {
        let name = "回家 & 音乐+导航 🌟"
        let url = try XCTUnwrap(ShortcutRunner.url(for: name))
        let components = try XCTUnwrap(URLComponents(url: url, resolvingAgainstBaseURL: false))
        XCTAssertEqual(components.scheme, "shortcuts")
        XCTAssertEqual(components.host, "run-shortcut")
        XCTAssertEqual(components.queryItems, [URLQueryItem(name: "name", value: name)])
        XCTAssertTrue(url.absoluteString.contains("%2B"))
    }
}
