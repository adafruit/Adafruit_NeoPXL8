// FIRST TIME HERE? START WITH THE NEOPXL8 strandtest EXAMPLE INSTEAD!
// That code explains and helps troubshoot wiring and NeoPixel color format.

// This is a companion to "move2serial" in the extras/Processing folder.

#include <Adafruit_NeoPXL8_config.h> // Also includes Adafruit_NeoPXL8.h and ArduinoJson.h

// This example is minimally adapted from one in PJRC's OctoWS2811 Library.
// Original comments appear first, and Adafruit_NeoPXL8 changes follow that.

/*  OctoWS2811 VideoDisplay.ino - Video on LEDs, from a PC, Mac, Raspberry Pi
    http://www.pjrc.com/teensy/td_libs_OctoWS2811.html
    Copyright (c) 2013 Paul Stoffregen, PJRC.COM, LLC

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
Adafruit_NeoPXL8 update:

    Reads from neoplx8.config on CIRCUITPY filesystem (if present),
    else defaults are used.
    Bunch of #defines originally here (LED_WIDTH, LED_HEIGHT, etc.)
    have been made variables, but otherwise the code structure is
    minimally changed.
    LEDs declared dynamically.
*/

#if 0
// The actual arrangement of the LEDs connected to this Teensy 3.0 board.
// LED_HEIGHT *must* be a multiple of 8.  When 16, 24, 32 are used, each
// strip spans 2, 3, 4 rows.  LED_LAYOUT indicates the direction the strips
// are arranged.  If 0, each strip begins on the left for its first row,
// then goes right to left for its second row, then left to right,
// zig-zagging for each successive row.
#define LED_WIDTH      30   // number of LEDs horizontally
#define LED_HEIGHT     16   // number of LEDs vertically (must be multiple of 8)
#define LED_LAYOUT     1    // 0 = even rows left->right, 1 = even rows right->left

// The portion of the video image to show on this set of LEDs.  All 4 numbers
// are percentages, from 0 to 100.  For a large LED installation with many
// Teensy 3.0 boards driving groups of LEDs, these parameters allow you to
// program each Teensy to tell the video application which portion of the
// video it displays.  By reading these numbers, the video application can
// automatically configure itself, regardless of which serial port COM number
// or device names are assigned to each Teensy 3.0 by your operating system.
#define VIDEO_XOFFSET  0
#define VIDEO_YOFFSET  0       // display entire image
#define VIDEO_WIDTH    100
#define VIDEO_HEIGHT   100

//#define VIDEO_XOFFSET  0
//#define VIDEO_YOFFSET  0     // display upper half
//#define VIDEO_WIDTH    100
//#define VIDEO_HEIGHT   50

//#define VIDEO_XOFFSET  0
//#define VIDEO_YOFFSET  50    // display lower half
//#define VIDEO_WIDTH    100
//#define VIDEO_HEIGHT   50
#endif

uint16_t led_width, led_height;
uint8_t  led_layout;
float    video_xoffset, video_yoffset, video_width, video_height;
uint8_t *imageBuffer;
uint32_t imageBufferSize;
uint32_t lastFrameSyncTime = 0;
int8_t   sync_pin = -1;

Adafruit_NeoPXL8 *leds;

void error_handler(const char *message, uint16_t speed) {
  Serial.print("Error: ");
  Serial.println(message);
  pinMode(LED_BUILTIN, OUTPUT);
  for (;;) digitalWrite(LED_BUILTIN, (millis() / speed) & 1);
}

void setup() {
  // MUST do this before starting serial
  DynamicJsonDocument doc = NeoPXL8configRead();

  Serial.begin(115200);
  Serial.setTimeout(50);

  const char* error = doc["ERROR"];
  if (error) error_handler(error, 20);

  int8_t pins[8] = NEOPXL8_DEFAULT_PINS;
  NeoPXL8configPins(doc["pins"], pins);
  uint16_t order = NeoPXL8configColorOrder(doc["order"], NEO_BGR);

  sync_pin      = doc["video"]["sync_pin"]      | -1;
  led_width     = doc["video"]["led_width"]     | 30;
  led_height    = doc["video"]["led_height"]    | 16;
  led_layout    = doc["video"]["led_layout"]    | 0;
  video_xoffset = doc["video"]["video_xoffset"] | 0;
  video_yoffset = doc["video"]["video_yoffset"] | 0;
  video_width   = doc["video"]["video_width"]   | 100;
  video_height  = doc["video"]["video_height"]  | 100;

  leds = new Adafruit_NeoPXL8(led_width * led_height / 8, pins, order);
  if (leds == NULL) error_handler("NeoPXL8 allocation", 100);

  imageBufferSize = led_width * led_height * 3;
  imageBuffer = (uint8_t *)malloc(imageBufferSize);
  if (imageBuffer == NULL) error_handler("Image buffer allocation", 200);

  if (!leds->begin()) error_handler("NeoPXL8 begin() failed", 500);

  leds->show(); // LEDs off ASAP

  if (sync_pin >= 0) pinMode(sync_pin, INPUT_PULLUP);
}

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
        uint8_t r = leds->gamma8(*ptr++);
        uint8_t g = leds->gamma8(*ptr++);
        uint8_t b = leds->gamma8(*ptr++);
        leds->setPixelColor(pixelIndex, r, g, b);
      }
    }
  } else {
    for (int y=0; y<led_height; y++) {
      for (int x=0; x<led_width; x++) {
        // Even rows are left-to-right, odd are right-to-left
        int pixelIndex = (y & 1) ? (y + 1) * led_width - 1 - x : y * led_width + x;
        uint8_t r = leds->gamma8(*ptr++);
        uint8_t g = leds->gamma8(*ptr++);
        uint8_t b = leds->gamma8(*ptr++);
        leds->setPixelColor(pixelIndex, r, g, b);
      }
    }
  }
  leds->show();
}
