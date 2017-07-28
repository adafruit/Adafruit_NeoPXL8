#ifndef _ADAFRUIT_NEOPXL8_H_
#define _ADAFRUIT_NEOPXL8_H_

#include <Adafruit_NeoPixel.h>
#include <Adafruit_ZeroDMA.h>

class Adafruit_NeoPXL8 : public Adafruit_NeoPixel {

 public:

  Adafruit_NeoPXL8(uint16_t n, uint8_t *p=NULL, neoPixelType t=NEO_GRB);
  ~Adafruit_NeoPXL8();

  boolean     begin(void),
              canShow(void),
              canStage(void);
  void        show(),
              stage(),
              setBrightness(uint8_t);
  uint8_t     getBrightness() const;

 protected:

  Adafruit_ZeroDMA dma;
  uint8_t          mux[5],
                   pins[8],
                   bitmask[8],
                  *dmaBuf,
                   xferComplete;
  uint16_t         brightness;
  boolean          staged;
};

#endif // _ADAFRUIT_NEOPXL8_H_
