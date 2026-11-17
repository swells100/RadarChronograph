#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H

#include <lvgl.h>

void create_main_screen(lv_obj_t* main_screen,lv_obj_t* settings_screen);

void main_screen_update_values(int shot_number, int velocity);

#endif