// SPDX-FileCopyrightText: 2017 P Burgess for Adafruit Industries
//
// SPDX-License-Identifier: MIT

/*!
 * @file config.h
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
#include <SdFat.h>

typedef enum {
  NEO_OK = 0,      // Config file successfully read
  NEO_ERR_CONFIG,  // Missing config structure
  NEO_ERR_FILESYS, // Can't initialize flash filesystem, will use defaults
  NEO_ERR_FILE,    // Can't open config file, will use defaults
  NEO_ERR_JSON,    // Config file isn't valid JSON, will use defaults
} NeoPXL8status;

typedef struct {
  const char *key;
  char value[21];
} NeoPXL8configExtras;

typedef struct {
  int8_t pins[8];
  uint8_t rowsPerPin;
  uint16_t cols;
  uint16_t order;
  uint8_t dither;
  char json_str[21];
  NeoPXL8configExtras *extras; // KEEP THIS AT END (inits to 0 if unspecified)
} NeoPXL8config;

NeoPXL8status func(NeoPXL8config *config, FatVolume *fs = NULL,
                   const char *filename = "neopxl8.cfg");
