#ifndef DISPLAY_SCREEN_H
#define DISPLAY_SCREEN_H

#include <lvgl.h>

void create_display_screen(lv_obj_t* display_screen,lv_obj_t* settings_screen);

void display_screen_update_layout();

#endif