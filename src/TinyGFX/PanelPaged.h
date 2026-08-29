// TinyGFX - the shared half of every page-addressed 1bpp panel
//
// SSD1306, SH1106, SSD1309 and the ST7565 family all lay their memory out the
// same way: **one byte is eight vertical pixels**, the screen is a stack of
// 8-row "pages", and the controller is written a page at a time.
//
// That layout, not the fact that they are monochrome, is what they share. A
// Sharp Memory LCD is monochrome too but packs eight *horizontal* pixels per
// byte, which makes it as different from an SSD1306 as an RGB565 TFT is - it
// would want its own base, not this one (docs/OPTIMIZE.ja.md H).
//
// Two things follow from the layout, and both live here.
//
//   1. **A framebuffer is required.** The GRAM cannot be read back and only
//      writes whole pages, so arbitrary drawing means keeping the bits
//      locally. That is 1,024 bytes for 128x64, supplied by the caller -
//      nothing is allocated here.
//   2. **Transfers are deferred.** Pushing 1 KB after every draw is out of the
//      question, so the panel tracks which pages changed and display() sends
//      only those.
//
// The drawing API stays RGB565; this is where it collapses to 1bpp, with
// "non-zero lights up". Colour depth is deliberately not abstracted in the
// core (docs/DECISIONS.ja.md D4).
//
// Rotation is done by indexing the buffer rather than in the controller: none
// of these parts has a 90-degree rotation, so there is no MADCTL to lean on.
//
// **What a controller has to supply for itself is init() and display().** They
// are not virtual: a sketch calls display() on the concrete panel it declared,
// never through a TinyGFXPanel*, so each panel simply has its own and nobody
// pays for a vtable slot. The page-addressing commands genuinely differ - an
// SSD1306 can be handed a column and page *range* (0x21 / 0x22) and then
// streamed, while an SH1106 has no such command and needs its cursor set per
// page - so this is a real difference, not boilerplate worth folding away.
#pragma once
#include <stdint.h>

// Take over fillRect and paint whole bytes, instead of letting the base class
// go through the address window and set one bit per pixel.
//
// Eight vertical pixels share a byte here, so this is most of the work for
// every rectangle, every span of a circle or a triangle, the background cell
// behind text, and every run of every glyph. It saves roughly 6ms of the ~30ms
// it takes to clear and push a 128x64 frame - the I2C transfer is the bigger
// half and this does not touch it.
//
// Set to 0 to get the flash back: 428 bytes on a CH32V003, 596 on AVR
// (measured 2026-08-29). Drawing then goes back to one bit at a time.
#ifndef TINYGFX_MONO_FAST_FILL
#define TINYGFX_MONO_FAST_FILL 1
#endif

#include "Panel.h"
#include "Progmem.h"

class TinyGFXPanelPaged : public TinyGFXPanel {
 public:
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
  // Deliberately empty, and not an oversight.
  //
  // TinyGFX::startWrite() calls this before a drawing burst, but a paged panel
  // puts nothing on the bus while drawing - every primitive lands in the local
  // framebuffer and only display() sends anything. Opening an SPI transaction
  // here would hold CS low for the whole of the drawing arithmetic, which is
  // exactly what a bus shared with an SD card must not do.
  //
  // The bus traffic that does exist - init(), display(), and the one-off
  // commands - opens its own transaction. See cmd() below.
  void beginTransaction() override {}
  void endTransaction() override {}

#if TINYGFX_MONO_FAST_FILL
  /// Fill a rectangle a byte at a time instead of a bit at a time.
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

    for (int16_t yy = ay; yy <= by;) {
      const int16_t page = (int16_t)(yy >> 3);
      const int16_t pageTop = (int16_t)(page << 3);
      const int16_t lo = (int16_t)(yy - pageTop);
      int16_t hi = (int16_t)(by - pageTop);
      if (hi > 7) hi = 7;
      // Bits lo..hi set, without looping over them: keep the low end, drop
      // everything above hi.
      const uint8_t mask = (uint8_t)((uint8_t)(0xFFu << lo) & (uint8_t)(0xFFu >> (7 - hi)));
      int16_t slot = page;
      if (_bandPages != 0) {
        slot = (int16_t)(page - _pageFirst);
        if (slot < 0 || slot >= _bandPages) { yy = (int16_t)(pageTop + 8); continue; }
      }
      uint8_t* row = &_buf[(int32_t)slot * _natW];
      if (on) {
        for (int16_t xx = ax; xx <= bx; ++xx) row[xx] = (uint8_t)(row[xx] | mask);
      } else {
        const uint8_t clear = (uint8_t)~mask;
        for (int16_t xx = ax; xx <= bx; ++xx) row[xx] = (uint8_t)(row[xx] & clear);
      }
      // Dirty is tracked in buffer space, so mark the slot, not the screen page.
      if (slot < _dirtyLo) _dirtyLo = slot;
      if (slot > _dirtyHi) _dirtyHi = slot;
      yy = (int16_t)(pageTop + 8);
    }
  }
#endif  // TINYGFX_MONO_FAST_FILL

  /// Blit a vertically packed 1bpp bitmap straight into the buffer.
  ///
  /// **This is the reason the vertical layout exists.** One byte of the data
  /// is eight vertical pixels, which is exactly what one byte of this panel's
  /// buffer is - so when the two line up, the whole picture is a copy. No bit
  /// twiddling, no runs, no fillRect.
  ///
  /// Lining up means all of:
  ///
  ///   - `y` on a page boundary and `h` a whole number of pages
  ///   - rotation 0 (any other rotation is a different traversal)
  ///   - the rectangle inside the panel, and inside the band if there is one
  ///
  /// **Returns false when they do not hold, having drawn nothing** - fall back
  /// to `lcd.drawImage()`, which handles any position at any rotation. A
  /// splash screen or a fixed background hits the fast path; a sprite moving a
  /// pixel at a time does not, and should not use this.
  ///
  /// ```cpp
  /// if (!panel.pushVBitmap(0, 0, 128, 64, splashData)) {
  ///   lcd.drawImage(&splashRef, 0, 0);   // どこにでも貼れる汎用経路
  /// }
  /// panel.display();
  /// ```
  ///
  /// Deliberately **not** virtual and not part of TinyGFXPanel: a colour panel
  /// has no such layout, and making it virtual would charge every panel for a
  /// vtable slot it can never use (the fillRect seam cost +40 B that way).
  ///
  /// The data is read with `tinygfx_rd8`, so on AVR it must be in PROGMEM -
  /// the same rule as fonts and `drawBitmap`.
  bool pushVBitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t* data) {
    if (data == nullptr || w <= 0 || h <= 0) return false;
    if (_rotation != 0) return false;
    if ((y & 7) != 0 || (h & 7) != 0) return false;
    if (x < 0 || y < 0 || x + w > _natW || y + h > _natH) return false;

    const int16_t page0 = (int16_t)(y >> 3);
    const int16_t pages = (int16_t)(h >> 3);
    for (int16_t p = 0; p < pages; ++p) {
      int16_t slot = (int16_t)(page0 + p);
      if (_bandPages != 0) {
        slot = (int16_t)(slot - _pageFirst);
        if (slot < 0 || slot >= _bandPages) continue;  // この帯には出ない
      }
      uint8_t* dst = &_buf[(int32_t)slot * _natW + x];
      const uint8_t* src = data + (int32_t)p * w;
      for (int16_t i = 0; i < w; ++i) dst[i] = tinygfx_rd8(&src[i]);
      if (slot < _dirtyLo) _dirtyLo = slot;
      if (slot > _dirtyHi) _dirtyHi = slot;
    }
    return true;
  }

  /// Which page the band buffer currently stands for. Only meaningful when the
  /// constructor was given a `bufferPages` smaller than the screen.
  ///
  /// This is how one of these panels is driven without a full framebuffer:
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

  /// Clear the local buffer. The screen is untouched until display().
  void clearBuffer(bool on = false) {
    // However many pages the buffer actually holds - clearing the whole
    // screen's worth would run off the end of a band buffer.
    const int16_t pages = (_bandPages != 0) ? _bandPages : _pages;
    const int32_t n = (int32_t)_natW * pages;
    const uint8_t v = on ? 0xFF : 0x00;
    for (int32_t i = 0; i < n; ++i) _buf[i] = v;
    _dirtyLo = 0;
    _dirtyHi = (int16_t)(pages - 1);
  }

 protected:
  /// `buffer` normally holds the whole screen: w * h / 8 bytes.
  ///
  /// Pass `bufferPages` to hand over less than that - `w * bufferPages` bytes -
  /// and drive the panel a band at a time with setBandPage(). One page is 8
  /// rows, so a 128x64 needs 128 bytes per page instead of 1,024 for the lot.
  TinyGFXPanelPaged(TinyGFXBus& bus, uint8_t* buffer, int16_t w, int16_t h, int16_t bufferPages)
      : _bus(&bus), _buf(buffer), _natW(w), _natH(h) {
    _width = w;
    _height = h;
    _pages = (int16_t)(h >> 3);
    _bandPages = (bufferPages > 0 && bufferPages < _pages) ? bufferPages : 0;
  }
  ~TinyGFXPanelPaged() = default;

  /// One command, in a transaction of its own. For a sketch calling
  /// invertDisplay() and friends outside any drawing burst.
  ///
  /// init() and display() do NOT use this - they open one transaction around
  /// the whole sequence instead, because SPI.beginTransaction() does not nest.
  void cmd(uint8_t c) {
    _bus->beginTransaction();
    _bus->writeCommand(c);
    _bus->endTransaction();
  }

  /// Is this panel set up in a way it can actually work with?
  ///
  /// Checked once, in init(), so that a mistake shows up as begin() returning
  /// false instead of as a corrupt picture or a write past the end of the
  /// buffer. None of it can be caught at compile time: the buffer is a pointer
  /// the sketch owns and the size comes from constructor arguments.
  bool configOk() const {
    if (_buf == nullptr) return false;
    if (_natW <= 0 || _natH <= 0) return false;
    if ((_natH & 7) != 0) return false;   // whole pages only - 8 rows to a byte
    if (_bandPages < 0 || _bandPages > _pages) return false;
    return true;
  }

  /// The multiplex ratio and COM pin layout for this height.
  ///
  /// The rest of the init sequence is the same for every size, but these two
  /// are not: a 128x32 wants 0x1F / 0x02 where a 128x64 wants 0x3F / 0x12.
  /// Sending the 64-row values to a 32-row panel gives a picture squeezed into
  /// half the glass, which is the usual symptom of a library that hardcodes
  /// them.
  uint8_t multiplexRatio() const { return (uint8_t)(_natH - 1); }
  uint8_t comPinsConfig() const { return (_natH == 32) ? (uint8_t)0x02 : (uint8_t)0x12; }

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
