// arm_control.h - user arming, the active-low radar enable pin and the auto-disarm timer
#pragma once
#include <stdint.h>

// Drives the enable pin HIGH (radar off, disarmed). Call first thing in setup().
void arm_init();

// Commands (take effect immediately; the pin changes right away).
void arm_request();          // arm, or restart the timer if already armed
void arm_cancel();           // disarm now

// How long one arming lasts. Default ARM_DURATION_MS; the touch display's "Shot Time" setting changes it.
void     arm_set_duration_ms(uint32_t ms);
uint32_t arm_duration_ms();

// Call every loop(): disarms when the arm timer runs out.
void arm_update();

bool     arm_active();
uint32_t arm_ms_left();      // 0 when disarmed
