#include <Arduino.h>
#include <lvgl.h>

#include "settings_screen.h"
#include "buttons.h"
#include "settings.h"

static lv_obj_t* shot_slider = nullptr;

// SHOT TIME CALLBACK
static void shot_time_cb(lv_event_t* event)
{
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(event);
    lv_obj_t* label = (lv_obj_t*)lv_event_get_user_data(event);

    // Round to nearest 5
    ShotTime = ((lv_slider_get_value(slider) + 2) / 5) * 5;

    lv_slider_set_value(slider, ShotTime, LV_ANIM_OFF);
    lv_label_set_text_fmt(label,"%d s",ShotTime);
}

// CREATE SETTINGS SCREEN
void create_settings_screen(lv_obj_t* settings_screen,lv_obj_t* main_screen,lv_obj_t* display_screen)
{
    lv_obj_set_style_bg_color(settings_screen,lv_color_hex(0x000000),LV_PART_MAIN);

    // Enable vertical scrolling
    lv_obj_set_scroll_dir( settings_screen, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(settings_screen, LV_SCROLLBAR_MODE_AUTO);

    // Header
    create_page_header(settings_screen,"BACK","SETTINGS",main_screen);

    // DISPLAY button
    lv_obj_t* display_button = lv_button_create(settings_screen);
    lv_obj_set_pos(display_button, 20, 90);
    lv_obj_set_size(display_button, 200, 55);

    lv_obj_set_style_bg_color(display_button, lv_color_hex(0x303F50), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(display_button, 0, LV_PART_MAIN);
    lv_obj_t* display_label = lv_label_create(display_button);
    lv_label_set_text(display_label, "DISPLAY");
    lv_obj_center(display_label);

    lv_obj_add_event_cb(display_button,[](lv_event_t* event){lv_screen_load((lv_obj_t*)lv_event_get_user_data(event));},LV_EVENT_CLICKED,display_screen);

    // Shot time title
    lv_obj_t* shot_label = lv_label_create(settings_screen);
    lv_label_set_text(shot_label, "Shot Time:");
    lv_obj_set_style_text_color(shot_label,lv_color_white(),LV_PART_MAIN);
    lv_obj_set_pos(shot_label, 20, 170);

    // Shot time value
    lv_obj_t* shot_value = lv_label_create(settings_screen);
    lv_label_set_text_fmt(shot_value,"%d s",ShotTime);
    lv_obj_set_style_text_color(shot_value,lv_color_white(),LV_PART_MAIN);
    lv_obj_set_pos(shot_value, 160, 170);

    // Shot time slider
    shot_slider = lv_slider_create(settings_screen);
    lv_slider_set_range(shot_slider, 5, 90);
    lv_slider_set_value(shot_slider, ShotTime, LV_ANIM_OFF);
    lv_obj_set_pos(shot_slider, 20, 210);

    settings_screen_update_layout();

    // Slider callbacks
    lv_obj_add_event_cb(shot_slider,shot_time_cb,LV_EVENT_VALUE_CHANGED,shot_value);
    lv_obj_add_event_cb(shot_slider,[](lv_event_t* event){save_settings();},LV_EVENT_RELEASED,NULL);
}

// UPDATE SETTINGS SCREEN LAYOUT
void settings_screen_update_layout()
{
    if (shot_slider == nullptr) return;

    int screen_width = Portrait ? 272 : 480;

    lv_obj_set_width(shot_slider,screen_width * 63 / 100);
}