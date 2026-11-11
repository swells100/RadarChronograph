
#include <Arduino.h>
#include <lvgl.h>

#include "buttons.h"
#include "screen.h"
#include "settings.h"

// =====================================================
// SCREEN OBJECTS
// =====================================================

static lv_obj_t *main_screen;
static lv_obj_t *settings_screen;
static lv_obj_t *display_screen;
static lv_obj_t *shot_slider;
static lv_obj_t *brightness_slider;
static lv_obj_t *arm_button;


// =====================================================
// BUTTON DATA
// =====================================================

struct ButtonData
{
    int value;

    const char* text_off;
    const char* text_on;

    lv_color_t color_off;
    lv_color_t color_on;
};

// Hardware-readable variables
int ARM = 0;
int Portrait = 0;
int variable3 = 0;
int ShotTime = 10; // Shot time in seconds
int ScreenBrightness = 100;

// ARM
static ButtonData data1 = {0,"ARM","ARMED",lv_color_hex(0x808080),lv_color_hex(0xFF0000)};

// TEST2
//static ButtonData data3 = {0,"TEST2","TEST2+",lv_color_hex(0x808080),lv_color_hex(0xFF9800)};

static void update_toggle_button(lv_obj_t* button, ButtonData* data)
{
    if (button == nullptr) return;

    lv_obj_t* label = lv_obj_get_child(button, 0);

    lv_label_set_text(
        label,
        data->value ? data->text_on : data->text_off
    );

    lv_obj_set_style_bg_color(
        button,
        data->value ? data->color_on : data->color_off,
        LV_PART_MAIN
    );
}

// =====================================================
// MAIN BUTTON CALLBACK
// =====================================================

static void button_cb(lv_event_t* event)
{
    lv_obj_t* button = (lv_obj_t*)lv_event_get_target(event);
    ButtonData* data = (ButtonData*)lv_event_get_user_data(event);

    data->value = !data->value;

    update_toggle_button(button, data);

    // Update hardware-readable variables
    if (data == &data1)
    {
        ARM = data->value;
    }
}

static void shot_time_cb(lv_event_t* event)
{
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(event);
    lv_obj_t* label = (lv_obj_t*)lv_event_get_user_data(event);

    // Round to nearest 5
    ShotTime = ((lv_slider_get_value(slider) + 2) / 5) * 5;

    lv_slider_set_value(slider, ShotTime, LV_ANIM_OFF);
    lv_label_set_text_fmt(label,"%d s", ShotTime);
}

// =====================================================
// ORIENTATION SWITCH CALLBACK
// =====================================================
void buttons_update_layout()
{
    int screen_width = Portrait ? 272 : 480;

    if (shot_slider != nullptr)
        lv_obj_set_width(shot_slider, screen_width * 63 / 100);

    if (brightness_slider != nullptr)
        lv_obj_set_width(brightness_slider, screen_width * 63 / 100);
}
static void orientation_cb(lv_event_t* event)
{
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(event);

    Portrait = lv_obj_has_state(sw, LV_STATE_CHECKED);

    screen_toggle_orientation();

    save_settings();
}
static void brightness_cb(lv_event_t* event)
{
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(event);
    lv_obj_t* label = (lv_obj_t*)lv_event_get_user_data(event);
    
    ScreenBrightness = lv_slider_get_value(slider);

    // Convert percentage (0–100) to RA8875 brightness (0–255)
    uint8_t brightness = ScreenBrightness * 255 / 100;

    screen_set_brightness(brightness);

    lv_label_set_text_fmt(label, "%d", ScreenBrightness);
}
// =====================================================
// CREATE BUTTON
// 
// parent: object that contains the button
// x: horizontal position
// y: vertical position
// width: button width
// height: button height
// text: button text
// color: button color
// destination_screen: screen to navigate to when button is clicked
// toggle_data: toggle button data
// user_data: user data passed to callback
// callbackFunction: executed when button is clicked
// =====================================================
static lv_obj_t* create_button(
    lv_obj_t* parent,
    int x,
    int y,
    int width,
    int height,
    const char* text,
    lv_color_t color,
    lv_obj_t* destination_screen = nullptr,
    ButtonData* toggle_data = nullptr,
    void* user_data = nullptr,
    lv_event_cb_t callback = nullptr
    )
{
    lv_obj_t* button = lv_button_create(parent);

    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);

    // Initial appearance
    if (toggle_data)
    {
        text = toggle_data->value ? toggle_data->text_on : toggle_data->text_off;
        color = toggle_data->value ? toggle_data->color_on : toggle_data->color_off;

        callback = button_cb;
        user_data = toggle_data;
    }

    // Button background
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    // Remove default shadow
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);

    // Screen navigation
    if (destination_screen)
    {
        lv_obj_add_event_cb(
            button,
            [](lv_event_t* event)
            {
                lv_screen_load((lv_obj_t*)lv_event_get_user_data(event));
            },
            LV_EVENT_CLICKED,
            destination_screen
        );
    }
    if (callback)
    {
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);
    }

    // Button label
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return button;
}

void set_arm_state(bool armed)
{
    ARM = armed ? 1 : 0;
    data1.value = ARM;

    update_toggle_button(arm_button, &data1);
}

// =====================================================
// CREATE PAGE HEADER
// =====================================================

static void create_page_header(lv_obj_t* parent,const char* back_text,const char* title_text,lv_obj_t* destination_screen)
{
    lv_obj_t* back_button = create_button(parent,0, 0,100, 45,back_text,lv_color_hex(0x303F50),destination_screen);
    lv_obj_set_style_radius(back_button, 0, LV_PART_MAIN);
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title,title_text);
    lv_obj_set_style_text_color(title,lv_color_white(),LV_PART_MAIN);
    lv_obj_align_to(title,back_button,LV_ALIGN_OUT_RIGHT_MID,15,0);
}

// =====================================================
// CREATE MAIN SCREEN
// =====================================================

static void create_main_screen()
{
    lv_obj_set_style_bg_color(main_screen,lv_color_hex(0x000000),LV_PART_MAIN);

    // Settings button
    lv_obj_t* settings_button = create_button(main_screen, 0, 0, 110, 45,"SETTINGS",lv_color_hex(0x303F50),settings_screen);
    lv_obj_set_style_radius(settings_button, 0, LV_PART_MAIN);
    // ARM button
    arm_button = create_button(main_screen, 0, 0, 130, 60,"ARM",lv_color_hex(0x808080),nullptr, &data1);
    lv_obj_align(arm_button, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    // TEST2 button
    //create_button(main_screen, 20, 170, 130, 60,"TEST2",lv_color_hex(0x808080),nullptr, &data3);
}

// =====================================================
// CREATE SETTINGS PAGE
// =====================================================

static void create_settings_screen()
{
    lv_obj_set_style_bg_color(settings_screen,lv_color_hex(0x000000),LV_PART_MAIN);
    // Enable vertical scrolling
    lv_obj_set_scroll_dir(settings_screen, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(settings_screen, LV_SCROLLBAR_MODE_AUTO);

    // Header
    create_page_header(settings_screen,"BACK","SETTINGS",main_screen);
    // Display option
    create_button(settings_screen, 20, 90, 200, 55,"DISPLAY",lv_color_hex(0x303F50),display_screen);

    // Shot time title
    lv_obj_t* shot_label = lv_label_create(settings_screen);
    lv_label_set_text(shot_label, "Shot Time:");
    lv_obj_set_style_text_color(shot_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_pos(shot_label, 20, 170);

    // Shot time value
    lv_obj_t* shot_value = lv_label_create(settings_screen);
    lv_label_set_text_fmt(shot_value, "%d s", ShotTime);
    lv_obj_set_style_text_color(shot_value, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_pos(shot_value, 160, 170);

    // Shot time slider
    shot_slider = lv_slider_create(settings_screen);
    lv_slider_set_range(shot_slider, 5, 90);
    lv_slider_set_value(shot_slider, ShotTime, LV_ANIM_OFF);
    lv_obj_set_pos(shot_slider, 20, 210);
    lv_display_t* display = lv_display_get_default();
    int screen_width = lv_display_get_horizontal_resolution(display);
    lv_obj_set_size(shot_slider, screen_width * 63 / 100, 15);

    // Slider callback
    lv_obj_add_event_cb(shot_slider,shot_time_cb,LV_EVENT_VALUE_CHANGED,shot_value);
    lv_obj_add_event_cb(shot_slider,[](lv_event_t* event){save_settings();},LV_EVENT_RELEASED,NULL);
}

// =====================================================
// CREATE DISPLAY SETTINGS PAGE
// =====================================================

static void create_display_screen()
{
    lv_obj_set_style_bg_color(display_screen,lv_color_hex(0x000000),LV_PART_MAIN);

    // Header
    create_page_header(display_screen,"BACK","DISPLAY",settings_screen);

    // Orientation label
    lv_obj_t* orientation_label = lv_label_create(display_screen);
    lv_label_set_text(orientation_label,"Portrait orientation");
    lv_obj_set_style_text_color(orientation_label,lv_color_white(),LV_PART_MAIN);
    lv_obj_set_pos(orientation_label,20,90);
    // Orientation switch
    lv_obj_t* orientation_switch = lv_switch_create(display_screen);
    lv_obj_set_pos(orientation_switch,200,85);

    if (Portrait)
    {
        lv_obj_add_state(orientation_switch,LV_STATE_CHECKED);
    }

    lv_obj_add_event_cb(orientation_switch,orientation_cb,LV_EVENT_VALUE_CHANGED,NULL);
    // Brightness label
    lv_obj_t* brightness_label = lv_label_create(display_screen);
    lv_label_set_text(brightness_label, "Brightness:");
    lv_obj_set_style_text_color(brightness_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_pos(brightness_label, 20, 150);

    // Brightness value
    lv_obj_t* brightness_value = lv_label_create(display_screen);
    lv_label_set_text_fmt(brightness_value, "%d%%", ScreenBrightness);
    lv_obj_set_style_text_color(brightness_value, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_pos(brightness_value, 160, 150);

    // Brightness slider
    brightness_slider = lv_slider_create(display_screen);
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_slider_set_value(brightness_slider, ScreenBrightness, LV_ANIM_OFF);
    lv_obj_set_pos(brightness_slider, 20, 190);

    lv_display_t* display = lv_display_get_default();
    int screen_width = lv_display_get_horizontal_resolution(display);

    lv_obj_set_size(brightness_slider, screen_width * 63 / 100, 15);

    lv_obj_add_event_cb(brightness_slider,brightness_cb,LV_EVENT_VALUE_CHANGED,brightness_value);
    lv_obj_add_event_cb(brightness_slider,[](lv_event_t* event){save_settings();},LV_EVENT_RELEASED,NULL);
}

// =====================================================
// CREATE ALL SCREENS
// =====================================================

void create_buttons()
{
    // Create all screens
    main_screen = lv_obj_create(NULL);
    settings_screen = lv_obj_create(NULL);
    display_screen = lv_obj_create(NULL);

    // Fill Main screen
    create_main_screen();

    // Fill Settings screen
    create_settings_screen();
    // Fill Display Settings screen
    create_display_screen();

    // Load main screen
    lv_screen_load(main_screen);
}
