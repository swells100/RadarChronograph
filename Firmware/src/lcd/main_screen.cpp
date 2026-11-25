// Adapted from swells100/RadarChronograph, Teensy_RA8875 (LVGL touch UI on an RA8875 480x272 display).
// Changes from upstream: the ARM button no longer flips itself. A press only sets a request flag
// (main_screen_take_arm_toggle); the radar firmware arms/disarms and calls main_screen_set_armed, so the
// button always shows the real state, including the automatic disarm when the shot time runs out.
#include "../radar_config.h"
#if RADAR_LCD
#include <Arduino.h>
#include <lvgl.h>
#include <math.h>

#include "main_screen.h"
#include "buttons.h"

static lv_obj_t* arm_button = nullptr;
static lv_obj_t* arm_label = nullptr;
static lv_obj_t* shot_counter_label = nullptr;
static lv_obj_t* velocity_label = nullptr;
static lv_obj_t* velocity_alt_label = nullptr;
static volatile bool arm_toggle_requested = false;

static void arm_button_cb(lv_event_t*)
{
    arm_toggle_requested = true;
}

bool main_screen_take_arm_toggle()
{
    const bool r = arm_toggle_requested;
    arm_toggle_requested = false;
    return r;
}

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
    lv_obj_set_size(arm_button, 150, 60);

    lv_obj_set_style_bg_color(arm_button, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(arm_button, 0, LV_PART_MAIN);
    arm_label = lv_label_create(arm_button);
    lv_label_set_text(arm_label, "ARM");
    lv_obj_center(arm_label);

    lv_obj_add_event_cb(arm_button, arm_button_cb, LV_EVENT_CLICKED, NULL);

    // Shot counter (top centre, below the SETTINGS button row)
    shot_counter_label = lv_label_create(main_screen);
    lv_label_set_text(shot_counter_label, "SHOT: 0");
    lv_obj_set_style_text_font(shot_counter_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(shot_counter_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(shot_counter_label, LV_ALIGN_TOP_MID, 0, 55);

    // Speed (large) in m/s
    velocity_label = lv_label_create(main_screen);
    lv_label_set_text(velocity_label, "-- m/s");
    lv_obj_set_style_text_font(velocity_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(velocity_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(velocity_label, LV_ALIGN_TOP_MID, 0, 88);

    // The same speed in mph and ft/s
    velocity_alt_label = lv_label_create(main_screen);
    lv_label_set_text(velocity_alt_label, "");
    lv_obj_set_style_text_font(velocity_alt_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(velocity_alt_label, lv_color_hex(0xB0B8C0), LV_PART_MAIN);
    lv_obj_align(velocity_alt_label, LV_ALIGN_TOP_MID, 0, 148);
}

void main_screen_set_armed(bool armed, uint32_t ms_left)
{
    ARM = armed ? 1 : 0;                       // keep the shared variable equal to the real state
    if (arm_button == nullptr) return;

    static int last_state = -1;
    static int last_secs = -1;
    const int secs = armed ? (int)((ms_left + 999) / 1000) : 0;
    if ((int)armed == last_state && secs == last_secs) return;   // nothing to redraw
    last_state = armed;
    last_secs = secs;

    lv_obj_set_style_bg_color(arm_button, armed ? lv_color_hex(0xFF0000) : lv_color_hex(0x808080), LV_PART_MAIN);
    if (armed) lv_label_set_text_fmt(arm_label, "ARMED %ds", secs);
    else       lv_label_set_text(arm_label, "ARM");
}

void main_screen_update_values(int shot_number, float speed_mps)
{
    if (shot_counter_label != nullptr)
    {
        lv_label_set_text_fmt(shot_counter_label, "SHOT: %d", shot_number);
    }

    if (speed_mps < 0.0f) speed_mps = 0.0f;
    const int tenths = (int)lroundf(speed_mps * 10.0f);               // LVGL is built without float printf
    const int mph = (int)lroundf(speed_mps * 2.236936f);
    const int fps = (int)lroundf(speed_mps * 3.28084f);

    if (velocity_label != nullptr)
    {
        lv_label_set_text_fmt(velocity_label, "%d.%d m/s", tenths / 10, tenths % 10);
    }
    if (velocity_alt_label != nullptr)
    {
        lv_label_set_text_fmt(velocity_alt_label, "%d mph / %d ft/s", mph, fps);
    }
}
#endif  // RADAR_LCD
