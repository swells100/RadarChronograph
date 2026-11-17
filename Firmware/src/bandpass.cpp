#include "bandpass.h"
#include "radar_config.h"
#include <math.h>
#include <stdio.h>

static constexpr int MAX_STAGES = 8;           // 4 sections per edge (order 8) x 2 edges
// Counted down inside fft_process, and checked by main after it, so N here means N-1 blocks are held back.
static constexpr uint32_t SETTLE_BLOCKS = 3;

struct Biquad { float b0, b1, b2, a1, a2, z1, z2; };

static Biquad st[MAX_STAGES];
static int   nst = 0;
static float fs_hz = 250000.0f;
static float eff_low = 0.0f, eff_high = 0.0f;  // edges in use (0 = none)
static bool  high_skipped = false;
static uint32_t settle = SETTLE_BLOCKS;

static constexpr float NYQ_LIMIT = 0.49f;      // highest usable edge as a fraction of fs

// Order-N Butterworth as N/2 sections (N even). RBJ bilinear design with pre-warping, so the
// -3 dB point lands exactly on fc. Section k has Q = 1 / (2 sin((2k+1)*pi/(2N))).
static void add_sections(bool highpass, float fc, int order) {
  const double w0 = 2.0 * M_PI * (double)fc / (double)fs_hz;
  const double cw = cos(w0), sw = sin(w0);
  for (int k = 0; k < order / 2 && nst < MAX_STAGES; k++) {
    const double q = 1.0 / (2.0 * sin((2.0 * k + 1.0) * M_PI / (2.0 * order)));
    const double alpha = sw / (2.0 * q);
    double b0, b1, b2;
    if (highpass) { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; }
    else          { b0 = (1 - cw) / 2; b1 =  (1 - cw); b2 = (1 - cw) / 2; }
    const double a0 = 1 + alpha, a1 = -2 * cw, a2 = 1 - alpha;
    Biquad &s = st[nst++];
    s.b0 = (float)(b0 / a0); s.b1 = (float)(b1 / a0); s.b2 = (float)(b2 / a0);
    s.a1 = (float)(a1 / a0); s.a2 = (float)(a2 / a0);
    s.z1 = s.z2 = 0.0f;
  }
}

bool bandpass_set(float low_hz, float high_hz) {
  if (!(low_hz >= 0.0f) || !(high_hz >= 0.0f)) return false;          // also rejects NaN
  if (low_hz >= NYQ_LIMIT * fs_hz) return false;
  const bool use_high = high_hz > 0.0f && high_hz < NYQ_LIMIT * fs_hz;
  const bool skipped = high_hz > 0.0f && !use_high;
  if (use_high && low_hz > 0.0f && low_hz >= high_hz) return false;

  nst = 0;
  int order = FILTER_ORDER;
  if (order < 2) order = 2;
  if (order > 8) order = 8;
  order &= ~1;                                  // even orders only
  if (low_hz > 0.0f) add_sections(true, low_hz, order);
  if (use_high)      add_sections(false, high_hz, order);
  eff_low = low_hz > 0.0f ? low_hz : 0.0f;
  eff_high = use_high ? high_hz : 0.0f;
  high_skipped = skipped;
  settle = SETTLE_BLOCKS;
  return true;
}

void bandpass_off() { bandpass_set(0.0f, 0.0f); }

void bandpass_init(float sample_rate_hz) {
  fs_hz = sample_rate_hz;
#if RADAR_BANDPASS
  if (!bandpass_set(FILTER_LOW_HZ, FILTER_HIGH_HZ)) bandpass_off();    // bad defaults: run unfiltered
#else
  bandpass_off();
#endif
}

void bandpass_process(float *x, uint32_t n) {
  for (int s = 0; s < nst; s++) {
    Biquad &f = st[s];
    float z1 = f.z1, z2 = f.z2;
    for (uint32_t i = 0; i < n; i++) {          // transposed direct form II
      const float in = x[i];
      const float y = f.b0 * in + z1;
      z1 = f.b1 * in - f.a1 * y + z2;
      z2 = f.b2 * in - f.a2 * y;
      x[i] = y;
    }
    f.z1 = z1; f.z2 = z2;
  }
}

void bandpass_edges(float *low_hz, float *high_hz) { *low_hz = eff_low; *high_hz = eff_high; }

bool bandpass_settling() { return nst > 0 && settle > 0; }
void bandpass_block_done() { if (settle) settle--; }

void bandpass_describe(char *buf, size_t n) {
  if (nst == 0) { snprintf(buf, n, "band-pass OFF"); return; }
  const char *note = high_skipped ? " (upper edge above 0.49*fs: low-pass skipped)" : "";
  if (eff_low > 0 && eff_high > 0)
    snprintf(buf, n, "band-pass %.0f..%.0f Hz%s", (double)eff_low, (double)eff_high, note);
  else if (eff_low > 0)
    snprintf(buf, n, "high-pass %.0f Hz%s", (double)eff_low, note);
  else
    snprintf(buf, n, "low-pass %.0f Hz", (double)eff_high);
}
