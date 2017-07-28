# Adafruit_NeoPXL8
DMA-driven 8-way concurrent NeoPixel driver for SAMD21 (M0+) boards.
Requires latest Adafruit_NeoPixel and Adafruit_ZeroDMA libraries.

(Pronounced "NeoPixelate")

Constructor is slightly different from Adafruit_NeoPixel. Rather than a pin number, this accepts an array of 8 signed bytes corresponding to pins for data outputs 0-7 (very specific rules apply here, more on this in a moment). Can also pass NULL to use the default pin assignments (digital pins 0-7 on Adafruit Metro Express or Arduino Zero):

`Adafruit_NeoPXL8 strip(NUM_LED, NULL, NEO_GRB);`

The first argument, strand length, applies to EACH strand. Total NeoPixel count is 8X this. Pixels are addressed (via setPixelColor()) as a single sequential strand. Supposing NUM_LED is 20, the first physical strand would be pixels 0-19, second strand is 20-39, third is 40-59 and so forth, up to pixel #159 on the eighth strand.

## Logic Levels

The SAMD21 is a 3.3V device, while NeoPixels ideally want 5V logic.  One solution is a logic level converter such as the 74HCT125N.  Alternately, just powering the NeoPixels from a slightly lower voltage (e.g. 4.5V) is sometimes all it takes!

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

Pin MUXing is a hairy thing and over time we'll try to build up some ready-to-use examples for different boards and peripherals. You can also try picking your way through the SAMD21 datasheet or the NeoPXL8 source code for pin/peripheral assignments.
