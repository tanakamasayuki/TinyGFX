// ILI9341 240x320 - the 2.4 / 2.8 inch SPI breakouts
//
// Portrait memory, inversion off, and mounted mirrored on X - which is
// what makes rotation 0 want MADCTL 0x48, the value every other library
// sends to this part.
//
// Verified: Not confirmed on real glass (M5).
#pragma once
#ifdef TINYGFX_DRIVER_ILI9341_INCLUDED
#error "TinyGFX: one ILI9341 panel per sketch. Include this header rather than <TinyGFX/DriverILI9341.h>, and do not include a second panels/ILI9341_*.h - the second one would be silently ignored and its panel driven with the first one's values."
#endif
#include "../DriverILI9341.h"

class TinyGFXPanelILI9341_240x320 : public TinyGFXDriverILI9341 {
 public:
  static const int16_t kWidth = 240;
  static const int16_t kHeight = 320;

  explicit TinyGFXPanelILI9341_240x320(TinyGFXBus& bus, int8_t rst = -1)
      : TinyGFXDriverILI9341(bus, kWidth, kHeight, rst) {}
};
