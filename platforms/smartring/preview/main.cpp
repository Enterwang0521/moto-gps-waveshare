#include <algorithm>
#include <cstdlib>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <emscripten.h>
#include "lvgl.h"
#include "moto_nav_ui.h"

namespace {
 moto_ui_state_t sample{};
void frame(void*) { lv_timer_handler(); }
void page_changed(moto_ui_page_t page, void*) { moto_nav_ui_set_page(page); }
void music_changed(moto_music_command_t, void*) {}
}

extern "C" EMSCRIPTEN_KEEPALIVE void preview_page(int page) {
    if(page == 0) { moto_nav_ui_set_phone_connection(MOTO_UI_PHONE_ONLINE); moto_nav_ui_set_state(&sample); }
    moto_nav_ui_set_page(static_cast<moto_ui_page_t>(std::clamp(page, 0, 3)));
}
extern "C" EMSCRIPTEN_KEEPALIVE void preview_ready() {
    moto_ui_state_t ready{};
    ready.mode = MOTO_UI_ACQUIRING_FIX;
    moto_nav_ui_set_phone_connection(MOTO_UI_PHONE_ONLINE);
    moto_nav_ui_set_state(&ready);
    moto_nav_ui_set_page(MOTO_UI_PAGE_NAVIGATION);
}
extern "C" EMSCRIPTEN_KEEPALIVE void preview_connecting() {
    moto_nav_ui_set_phone_connection(MOTO_UI_PHONE_CONNECTING);
    moto_nav_ui_set_page(MOTO_UI_PAGE_NAVIGATION);
}
extern "C" EMSCRIPTEN_KEEPALIVE void preview_track(int mode) {
    const char *titles[] = {"NIGHT RIDE", "夜空中最亮的星", "A VERY LONG TRACK NAME FOR SCROLLING"};
    const char *artists[] = {"THE MIDNIGHT", "逃跑计划", "EXAMPLE ARTIST"};
    const int index = std::clamp(mode, 0, 2);
    const moto_music_state_t music = {"PHONE MUSIC", titles[index], artists[index], 1, 1, 0, 0, 64, 180};
    moto_nav_ui_set_music_state(&music);
}
int main() {
    lv_init();
    emscripten_set_main_loop_arg(frame, nullptr, 0, false);
    lv_sdl_window_create(360, 360);
    lv_sdl_mouse_create();
    moto_nav_ui_create();
    moto_nav_ui_set_page_change_callback(page_changed, nullptr);
    moto_nav_ui_set_music_command_callback(music_changed, nullptr);
    moto_nav_ui_set_music_page_enabled(1);
    moto_nav_ui_set_music_volume(1, 55);
    sample.page = MOTO_UI_PAGE_NAVIGATION;
    sample.mode = MOTO_UI_NAVIGATING;
    sample.maneuver = MOTO_MANEUVER_RIGHT;
    sample.distance_to_maneuver_m = 280;
    sample.remaining_distance_m = 4800;
    sample.remaining_time_s = 600;
    sample.route_progress_percent = 35;
    sample.gps_accuracy_m = 5;
    sample.online = 1;
    sample.has_destination = 1;
    sample.speed_kph = 48;
    sample.heading_deg = 86;
    sample.route_point_count = 3;
    sample.route_points[0] = {180, 310};
    sample.route_points[1] = {180, 180};
    sample.route_points[2] = {270, 140};
    moto_nav_ui_set_state(&sample);
    moto_nav_ui_set_phone_connection(MOTO_UI_PHONE_ONLINE);
    preview_track(0);
    moto_nav_ui_set_page(MOTO_UI_PAGE_MUSIC);
    emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
    return EXIT_SUCCESS;
}
