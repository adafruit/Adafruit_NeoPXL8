// SPDX-FileCopyrightText: 2017 P Burgess for Adafruit Industries
//
// SPDX-License-Identifier: MIT

/*!
 * @file Adafruit_NeoPXL8.h
 *
 * 8-way concurrent DMA NeoPixel library for SAMD21, SAMD51, RP2040, and
 * ESP32S3 microcontrollers.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * Written by Phil "Paint Your Dragon" Burgess for Adafruit Industries.
 *
 * MIT license, all text here must be included in any redistribution.
 *
 */

#ifndef _ADAFRUIT_NEOPXL8_H_
#define _ADAFRUIT_NEOPXL8_H_

#include <Adafruit_NeoPixel.h>
#if defined(ARDUINO_ARCH_RP2040)
#include "../../hardware_dma/include/hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "pico/mutex.h"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#include <driver/periph_ctrl.h>
#include <esp_private/gdma.h>
#include <esp_rom_gpio.h>
#include <hal/dma_types.h>
#include <hal/gpio_hal.h>
#include <soc/lcd_cam_struct.h>
#else // SAMD
#include <Adafruit_ZeroDMA.h>
#endif

// NEOPXL8 CLASS -----------------------------------------------------------

/*!
  @brief Adafruit_NeoPXL8 is a subclass of Adafruit_NeoPixel containing
         buffers for DMA-formatted 8-way concurrent output. Once a transfer
         is initiated, the original NeoPixel data can then be modified for
         the next frame while the transfer operates in the background.
*/
class Adafruit_NeoPXL8 : public Adafruit_NeoPixel {

public:
  /*!
    @brief  NeoPXL8 constructor. Instantiates a new NeoPXL8 object (must
            follow with a begin() call to alloc buffers and init hardware).
    @param  n
            Length of each NeoPixel strand (total number of pixels will be
            8X this).
    @param  p
            Optional int8_t array of eight pin numbers for NeoPixel strands
            0-7. There are specific hardware limitations as to which pins
            can be used, see the example sketch. If fewer than 8 outputs are
            needed, assign a value of -1 to the unused outputs, keeping in
            mind that this will always still use the same amount of memory
            as 8-way output. If unspecified (or if NULL is passed), a
            default 8-pin setup will be used (see example sketch).
            On RP2040, these are GP## numbers, not necessarily the digital
            pin numbers silkscreened on the board.
    @param  t
            NeoPixel color data order, same as in Adafruit_NeoPixel library
            (optional, default is GRB).
  */
  Adafruit_NeoPXL8(uint16_t n, int8_t *p = NULL, neoPixelType t = NEO_GRB);
  ~Adafruit_NeoPXL8(void);

  /*!
    @brief  Allocate buffers and initialize hardware for NeoPXL8 output.
    @param  dbuf  If true, 2X DMA buffers are allocated so that a frame
                  can be staged while the prior is in mid-transfer.
                  Might yield slightly improved frame rates in some cases,
                  others just waste RAM. Super esoteric and mostly for
                  NeoPXL8HDR's use. Currently ignored on SAMD.
    @return true on successful alloc/init, false otherwise.
  */
#if defined(ARDUINO_ARCH_RP2040)
  bool begin(bool dbuf = false, PIO pio_instance = pio0);
#else
  bool begin(bool dbuf = false);
#endif

  /*!
    @brief  Process and issue new data to the NeoPixel strands.
  */
  void show(void);

  /*!
    @brief  Preprocess NeoPixel data into DMA-ready format, but do not issue
            to strands yet. Esoteric but potentially useful if aiming to
            have pixels refresh at precise intervals (since preprocessing is
            not deterministic) -- e.g. 60 Hz -- follow with a call to show()
            when the actual data transfer is to occur.
  */
  void stage(void);

  /*!
    @brief  Poll whether last show() transfer has completed and library is
            idle. Of questionable utility, but provided for compatibility
            with similarly questionable function in the NeoPixel library.
    @return true if show() can be called without blocking, false otherwise.
  */
  bool canShow(void) const;

  /*!
    @brief  Poll whether last show() data transfer has completed, but not
            necessarily the end-of-data latch. This is the earliest moment
            at which one can stage() the next frame of data.
    @return true if stage() can safely be called, false otherwise.
  */
  bool canStage(void) const;

  // Brightness is stored differently here than in normal NeoPixel library.
  // In either case it's *specified* the same: 0 (off) to 255 (brightest).
  // Classic NeoPixel rearranges this internally so 0 is max, 1 is off and
  // 255 is just below max...it's a decision based on how fixed-point math
  // is handled in that code. Here it's stored internally as 1 (off) to
  // 256 (brightest), requiring a 16-bit value.

  /*!
    @brief  Set strip brightness for subsequent show() calls. Unlike the
            Adafruit_NeoPixel library, this function is non-destructive --
            original color values passed to setPixelColor() will always be
            accurately returned by getPixelColor(), even if brightness is
            adjusted.
    @param  b
            Brightness, from 0 (off) to 255 (maximum).
  */
  void setBrightness(uint8_t b) { brightness = (uint16_t)b + 1; }

  /*!
    @brief  Query brightness value last assigned with setBrightness().
    @return Brightness, from 0 (off) to 255 (maximum).
  */
  uint8_t getBrightness(void) const { return brightness - 1; }

  /*!
    @brief  Change the NeoPixel end-of-data latch period. Here be dragons.
    @param  us  Latch time in microseconds. Different manufacturers and
                generations of pixels may have different end-of-data latch
                periods. For example, early WS2812 pixels recommend a 50 uS
                latch, late-model WS2812Bs suggest 300 (the default value).
                A shorter latch may allow very slightly faster refresh rates
                (a few percent at best -- strand length is a much bigger
                influence). Check datasheet for your specific pixels to get
                a recommended figure, then you can try empirically dialing
                down from there, with the understanding that this change may
                not work with other pixels. What you ABSOLUTELY MUST NOT DO
                is rely on the bare minimum value that works on your bench,
                because addressable LEDs are affected by environmental
                factors like temperature; an installation relying on a bare-
                minimum figure WILL fail when relocated, e.g. Burning Man.
                ALWAYS allow a generous overhead. Better yet, don't mess
                with it.
  */
  void setLatchTime(uint16_t us = 300) { latchtime = us; };

#if defined(ARDUINO_ARCH_RP2040)
  /*!
    @brief  Callback function used internally by the DMA transfer interrupt.
            User code shouldn't access this, but it couldn't be put in the
            protected section below because IRQ is outside the class context.
            Ignore it, thanks.
    @return None (void).
  */
  void dma_callback(void);
#endif

protected:
#if defined(ARDUINO_ARCH_RP2040)
  PIO pio;                       ///< PIO peripheral
  uint8_t sm;                    ///< State machine #
  int dma_channel;               ///< DMA channel #
  dma_channel_config dma_config; ///< DMA configuration
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  gdma_channel_handle_t dma_chan; ///< DMA channel
  dma_descriptor_t *desc;         ///< DMA descriptor pointer
  uint8_t *allocAddr;             ///< Allocated buf into which dmaBuf points
  uint32_t *alignedAddr[2];       ///< long-aligned ptrs into dmaBuf
#else // SAMD
  Adafruit_ZeroDMA dma;     ///< DMA object
  DmacDescriptor *desc;     ///< DMA descriptor pointer
  uint8_t *allocAddr;       ///< Allocated buffer into which dmaBuf points
  uint32_t *alignedAddr[2]; ///< long-aligned ptrs into dmaBuf
#endif
  int8_t pins[8];                    ///< Pin list for 8 NeoPixel strips
  uint8_t bitmask[8];                ///< Pattern generator bitmask for each pin
  uint8_t *dmaBuf[2] = {NULL, NULL}; ///< Buffer for pixel data + any extra
  uint16_t brightness = 255;         ///< Brightness (stored 1-256, not 0-255)
  bool staged;                       ///< If set, data is ready for DMA trigger
  uint16_t latchtime = 300;          ///< Pixel data latch time, microseconds
  uint8_t dbuf_index = 0;            ///< 0/1 DMA buffer index
};

// NEOPXL8HDR CLASS --------------------------------------------------------

/*!
  @brief Adafruit_NeoPXL8HDR is a subclass of Adafruit_NeoPXL8 with
         additions for 16-bits-per-channel color, temporal dithering,
         frame blending and gamma correction. This requires inordinate RAM,
         and the frequent need for refreshing makes it best suited for
         multi-core chips (e.g. RP2040).
*/
class Adafruit_NeoPXL8HDR : public Adafruit_NeoPXL8 {

public:
  /*!
    @brief  NeoPXL8HDR constructor. Instantiates a new NeoPXL8HDR object
            (must follow with a begin() call to alloc buffers and init
            hardware).
    @param  n
            Length of each NeoPixel strand (total number of pixels will be
            8X this).
    @param  p
            Optional int8_t array of eight pin numbers for NeoPixel strands
            0-7. There are specific hardware limitations as to which pins
            can be used, see the example sketch. If fewer than 8 outputs are
            needed, assign a value of -1 to the unused outputs, keeping in
            mind that this will always still use the same amount of memory
            as 8-way output. If unspecified (or if NULL is passed), a
            default 8-pin setup will be used (see example sketch).
            On RP2040, these are GP## numbers, not necessarily the digital
            pin numbers silkscreened on the board.
    @param  t
            NeoPixel color data order, same as in Adafruit_NeoPixel library
            (optional, default is GRB).
  */
  Adafruit_NeoPXL8HDR(uint16_t n, int8_t *p = NULL, neoPixelType t = NEO_GRB);
  ~Adafruit_NeoPXL8HDR();

  /*!
    @brief  Allocate buffers and initialize hardware for NeoPXL8 output.
    @param  blend  If true, provide frame-to-frame blending via the
                   refresh() function between show() calls (uses more RAM).
                   If false (default), no blending. This is useful ONLY if
                   sketch can devote time to many refresh() calls, e.g. on
                   multicore RP2040.
    @param  bits   Number of bits for temporal dithering, 0-8. Higher values
                   provide more intermediate shades but slower refresh;
                   dither becomes more apparent. Default is 4, providing
                   12-bit effective range per channel.
    @param  dbuf   If true, 2X DMA buffers are allocated so that a frame
                   can be NeoPXL8-staged while the prior is in mid-transfer.
                   Might yield slightly improved frame rates in some cases,
                   others just waste RAM. Currently ignored on SAMD.
    @return true on successful alloc/init, false otherwise.
  */
#if defined(ARDUINO_ARCH_RP2040)
  bool begin(bool blend = false, uint8_t bits = 4, bool dbuf = false,
             PIO pio_instance = pio0);
#else
  bool begin(bool blend = false, uint8_t bits = 4, bool dbuf = false);
#endif

  /*!
    @brief  Set peak output brightness for all channels (RGB and W if
            present) to the same value. Existing gamma setting is unchanged.
            This is for compatibility with existing NeoPixel or NeoPXL8
            sketches moved directly to NeoPXL8HDR. New code may prefer
            one of the other setBrightness() invocations, which provide
            16-bit adjustment plus gamma correction.
    @param  b  Brightness value, 0-255. This is the LEDs' maximum duty
               cycle and is not itself gamma-corrected.
    @note   This is typically set once at program startup and is not
            intended as an animation effect in itself, partly because the
            top value is an duty cycle and not a gamma-corrected level.
            Also, pixels will likely flash if this is called on an active
            NeoPXL8HDR object, as gamma tables are rewritten while
            blending/dithering is occurring. Well-designed animation code
            should handle its own fades!
  */
  void setBrightness(uint8_t b);

  /*!
    @brief  Set peak output brightness for all channels (RGB and W if
            present) to the same value, and set gamma curve.
    @param  b  Brightness value, 0-65535. This is the LEDs' maximum duty
               cycle and is not itself gamma-corrected.
    @param  y  Gamma exponent; 1.0 is linear, 2.6 is a typical correction
               factor for NeoPixels.
    @note   This is typically set once at program startup and is not
            intended as an animation effect in itself, partly because the
            top value is an duty cycle and not a gamma-corrected level.
            Also, pixels will likely flash if this is called on an active
            NeoPXL8HDR object, as gamma tables are rewritten while
            blending/dithering is occurring. Well-designed animation code
            should handle its own fades!
            Note to future self: do NOT provide a default gamma value here,
            it MUST be specified, even if 1.0. This avoids ambiguity with
            the back-compatible setBrightness(uint8_t) above without weird
            explicit casts in user code.
  */
  void setBrightness(uint16_t b, float y);

  /*!
    @brief  Set peak output brightness for R, G, B channels independently.
            Existing gamma setting is unchanged. W brightness, if present,
            is unchanged.
    @param  r  Red brightness value, 0-65535. This is the maximum duty cycle
               and is not itself gamma-corrected.
    @param  g  Green brightness value, 0-65535.
    @param  b  Blue brightness value, 0-65535.
    @note   This is typically set once at program startup and is not
            intended as an animation effect in itself, partly because the
            top value is an duty cycle and not a gamma-corrected level.
            Also, pixels will likely flash if this is called on an active
            NeoPXL8HDR object, as gamma tables are rewritten while
            blending/dithering is occurring. Well-designed animation code
            should handle its own fades!
  */
  void setBrightness(uint16_t r, uint16_t g, uint16_t b);

  /*!
    @brief  Set peak output brightness for R, G, B, W channels independently.
            Existing gamma setting is unchanged.
    @param  r  Red brightness value, 0-65535. This is the maximum duty cycle
               and is not itself gamma-corrected.
    @param  g  Green brightness value, 0-65535.
    @param  b  Blue brightness value, 0-65535.
    @param  w  White brightness value, 0-65535. Ignored if NeoPixel strips
               are RGB variety with no W.
    @note   This is typically set once at program startup and is not
            intended as an animation effect in itself, partly because the
            top value is an duty cycle and not a gamma-corrected level.
            Also, pixels will likely flash if this is called on an active
            NeoPXL8HDR object, as gamma tables are rewritten while
            blending/dithering is occurring. Well-designed animation code
            should handle its own fades!
  */
  void setBrightness(uint16_t r, uint16_t g, uint16_t b, uint16_t w);

  /*!
    @brief  Set peak output brightness for R, G, B channels independently,
            and set gamma curve. W brightness, if present, is unchanged.
    @param  r  Red brightness value, 0-65535. This is the maximum duty cycle
               and is not itself gamma-corrected.
    @param  g  Green brightness value, 0-65535.
    @param  b  Blue brightness value, 0-65535.
    @param  y  Gamma exponent; 1.0 is linear, 2.6 is a typical correction
               factor for NeoPixels.
    @note   This is typically set once at program startup and is not
            intended as an animation effect in itself, partly because the
            top value is an duty cycle and not a gamma-corrected level.
            Also, pixels will likely flash if this is called on an active
            NeoPXL8HDR object, as gamma tables are rewritten while
            blending/dithering is occurring. Well-designed animation code
            should handle its own fades!
  */
  void setBrightness(uint16_t r, uint16_t g, uint16_t b, float y);

  /*!
    @brief  Set peak output brightness for R, G, B, W channels independently,
            and set gamma curve.
    @param  r  Red brightness value, 0-65535. This is the maximum duty cycle
               and is not itself gamma-corrected.
    @param  g  Green brightness value, 0-65535.
    @param  b  Blue brightness value, 0-65535.
    @param  w  White brightness value, 0-65535. Ignored if NeoPixel strips
               are RGB variety with no W.
    @param  y  Gamma exponent; 1.0 is linear, 2.6 is a typical correction
               factor for NeoPixels.
    @note   This is typically set once at program startup and is not
            intended as an animation effect in itself, partly because the
            top value is an duty cycle and not a gamma-corrected level.
            Also, pixels will likely flash if this is called on an active
            NeoPXL8HDR object, as gamma tables are rewritten while
            blending/dithering is occurring. Well-designed animation code
            should handle its own fades!
  */
  void setBrightness(uint16_t r, uint16_t g, uint16_t b, uint16_t w, float y);

  /*!
    @brief  Provide new pixel data to the refresh handler (but does not
            actually refresh the strip - use refresh() for that).
  */
  void show(void);

  /*!
    @brief  Dither (and blend, if enabled) and issue new data to the
            NeoPixel strands.
  */
  void refresh(void);

  /*!
    @brief  Overload the stage() function from Adafruit_NeoPXL8.
            Does nothing in NeoPXL8HDR, provided for compatibility.
  */
  void stage(void) const {};

  /*!
    @brief   Overload the canStage() function from Adafruit_NeoPXL8.
             Does nothing in NeoPXL8HDR, provided for compatibility.
    @return  true always.
  */
  bool canStage(void) const { return true; }

  /*!
    @brief   Overload the canShow() function from Adafruit_NeoPXL8.
             Does nothing in NeoPXL8HDR, provided for compatibility.
    @return  true always.
  */
  bool canShow(void) const { return true; }

  /*!
    @brief   Set a pixel's color using separate red, green and blue
             components. If using RGBW pixels, white will be set to 0.
    @param   n  Pixel index, starting from 0.
    @param   r  Red brightness, 0 = minimum (off), 255 = maximum.
    @param   g  Green brightness, 0 = minimum (off), 255 = maximum.
    @param   b  Blue brightness, 0 = minimum (off), 255 = maximum.
  */
  void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b);

  /*!
    @brief   Set a pixel's color using separate red, green, blue and white
             components (for RGBW NeoPixels).
    @param   n  Pixel index, starting from 0.
    @param   r  Red brightness, 0 = minimum (off), 255 = maximum.
    @param   g  Green brightness, 0 = minimum (off), 255 = maximum.
    @param   b  Blue brightness, 0 = minimum (off), 255 = maximum.
    @param   w  White brightness, 0 = minimum (off), 255 = maximum, ignored
                if using RGB pixels.
  */
  void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b, uint8_t w);

  /*!
    @brief   Set a pixel's color using a 32-bit 'packed' RGB or RGBW value.
    @param   n  Pixel index, starting from 0.
    @param   c  32-bit color value. Most significant byte is white (for RGBW
                pixels) or ignored (for RGB pixels), next is red, then green,
                and least significant byte is blue.
  */
  void setPixelColor(uint16_t n, uint32_t c);

  /*!
    @brief  Set a pixel's color using a 16-bit R, G, B and optionally W
            values.
    @param  n  Pixel index, starting from 0.
    @param  r  16-bit red component.
    @param  g  16-bit green component.
    @param  b  16-bit blue component.
    @param  w  16-bit white component (optional argument, RGBW strips only,
               ignored if RGB).
  */
  void set16(uint16_t n, uint16_t r, uint16_t g, uint16_t b, uint16_t w = 0);

  /*!
    @brief   Query the color of a previously-set pixel, reduced to 8-bit
             components. This is for compatibility with existing NeoPixel
             or NeoPXL8 sketches moved directly to NeoPXL8HDR. New code
             may prefer get16() instead, which returns the pristine true
             16-bit value previously set.
    @param   n  Index of pixel to read (0 = first).
    @return  'Packed' 32-bit RGB or WRGB value. Most significant byte is
             white (for RGBW pixels) or 0 (for RGB pixels), next is red,
             then green, and least significant byte is blue.
    @note    Unlike the NeoPixel library, brightness scaling is not a
             destructive operation; the data stored is what was set.
             However, color components are reduced from 16 to 8 bits.
             "16-bit-aware" code should use get16() instead.
  */
  uint32_t getPixelColor(uint16_t n) const;

  /*!
    @brief   Query the color of a previously-set pixel, returning the
             original 16-bit values.
    @param   n  Index of pixel to read (0 = first).
    @param   r  Pointer to unsigned 16-bit variable to hold red result,
                must be non-NULL, no check performed.
    @param   g  Pointer to unsigned 16-bit variable to hold green result,
                must be non-NULL, no check performed.
    @param   b  Pointer to unsigned 16-bit variable to hold blue result,
                must be non-NULL, no check performed.
    @param   w  Pointer to unsigned 16-bit variable to hold white result
                (if RGBW strip, else 0 if RGB), or NULL to ignore.
    @note    Unlike the NeoPixel library, the value returned is always equal
             to the value previously set; brightness scaling is not a
             destructive operation with this library.
  */
  void get16(uint16_t n, uint16_t *r, uint16_t *g, uint16_t *b,
             uint16_t *w = NULL) const;

  /*!
    @brief   Get a pointer directly to the NeoPXL8HDR data buffer in RAM.
             Pixel data is unsigned 16-bit, always RGB (3 words/pixel) or
             RGBW (4 words/pixel) order; different NeoPixel hardware color
             orders are covered by the library and do not need to be handled
             in calling code. Nice.
    @return  Pointer to NeoPixel buffer (uint16_t* array).
    @note    This is for high-performance applications where calling set16()
             or setPixelColor() on every single pixel would be too slow.
             There is no bounds checking on the array, creating tremendous
             potential for mayhem if one writes past the ends of the buffer.
             Great power, great responsibility and all that.
  */
  uint16_t *getPixels(void) const { return pixel_buf[2]; }

  /*!
    @brief   Query overall display refresh rate in frames-per-second.
             This is only an estimate and requires a moment to stabilize;
             will initially be 0. It's really only helpful on RP2040 where
             the refresh loop runs full-tilt on its own core; on SAMD,
             refresh is handled with a fixed timer interrupt.
    @return  Integer frames (pixel refreshes) per second.
  */
  uint32_t getFPS(void) const { return fps; }

  /*!
    @brief   Fill the whole NeoPixel strip with 0 / black / off.
    @note    Overloaded from Adafruit_NeoPixel because stored different here.
  */
  void clear(void) { memset(pixel_buf[2], 0, numBytes * sizeof(uint16_t)); }

protected:
  /*!
    @brief  Recalculate the tables used for gamma correction and temporal
            dithering. Used internally, not for user code. Invoked by the
            brightness/gamma-setting functions.
  */
  void calc_gamma_table(void);
  float gfactor;                               ///< Gamma: 1.0=linear, 2.6=typ
  uint16_t *pixel_buf[3] = {NULL, NULL, NULL}; ///< Buffer for NeoPXL8 staging
  uint16_t *dither_table = NULL;               ///< Temporal dithering lookup
  uint32_t last_show_time = 0;                 ///< micros() @ last show()
  uint32_t avg_show_interval = 0;              ///< Avergage uS between show()
  uint32_t fps = 0;                            ///< Estimated refreshes/second
  uint32_t last_fps_time = 0;                  ///< micros() @ last estimate
  uint16_t g16[4][256];                        ///< Gamma look up table
  uint16_t brightness_rgbw[4];                 ///< Peak brightness/channel
  uint8_t dither_bits;                         ///< # bits for temporal dither
  uint8_t dither_index = 0;                    ///< Current dither_table pos
  uint8_t stage_index = 0;                     ///< Ping-pong pixel_buf
  volatile bool new_pixels = true;             ///< show()/refresh() sync
#if defined(ARDUINO_ARCH_RP2040)
  mutex_t mutex; ///< For synchronizing cores
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  SemaphoreHandle_t mutex; ///< For synchronizing cores
#endif
};

// The DEFAULT_PINS macros provide shortcuts for the most commonly-used pin
// lists on certain boards. For example, with a Feather M0, the default list
// will match an unaltered, factory-fresh NeoPXL8 FeatherWing M0. If ANY pins
// are changed on the FeatherWing, or if using a different pin sequence than
// these defaults, a user sketch must provide its own correct pin list.
// These may work for sloppy quick code but are NOT true in all situations!
#if defined(ARDUINO_ADAFRUIT_FEATHER_RP2040_SCORPIO)
#define NEOPXL8_DEFAULT_PINS                                                   \
  { 16, 17, 18, 19, 20, 21, 22, 23 }
#elif defined(ADAFRUIT_FEATHER_M0) || defined(ARDUINO_SAMD_FEATHER_M0_EXPRESS)
#define NEOPXL8_DEFAULT_PINS                                                   \
  { PIN_SERIAL1_RX, PIN_SERIAL1_TX, MISO, 13, 5, SDA, A4, A3 }
#elif defined(ADAFRUIT_FEATHER_M4_EXPRESS) ||                                  \
    defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S3) ||                               \
    defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S3_NOPSRAM)
#define NEOPXL8_DEFAULT_PINS                                                   \
  { SCK, 5, 9, 6, 13, 12, 11, 10 }
#elif defined(ADAFRUIT_METRO_M4_EXPRESS)
#define NEOPXL8_DEFAULT_PINS                                                   \
  { 7, 4, 5, 6, 3, 2, 10, 11 }
#elif defined(ADAFRUIT_GRAND_CENTRAL_M4)
#define NEOPXL8_DEFAULT_PINS                                                   \
  { 30, 31, 32, 33, 36, 37, 34, 35 }
#elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040)
#define NEOPXL8_DEFAULT_PINS                                                   \
  { 6, 7, 9, 8, 13, 12, 11, 10 }
#else
#define NEOPXL8_DEFAULT_PINS                                                   \
  { 0, 1, 2, 3, 4, 5, 6, 7 } ///< Generic pin list
#endif

#endif // _ADAFRUIT_NEOPXL8_H_
