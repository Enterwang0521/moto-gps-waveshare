#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "preset_routes/preset_route_store.hpp"
// Owns all TF I/O, including status probing, so unmount cannot race an upload.
class PresetRouteService {
public:
  void start(void (*changed)(void *), void *context);
  bool consume(const moto::ble::PresetRoute &packet);
  std::array<moto::presets::Entry, 4> entries();
  bool available();

private:
  static void task(void *context);
  void poll();
  std::mutex mutex_;
  moto::presets::Store store_{"/sdcard/routes"};
  TaskHandle_t task_ = nullptr;
  void (*changed_)(void *) = nullptr;
  void *context_ = nullptr;
};
