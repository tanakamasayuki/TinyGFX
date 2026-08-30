// ST7789 240x240 - the 1.3 inch square module
//
// The controller's memory is 240x320, so the GRAM size has to be told
// apart from the panel size or rotations 2 and 3 land in the wrong place.
//
// Verified: Not confirmed on real glass (M1).
#pragma once
#ifdef TINYGFX_DRIVER_ST7789_INCLUDED
#error "TinyGFX: one ST7789 panel per sketch. Include this header rather than <TinyGFX/DriverST7789.h>, and do not include a second panels/ST7789_*.h - the second one would be silently ignored and its panel driven with the first one's values."
#endif
#include "../DriverST7789.h"

class TinyGFXPanelST7789_240x240 : public TinyGFXDriverST7789 {
 public:
  static const int16_t kWidth = 240;
  static const int16_t kHeight = 240;

  explicit TinyGFXPanelST7789_240x240(TinyGFXBus& bus, int8_t rst = -1)
      : TinyGFXDriverST7789(bus, kWidth, kHeight, rst) {
    setGramSize(240, 320);
  }
};
