cmake_minimum_required(VERSION 3.20)

get_filename_component(MOTO_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(MOTO_LV_CONF_PATH "${MOTO_REPOSITORY_ROOT}/config/lv_conf.h")
set(ENV{LVGL_VERSION} "9.5.0")
if(NOT EXISTS "${MOTO_REPOSITORY_ROOT}/third_party/lvgl/CMakeLists.txt")
    message(FATAL_ERROR "Initialize the LVGL submodule first")
endif()
add_compile_definitions(
    LV_CONF_PATH="${MOTO_LV_CONF_PATH}"
    MOTO_BOARD_SMARTRING_PLUS=1
    MOTO_BOARD_SMARTRING_PLUS_B=1
    MOTO_UI_CANVAS_WIDTH=360
    MOTO_UI_CANVAS_HEIGHT=360
)
set(IDF_TARGET "esp32s3" CACHE STRING "ESP-IDF chip target")
set(PROJECT_VER "0.3.0-smartring-tf")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(moto_gps_smartring)
