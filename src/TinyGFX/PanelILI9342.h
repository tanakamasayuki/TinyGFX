// TinyGFX - ILI9342C panel (M5Stack Core / BASIC and friends)
//
// A sibling of the ILI9341, except its GRAM is landscape from the start
// (320x240). There are no modules with an origin offset, so there is no
// setOffset or setGramSize here.
//
// In practice only three things differ from the ST7789, and none of them can
// be confirmed anywhere but on real glass - so each is a one-line fix.
//   1. Colour order is BGR by default  -> setRgbOrder(false) for RGB
//   2. Inversion is on by default      -> invertDisplay(false), after begin()
//   3. How the glass is mounted        -> setMirror(mx, my)
//
// The long gamma and power init sequences are left out on purpose. An ILI934x
// comes up fine on its power-on defaults; add them when you want to chase the
// colour rendition.
#pragma once
#include <stdint.h>

#include "Color.h"
#include "Panel.h"

class TinyGFXPanelILI9342 : public TinyGFXPanel {
 public:
  TinyGFXPanelILI9342(TinyGFXBus& bus, int16_t w = 320, int16_t h = 240, int8_t rst = -1)
      : _bus(&bus), _natW(w), _natH(h), _rst(rst) {
    _width = w;
    _height = h;
  }

  /// Colour order. ILI9342C modules are usually BGR, hence the default of
  /// true. Set false if red and blue come out swapped.
  void setRgbOrder(bool bgr) { _bgr = bgr; }

  /// How the glass is mounted. XORed into the MADCTL of every rotation.
  /// Fix a picture that comes out flipped horizontally or vertically here.
  void setMirror(bool mirrorX, bool mirrorY) {
    _flip = (uint8_t)((mirrorX ? MADCTL_MX : 0) | (mirrorY ? MADCTL_MY : 0));
  }

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

  // ---- read-back --------------------------------------------------------
  //
  // Not calling these costs nothing at all: an inline member function that is
  // never called is never emitted in the first place - the linker does not
  // even get a chance to drop it. The one thing everyone pays for is the bus's
  // readSequence, measured at 8 bytes.
  //
  // Only TinyGFXBusSPI can read. Software SPI has no MISO wire, so it returns
  // without touching the buffer, leaving it zeroed.

  /// Read the controller ID into `out` (3 bytes; the dummy byte is skipped).
  /// An ILI9341-family part answers with something like `00 93 41`.
  /// All-00 or all-FF means the data line never reaches you.
  void readId(uint8_t* out3) { readRegister(0x04, out3, 3); }

  /// Read any register, skipping the leading dummy byte.
  void readRegister(uint8_t reg, uint8_t* out, uint8_t n) {
    const uint8_t script[2] = {reg, 0};
    _bus->readSequence(script, 2, 1, out, n);
  }

  /// RDID4 (0xD3). An ILI9341 answers `00 93 41`.
  void readId4(uint8_t* out4) { readRegister(0xD3, out4, 4); }

  /// Read the GRAM back. Returns the number of pixels read.
  ///
  /// Slow - roughly 150us per pixel, because each one needs its own window and
  /// RAMRD. This is a debugging and verification tool, not a drawing path.
  ///
  /// Even though pixels are written at 16bpp, they read back 3 bytes each
  /// (RGB666, in the high bits of each byte), and a dummy byte comes first.
  /// Both are ILI934x conventions, absorbed here.
  ///
  /// Inversion (INVON / INVOFF) happens on the display side and never shows up
  /// in the GRAM, so read-back cannot tell you whether it is on.
  uint32_t readPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* out);



 private:
  enum : uint8_t {
    MADCTL_MY = 0x80, MADCTL_MX = 0x40, MADCTL_MV = 0x20, MADCTL_BGR = 0x08,
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
  int8_t _rst;
  uint8_t _flip = 0;
  bool _bgr = true;
};

inline bool TinyGFXPanelILI9342::init() {
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
  cmdData(0x21, nullptr, 0);  // INVON (the ILI9342C on an M5Stack needs it)
  cmdData(0x13, nullptr, 0);  // NORON
  _bus->endTransaction();
  setRotation(0);
  _bus->beginTransaction();
  cmdData(0x29, nullptr, 0);  // DISPON
  _bus->endTransaction();
  return true;
}

inline void TinyGFXPanelILI9342::setRotation(uint8_t r) {
  r = (uint8_t)(r & 3);
  // The table itself is the same as the ST7789's; because the GRAM is
  // landscape, rotation 0 comes out 320x240. Differences in how the glass is
  // mounted are absorbed by _flip.
  uint8_t madctl;
  switch (r) {
    case 0: madctl = 0; _width = _natW; _height = _natH; break;
    case 1: madctl = (uint8_t)(MADCTL_MV | MADCTL_MX); _width = _natH; _height = _natW; break;
    case 2: madctl = (uint8_t)(MADCTL_MX | MADCTL_MY); _width = _natW; _height = _natH; break;
    default: madctl = (uint8_t)(MADCTL_MV | MADCTL_MY); _width = _natH; _height = _natW; break;
  }
  madctl ^= _flip;
  if (_bgr) madctl |= MADCTL_BGR;
  _bus->beginTransaction();
  cmdData(0x36, &madctl, 1);
  _bus->endTransaction();
}

inline uint32_t TinyGFXPanelILI9342::readPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                                uint16_t* out) {
  if (w == 0 || h == 0 || out == nullptr) return 0;
  uint32_t i = 0;
  for (uint16_t row = 0; row < h; ++row) {
    const uint16_t cy = (uint16_t)(y + row);
    for (uint16_t col = 0; col < w; ++col) {
      const uint16_t cx = (uint16_t)(x + col);
      // One window and one RAMRD per pixel. A single RAMRD over a run does not
      // advance the column address on this controller - every pixel comes back
      // as the first one (measured).
      const uint8_t script[14] = {
          0x2A, 4, (uint8_t)(cx >> 8), (uint8_t)cx, (uint8_t)(cx >> 8), (uint8_t)cx,
          0x2B, 4, (uint8_t)(cy >> 8), (uint8_t)cy, (uint8_t)(cy >> 8), (uint8_t)cy,
          0x2E, 0,
      };
      uint8_t px[3] = {0, 0, 0};
      _bus->readSequence(script, 14, 1, px, 3);
      out[i++] = tinygfx_color565(px[0], px[1], px[2]);
    }
  }
  return i;
}

inline void TinyGFXPanelILI9342::setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
  uint8_t a[4];
  a[0] = (uint8_t)(xs >> 8); a[1] = (uint8_t)xs; a[2] = (uint8_t)(xe >> 8); a[3] = (uint8_t)xe;
  cmdData(0x2A, a, 4);  // CASET
  a[0] = (uint8_t)(ys >> 8); a[1] = (uint8_t)ys; a[2] = (uint8_t)(ye >> 8); a[3] = (uint8_t)ye;
  cmdData(0x2B, a, 4);  // RASET
  cmdData(0x2C, nullptr, 0);  // RAMWR
}
