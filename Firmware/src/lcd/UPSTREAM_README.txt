Explaination of set up, varaibles, and functions

------------------------------------------------------------------------------------------------------------------
At the moment in setup()
------------------------------------------------------------------------------------------------------------------
-load_settings(); 
Loads settings that are saved to Teensy EEPROM before lvgl startup. This includes screen brightness, screen orientation, and shot time
-screen_init();
This is the main screen set up
-create_buttons();
This is the lvgl GUI set up

------------------------------------------------------------------------------------------------------------------
At the moment in loop()
------------------------------------------------------------------------------------------------------------------

-lv_timer_handler();
This is required for the lvgl timers and needs to run periodically. This needs to run at most, every 5 ms, don't let interrupts of program interrupt this
Example:
void loop() {
    lv_timer_handler();
    delay(5);
}
-screen_process_orientation();
This checks if orientation needs to be updated and updates if it is true. It should run in a loop

------------------------------------------------------------------------------------------------------------------
At the moment this is what can be used to read and control screen
------------------------------------------------------------------------------------------------------------------

-"ARM" 
Variable that can be read to determine if ARM button has been ARMED (True means ARMED state and False means not ARMED state)
-set_arm_state(false) 
Function to control the value of the ARM variable, the screen will update based on current ARM value
-"ShotTime" 
Variable that can be read to determine the currently set shot time, this is controlled with the slider in the GUI settings screen
-main_screen_update_values(currentShotNumber, currentVelocity)
Function to control displayed shot number and velocity of shot on main display
-"Portrait"
Variable that can be read to determine if screen is in portrait mode, this is mostly a bug fixing variable
