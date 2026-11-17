#include "arm_control.h"
#include "radar_config.h"
#include "transport.h"

static bool armed = false;
static uint32_t armed_at = 0;

static void announce(const char *msg) {
#if RADAR_DEBUG_SERIAL
  radar_logf("%s", msg);
#else
  (void)msg;
#endif
}

void arm_init() {
  pinMode(PIN_RADAR_ENABLE, OUTPUT);
  digitalWriteFast(PIN_RADAR_ENABLE, HIGH);   // not armed -> radar disabled
  armed = false;
}

void arm_request() {
  armed_at = millis();
  if (!armed) {
    armed = true;
    digitalWriteFast(PIN_RADAR_ENABLE, LOW);  // active-low enable
    announce("ARMED: radar enable pin LOW");
  } else {
    announce("ARMED: timer restarted");
  }
}

void arm_cancel() {
  if (armed) {
    armed = false;
    digitalWriteFast(PIN_RADAR_ENABLE, HIGH);
    announce("DISARMED (command): radar enable pin HIGH");
  }
}

void arm_update() {
  if (armed && (uint32_t)(millis() - armed_at) >= ARM_DURATION_MS) {
    armed = false;
    digitalWriteFast(PIN_RADAR_ENABLE, HIGH);
    announce("DISARMED (timeout): radar enable pin HIGH");
  }
}

bool arm_active() { return armed; }

uint32_t arm_ms_left() {
  if (!armed) return 0;
  uint32_t used = millis() - armed_at;
  return used >= ARM_DURATION_MS ? 0 : ARM_DURATION_MS - used;
}
