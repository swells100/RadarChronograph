#include "shot_summary.h"
#include "radar_types.h"

static float min_speed = 6.0f;
static bool active = false;
static uint32_t gap = 0, n = 0;
static ShotResult best;

void shot_init(float min_speed_mps) { min_speed = min_speed_mps; active = false; gap = 0; n = 0; }

static bool finish(ShotResult *out) {
  const bool ok = active && n > 0 && best.speed_mps >= min_speed;
  if (ok && out) { *out = best; out->blocks = n; }
  active = false; gap = 0; n = 0;
  return ok;
}

bool shot_update(uint32_t flags, float speed, float mag, float freq, bool armed, ShotResult *out) {
  if (!(flags & FLAG_ARMED) && !armed) return finish(out);        // disarmed: close whatever was in progress
  bool done = false;
  if (active && (flags & FLAG_NEW_BULLET)) done = finish(out);    // a second bullet started
  if (flags & FLAG_TRACKING) {
    if (!active) { active = true; gap = 0; n = 0; best.mag = -1.0f; }
    gap = 0;
    if (mag > best.mag) { best.speed_mps = speed; best.freq_hz = freq; best.mag = mag; }
    n++;
  } else if (active) {
    if (++gap >= SHOT_END_GAP && !done) done = finish(out);
    else if (gap >= SHOT_END_GAP) { active = false; gap = 0; n = 0; }   // already reported the first one
  }
  return done;
}

bool shot_active() { return active; }
