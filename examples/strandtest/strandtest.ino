// This is a PARED-DOWN NeoPixel example for the Adafruit_NeoPXL8 library.
// For more complete examples of NeoPixel operations, see the examples
// included with the 'regular' Adafruit_NeoPixel library.

// Also requires LATEST Adafruit_NeoPixel and Adafruit_ZeroDMA libraries.

#include <Adafruit_NeoPXL8.h>

#define NUM_LED 64 // Total number of pixels is 8X this!

// Second argument to constructor is an 8-byte pin list,
// or pass NULL to use pins 0-7 on Metro Express, Arduino Zero, etc.
// Pin mapping on Feather and other boards is fussy and will require
// some documentation.
Adafruit_NeoPXL8 strip(NUM_LED, NULL, NEO_GRB);

void setup() {
  strip.begin();
  strip.setBrightness(32);
}

uint8_t j;

void loop() {
  for(int i=0; i< strip.numPixels(); i++) {
    strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
  }
  strip.show();
  delay(1);
  j++;
}

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

