// Adapted from swells100/RadarChronograph, Teensy_RA8875 (LVGL touch UI on an RA8875 480x272 display).
#include "../radar_config.h"
#if RADAR_LCD
#include <Arduino.h>
#include <lvgl.h>

#include "display_screen.h"
#include "buttons.h"
#include "screen.h"
#include "settings.h"

static lv_obj_t* brightness_slider = nullptr;

// ORIENTATION CALLBACK
static void orientation_cb(lv_event_t* event)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(event);

    Portrait = lv_obj_has_state(sw, LV_STATE_CHECKED);

    screen_toggle_orientation();

    save_settings();
}

// BRIGHTNESS CALLBACK
static void brightness_cb(lv_event_t* event)
{
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(event);
    lv_obj_t* label = (lv_obj_t*)lv_event_get_user_data(event);

    ScreenBrightness = lv_slider_get_value(slider);

    // Convert percentage (0-100) to RA8875 brightness (0-255)
    uint8_t brightness = ScreenBrightness * 255 / 100;

    screen_set_brightness(brightness);

    lv_label_set_text_fmt(label, "%d%%", ScreenBrightness);
}

// CREATE DISPLAY SCREEN
void create_display_screen(lv_obj_t* display_screen,lv_obj_t* settings_screen)
{
    lv_obj_set_style_bg_color(display_screen,lv_color_hex(0x000000),LV_PART_MAIN);

    // Header
    create_page_header(display_screen,"BACK","DISPLAY",settings_screen);
    
    lv_obj_t* orientation_label = lv_label_create(display_screen);
    lv_label_set_text(orientation_label,"Portrait orientation");
    lv_obj_set_style_text_color(orientation_label,lv_color_white(),LV_PART_MAIN);
    lv_obj_set_pos(orientation_label, 20, 90);

    // Orientation switch
    lv_obj_t* orientation_switch = lv_switch_create(display_screen);
    lv_obj_set_pos(orientation_switch, 200, 85);

    if (Portrait)
    {
        lv_obj_add_state(orientation_switch,LV_STATE_CHECKED);
    }

    lv_obj_add_event_cb(orientation_switch,orientation_cb,LV_EVENT_VALUE_CHANGED,NULL);

    // Brightness label
    lv_obj_t* brightness_label = lv_label_create(display_screen);
    lv_label_set_text(brightness_label,"Brightness:");
    lv_obj_set_style_text_color(brightness_label,lv_color_white(),LV_PART_MAIN);
    lv_obj_set_pos(brightness_label, 20, 150);

    // Brightness value
    lv_obj_t* brightness_value = lv_label_create(display_screen);
    lv_label_set_text_fmt(brightness_value,"%d%%",ScreenBrightness);
    lv_obj_set_style_text_color(brightness_value,lv_color_white(),LV_PART_MAIN);
    lv_obj_set_pos(brightness_value, 160, 150);

    // Brightness slider
    brightness_slider = lv_slider_create(display_screen);
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_slider_set_value(brightness_slider,ScreenBrightness,LV_ANIM_OFF);
    lv_obj_set_pos(brightness_slider, 20, 190);

    display_screen_update_layout();

    // Slider callbacks
    lv_obj_add_event_cb(brightness_slider,brightness_cb,LV_EVENT_VALUE_CHANGED,brightness_value);
    lv_obj_add_event_cb(brightness_slider,[](lv_event_t* event){save_settings();},LV_EVENT_RELEASED,NULL);
}

// UPDATE DISPLAY SCREEN LAYOUT
void display_screen_update_layout()
{
    if (brightness_slider == nullptr) return;

    int screen_width = Portrait ? 272 : 480;

    lv_obj_set_width(brightness_slider,screen_width * 63 / 100);
}
#endif  // RADAR_LCD
