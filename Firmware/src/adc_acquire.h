// adc_acquire.h - single-channel, timer-triggered, DMA ping-pong acquisition
#pragma once
#include <stdint.h>

struct AdcBlock {
  const uint16_t *s;     // FFT_SIZE raw 12-bit samples of the Doppler signal (cache already invalidated)
  uint32_t seq;          // running block number
};

bool adc_acquire_begin();

// Non-blocking. Returns true and fills *out when a block is ready. It stays
// valid until adc_acquire_release(), which must happen within one block period
// (FFT_SIZE / SAMPLE_RATE_HZ).
bool adc_acquire_poll(AdcBlock *out);
void adc_acquire_release();

uint32_t adc_acquire_actual_rate_hz();
uint32_t adc_acquire_overruns();        // blocks lost because the consumer was too slow
