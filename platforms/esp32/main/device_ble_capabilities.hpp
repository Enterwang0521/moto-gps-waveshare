#pragma once
#include "moto/ble_protocol/ble_protocol.hpp"
namespace moto::esp32 {
constexpr uint32_t device_capabilities() {
  constexpr uint32_t common = ble::CapabilityNavigation | ble::CapabilityRouteGeometry |
      ble::CapabilityTraffic | ble::CapabilityMediaState | ble::CapabilityTouchCommands |
      ble::CapabilityMusicCommands | ble::CapabilityCommandAck | ble::CapabilityMapScene;
#if defined(MOTO_BOARD_SMARTRING_PLUS)
  return common | ble::CapabilityPresetRoutes;
#else
  return common | ble::CapabilityShortcuts;
#endif
}
}
