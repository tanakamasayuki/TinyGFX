// TinyGFX - ST7789 panel
#pragma once
#include <stdint.h>

#include "Panel.h"

class TinyGFXPanelST7789 : public TinyGFXPanel {
 public:
  TinyGFXPanelST7789(TinyGFXBus& bus, int16_t w, int16_t h, int8_t rst = -1)
      : _bus(&bus), _natW(w), _natH(h), _rst(rst) {
    _width = w;
    _height = h;
  }

  /// パネルモジュールごとの GRAM 原点ずれ。回転 0 のときの値を渡す。
  void setOffset(int16_t x, int16_t y) { _offX0 = x; _offY0 = y; }

  bool init() override;
  void setRotation(uint8_t r) override;
  void setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) override;
  void writeColor(uint16_t color, uint32_t count) override { _bus->writeColor(color, count); }
  void writePixels(const uint16_t* data, uint32_t count) override { _bus->writePixels(data, count); }
  void beginTransaction() override { _bus->beginTransaction(); }
  void endTransaction() override { _bus->endTransaction(); }

  // 仮想にしない（全員が払うほどではない。docs/DECISIONS.ja.md Q7）
  void invertDisplay(bool invert) { cmd(invert ? 0x21 : 0x20); }
  void setSleep(bool sleep) { cmd(sleep ? 0x10 : 0x11); }
  void displayOn(bool on) { cmd(on ? 0x29 : 0x28); }

 private:
  enum : uint8_t {
    MADCTL_MY = 0x80, MADCTL_MX = 0x40, MADCTL_MV = 0x20, MADCTL_RGB = 0x00,
  };

  void cmd(uint8_t c) {
    _bus->beginTransaction();
    _bus->writeCommand(c);
    _bus->endTransaction();
  }
  void cmdData(uint8_t c, const uint8_t* d, uint8_t n) {
    _bus->writeCommand(c);
    if (n) _bus->writeData(d, n);
  }

  TinyGFXBus* _bus;
  int16_t _natW, _natH;
  int16_t _offX0 = 0, _offY0 = 0;  // 回転 0 でのオフセット
  int16_t _offX = 0, _offY = 0;    // 現在の回転でのオフセット
  int8_t _rst;
};

inline bool TinyGFXPanelST7789::init() {
  _bus->init();
  if (_rst >= 0) {
#if defined(ARDUINO)
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, HIGH);
    delay(10);
    digitalWrite(_rst, LOW);
    delay(10);
    digitalWrite(_rst, HIGH);
    delay(120);
#endif
  }
  _bus->beginTransaction();
  cmdData(0x01, nullptr, 0);  // SWRESET
  _bus->endTransaction();
#if defined(ARDUINO)
  delay(150);
#endif
  _bus->beginTransaction();
  cmdData(0x11, nullptr, 0);  // SLPOUT
  _bus->endTransaction();
#if defined(ARDUINO)
  delay(120);
#endif
  _bus->beginTransaction();
  const uint8_t colmod = 0x55;  // 16bit/pixel
  cmdData(0x3A, &colmod, 1);
  cmdData(0x21, nullptr, 0);  // INVON (ST7789 は通常反転)
  cmdData(0x13, nullptr, 0);  // NORON
  _bus->endTransaction();
  setRotation(0);
  _bus->beginTransaction();
  cmdData(0x29, nullptr, 0);  // DISPON
  _bus->endTransaction();
  return true;
}

inline void TinyGFXPanelST7789::setRotation(uint8_t r) {
  r = (uint8_t)(r & 3);
  uint8_t madctl = MADCTL_RGB;
  switch (r) {
    case 0: madctl |= 0; _width = _natW; _height = _natH; _offX = _offX0; _offY = _offY0; break;
    case 1: madctl |= (uint8_t)(MADCTL_MV | MADCTL_MX); _width = _natH; _height = _natW;
            _offX = _offY0; _offY = _offX0; break;
    case 2: madctl |= (uint8_t)(MADCTL_MX | MADCTL_MY); _width = _natW; _height = _natH;
            _offX = _offX0; _offY = _offY0; break;
    default: madctl |= (uint8_t)(MADCTL_MV | MADCTL_MY); _width = _natH; _height = _natW;
             _offX = _offY0; _offY = _offX0; break;
  }
  _bus->beginTransaction();
  cmdData(0x36, &madctl, 1);
  _bus->endTransaction();
}

inline void TinyGFXPanelST7789::setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
  xs = (uint16_t)(xs + _offX); xe = (uint16_t)(xe + _offX);
  ys = (uint16_t)(ys + _offY); ye = (uint16_t)(ye + _offY);
  uint8_t a[4];
  a[0] = (uint8_t)(xs >> 8); a[1] = (uint8_t)xs; a[2] = (uint8_t)(xe >> 8); a[3] = (uint8_t)xe;
  cmdData(0x2A, a, 4);  // CASET
  a[0] = (uint8_t)(ys >> 8); a[1] = (uint8_t)ys; a[2] = (uint8_t)(ye >> 8); a[3] = (uint8_t)ye;
  cmdData(0x2B, a, 4);  // RASET
  cmdData(0x2C, nullptr, 0);  // RAMWR
}
