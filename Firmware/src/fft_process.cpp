#include "fft_process.h"
#include "radar_config.h"
#include "bandpass.h"

#include <arm_math.h>

static_assert(FFT_SIZE == 1024, "fft_process is set up for 1024 points");

static float window[FFT_SIZE];
static float inbuf[FFT_SIZE];
static arm_rfft_fast_instance_f32 rfft;
static float out_scale;

void fft_init() {
  float sum = 0.0f;
  for (uint32_t n = 0; n < FFT_SIZE; n++) {
    const float x = 2.0f * (float)PI * (float)n / (float)FFT_SIZE;   // periodic (DFT-even) window
#if RADAR_WINDOW == RADAR_WINDOW_BLACKMAN
    window[n] = 0.42f - 0.5f * cosf(x) + 0.08f * cosf(2.0f * x);
#else
    window[n] = 0.5f - 0.5f * cosf(x);
#endif
    sum += window[n];
  }
  // One-sided amplitude: 2 / sum(window); raw counts / 2048 = full scale 1.0.
  out_scale = 2.0f / ((float)(1u << (ADC_BITS - 1)) * sum);
  arm_rfft_fast_init_f32(&rfft, FFT_SIZE);
}

const char *fft_window_name() {
#if RADAR_WINDOW == RADAR_WINDOW_BLACKMAN
  return "Blackman";
#else
  return "Hann";
#endif
}

void fft_process(const AdcBlock &blk, float *out) {
  // Raw counts -> signed samples around mid-scale. This is a FIXED offset (not the block mean),
  // so the stream stays continuous from block to block for the band-pass filter's state.
  const float mid = (float)(1u << (ADC_BITS - 1));
  for (uint32_t n = 0; n < FFT_SIZE; n++) inbuf[n] = (float)blk.s[n] - mid;

  bandpass_process(inbuf, FFT_SIZE);             // no-op when the filter is off
  bandpass_block_done();

  float mean = 0.0f;
#if RADAR_REMOVE_DC
  for (uint32_t n = 0; n < FFT_SIZE; n++) mean += inbuf[n];
  mean /= (float)FFT_SIZE;                       // residual offset (bias not exactly mid-scale)
#endif
  for (uint32_t n = 0; n < FFT_SIZE; n++)
    inbuf[n] = (inbuf[n] - mean) * window[n];

  // Packed output: out[0] = Re(X0), out[1] = Re(X[N/2]), then Re/Im pairs for k = 1..N/2-1.
  arm_rfft_fast_f32(&rfft, inbuf, out, 0);
  out[1] = 0.0f;                                 // drop the Nyquist term, DC imaginary = 0
  for (uint32_t k = 0; k < FFT_SIZE; k++) out[k] *= out_scale;
}
