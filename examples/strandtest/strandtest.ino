// This is a PARED-DOWN NeoPixel example for the Adafruit_NeoPXL8 library.
// For more complete examples of NeoPixel operations, see the examples
// included with the 'regular' Adafruit_NeoPixel library.

// Also requires LATEST Adafruit_NeoPixel and Adafruit_ZeroDMA libraries.

// May require a logic level shifter (e.g. 75HCT245) for 5V pixels.

#include <Adafruit_NeoPXL8.h>

#define NUM_LED 64 // Total number of pixels is 8X this!

// Second argument to constructor is an 8-byte pin list,
// or pass NULL to use pins 0-7 on Metro Express, Arduino Zero, etc.
Adafruit_NeoPXL8 strip(NUM_LED, NULL, NEO_GRB);
// Here's an alternate pinout that could work on Feather M0:
//int8_t pins[8] = { PIN_SERIAL1_RX, PIN_SERIAL1_TX, MISO, 13, 5, SDA, A4, A3 };
//Adafruit_NeoPXL8 strip(NUM_LED, pins, NEO_GRB);
// Or, if those collide with vital peripherals, alternates could include:
//int8_t pins[8] = { 12, 10, 11, 13, SCK, MOSI, A4, A3 };
//Adafruit_NeoPXL8 strip(NUM_LED, pins, NEO_GRB);

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

