// TinyGFX - Print / printf / float support (opt-in)
//
// Print is linked in only when this header is included. Hand it a float and
// floating-point formatting comes along too, which is fatally large on a
// CH32V003. See docs/FOOTPRINT.ja.md for what it costs.
#pragma once
#include <Arduino.h>  // Print comes from the core's Arduino.h (also covers api/Print.h cores)

#include "Gfx.h"

class TinyGFXPrint : public TinyGFX, public Print {
 public:
  explicit TinyGFXPrint(TinyGFXPanel& panel) : TinyGFX(panel) {}

  using TinyGFX::setTextSize;
  /// Fractional text size. Kept out of the core because it drags in float.
  void setTextSize(float size) {
    int16_t s = (int16_t)(size + 0.5f);
    if (s < 1) s = 1;
    TinyGFX::setTextSize((uint8_t)s);
  }

  size_t write(uint8_t c) override {
    if (c == '\r') return 1;
    if (c == '\n') {
      _cursorX = _lineStartX;
      _cursorY = (int16_t)(_cursorY + fontHeight());
      return 1;
    }
    const int16_t adv = drawChar(c, _cursorX, _cursorY);
    _cursorX = (int16_t)(_cursorX + adv);
    return 1;
  }

  void setCursor(int16_t x, int16_t y) {
    TinyGFX::setCursor(x, y);
    _lineStartX = x;
  }

 private:
  int16_t _lineStartX = 0;
};
