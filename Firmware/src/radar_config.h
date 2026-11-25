// radar_config.h - all tunables in one place.
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Analog input (Teensy 4.1): the Doppler signal on A10 = pin 24 (ADC1 peripheral,
// library name adc0). It must stay within 0..3.3 V (NOT 5 V tolerant).
// ---------------------------------------------------------------------------
static constexpr uint8_t PIN_SIGNAL = A10;

// ---------------------------------------------------------------------------
// Arming and the radar enable output
//   The device is DISARMED at power-up. The user arms it with a command (USB serial
//   or the network link, see README: "arm" / "a"). While armed:
//     * PIN_RADAR_ENABLE is driven LOW (active-low enable: the radar is on), and
//     * bullets are detected in the spectrum.
//   After ARM_DURATION_MS the device disarms itself (pin goes HIGH, radar off).
//   "disarm" / "d" disarms immediately; "arm" while armed restarts the timer.
//   The pin is HIGH whenever the device is not armed, including during reset
//   (add an external pull-up so it also reads HIGH while the Teensy boots).
//   3.3 V logic, GPIO drive only (a few mA): buffer it if it must drive a load.
// ---------------------------------------------------------------------------
static constexpr uint8_t  PIN_RADAR_ENABLE = 10;
static constexpr uint32_t ARM_DURATION_MS  = 10000;

// ---------------------------------------------------------------------------
// Debugging
//   RADAR_DEBUG_WAVEFORM 1 appends the raw time-domain samples to every packet
//   (+2 KB per packet) so tools/receiver_plot.py can show the incoming waveform.
//   Set to 0 for the leanest stream (spectrum only).
//   RADAR_DEBUG_SERIAL 1 prints ARMED / DISARMED / BULLET events to the USB serial port.
// ---------------------------------------------------------------------------
#define RADAR_DEBUG_WAVEFORM 1
#define RADAR_DEBUG_SERIAL   1

// ---------------------------------------------------------------------------
// Sampling
// ---------------------------------------------------------------------------
static constexpr uint32_t FFT_SIZE        = 1024;     // real samples per block; must be 1024
static constexpr uint32_t SAMPLE_RATE_HZ  = 250000;   // see README for ceiling
static constexpr uint8_t  ADC_BITS        = 12;
static constexpr uint8_t  ADC_AVERAGING   = 1;        // 1 = off. More averaging lowers max rate.
#define RADAR_CONV_SPEED  ADC_CONVERSION_SPEED::HIGH_SPEED
#define RADAR_SAMP_SPEED  ADC_SAMPLING_SPEED::HIGH_SPEED

// Blocks discarded after start-up (settling / stray first conversions).
static constexpr uint32_t WARMUP_BLOCKS = 3;

static constexpr uint32_t FFT_BINS = FFT_SIZE / 2;    // bins sent: 0 .. fs/2 (exclusive)

// ---------------------------------------------------------------------------
// Signal processing
//   A window is ALWAYS applied before the FFT (a rectangular window leaks badly
//   when the tone is not exactly on a bin). Choose one:
//     RADAR_WINDOW_HANN      : narrower main lobe (4 bins wide), first side lobe -31 dB
//     RADAR_WINDOW_BLACKMAN  : wider main lobe (6 bins wide), side lobes -58 dB (default)
//   Both use the periodic ("DFT-even") form, the right one for spectral analysis.
//   Amplitude scaling compensates the window's gain, so a sine of A volts reads A / 1.65 V.
// ---------------------------------------------------------------------------
#define RADAR_WINDOW_HANN      0
#define RADAR_WINDOW_BLACKMAN  1
#define RADAR_WINDOW           RADAR_WINDOW_BLACKMAN

// ---------------------------------------------------------------------------
// Touch display (RA8875 480x272 + FT5206 touch + LVGL), from swells100/RadarChronograph Teensy_RA8875
//   RA8875: CS pin 8, RESET pin 9, SPI0 = MOSI 11 / MISO 12 / SCK 13
//   FT5206 touch: I2C (Wire: SDA 18, SCL 19), as wired by the original project
//   RADAR_LCD 0 leaves the display code out of the build.
// The UI's "Shot Time" setting (5-90 s) is the arm time; ARM_DURATION_MS below is only the default when no display.
// ---------------------------------------------------------------------------
#define RADAR_LCD 1
static constexpr uint32_t LCD_SERVICE_MS   = 5;      // run LVGL at most this often
static constexpr uint32_t LCD_MAX_DEFER_MS = 400;    // never hold the screen back longer than this while a shot is tracked
static constexpr float    SHOT_MIN_SPEED_MPS = 6.0f; // tracks slower than this are not reported as shots

#define RADAR_REMOVE_DC   1   // subtract per-block mean (removes the 1.65 V bias)

// ---------------------------------------------------------------------------
// Band-pass filter on the samples before the window and FFT (Butterworth, see bandpass.h).
//   FILTER_LOW_HZ  : lower band edge, -3 dB point (0 = no high-pass)
//   FILTER_HIGH_HZ : upper band edge, -3 dB point (0 = no low-pass)
//   Both are only the START-UP values: "band LOW HIGH" changes them while running.
//   An upper edge at or above 0.49 * sample rate cannot exist in sampled data (fs/2 = 125 kHz at
//   250 kS/s), so the low-pass is skipped and a note is printed at start-up.
//   FILTER_ORDER   : poles per edge, even 2..8 (4 = 24 dB/octave per edge)
// ---------------------------------------------------------------------------
#define RADAR_BANDPASS 1
static constexpr float   FILTER_LOW_HZ  = 3000.0f;
static constexpr float   FILTER_HIGH_HZ = 300000.0f;
static constexpr uint8_t FILTER_ORDER   = 4;

// ---------------------------------------------------------------------------
// Radar
// ---------------------------------------------------------------------------
static constexpr float RADAR_CARRIER_HZ = 24.125e9f;   // transmit frequency
static constexpr float LIGHT_SPEED_MPS  = 299792458.0f;
// Doppler shift -> radial speed: v = f_d * c / (2 * f_carrier)  (about 161 Hz per m/s)
static constexpr float DOPPLER_MPS_PER_HZ = LIGHT_SPEED_MPS / (2.0f * RADAR_CARRIER_HZ);

// ---------------------------------------------------------------------------
// Bullet detection (only while armed)
//
// SEARCH: the strongest spectral peak above DET_MIN_DBFS (ignoring the lowest
//   DET_MIN_BIN bins) seen in DET_CONFIRM_BLOCKS consecutive blocks is a bullet.
//   It is counted once and the detector then follows that peak (TRACK).
// TRACK: follow the peak within +/- DET_TRACK_BINS per block (it slows down). When it
//   has been missing for DET_LOST_BLOCKS blocks the detector returns to SEARCH, so
//   DET_LOST_BLOCKS also acts as the dead time before the next bullet can count.
// A block is 1024 samples (4.1 ms at 250 kS/s), so a fast bullet crossing a narrow beam
// may only be present in one block: keep DET_CONFIRM_BLOCKS at 1 unless noise triggers it.
// ---------------------------------------------------------------------------
static constexpr float    DET_MIN_DBFS       = -50.0f; // peak must exceed this to count as a bullet
static constexpr uint32_t DET_MIN_BIN        = 4;      // ignore the lowest bins (DC / bias residue)
static constexpr uint32_t DET_CONFIRM_BLOCKS = 1;      // consecutive blocks with a peak before a bullet counts
static constexpr uint32_t DET_TRACK_BINS     = 12;     // follow the peak +/- this many bins per block
static constexpr uint32_t DET_LOST_BLOCKS    = 8;      // blocks without the peak before searching again

// ---------------------------------------------------------------------------
// Where the data goes
//   RADAR_OUT_ETH 1  stream over Ethernet (TCP client to NET_DEST_IP). Set to 0 to leave the
//                    Ethernet stack completely out, e.g. for bench tests with no cable.
//   RADAR_OUT_USB 1  allow streaming the same packets over the USB serial port. The device
//                    keeps printing readable text on USB until a PC tool sends the command
//                    "stream usb" (tools/receiver_plot.py --serial does this); it goes
//                    back to text when that tool closes the port. Both outputs can be used at once.
// ---------------------------------------------------------------------------
#define RADAR_OUT_ETH 1
#define RADAR_OUT_USB 1

// ---------------------------------------------------------------------------
// Network
//   Use a subnet that no other interface of the PC (Wi-Fi, VPN, Docker) is already on:
//   the common 192.168.1.x often is, and then the PC does not answer the Teensy's ARP on the cable.
//   The PC's wired port needs the static address NET_DEST_IP (/24), e.g.
//     sudo ip addr add 192.168.77.100/24 dev <wired interface>
// ---------------------------------------------------------------------------
#define RADAR_USE_DHCP 0
static const uint8_t  NET_LOCAL_IP[4] = {192, 168, 77, 50};
static const uint8_t  NET_MASK[4]     = {255, 255, 255, 0};
static const uint8_t  NET_GATEWAY[4]  = {192, 168, 77, 1};
static const uint8_t  NET_DEST_IP[4]  = {192, 168, 77, 100};  // PC running receiver_plot.py
static constexpr uint16_t NET_DEST_PORT = 5000;
static constexpr uint32_t NET_RECONNECT_MS = 1000;

// Outgoing packet ring (each slot = PACKET_BYTES, kept in RAM2)
static constexpr uint32_t TX_SLOTS = 8;
