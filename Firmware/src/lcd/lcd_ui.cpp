#include "lcd_ui.h"
#if RADAR_LCD
#include <Arduino.h>
#include <lvgl.h>
#include "screen.h"
#include "buttons.h"
#include "settings.h"
#include "main_screen.h"

void lcd_init()
{
    load_settings();      // brightness, orientation, shot time from EEPROM
    screen_init();        // RA8875 + LVGL display + touch
    create_buttons();     // screens
}

// LVGL draws and sends the screen over SPI in the foreground (a few ms for a label, ~100 ms for a full
// redraw), and the ADC only has two 4 ms blocks of buffer. So LVGL is not run while a shot is being
// tracked, for at most LCD_MAX_DEFER_MS, and only every LCD_SERVICE_MS otherwise.
void lcd_service(bool radar_busy)
{
    static uint32_t last_run = 0;
    static uint32_t deferred_since = 0;
    const uint32_t now = millis();

    if ((uint32_t)(now - last_run) < LCD_SERVICE_MS) return;
    if (radar_busy)
    {
        if (deferred_since == 0) deferred_since = now;
        if ((uint32_t)(now - deferred_since) < LCD_MAX_DEFER_MS) return;
    }
    deferred_since = 0;
    last_run = now;

    lv_timer_handler();
    screen_process_orientation();
}

void lcd_set_armed(bool armed, uint32_t ms_left) { main_screen_set_armed(armed, ms_left); }
void lcd_show_shot(uint32_t shot_number, float speed_mps) { main_screen_update_values((int)shot_number, speed_mps); }
bool lcd_take_arm_toggle() { return main_screen_take_arm_toggle(); }
uint32_t lcd_arm_duration_ms() { return (uint32_t)ShotTime * 1000u; }
#endif  // RADAR_LCD
