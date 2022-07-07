// FIRST TIME HERE? START WITH THE NEOPXL8 strandtest EXAMPLE INSTEAD!
// That code explains and helps troubshoot wiring and NeoPixel color format.

#include <Adafruit_NeoPXL8.h>

// CHANGE these to match your strandtest findings or this WILL NOT WORK:

int8_t pins[8] = { 6, 7, 9, 8, 13, 12, 11, 10 };
#define COLOR_ORDER NEO_GRB

// This example is minimally adapted from one in PJRC's OctoWS2811 Library:

// Animated Fire Example - OctoWS2811 Library
//  http://www.pjrc.com/teensy/td_libs_OctoWS2811.html
//
// Based on the simple algorithm explained here:
//  http://caraesnaur.github.io/fire/
//
// This example code is in the public domain.

const unsigned int width = 30;
const unsigned int height = 16; // Each strand spans two successive rows

// These parameters control the fire appearance
// (try controlling these with knobs / analogRead....)
unsigned int heat = width / 5;
unsigned int focus = 9;
unsigned int cool = 26;

const int ledsPerPin = width * height / 8;
Adafruit_NeoPXL8 leds(ledsPerPin, pins, COLOR_ORDER);

// Arrays for fire animation
unsigned char canvas[width*height];
extern const unsigned int fireColor[100];

// Run setup once
void setup() {
  if (!leds.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }
  leds.show(); // Clear initial LED state
}

// A simple xy() function to turn display matrix coordinates
// into the index numbers NeoPXL8 requires.  If your LEDs
// are arranged differently, edit this code...
unsigned int xy(unsigned int x, unsigned int y) {
  if ((y & 1) == 0) {
    // even numbered rows (0, 2, 4...) are left to right
    return y * width + x;
  } else {
    // odd numbered rows (1, 3, 5...) are right to left
    return y * width + width - 1 - x;
  }
}

uint32_t lastTime = 0;

// Run repetitively
void loop() {
  uint32_t now = millis();
  if (now - lastTime >= 45) {
    lastTime = now;
    animateFire();
  }
}

void animateFire() {
  unsigned int i, c, n, x, y;

  // Step 1: move all data up one line
  memmove(canvas + width, canvas, width * (height - 1));
  memset(canvas, 0, width);

  // Step 2: draw random heatspots on bottom line
  i = heat;
  if (i > width-8) i = width-8;
  while (i > 0) {
    x = random(width - 2) + 1;
    if (canvas[x] == 0) {
      canvas[x] = 99;
      i--;
    }
  }

  // Step 3: interpolate
  for (y=0; y < height; y++) {
    for (x=0; x < width; x++) {
      c = canvas[y * width + x] * focus;
      n = focus;
      if (x > 0) {
        c = c + canvas[y * width + (x - 1)];
        n = n + 1;
      }
      if (x < width-1) {
        c = c + canvas[y * width + (x + 1)];
        n = n + 1;
      }
      if (y > 0) {
        c = c + canvas[(y -1) * width + x];
        n = n + 1;
      }
      if (y < height-1) {
        c = c + canvas[(y + 1) * width + x];
        n = n + 1;
      }
      c = (c + (n / 2)) / n;
      i = (random(1000) * cool) / 10000;
      if (c > i) {
        c = c - i;
      } else {
        c = 0;
      }
      canvas[y * width + x] = c;
    }
  }

  // Step 4: render canvas to LEDs
  for (y=0; y < height; y++) {
    for (x=0; x < width; x++) {
      c = canvas[((height - 1) - y) * width + x];
      leds.setPixelColor(xy(x, y), leds.gamma32(fireColor[c]));
    }
  }
  leds.show();
}
