// Example/diagnostic for the Adafruit_NeoPXL8 library.  Each of 8 strands
// is a distinct color, helps identify which pin goes to which strand.
// For more complete usage of NeoPixel operations, see the examples
// included with the 'regular' Adafruit_NeoPixel library.

// Also requires LATEST Adafruit_NeoPixel, Adafruit_ZeroDMA and
// Adafruit_ASFcore libraries.

// May require a logic level shifter (e.g. 75HCT245) for 5V pixels,
// or use NeoPXL8 Featherwing for Adafruit Feather M0 boards.

#include <Adafruit_NeoPXL8.h>

#define NUM_LED 64  // Per strand. Total number of pixels is 8X this!

// Second argument to constructor is an optional 8-byte pin list,
// or pass NULL to use pins 0-7 on Metro Express, Arduino Zero, etc.
//int8_t pins0[8] = {  2, 13,  3,    6,   8,  9, 10, 11 }, // TCC0
//       pins1[8] = {  0,  1, 12, MOSI, SCK, A4, -1, -1 }; // TCC1

// Metro M4 TCC0 pins:
//int8_t pins[8] = { 7, 4, 5, 6, 3, 2, 10, 11 };
// Alt TCC0 pins:
//int8_t pins[8] = { 9, 8, 0, 1, 13, 12, -1, SCK };

int8_t pins0[8] = {  7,  4,    5,   6, 3, 2, 10, 11 }, // TCC0
       pins1[8] = { 13, 12, MOSI, SCK, 9, 8,  1,  0 }; // TCC1

Adafruit_NeoPXL8 leds0(NUM_LED, pins0, NEO_GRB, 0),
                 leds1(NUM_LED, pins1, NEO_GRB, 1);

void setup() {
  Serial.begin(115200);
  while(!Serial);
  leds0.begin();
  leds0.setBrightness(32);
  leds1.begin();
  leds1.setBrightness(32);
}

uint8_t frame = 0;

void loop() {
  for(uint8_t r=0; r<8; r++) { // For each row...
    for(int p=0; p<NUM_LED; p++) { // For each pixel of row...
      leds0.setPixelColor(r * NUM_LED + p, rain(r, p));
      leds1.setPixelColor(r * NUM_LED + p, rain(r, p));
    }
  }
  leds0.stage();
  leds1.stage();
  leds0.show();
for(volatile int i=0; i<2000; i++);
  leds1.show();
  frame++;
}

uint8_t colors[8][3] = { // RGB colors for the 8 rows...
  255,   0,   0, // Row 0: Red
  255, 160,   0, // Row 1: Orange
  255, 255,   0, // Row 2: Yellow
    0, 255,   0, // Row 3: Green
    0, 255, 255, // Row 4: Cyan
    0,   0, 255, // Row 5: Blue
  192,   0, 255, // Row 6: Purple
  255,   0, 255  // Row 7: Magenta
};

// Gamma-correction table improves the appearance of midrange colors
#define _GAMMA_ 2.6
const int _GBASE_ = __COUNTER__ + 1; // Index of 1st __COUNTER__ ref below
#define _G1_ (uint8_t)(pow((__COUNTER__ - _GBASE_) / 255.0, _GAMMA_) * 255.0 + 0.5),
#define _G2_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ // Expands to 8 lines
#define _G3_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ // Expands to 64 lines
const uint8_t gamma8[] = { _G3_ _G3_ _G3_ _G3_ };    // 256 lines

// Given row number (0-7) and pixel number along row (0 - (NUM_LED-1)),
// first calculate brightness (b) of pixel, then multiply row color by
// this and run it through gamma-correction table.
uint32_t rain(uint8_t row, int pixelNum) {
  uint16_t b = 256 - ((frame - row * 32 + pixelNum * 256 / NUM_LED) & 0xFF);
  return ((uint32_t)gamma8[(colors[row][0] * b) >> 8] << 16) |
         ((uint32_t)gamma8[(colors[row][1] * b) >> 8] <<  8) |
                    gamma8[(colors[row][2] * b) >> 8];
}

