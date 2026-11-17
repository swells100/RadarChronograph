// fft_process.h - window + 1024-point REAL FFT (CMSIS-DSP, Cortex-M7 FPU/DSP)
#pragma once
#include <stdint.h>
#include "adc_acquire.h"

void fft_init();

// Turn one raw block into a one-sided complex spectrum. Order: band-pass filter (bandpass.h,
// state carried across blocks, so call this for every block in order), optional mean removal,
// window, real FFT.
// out: FFT_BINS (= N/2) complex floats interleaved (Re, Im), bin k = k * fs / N,
// bin 0 = DC. A window (Hann or Blackman, RADAR_WINDOW) is always applied and its
// gain is compensated: a sine of amplitude A volts reads A / 1.65 V (full scale = 1.0)
// at its bin when it sits on a bin centre.
void fft_process(const AdcBlock &blk, float *out);

// Name of the window in use, for start-up messages.
const char *fft_window_name();
