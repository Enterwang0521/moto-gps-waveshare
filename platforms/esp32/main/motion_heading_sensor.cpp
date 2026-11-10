#include "motion_heading_sensor.h"
#include "motion_heading_axis.hpp"

#include <algorithm>
#include <cmath>

#if defined(MOTO_BOARD_SMARTRING_PLUS)
#include "bsp/smartring_plus.h"
#else
#include "bsp/esp-bsp.h"
#endif
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// qmi8658.h defines M_PI globally. This adapter does not use that macro and
// removes it after the official component header to keep C++ <cmath> clean.
#include "qmi8658.h"
#ifdef M_PI
#undef M_PI
#endif

namespace {
constexpr char kTag[] = "motion_heading";
constexpr TickType_t kSampleDelay = pdMS_TO_TICKS(8);  // 125 Hz target.
constexpr int kCalibrationSamples = 120;
constexpr std::uint64_t kCalibrationDeadlineMs = 4'000;
constexpr float kMinimumGravity = 6.8F;
constexpr float kMaximumGravity = 12.8F;

struct GravityEstimate {
  float x = 0.0F;
  float y = 0.0F;
  float z = 1.0F;
  bool initialized = false;
};

std::uint64_t monotonic_ms() {
  return static_cast<std::uint64_t>(esp_timer_get_time()) / 1'000U;
}

float magnitude(float x, float y, float z) {
  return std::sqrt(x * x + y * y + z * z);
}

bool update_gravity(GravityEstimate& gravity, const qmi8658_data_t& sample,
                    float blend) {
  const float length = magnitude(sample.accelX, sample.accelY, sample.accelZ);
  if (!std::isfinite(length) || length < kMinimumGravity ||
      length > kMaximumGravity) {
    return gravity.initialized;
  }
  if (!gravity.initialized) {
    gravity.x = sample.accelX;
    gravity.y = sample.accelY;
    gravity.z = sample.accelZ;
    gravity.initialized = true;
  } else {
    gravity.x += (sample.accelX - gravity.x) * blend;
    gravity.y += (sample.accelY - gravity.y) * blend;
    gravity.z += (sample.accelZ - gravity.z) * blend;
  }
  return true;
}

// Project angular velocity onto the board's gravity axis so a moderately
// tilted or forward-facing mount still responds to a turn around vertical.
float clockwise_heading_rate(const GravityEstimate& gravity,
                             const qmi8658_data_t& sample) {
  if (!gravity.initialized) return 0.0F;
  return clockwise_gravity_heading_rate(
      gravity.x, gravity.y, gravity.z,
      sample.gyroX, sample.gyroY, sample.gyroZ);
}
}  // namespace

esp_err_t MotionHeadingSensor::start(SampleCallback callback, void* context) {
  if (callback == nullptr) return ESP_ERR_INVALID_ARG;
  if (started_) return ESP_ERR_INVALID_STATE;
  callback_ = callback;
  callback_context_ = context;
  // Sensor callbacks reach the presenter and LVGL adapter. Keep enough margin
  // for those nested frames even though the large map snapshot now remains in
  // PhoneNavBridge member storage.
  if (xTaskCreate(task_entry, "moto_qmi_heading", 8'192, this, 3, nullptr) !=
      pdPASS) {
    callback_ = nullptr;
    callback_context_ = nullptr;
    return ESP_ERR_NO_MEM;
  }
  started_ = true;
  return ESP_OK;
}

void MotionHeadingSensor::task_entry(void* context) {
  static_cast<MotionHeadingSensor*>(context)->run();
  vTaskDelete(nullptr);
}

void MotionHeadingSensor::run() {
  qmi8658_dev_t device{};
  const esp_err_t init = qmi8658_init(
      &device,
#if defined(MOTO_BOARD_SMARTRING_PLUS)
      bsp_i2c_get_bus(),
#else
      bsp_i2c_get_handle(),
#endif
      QMI8658_ADDRESS_HIGH);
  if (init != ESP_OK) {
    ESP_LOGW(kTag, "QMI8658 unavailable; using phone course only: %s",
             esp_err_to_name(init));
    return;
  }
#if defined(MOTO_BOARD_SMARTRING_PLUS)
  // The SmartRing BSP requires stopping both engines before changing ODR and
  // range; leave the original Waveshare setup untouched.
  ESP_ERROR_CHECK_WITHOUT_ABORT(qmi8658_enable_sensors(&device, QMI8658_DISABLE_ALL));
#endif
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      qmi8658_set_accel_range(&device, QMI8658_ACCEL_RANGE_4G));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      qmi8658_set_accel_odr(&device, QMI8658_ACCEL_ODR_125HZ));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      qmi8658_set_gyro_range(&device, QMI8658_GYRO_RANGE_512DPS));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      qmi8658_set_gyro_odr(&device, QMI8658_GYRO_ODR_125HZ));
#if defined(MOTO_BOARD_SMARTRING_PLUS)
  // QMI8658's 512 dps range is CTRL3.gFS=5. The vendor component's enum
  // writes 4 (256 dps) while selecting the 64 LSB/dps divisor for 512 dps.
  // Correct the register so the actual range matches the component's divisor.
  const esp_err_t gyro_config = qmi8658_write_register(&device, QMI8658_CTRL3, 0x56);
  if (gyro_config != ESP_OK) {
    ESP_LOGW(kTag, "IMU range setup failed; using phone course only: %s",
             esp_err_to_name(gyro_config));
    return;
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(qmi8658_enable_sensors(
      &device, QMI8658_ENABLE_ACCEL | QMI8658_ENABLE_GYRO));
  // Give both engines time to settle before accepting calibration samples.
  vTaskDelay(pdMS_TO_TICKS(120));
#endif
  qmi8658_set_accel_unit_mps2(&device, true);
  qmi8658_set_gyro_unit_dps(&device, true);

  GravityEstimate gravity;
#if defined(MOTO_BOARD_SMARTRING_PLUS)
  float bias_sum_x = 0.0F, bias_sum_y = 0.0F, bias_sum_z = 0.0F;
#else
  float bias_sum = 0.0F;
#endif
  int stable_samples = 0;
  const std::uint64_t calibration_start = monotonic_ms();
  qmi8658_data_t data{};
#if defined(MOTO_BOARD_SMARTRING_PLUS)
  int observed_samples = 0;
#endif
  TickType_t next_sample_tick = xTaskGetTickCount();
  while (stable_samples < kCalibrationSamples &&
         monotonic_ms() - calibration_start < kCalibrationDeadlineMs) {
#if defined(MOTO_BOARD_SMARTRING_PLUS)
    // The vendor's SmartRing BSP samples the QMI8658 directly on a timer;
    // do not depend on the Waveshare driver's data-ready status bit here.
    if (qmi8658_read_sensor_data(&device, &data) == ESP_OK) {
      ++observed_samples;
#else
    bool ready = false;
    if (qmi8658_is_data_ready(&device, &ready) == ESP_OK && ready &&
        qmi8658_read_sensor_data(&device, &data) == ESP_OK) {
#endif
      update_gravity(gravity, data, 0.08F);
      const float projected = clockwise_heading_rate(gravity, data);
      const float angular = magnitude(data.gyroX, data.gyroY, data.gyroZ);
#if defined(MOTO_BOARD_SMARTRING_PLUS)
      const float accel = magnitude(data.accelX, data.accelY, data.accelZ);
      const bool stable = gravity.initialized && std::isfinite(projected) &&
                          std::isfinite(angular) && angular < 4.0F &&
                          std::isfinite(accel) && accel > 8.2F && accel < 11.4F;
#else
      const bool stable = std::isfinite(projected) && angular < 4.0F;
#endif
      if (stable) {
#if defined(MOTO_BOARD_SMARTRING_PLUS)
        bias_sum_x += data.gyroX;
        bias_sum_y += data.gyroY;
        bias_sum_z += data.gyroZ;
#else
        bias_sum += projected;
#endif
        ++stable_samples;
      } else {
#if defined(MOTO_BOARD_SMARTRING_PLUS)
        bias_sum_x = bias_sum_y = bias_sum_z = 0.0F;
#else
        bias_sum = 0.0F;
#endif
        stable_samples = 0;
      }
    }
    xTaskDelayUntil(&next_sample_tick, kSampleDelay);
  }

#if defined(MOTO_BOARD_SMARTRING_PLUS)
  ESP_LOGI(kTag, "SmartRing IMU: %d samples, accel %.2f m/s2, gyro %.2f dps",
           observed_samples,
           static_cast<double>(magnitude(data.accelX, data.accelY, data.accelZ)),
           static_cast<double>(magnitude(data.gyroX, data.gyroY, data.gyroZ)));
  if (stable_samples < kCalibrationSamples || !gravity.initialized) {
    ESP_LOGW(kTag, "IMU calibration unavailable; using phone course only");
    return;
  }
#endif
#if defined(MOTO_BOARD_SMARTRING_PLUS)
  float bias_x = bias_sum_x / stable_samples;
  float bias_y = bias_sum_y / stable_samples;
  float bias_z = bias_sum_z / stable_samples;
  const float bias_dps = clockwise_gravity_heading_rate(
      gravity.x, gravity.y, gravity.z, bias_x, bias_y, bias_z);
#else
  float bias_dps = stable_samples > 0 ? bias_sum / stable_samples : 0.0F;
#endif
  ESP_LOGI(kTag,
           "QMI8658 yaw assist ready (bias %.3f dps, %d stable samples)",
           static_cast<double>(bias_dps), stable_samples);

  int consecutive_failures = 0;
  next_sample_tick = xTaskGetTickCount();
  while (true) {
    bool ready = false;
#if defined(MOTO_BOARD_SMARTRING_PLUS)
    esp_err_t result = qmi8658_read_sensor_data(&device, &data);
    ready = result == ESP_OK;
#else
    esp_err_t result = qmi8658_is_data_ready(&device, &ready);
    if (result == ESP_OK && ready) {
      result = qmi8658_read_sensor_data(&device, &data);
    }
#endif
    if (result == ESP_OK && ready) {
      consecutive_failures = 0;
#if defined(MOTO_BOARD_SMARTRING_PLUS)
      const float angular = magnitude(data.gyroX, data.gyroY, data.gyroZ);
      if (!std::isfinite(angular) || angular >= 500.0F) {
        xTaskDelayUntil(&next_sample_tick, kSampleDelay);
        continue;
      }
#endif
      update_gravity(gravity, data, 0.025F);
#if defined(MOTO_BOARD_SMARTRING_PLUS)
      // Correct all three axes before projecting onto gravity, since a change
      // in mounting tilt changes which sensor axis contributes to yaw.
      qmi8658_data_t corrected = data;
      corrected.gyroX -= bias_x;
      corrected.gyroY -= bias_y;
      corrected.gyroZ -= bias_z;
      const float corrected_rate = clockwise_heading_rate(gravity, corrected);
      const float corrected_angular = magnitude(corrected.gyroX, corrected.gyroY,
                                                corrected.gyroZ);
#else
      const float raw_rate = clockwise_heading_rate(gravity, data);
      const float corrected_rate = raw_rate - bias_dps;
      const float angular = magnitude(data.gyroX, data.gyroY, data.gyroZ);
#endif
      const float accel = magnitude(data.accelX, data.accelY, data.accelZ);
      // Learn residual bias only during convincingly quiet intervals.
      if (std::abs(corrected_rate) < 1.1F &&
#if defined(MOTO_BOARD_SMARTRING_PLUS)
          corrected_angular < 2.0F &&
#else
          angular < 2.0F &&
#endif
          accel > 8.2F && accel < 11.4F) {
#if defined(MOTO_BOARD_SMARTRING_PLUS)
        bias_x += (data.gyroX - bias_x) * 0.0008F;
        bias_y += (data.gyroY - bias_y) * 0.0008F;
        bias_z += (data.gyroZ - bias_z) * 0.0008F;
#else
        bias_dps += (raw_rate - bias_dps) * 0.0008F;
#endif
      }
      callback_(corrected_rate, monotonic_ms(), callback_context_);
    }
    if (result != ESP_OK && ++consecutive_failures == 25) {
      ESP_LOGW(kTag, "QMI8658 read errors continue: %s",
               esp_err_to_name(result));
    }
    xTaskDelayUntil(&next_sample_tick, kSampleDelay);
  }
}
