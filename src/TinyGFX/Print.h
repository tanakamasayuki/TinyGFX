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

  /// Print hands over one byte at a time, so with UTF-8 on, a multi-byte
  /// character arrives in pieces and has to be held until it is whole. That
  /// state is the only difference from drawString, which sees the whole string
  /// and can just call nextCode.
  ///
  /// One consequence: a string that ends part way through a character leaves
  /// that character pending, and it resolves to a notdef on the next write -
  /// which, if setCursor was called in between, lands at the new position
  /// rather than where the broken character began. Flushing it in setCursor
  /// instead was measured at **+36 bytes on a CH32V003**, paid by every sketch
  /// that prints, to move one notdef in a case that only arises from malformed
  /// input. Not taken.
  size_t write(uint8_t c) override {
#if TINYGFX_FONT_UTF8
    if (_need != 0) {
      if ((c & 0xC0u) == 0x80u) {
        _acc = (uint16_t)((_acc << 6) | (uint16_t)(c & 0x3Fu));
        if (--_need == 0) put(_astral ? (uint16_t)TINYGFX_NOTDEF : _acc);
        return 1;
      }
      // The sequence was cut short. One notdef for it - and then this byte is
      // reconsidered from the top rather than dropped, because a '\n' or a
      // fresh lead byte here is a character in its own right.
      _need = 0;
      put(TINYGFX_NOTDEF);
    }
#endif
    if (c == '\r') return 1;
    if (c == '\n') {
      _cursorX = _lineStartX;
      _cursorY = (int16_t)(_cursorY + fontHeight());
      return 1;
    }
#if TINYGFX_FONT_UTF8
    if (c >= 0x80u) {
      const uint8_t len = utf8Len(c);
      if (len == 0) {  // a continuation byte with no lead, or 0xFE / 0xFF
        put(TINYGFX_NOTDEF);
      } else {
        _acc = (uint16_t)(c & (uint8_t)(0x7Fu >> len));
        _need = (uint8_t)(len - 1);
        _astral = (len == 4);
      }
      return 1;
    }
#endif
    put(c);
    return 1;
  }

  void setCursor(int16_t x, int16_t y) {
    TinyGFX::setCursor(x, y);
    _lineStartX = x;
  }

 private:
  void put(uint16_t ch) {
    _cursorX = (int16_t)(_cursorX + drawChar(ch, _cursorX, _cursorY));
  }

  int16_t _lineStartX = 0;
#if TINYGFX_FONT_UTF8
  uint16_t _acc = 0;    // the code point so far
  uint8_t _need = 0;    // continuation bytes still owed
  bool _astral = false; // above U+FFFF: consume it, then draw one notdef
#endif
};
