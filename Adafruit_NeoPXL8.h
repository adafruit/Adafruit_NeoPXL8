/*!
 * @file Adafruit_NeoPXL8.h
 *
 * 8-way concurrent DMA NeoPixel library for SAMD21, SAMD51 and RP2040
 * microcontrollers.
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
#else // SAMD
#include <Adafruit_ZeroDMA.h>
#endif

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
    @return true on successful alloc/init, false otherwise.
  */
#if defined(ARDUINO_ARCH_RP2040)
  boolean begin(PIO pio = pio0);
#else
  boolean begin(void);
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
  boolean canShow(void) const;

  /*!
    @brief  Poll whether last show() data transfer has completed, but not
            necessarily the end-of-data latch. This is the earliest moment
            at which one can stage() the next frame of data.
    @return true if stage() can safely be called, false otherwise.
  */
  boolean canStage(void) const;

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
#else
  Adafruit_ZeroDMA dma;  ///< DMA object
  uint32_t *alignedAddr; ///< long-aligned ptr into dmaBuf (see code)
#endif
  int8_t pins[8];            ///< Pin list for 8 NeoPixel strips
  uint8_t bitmask[8];        ///< Pattern generator bitmask for each pin
  uint8_t *dmaBuf = NULL;    ///< Allocated buffer for pixel data + any extra
  uint16_t brightness = 255; ///< Brightness (stored as 1-256, not 0-255)
  boolean staged;            ///< If set, data is ready for DMA trigger
};

#endif // _ADAFRUIT_NEOPXL8_H_
