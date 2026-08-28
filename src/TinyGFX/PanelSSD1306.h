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

// Fill whole bytes when a rectangle covers a page, instead of setting one bit
// at a time. Costs 512 bytes of flash on a CH32V003 and saves roughly 6ms of
// the ~30ms it takes to clear and push a 128x64 frame - the I2C transfer is
// the bigger half and this does not touch it. Set to 0 to get the flash back.
#ifndef TINYGFX_MONO_FAST_FILL
#define TINYGFX_MONO_FAST_FILL 1
#endif

#include "Panel.h"

class TinyGFXPanelSSD1306 : public TinyGFXPanel {
 public:
  /// `buffer` is w * h / 8 bytes - 1,024 for 128x64.
  /// `buffer` normally holds the whole screen: w * h / 8 bytes.
  ///
  /// Pass `bufferPages` to hand over less than that - `w * bufferPages` bytes -
  /// and drive the panel a band at a time with setBandPage(). One page is 8
  /// rows, so a 128x64 needs 128 bytes per page instead of 1,024 for the lot.
  TinyGFXPanelSSD1306(TinyGFXBus& bus, uint8_t* buffer, int16_t w = 128, int16_t h = 64,
                      int16_t bufferPages = 0)
      : _bus(&bus), _buf(buffer), _natW(w), _natH(h) {
    _width = w;
    _height = h;
    _pages = (int16_t)(h >> 3);
    _bandPages = (bufferPages > 0 && bufferPages < _pages) ? bufferPages : 0;
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
    // Only drawPixel and a hand-driven setAddrWindow reach this now; solid
    // rectangles go through fillRect below.
    const bool on = (color != 0);
    while (count--) put(on);
  }
  void writePixels(const uint16_t* data, uint32_t count) override {
    while (count--) put(*data++ != 0);
  }
  void beginTransaction() override {}
  void endTransaction() override {}

  /// Which page the band buffer currently stands for. Only meaningful when the
  /// constructor was given a `bufferPages` smaller than the screen.
  ///
  /// This is how a monochrome panel is driven without a full framebuffer:
  /// point the band at a page, clear it, draw the whole scene with the clip set
  /// to that band, push it, move on.
  ///
  /// ```cpp
  /// for (int16_t p = 0; p < 8; ++p) {
  ///   panel.setBandPage(p);
  ///   panel.clearBuffer();
  ///   lcd.setClipRect(0, p * 8, 128, 8);
  ///   drawScene(lcd);
  ///   panel.display();
  /// }
  /// lcd.resetClipRect();
  /// ```
  ///
  /// The trade is on the wire, not in RAM. Both ways push the same bytes for a
  /// full redraw, but a whole-screen buffer can push just the pages that
  /// changed, and a band cannot. docs/FOOTPRINT.ja.md has the numbers.
  void setBandPage(int16_t first) {
    _pageFirst = first;
    _dirtyLo = 32767;
    _dirtyHi = -1;
  }

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

 public:
#if TINYGFX_MONO_FAST_FILL
  /// Fill a rectangle a byte at a time instead of a bit at a time.
  ///
  /// This is the common case by a wide margin - fillRect, fillScreen, every
  /// span of a circle or a triangle, the background cell behind text, and
  /// every run of every glyph. Eight vertical pixels share a byte here, so
  /// going through the address window and setting one bit per pixel throws
  /// most of the work away.
  ///
  /// Whatever the rotation, an axis-aligned logical rectangle is still an
  /// axis-aligned rectangle in the buffer, so map the two corners and fill
  /// that. Within a page the covered rows become one mask, and every column
  /// in the run shares it.
  void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) override {
    const bool on = (color != 0);
    int16_t ax, ay, bx, by;
    toBuffer((int16_t)x, (int16_t)y, &ax, &ay);
    toBuffer((int16_t)(x + w - 1), (int16_t)(y + h - 1), &bx, &by);
    if (ax > bx) { const int16_t t = ax; ax = bx; bx = t; }
    if (ay > by) { const int16_t t = ay; ay = by; by = t; }
    if (ax < 0) ax = 0;
    if (ay < 0) ay = 0;
    if (bx > (int16_t)(_natW - 1)) bx = (int16_t)(_natW - 1);
    if (by > (int16_t)(_natH - 1)) by = (int16_t)(_natH - 1);
    if (ax > bx || ay > by) return;

    for (int16_t y = ay; y <= by;) {
      const int16_t page = (int16_t)(y >> 3);
      const int16_t pageTop = (int16_t)(page << 3);
      const int16_t lo = (int16_t)(y - pageTop);
      int16_t hi = (int16_t)(by - pageTop);
      if (hi > 7) hi = 7;
      // Bits lo..hi set, without looping over them: keep the low end, drop
      // everything above hi.
      const uint8_t mask = (uint8_t)((uint8_t)(0xFFu << lo) & (uint8_t)(0xFFu >> (7 - hi)));
      int16_t slot = page;
      if (_bandPages != 0) {
        slot = (int16_t)(page - _pageFirst);
        if (slot < 0 || slot >= _bandPages) { y = (int16_t)(pageTop + 8); continue; }
      }
      uint8_t* row = &_buf[(int32_t)slot * _natW];
      if (on) {
        for (int16_t x = ax; x <= bx; ++x) row[x] = (uint8_t)(row[x] | mask);
      } else {
        const uint8_t clear = (uint8_t)~mask;
        for (int16_t x = ax; x <= bx; ++x) row[x] = (uint8_t)(row[x] & clear);
      }
      // Dirty is tracked in buffer space, so mark the slot, not the screen page.
      if (slot < _dirtyLo) _dirtyLo = slot;
      if (slot > _dirtyHi) _dirtyHi = slot;
      y = (int16_t)(pageTop + 8);
    }
  }
#endif  // TINYGFX_MONO_FAST_FILL

 private:

  /// Logical coordinates to buffer coordinates. Rotation lives here.
  void toBuffer(int16_t x, int16_t y, int16_t* fx, int16_t* fy) const {
    switch (_rotation) {
      case 1:  *fx = y;                        *fy = (int16_t)(_natH - 1 - x); break;
      case 2:  *fx = (int16_t)(_natW - 1 - x); *fy = (int16_t)(_natH - 1 - y); break;
      case 3:  *fx = (int16_t)(_natW - 1 - y); *fy = x;                        break;
      default: *fx = x;                        *fy = y;                        break;
    }
  }

  /// Logical coordinates to a bit in the buffer. Out-of-range writes are dropped.
  void put(bool on) {
    const uint16_t x = _cx, y = _cy;
    // Advance to the next pixel up front, so an early return cannot desync it
    if (_cx >= _xe) { _cx = _xs; ++_cy; } else { ++_cx; }
    // Unsigned, so "below zero" and "past the edge" are the same comparison.
    if (x >= (uint16_t)_width || y >= (uint16_t)_height) return;

    int16_t fx, fy;
    toBuffer((int16_t)x, (int16_t)y, &fx, &fy);
    int16_t page = (int16_t)(fy >> 3);
    if (_bandPages != 0) {
      page = (int16_t)(page - _pageFirst);
      if (page < 0 || page >= _bandPages) return;  // outside the band
    }
    // 16 bits is enough: a page-addressed panel is at most a few hundred
    // pixels wide and eight pages tall, so the index cannot overflow.
    uint8_t* slot = &_buf[(int16_t)(page * _natW + fx)];
    const uint8_t mask = (uint8_t)(1u << (fy & 7));
    if (on) *slot = (uint8_t)(*slot | mask);
    else    *slot = (uint8_t)(*slot & (uint8_t)~mask);
    if (page < _dirtyLo) _dirtyLo = page;
    if (page > _dirtyHi) _dirtyHi = page;
  }

  TinyGFXBus* _bus;
  uint8_t* _buf;
  int16_t _natW, _natH, _pages;
  int16_t _pageFirst = 0;
  int16_t _bandPages = 0;  // 0 means the buffer covers the whole screen
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
  // However many pages the buffer actually holds - clearing the whole screen's
  // worth would run off the end of a band buffer.
  const int16_t pages = (_bandPages != 0) ? _bandPages : _pages;
  const int32_t n = (int32_t)_natW * pages;
  const uint8_t v = on ? 0xFF : 0x00;
  for (int32_t i = 0; i < n; ++i) _buf[i] = v;
  _dirtyLo = 0;
  _dirtyHi = (int16_t)(pages - 1);
}

inline void TinyGFXPanelSSD1306::display() {
  if (_dirtyHi < _dirtyLo) return;  // nothing changed
  // Dirty pages are tracked in buffer space; the screen may be further down.
  const int16_t base = (_bandPages != 0) ? _pageFirst : 0;
  cmd(0x21); cmd(0); cmd((uint8_t)(_natW - 1));                              // column range
  cmd(0x22); cmd((uint8_t)(base + _dirtyLo)); cmd((uint8_t)(base + _dirtyHi));  // page range
  _bus->writeData(&_buf[(int32_t)_dirtyLo * _natW],
                  (size_t)((int32_t)(_dirtyHi - _dirtyLo + 1) * _natW));
  _dirtyLo = 32767;
  _dirtyHi = -1;
}
