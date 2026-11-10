#include "moto_nav_ui.h"
#include "lvgl.h"
#include <cstdio>
#include <cstring>
#include <vector>
static bool contains_title(lv_obj_t* object) {
  if(lv_obj_check_type(object,&lv_label_class) &&
     std::strcmp(lv_label_get_text(object),"TF ROUTES")==0) return true;
  for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)
    if(contains_title(lv_obj_get_child(object,i)))return true;
  return false;
}
static bool check_slot_fonts(lv_obj_t* object, int& count) {
  if(lv_obj_check_type(object, &lv_label_class) &&
     std::strcmp(lv_label_get_text(object), "EMPTY") == 0) {
    ++count;
    const auto* font = lv_obj_get_style_text_font(object, LV_PART_MAIN);
    const uint32_t characters[] = {'E', 'M', 'P', 'T', 'Y', '1', '4', ' ', '-',
                                   0x901a, 0x52e4, 0x5468, 0x672b, 0x8def, 0x7ebf};
    for(auto character : characters) {
      lv_font_glyph_dsc_t glyph{};
      if(!lv_font_get_glyph_dsc(font, &glyph, character, 0) || glyph.is_placeholder) {
        std::fprintf(stderr, "Route slot missing glyph U+%04X\n", unsigned(character));
        return false;
      }
    }
  }
  for(uint32_t i=0; i<lv_obj_get_child_count(object); ++i)
    if(!check_slot_fonts(lv_obj_get_child(object, i), count)) return false;
  return true;
}
int main() {
  lv_init();
  static std::vector<uint32_t> pool(2*1024*1024/4);
  if(!lv_mem_add_pool(pool.data(),pool.size()*4))return 1;
  auto* display=lv_display_create(360,360);
  static uint16_t buffer[360*40];
  lv_display_set_buffers(display,buffer,nullptr,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});
  for(int cycle=0;cycle<3;++cycle) {
    moto_nav_ui_create(); // Includes the memset that previously cleared the B-only page flag.
    moto_nav_ui_set_music_page_enabled(0);
    moto_nav_ui_set_shortcuts_page_enabled(0);
    moto_nav_ui_set_page(MOTO_UI_PAGE_PRESETS);
    if(moto_nav_ui_get_page()!=MOTO_UI_PAGE_PRESETS ||
       !contains_title(lv_obj_get_child(lv_screen_active(),MOTO_UI_PAGE_PRESETS))) {
      std::fprintf(stderr,"TF ROUTES unavailable after UI reset, cycle=%d\n",cycle);return 1;
    }
    int slot_count = 0;
    if(!check_slot_fonts(lv_obj_get_child(lv_screen_active(), MOTO_UI_PAGE_PRESETS), slot_count) ||
       slot_count != 4) return 1;
    moto_nav_ui_set_page(MOTO_UI_PAGE_NAVIGATION);
    moto_nav_ui_set_page(MOTO_UI_PAGE_PRESETS);
    if(moto_nav_ui_get_page()!=MOTO_UI_PAGE_PRESETS)return 1;
    moto_nav_ui_show_boot_screen();
  }
  std::puts("SmartRing TF ROUTES survives UI initialization and recreation");
}
