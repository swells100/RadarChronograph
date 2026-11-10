#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

struct SavedSettings
{
    uint32_t magic;
    uint8_t version;
    uint8_t portrait;
    uint8_t brightness;
    uint16_t shotTime;
};

void save_settings();
void load_settings();

#endif