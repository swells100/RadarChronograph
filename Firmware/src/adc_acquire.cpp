#include "adc_acquire.h"
#include "radar_config.h"

#include <ADC.h>
#include <AnalogBufferDMA.h>
#include <DMAChannel.h>

#if !(defined(ADC_USE_DMA) && defined(ADC_USE_TIMER))
#error "ADC library must have DMA and timer support (Teensy 4.x)"
#endif

// DMA buffers live in RAM2 (DMAMEM), 32-byte aligned for cache maintenance.
DMAMEM static volatile uint16_t __attribute__((aligned(32))) buf_a[FFT_SIZE];
DMAMEM static volatile uint16_t __attribute__((aligned(32))) buf_b[FFT_SIZE];

static ADC *adc = nullptr;
static AnalogBufferDMA dma(buf_a, FFT_SIZE, buf_b, FFT_SIZE);

static uint32_t last_count = 0;
static uint32_t overruns = 0;
static uint32_t actual_rate = SAMPLE_RATE_HZ;
static bool holding = false;

bool adc_acquire_begin() {
  adc = new ADC();

  pinMode(PIN_SIGNAL, INPUT_DISABLE);   // disable digital pad on the analog pin

  adc->adc0->setAveraging(ADC_AVERAGING);
  adc->adc0->setResolution(ADC_BITS);
  adc->adc0->setConversionSpeed(RADAR_CONV_SPEED);
  adc->adc0->setSamplingSpeed(RADAR_SAMP_SPEED);

  dma.init(adc, ADC_0);

  adc->adc0->startSingleRead(PIN_SIGNAL);   // selects the channel before the timer takes over
  adc->adc0->startTimer(SAMPLE_RATE_HZ);

  actual_rate = adc->adc0->getTimerFrequency();
  if (actual_rate == 0) actual_rate = SAMPLE_RATE_HZ;
  last_count = 0;
  return true;
}

bool adc_acquire_poll(AdcBlock *out) {
  if (holding || !dma.interrupted()) return false;

  uint32_t c = dma.interruptCount();
  if (last_count && c - last_count > 1) overruns += c - last_count - 1;
  last_count = c;

  const volatile uint16_t *p = dma.bufferLastISRFilled();
  // DMA wrote to RAM2 behind the cache: drop stale cache lines before reading.
  arm_dcache_delete((void *)p, FFT_SIZE * sizeof(uint16_t));

  out->s = (const uint16_t *)p;
  out->seq = c;
  holding = true;
  return true;
}

void adc_acquire_release() {
  dma.clearInterrupt();
  holding = false;
}

uint32_t adc_acquire_actual_rate_hz() { return actual_rate; }
uint32_t adc_acquire_overruns() { return overruns; }
