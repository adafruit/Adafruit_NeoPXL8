// SPDX-FileCopyrightText: 2023 P Burgess for Adafruit Industries
//
// SPDX-License-Identifier: MIT

/*!
 * @file FFS.cpp
 *
 * This is a barebones library to:
 *
 * - Make a CircuitPython-capable board's [F]lash [F]ile[S]ystem accessible
 *   to Arduino code.
 * - Make this same drive accessible to a host computer over USB.
 *
 * This is to cover the most common use case, with least documentation, for
 * non-technical users: if a board supports CircuitPython, Arduino code and
 * a host computer can both access that drive. Leans on CircuitPython (pre-
 * built for just about everything) for flash formatting rather than needing
 * a unique step. That's it. Not for SD cards, other flash partitioning, etc.
 * Those can always be implemented manually using Adafruit_TinyUSB library,
 * but this is not the code for it. Keeping it simple.
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

#if defined(USE_TINYUSB) || defined(ESP32)

#include "FFS.h"
#include <Adafruit_SPIFlash.h>
#include <Adafruit_TinyUSB.h>
#if defined(_SAMD21_)
#include <Adafruit_InternalFlash.h>
// These apply to M0 boards only, ignored elsewhere:
#define INTERNAL_FLASH_FS_SIZE (64 * 1024)
#define INTERNAL_FLASH_FS_START (0x00040000 - 256 - 0 - INTERNAL_FLASH_FS_SIZE)
#endif

// Library state is maintained in a few global variables (rather than in the
// FFS class) because there's only one filesystem instance anyway, and also
// that the mass storage callbacks are C and require access to this info.

// For several major board types, the correct flash transport to use
// is known at compile-time:
#if defined(ARDUINO_ARCH_ESP32)
static Adafruit_FlashTransport_ESP32 transport;
#elif defined(ARDUINO_ARCH_RP2040)
static Adafruit_FlashTransport_RP2040_CPY transport;
#elif defined(EXTERNAL_FLASH_USE_QSPI)
static Adafruit_FlashTransport_QSPI transport;
#elif defined(EXTERNAL_FLASH_USE_CS) && defined(EXTERNAL_FLASH_USE_SPI)
static Adafruit_FlashTransport_SPI transport(EXTERNAL_FLASH_USE_CS,
                                             EXTERNAL_FLASH_USE_SPI);
#else
// If not one of the above board types, nor EXTERNAL_FLASH_USE_* defined,
// it's probably a SAMD21. Some can be "Haxpress" modified to add external
// SPI flash & run a special CircuitPython build, but Arduino IDE lacks a
// distinct special board select...at compile-time, indistinguishable from
// a stock M0 board, could go either way. Thus, the transport and flash
// members are pointers, initialized at run-time depending on arguments
// passed (or not) to the begin() function.
#define HAXPRESS
static Adafruit_FlashTransport_SPI *transport;
static void *flash;
#endif
#if !defined HAXPRESS
static Adafruit_SPIFlash flash(&transport);
#endif

static Adafruit_USBD_MSC usb_msc;
static FatVolume fatfs;
static bool started = 0;
static bool changeflag = 0;

#if defined(HAXPRESS)

// On Haxpress-capable boards, flash type (internal vs SPI) isn't known
// at compile time, so callbacks are provided for both, and one set or
// other is installed in begin().

static int32_t msc_read_cb_internal(uint32_t lba, void *buffer,
                                    uint32_t bufsize) {
  return ((Adafruit_InternalFlash *)flash)
                 ->readBlocks(lba, (uint8_t *)buffer, bufsize / 512)
             ? bufsize
             : -1;
}

static int32_t msc_write_cb_internal(uint32_t lba, uint8_t *buffer,
                                     uint32_t bufsize) {
  changeflag = 1;
  return ((Adafruit_InternalFlash *)flash)
                 ->writeBlocks(lba, buffer, bufsize / 512)
             ? bufsize
             : -1;
}

static void msc_flush_cb_internal(void) {
  ((Adafruit_InternalFlash *)flash)->syncBlocks();
  fatfs.cacheClear();
}

static int32_t msc_read_cb_spi(uint32_t lba, void *buffer, uint32_t bufsize) {
  return ((Adafruit_SPIFlash *)flash)
                 ->readBlocks(lba, (uint8_t *)buffer, bufsize / 512)
             ? bufsize
             : -1;
}

static int32_t msc_write_cb_spi(uint32_t lba, uint8_t *buffer,
                                uint32_t bufsize) {
  changeflag = 1;
  return ((Adafruit_SPIFlash *)flash)->writeBlocks(lba, buffer, bufsize / 512)
             ? bufsize
             : -1;
}

static void msc_flush_cb_spi(void) {
  ((Adafruit_SPIFlash *)flash)->syncBlocks();
  fatfs.cacheClear();
}

#else

// Flash type is known at compile time. Simple callbacks.

static int32_t msc_read_cb(uint32_t lba, void *buffer, uint32_t bufsize) {
  return flash.readBlocks(lba, (uint8_t *)buffer, bufsize / 512) ? bufsize : -1;
}

static int32_t msc_write_cb(uint32_t lba, uint8_t *buffer, uint32_t bufsize) {
  changeflag = 1;
  return flash.writeBlocks(lba, buffer, bufsize / 512) ? bufsize : -1;
}

static void msc_flush_cb(void) {
  flash.syncBlocks();
  fatfs.cacheClear();
}

#endif // end !HAXPRESS

FatVolume *FFS::begin(int cs, void *spi) {

  if (started)
    return &fatfs; // Don't re-init if already running

  started = 1;

#if defined(HAXPRESS)

  if ((cs >= 0) && (spi != NULL)) { // External flash
    if ((transport = new Adafruit_FlashTransport_SPI(cs, (SPIClass *)spi))) {
      if ((flash = (void *)new Adafruit_SPIFlash(transport))) {
        ((Adafruit_SPIFlash *)flash)->begin();
        usb_msc.setID("Adafruit", "External Flash", "1.0");
        usb_msc.setReadWriteCallback(msc_read_cb_spi, msc_write_cb_spi,
                                     msc_flush_cb_spi);
        usb_msc.setCapacity(((Adafruit_SPIFlash *)flash)->size() / 512, 512);
        usb_msc.setUnitReady(true);
        usb_msc.begin();
        if (fatfs.begin((Adafruit_SPIFlash *)flash))
          return &fatfs;
      }    // end if new flash
    }      // end if new transport
  } else { // Internal flash
    if ((flash = (void *)new Adafruit_InternalFlash(INTERNAL_FLASH_FS_START,
                                                    INTERNAL_FLASH_FS_SIZE))) {
      ((Adafruit_InternalFlash *)flash)->begin();
      usb_msc.setID("Adafruit", "Internal Flash", "1.0");
      usb_msc.setReadWriteCallback(msc_read_cb_internal, msc_write_cb_internal,
                                   msc_flush_cb_internal);
      usb_msc.setCapacity(((Adafruit_InternalFlash *)flash)->size() / 512, 512);
      usb_msc.setUnitReady(true);
      usb_msc.begin();
      if (fatfs.begin((Adafruit_InternalFlash *)flash))
        return &fatfs;
    } // end if new flash
  }

#else

  flash.begin();
  usb_msc.setID("Adafruit", "Onboard Flash", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
  usb_msc.setCapacity(flash.size() / 512, 512);
  usb_msc.setUnitReady(true);
  usb_msc.begin();

  if (fatfs.begin(&flash))
    return &fatfs;

#endif // end HAXPRESS

  started = 0;
  return NULL;
}

bool FFS::changed(void) { return changeflag; }
void FFS::change_ack(void) { changeflag = 0; }

#endif // end USE_TINYUSB || ESP32
