
#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>
#include <lvgl.h>

// Hardware-readable variables
extern int ARM;
extern int Portrait;
extern int ShotTime;
extern int ScreenBrightness;

// Shared button functions
struct ToggleButtonData
{
    int value;
    const char *text_off;
    const char *text_on;
    lv_color_t color_off;
    lv_color_t color_on;
    int* linked_value;
};

void create_page_header(
    lv_obj_t* parent,
    const char* back_text,
    const char* title_text,
    lv_obj_t* destination_screen
);

void toggle_button_cb(lv_event_t *event);
void update_toggle_button(lv_obj_t *button, ToggleButtonData *data);

void buttons_update_layout();
void create_buttons();

#endif
