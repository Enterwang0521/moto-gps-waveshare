#pragma once

#include <atomic>
#include <cstdint>

#include "moto/ble_protocol/ble_protocol.hpp"

struct ble_gatt_error;
struct ble_gatt_svc;
struct ble_gatt_chr;
struct ble_gatt_dsc;
struct ble_gatt_attr;
struct os_mbuf;

// An AMS GATT client on the same BLE link used by the navigation app.
class AppleMediaClient {
 public:
  // Volume is 0..100 when supplied by iOS, or -1 when unknown.
  using StateCallback = void (*)(bool, const moto::ble::MediaState&, int, void*);
  void set_callback(StateCallback callback, void* context);
  void connect(std::uint16_t connection);
  void disconnect();
  void notification(std::uint16_t connection, std::uint16_t attribute,
                    const os_mbuf* data);
  bool active() const { return active_.load(); }
  bool send(moto::ble::DeviceCommandKind kind);
  bool change_volume(bool increase);

 private:
  static int service_cb(std::uint16_t, const ble_gatt_error*,
                        const ble_gatt_svc*, void*);
  static int characteristic_cb(std::uint16_t, const ble_gatt_error*,
                               const ble_gatt_chr*, void*);
  static int descriptor_cb(std::uint16_t, const ble_gatt_error*, std::uint16_t,
                           const ble_gatt_dsc*, void*);
  static int setup_write_cb(std::uint16_t, const ble_gatt_error*,
                            ble_gatt_attr*, void*);
  void setup_write_done(std::uint16_t, int);
  void publish();
  void fail(const char* stage, int error);

  StateCallback callback_ = nullptr;
  void* callback_context_ = nullptr;
  std::atomic<std::uint16_t> connection_{0xffff};
  std::atomic<std::uint16_t> command_handle_{0};
  std::atomic<bool> active_{false};
  std::uint16_t service_start_ = 0, service_end_ = 0;
  std::uint16_t update_handle_ = 0, update_end_ = 0, update_cccd_ = 0;
  std::uint8_t setup_stage_ = 0;
  moto::ble::MediaState state_{};
  int volume_percent_ = -1;
  double playback_position_s_ = 0;
  double playback_rate_ = 0;
  std::uint64_t playback_updated_ms_ = 0;
  bool playback_position_known_ = false;
};
