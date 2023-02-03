// SPDX-FileCopyrightText: 2017 P Burgess for Adafruit Industries
//
// SPDX-License-Identifier: MIT

/*!
 * @file Adafruit_NeoPXL8_config.cpp
 *
 * Helper code to assist in reading NeoPXL8/NeoPXL8HDR setup from JSON
 * configuration file (rather than hardcoded in sketch). This is
 * intentionally NOT in the main Adafruit_NeoPXL8 code as it incurs some
 * extra dependencies that would complicate basic NeoPXL8 use.
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

#include "Adafruit_NeoPXL8_config.h"
#define ARDUINOJSON_ENABLE_COMMENTS 1 ///< Allow comments in JSON file
#include <ArduinoJson.h>

#if defined(USE_TINYUSB) || defined(ESP32)

#include <Adafruit_TinyUSB.h>

#if defined(ADAFRUIT_FEATHER_M0) || defined(ADAFRUIT_TRINKET_M0) ||            \
    defined(ADAFRUIT_GEMMA_M0) || defined(ADAFRUIT_QTPY_M0)

// A few M0 boards handle a tiny CIRCUITPY drive in the MCU's internal flash.
// The Feather M0 is the only one of these I could reasonably see being used
// with NeoPXL8 (the others have few GPIO and I haven't even checked to see
// which pins IF ANY are pattern generator compatible), but they're included
// in the check above for good measure. The QT Py M0 "Haxpress" (external
// flash added under board) is not correctly handled in this regard because
// there's no distinct board menu selection for it, and because come ON.
// Trinkey M0 boards are also skipped, being unlikely candidates for NeoPXL8.

#include <Adafruit_InternalFlash.h>
#define FLASH_FS_SIZE (64 * 1024)
#define FLASH_FS_START (0x00040000 - 256 - 0 - FLASH_FS_SIZE)
Adafruit_InternalFlash flash(FLASH_FS_START, FLASH_FS_SIZE);

#else

#include <Adafruit_SPIFlash.h>
#if defined(ARDUINO_ARCH_ESP32)
static Adafruit_FlashTransport_ESP32 flashTransport;
#elif defined(ARDUINO_ARCH_RP2040)
// Enable ONE of these lines: 1st if CIRCUITPY drive, 2nd if other partioning:
static Adafruit_FlashTransport_RP2040_CPY flashTransport;
// Adafruit_FlashTransport_RP2040 flashTransport;
#elif defined(EXTERNAL_FLASH_USE_QSPI)
static Adafruit_FlashTransport_QSPI flashTransport;
#elif defined(EXTERNAL_FLASH_USE_SPI)
static Adafruit_FlashTransport_SPI flashTransport(EXTERNAL_FLASH_USE_CS,
                                                  EXTERNAL_FLASH_USE_SPI);
#else
#error "Flash specifications are unknown for this board."
#endif
static Adafruit_SPIFlash flash(&flashTransport);

#endif // end !ADAFRUIT_FEATHER_M0, etc.

static Adafruit_USBD_MSC usb_msc;
static FatVolume fatfs;

static int32_t msc_read_cb(uint32_t lba, void *buffer, uint32_t bufsize) {
  return flash.readBlocks(lba, (uint8_t *)buffer, bufsize / 512) ? bufsize : -1;
}

static int32_t msc_write_cb(uint32_t lba, uint8_t *buffer, uint32_t bufsize) {
  return flash.writeBlocks(lba, buffer, bufsize / 512) ? bufsize : -1;
}

static void msc_flush_cb(void) {
  flash.syncBlocks();
  fatfs.cacheClear();
}

#else
#warning "TinyUSB stack not selected. While technically not an error,"
#warning "it's required if you want to read NeoPXL8 JSON configuration"
#warning "from the CIRCUITPY filesystem. Otherwise defaults are used."
#endif // end if USE_TINYUSB/ESP32

NeoPXL8status NeoPXL8readConfig(NeoPXL8config *config, FatVolume *fs,
                                const char *filename) {

  if (!config)
    return NEO_ERR_CONFIG;

  // Initialize config struct defaults
  config->message[0] = 0;

#if defined(USE_TINYUSB) || defined(ESP32)
  // If no filesystem was passed in, try accessing the CIRCUITPY drive.
  if (!fs) {
    flash.begin();
#if defined(ADAFRUIT_FEATHER_M0)
    usb_msc.setID("Adafruit", "Internal Flash", "1.0");
#else
    usb_msc.setID("Adafruit", "External Flash", "1.0");
#endif
    usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
    usb_msc.setCapacity(flash.size() / 512, 512);
    usb_msc.setUnitReady(true);
    usb_msc.begin();
    if (!fatfs.begin(&flash)) {
      strcpy(config->message, "No filesystem");
      return NEO_ERR_FILESYS;
    }
    fs = &fatfs;
    // If flash/msc is initialized here, it REMAINS ACTIVE after function
    // returns (is not stopped), to allow USB access to files (editing the
    // config, moving over files, etc.) via the msc_* callbacks above.
  }
#endif

  FatFile file;
  NeoPXL8status status = NEO_OK;

  // Open and decode JSON file...
  if ((file = fs->open(filename, FILE_READ))) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
      // Some JSON syntax error. config message holds a brief summary.
      strncpy(config->message, error.c_str(), sizeof(config->message) - 1);
      config->message[sizeof(config->message) - 1] = 0;
      status = NEO_ERR_JSON;
    } else {
      // Valid JSON, process the configuration...
      JsonVariant v;
      v = doc["pins"];
      if (v.is<JsonArray>()) {
        uint8_t n = v.size() < 8 ? v.size() : 8;
        for (uint8_t i = 0; i < n; i++)
          config->pins[i] = v[i].as<int>();
      }
      config->length = doc["length"] | config->length;
      v = doc["order"];
      if (v.is<const char *>()) {
        // Although color order *could* be done by examining each character
        // and some bit-fiddling, a string lookup table is used instead in
        // case NeoPixel library changes how the color order bits work.
        const struct {
          const char *order; // Color order identifier, e.g. "GBRW"
          uint8_t value;     // From Adafruit_NeoPixel.h, minus the KHZ bit
        } order[] = {
            "RGB",  NEO_RGB,  "RBG",  NEO_RBG,  "GRB",  NEO_GRB,
            "GBR",  NEO_GBR,  "BRG",  NEO_BRG,  "BGR",  NEO_BGR,
            "WRGB", NEO_WRGB, "WRBG", NEO_WRBG, "WGRB", NEO_WGRB,
            "WGBR", NEO_WGBR, "WBRG", NEO_WBRG, "WBGR", NEO_WBGR,
            "RWGB", NEO_RWGB, "RWBG", NEO_RWBG, "RGWB", NEO_RGWB,
            "RGBW", NEO_RGBW, "RBWG", NEO_RBWG, "RBGW", NEO_RBGW,
            "GWRB", NEO_GWRB, "GWBR", NEO_GWBR, "GRWB", NEO_GRWB,
            "GRBW", NEO_GRBW, "GBWR", NEO_GBWR, "GBRW", NEO_GBRW,
            "BWRG", NEO_BWRG, "BWGR", NEO_BWGR, "BRWG", NEO_BRWG,
            "BRGW", NEO_BRGW, "BGWR", NEO_BGWR, "BGRW", NEO_BGRW,
        };
        for (int i = 0; i < (sizeof(order) / sizeof(order[0])); i++) {
          if (!strcasecmp(v, order[i].order)) {
            config->order = order[i].value + NEO_KHZ800;
            break;
          }
        }
      }
      config->dither = doc["dither"] | config->dither;
      // Sketches can include their own configurables in the JSON file.
      // It's very rudimentary -- strings only (max length 20), sketch will
      // need to do any int/float/etc conversion on its own, and strictly
      // "flat," no hierarchy or nesting -- but does keep the sketch code
      // very simple.
      if (config->extra) {
        for (int i = 0; config->extra[i].key; i++) {
          v = doc[config->extra[i].key];
          if (v.is<const char *>()) {
            strncpy(config->extra[i].value, v,
                    sizeof(config->extra[i].value) - 1);
            config->extra[i].value[sizeof(config->extra[i].value) - 1] = 0;
          }
        }
      }
    }
    file.close();
  } else {
    strcpy(config->message, "Can't open config");
    status = NEO_ERR_FILE;
  }

  return status;
}
