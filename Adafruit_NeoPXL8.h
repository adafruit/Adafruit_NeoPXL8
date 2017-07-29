#ifndef _ADAFRUIT_NEOPXL8_H_
#define _ADAFRUIT_NEOPXL8_H_

#include <Adafruit_NeoPixel.h>
#include <Adafruit_ZeroDMA.h>

class Adafruit_NeoPXL8 : public Adafruit_NeoPixel {

 public:

  Adafruit_NeoPXL8(uint16_t n, int8_t *p=NULL, neoPixelType t=NEO_GRB);
  ~Adafruit_NeoPXL8();

  boolean          begin(void),
                   canStage(void),
                   canShow(void);
  void             show(),
                   stage(),
                   setBrightness(uint8_t);
  uint8_t          getBrightness() const;

 protected:

  Adafruit_ZeroDMA dma;
  int8_t           pins[8];     // Pin list for 8 NeoPixel strips
  uint8_t          bitmask[8],  // Pattern generator bitmask for each pin
                  *dmaBuf;      // Allocated buffer for pixel data + extra
  uint32_t        *alignedAddr; // long-aligned ptr into dmaBuf (see code)
  uint16_t         brightness;
  boolean          staged;      // If set, data is ready for DMA trigger
};

#endif // _ADAFRUIT_NEOPXL8_H_
