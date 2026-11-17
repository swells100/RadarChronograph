// bandpass.h - Butterworth band-pass applied to the sample stream before windowing/FFT.
//
// Built from a high-pass (lower band edge) and a low-pass (upper band edge), each of
// FILTER_ORDER poles, as cascaded second-order sections. The -3 dB points are exactly the
// band edges. The filter keeps its state from block to block (the stream is continuous).
//
// The edges are variables: defaults FILTER_LOW_HZ / FILTER_HIGH_HZ in radar_config.h,
// changeable at run time with the "band" command (see command.h).
//   * an edge of 0 means "no limit" on that side
//   * an upper edge at or above 0.49 * fs cannot be realised (fs/2 is the limit of a
//     sampled signal), so the low-pass is skipped and only the high-pass remains
//   * this is a DIGITAL filter: it cannot remove energy that has already aliased, so
//     keep an analog anti-alias low-pass in front of the ADC (see README)
#pragma once
#include <stdint.h>
#include <stddef.h>

void bandpass_init(float sample_rate_hz);

// Set the edges in Hz (0 = no limit). Returns false (and changes nothing) if they are
// invalid: negative, low >= high, or low edge at/above 0.49 * fs.
bool bandpass_set(float low_hz, float high_hz);

// Bypass the filter completely (same as bandpass_set(0, 0)).
void bandpass_off();

// Filter n samples in place (continues from the previous call).
void bandpass_process(float *x, uint32_t n);

// Effective edges actually in use (0 = no limit on that side).
void bandpass_edges(float *low_hz, float *high_hz);

// True for the first blocks after start-up or a change of edges, while the filter's
// start-up transient is still in the output. The detector ignores these blocks.
bool bandpass_settling();
void bandpass_block_done();      // call once per processed block

// Human readable summary, e.g. "band-pass 3000..125000 Hz (upper edge skipped: above 0.49*fs)".
void bandpass_describe(char *buf, size_t n);
