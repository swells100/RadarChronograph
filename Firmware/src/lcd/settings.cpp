// Adapted from swells100/RadarChronograph, Teensy_RA8875 (LVGL touch UI on an RA8875 480x272 display).
#include "../radar_config.h"
#if RADAR_LCD
#include <Arduino.h>
#include "../transport.h"
#include <EEPROM.h>
#include "settings.h"
#include "buttons.h"

static const uint32_t SETTINGS_MAGIC = 0x5445454E;
static const uint8_t SETTINGS_VERSION = 1;

void save_settings()
{
    SavedSettings settings;

    settings.magic = SETTINGS_MAGIC;
    settings.version = SETTINGS_VERSION;
    settings.portrait = Portrait;
    settings.brightness = ScreenBrightness;
    settings.shotTime = ShotTime;

    EEPROM.put(0, settings);

    radar_logf("Settings saved");
}

void load_settings()
{
    SavedSettings settings;

    EEPROM.get(0, settings);

    if (settings.magic != SETTINGS_MAGIC ||
        settings.version != SETTINGS_VERSION)
    {
        Portrait = 0;
        ScreenBrightness = 100;
        ShotTime = 10;

        save_settings();
        return;
    }

    Portrait = settings.portrait ? 1 : 0;

    ScreenBrightness = constrain(settings.brightness, 0, 100);

    ShotTime = constrain(settings.shotTime, 5, 90);

    radar_logf("Settings loaded");
}
#endif  // RADAR_LCD
