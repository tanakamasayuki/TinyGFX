// TinyGFX - SSD1306 (monochrome OLED, I2C or SPI)
//
// Two things differ from a colour SPI panel.
//
//   1. It needs a framebuffer. The GRAM cannot be read back and only writes a
//      page (8 rows) at a time, so arbitrary drawing means keeping the bits
//      locally. That is 1,024 bytes for 128x64, and the buffer is supplied by
//      the caller - nothing is allocated here.
//   2. Transfers are deferred. Pushing 1 KB after every draw is out of the
//      question, so display() sends only the pages that changed.
//
// The drawing API stays RGB565; this panel is what collapses it to 1bpp, with
// "non-zero lights up". Colour depth is deliberately not abstracted in the
// core (docs/DECISIONS.ja.md D4).
//
// Rotation is done by indexing the buffer rather than in the controller: the
// SSD1306 has no 90-degree rotation, so there is no MADCTL equivalent to lean
// on.
#pragma once
#include <stdint.h>

#include "Panel.h"

class TinyGFXPanelSSD1306 : public TinyGFXPanel {
 public:
  /// `buffer` is w * h / 8 bytes - 1,024 for 128x64.
  TinyGFXPanelSSD1306(TinyGFXBus& bus, uint8_t* buffer, int16_t w = 128, int16_t h = 64)
      : _bus(&bus), _buf(buffer), _natW(w), _natH(h) {
    _width = w;
    _height = h;
    _pages = (int16_t)(h >> 3);
  }

  bool init() override;
  void setRotation(uint8_t r) override {
    _rotation = (uint8_t)(r & 3);
    if (_rotation & 1) { _width = _natH; _height = _natW; }
    else               { _width = _natW; _height = _natH; }
  }

  void setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) override {
    _xs = xs; _ys = ys; _xe = xe; _ye = ye;
    _cx = xs; _cy = ys;
  }
  void writeColor(uint16_t color, uint32_t count) override {
    const bool on = (color != 0);
    while (count--) put(on);
  }
  void writePixels(const uint16_t* data, uint32_t count) override {
    while (count--) put(*data++ != 0);
  }
  void beginTransaction() override {}
  void endTransaction() override {}

  /// Push only the pages that changed. Nothing reaches the screen until this
  /// is called.
  void display();
  /// Clear the local buffer. The screen is untouched until display().
  void clearBuffer(bool on = false);

  // Deliberately not virtual - not worth charging everyone for
  void invertDisplay(bool invert) { cmd(invert ? 0xA7 : 0xA6); }
  void setSleep(bool sleep) { cmd(sleep ? 0xAE : 0xAF); }
  void setContrast(uint8_t value) { cmd(0x81); cmd(value); }

 private:
  void cmd(uint8_t c) { _bus->writeCommand(c); }

  /// Logical coordinates to a bit in the buffer. Out-of-range writes are dropped.
  void put(bool on) {
    int16_t x = (int16_t)_cx, y = (int16_t)_cy;
    // Advance to the next pixel up front, so an early return cannot desync it
    if (_cx >= _xe) { _cx = _xs; ++_cy; } else { ++_cx; }
    if (x < 0 || y < 0 || x >= _width || y >= _height) return;

    int16_t fx, fy;
    switch (_rotation) {
      case 1:  fx = y;                       fy = (int16_t)(_natH - 1 - x); break;
      case 2:  fx = (int16_t)(_natW - 1 - x); fy = (int16_t)(_natH - 1 - y); break;
      case 3:  fx = (int16_t)(_natW - 1 - y); fy = x;                       break;
      default: fx = x;                       fy = y;                        break;
    }
    const int16_t page = (int16_t)(fy >> 3);
    uint8_t* slot = &_buf[(int32_t)page * _natW + fx];
    const uint8_t mask = (uint8_t)(1u << (fy & 7));
    if (on) *slot = (uint8_t)(*slot | mask);
    else    *slot = (uint8_t)(*slot & (uint8_t)~mask);
    if (page < _dirtyLo) _dirtyLo = page;
    if (page > _dirtyHi) _dirtyHi = page;
  }

  TinyGFXBus* _bus;
  uint8_t* _buf;
  int16_t _natW, _natH, _pages;
  uint16_t _xs = 0, _ys = 0, _xe = 0, _ye = 0;
  uint16_t _cx = 0, _cy = 0;
  int16_t _dirtyLo = 32767, _dirtyHi = -1;
  uint8_t _rotation = 0;
};

inline bool TinyGFXPanelSSD1306::init() {
  static const uint8_t kInit[] = {
      0xAE,        // display off
      0xD5, 0x80,  // clock
      0xA8, 0x3F,  // multiplex（128x64）
      0xD3, 0x00,  // display offset
      0x40,        // start line 0
      0x8D, 0x14,  // charge pump on
      0x20, 0x00,  // horizontal addressing
      0xA1,        // segment remap
      0xC8,        // com scan dec
      0xDA, 0x12,  // com pins
      0x81, 0xCF,  // contrast
      0xD9, 0xF1,  // precharge
      0xDB, 0x40,  // vcom detect
      0xA4,        // resume from RAM
      0xA6,        // normal (not inverted)
      0xAF,        // display on
  };
  _bus->init();
  for (uint8_t i = 0; i < sizeof(kInit); ++i) cmd(kInit[i]);
  clearBuffer(false);
  return true;
}

inline void TinyGFXPanelSSD1306::clearBuffer(bool on) {
  const int32_t n = (int32_t)_natW * _pages;
  const uint8_t v = on ? 0xFF : 0x00;
  for (int32_t i = 0; i < n; ++i) _buf[i] = v;
  _dirtyLo = 0;
  _dirtyHi = (int16_t)(_pages - 1);
}

inline void TinyGFXPanelSSD1306::display() {
  if (_dirtyHi < _dirtyLo) return;  // nothing changed
  cmd(0x21); cmd(0); cmd((uint8_t)(_natW - 1));                    // column range
  cmd(0x22); cmd((uint8_t)_dirtyLo); cmd((uint8_t)_dirtyHi);       // page range
  _bus->writeData(&_buf[(int32_t)_dirtyLo * _natW],
                  (size_t)((int32_t)(_dirtyHi - _dirtyLo + 1) * _natW));
  _dirtyLo = 32767;
  _dirtyHi = -1;
}
