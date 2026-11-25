// Adapted from swells100/RadarChronograph, Teensy_RA8875 (LVGL touch UI on an RA8875 480x272 display).
#include "../radar_config.h"
#if RADAR_LCD

#include <Arduino.h>
#include <lvgl.h>

#include "buttons.h"
#include "screen.h"
#include "settings.h"

#include "main_screen.h"
#include "settings_screen.h"
#include "display_screen.h"

// HARDWARE-READABLE VARIABLES
int ARM = 0;
int Portrait = 0;
int ShotTime = 10;
int ScreenBrightness = 100;

// TOGGLE CALLBACK
void toggle_button_cb(lv_event_t* event)
{
    lv_obj_t* button = (lv_obj_t*)lv_event_get_target(event);
    ToggleButtonData* data = (ToggleButtonData*)lv_event_get_user_data(event);

    if (data == nullptr) return;

    data->value = !data->value;

    if (data->linked_value != nullptr) *data->linked_value = data->value;

    lv_obj_t* label = lv_obj_get_child(button, 0);
    lv_label_set_text(label,data->value ? data->text_on : data->text_off);
    lv_obj_set_style_bg_color(button,data->value ? data->color_on : data->color_off,LV_PART_MAIN);
}

// CREATE PAGE HEADER
void create_page_header(lv_obj_t* parent,const char* back_text,const char* title_text,lv_obj_t* destination_screen)
{

    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_pos(button, 0, 0);
    lv_obj_set_size(button, 100, 45);

    lv_obj_set_style_bg_color(button, lv_color_hex(0x303F50), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 0, LV_PART_MAIN);

    lv_obj_t* back_label = lv_label_create(button);
    lv_label_set_text(back_label, back_text);
    lv_obj_center(back_label);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(title,button,LV_ALIGN_OUT_RIGHT_MID,15,0);

    lv_obj_add_event_cb(button,[](lv_event_t* event){lv_screen_load((lv_obj_t*)lv_event_get_user_data(event));},LV_EVENT_CLICKED,destination_screen);
}

// UPDATE SCREEN LAYOUTS AFTER ORIENTATION CHANGE
void buttons_update_layout()
{
    settings_screen_update_layout();
    display_screen_update_layout();
}

// CREATE ALL SCREENS
void create_buttons()
{
    // Create screen objects
    lv_obj_t* main_screen = lv_obj_create(NULL);
    lv_obj_t* settings_screen = lv_obj_create(NULL);
    lv_obj_t* display_screen = lv_obj_create(NULL);

    // Create screen contents
    create_main_screen(main_screen,settings_screen);

    create_settings_screen(settings_screen,main_screen,display_screen);

    create_display_screen(display_screen,settings_screen);

    // Load main screen
    lv_screen_load(main_screen);
}
#endif  // RADAR_LCD
