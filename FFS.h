// SPDX-FileCopyrightText: 2023 P Burgess for Adafruit Industries
//
// SPDX-License-Identifier: MIT

/*!
 * @file FFS.h
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

#pragma once

#if defined(USE_TINYUSB) || defined(ESP32)

#include <SdFat.h>

/*!
  @brief  FFS is a minimal class to assist in using a board's CIRCUITPY
          flash filesystem with Arduino code, and making it available to
          a host computer over USB.
          All functions here are currently static -- you do not need to
          declare an object (unless you want to). Since there's only one
          CIRCUITPY filesystem, library state is maintained internally and
          any of these functions can be called directly, e.g. FFS::begin().
          Putting the functions inside a class simply avoids namespace
          issues.
*/
class FFS {
public:
  /*!
    @brief  FFS constructor. No arguments. User code is not required to
            declare an object (all functions are static and can be invoked
            directly without an object), but it's still an option if the
            resulting code reads easier for you (e.g. using object.func()
            rather than class::func() syntax), all good.
  */
  FFS(void){};

  /*!
    @brief  FFS destructor.
  */
  ~FFS(void){};

  /*!
    @brief   Access a board's CIRCUITPY flash filesystem, making it
             available to code and to a host computer over USB.
             IMPORTANT: this function should always be called BEFORE
             Serial.begin().
    @param   cs   OPTIONAL  SPI flash chip-select pin. This should ONLY be
                            used on "Haxpress" boards (QT Py or Trinket M0
                            with flash chip retrofitted). For most boards,
                            including unmodified QT Py or Trinket M0, do not
                            pass any arguments.
    @param   spi  OPTIONAL  Pointer to SPI peripheral interfaced with flash
                            chip. Again, only for a couple of Haxpress M0
                            boards.
    @return  FatVolume*  On success, a non-NULL pointer to a FatVolume
                         object, where files can then be opened and accessed.
                         NULL on error (uninitialized CIRCUITPY drive, or
                         invalid cs/spi combo)..
  */
  static FatVolume *begin(int cs = -1, void *spi = NULL);

  /*!
    @brief   Checks if USB-connected host computer has made any changes
             (new or altered files) to the drive. Code can use this if it
             needs to auto-restart on change.
    @return  1/true if host computer has written to drive, 0/false otherwise.
  */
  static bool changed(void);

  /*!
    @brief   Acknowledge and reset status of changed() polling. Change-
             sensitive code can call this to distinguish subsequent
             changed() calls.
  */
  static void change_ack(void);
};

#else

#error "Requires TinyUSB stack. From the Arduino IDE 'Tools' menu,"
#error "select 'USB Stack -> Adafruit TinyUSB' and recompile."

#endif // end USE_TINYUSB || ESP32
