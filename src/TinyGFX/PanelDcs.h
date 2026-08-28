// TinyGFX - the shared half of every 4-wire SPI TFT
//
// ST7789, ST7735, ILI9341 and ILI9342C are different controllers, but they all
// speak the same command set - MIPI DCS - and TinyGFX only ever uses the part
// of it that is identical across them:
//
//   0x2A CASET   column address       0x36 MADCTL  rotation / mirror / colour order
//   0x2B RASET   row address          0x3A COLMOD  pixel format
//   0x2C RAMWR   start writing        0x2E RAMRD   start reading
//
// Their power-on sequences agree too: reset, SWRESET, SLPOUT, COLMOD 16bpp,
// inversion, NORON, MADCTL, DISPON. So does the rest of the plumbing.
//
// What actually differs between them is data, not code:
//
//   - the GRAM origin offset, and how big the GRAM is compared to the panel
//     (ST7735 modules vary wildly, a 135x240 ST7789 needs both, an ILI9342C
//     needs neither)
//   - whether the glass is wired BGR or RGB
//   - whether it is mounted mirrored
//   - whether the panel wants inversion on
//
// All four live here as members that default to "no adjustment", so a
// controller that needs none of them writes nothing, and one that needs all of
// them sets four values in its constructor. That is why adding a controller is
// a few lines rather than another copy of this file.
//
// This class is not meant to be used directly - construct a TinyGFXPanelST7789
// or a TinyGFXPanelILI9342.
#pragma once
#include <stdint.h>

#include "Color.h"
#include "Panel.h"

class TinyGFXPanelDcs : public TinyGFXPanel {
 public:
  /// The module's GRAM origin offset. Give the value for rotation 0; the
  /// offsets for rotations 1-3 are derived from this and setGramSize().
  void setOffset(int16_t x, int16_t y) { _offX0 = x; _offY0 = y; }

  /// The controller's GRAM size; defaults to the panel size.
  /// On a module with an offset, rotations 2 and 3 land in the wrong place
  /// unless this is set.
  /// For example: a 240x240 ST7789 wants setGramSize(240, 320), and a
  /// 135x240 one wants setGramSize(240, 320) plus setOffset(52, 40).
  void setGramSize(int16_t w, int16_t h) { _gramW = w; _gramH = h; }

  /// Colour order. Set the other way if red and blue come out swapped.
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

  /// RDID4 (0xD3). An ILI9341 answers `00 93 41`.
  void readId4(uint8_t* out4) { readRegister(0xD3, out4, 4); }

  /// Read any register, skipping the leading dummy byte.
  void readRegister(uint8_t reg, uint8_t* out, uint8_t n) {
    const uint8_t script[2] = {reg, 0};
    _bus->readSequence(script, 2, 1, out, n);
  }

  /// Read the GRAM back. Returns the number of pixels read.
  ///
  /// Slow - roughly 150us per pixel, because each one needs its own window and
  /// RAMRD. This is a debugging and verification tool, not a drawing path.
  ///
  /// Even though pixels are written at 16bpp, they read back 3 bytes each
  /// (RGB666, in the high bits of each byte), and a dummy byte comes first.
  /// Both are DCS conventions, absorbed here.
  ///
  /// Inversion (INVON / INVOFF) happens on the display side and never shows up
  /// in the GRAM, so read-back cannot tell you whether it is on.
  uint32_t readPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* out);

 protected:
  enum : uint8_t {
    MADCTL_MY = 0x80, MADCTL_MX = 0x40, MADCTL_MV = 0x20, MADCTL_BGR = 0x08,
  };

  /// `bgr` and `invert` are the panel's power-on character, so they are
  /// constructor arguments rather than setters - a controller states them once
  /// and a sketch overrides with setRgbOrder() / invertDisplay() if its
  /// particular module disagrees.
  TinyGFXPanelDcs(TinyGFXBus& bus, int16_t w, int16_t h, int8_t rst, bool bgr, bool invert)
      : _bus(&bus), _natW(w), _natH(h), _rst(rst), _bgr(bgr), _invert(invert) {
    _width = w;
    _height = h;
  }
  ~TinyGFXPanelDcs() = default;

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
  uint8_t _flip = 0;
  bool _bgr;
  bool _invert;
};

inline bool TinyGFXPanelDcs::init() {
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
  cmdData(_invert ? 0x21 : 0x20, nullptr, 0);  // INVON / INVOFF
  cmdData(0x13, nullptr, 0);                   // NORON
  _bus->endTransaction();
  setRotation(0);
  _bus->beginTransaction();
  cmdData(0x29, nullptr, 0);  // DISPON
  _bus->endTransaction();
  return true;
}

inline void TinyGFXPanelDcs::setRotation(uint8_t r) {
  r = (uint8_t)(r & 3);
  uint8_t madctl;
  // The offset measured from the far side. This matters on modules whose
  // GRAM is larger than the panel.
  const int16_t gw = (_gramW > 0) ? _gramW : _natW;
  const int16_t gh = (_gramH > 0) ? _gramH : _natH;
  int16_t cs2 = (int16_t)(gw - _natW - _offX0);
  int16_t rs2 = (int16_t)(gh - _natH - _offY0);
  if (cs2 < 0) cs2 = 0;
  if (rs2 < 0) rs2 = 0;
  switch (r) {
    case 0: madctl = 0; _width = _natW; _height = _natH;
            _offX = _offX0; _offY = _offY0; break;
    case 1: madctl = (uint8_t)(MADCTL_MV | MADCTL_MX); _width = _natH; _height = _natW;
            _offX = _offY0; _offY = cs2; break;
    case 2: madctl = (uint8_t)(MADCTL_MX | MADCTL_MY); _width = _natW; _height = _natH;
            _offX = cs2; _offY = rs2; break;
    default: madctl = (uint8_t)(MADCTL_MV | MADCTL_MY); _width = _natH; _height = _natW;
             _offX = rs2; _offY = _offX0; break;
  }
  madctl ^= _flip;
  if (_bgr) madctl |= MADCTL_BGR;
  _bus->beginTransaction();
  cmdData(0x36, &madctl, 1);
  _bus->endTransaction();
}

inline void TinyGFXPanelDcs::setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
  xs = (uint16_t)(xs + _offX); xe = (uint16_t)(xe + _offX);
  ys = (uint16_t)(ys + _offY); ye = (uint16_t)(ye + _offY);
  uint8_t a[4];
  a[0] = (uint8_t)(xs >> 8); a[1] = (uint8_t)xs; a[2] = (uint8_t)(xe >> 8); a[3] = (uint8_t)xe;
  cmdData(0x2A, a, 4);  // CASET
  a[0] = (uint8_t)(ys >> 8); a[1] = (uint8_t)ys; a[2] = (uint8_t)(ye >> 8); a[3] = (uint8_t)ye;
  cmdData(0x2B, a, 4);  // RASET
  cmdData(0x2C, nullptr, 0);  // RAMWR
}

inline uint32_t TinyGFXPanelDcs::readPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                            uint16_t* out) {
  if (w == 0 || h == 0 || out == nullptr) return 0;
  uint32_t i = 0;
  for (uint16_t row = 0; row < h; ++row) {
    const uint16_t cy = (uint16_t)(y + row + _offY);
    for (uint16_t col = 0; col < w; ++col) {
      const uint16_t cx = (uint16_t)(x + col + _offX);
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
