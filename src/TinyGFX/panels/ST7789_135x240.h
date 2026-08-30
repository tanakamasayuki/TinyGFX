// ST7789 135x240 - the TTGO T-Display size
//
// Small glass on a large memory, offset into the middle of it. Both the
// GRAM size and the offset are needed; either alone gets rotations wrong.
//
// Verified: Not confirmed on real glass (M1).
#pragma once
#ifdef TINYGFX_DRIVER_ST7789_INCLUDED
#error "TinyGFX: one ST7789 panel per sketch. Include this header rather than <TinyGFX/DriverST7789.h>, and do not include a second panels/ST7789_*.h - the second one would be silently ignored and its panel driven with the first one's values."
#endif
#include "../DriverST7789.h"

class TinyGFXPanelST7789_135x240 : public TinyGFXDriverST7789 {
 public:
  static const int16_t kWidth = 135;
  static const int16_t kHeight = 240;

  explicit TinyGFXPanelST7789_135x240(TinyGFXBus& bus, int8_t rst = -1)
      : TinyGFXDriverST7789(bus, kWidth, kHeight, rst) {
    setGramSize(240, 320);
    setOffset(52, 40);
  }
};
