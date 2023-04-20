// This sketch is Just Too Much for SAMD21 (M0) and SAMD51 (M4) boards.
// Recommend RP2040/SCORPIO or ESP32-S3.

// FIRST TIME HERE? START WITH THE NEOPXL8 (not HDR) strandtest EXAMPLE!
// That code explains and helps troubshoot wiring and NeoPixel color format.
// Then move on to NeoPXL8HDR strandtest, THEN try this one.

// This is a companion to "move2msc" in the extras/Processing folder.
// It plays preconverted videos from the on-board flash filesystem.

#include <Adafruit_NeoPXL8.h>
#include <Adafruit_CPFS.h> // For accessing CIRCUITPY drive
#define ARDUINOJSON_ENABLE_COMMENTS 1
#include <ArduinoJson.h>

// This example is adapted from one in PJRC's OctoWS2811 Library. Original
// comments appear first, and Adafruit_NeoPXL8 changes follow that. Any
// original comments about "SD card" now apply to a board's CIRCUITPY flash
// filesystem instead.

/*  OctoWS2811 VideoSDcard.ino - Video on LEDs, played from SD Card
    http://www.pjrc.com/teensy/td_libs_OctoWS2811.html
    Copyright (c) 2014 Paul Stoffregen, PJRC.COM, LLC

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

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

/*
    ADAFRUIT_NEOPXL8 UPDATE:

    Aside from changes to convert from OctoWS2811 to Adafruit_NeoPXL8, the
    big drastic change here is to eliminate many compile-time constants and
    instead place these in a JSON configuration file on a board's CIRCUITPY
    flash filesystem (though this is Arduino code, we can still make use of
    that drive), and declare the LEDs at run time. Other than those
    alterations, the code is minimally changed. Paul did the real work. :)

    Run-time configuration is stored in CIRCUITPY/neopxl8.cfg and resembles:

    {
      "pins" : [ 16, 17, 18, 19, 20, 21, 22, 23 ],
      "order" : "GRB",
      "led_width" : 30,
      "led_height" : 16,
      "led_layout" : 0
    }

    If this file is missing, or if any individual elements are unspecified,
    defaults will be used (these are noted later in the code). It's possible,
    likely even, that there will be additional elements in this file...
    for example, some NeoPXL8 code might use a single "length" value rather
    than width/height, as not all projects are using a grid. Be warned that
    JSON is highly picky and even a single missing or excess comma will stop
    everything, so read through it very carefully if encountering an error.
*/

#define FILENAME "mymovie.bin"

// In original code, these were constants LED_WIDTH, LED_HEIGHT and
// LED_LAYOUT. Values here are defaults but can override in config file.
// led_height MUST be a multiple of 8. When 16, 24, 32 are used, each strip
// spans 2, 3, 4 rows. led_layout indicates how strips are arranged.
uint16_t led_width  = 30; // Number of LEDs horizontally
uint16_t led_height = 16; // Number of LEDs vertically
uint8_t  led_layout = 0;  // 0 = even rows left->right, 1 = right->left

FatFile file;
bool playing = false;
uint32_t timeOfLastFrame = 0;
uint8_t *imageBuffer;     // LED data from filesystem is staged here
uint32_t imageBufferSize; // Size (in bytes) of imageBuffer

Adafruit_NeoPXL8HDR *leds = NULL;

void error_handler(const char *message, uint16_t speed) {
  Serial.print("Error: ");
  Serial.println(message);
  if (speed) { // Fatal error, blink LED
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) {
      digitalWrite(LED_BUILTIN, (millis() / speed) & 1);
      yield(); // Keep filesystem accessible for editing
    }
  } else { // Not fatal, just show message
    Serial.println("Continuing with defaults");
  }
}

#if defined(ARDUINO_ARCH_RP2040)

// On RP2040, the refresh() function is called in a tight loop on the
// second core (via the loop1() function). The first core is then 100%
// free for animation logic in the regular loop() function.

void loop1() {
  if (leds) leds->refresh();
}

// Pause just a moment before starting the refresh() loop.
// Mass storage has a lot to do on startup and the sketch locks up
// without this. So far it's only needed on the MSC+HDR example.
void setup1() {
  delay(1000);
}

#elif defined(CONFIG_IDF_TARGET_ESP32S3)

// On ESP32S3, refresh() is called in a tight loop on core 0. The other
// core (#1), where Arduino code normally runs, is then 100% free for
// animation logic in the loop() function.

void loop0(void *param) {
  for(;;) {
    yield();
    if (leds) leds->refresh();
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
  if (leds) leds->refresh();
}

#endif // end SAMD

void setup() {
  // CHANGE these to match your strandtest findings (or use .cfg file):
  int8_t pins[8] = NEOPXL8_DEFAULT_PINS;
  uint16_t order = NEO_GRB;

  // Start the CIRCUITPY flash filesystem first. Very important!
  FatVolume *fs = Adafruit_CPFS::begin();

  // Start Serial AFTER FFS begin, else CIRCUITPY won't show on computer.
  Serial.begin(115200);
  //while(!Serial);
  delay(1000);
  Serial.setTimeout(50);
  Serial.println("VideoMSC");

  if (fs == NULL) {
    error_handler("Can't access CIRCUITPY drive", 0);
  } else {
    StaticJsonDocument<1024> doc;
    DeserializationError error;

    // Open NeoPXL8 configuration file and attempt to decode JSON data within.
    if ((file = fs->open("neopxl8.cfg", FILE_READ))) {
      error = deserializeJson(doc, file);
      file.close();
    } else {
      error_handler("neopxl8.cfg not found", 0);
    }

    if(error) {
      error_handler("neopxl8.cfg syntax error", 0);
      Serial.print("JSON error: ");
      Serial.println(error.c_str());
    } else {
      // Config is valid, override defaults in program variables...

      JsonVariant v = doc["pins"];
      if (v.is<JsonArray>()) {
        uint8_t n = v.size() < 8 ? v.size() : 8;
        for (uint8_t i = 0; i < n; i++)
          pins[i] = v[i].as<int>();
      }

      v = doc["order"];
      if (v.is<const char *>()) order = Adafruit_NeoPixel::str2order(v);

      led_width  = doc["led_width"]  | led_width;
      led_height = doc["led_height"] | led_height;
      led_layout = doc["led_layout"] | led_layout;
    } // end JSON OK
  } // end filesystem OK

  // Any errors after this point are unrecoverable and program will stop.

    // Dynamically allocate NeoPXL8 object
  leds = new Adafruit_NeoPXL8HDR(led_width * led_height / 8, pins, order);
  if (leds == NULL) error_handler("NeoPXL8 allocation", 100);

  // Allocate imageBuffer
  imageBufferSize = led_width * led_height * 3;
  imageBuffer = (uint8_t *)malloc(imageBufferSize);
  if (imageBuffer == NULL) error_handler("Image buffer allocation", 200);

  if (!leds->begin()) error_handler("NeoPXL8 begin() failed", 500);

  // At this point, everything but input file is ready to go!

  leds->setBrightness(65535, 2.6); // Full brightness, 2.6 gamma factor
  leds->show(); // LEDs off ASAP

  bool status = file.open(FILENAME, O_RDONLY);
  if (!status) error_handler("Can't open movie .bin file", 1000);
  Serial.println("File opened");
  delay(500); // Give MSC a moment to compose itself
  playing = true;
  timeOfLastFrame = 0;

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
        if (readsize > imageBufferSize) {
          readsize = imageBufferSize;
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

void convert_and_show() {
  uint8_t *ptr = imageBuffer;
  if (led_layout == 0) {
    for (int y=0; y<led_height; y++) {
      for (int x=0; x<led_width; x++) {
        int pixelIndex = y * led_width + x; // Always left-to-right
        uint8_t r = *ptr++;
        uint8_t g = *ptr++;
        uint8_t b = *ptr++;
        leds->setPixelColor(pixelIndex, r, g, b);
      }
    }
  } else {
    for (int y=0; y<led_height; y++) {
      for (int x=0; x<led_width; x++) {
        // Even rows are left-to-right, odd are right-to-left
        int pixelIndex = (y & 1) ? (y + 1) * led_width - 1 - x : y * led_width + x;
        uint8_t r = *ptr++;
        uint8_t g = *ptr++;
        uint8_t b = *ptr++;
        leds->setPixelColor(pixelIndex, r, g, b);
      }
    }
  }
  leds->show();
}
