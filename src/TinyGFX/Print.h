// TinyGFX - Print / printf / float support (opt-in)
//
// このヘッダを include したときだけ Print がリンクされる。
// float を渡すと浮動小数点の書式化まで載る。CH32V003 では致命的に大きい。
// コストは docs/FOOTPRINT.ja.md を見ること。
#pragma once
#include <Arduino.h>  // Print はコアの Arduino.h が出す（api/Print.h 型のコアにも対応）

#include "Gfx.h"

class TinyGFXPrint : public TinyGFX, public Print {
 public:
  explicit TinyGFXPrint(TinyGFXPanel& panel) : TinyGFX(panel) {}

  using TinyGFX::setTextSize;
  /// 小数倍の文字サイズ。float を持ち込むのでコアには置かない。
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
