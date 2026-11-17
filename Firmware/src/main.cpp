// Teensy 4.1 Doppler radar: single ADC channel -> 1024-pt real FFT -> TCP,
// with bullet detection and speed (Doppler).
// The device must be armed by a command (arm_control); the radar enable pin is
// driven LOW while armed and the device disarms itself after ARM_DURATION_MS.
//
// The i.MX RT1062 has one Cortex-M7 core, so:
//   * DMA + QuadTimer + ADC_ETC do ALL the sampling in hardware (zero CPU).
//   * loop() wakes once per 1024-sample block to FFT it, run the bullet
//     detector and queue the result.
//   * transport_service() moves the bytes out over Ethernet (lwIP/QNEthernet) and/or USB serial.
#include <Arduino.h>
#include <string.h>
#include "radar_config.h"
#include "radar_types.h"
#include "adc_acquire.h"
#include "fft_process.h"
#include "bandpass.h"
#include "bullet_detect.h"
#include "transport.h"
#include "arm_control.h"
#include "command.h"

static float spectrum[FFT_SIZE];      // FFT_BINS complex values (DTCM scratch)
static uint32_t warmup_left = WARMUP_BLOCKS;
static uint32_t last_report = 0;
static uint32_t blocks = 0;
static uint32_t max_proc_us = 0;
static uint32_t last_bullets = 0;
static uint16_t sig_min = 0xFFFF, sig_max = 0;   // raw Doppler level seen since the last status line

void setup() {
  // First thing: radar enable pin to its inactive (high) level = disarmed.
  arm_init();

  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) {}

  radar_logf("Teensy 4.1 radar starting");
  fft_init();
  transport_init();
  if (!adc_acquire_begin()) {
    radar_logf("ADC init failed");
    while (true) {}
  }
  detect_init(adc_acquire_actual_rate_hz());
  bandpass_init((float)adc_acquire_actual_rate_hz());
  radar_logf("fs=%lu Hz  N=%lu  bin=%.2f Hz  signal pin=%d  window=%s",
                (unsigned long)adc_acquire_actual_rate_hz(), (unsigned long)FFT_SIZE,
                (double)adc_acquire_actual_rate_hz() / FFT_SIZE, PIN_SIGNAL, fft_window_name());
  { char b[96]; bandpass_describe(b, sizeof b); radar_logf("%s  (fs/2 = %.0f Hz)", b, (double)(adc_acquire_actual_rate_hz() * 0.5f)); }
#if RADAR_OUT_ETH
  radar_logf("Ethernet: Teensy %u.%u.%u.%u  ->  PC %u.%u.%u.%u port %u (the PC must have that address and be listening)",
             NET_LOCAL_IP[0], NET_LOCAL_IP[1], NET_LOCAL_IP[2], NET_LOCAL_IP[3],
             NET_DEST_IP[0], NET_DEST_IP[1], NET_DEST_IP[2], NET_DEST_IP[3], (unsigned)NET_DEST_PORT);
#endif
  radar_logf("DISARMED at power-up. Send \"arm\" (or \"a\") over USB serial or the network link to arm for %lu s.\n",
                (unsigned long)(ARM_DURATION_MS / 1000));
  radar_logf("speed resolution %.2f m/s per bin, max unambiguous speed %.0f m/s\n",
                (double)(adc_acquire_actual_rate_hz() / (float)FFT_SIZE * DOPPLER_MPS_PER_HZ),
                (double)(adc_acquire_actual_rate_hz() * 0.5f * DOPPLER_MPS_PER_HZ));
}

void loop() {
  transport_service();

  // Commands from USB serial and from the PC over TCP.
  { CmdSource src; for (int c; (c = transport_read_byte(&src)) >= 0;) command_feed(c, src); }
  command_poll();
  arm_update();                         // auto-disarm after ARM_DURATION_MS

  AdcBlock blk;
  if (adc_acquire_poll(&blk)) {
    if (warmup_left) {
      warmup_left--;
    } else {
      uint32_t t = micros();

      // The detector must see every block, even when nothing can be sent.
      const bool armed = arm_active();
      for (uint32_t i = 0; i < FFT_SIZE; i++) {
        if (blk.s[i] < sig_min) sig_min = blk.s[i];
        if (blk.s[i] > sig_max) sig_max = blk.s[i];
      }
      fft_process(blk, spectrum);
      DetectResult d = detect_update(spectrum, armed && !bandpass_settling());   // skip the filter start-up transient
      last_bullets = d.bullet_count;
      if (armed) d.flags |= FLAG_ARMED;

      uint8_t *slot = transport_acquire_slot();
      if (slot) {
        RadarHeader *h = (RadarHeader *)slot;
        h->magic = RADAR_MAGIC;
        h->seq = blk.seq;
        h->fft_size = FFT_SIZE;
        h->sample_rate_hz = adc_acquire_actual_rate_hz();
        h->flags = d.flags | (RADAR_DEBUG_WAVEFORM ? FLAG_HAS_WAVE : 0u);
        h->armed_ms_left = arm_ms_left();
        h->bullet_count = d.bullet_count;
        h->track_freq_hz = d.freq_hz;
        h->track_mag = d.mag;
        h->track_speed_mps = d.speed_mps;
        memcpy(slot + sizeof(RadarHeader), spectrum, SPECTRUM_BYTES);
#if RADAR_DEBUG_WAVEFORM
        {
          uint8_t *w = slot + sizeof(RadarHeader) + SPECTRUM_BYTES;
          WaveInfo wi = {};
          wi.adc_bits = ADC_BITS;
          memcpy(w, &wi, sizeof(wi));
          memcpy(w + sizeof(wi), blk.s, FFT_SIZE * sizeof(uint16_t));
        }
#endif
        transport_commit();
      }

      if (d.flags & FLAG_NEW_BULLET)
        radar_logf("BULLET #%lu  f=%.0f Hz  v=%.1f m/s (%.0f ft/s)  mag=%.4f\n",
                      (unsigned long)d.bullet_count,
                      (double)d.freq_hz, (double)d.speed_mps, (double)(d.speed_mps * 3.28084f),
                      (double)d.mag);

      uint32_t dt = micros() - t;
      if (dt > max_proc_us) max_proc_us = dt;
      blocks++;
    }
    adc_acquire_release();
  }

  if (millis() - last_report >= 2000) {
    last_report = millis();
    const float v_per_count = 3.3f / (float)((1u << ADC_BITS) - 1);
    radar_logf("%s %lus left  blocks=%lu sent=%lu dropped=%lu overrun=%lu bullets=%lu siglvl=%.2f..%.2fV link=%d conn=%d tries=%lu maxproc=%luus\n",
                  arm_active() ? "ARMED" : "disarmed", (unsigned long)((arm_ms_left() + 999) / 1000),
                  (unsigned long)blocks, (unsigned long)transport_sent_packets(),
                  (unsigned long)transport_dropped(), (unsigned long)adc_acquire_overruns(),
                  (unsigned long)last_bullets,
                  (double)(sig_min * v_per_count), (double)(sig_max * v_per_count),
                  (int)transport_eth_link(), (int)transport_eth_connected(),
                  (unsigned long)transport_eth_attempts(), (unsigned long)max_proc_us);
    max_proc_us = 0;
    sig_min = 0xFFFF;
    sig_max = 0;
  }
}
