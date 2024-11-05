#ifndef __OBP60canvas__
#define __OBP60canvas__
#include <Adafruit_GFX.h>

class OBP60canvas : public GFXcanvas1 {
public:
  OBP60canvas(uint16_t w, uint16_t h);
  void print(bool rotated);
};
#endif
