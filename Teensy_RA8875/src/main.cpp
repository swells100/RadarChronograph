#include <Arduino.h>
#include <stdint.h>
#include <SPI.h>
#include <Wire.h>

#include <RA8875.h>
#include <lvgl.h>

#include "buttons.h"

// Teensy 4.1 SPI0 pins:
// MOSI = 11, MISO = 12, SCK = 13

#define RA8875_CS     8
#define RA8875_RESET  9

// FT5206 capacitive touchscreen
#define FT5206_WAKE   6
#define FT5206_INT    7

static constexpr uint8_t CTP_ADDR = 0x38;

// Native RA8875 dimensions
static constexpr uint16_t TFT_NATIVE_WIDTH  = 480;
static constexpr uint16_t TFT_NATIVE_HEIGHT = 272;
// LVGL dimensions after 90-degree clockwise rotation
static constexpr uint16_t TFT_WIDTH  = 272;
static constexpr uint16_t TFT_HEIGHT = 480;

// LVGL partial-render buffer: 40 RGB565 rows
static constexpr uint16_t BUFFER_ROWS = 40;

// Native hardware SPI0
RA8875 tft(RA8875_CS, RA8875_RESET);

// LVGL objects
static lv_display_t *lvDisplay = nullptr;
static lv_indev_t *lvTouch = nullptr;

// RGB565 draw buffer: 38,400 bytes
static uint16_t drawBuffer[TFT_WIDTH * BUFFER_ROWS];
// Temporary buffer for rotated display rows
static uint16_t rotatedRow[BUFFER_ROWS];

// FT5206 I2C functions

static uint8_t readTouchRegister(uint8_t reg)
{
    Wire.beginTransmission(CTP_ADDR);
    Wire.write(reg);

    if (Wire.endTransmission() != 0) {
        return 0;
    }

    if (Wire.requestFrom(CTP_ADDR, (uint8_t)1) != 1) {
        return 0;
    }

    return (uint8_t)Wire.read();
}

static uint8_t readTouchBytes(
    uint8_t reg,
    uint8_t *buffer,
    uint8_t length)
{
    Wire.beginTransmission(CTP_ADDR);
    Wire.write(reg);

    if (Wire.endTransmission() != 0) {
        return 0;
    }

    uint8_t received = Wire.requestFrom(CTP_ADDR, length);
    uint8_t count = 0;

    while (count < received && Wire.available()) {
        buffer[count++] = (uint8_t)Wire.read();
    }

    return count;
}

static void writeTouchRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(CTP_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

// LVGL display flush

static void displayFlush(lv_display_t *display,const lv_area_t *area,uint8_t *pixelMap)
{
    const int32_t width = area->x2 - area->x1 + 1;
    const int32_t height = area->y2 - area->y1 + 1;

    uint16_t *pixels =
        reinterpret_cast<uint16_t *>(pixelMap);

    //for (int32_t row = 0; row < height; row++) {
    //    tft.drawPixels( &pixels[row * width], width,area->x1,area->y1 + row);
    //}
     // Each LVGL column becomes one native display row.
    for (int32_t col = 0; col < width; col++) {

        // Reverse the LVGL row order for clockwise rotation.
        for (int32_t row = 0; row < height; row++) {
            rotatedRow[row] =
                pixels[(height - 1 - row) * width + col];
        }

        const int16_t nativeX =
            TFT_NATIVE_WIDTH - 1 - area->y2;

        const int16_t nativeY =
            area->x1 + col;

        // Write this rotated row as a bulk pixel transfer.
        tft.drawPixels(
            rotatedRow,
            height,
            nativeX,
            nativeY
        );
    }

    lv_display_flush_ready(display);
}

// LVGL touchscreen input

static void touchRead(
    lv_indev_t *indev,
    lv_indev_data_t *data)
{
    (void)indev;

    uint8_t status = readTouchRegister(0x02);
    uint8_t points = status & 0x0F;

    if (points == 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    // Each FT5206 touch record is 6 bytes.
    uint8_t touchData[6];

    if (readTouchBytes( 0x03, touchData, sizeof(touchData)) != sizeof(touchData)) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    //uint16_t x = ((uint16_t)(touchData[0] & 0x0F) << 8) | touchData[1];

    //uint16_t y = ((uint16_t)(touchData[2] & 0x0F) << 8) | touchData[3];

    //if (x >= TFT_WIDTH || y >= TFT_HEIGHT) {
    //    data->state = LV_INDEV_STATE_RELEASED;
    //    return;
    //}

    //data->point.x = x;
    //data->point.y = y;
    //data->state = LV_INDEV_STATE_PRESSED;
    uint16_t rawX =
        ((uint16_t)(touchData[0] & 0x0F) << 8) |
        touchData[1];

    uint16_t rawY =
        ((uint16_t)(touchData[2] & 0x0F) << 8) |
        touchData[3];

    // Validate against the native landscape display.
    if (rawX >= TFT_NATIVE_WIDTH ||
        rawY >= TFT_NATIVE_HEIGHT) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    // Convert native touch coordinates to portrait LVGL coordinates.
    // This is the inverse of the display-flush mapping.
    uint16_t x = rawY;
    uint16_t y = TFT_NATIVE_WIDTH - 1 - rawX;

    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
}

// Initialize FT5206

static void initTouch()
{
    Wire.begin();

    pinMode(FT5206_WAKE, OUTPUT);
    digitalWrite(FT5206_WAKE, HIGH);

    pinMode(FT5206_INT, INPUT);

    delay(100);

    // Return FT5206 to normal operating mode.
    writeTouchRegister(0x00, 0x00);

    delay(20);

    Serial.print("FT5206 status: 0x");
    Serial.println(readTouchRegister(0x02), HEX);
}

// Initialize RA8875

static void initDisplay()
{
    SPI.begin();

    // Teensy 4.1 SPI0:
    // MOSI = 11, MISO = 12, SCK = 13
    tft.begin(RA8875_480x272);

    tft.displayOn(true);
    tft.GPIOX(true);

    // Backlight at full brightness
    tft.PWMout(255, 1);

    tft.fillScreen(RA8875_BLACK);

    Serial.println("RA8875 initialized on SPI0");
}

// Initialize LVGL

static void initLVGL()
{
    lv_init();

    lv_tick_set_cb(millis);

    lvDisplay = lv_display_create(TFT_WIDTH,TFT_HEIGHT);

    lv_display_set_color_format(lvDisplay,LV_COLOR_FORMAT_RGB565);

    lv_display_set_buffers(lvDisplay,drawBuffer,nullptr,sizeof(drawBuffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lvDisplay,displayFlush);

    lvTouch = lv_indev_create();

    lv_indev_set_type(lvTouch,LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb( lvTouch,touchRead);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("Starting Teensy 4.1 RA8875");

    initTouch();
    initDisplay();
    initLVGL();

    // Create the buttons defined in buttons.cpp.
    create_buttons();

    Serial.println("LVGL initialized");
    Serial.println("Touch the on-screen buttons");
}

void loop()
{
    lv_timer_handler();

    // Variable test
    if (variable1 == 1) {
        Serial.println("Button 1 is ON");
    }

    if (variable2 == 1) {
        Serial.println("Button 2 is ON");
    }

    if (variable3 == 1) {
        Serial.println("Button 3 is ON");
    }

    delay(1);
}