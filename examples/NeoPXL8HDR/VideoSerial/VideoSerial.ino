// This sketch is Just Too Much for SAMD21 (M0) and SAMD51 (M4) boards.
// Recommend RP2040/SCORPIO or ESP32-S3.

// FIRST TIME HERE? START WITH THE NEOPXL8 (not HDR) strandtest EXAMPLE!
// That code explains and helps troubshoot wiring and NeoPixel color format.
// Then move on to NeoPXL8HDR strandtest, THEN try this one.

// This is a companion to "move2serial" in the extras/Processing folder.

#include <Adafruit_NeoPXL8.h>
#include <Adafruit_CPFS.h> // For accessing CIRCUITPY drive
#define ARDUINOJSON_ENABLE_COMMENTS 1
#include <ArduinoJson.h>

// This example is adapted from one in PJRC's OctoWS2811 Library. Original
// comments appear first, followed by Adafruit_NeoPXL8 changes.

/*  OctoWS2811 VideoDisplay.ino - Video on LEDs, from a PC, Mac, Raspberry Pi
    http://www.pjrc.com/teensy/td_libs_OctoWS2811.html
    Copyright (c) 2013 Paul Stoffregen, PJRC.COM, LLC

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

Update: The movie2serial program which transmit data has moved to "extras"
https://github.com/PaulStoffregen/OctoWS2811/tree/master/extras

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
    pin 3:  Do not use as PWM.  Normal use is ok.
    pin 12: Frame Sync

    When using more than 1 Teensy to display a video image, connect
    the Frame Sync signal between every board.  All boards will
    synchronize their WS2811 update using this signal.

    Beware of image distortion from long LED strip lengths.  During
    the WS2811 update, the LEDs update in sequence, not all at the
    same instant!  The first pixel updates after 30 microseconds,
    the second pixel after 60 us, and so on.  A strip of 120 LEDs
    updates in 3.6 ms, which is 10.8% of a 30 Hz video frame time.
    Doubling the strip length to 240 LEDs increases the lag to 21.6%
    of a video frame.  For best results, use shorter length strips.
    Multiple boards linked by the frame sync signal provides superior
    video timing accuracy.

    A Multi-TT USB hub should be used if 2 or more Teensy boards
    are connected.  The Multi-TT feature allows proper USB bandwidth
    allocation.  Single-TT hubs, or direct connection to multiple
    ports on the same motherboard, may give poor performance.
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
      "sync_pin" : -1,
      "led_width" : 30,
      "led_height" : 16,
      "led_layout" : 0,
      "video_xoffset" : 0,
      "video_yoffset" : 0,
      "video_width" : 100,
      "video_height" : 100
    }

    If this file is missing, or if any individual elements are unspecified,
    defaults will be used (these are noted later in the code). It's possible,
    likely even, that there will be additional elements in this file...
    for example, some NeoPXL8 code might use a single "length" value rather
    than width/height, as not all projects are using a grid. Be warned that
    JSON is highly picky and even a single missing or excess comma will stop
    everything, so read through it very carefully if encountering an error.
*/

// In original code, these were constants LED_WIDTH, LED_HEIGHT and
// LED_LAYOUT. Values here are defaults but can override in config file.
// led_height MUST be a multiple of 8. When 16, 24, 32 are used, each strip
// spans 2, 3, 4 rows. led_layout indicates how strips are arranged.
uint16_t led_width  = 30; // Number of LEDs horizontally
uint16_t led_height = 16; // Number of LEDs vertically
uint8_t  led_layout = 0;  // 0 = even rows left->right, 1 = right->left

// The portion of the video image to show on this set of LEDs.  All 4 numbers
// are percentages, from 0 to 100.  For a large LED installation with many
// boards driving groups of LEDs, these parameters allow you to program each
// one to tell the video application which portion of the video it displays.
// By reading these numbers, the video application can automatically configure
// itself, regardless of which serial port COM number or device names are
// assigned to each board by your operating system. 0/0/100/100 displays the
// entire image on one board's LEDs. With two boards, this could be split
// between them, 0/0/100/50 for the top and 0/50/100/50 for the bottom.
uint8_t video_xoffset =   0;
uint8_t video_yoffset =   0;
uint8_t video_width   = 100;
uint8_t video_height  = 100;

uint8_t *imageBuffer;     // Serial LED data is received here
uint32_t imageBufferSize; // Size (in bytes) of imageBuffer
int8_t   sync_pin = -1;   // If multiple boards, wire pins together
uint32_t lastFrameSyncTime = 0;

Adafruit_NeoPXL8HDR *leds = NULL; // NeoPXL8HDR object is allocated after reading config

#if defined(ARDUINO_ARCH_RP2040)

// On RP2040, the refresh() function is called in a tight loop on the
// second core (via the loop1() function). The first core is then 100%
// free for animation logic in the regular loop() function.

void loop1() {
  if (leds) leds->refresh();
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

  if (fs == NULL) {
    error_handler("Can't access CIRCUITPY drive", 0);
  } else {
    FatFile file;
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

      sync_pin      = doc["sync_pin"]      | sync_pin;
      led_width     = doc["led_width"]     | led_width;
      led_height    = doc["led_height"]    | led_height;
      led_layout    = doc["led_layout"]    | led_layout;
      video_xoffset = doc["video_xoffset"] | video_xoffset;
      video_yoffset = doc["video_yoffset"] | video_yoffset;
      video_width   = doc["video_width"]   | video_width;
      video_height  = doc["video_height"]  | video_height;
    } // end JSON OK
  } // end filesystem OK

  // Any errors after this point are unrecoverable and program will stop.

  // Dynamically allocate NeoPXL8HDR object
  leds = new Adafruit_NeoPXL8HDR(led_width * led_height / 8, pins, order);
  if (leds == NULL) error_handler("NeoPXL8HDR allocation", 100);

  // Allocate imageBuffer
  imageBufferSize = led_width * led_height * 3;
  imageBuffer = (uint8_t *)malloc(imageBufferSize);
  if (imageBuffer == NULL) error_handler("Image buffer allocation", 200);

  if (!leds->begin()) error_handler("NeoPXL8HDR begin() failed", 500);

  // At this point, everything is fully configured, allocated and started!

  leds->setBrightness(65535, 2.6); // Full brightness, 2.6 gamma factor
  leds->show(); // LEDs off ASAP

  if (sync_pin >= 0) pinMode(sync_pin, INPUT_PULLUP);

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

void loop() {
//
// wait for a Start-Of-Message character:
//
//   '*' = Frame of image data, with frame sync pulse to be sent
//         a specified number of microseconds after reception of
//         the first byte (typically at 75% of the frame time, to
//         allow other boards to fully receive their data).
//         Normally '*' is used when the sender controls the pace
//         of playback by transmitting each frame as it should
//         appear.
//
//   '$' = Frame of image data, with frame sync pulse to be sent
//         a specified number of microseconds after the previous
//         frame sync.  Normally this is used when the sender
//         transmits each frame as quickly as possible, and we
//         control the pacing of video playback by updating the
//         LEDs based on time elapsed from the previous frame.
//
//   '%' = Frame of image data, to be displayed with a frame sync
//         pulse is received from another board.  In a multi-board
//         system, the sender would normally transmit one '*' or '$'
//         message and '%' messages to all other boards, so every
//         Teensy 3.0 updates at the exact same moment.
//
//   '@' = Reset the elapsed time, used for '$' messages.  This
//         should be sent before the first '$' message, so many
//         frames are not played quickly if time as elapsed since
//         startup or prior video playing.
//
//   '?' = Query LED and Video parameters.  Teensy 3.0 responds
//         with a comma delimited list of information.
//
  int startChar = Serial.read();

  if (startChar == '*') {
    // receive a "master" frame - we send the frame sync to other boards
    // the sender is controlling the video pace.  The 16 bit number is
    // how far into this frame to send the sync to other boards.
    unsigned int startAt = micros();
    unsigned int usecUntilFrameSync = 0;
    int count = Serial.readBytes((char *)&usecUntilFrameSync, 2);
    if (count != 2) return;
    count = Serial.readBytes((char *)imageBuffer, imageBufferSize);
    if (count == imageBufferSize) {
      unsigned int endAt = micros();
      unsigned int usToWaitBeforeSyncOutput = 100;
      if (endAt - startAt < usecUntilFrameSync) {
        usToWaitBeforeSyncOutput = usecUntilFrameSync - (endAt - startAt);
      }
      digitalWrite(sync_pin, HIGH);
      pinMode(sync_pin, OUTPUT);
      delayMicroseconds(usToWaitBeforeSyncOutput);
      digitalWrite(sync_pin, LOW);
      // WS2811 update begins immediately after falling edge of frame sync
      convert_and_show();
    }

  } else if (startChar == '$') {
    // receive a "master" frame - we send the frame sync to other boards
    // we are controlling the video pace.  The 16 bit number is how long
    // after the prior frame sync to wait until showing this frame
    unsigned int usecUntilFrameSync = 0;
    int count = Serial.readBytes((char *)&usecUntilFrameSync, 2);
    if (count != 2) return;
    count = Serial.readBytes((char *)imageBuffer, imageBufferSize);
    if (count == imageBufferSize) {
      digitalWrite(sync_pin, HIGH);
      pinMode(sync_pin, OUTPUT);
      uint32_t now, elapsed;
      do {
        now = micros();
        elapsed = now - lastFrameSyncTime;
      } while (elapsed < usecUntilFrameSync); // wait
      lastFrameSyncTime = now;
      digitalWrite(sync_pin, LOW);
      // WS2811 update begins immediately after falling edge of frame sync
      convert_and_show();
    }

  } else if (startChar == '%') {
    // receive a "slave" frame - wait to show it until the frame sync arrives
    pinMode(sync_pin, INPUT_PULLUP);
    unsigned int unusedField = 0;
    int count = Serial.readBytes((char *)&unusedField, 2);
    if (count != 2) return;
    count = Serial.readBytes((char *)imageBuffer, imageBufferSize);
    if (count == imageBufferSize) {
      uint32_t startTime = millis();
      while (digitalRead(sync_pin) != HIGH && (millis() - startTime) < 30) ; // wait for sync high
      while (digitalRead(sync_pin) != LOW && (millis() - startTime) < 30) ;  // wait for sync high->low
      // WS2811 update begins immediately after falling edge of frame sync
      if ((millis() - startTime) < 30) {
        convert_and_show();
      }
    }

  } else if (startChar == '@') {
    // reset the elapsed frame time, for startup of '$' message playing
    lastFrameSyncTime = micros();
  } else if (startChar == '?') {
    // when the video application asks, give it all our info
    // for easy and automatic configuration
    Serial.print(led_width);
    Serial.write(',');
    Serial.print(led_height);
    Serial.write(',');
    Serial.print(led_layout);
    Serial.write(',');
    Serial.print(0);
    Serial.write(',');
    Serial.print(0);
    Serial.write(',');
    Serial.print(video_xoffset);
    Serial.write(',');
    Serial.print(video_yoffset);
    Serial.write(',');
    Serial.print(video_width);
    Serial.write(',');
    Serial.print(video_height);
    Serial.write(',');
    Serial.print(0);
    Serial.write(',');
    Serial.print(0);
    Serial.write(',');
    Serial.print(0);
    Serial.println();

  } else if (startChar >= 0) {
    // discard unknown characters
  }
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
