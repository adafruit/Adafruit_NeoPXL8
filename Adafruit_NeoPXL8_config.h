// SPDX-FileCopyrightText: 2017 P Burgess for Adafruit Industries
//
// SPDX-License-Identifier: MIT

/*!
 * @file Adafruit_NeoPXL8_config.h
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

#pragma once

#include "Adafruit_NeoPXL8.h"
#include <SdFat.h>
/** Allow comments in JSON file (not canon, but really helpful) */
#define ARDUINOJSON_ENABLE_COMMENTS 1
#include <ArduinoJson.h>

/*!
  @brief   NeoPXL8 configuration file reader. Reads JSON file from CIRCUITPY
           or other filesystem, allows some projects to be run-time
           configurable rather than needing edit/compile for every change.
  @param   size      OPTIONAL: Maximum size for config file; 1024 default.
  @param   fs        OPTIONAL: Pointer to FAT filesystem (can omit or pass
                               NULL if using the default CIRCUITPY drive).
  @param   filename  OPTIONAL: Configuration file name, non-NULL (can omit
                               to use the default "neopxl8.cfg" in root
                               directory).
  @return  DynamicJsonDocument
           Document should be checked for "ERROR" key. If not present,
           document was successfully read. If present, the corresponding
           value gives some indication of what went wrong: typically a
           missing or invalid filesystem, missing JSON file, or a JSON
           parsing error (value may be helpful for troubleshooting).
*/
DynamicJsonDocument NeoPXL8configRead(int size = 1024, FatVolume *fs = NULL,
                                      const char *filename = "neopxl8.cfg");

/*!
  @brief  Given an integer list from the JSON config, copy values to an
          int8_t array to be passed to NeoPXL8(HDR) constructor.
  @param  v     JsonVariant element containing pin list.
  @param  pins  int8_t[8] array, used both for passing defaults IN and
                configuration values OUT. Defaults are kept if config value
                is not a JSON array.
*/
void NeoPXL8configPins(JsonVariant v, int8_t pins[8]);

/*!
  @brief   Given a short string such as "BGR" or "WRGB" from the JSON
           config, find and return the matching NEO_* color order from
           Adafruit_Neopixel.h (string to bit-fields conversion).
  @param   v      JsonVariant element containing color string.
  @param   value  Default to assign if not found in config file or if
                  config value is not a string..
  @return  One of the NeoPixel library color orders, e.g. NEO_BGR.
*/
uint16_t NeoPXL8configColorOrder(JsonVariant v, uint16_t value);
