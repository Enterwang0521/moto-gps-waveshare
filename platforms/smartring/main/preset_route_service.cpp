#include "preset_route_service.h"
#include "bsp/smartring_plus.h"
#include "esp_log.h"
#include "sdmmc_cmd.h"
#include <cerrno>
void PresetRouteService::start(void (*changed)(void *), void *context) {
  if (task_)
    return;
  changed_ = changed;
  context_ = context;
  if (xTaskCreate(task, "moto_tf", 6144, this, 1, &task_) != pdPASS)
    ESP_LOGE("moto_tf", "could not start TF monitor");
}
void PresetRouteService::task(void *context) {
  auto *self = static_cast<PresetRouteService *>(context);
  while (true) {
    self->poll();
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}
void PresetRouteService::poll() {
  bool changed = false;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    const bool was_ready = store_.available();
    // No card-detect GPIO exists on this BSP. CMD13 checks actual card
    // response.
    if (bsp_sd_is_mounted()) {
      auto *card = const_cast<sdmmc_card_t *>(bsp_sd_get_card());
      if (!card || sdmmc_get_status(card) != ESP_OK) {
        store_.detach();
        bsp_sd_deinit();
        changed = true;
      }
    } else if (bsp_sd_init() == ESP_OK) {
      // Scan files only after a successful FAT mount. Never format
      // automatically.
      store_.initialize();
      changed = true;
    }
    if (bsp_sd_is_mounted() && !store_.available())
      store_.initialize();
    changed = changed || was_ready != store_.available();
  }
  (void)changed;
  if (changed_)
    changed_(context_);
}
bool PresetRouteService::consume(const moto::ble::PresetRoute &packet) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!bsp_sd_is_mounted())
    return false;
  auto *card = const_cast<sdmmc_card_t *>(bsp_sd_get_card());
  if (!card || sdmmc_get_status(card) != ESP_OK)
    return false;
  errno = 0;
  const bool ok = store_.consume(packet);
  if (!ok) {
    ESP_LOGW("moto_tf",
             "preset write rejected op=%u slot=%u offset=%lu/%lu errno=%d",
             packet.operation, packet.slot,
             static_cast<unsigned long>(packet.offset),
             static_cast<unsigned long>(packet.total_size), errno);
  } else if (packet.operation == 1 || packet.operation == 3) {
    ESP_LOGI("moto_tf", "preset %s slot=%u bytes=%lu",
             packet.operation == 1 ? "begin" : "committed", packet.slot,
             static_cast<unsigned long>(packet.total_size));
  }
  return ok;
}
std::array<moto::presets::Entry, 4> PresetRouteService::entries() {
  const std::lock_guard<std::mutex> lock(mutex_);
  return store_.entries();
}
bool PresetRouteService::available() {
  const std::lock_guard<std::mutex> lock(mutex_);
  return store_.available();
}
