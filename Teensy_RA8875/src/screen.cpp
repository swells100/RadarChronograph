
#include <Arduino.h>
#include <stdint.h>
#include <SPI.h>
#include <Wire.h>
#include "buttons.h"
#include <RA8875.h>
#include <lvgl.h>

// =====================================================
// HARDWARE PINS
// =====================================================

// Teensy 4.1 SPI0:
// MOSI = 11, MISO = 12, SCK = 13

#define RA8875_CS       8
#define RA8875_RESET    9

// FT5206 capacitive touchscreen (unused)
#define FT5206_WAKE     6
#define FT5206_INT      7

// Teensy 4.1 SPI0
#define RA8875_MOSI     11
#define RA8875_MISO     12
#define RA8875_SCK      13

static constexpr uint16_t TFT_LANDSCAPE_WIDTH = 480;
static constexpr uint16_t TFT_LANDSCAPE_HEIGHT = 272;

static constexpr uint16_t TFT_PORTRAIT_WIDTH = 272;
static constexpr uint16_t TFT_PORTRAIT_HEIGHT = 480;


static lv_display_t *display;
static bool rotation = false;
static bool orientation_change_pending = false;

// =====================================================
// DISPLAY OBJECTS
// =====================================================

// Native Teensy SPI0
RA8875 tft(RA8875_CS, RA8875_RESET,RA8875_MOSI, RA8875_SCK, RA8875_MISO);
void screen_set_brightness(uint8_t brightness)
{
    tft.brightness(brightness);
}
// =====================================================
// DISPLAY FLUSH
// =====================================================

static void displayFlush(
    lv_display_t *display,
    const lv_area_t *area,
    uint8_t *pixelMap)
{
    const int32_t width = area->x2 - area->x1 + 1;
    const int32_t height = area->y2 - area->y1 + 1;

    uint16_t *pixels = (uint16_t *)pixelMap;

    tft.writeRect(area->x1,area->y1,width,height,pixels);

    lv_display_flush_ready(display);
}

// =====================================================
// TOUCH INPUT
// =====================================================

static void my_touch_read_cb(lv_indev_t *indev,lv_indev_data_t *data)
{
    static uint16_t touch_coordinates[5][2];

    tft.updateTS();

    if (tft.getTouches() > 0)
    {
        tft.getTScoordinates(touch_coordinates);

        data->point.x = touch_coordinates[0][0];
        data->point.y = touch_coordinates[0][1];

        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// =====================================================
// INITIALIZE DISPLAY
// =====================================================

static void initDisplay()
{
    SPI.begin();

    // Initialize native RA8875 display
    tft.begin(RA8875_480x272);
    // Apply saved orientation
    rotation = Portrait;
    tft.setRotation(Portrait ? 3 : 0);
    // Apply saved brightness (percentage to 0-255)
    tft.brightness(ScreenBrightness * 255 / 100);
    tft.fillScreen(RA8875_BLACK);

    Serial.println("RA8875 initialized");
}

// =====================================================
// INITIALIZE LVGL
// =====================================================

static void initLVGL()
{
    //Initialize LVGL
    lv_init();

    // Set millisecond-based tick source for LVGL so that it can track time
    lv_tick_set_cb(millis);

    // Create a display where screens and widgets can be added
    uint16_t width = Portrait ? TFT_PORTRAIT_WIDTH : TFT_LANDSCAPE_WIDTH;
    uint16_t height = Portrait ? TFT_PORTRAIT_HEIGHT : TFT_LANDSCAPE_HEIGHT;

    display = lv_display_create(width, height);
    lv_display_set_resolution(display, width, height);

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    DMAMEM static uint16_t buf[480 * 272/10*2];
    lv_display_set_buffers(display, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Add a callback that can flush the content from `buf` when it has been rendered
    lv_display_set_flush_cb(display, displayFlush);

    // Create an input device for touch handling
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev,LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev,my_touch_read_cb);
}

void screen_toggle_orientation()
{
    orientation_change_pending = true;
}
void screen_process_orientation()
{
    if (!orientation_change_pending) return;

    orientation_change_pending = false;

    rotation = !rotation;

    if (rotation)
    {
        tft.setRotation(3);

        lv_display_set_resolution(display, TFT_PORTRAIT_WIDTH, TFT_PORTRAIT_HEIGHT);

        Serial.println("Portrait orientation");
    }
    else
    {
        tft.setRotation(0);

        lv_display_set_resolution(display, TFT_LANDSCAPE_WIDTH, TFT_LANDSCAPE_HEIGHT);

        Serial.println("Landscape orientation");
    }
    // Resize sliders before refreshing the display
    buttons_update_layout();

    // Clear the display and request a complete LVGL redraw
    tft.fillScreen(RA8875_BLACK);
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(display);
}
// =====================================================
// PUBLIC SCREEN INITIALIZATION
// =====================================================

void screen_init()
{
    Serial.println("Initializing screen...");
    initDisplay();
    initLVGL();

    Serial.println("Screen orientation: Portrait");
    Serial.println("Screen initialized");
}

