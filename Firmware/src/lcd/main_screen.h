#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H

#include <lvgl.h>
#include <stdint.h>

void create_main_screen(lv_obj_t* main_screen,lv_obj_t* settings_screen);

// Show the result of a shot (speed in metres per second; also shown as mph and ft/s).
void main_screen_update_values(int shot_number, float speed_mps);

// Show the real arm state of the radar (button colour, label and countdown). Cheap to call every loop.
void main_screen_set_armed(bool armed, uint32_t ms_left);

// True once each time the user pressed the ARM button (the firmware decides what that does).
bool main_screen_take_arm_toggle();

#endif
