// lcd_ui.h - the touch display, seen from the radar firmware.
//
// Display: RA8875 480x272 with FT5206 capacitive touch, LVGL UI (swells100/RadarChronograph, Teensy_RA8875).
// With RADAR_LCD 0 everything here compiles to nothing.
#pragma once
#include <stdint.h>
#include "../radar_config.h"

#if RADAR_LCD
void lcd_init();                                   // load saved settings, start display + LVGL, build the screens
void lcd_service(bool radar_busy);                 // call every loop(); runs LVGL (rate-limited, held back while a shot is tracked)
void lcd_set_armed(bool armed, uint32_t ms_left);  // show the real arm state and countdown
void lcd_show_shot(uint32_t shot_number, float speed_mps);
bool lcd_take_arm_toggle();                        // true once per press of the ARM button
uint32_t lcd_arm_duration_ms();                    // the "Shot Time" setting (seconds) in ms
#else
static inline void lcd_init() {}
static inline void lcd_service(bool) {}
static inline void lcd_set_armed(bool, uint32_t) {}
static inline void lcd_show_shot(uint32_t, float) {}
static inline bool lcd_take_arm_toggle() { return false; }
static inline uint32_t lcd_arm_duration_ms() { return ARM_DURATION_MS; }
#endif
