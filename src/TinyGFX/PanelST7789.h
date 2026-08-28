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

  /// The module's GRAM origin offset. Give the value for rotation 0; the
  /// offsets for rotations 1-3 are derived from this and setGramSize().
  void setOffset(int16_t x, int16_t y) { _offX0 = x; _offY0 = y; }

  /// The controller's GRAM size; defaults to the panel size.
  /// On a module with an offset, rotations 2 and 3 land in the wrong place
  /// unless this is set.
  /// For example: a 240x240 ST7789 wants setGramSize(240, 320), and a
  /// 135x240 one wants setGramSize(240, 320) plus setOffset(52, 40).
  void setGramSize(int16_t w, int16_t h) { _gramW = w; _gramH = h; }

  bool init() override;
  void setRotation(uint8_t r) override;
  void setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) override;
  void writeColor(uint16_t color, uint32_t count) override { _bus->writeColor(color, count); }
  void writePixels(const uint16_t* data, uint32_t count) override { _bus->writePixels(data, count); }
  void beginTransaction() override { _bus->beginTransaction(); }
  void endTransaction() override { _bus->endTransaction(); }

  // Deliberately not virtual - not worth charging everyone for
  // (docs/DECISIONS.ja.md Q7).
  //
  // Call all of these after init() (that is, after lcd.begin()). Earlier and
  // the bus is not up yet, and even if the bytes got out, init()'s own
  // sequence would overwrite them.
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
  int16_t _offX0 = 0, _offY0 = 0;  // offset at rotation 0
  int16_t _gramW = 0, _gramH = 0;  // 0 means "same as the panel"
  int16_t _offX = 0, _offY = 0;    // offset at the current rotation
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
  cmdData(0x21, nullptr, 0);  // INVON (ST7789 panels are normally inverted)
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
  // The offset measured from the far side. This matters on modules whose
  // GRAM is larger than the panel.
  const int16_t gw = (_gramW > 0) ? _gramW : _natW;
  const int16_t gh = (_gramH > 0) ? _gramH : _natH;
  int16_t cs2 = (int16_t)(gw - _natW - _offX0);
  int16_t rs2 = (int16_t)(gh - _natH - _offY0);
  if (cs2 < 0) cs2 = 0;
  if (rs2 < 0) rs2 = 0;
  switch (r) {
    case 0: madctl |= 0; _width = _natW; _height = _natH;
            _offX = _offX0; _offY = _offY0; break;
    case 1: madctl |= (uint8_t)(MADCTL_MV | MADCTL_MX); _width = _natH; _height = _natW;
            _offX = _offY0; _offY = cs2; break;
    case 2: madctl |= (uint8_t)(MADCTL_MX | MADCTL_MY); _width = _natW; _height = _natH;
            _offX = cs2; _offY = rs2; break;
    default: madctl |= (uint8_t)(MADCTL_MV | MADCTL_MY); _width = _natH; _height = _natW;
             _offX = rs2; _offY = _offX0; break;
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
