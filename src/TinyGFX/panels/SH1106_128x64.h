// SH1106 128x64 - the 1.3 inch module
//
// Sold as an "SSD1306" more often than not. 132 columns of RAM behind
// 128 of glass, so the picture sits two columns in - which the driver
// derives, since the glass is centred.
//
// Verified: host tests (tests/sh1106/). Not confirmed on real glass (M6).
#pragma once
#ifdef TINYGFX_DRIVER_SH1106_INCLUDED
#error "TinyGFX: one SH1106 panel per sketch. Include this header rather than <TinyGFX/DriverSH1106.h>, and do not include a second panels/SH1106_*.h - the second one would be silently ignored and its panel driven with the first one's values."
#endif
#include "../DriverSH1106.h"

class TinyGFXPanelSH1106_128x64 : public TinyGFXDriverSH1106 {
 public:
  static const int16_t kWidth = 128;
  static const int16_t kHeight = 64;
  /// Size the framebuffer with this, so it cannot be got wrong:
  ///   static uint8_t fb[TinyGFXPanelSH1106_128x64::kBufferBytes];
  static const uint16_t kBufferBytes = (uint16_t)(kWidth * kHeight / 8);

  /// `bufferPages` splits the buffer into a band; 0 means the whole screen.
  TinyGFXPanelSH1106_128x64(TinyGFXBus& bus, uint8_t* buffer, int16_t bufferPages = 0)
      : TinyGFXDriverSH1106(bus, buffer, kWidth, kHeight, bufferPages) {}
};
