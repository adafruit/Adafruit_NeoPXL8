# Adafruit_NeoPXL8
DMA-driven 8-way concurrent NeoPixel driver for SAMD21 (M0+), SAMD51 (M4), RP2040 and ESP32S3 microcontrollers. Requires latest Adafruit_NeoPixel and Adafruit_ZeroDMA libraries.

(Pronounced "NeoPixelate")

Constructor is slightly different from Adafruit_NeoPixel. Rather than a pin number, this accepts an array of 8 signed bytes corresponding to pins for data outputs 0-7 (very specific rules apply here, more on this in a moment). Can also pass NULL to use the default pin assignments (digital pins 0-7 on Adafruit Metro Express or Arduino Zero):

`Adafruit_NeoPXL8 strip(NUM_LED, NULL, NEO_GRB);`

The first argument, strand length, applies to EACH strand. Total NeoPixel count is 8X this. Pixels are addressed (via setPixelColor()) as a single sequential strand. Supposing NUM_LED is 20, the first physical strand would be pixels 0-19, second strand is 20-39, third is 40-59 and so forth, up to pixel #159 on the eighth strand.

## Logic Levels

Microcontrollers compatible with this code are all 3.3V devices, while NeoPixels ideally want 5V logic. One solution is a logic level converter such as the 74HCT125N. Alternately, just powering the NeoPixels from a slightly lower voltage (e.g. 4.5V) is sometimes all it takes!

## Pin MUXing

Because this library relies on the SAMD21's TCC0 "pattern generator" peripheral, outputs can only be assigned to very specific pins. Fortunately most provide alternate options if the default layout doesn't meet your needs (e.g. on Feather boards).

Pin 0 alternate is 12
Pin 1 alternate is 10
Pin 2 alternates are MOSI or SDA
Pin 3 alternate is A4
Pin 4 alternate is A3
Pin 5 alternates are SCK or SCL
Pin 6 alternates are 11 or MISO
Pin 7 alternate is 13

On the Metro Express and Arduino Zero boards, some of these pins (MOSI, MISO and SCK) are on the 6-pin ICSP header.

Because Feather boards have a pared-down pinout, some of the alternates need to be used. One possible layout might be:

`int8_t pins[] = { 13, 12, 11, 10, 5, SDA, A3, A4 };`

This keeps the Serial1 and SPI peripherals available, but prevents using I2C. A different arrangement would keep I2C free at the expense of one NeoPixel output:

`int8_t pins[] = { 13, 12, 11, 10, 5, -1, A3, A4 };`

The '-1' for a pin assignment disables that NeoPixel output (so this would have only 7 concurrent strands), but those pixels still take up memory and positions among the pixel indices.

Other boards (such as Grand Central) have an altogether different pinout. See the example for insights.

Pin MUXing is a hairy thing and over time we'll try to build up some ready-to-use examples for different boards and peripherals. You can also try picking your way through the SAMD21/51 datasheet or the NeoPXL8 source code for pin/peripheral assignments.

On RP2040 boards, the pins can be within any 8-pin range (e.g. 0-7, or 4-11, etc.). If using fewer than 8 outputs, they do not need to be contiguous, but the lowest and highest pin number must still be within 8-pin range.

On ESP32S3 boards, go wild...there are no pin restrictions.

## NeoPXL8HDR

Adafruit_NeoPXL8HDR is a subclass of Adafruit_NeoPXL8 with additions for 16-bit color, temporal dithering, gamma correction and frame blending. This requires inordinate RAM, and the need for frequent refreshing makes it best suited for multi-core chips (e.g. RP2040).

See examples/NeoPXL8HDR/strandtest for use.
