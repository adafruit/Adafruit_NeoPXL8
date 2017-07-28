#include "Adafruit_NeoPXL8.h"
#include "wiring_private.h" // pinPeripheral() function

#define EXTRASTARTBYTES 6 // Empirical; needed while DMA stabilizes

static uint8_t defaultPins[] = { 0,1,2,3,4,5,6,7 };

volatile boolean  sending = 0;
volatile uint32_t lastBitTime;

static void dmaCallback(struct dma_resource* const resource) {
  sending     = 0;
  lastBitTime = micros();
}

Adafruit_NeoPXL8::Adafruit_NeoPXL8(
  uint16_t n, uint8_t *p, neoPixelType t) : Adafruit_NeoPixel(n * 8, -1, t),
  brightness(256), dmaBuf(NULL) {
  memcpy(pins, p ? p : defaultPins, sizeof(pins));
}

Adafruit_NeoPXL8::~Adafruit_NeoPXL8() {
  dma.abort();
  if(dmaBuf) free(dmaBuf);
}

// Stuff not in Arduino arrays.  This is from SAMD21 datasheet.
static struct {
  EPortType port;       // PORTA|PORTB
  uint8_t   bit;        // Port bit (0-31)
  uint8_t   wo;         // TCC0/WO# (0-7)
  EPioType  peripheral; // Peripheral to select for TCC0 out
} tcc0pinMap[] = {
  { PORTA,  4, 0, PIO_TIMER     },
  { PORTA,  5, 1, PIO_TIMER     },
  { PORTA,  8, 0, PIO_TIMER     },
  { PORTA,  9, 1, PIO_TIMER     },
  { PORTA, 10, 2, PIO_TIMER_ALT },
  { PORTA, 11, 3, PIO_TIMER_ALT },
  { PORTA, 12, 6, PIO_TIMER_ALT },
  { PORTA, 13, 7, PIO_TIMER_ALT },
  { PORTA, 14, 4, PIO_TIMER_ALT },
  { PORTA, 15, 5, PIO_TIMER_ALT },
  { PORTA, 16, 6, PIO_TIMER_ALT }, // Not in Rev A silicon
  { PORTA, 17, 7, PIO_TIMER_ALT }, // Not in Rev A silicon
  { PORTA, 18, 2, PIO_TIMER_ALT },
  { PORTA, 19, 3, PIO_TIMER_ALT },
  { PORTA, 20, 6, PIO_TIMER_ALT },
  { PORTA, 21, 7, PIO_TIMER_ALT },
  { PORTA, 22, 4, PIO_TIMER_ALT },
  { PORTA, 23, 5, PIO_TIMER_ALT },
  { PORTB, 10, 4, PIO_TIMER_ALT },
  { PORTB, 11, 5, PIO_TIMER_ALT },
  { PORTB, 12, 6, PIO_TIMER_ALT },
  { PORTB, 13, 7, PIO_TIMER_ALT },
  { PORTB, 16, 4, PIO_TIMER_ALT },
  { PORTB, 17, 5, PIO_TIMER_ALT },
  { PORTB, 30, 0, PIO_TIMER     },
  { PORTB, 31, 1, PIO_TIMER     }
};
#define PINMAPSIZE (sizeof(tcc0pinMap) / sizeof(tcc0pinMap[0]))

static uint8_t configurePin(uint8_t pin) {
  if((pin >= 0) && (pin < PINS_COUNT)) {
    EPortType port = g_APinDescription[pin].ulPort;
    uint8_t   bit  = g_APinDescription[pin].ulPin;
    for(uint8_t i=0; i<PINMAPSIZE; i++) {
      if((port == tcc0pinMap[i].port) && (bit == tcc0pinMap[i].bit)) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        pinPeripheral(pin, tcc0pinMap[i].peripheral);
        return (1 << tcc0pinMap[i].wo);
      }
    }
  }
  return 0;
}

boolean Adafruit_NeoPXL8::begin(void) {

  Adafruit_NeoPixel::begin(); // Call base class begin() function 1st
  if(pixels) { // Successful malloc of NeoPixel buffer?
    uint8_t  bytesPerPixel = (wOffset == rOffset) ? 3 : 4;
    uint32_t bytesTotal    = numLEDs * bytesPerPixel * 3 + EXTRASTARTBYTES;
    if((dmaBuf = (uint8_t *)malloc(bytesTotal))) {
      int i;

      dma.configure_peripheraltrigger(TCC0_DMAC_ID_OVF);
      dma.configure_triggeraction(DMA_TRIGGER_ACTON_BEAT);

      uint8_t *dst = &((uint8_t *)(&TCC0->PATT))[1]; // PAT.vec.PGV
      dma.allocate();
      dma.setup_transfer_descriptor(
        dmaBuf,             // source
        dst,                // destination
        bytesTotal,         // count
        DMA_BEAT_SIZE_BYTE, // size per
        true,               // increment source
        false);             // don't increment destination
      dma.add_descriptor();

      dma.register_callback(dmaCallback);
      dma.enable_callback();

      dma.start_transfer_job();

      // Enable GCLK for TCC0
      GCLK->CLKCTRL.reg = (uint16_t)(GCLK_CLKCTRL_CLKEN |
        GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCM_TCC0_TCC1));
      while(GCLK->STATUS.bit.SYNCBUSY == 1);

      // Disable TCC before configuring it
      TCC0->CTRLA.bit.ENABLE = 0;
      while(TCC0->SYNCBUSY.reg & TCC_SYNCBUSY_MASK);

      TCC0->CTRLA.reg = TCC_CTRLA_PRESCALER_DIV1; // 1:1 Prescale
      while(TCC0->SYNCBUSY.reg & TCC_SYNCBUSY_MASK);

      TCC0->WAVE.bit.WAVEGEN = TCC_WAVE_WAVEGEN_NPWM_Val; // Normal PWM mode
      while(TCC0->SYNCBUSY.reg & TCC_SYNCBUSY_MASK);

      TCC0->CC[0].reg = 0; // No PWM out
      while(TCC0->SYNCBUSY.reg & TCC_SYNCBUSY_MASK);

      // 2.4 GHz clock: 3 DMA xfers per NeoPixel bit = 800 KHz
      TCC0->PER.reg = ((F_CPU + 1200000)/ 2400000) - 1;
      while(TCC0->SYNCBUSY.reg & TCC_SYNCBUSY_MASK);


      memset(bitmask, 0, sizeof(bitmask));
      uint8_t enableMask = 0x00;       // Bitmask of pattern gen outputs
      for(i=0; i<8; i++) {
        if(bitmask[i] = configurePin(pins[i])) enableMask |= 1 << i;
      }
      TCC0->PATT.vec.PGV = 0x00;       // Set all pattern outputs to 0
      TCC0->PATT.vec.PGE = enableMask; // Enable pattern outputs

      TCC0->CTRLA.bit.ENABLE = 1;
      while(TCC0->SYNCBUSY.reg & TCC_SYNCBUSY_MASK);

      // Initialize dmaBuf contents
      memset(dmaBuf, 0, bytesTotal);
      uint32_t foo = numLEDs * bytesPerPixel;
      for(i=0; i<foo; i++) dmaBuf[EXTRASTARTBYTES + i * 3] = 0xFF;

      return true;
    }
    free(pixels);
    pixels = NULL;
  }

  return false;
}

// Convert NeoPixel buffer to NeoPXL8 output format
void Adafruit_NeoPXL8::stage(void) {

  uint32_t i;
  uint32_t pixelsPerRow = numLEDs / 8;
  uint8_t  bytesPerLED  = (wOffset == rOffset) ? 3 : 4;
  uint32_t bytesPerRow  = pixelsPerRow * bytesPerLED;

// numLEDs and numBytes are things in NeoPixel library
// bytes per LED is an on-the-fly calc

  // Clear DMA buffer data
  uint32_t foo = pixelsPerRow * bytesPerLED * 8;
  for(i=0; i<foo; i++) dmaBuf[EXTRASTARTBYTES + i * 3 + 1] = 0;

  for(uint8_t b=0; b<8; b++) { // For each output pin 0-7
    if(bitmask[b]) { // Enabled?
      uint8_t *src = &pixels[b * bytesPerRow]; // Start of row data
      uint8_t *dst = &dmaBuf[EXTRASTARTBYTES + 1];
      for(i=0; i<bytesPerRow; i++) { // Each byte in row...
        uint16_t value = *src++ * brightness; // Bits 15-8 have adjusted value
        for(uint16_t foo=0x8000; foo >= 0x0100; foo >>= 1) {
          if(value & foo) *dst |= bitmask[b];
          dst += 3;
        }
      }
    }
  }

  staged = true;
}

void Adafruit_NeoPXL8::show(void) {
  if(!staged) stage();
  while(sending); // Wait for DMA callback
  while((micros() - lastBitTime) < 300); // Wait for latch
  dma.start_transfer_job();
  staged  = false;
  sending = 1;
  dma.trigger_transfer();
}

boolean Adafruit_NeoPXL8::canShow(void) {
}

boolean Adafruit_NeoPXL8::canStage(void) {
}

// Brightness is stored differently here than in normal NeoPixel library.
// In either case it's *specified* the same: 0 (off) to 255 (brightest).
// Classic NeoPixel rearranges this internally so 0 is max, 1 is off and
// 255 is just below max...it's a decision based on how fixed-point math
// is handled in that code.  Here it's stored internally as 1 (off) to
// 256 (brightest), requiring a 16-bit value.

void Adafruit_NeoPXL8::setBrightness(uint8_t b) {
  brightness = (uint16_t)b + 1; // 0-255 in, 1-256 out
}

uint8_t Adafruit_NeoPXL8::getBrightness(void) const {
  return brightness - 1; // 1-256 in, 0-255 out
}
