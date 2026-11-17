#include "bullet_detect.h"
#include "radar_config.h"
#include "radar_types.h"

#include <math.h>

static float fs_hz;
static float thr2;                    // detection threshold, squared magnitude

static bool tracking = false;
static uint32_t cand_bin = 0;         // SEARCH: bin of the peak seen in the previous block(s)
static uint32_t confirm = 0;          // SEARCH: consecutive blocks with a peak near cand_bin
static uint32_t fund_bin = 0;         // TRACK: followed peak
static uint32_t lost = 0;             // TRACK: blocks without the peak
static uint32_t bullets = 0;
static float m2[FFT_BINS];

void detect_init(uint32_t sample_rate_hz) {
  fs_hz = (float)sample_rate_hz;
  float thr = powf(10.0f, DET_MIN_DBFS / 20.0f);
  thr2 = thr * thr;
}

// Index of the strongest bin in [lo, hi] (inclusive, clipped to the valid range).
static uint32_t argmax(uint32_t lo, uint32_t hi) {
  if (lo < DET_MIN_BIN) lo = DET_MIN_BIN;
  if (hi > FFT_BINS - 2) hi = FFT_BINS - 2;
  uint32_t best = lo;
  for (uint32_t k = lo; k <= hi; k++)
    if (m2[k] > m2[best]) best = k;
  return best;
}

// Fractional peak position from the neighbouring bins: parabola through the log
// magnitudes (accurate for Hann and Blackman main lobes alike).
static float refine(uint32_t k) {
  const float eps = 1e-20f;
  float a = 0.5f * logf(m2[k - 1] + eps), b = 0.5f * logf(m2[k] + eps), c = 0.5f * logf(m2[k + 1] + eps);
  float d = a - 2.0f * b + c;
  if (d >= 0.0f) return (float)k;       // not a local maximum shape
  float delta = 0.5f * (a - c) / d;
  if (delta > 0.5f) delta = 0.5f;
  if (delta < -0.5f) delta = -0.5f;
  return (float)k + delta;
}

static uint32_t absdiff(uint32_t a, uint32_t b) { return a > b ? a - b : b - a; }

DetectResult detect_update(const float *spec, bool armed) {
  DetectResult r = {};
  r.bullet_count = bullets;

  if (!armed) {                          // disarmed: drop everything in progress
    tracking = false;
    confirm = 0;
    return r;
  }

  for (uint32_t k = DET_MIN_BIN - 1; k < FFT_BINS; k++)
    m2[k] = spec[2 * k] * spec[2 * k] + spec[2 * k + 1] * spec[2 * k + 1];

  if (!tracking) {
    // SEARCH: strongest peak above the threshold, present in DET_CONFIRM_BLOCKS blocks in a row.
    uint32_t kmax = argmax(DET_MIN_BIN, FFT_BINS - 2);
    if (m2[kmax] > thr2) {
      confirm = (confirm && absdiff(kmax, cand_bin) <= DET_TRACK_BINS) ? confirm + 1 : 1;
      cand_bin = kmax;
      if (confirm >= DET_CONFIRM_BLOCKS) {
        tracking = true;
        fund_bin = kmax;
        lost = 0;
        bullets++;
        r.bullet_count = bullets;
        r.flags |= FLAG_NEW_BULLET;
      }
    } else {
      confirm = 0;
    }
  } else {
    // TRACK: follow the neighbourhood of the peak as it decelerates.
    uint32_t lo = fund_bin > DET_TRACK_BINS ? fund_bin - DET_TRACK_BINS : 0;
    uint32_t k = argmax(lo, fund_bin + DET_TRACK_BINS);
    if (m2[k] > thr2) {
      fund_bin = k;
      lost = 0;
    } else if (++lost >= DET_LOST_BLOCKS) {
      tracking = false;                  // dead time over: look for the next bullet
      confirm = 0;
    }
  }

  if (tracking && lost == 0) {
    r.flags |= FLAG_TRACKING;
    r.freq_hz = refine(fund_bin) * fs_hz / FFT_SIZE;
    r.mag = sqrtf(m2[fund_bin]);
    r.speed_mps = r.freq_hz * DOPPLER_MPS_PER_HZ;
  }
  return r;
}
