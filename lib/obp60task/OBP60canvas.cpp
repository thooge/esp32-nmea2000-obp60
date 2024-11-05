#include "OBP60canvas.h"
#include <Arduino.h>

OBP60canvas::OBP60canvas(uint16_t w, uint16_t h)
    : GFXcanvas1(w, h) {}

void OBP60canvas::print(bool rotated) {
  char pixel_buffer[8];
  uint16_t width, height;

  if (rotated) {
    width = this->width();
    height = this->height();
  } else {
    width = this->WIDTH;
    height = this->HEIGHT;
  }

  for (uint16_t y = 0; y < height; y++) {
    for (uint16_t x = 0; x < width; x++) {
      bool pixel;
      if (rotated) {
        pixel = this->getPixel(x, y);
      } else {
        pixel = this->getRawPixel(x, y);
      }
      sprintf(pixel_buffer, " %d", pixel);
      Serial.print(pixel_buffer);
    }
    Serial.print("\n");
  }
}
