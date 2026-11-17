// radar_types.h - wire format shared with tools/receiver_plot.py
#pragma once
#include <stdint.h>
#include "radar_config.h"

static constexpr uint32_t RADAR_MAGIC = 0x33444152u;  // "RAD3" little-endian

// Flag bits
static constexpr uint32_t FLAG_ARMED       = 1u << 0;  // user armed the device (radar enable pin LOW)
static constexpr uint32_t FLAG_TRACKING    = 1u << 1;  // a bullet is being tracked in this block
static constexpr uint32_t FLAG_NEW_BULLET  = 1u << 2;  // bullet first detected in this block
static constexpr uint32_t FLAG_HAS_WAVE    = 1u << 4;  // raw waveform trailer follows the spectrum

// 40-byte little-endian header followed by FFT_BINS complex float32 (Re, Im).
struct __attribute__((packed)) RadarHeader {
  uint32_t magic;            // RADAR_MAGIC
  uint32_t seq;              // block counter (gaps = dropped packets)
  uint32_t fft_size;         // real input samples per block (spectrum has fft_size/2 bins)
  uint32_t sample_rate_hz;   // actual rate of the sample timer
  uint32_t flags;            // FLAG_*
  uint32_t bullet_count;     // bullets detected since power-up
  float    track_freq_hz;    // Doppler frequency of the tracked bullet, 0 if none
  float    track_mag;        // its magnitude, full scale = 1.0, 0 if none
  float    track_speed_mps;  // radial speed of the tracked bullet (Doppler), 0 if none
  uint32_t armed_ms_left;    // milliseconds until auto-disarm, 0 when disarmed
};
static_assert(sizeof(RadarHeader) == 40, "header must be 40 bytes");

// Optional debug trailer (when FLAG_HAS_WAVE is set), directly after the spectrum:
//   WaveInfo, then FFT_SIZE uint16 raw samples of the Doppler channel (same time span).
struct __attribute__((packed)) WaveInfo {
  uint8_t adc_bits;        // 12
  uint8_t reserved[3];
};
static_assert(sizeof(WaveInfo) == 4, "WaveInfo must be 4 bytes");

static constexpr uint32_t SPECTRUM_BYTES = FFT_BINS * 2 * sizeof(float);
#if RADAR_DEBUG_WAVEFORM
static constexpr uint32_t WAVE_BYTES = sizeof(WaveInfo) + FFT_SIZE * sizeof(uint16_t);
#else
static constexpr uint32_t WAVE_BYTES = 0;
#endif
static constexpr uint32_t PACKET_BYTES = sizeof(RadarHeader) + SPECTRUM_BYTES + WAVE_BYTES;
