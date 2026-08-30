// SSD1306 128x32 - the 0.91 inch module
//
// Half the height of the 128x64, and it needs a different COM pin
// layout: sequential rather than alternative.
//
// Verified: host tests. Not confirmed on real glass.
#pragma once
#ifdef TINYGFX_DRIVER_SSD1306_INCLUDED
#error "TinyGFX: one SSD1306 panel per sketch. Include this header rather than <TinyGFX/DriverSSD1306.h>, and do not include a second panels/SSD1306_*.h - the second one would be silently ignored and its panel driven with the first one's values."
#endif

// This glass wires its COM lines sequentially, not alternately.
#define TINYGFX_SSD1306_COM_PINS 0x02
#include "../DriverSSD1306.h"

class TinyGFXPanelSSD1306_128x32 : public TinyGFXDriverSSD1306 {
 public:
  static const int16_t kWidth = 128;
  static const int16_t kHeight = 32;
  /// Size the framebuffer with this, so it cannot be got wrong:
  ///   static uint8_t fb[TinyGFXPanelSSD1306_128x32::kBufferBytes];
  static const uint16_t kBufferBytes = (uint16_t)(kWidth * kHeight / 8);

  /// `bufferPages` splits the buffer into a band; 0 means the whole screen.
  TinyGFXPanelSSD1306_128x32(TinyGFXBus& bus, uint8_t* buffer, int16_t bufferPages = 0)
      : TinyGFXDriverSSD1306(bus, buffer, kWidth, kHeight, bufferPages) {}
};
