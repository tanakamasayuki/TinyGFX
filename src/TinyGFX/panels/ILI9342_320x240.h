// ILI9342C 320x240 - M5Stack Core / BASIC
//
// Landscape memory from the start, so nothing has to be offset. Older
// BASIC units come up with the colours inverted; invertDisplay(false)
// after begin() is the fix.
//
// Verified: **Real hardware, 2026-08-28 (M0).** The picture matched the host golden exactly.
#pragma once
#ifdef TINYGFX_DRIVER_ILI9342_INCLUDED
#error "TinyGFX: one ILI9342 panel per sketch. Include this header rather than <TinyGFX/DriverILI9342.h>, and do not include a second panels/ILI9342_*.h - the second one would be silently ignored and its panel driven with the first one's values."
#endif
#include "../DriverILI9342.h"

class TinyGFXPanelILI9342_320x240 : public TinyGFXDriverILI9342 {
 public:
  static const int16_t kWidth = 320;
  static const int16_t kHeight = 240;

  explicit TinyGFXPanelILI9342_320x240(TinyGFXBus& bus, int8_t rst = -1)
      : TinyGFXDriverILI9342(bus, kWidth, kHeight, rst) {}
};
