#include <climits>
#include <cstddef>
#include <cstdint>

#include "board_port.h"
#include "bsp/smartring_plus.h"
#include "bsp/board_config.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"

namespace {
constexpr char kTag[] = "board_smartring";
#if !defined(MOTO_BOARD_SMARTRING_PLUS_B)
#error "This board port targets SmartRing-Plus B"
#endif
// B profile: catch incompatible vendor BSP pin changes at build time.
static_assert(BOARD_LCD_H_RES == 360 && BOARD_LCD_V_RES == 360);
static_assert(BOARD_I2C_SDA_IO == GPIO_NUM_8 && BOARD_I2C_SCL_IO == GPIO_NUM_9);
static_assert(BOARD_SD_CLK_IO == GPIO_NUM_4 && BOARD_SD_CMD_IO == GPIO_NUM_5);
static_assert(BOARD_SD_D0_IO == GPIO_NUM_3 && BOARD_SD_D1_IO == GPIO_NUM_2 &&
              BOARD_SD_D2_IO == GPIO_NUM_7 && BOARD_SD_D3_IO == GPIO_NUM_6);
constexpr std::size_t kExtraPoolBytes = 2U * 1024U * 1024U;
void* extra_pool = nullptr;
bool rotation_attempted = false;

// The BSP does not expose its panel handle. The adapter passes it to this
// callback before the first bitmap transfer, so apply the hardware rotation
// once and keep the adapter's normal asynchronous flush path.
esp_err_t draw_rotated_bitmap(lv_display_t*, esp_lcd_panel_handle_t panel,
                              int x_start, int y_start, int x_end, int y_end,
                              const void* pixels, void*) {
  if (!rotation_attempted) {
    rotation_attempted = true;
    esp_lcd_touch_handle_t touch = bsp_touch_get_handle();
    esp_err_t result = touch ? esp_lcd_panel_mirror(panel, true, true)
                             : ESP_ERR_INVALID_STATE;
    if (result == ESP_OK) result = esp_lcd_touch_set_mirror_x(touch, true);
    if (result == ESP_OK) result = esp_lcd_touch_set_mirror_y(touch, true);
    if (result == ESP_OK) {
      ESP_LOGI(kTag, "display and touch rotated 180 degrees");
    } else {
      // Keep the original orientation usable if either device cannot rotate.
      if (touch) {
        (void)esp_lcd_touch_set_mirror_x(touch, false);
        (void)esp_lcd_touch_set_mirror_y(touch, false);
      }
      (void)esp_lcd_panel_mirror(panel, false, false);
      ESP_LOGE(kTag, "display rotation failed: %s", esp_err_to_name(result));
    }
  }
  return esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end,
                                   pixels);
}
}  // namespace

extern "C" esp_err_t board_port_init(void) {
  ESP_LOGI(kTag, "development target: SmartRing-Plus B (not hardware auto-detection)");
  // GPIO47 is the SmartRing shutdown output. Drive it inactive before the
  // display starts; an undriven pin must not request a hardware shutdown.
  const esp_err_t power_result = bsp_power_init();
  if (power_result != ESP_OK) return power_result;
  // The vendor BSP selects the B-panel initialization and registers CST816S
  // touch with LVGL. Its V1/V2 power support is independent of the display.
  const esp_err_t result = bsp_display_init();
  if (result != ESP_OK) return result;
  bsp_display_backlight_set(0);

  if (!board_port_lock(UINT32_MAX)) return ESP_ERR_TIMEOUT;
  esp_lv_adapter_draw_bitmap_callbacks_t draw_callbacks{};
  draw_callbacks.custom_draw_bitmap = draw_rotated_bitmap;
  const esp_err_t draw_result = esp_lv_adapter_set_draw_bitmap_callbacks(
      bsp_display_get_handle(), &draw_callbacks, nullptr);
  if (draw_result != ESP_OK) {
    board_port_unlock();
    ESP_LOGE(kTag, "could not install display rotation: %s",
             esp_err_to_name(draw_result));
    return draw_result;
  }
  extra_pool =
      heap_caps_malloc(kExtraPoolBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (extra_pool == nullptr ||
      lv_mem_add_pool(extra_pool, kExtraPoolBytes) == nullptr) {
    board_port_unlock();
    ESP_LOGE(kTag, "could not add the LVGL PSRAM pool");
    return ESP_ERR_NO_MEM;
  }
  board_port_unlock();
  return ESP_OK;
}

extern "C" lv_display_t* board_port_get_display(void) {
  return bsp_display_get_handle();
}

extern "C" esp_err_t board_port_reveal_display(void) {
  bsp_display_backlight_set(100);
  return ESP_OK;
}

extern "C" bool board_port_lock(std::uint32_t timeout_ms) {
  const int32_t wait =
      timeout_ms == UINT32_MAX
          ? -1
          : static_cast<int32_t>(timeout_ms > INT32_MAX ? INT32_MAX
                                                        : timeout_ms);
  return esp_lv_adapter_lock(wait) == ESP_OK;
}

extern "C" void board_port_unlock(void) { esp_lv_adapter_unlock(); }

// SmartRing's power circuitry and button are not the Waveshare GPIO3 key.
// The Waveshare long-press task is disabled until this board is validated.
extern "C" bool board_port_power_button_pressed(void) { return false; }
extern "C" esp_err_t board_port_power_off(void) {
  return ESP_ERR_NOT_SUPPORTED;
}
