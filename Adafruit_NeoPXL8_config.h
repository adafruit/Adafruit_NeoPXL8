// SPDX-FileCopyrightText: 2017 P Burgess for Adafruit Industries
//
// SPDX-License-Identifier: MIT

/*!
 * @file Adafruit_NeoPXL8_config.h
 *
 * Helper code to assist in reading NeoPXL8/NeoPXL8HDR setup from JSON
 * configuration file (rather than hardcoded in sketch).
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

#pragma once
#include "Adafruit_NeoPXL8.h"

typedef enum {
  NEO_OK = 0,      // Config file successfully read
  NEO_ERR_CONFIG,  // Missing config structure
  NEO_ERR_FILESYS, // Can't initialize flash filesystem, will use defaults
  NEO_ERR_FILE,    // Can't open config file, will use defaults
  NEO_ERR_JSON,    // Config file isn't valid JSON, will use defaults
} NEOPXL8status;

typedef struct {
  int8_t pins[8];
  uint16_t order;
  uint8_t ditherBits;
  uint16_t cols;
  uint16_t rows; // Implied from pins and rowsPer
  uint8_t rowsPer;
  uint8_t layout; // Progressive (0) vs zigzag (1)
  // Length is assembled from cols and rowsper and rows or something
  char json_str[20];
} NEOPXL8config;

#if defined(USE_TINYUSB)

#include <Adafruit_SPIFlash.h>
#include <Adafruit_TinyUSB.h>
#include <SdFat.h>
#define ARDUINOJSON_ENABLE_COMMENTS 1
#include <ArduinoJson.h>

#if defined(ARDUINO_ARCH_ESP32)
Adafruit_FlashTransport_ESP32 flashTransport;
#elif defined(ARDUINO_ARCH_RP2040)
// Enable ONE of these lines: 1st if CIRCUITPY drive, 2nd if other partioning:
Adafruit_FlashTransport_RP2040_CPY flashTransport;
// Adafruit_FlashTransport_RP2040 flashTransport;
#elif defined(EXTERNAL_FLASH_USE_QSPI)
Adafruit_FlashTransport_QSPI flashTransport;
#elif defined(EXTERNAL_FLASH_USE_SPI)
Adafruit_FlashTransport_SPI flashTransport(EXTERNAL_FLASH_USE_CS,
                                           EXTERNAL_FLASH_USE_SPI);
#else
#error "Flash specifications are unknown for this board."
#endif
static Adafruit_SPIFlash flash(&flashTransport);
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
#endif // end if USE_TINYUSB

NEOPXL8status func(NEOPXL8config *config, FatVolume *fs = NULL,
                   const char *filename = "neopxl8.cfg") {

  if (!config)
    return NEO_ERR_CONFIG;

  // Initialize config struct defaults
  config->json_str[0] = 0;

#if defined(USE_TINYUSB)
  if (!fs) {
    flash.begin();
    usb_msc.setID("Adafruit", "External Flash", "1.0");
    usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
    usb_msc.setCapacity(flash.size() / 512, 512);
    usb_msc.setUnitReady(true);
    usb_msc.begin();
    if (!fatfs.begin(&flash))
      return NEO_ERR_FILESYS;
    fs = &fatfs;
    // If flash/msc is initialized here, it REMAINS ACTIVE after function
    // returns (is not stopped), to allow USB access to files (editing the
    // config, moving over files, etc.).
  }
#endif

  FatFile file;

  if (file = fs->open(filename, FILE_READ)) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
      strncpy(config->json_str, error.c_str(), sizeof(config->json_str));
      return NEO_ERR_JSON;
    } else {
      JsonVariant v;
      v = doc["order"];
      if (v.is<const char *>()) {
        // Although this could be done with examining each char and some bit
        // fiddling, a lookup table is used in case NeoPixel lib changes bit
        // stuff.
        const struct {
          const char *order;
          uint8_t value; // From Adafruit_NeoPixel.h, minus the KHZ400 bit
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
      v = doc["pins"];
      if (v.is<JsonArray>()) {
        int n = min(v.size(), 8);
        for (uint8_t i = 0; i < n; i++)
          config->pins[i] = v[i].as<int>();
      }
      config->ditherBits = doc["dither"] | config->ditherBits;
      config->cols = doc["cols"] | config->cols;
      config->rows = doc["rows"] | config->rows;
      config->rowsPer = doc["rowsPer"] | config->rowsPer;
      config->layout = doc["layout"] | config->layout;
    }

    file.close();
  }

  if (fs == &fatfs)
    fatfs.end(); // No longer accessed in this code

  return NEO_OK;
}
