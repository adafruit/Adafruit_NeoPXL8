// FIRST TIME HERE? START WITH THE NEOPXL8 (not HDR) strandtest EXAMPLE!
// That code explains and helps troubshoot wiring and NeoPixel color format.
// Then move on to NeoPXL8HDR strandtest, THEN try this one.

#include <Adafruit_NeoPXL8.h>

// CHANGE these to match your strandtest findings or this WILL NOT WORK:

int8_t pins[8] = { 6, 7, 9, 8, 13, 12, 11, 10 };
#define COLOR_ORDER NEO_GRB

// This example is minimally adapted from one in PJRC's OctoWS2811 Library:

//PlazINT  -  Fast Plasma Generator using Integer Math Only
//Edmund "Skorn" Horn
//March 4,2013
//Version 1.0 adapted for OctoWS2811Lib (tested, working...)

//OctoWS2811 Defn. Stuff
#define COLS_LEDs 30  // all of the following params need to be adjusted for screen size
#define ROWS_LEDs 16  // LED_LAYOUT assumed 0 if ROWS_LEDs > 8
#define LEDS_PER_STRIP (COLS_LEDs * ROWS_LEDs / 8)

Adafruit_NeoPXL8HDR leds(LEDS_PER_STRIP, pins, COLOR_ORDER);

//Byte val 2PI Cosine Wave, offset by 1 PI
//supports fast trig calcs and smooth LED fading/pulsing.
uint8_t const cos_wave[256] PROGMEM =
{0,0,0,0,1,1,1,2,2,3,4,5,6,6,8,9,10,11,12,14,15,17,18,20,22,23,25,27,29,31,33,35,38,40,42,
45,47,49,52,54,57,60,62,65,68,71,73,76,79,82,85,88,91,94,97,100,103,106,109,113,116,119,
122,125,128,131,135,138,141,144,147,150,153,156,159,162,165,168,171,174,177,180,183,186,
189,191,194,197,199,202,204,207,209,212,214,216,218,221,223,225,227,229,231,232,234,236,
238,239,241,242,243,245,246,247,248,249,250,251,252,252,253,253,254,254,255,255,255,255,
255,255,255,255,254,254,253,253,252,252,251,250,249,248,247,246,245,243,242,241,239,238,
236,234,232,231,229,227,225,223,221,218,216,214,212,209,207,204,202,199,197,194,191,189,
186,183,180,177,174,171,168,165,162,159,156,153,150,147,144,141,138,135,131,128,125,122,
119,116,113,109,106,103,100,97,94,91,88,85,82,79,76,73,71,68,65,62,60,57,54,52,49,47,45,
42,40,38,35,33,31,29,27,25,23,22,20,18,17,15,14,12,11,10,9,8,6,6,5,4,3,2,2,1,1,1,0,0,0,0
};

// Gamma table removed because NeoPXL8HDR provides this functionality.

#if defined(ARDUINO_ARCH_RP2040)

// On RP2040, the refresh() function is called in a tight loop on the
// second core (via the loop1() function). The first core is then 100%
// free for animation logic in the regular loop() function.

void loop1() {
  leds.refresh();
}

#elif defined(CONFIG_IDF_TARGET_ESP32S3)

// On ESP32S3, refresh() is called in a tight loop on core 0. The other
// core (#1), where Arduino code normally runs, is then 100% free for
// animation logic in the loop() function.

void loop0(void *param) {
  for(;;) {
    yield();
    leds.refresh();
  }
}

#else // SAMD

// SAMD, being single-core, uses a timer interrupt for refresh().
// As written, this uses Timer/Counter 3, but with some minor edits
// you can change that if it interferes with other code.

#include "Adafruit_ZeroTimer.h"

Adafruit_ZeroTimer zerotimer = Adafruit_ZeroTimer(3);

void TC3_Handler() {
  Adafruit_ZeroTimer::timerHandler(3);
}

void timerCallback(void) {
  leds.refresh();
}

#endif // end SAMD

void setup() {
  if (!leds.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }
  leds.setBrightness(25000, 2.6); // Less bright, enable gamma
  leds.show();                    // Clear initial LED state

#if defined(CONFIG_IDF_TARGET_ESP32S3)

  // Run loop0() on core 0 (Arduino uses core 1) w/16K stack
  (void)xTaskCreatePinnedToCore(loop0, "NeoPXL8HDR", 16384, NULL, 0, NULL, 0);

#elif defined(__SAMD51__) || defined(_SAMD21_)

  // Setup diverges again for SAMD51 only, to activate the timer code shown
  // earlier. The timer is currently configured for a 120 Hz refresh. This
  // saps a fair percentage of time from the main thread, despite all the
  // DMA going on, which is why RP2040 is most recommended for NeoPXL8HDR.

  zerotimer.enable(false);
  zerotimer.configure(TC_CLOCK_PRESCALER_DIV16, // 3 MHz clock
    TC_COUNTER_SIZE_16BIT,                      // 16-bit counter
    TC_WAVE_GENERATION_MATCH_PWM);              // Freq or PWM mode

  zerotimer.setCompare(0, 3000000 / 120); // 120 Hz refresh
  zerotimer.setCallback(true, TC_CALLBACK_CC_CHANNEL0, timerCallback);
  zerotimer.enable(true);

#endif // end SAMD
}

// REMAINDER OF CODE IS NEARLY IDENTICAL TO NON-HDR VERSION. This version
// doesn't require the gamma correction lookup, handled by the HDR class.

unsigned long frameCount=25500;  // arbitrary seed to calculate the three time displacement variables t,t2,t3

void loop() {
  frameCount++ ;
  uint16_t t = fastCosineCalc((42 * frameCount)/400);  //time displacement - fiddle with these til it looks good...
  uint16_t t2 = fastCosineCalc((35 * frameCount)/400);
  uint16_t t3 = fastCosineCalc((38 * frameCount)/400);

  for (uint8_t y = 0; y < ROWS_LEDs; y++) {
    int left2Right, pixelIndex;
    if (((y % (ROWS_LEDs/8)) & 1) == 0) {
      left2Right = 1;
      pixelIndex = y * COLS_LEDs;
    } else {
      left2Right = -1;
      pixelIndex = (y + 1) * COLS_LEDs - 1;
    }
    for (uint8_t x = 0; x < COLS_LEDs ; x++) {
      //Calculate 3 seperate plasma waves, one for each color channel
      uint8_t r = fastCosineCalc(((x << 3) + (t >> 1) + fastCosineCalc((t2 + (y << 3)))));
      uint8_t g = fastCosineCalc(((y << 3) + t + fastCosineCalc(((t3 >> 2) + (x << 3)))));
      uint8_t b = fastCosineCalc(((y << 3) + t2 + fastCosineCalc((t + x + (g >> 2)))));
      leds.setPixelColor(pixelIndex, r, g, b);
    	pixelIndex += left2Right;
    }
  }
  leds.show();
}

inline uint8_t fastCosineCalc( uint16_t preWrapVal) {
  uint8_t wrapVal = (preWrapVal % 255);
  if (wrapVal<0) wrapVal=255+wrapVal;
  return (pgm_read_byte_near(cos_wave+wrapVal));
}
