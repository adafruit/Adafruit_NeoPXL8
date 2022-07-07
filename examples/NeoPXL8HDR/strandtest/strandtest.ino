/*
*** FIRST TIME HERE? START W/THE NEOPXL8 (not HDR) strandtest INSTEAD! ***
That code explains and helps troubshoot wiring and NeoPixel color format.

Adafruit_NeoPXL8HDR is a subclass of Adafruit_NeoPXL8 that provides
16-bits-per-channel color, temporal dithering, and gamma correction.
This is a minimal demonstration of "HDR" vs regular NeoPXL8 to show the
extra setup required. It runs an animation loop similar to the non-HDR
strandtest example, with the color fills on startup removed...those are
for troubleshooting wiring and color order...work through that in the
easier non-HDR strandtest and then copy your findings here. You'll
find MUCH more documentation in that example.

Most important thing to note here is that different chips (RP2040, SAMD51)
require very different startup code. Animation loop is identical to NeoPXL8,
just the setup changes. SAMD21 is NOT supported; HDR is just too demanding.
SAMD51 requires the Adafruit_ZeroTimer library.
*/

#include <Adafruit_NeoPXL8.h>

// CHANGE these to match your strandtest findings or this WILL NOT WORK:

int8_t pins[8] = { 6, 7, 9, 8, 13, 12, 11, 10 };
#define COLOR_ORDER NEO_GRB // NeoPixel color format (see Adafruit_NeoPixel)
#define NUM_LEDS    60      // NeoPixels PER STRAND, total number is 8X this!

// The arguments to the Adafruit_NeoPXL8HDR constructor are
// identical to Adafruit_NeoPXL8, just the name changes, adding "HDR":

Adafruit_NeoPXL8HDR leds(NUM_LEDS, pins, COLOR_ORDER);


// SETUP (VERY DIFFERENT from "regular" NeoPXL8) ---------------------------

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

  // In Adafruit_NeoPXL8, begin() normally doesn't take arguments. The HDR
  // version has some extra flags to select frame blending, added "depth"
  // for temporal dithering, and whether to double-buffer DMA pixel writes:
  if (!leds.begin(true, 4, true)) {
    // If begin() returns false, either an invalid pin list was provided,
    // or requested too many pixels for available memory (HDR requires
    // INORDINATE RAM). Blink onboard LED to indicate startup problem.
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }

  // NeoPXL8HDR can provide automatic gamma-correction, but it's disabled
  // by default; pixel values correlate to linear duty cycle. This is so
  // existing NeoPixel or NeoPXL8 code brought over to NeoPXL8HDR looks the
  // same, no surprises. When reworking a project for HDR, it's best to
  // enable, animation code then doesn't need to process this. Gamma curve
  // (2.6 in this example) is set at the same time as global brightness.
  // Notice the brightness uses a 16-bit value (0-65535), not 8-bit
  // (0-255) as in NeoPixel and NeoPXL8:
  leds.setBrightness(65535 / 8, 2.6); // 1/8 max duty cycle, 2.6 gamma factor

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

  // The startup colors are REMOVED in this example. It's assumed at
  // this point (via non-HDR strandtest) that your code and hardware
  // are confirmed in sync, making these tests redundant.
}


// ANIMATION LOOP (similar to "regular" NeoPXL8) ---------------------------

// The loop() function is identical to the non-HDR example. Although
// NeoPXL8HDR can handle 16-bits-per-channel colors, we're using 8-bit
// here (packed into a 32-bit value) to show how existing NeoPixel or
// NeoPXL8 code can carry over directly. The library will expand these
// to 16 bits behind the scenes.
void loop() {
  uint32_t now = millis(); // Get time once at start of each frame
  for(uint8_t r=0; r<8; r++) { // For each row...
    for(int p=0; p<NUM_LEDS; p++) { // For each pixel of row...
      leds.setPixelColor(r * NUM_LEDS + p, rain(now, r, p));
    }
  }
  leds.show();
}

// Pixel strand colors, same code as non-HDR strandtest:
static uint8_t colors[8][3] = {
  255,   0,   0, // Row 0: Red
  255, 160,   0, // Row 1: Orange
  255, 255,   0, // Row 2: Yellow
    0, 255,   0, // Row 3: Green
    0, 255, 255, // Row 4: Cyan
    0,   0, 255, // Row 5: Blue
  192,   0, 255, // Row 6: Purple
  255,   0, 255  // Row 7: Magenta
};

// This function changes very slightly from the non-HDR strandtest example.
// That code had to apply gamma correction on is own. It's not necessary
// with the HDR class once configured (see setBrightness() in setup()),
// we get that "free" now.
uint32_t rain(uint32_t now, uint8_t row, int pixelNum) {
  uint8_t frame = now / 4; // uint8_t rolls over for a 0-255 range
  uint16_t b = 256 - ((frame - row * 32 + pixelNum * 256 / NUM_LEDS) & 0xFF);
  return leds.Color((colors[row][0] * b) >> 8,
                    (colors[row][1] * b) >> 8,
                    (colors[row][2] * b) >> 8);
}
