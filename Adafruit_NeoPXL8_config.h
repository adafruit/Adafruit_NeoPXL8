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

// Status codes returned by the config-reading function
typedef enum {
  NEO_OK = 0,      // Config file successfully read
  NEO_ERR_CONFIG,  // Missing config structure
  NEO_ERR_FILESYS, // Can't initialize flash filesystem, will use defaults
  NEO_ERR_FILE,    // Can't open config file, will use defaults
  NEO_ERR_JSON,    // Config file isn't valid JSON, will use defaults
} NeoPXL8status;

// This structure provides a rudimentary way for sketches to read additional
// config data from the file, without having to extend the library in very
// specific ways.
typedef struct {
  const char *key; // JSON key
  char value[21];  // Paired value (always a string, may require conversion)
} NeoPXL8configExtra;

typedef struct {
  int8_t pins[8];            // List of pins for NeoPXL8(HDR) constructor
  uint16_t length;           // Length of NeoPixel strand PER PIN
  uint16_t order;            // Color order, e.g. NEO_GRB
  uint8_t dither;            // Extra bits for HDR dithering (if NeoPXL8HDR)
  char message[21];          // Status message, may help w/JSON debugging
  NeoPXL8configExtra *extra; // Sketch can attach config array if needed
} NeoPXL8config;

/*!
  @brief   NeoPXL8 configuration file reader. Reads JSON file from CIRCUITPY
           or other filesystem, allows some projects to be run-time
           configurable rather than needing edit/compile for every change.
  @param   config    REQUIRED: Pointer to NeoPXL8config struct. This is used
                               both for inputs (to establish default values
                               for program) and outputs (to return values
                               read from file).
  @param   fs        OPTIONAL: Pointer to FAT filesystem (can omit if using
                               the default CIRCUITPY drive).
  @param   filename  OPTIONAL: Configuration file name (can omit to use the
                               default "neopxl8.cfg" in root directory)
  @return  NeoPXL8status code:
           NEO_OK              Config file successfully read.
           NEO_ERR_CONFIG      Missing config structure (must be non-NULL)..
           NEO_ERR_FILESYS     Can't initialize flash filesystem, will use
                               config defaults. This status code may appear
                               only when using the CIRCUITPY drive.
           NEO_ERR_FILE        Can't open config file, will use defaults.
           NEO_ERR_JSON        Config file isn't valid JSON, will use defaults.
           For most non-OK return values (especially NEO_ERR_JSON), the
           config struct 'message' element might contain useful
           troubleshooting information.
*/
NeoPXL8status NeoPXL8readConfig(NeoPXL8config *config, FatVolume *fs = NULL,
                                const char *filename = "neopxl8.cfg");
