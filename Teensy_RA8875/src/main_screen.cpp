#include <Arduino.h>
#include <lvgl.h>

#include "main_screen.h"
#include "buttons.h"

// =====================================================
// CREATE MAIN SCREEN
// =====================================================
static lv_obj_t* arm_button;
static lv_obj_t* shot_counter_label;
static lv_obj_t* velocity_label;

static ToggleButtonData arm_data = {
    0,
    "ARM",
    "ARMED",
    lv_color_hex(0x808080),
    lv_color_hex(0xFF0000),
    &ARM
};

void create_main_screen(lv_obj_t* main_screen,lv_obj_t* settings_screen)
{
    lv_obj_set_style_bg_color(main_screen,lv_color_hex(0x000000),LV_PART_MAIN);

    // SETTINGS button
    lv_obj_t* settings_button = lv_button_create(main_screen);
    lv_obj_set_pos(settings_button, 0, 0);
    lv_obj_set_size(settings_button, 110, 45);
    lv_obj_set_style_radius(settings_button, 0, LV_PART_MAIN);

    lv_obj_set_style_bg_color(settings_button, lv_color_hex(0x303F50), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(settings_button, 0, LV_PART_MAIN);
    
    lv_obj_t* settings_label = lv_label_create(settings_button);
    lv_label_set_text(settings_label, "SETTINGS");
    lv_obj_center(settings_label);

    lv_obj_add_event_cb(settings_button,[](lv_event_t* event){lv_screen_load((lv_obj_t*)lv_event_get_user_data(event));},LV_EVENT_CLICKED,settings_screen);
    
    // ARM button
    arm_button = lv_button_create(main_screen);
    lv_obj_align(arm_button, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_size(arm_button, 130, 60);

    lv_obj_set_style_bg_color(arm_button, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(arm_button, 0, LV_PART_MAIN);
    lv_obj_t* arm_label = lv_label_create(arm_button);
    lv_label_set_text(arm_label, "ARM");
    lv_obj_center(arm_label);

    lv_obj_add_event_cb(arm_button,toggle_button_cb,LV_EVENT_CLICKED,&arm_data);

    // Shot Counter
    shot_counter_label = lv_label_create(main_screen);
    lv_label_set_text(shot_counter_label, "SHOT: 0");
    lv_obj_set_style_text_font(shot_counter_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(shot_counter_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(shot_counter_label, LV_ALIGN_CENTER, 0, -45);

    // Velocity
    velocity_label = lv_label_create(main_screen);
    lv_label_set_text(velocity_label, "0 m/s");
    lv_obj_set_style_text_font(velocity_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(velocity_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(velocity_label, LV_ALIGN_CENTER, 0, 0);
}

void main_screen_update_values(int shot_number, int velocity)
{
    if (shot_counter_label != nullptr)
    {
        lv_label_set_text_fmt(shot_counter_label, "SHOT: %d", shot_number);
    }

    if (velocity_label != nullptr)
    {
        lv_label_set_text_fmt(velocity_label, "%d m/s", velocity);
    }
}