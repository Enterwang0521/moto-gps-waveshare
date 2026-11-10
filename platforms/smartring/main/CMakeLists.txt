set(MOTO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(MOTO_ESP32 "${MOTO_ROOT}/platforms/esp32/main")

idf_component_register(
    SRCS
        "board_port_smartring.cpp"
        "apple_media_client.cpp"
        "preset_route_service.cpp"
        "${MOTO_ROOT}/shared/preset_routes/preset_route_store.cpp"
        "../assets/moto_music_font_16_a.c"
        "../assets/moto_music_font_16_b.c"
        "../assets/moto_pixel_16.c"
        "../assets/moto_pixel_24.c"
        "../assets/moto_pixel_48.c"
        "${MOTO_ESP32}/app_main.cpp"
        "${MOTO_ESP32}/ble_nav_transport_nimble.cpp"
        "${MOTO_ESP32}/motion_heading_sensor.cpp"
        "${MOTO_ESP32}/phone_nav_bridge.cpp"
        "${MOTO_ROOT}/shared/ble_protocol/src/ble_protocol.cpp"
        "${MOTO_ROOT}/shared/nav_presenter/src/moto_nav_presenter.cpp"
        "${MOTO_ROOT}/shared/nav_ui/assets/moto_font_nav_16.c"
        "${MOTO_ROOT}/shared/nav_ui/src/moto_nav_ui.cpp"
    INCLUDE_DIRS
        "." "${MOTO_ESP32}" "${MOTO_ROOT}/shared"
        "${MOTO_ROOT}/shared/ble_protocol/include"
        "${MOTO_ROOT}/shared/nav_presenter/include"
        "${MOTO_ROOT}/shared/nav_ui/include"
    REQUIRES
        sdmmc esp_timer esp_driver_gpio esp_driver_i2c esp_hw_support freertos log
        lvgl nvs_flash bt viewesmart__smartring_plus waveshare__qmi8658
        espressif__esp_lvgl_adapter
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)
