#include <Arduino.h>
#include <lvgl.h>

#include "screen.h"
#include "buttons.h"
#include "settings.h"
#include "main_screen.h"

int previousShotTime;
int previousPortrait;
int previousARM;
int currentShotNumber = 0;
int currentVelocity = 10;
void setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println("Starting Teensy 4.1 RA8875");

    // Restore settings from EEPROM
    load_settings();

    previousShotTime = ShotTime; // shot time check
    previousPortrait = Portrait;
    previousARM = ARM;

    // Initialize display using restored settings
    screen_init();

    // Create LVGL buttons and screens
    create_buttons();
    
}

void loop()
{
    lv_timer_handler();

    screen_process_orientation(); //wait until orientation is processed

    // Variable test
    if (ARM != previousARM)
    {
        Serial.println(ARM ? "ARM is ON" : "ARM is OFF");
        previousARM = ARM;
        currentShotNumber++;
        currentVelocity = 10*currentShotNumber; // Example velocity value
        main_screen_update_values(currentShotNumber, currentVelocity);
    }
       
    if (Portrait != previousPortrait)
    {
        Serial.println(Portrait ? "Portrait mode is ON" : "Portrait mode is OFF");
        previousPortrait = Portrait;
        //set_arm_state(false);
        //main_screen_update_values(currentShotNumber, currentVelocity);
    }

    //if (variable3 == 1)
    //    Serial.println("Button 3 is ON");

    if (ShotTime != previousShotTime)
    {
        Serial.print("Shot Time updated: ");
        Serial.print(ShotTime);
        Serial.println(" seconds");

        previousShotTime = ShotTime;
    }

    delay(1);
}