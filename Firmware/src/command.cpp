#include "command.h"
#include "radar_config.h"
#include "arm_control.h"
#include "transport.h"
#include "adc_acquire.h"
#include "bandpass.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static char buf[40];
static uint8_t len = 0;
static uint32_t last_char_ms = 0;
static int buf_src = SRC_USB;

static void reply(int src, const char *msg) { transport_reply((CmdSource)src, msg); }

static void run(int src) {
  buf[len] = 0;
  len = 0;
  if (!buf[0]) return;
  for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);

  if (!strcmp(buf, "a") || !strcmp(buf, "arm")) {
    arm_request();
    char m[48];
    snprintf(m, sizeof m, "OK armed for %lu ms", (unsigned long)ARM_DURATION_MS);
    reply(src, m);
  } else if (!strcmp(buf, "d") || !strcmp(buf, "disarm")) {
    arm_cancel();
    reply(src, "OK disarmed");
  } else if (!strcmp(buf, "s") || !strcmp(buf, "status") || !strcmp(buf, "?")) {
    char m[64];
    snprintf(m, sizeof m, "%s, %lu ms left, link=%d", arm_active() ? "ARMED" : "DISARMED",
             (unsigned long)arm_ms_left(), (int)transport_eth_connected());
    reply(src, m);
  } else if (!strncmp(buf, "band", 4) && (buf[4] == 0 || buf[4] == ' ')) {
    // band            show the band-pass edges
    // band off        no filtering
    // band LOW HIGH   edges in Hz, 0 = no limit on that side (e.g. "band 3000 300000")
    char m[96];
    const char *args = buf + 4;
    while (*args == ' ') args++;
    if (!*args) {
      bandpass_describe(m, sizeof m);
      reply(src, m);
    } else if (!strcmp(args, "off")) {
      bandpass_off();
      reply(src, "OK band-pass off");
    } else {
      char *e1, *e2;
      const float lo = strtof(args, &e1);
      const float hi = (e1 != args) ? strtof(e1, &e2) : 0.0f;
      if (e1 == args || e2 == e1 || !bandpass_set(lo, hi)) {
        reply(src, "bad band: use  band LOW HIGH  in Hz (0 = no limit), LOW < HIGH, LOW < 0.49*fs");
      } else {
        char d[80];
        bandpass_describe(d, sizeof d);
        snprintf(m, sizeof m, "OK %s", d);
        reply(src, m);
      }
    }
  } else if (!strcmp(buf, "stream usb") || !strcmp(buf, "usb")) {
    if (transport_usb_enable(true)) reply(src, "OK streaming over USB (close the port to stop)");
    else reply(src, "USB streaming is disabled in this build (RADAR_OUT_USB)");
  } else if (!strcmp(buf, "stream off")) {
    transport_usb_enable(false);
    reply(src, "OK USB back to text");
  } else {
    reply(src, "unknown command (arm, disarm, status, band, stream usb, stream off)");
  }
}

void command_feed(int c, int source) {
  if (c < 0) return;
  last_char_ms = millis();
  if (c == '\n' || c == '\r') {
    if (len) run(buf_src);
    return;
  }
  if (len == 0) buf_src = source;
  if (len < sizeof(buf) - 1) buf[len++] = (char)c;
}

void command_poll() {
  if (len && (uint32_t)(millis() - last_char_ms) >= 50) run(buf_src);
}
