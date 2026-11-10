cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(moto_nav_native_tests LANGUAGES CXX)
endif()

enable_testing()

if(NOT TARGET moto_nav_core)
  add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../../shared/nav_core"
    "${CMAKE_CURRENT_BINARY_DIR}/nav_core"
  )
endif()

if(NOT TARGET moto_coordinates)
  add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../../shared/coordinates"
    "${CMAKE_CURRENT_BINARY_DIR}/coordinates"
  )
endif()

if(NOT TARGET moto_nav_app)
  add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../../shared/nav_app"
    "${CMAKE_CURRENT_BINARY_DIR}/nav_app"
  )
endif()

set(MOTO_USING_NAV_UI_TEST_STUB OFF)
if(NOT TARGET moto_nav_ui)
  # Mapping tests do not need an LVGL display. This ABI-compatible stub keeps
  # the standalone native test build small and lets apply_to_lvgl() be probed.
  add_library(moto_nav_ui STATIC moto_nav_ui_stub.cpp)
  target_include_directories(
    moto_nav_ui PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/../../shared/nav_ui/include"
  )
  target_compile_features(moto_nav_ui PUBLIC cxx_std_17)
  set(MOTO_USING_NAV_UI_TEST_STUB ON)
endif()

if(NOT TARGET moto_nav_presenter)
  add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../../shared/nav_presenter"
    "${CMAKE_CURRENT_BINARY_DIR}/nav_presenter"
  )
endif()

if(NOT TARGET moto_ble_protocol)
  add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../../shared/ble_protocol"
    "${CMAKE_CURRENT_BINARY_DIR}/ble_protocol"
  )
endif()

add_executable(nav_core_tests nav_core_tests.cpp)
target_link_libraries(nav_core_tests PRIVATE moto_nav_core moto_coordinates)
target_compile_features(nav_core_tests PRIVATE cxx_std_17)

# The full LVGL target exists in the project build, but not when this test
# directory is configured standalone with its lightweight UI stub.
if(TARGET lvgl)
  set(MOTO_NATIVE_LVGL_HEAP_BYTES "8388608" CACHE STRING
      "LVGL internal heap for native render tests; use 65536 for board parity")
  target_compile_definitions(lvgl PRIVATE
    CONFIG_LV_MEM_SIZE=${MOTO_NATIVE_LVGL_HEAP_BYTES})
  add_executable(nav_ui_render_tests nav_ui_render_tests.cpp)
  target_link_libraries(nav_ui_render_tests PRIVATE moto_nav_ui lvgl)
  target_compile_features(nav_ui_render_tests PRIVATE cxx_std_17)
endif()

add_executable(nav_core_fixture_demo nav_core_fixture_demo.cpp)
target_link_libraries(nav_core_fixture_demo PRIVATE moto_nav_core moto_coordinates)
target_compile_features(nav_core_fixture_demo PRIVATE cxx_std_17)

add_executable(nav_app_tests nav_app_tests.cpp)
target_link_libraries(nav_app_tests PRIVATE moto_nav_app)
target_compile_features(nav_app_tests PRIVATE cxx_std_17)

add_executable(nav_presenter_tests nav_presenter_tests.cpp)
target_link_libraries(nav_presenter_tests PRIVATE moto_nav_presenter)
target_compile_features(nav_presenter_tests PRIVATE cxx_std_17)
if(MOTO_USING_NAV_UI_TEST_STUB)
  target_compile_definitions(nav_presenter_tests PRIVATE
    MOTO_NAV_UI_TEST_STUB=1
  )
endif()

add_executable(ble_protocol_tests ble_protocol_tests.cpp)
target_link_libraries(ble_protocol_tests PRIVATE moto_ble_protocol)
target_compile_features(ble_protocol_tests PRIVATE cxx_std_17)
target_compile_definitions(ble_protocol_tests PRIVATE
  "MOTO_BLE_GOLDEN_FIXTURE=\"${CMAKE_CURRENT_LIST_DIR}/../../shared/protocol/fixtures/ble-navigation-v1.golden.txt\""
)

add_executable(connection_epoch_gate_tests connection_epoch_gate_tests.cpp)
target_include_directories(connection_epoch_gate_tests PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp32/main"
)
target_compile_features(connection_epoch_gate_tests PRIVATE cxx_std_17)

add_executable(motion_heading_fusion_tests motion_heading_fusion_tests.cpp)
target_include_directories(motion_heading_fusion_tests PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp32/main"
)
target_compile_features(motion_heading_fusion_tests PRIVATE cxx_std_17)

find_package(Threads REQUIRED)
add_executable(phone_nav_bridge_tests
  phone_nav_bridge_tests.cpp
  esp32_host_stubs/phone_nav_bridge_platform_stubs.cpp
  "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp32/main/phone_nav_bridge.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../../shared/nav_presenter/src/moto_nav_presenter.cpp"
)
target_include_directories(phone_nav_bridge_tests PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/esp32_host_stubs"
  "${CMAKE_CURRENT_LIST_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp32/main"
  "${CMAKE_CURRENT_LIST_DIR}/../../shared/nav_presenter/include"
  "${CMAKE_CURRENT_LIST_DIR}/../../shared/nav_ui/include"
)
target_link_libraries(phone_nav_bridge_tests PRIVATE
  moto_ble_protocol
  moto_nav_core
  Threads::Threads
)
target_compile_features(phone_nav_bridge_tests PRIVATE cxx_std_17)

add_test(NAME nav_core_tests COMMAND nav_core_tests)
if(TARGET nav_ui_render_tests)
  add_test(NAME nav_ui_render_tests COMMAND nav_ui_render_tests)
endif()
add_test(NAME nav_app_tests COMMAND nav_app_tests)
add_test(NAME nav_presenter_tests COMMAND nav_presenter_tests)
add_test(NAME ble_protocol_tests COMMAND ble_protocol_tests)
add_test(NAME connection_epoch_gate_tests COMMAND connection_epoch_gate_tests)
add_test(NAME motion_heading_fusion_tests COMMAND motion_heading_fusion_tests)
add_test(NAME phone_nav_bridge_tests COMMAND phone_nav_bridge_tests)

add_executable(preset_route_store_tests
  preset_route_store_tests.cpp
  ../../shared/preset_routes/preset_route_store.cpp)
target_include_directories(preset_route_store_tests PRIVATE ../../shared ../../shared/ble_protocol/include)
target_link_libraries(preset_route_store_tests PRIVATE moto_ble_protocol)
target_compile_features(preset_route_store_tests PRIVATE cxx_std_17)
# Assertions are the checks in this focused persistence executable, including Release CI.
target_compile_options(preset_route_store_tests PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-UNDEBUG>)
add_test(NAME preset_route_store_tests COMMAND preset_route_store_tests)

if(TARGET lvgl)
# Compile the actual B UI, including board-only page availability, rather than
# relying on the generic 466px UI tests to cover the 360px SmartRing variant.
add_executable(smartring_presets_page_tests
  smartring_presets_page_tests.cpp
  ../../shared/nav_ui/src/moto_nav_ui.cpp
  ../../shared/nav_ui/assets/moto_font_nav_16.c
  ../../platforms/smartring/assets/moto_music_font_16_a.c
  ../../platforms/smartring/assets/moto_music_font_16_b.c
  ../../platforms/smartring/assets/moto_pixel_16.c
  ../../platforms/smartring/assets/moto_pixel_24.c
  ../../platforms/smartring/assets/moto_pixel_48.c)
target_include_directories(smartring_presets_page_tests PRIVATE ../../shared/nav_ui/include)
target_compile_definitions(smartring_presets_page_tests PRIVATE
  MOTO_BOARD_SMARTRING_PLUS=1 MOTO_UI_CANVAS_WIDTH=360 MOTO_UI_CANVAS_HEIGHT=360)
target_link_libraries(smartring_presets_page_tests PRIVATE lvgl)
target_compile_features(smartring_presets_page_tests PRIVATE cxx_std_17)
add_test(NAME smartring_presets_page_tests COMMAND smartring_presets_page_tests)
endif()
