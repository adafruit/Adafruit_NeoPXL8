// FIRST TIME HERE? START WITH THE NEOPXL8 (not HDR) strandtest EXAMPLE!
// That code explains and helps troubshoot wiring and NeoPixel color format.
// Then move on to NeoPXL8HDR strandtest, THEN try this one.

// This is a companion to "move2msc" in the extras/Processing folder.
// It plays preconverted videos from the on-board flash filesystem.

#include <Adafruit_NeoPXL8.h>

// CHANGE these to match your strandtest findings or this WILL NOT WORK:

int8_t pins[8] = { 6, 7, 9, 8, 13, 12, 11, 10 };
#define COLOR_ORDER NEO_GRB

#define SYNC_PIN -1 // -1 = not used

// This example is minimally adapted from one in PJRC's OctoWS2811 Library:

/*  OctoWS2811 VideoSDcard.ino - Video on LEDs, played from SD Card
    http://www.pjrc.com/teensy/td_libs_OctoWS2811.html
    Copyright (c) 2014 Paul Stoffregen, PJRC.COM, LLC

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

Update: The programs to prepare the SD card video file have moved to "extras"
https://github.com/PaulStoffregen/OctoWS2811/tree/master/extras

  The recommended hardware for SD card playing is:

    Teensy 3.1:     http://www.pjrc.com/store/teensy31.html
    Octo28 Apaptor: http://www.pjrc.com/store/octo28_adaptor.html
    SD Adaptor:     http://www.pjrc.com/store/wiz820_sd_adaptor.html
    Long Pins:      http://www.digikey.com/product-search/en?keywords=S1082E-36-ND

  See the included "hardware.jpg" image for suggested pin connections,
  with 2 cuts and 1 solder bridge needed for the SD card pin 3 chip select.
 
  Required Connections
  --------------------
    pin 2:  LED Strip #1    OctoWS2811 drives 8 LED Strips.
    pin 14: LED strip #2    All 8 are the same length.
    pin 7:  LED strip #3
    pin 8:  LED strip #4    A 100 to 220 ohm resistor should used
    pin 6:  LED strip #5    between each Teensy pin and the
    pin 20: LED strip #6    wire to the LED strip, to minimize
    pin 21: LED strip #7    high frequency ringining & noise.
    pin 5:  LED strip #8
    pin 15 & 16 - Connect together, but do not use
    pin 4:  Do not use

    pin 3:  SD Card, CS
    pin 11: SD Card, MOSI
    pin 12: SD Card, MISO
    pin 13: SD Card, SCLK
*/

#include "SPI.h"
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_TinyUSB.h"

#define LED_WIDTH    30   // number of LEDs horizontally
#define LED_HEIGHT   16   // number of LEDs vertically (must be multiple of 8)
#define LED_LAYOUT   1    // 0 = even rows left->right, 1 = even rows right->left

#define FILENAME     "mymovie.bin"

const int ledsPerStrip = LED_WIDTH * LED_HEIGHT / 8;
uint8_t imageBuffer[LED_WIDTH * LED_HEIGHT * 3];
uint32_t timeOfLastFrame = 0;
bool playing = false;

Adafruit_NeoPXL8HDR leds(ledsPerStrip, pins, COLOR_ORDER);

// From tinyUSB msc_external_flash example
#if defined(ARDUINO_ARCH_RP2040)
  Adafruit_FlashTransport_RP2040 flashTransport;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  Adafruit_FlashTransport_ESP32 flashTransport;
#else
  #if defined(EXTERNAL_FLASH_USE_QSPI)
    Adafruit_FlashTransport_QSPI flashTransport;
  #elif defined(EXTERNAL_FLASH_USE_SPI)
    Adafruit_FlashTransport_SPI flashTransport(EXTERNAL_FLASH_USE_CS, EXTERNAL_FLASH_USE_SPI);
  #else
    #error No QSPI/SPI flash are defined on your board variant.h !
  #endif
#endif

Adafruit_SPIFlash flash(&flashTransport);

FatFileSystem fatfs; // file system object from SdFat
FatFile file;
Adafruit_USBD_MSC usb_msc; // USB Mass Storage object
bool fs_changed = true; // Set to true when PC write to flash

#if defined(ARDUINO_ARCH_RP2040)

// On RP2040, the refresh() function is called in a tight loop on the
// second core (via the loop1() function). The first core is then 100%
// free for animation logic in the regular loop() function.

void loop1() {
  leds.refresh();
}

// Pause just a moment before starting the refresh() loop.
// Mass storage has a lot to do on startup and the sketch locks up
// without this. So far it's only needed on the MSC+HDR example.
void setup1() {
  delay(100);
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
  // Flash setup from tinyUSB msc_external_flash example
  flash.begin();
  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Adafruit", "External Flash", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb); // Set callback
  // Set disk size, block size should be 512 regardless of spi flash page size
  usb_msc.setCapacity(flash.size()/512, 512);
  usb_msc.setUnitReady(true); // MSC is ready for read/write
  usb_msc.begin();
  fatfs.begin(&flash); // Init file system on the flash

  Serial.begin(115200);
  //while (!Serial) ;
  Serial.println("VideoMSC");

  if (!leds.begin()) {
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }
  leds.setBrightness(65535, 2.6); // Full brightness, 2.6 gamma factor
  leds.show();

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

  bool status = file.open(FILENAME, O_RDONLY);
  if (!status) stopWithErrorMessage("Could not read " FILENAME);
  Serial.println("File opened");
  playing = true;
  timeOfLastFrame = 0;
}

// REMAINDER OF CODE IS NEARLY IDENTICAL TO NON-HDR VERSION. This version
// doesn't require the gamma correction lookup, handled by the HDR class.

// read from the SD card, true=ok, false=unable to read
// the SD library is much faster if all reads are 512 bytes
// this function lets us easily read any size, but always
// requests data from the SD library in 512 byte blocks.
//
bool sd_card_read(void *ptr, unsigned int len)
{
  static unsigned char buffer[512];
  static unsigned int bufpos = 0;
  static unsigned int buflen = 0;
  unsigned char *dest = (unsigned char *)ptr;
  unsigned int n;

  while (len > 0) {
    if (buflen == 0) {
      n = file.read(buffer, 512);
      if (n == 0) return false;		
      buflen = n;
      bufpos = 0;
    }
    unsigned int n = buflen;
    if (n > len) n = len;
    memcpy(dest, buffer + bufpos, n);
    dest += n;
    bufpos += n;
    buflen -= n;
    len -= n;
  }
  return true;
}

// skip past data from the SD card
void sd_card_skip(unsigned int len)
{
  unsigned char buf[256];

  while (len > 0) {
    unsigned int n = len;
    if (n > sizeof(buf)) n = sizeof(buf);
    sd_card_read(buf, n);
    len -= n;
  }
}


void loop()
{
  unsigned char header[5];

  if (playing) {
    if (sd_card_read(header, 5)) {
      if (header[0] == '*') {
        // found an image frame
        unsigned int size = (header[1] | (header[2] << 8)) * 3;
        unsigned int usec = header[3] | (header[4] << 8);
        unsigned int readsize = size;
	      //Serial.printf("v: %u %u\n", size, usec);
        if (readsize > sizeof(imageBuffer)) {
          readsize = sizeof(imageBuffer);
        }
        if (sd_card_read(imageBuffer, readsize)) {
          uint32_t now;
          // Serial.printf(", us = %u", (unsigned int)(micros() - timeOfLastFrame));
          // Serial.println();
          do {
            now = micros();
          } while ((now - timeOfLastFrame) < usec) ; // wait
          timeOfLastFrame = now;
          convert_and_show();
        } else {
          error("unable to read video frame data");
          return;
        }
        if (readsize < size) {
          sd_card_skip(size - readsize);
        }
      } else if (header[0] == '%') {
        // found a chunk of audio data
        unsigned int size = (header[1] | (header[2] << 8)) * 2;
#if 0
        // Serial.printf("a: %u", size);
      	// Serial.println();
      	while (size > 0) {
      	  unsigned int len = size;
      	  if (len > 256) len = 256;
      	  int16_t *p = audio.getBuffer();
      	  if (!sd_card_read(p, len)) {
      	    error("unable to read audio frame data");
                  return;
      	  }
      	  if (len < 256) {
                  for (int i=len; i < 256; i++) {
                    *((char *)p + i) = 0;  // fill rest of buffer with zero
                  }
      	  }
                audio.playBuffer();
      	  size -= len;
      	}
#else
        file.seekCur(size);
#endif
      } else {
        error("unknown header");
        return;
      }
    } else {
      error("unable to read 5-byte header");
      return;
    }
  } else {
    delay(2000);
    bool status = file.open(FILENAME, FILE_READ);
    if (status) {
      Serial.println("File opened");
      playing = true;
      timeOfLastFrame = micros();
    }
  }
}

// when any error happens during playback, close the file and restart
void error(const char *str)
{
  Serial.print("error: ");
  Serial.println(str);
  file.close();
  playing = false;
}

// when an error happens during setup, give up and print a message
// to the serial monitor.
void stopWithErrorMessage(const char *str)
{
  while (1) {
    Serial.println(str);
    delay(1000);
  }
}

void convert_and_show() {
  uint8_t *ptr = imageBuffer;
  for (int y=0; y<LED_HEIGHT; y++) {
    for (int x=0; x<LED_WIDTH; x++) {
      int pixelIndex;
#if (LED_LAYOUT == 0)
      // Always left-to-right
      pixelIndex = y * LED_WIDTH + x;
#else
      // Even rows are left-to-right, odd are right-to-left
      if (y & 1) {
        pixelIndex = (y + 1) * LED_WIDTH - 1 - x;
      } else {
        pixelIndex = y * LED_WIDTH + x;
      }
#endif
      uint8_t r = *ptr++;
      uint8_t g = *ptr++;
      uint8_t b = *ptr++;
      leds.setPixelColor(pixelIndex, r, g, b);
    }
  }
  leds.show();
}

// More code from tinyUSB msc_external_flash example; commented there

int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize) {
  return flash.readBlocks(lba, (uint8_t*) buffer, bufsize/512) ? bufsize : -1;
}

int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  return flash.writeBlocks(lba, buffer, bufsize/512) ? bufsize : -1;
}

void msc_flush_cb (void) {
  flash.syncBlocks();
  fatfs.cacheClear();
  fs_changed = true;
}
