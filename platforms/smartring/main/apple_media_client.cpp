#include "apple_media_client.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "esp_log.h"
#include "esp_timer.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"

namespace {
constexpr char kTag[] = "apple_media";
const ble_uuid128_t kService =
    BLE_UUID128_INIT(0xdc, 0xf8, 0x55, 0xad, 0x02, 0xc5, 0xf4, 0x8e, 0x3a, 0x43,
                     0x36, 0x0f, 0x2b, 0x50, 0xd3, 0x89);
const ble_uuid128_t kCommand =
    BLE_UUID128_INIT(0xc2, 0x51, 0xca, 0xf7, 0x56, 0x0e, 0xdf, 0xb8, 0x8a, 0x4a,
                     0xb1, 0x57, 0xd8, 0x81, 0x3c, 0x9b);
const ble_uuid128_t kUpdate =
    BLE_UUID128_INIT(0x02, 0xc1, 0x96, 0xba, 0x92, 0xbb, 0x0c, 0x9a, 0x1f, 0x41,
                     0x8d, 0x80, 0xce, 0xab, 0x7c, 0x2f);
const ble_uuid16_t kCccd = BLE_UUID16_INIT(0x2902);

// Keep complete UTF-8 characters within the UI's existing byte limits.
std::string clipped_text(const std::uint8_t* bytes, std::size_t length,
                         std::size_t limit) {
  std::size_t end = 0;
  while (end < length && end < limit && bytes[end] != 0) {
    const auto c = bytes[end];
    const std::size_t width = c < 0x80             ? 1
                              : (c & 0xe0) == 0xc0 ? 2
                              : (c & 0xf0) == 0xe0 ? 3
                              : (c & 0xf8) == 0xf0 ? 4
                                                   : 0;
    if (!width || end + width > length || end + width > limit) break;
    bool valid = true;
    for (std::size_t i = 1; i < width; ++i)
      valid = valid && (bytes[end + i] & 0xc0) == 0x80;
    if (!valid) break;
    end += width;
  }
  return std::string(reinterpret_cast<const char*>(bytes), end);
}

bool parse_number(const char*& cursor, double& result) {
  char* end = nullptr;
  result = std::strtod(cursor, &end);
  if (end == cursor || !std::isfinite(result)) return false;
  cursor = end;
  return true;
}

std::uint64_t now_ms() {
  return static_cast<std::uint64_t>(esp_timer_get_time()) / 1000U;
}
}  // namespace

void AppleMediaClient::set_callback(StateCallback callback, void* context) {
  callback_ = callback;
  callback_context_ = context;
}

void AppleMediaClient::connect(std::uint16_t connection) {
  connection_.store(connection);
  active_.store(false);
  command_handle_.store(0);
  service_start_ = service_end_ = update_handle_ = update_end_ = update_cccd_ =
      0;
  setup_stage_ = 0;
  state_ = {};
  volume_percent_ = -1;
  playback_position_known_ = false;
  playback_position_s_ = playback_rate_ = 0;
  state_.source_name = "iPhone";
  const int rc =
      ble_gattc_disc_svc_by_uuid(connection, &kService.u, service_cb, this);
  if (rc) fail("service discovery", rc);
}

void AppleMediaClient::disconnect() {
  connection_.store(0xffff);
  command_handle_.store(0);
  const bool was_active = active_.exchange(false);
  if (was_active && callback_) callback_(false, state_, -1, callback_context_);
}

void AppleMediaClient::fail(const char* stage, int error) {
  ESP_LOGW(kTag, "%s failed: %d; using app media", stage, error);
  const bool was_active = active_.exchange(false);
  if (was_active && callback_) callback_(false, state_, -1, callback_context_);
}

int AppleMediaClient::service_cb(std::uint16_t conn,
                                 const ble_gatt_error* error,
                                 const ble_gatt_svc* svc, void* arg) {
  auto* self = static_cast<AppleMediaClient*>(arg);
  if (conn != self->connection_.load()) return 0;
  if (error->status == 0 && svc) {
    self->service_start_ = svc->start_handle;
    self->service_end_ = svc->end_handle;
  } else if (error->status == BLE_HS_EDONE && self->service_start_) {
    const int rc =
        ble_gattc_disc_all_chrs(conn, self->service_start_, self->service_end_,
                                characteristic_cb, self);
    if (rc) self->fail("characteristic discovery", rc);
  } else if (error->status)
    self->fail("service discovery", error->status);
  return 0;
}

int AppleMediaClient::characteristic_cb(std::uint16_t conn,
                                        const ble_gatt_error* error,
                                        const ble_gatt_chr* chr, void* arg) {
  auto* self = static_cast<AppleMediaClient*>(arg);
  if (conn != self->connection_.load()) return 0;
  if (error->status == 0 && chr) {
    if (self->update_handle_ && !self->update_end_ &&
        chr->def_handle > self->update_handle_)
      self->update_end_ = chr->def_handle - 1;
    if (ble_uuid_cmp(&chr->uuid.u, &kUpdate.u) == 0)
      self->update_handle_ = chr->val_handle;
    if (ble_uuid_cmp(&chr->uuid.u, &kCommand.u) == 0)
      self->command_handle_.store(chr->val_handle);
  } else if (error->status == BLE_HS_EDONE && self->update_handle_ &&
             self->command_handle_.load()) {
    if (!self->update_end_) self->update_end_ = self->service_end_;
    const int rc = ble_gattc_disc_all_dscs(
        conn, self->update_handle_, self->update_end_, descriptor_cb, self);
    if (rc) self->fail("descriptor discovery", rc);
  } else if (error->status)
    self->fail("characteristic discovery", error->status);
  return 0;
}

int AppleMediaClient::descriptor_cb(std::uint16_t conn,
                                    const ble_gatt_error* error, std::uint16_t,
                                    const ble_gatt_dsc* dsc, void* arg) {
  auto* self = static_cast<AppleMediaClient*>(arg);
  if (conn != self->connection_.load()) return 0;
  if (error->status == 0 && dsc && ble_uuid_cmp(&dsc->uuid.u, &kCccd.u) == 0)
    self->update_cccd_ = dsc->handle;
  else if (error->status == BLE_HS_EDONE && self->update_cccd_) {
    const std::uint8_t enable[] = {1, 0};
    self->setup_stage_ = 1;
    const int rc = ble_gattc_write_flat(conn, self->update_cccd_, enable,
                                        sizeof(enable), setup_write_cb, self);
    if (rc) self->fail("notification subscription", rc);
  } else if (error->status)
    self->fail("descriptor discovery", error->status);
  return 0;
}

int AppleMediaClient::setup_write_cb(std::uint16_t conn,
                                     const ble_gatt_error* error,
                                     ble_gatt_attr*, void* arg) {
  static_cast<AppleMediaClient*>(arg)->setup_write_done(conn, error->status);
  return 0;
}

void AppleMediaClient::setup_write_done(std::uint16_t conn, int status) {
  if (conn != connection_.load()) return;
  if (status) {
    fail("subscription setup", status);
    return;
  }
  const std::uint8_t player[] = {0, 0, 1, 2};
  const std::uint8_t track[] = {2, 0, 2, 3};
  int rc = 0;
  if (setup_stage_ == 1) {
    setup_stage_ = 2;
    rc = ble_gattc_write_flat(conn, update_handle_, player, sizeof(player),
                              setup_write_cb, this);
  } else if (setup_stage_ == 2) {
    setup_stage_ = 3;
    rc = ble_gattc_write_flat(conn, update_handle_, track, sizeof(track),
                              setup_write_cb, this);
  } else if (setup_stage_ == 3) {
    state_.flags |= moto::ble::MediaConnected;
    active_.store(true);
    ESP_LOGI(kTag, "media service ready");
    publish();
  }
  if (rc) fail("subscription request", rc);
}

void AppleMediaClient::notification(std::uint16_t conn, std::uint16_t attribute,
                                    const os_mbuf* data) {
  if (conn != connection_.load() || attribute != update_handle_ || !data)
    return;
  std::uint8_t bytes[185]{};
  const auto length =
      std::min<std::size_t>(OS_MBUF_PKTLEN(data), sizeof(bytes));
  if (length < 3 || os_mbuf_copydata(data, 0, length, bytes) != 0) return;
  const auto entity = bytes[0], property = bytes[1];
  const std::uint8_t* value = bytes + 3;
  const auto value_len = length - 3;
  if (entity == 0 && property == 0)
    state_.source_name = clipped_text(value, value_len, 31);
  else if (entity == 0 && property == 1) {
    // AMS player playback info is "state,rate,elapsed seconds".
    // Never extrapolate a malformed or truncated update.
    if (bytes[2] != 0) return;
    const std::string text(reinterpret_cast<const char*>(value), value_len);
    const char* cursor = text.c_str();
    double playback_state = 0, rate = 0, elapsed = 0;
    if (!parse_number(cursor, playback_state) || *cursor++ != ',' ||
        !parse_number(cursor, rate) || *cursor++ != ',' ||
        !parse_number(cursor, elapsed) || *cursor != '\0' ||
        elapsed < 0 || playback_state < 0 || playback_state > 3 ||
        rate < -16 || rate > 16) return;
    if (playback_state != 0)
      state_.flags |= moto::ble::MediaPlaying;
    else
      state_.flags &= ~moto::ble::MediaPlaying;
    playback_position_s_ = elapsed;
    playback_rate_ = playback_state != 0 ? rate : 0;
    playback_updated_ms_ = now_ms();
    playback_position_known_ = true;
    ESP_LOGI(kTag, "playback update: state %.0f, elapsed %.1f", playback_state, elapsed);
  } else if (entity == 0 && property == 2) {
    if (bytes[2] != 0) return;
    const std::string text(reinterpret_cast<const char*>(value), value_len);
    const char* cursor = text.c_str();
    double volume = 0;
    if (!parse_number(cursor, volume) || *cursor != '\0' || volume < 0 || volume > 1) return;
    volume_percent_ = static_cast<int>(std::lround(volume * 100));
  } else if (entity == 2 && property == 0)
    state_.artist_name = clipped_text(value, value_len, 95);
  else if (entity == 2 && property == 2)
    state_.track_title = clipped_text(value, value_len, 127);
  else if (entity == 2 && property == 3) {
    if (bytes[2] != 0) return;
    const std::string text(reinterpret_cast<const char*>(value), value_len);
    const char* cursor = text.c_str();
    double duration = 0;
    if (!parse_number(cursor, duration) || *cursor != '\0' || duration < 0) return;
    state_.duration_s = static_cast<std::uint16_t>(std::min(65535.0, std::ceil(duration)));
    ESP_LOGI(kTag, "track duration: %u s", state_.duration_s);
  }
  else
    return;
  if (active_.load()) publish();
}

void AppleMediaClient::publish() {
  if (!callback_) return;
  auto current = state_;
  if (playback_position_known_) {
    const double seconds = playback_position_s_ + playback_rate_ *
        static_cast<double>(now_ms() - playback_updated_ms_) / 1000.0;
    current.position_s = static_cast<std::uint16_t>(
        std::clamp(seconds, 0.0, 65535.0));
  }
  callback_(true, current, volume_percent_, callback_context_);
}

bool AppleMediaClient::send(moto::ble::DeviceCommandKind kind) {
  if (!active_.load()) return false;
  std::uint8_t cmd;
  switch (kind) {
    case moto::ble::DeviceCommandKind::MusicPrevious:
      cmd = 4;
      break;
    case moto::ble::DeviceCommandKind::MusicTogglePlayback:
      cmd = 2;
      break;
    case moto::ble::DeviceCommandKind::MusicNext:
      cmd = 3;
      break;
    default:
      return false;
  }
  const int rc = ble_gattc_write_flat(
      connection_.load(), command_handle_.load(), &cmd, 1, nullptr, nullptr);
  if (rc) ESP_LOGW(kTag, "media command failed: %d", rc);
  return true;  // Do not also send the command to the system Music app.
}

bool AppleMediaClient::change_volume(bool increase) {
  if (!active_.load()) return false;
  // Apple Media Service RemoteCommandIDVolumeUp / RemoteCommandIDVolumeDown.
  const std::uint8_t command = increase ? 5 : 6;
  const int rc = ble_gattc_write_flat(
      connection_.load(), command_handle_.load(), &command, 1, nullptr, nullptr);
  if (rc) ESP_LOGW(kTag, "volume command failed: %d", rc);
  return rc == 0;
}
