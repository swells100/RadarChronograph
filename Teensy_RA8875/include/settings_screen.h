#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H

#include <lvgl.h>

void create_settings_screen(
    lv_obj_t* settings_screen,
    lv_obj_t* main_screen,
    lv_obj_t* display_screen
);

void settings_screen_update_layout();

#endif