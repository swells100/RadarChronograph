
#ifndef SCREEN_H
#define SCREEN_H

#include <Arduino.h>
#include <lvgl.h>

void screen_init();
void screen_toggle_orientation();
void screen_process_orientation();
void screen_set_brightness(uint8_t brightness);

#endif
