#include <Arduino.h>
#include "lvgl.h"
#include "buttons.h"

struct ButtonData {
    int value;
    const char* text_off;
    const char* text_on;
    lv_color_t color_off;
    lv_color_t color_on;
};

int variable1 = 0;
int variable2 = 0;
int variable3 = 0;

static ButtonData data1 = {
    0, "ARM", "ARMED",
    lv_color_hex(0x808080), lv_color_hex(0xFF0000)
};

static ButtonData data2 = {
    0, "TEST1", "TEST1!",
    lv_color_hex(0x808080), lv_color_hex(0x2196F3)
};

static ButtonData data3 = {
    0, "TEST2", "TEST2!",
    lv_color_hex(0x808080), lv_color_hex(0xFF9800)
};

static void button_cb(lv_event_t* event)
{
    lv_obj_t* button = (lv_obj_t*)lv_event_get_target(event);
    lv_obj_t* label = lv_obj_get_child(button, 0);

    ButtonData* data = (ButtonData*)lv_event_get_user_data(event);

    // Toggle between 0 and 1
    data->value = !data->value;

    // Update text and background color
    if (data->value == 1) {
        lv_label_set_text(label, data->text_on);
        lv_obj_set_style_bg_color(button, data->color_on, LV_PART_MAIN);
    } else {
        lv_label_set_text(label, data->text_off);
        lv_obj_set_style_bg_color(button, data->color_off, LV_PART_MAIN);
    }
    if (data == &data1) {
        variable1 = data->value;
    } 
    else if (data == &data2) {
    variable2 = data->value;
    } 
    else if (data == &data3) {
        variable3 = data->value;
}
}

static void create_one_button(int x,int y,ButtonData* data)
{
    lv_obj_t* button = lv_button_create(lv_screen_active());

    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, 130, 60);

    // Set initial color
    lv_obj_set_style_bg_color(
        button, data->color_off, LV_PART_MAIN
    );

    // Register callback and pass this button's data
    lv_obj_add_event_cb(button, button_cb, LV_EVENT_CLICKED, data);

    // Create label
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, data->text_off);
    lv_obj_center(label);
}

void create_buttons()
{
    create_one_button(20, 20, &data1);
    create_one_button(20, 100, &data2);
    create_one_button(20, 180, &data3);
}