// bullet_detect.h - bullet detection on the FFT spectrum (only while armed)
#pragma once
#include <stdint.h>

struct DetectResult {
  uint32_t flags;          // FLAG_TRACKING / FLAG_NEW_BULLET from radar_types.h
  uint32_t bullet_count;
  float    freq_hz;        // tracked Doppler frequency, 0 if none
  float    mag;            // its magnitude (full scale = 1.0), 0 if none
  float    speed_mps;      // radial speed from the Doppler shift, 0 if none
};

void detect_init(uint32_t sample_rate_hz);

// Call once per processed block, in order, with that block's one-sided spectrum
// (FFT_BINS complex floats, Re/Im interleaved). While `armed` is false nothing is
// detected and any tracking is dropped.
DetectResult detect_update(const float *spectrum, bool armed);
