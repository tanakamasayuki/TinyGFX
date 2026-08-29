// TinyGFX - drawing core
//
// Not one virtual method in this class (docs/DECISIONS.ja.md D1). Everything
// is inline in the header, so a method nobody calls is never emitted at all.
// The core references neither <Arduino.h> nor Serial (docs/CORE_DESIGN.ja.md
// 7.4, rule R4), and never divides or takes a remainder - the CH32V003 is
// rv32ec and has no divide instruction.
#pragma once
#include <stdint.h>

#include "Color.h"
#include "Font.h"
#include "Panel.h"


/// Where an uncovered code falls back to (REPLACEMENT CHARACTER).
/// Whether a notdef box appears is decided by whether the font includes
/// U+FFFD; the core has no built-in box of its own (CellFont spec 7.2).
#define TINYGFX_NOTDEF 0xFFFDu

class TinyGFX {
 public:
  explicit TinyGFX(TinyGFXPanel& panel) : _panel(&panel) {}

  // ---- basics ----------------------------------------------------------
  /// Bring the panel up. Returns whether the **configuration** is usable.
  ///
  /// It does not, and cannot, mean "a panel answered". Every panel TinyGFX
  /// drives is write-only in normal use - there is no acknowledgement on a
  /// 4-wire SPI display at all, and the one panel that can be read back
  /// (docs/MANUAL_TEST.ja.md) needs the line turned around at 150us a pixel,
  /// which is not something begin() is going to do. A sketch that treats
  /// `true` as "the screen is alive" is fooling itself, whatever this returned.
  ///
  /// What it does catch is the class of mistake the compiler cannot: a null
  /// framebuffer, a height that is not a whole number of pages, a zero
  /// dimension. Those come from constructor arguments and a pointer the sketch
  /// owns, and they end in a corrupt picture or a write past the end of a
  /// buffer. **Those return false, and nothing is sent.**
  bool begin() {
    const bool ok = _panel->init();
    resetClipRect();
    return ok;
  }
  int16_t width() const { return _panel->width(); }
  int16_t height() const { return _panel->height(); }
  uint8_t getRotation() const { return _rotation; }
  void setRotation(uint8_t r) {
    _rotation = (uint8_t)(r & 3);
    _panel->setRotation(_rotation);
    resetClipRect();
  }
  static constexpr uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return tinygfx_color565(r, g, b);
  }

  // ---- transfer control ------------------------------------------------
  void startWrite() {
    if (_txn++ == 0) _panel->beginTransaction();
  }
  void endWrite() {
    if (_txn != 0 && --_txn == 0) _panel->endTransaction();
  }

  // ---- clipping --------------------------------------------------------
  void setClipRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    // 32 bits for the far edge - see fillRect for why.
    int32_t x1 = (int32_t)x + w - 1;
    int32_t y1 = (int32_t)y + h - 1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    const int16_t mx = (int16_t)(width() - 1);
    const int16_t my = (int16_t)(height() - 1);
    if (x1 > mx) x1 = mx;
    if (y1 > my) y1 = my;
    _clipX0 = x; _clipY0 = y;
    _clipX1 = (int16_t)x1; _clipY1 = (int16_t)y1;  // clamped above, so this fits
  }
  void resetClipRect() {
    _clipX0 = 0; _clipY0 = 0;
    _clipX1 = (int16_t)(width() - 1);
    _clipY1 = (int16_t)(height() - 1);
  }
  void clearClipRect() { resetClipRect(); }

  // ---- low-level transfer ----------------------------------------------
  void setAddrWindow(int16_t x, int16_t y, int16_t w, int16_t h) {
    _panel->setWindow((uint16_t)x, (uint16_t)y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
  }
  void writeColor(uint16_t color, uint32_t count) { _panel->writeColor(color, count); }
  void writePixels(const uint16_t* data, uint32_t count) { _panel->writePixels(data, count); }

  // ---- primitives ------------------------------------------------------
  /// A pixel is a 1x1 rectangle. Going through fillRect means clipping is
  /// written once and a panel that took over fillRect gets to serve this too.
  void drawPixel(int16_t x, int16_t y, uint16_t color) { fillRect(x, y, 1, 1, color); }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    // The far edge in 32 bits. Taking coordinates from outside the screen and
    // clipping them is part of the contract, and x + w - 1 overflows int16_t
    // long before the caller is doing anything unreasonable: x=2, w=32767
    // wraps to -32768, so x > x1 and a rectangle that should have covered the
    // whole screen is dropped instead. 28,441 (x, w) pairs behave that way.
    // Both are back inside int16_t by the time they are used, because the clip
    // rectangle is.
    int32_t x1 = (int32_t)x + w - 1;
    int32_t y1 = (int32_t)y + h - 1;
    if (x < _clipX0) x = _clipX0;
    if (y < _clipY0) y = _clipY0;
    if (x1 > _clipX1) x1 = _clipX1;
    if (y1 > _clipY1) y1 = _clipY1;
    if (x > x1 || y > y1) return;
    startWrite();
    _panel->fillRect((uint16_t)x, (uint16_t)y, (uint16_t)(x1 - x + 1),
                     (uint16_t)(y1 - y + 1), color);
    endWrite();
  }

  void fillScreen(uint16_t color) { fillRect(0, 0, width(), height(), color); }
  void clear(uint16_t color = 0) { fillScreen(color); }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { fillRect(x, y, w, 1, color); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) { fillRect(x, y, 1, h, color); }

  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    startWrite();
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, (int16_t)(y + h - 1), w, color);
    if (h > 2) {
      drawFastVLine(x, (int16_t)(y + 1), (int16_t)(h - 2), color);
      drawFastVLine((int16_t)(x + w - 1), (int16_t)(y + 1), (int16_t)(h - 2), color);
    }
    endWrite();
  }

  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (y0 == y1) {
      if (x1 < x0) { const int16_t t = x0; x0 = x1; x1 = t; }
      drawFastHLine(x0, y0, (int16_t)(x1 - x0 + 1), color);
      return;
    }
    if (x0 == x1) {
      if (y1 < y0) { const int16_t t = y0; y0 = y1; y1 = t; }
      drawFastVLine(x0, y0, (int16_t)(y1 - y0 + 1), color);
      return;
    }
    int16_t dx = (int16_t)(x1 > x0 ? x1 - x0 : x0 - x1);
    int16_t dy = (int16_t)(y1 > y0 ? y1 - y0 : y0 - y1);
    const int16_t sx = (int16_t)(x0 < x1 ? 1 : -1);
    const int16_t sy = (int16_t)(y0 < y1 ? 1 : -1);
    int16_t err = (int16_t)(dx - dy);
    startWrite();
    for (;;) {
      drawPixel(x0, y0, color);
      if (x0 == x1 && y0 == y1) break;
      const int16_t e2 = (int16_t)(err << 1);
      if (e2 > -dy) { err = (int16_t)(err - dy); x0 = (int16_t)(x0 + sx); }
      if (e2 < dx)  { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
    endWrite();
  }

  void drawCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    if (r < 0) return;
    int16_t x = 0, y = r, d = (int16_t)(1 - r);
    startWrite();
    while (x <= y) {
      drawPixel((int16_t)(cx + x), (int16_t)(cy + y), color);
      drawPixel((int16_t)(cx - x), (int16_t)(cy + y), color);
      drawPixel((int16_t)(cx + x), (int16_t)(cy - y), color);
      drawPixel((int16_t)(cx - x), (int16_t)(cy - y), color);
      drawPixel((int16_t)(cx + y), (int16_t)(cy + x), color);
      drawPixel((int16_t)(cx - y), (int16_t)(cy + x), color);
      drawPixel((int16_t)(cx + y), (int16_t)(cy - x), color);
      drawPixel((int16_t)(cx - y), (int16_t)(cy - x), color);
      if (d < 0) {
        d = (int16_t)(d + (x << 1) + 3);
      } else {
        d = (int16_t)(d + ((x - y) << 1) + 5);
        --y;
      }
      ++x;
    }
    endWrite();
  }

  void fillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    if (r < 0) return;
    int16_t x = 0, y = r, d = (int16_t)(1 - r);
    startWrite();
    while (x <= y) {
      drawFastHLine((int16_t)(cx - x), (int16_t)(cy + y), (int16_t)((x << 1) + 1), color);
      drawFastHLine((int16_t)(cx - x), (int16_t)(cy - y), (int16_t)((x << 1) + 1), color);
      drawFastHLine((int16_t)(cx - y), (int16_t)(cy + x), (int16_t)((y << 1) + 1), color);
      drawFastHLine((int16_t)(cx - y), (int16_t)(cy - x), (int16_t)((y << 1) + 1), color);
      if (d < 0) {
        d = (int16_t)(d + (x << 1) + 3);
      } else {
        d = (int16_t)(d + ((x - y) << 1) + 5);
        --y;
      }
      ++x;
    }
    endWrite();
  }

  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    const int16_t rmax = (int16_t)(((w < h ? w : h) - 1) >> 1);
    if (r > rmax) r = rmax;
    if (r <= 0) { drawRect(x, y, w, h, color); return; }
    const int16_t x1 = (int16_t)(x + w - 1), y1 = (int16_t)(y + h - 1);
    startWrite();
    drawFastHLine((int16_t)(x + r), y, (int16_t)(w - (r << 1)), color);
    drawFastHLine((int16_t)(x + r), y1, (int16_t)(w - (r << 1)), color);
    drawFastVLine(x, (int16_t)(y + r), (int16_t)(h - (r << 1)), color);
    drawFastVLine(x1, (int16_t)(y + r), (int16_t)(h - (r << 1)), color);
    int16_t cx = 0, cy = r, d = (int16_t)(1 - r);
    while (cx <= cy) {
      drawPixel((int16_t)(x1 - r + cx), (int16_t)(y1 - r + cy), color);
      drawPixel((int16_t)(x + r - cx), (int16_t)(y1 - r + cy), color);
      drawPixel((int16_t)(x1 - r + cx), (int16_t)(y + r - cy), color);
      drawPixel((int16_t)(x + r - cx), (int16_t)(y + r - cy), color);
      drawPixel((int16_t)(x1 - r + cy), (int16_t)(y1 - r + cx), color);
      drawPixel((int16_t)(x + r - cy), (int16_t)(y1 - r + cx), color);
      drawPixel((int16_t)(x1 - r + cy), (int16_t)(y + r - cx), color);
      drawPixel((int16_t)(x + r - cy), (int16_t)(y + r - cx), color);
      if (d < 0) { d = (int16_t)(d + (cx << 1) + 3); }
      else { d = (int16_t)(d + ((cx - cy) << 1) + 5); --cy; }
      ++cx;
    }
    endWrite();
  }

  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    const int16_t rmax = (int16_t)(((w < h ? w : h) - 1) >> 1);
    if (r > rmax) r = rmax;
    if (r <= 0) { fillRect(x, y, w, h, color); return; }
    const int16_t x1 = (int16_t)(x + w - 1);
    startWrite();
    fillRect((int16_t)(x + r), y, (int16_t)(w - (r << 1)), h, color);
    int16_t cx = 0, cy = r, d = (int16_t)(1 - r);
    while (cx <= cy) {
      drawFastVLine((int16_t)(x + r - cy), (int16_t)(y + r - cx), (int16_t)(h - ((r - cx) << 1)), color);
      drawFastVLine((int16_t)(x1 - r + cy), (int16_t)(y + r - cx), (int16_t)(h - ((r - cx) << 1)), color);
      drawFastVLine((int16_t)(x + r - cx), (int16_t)(y + r - cy), (int16_t)(h - ((r - cy) << 1)), color);
      drawFastVLine((int16_t)(x1 - r + cx), (int16_t)(y + r - cy), (int16_t)(h - ((r - cy) << 1)), color);
      if (d < 0) { d = (int16_t)(d + (cx << 1) + 3); }
      else { d = (int16_t)(d + ((cx - cy) << 1) + 5); --cy; }
      ++cx;
    }
    endWrite();
  }

  void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                    uint16_t color) {
    startWrite();
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
    endWrite();
  }

  /// Filled triangle without a divide: the edges are stepped with Bresenham
  /// and each scanline becomes a horizontal line.
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                    uint16_t color) {
    // sort by y, ascending
    if (y0 > y1) { swap16(x0, x1); swap16(y0, y1); }
    if (y1 > y2) { swap16(x1, x2); swap16(y1, y2); }
    if (y0 > y1) { swap16(x0, x1); swap16(y0, y1); }
    if (y0 == y2) {  // degenerate: a horizontal line
      int16_t lo = x0, hi = x0;
      if (x1 < lo) lo = x1; else if (x1 > hi) hi = x1;
      if (x2 < lo) lo = x2; else if (x2 > hi) hi = x2;
      drawFastHLine(lo, y0, (int16_t)(hi - lo + 1), color);
      return;
    }
    Edge longEdge, shortEdge;
    longEdge.init(x0, y0, x2, y2);
    shortEdge.init(x0, y0, x1, y1);
    startWrite();
    for (int16_t y = y0; y <= y2; ++y) {
      if (y == y1) shortEdge.init(x1, y1, x2, y2);
      int16_t a = longEdge.x, b = shortEdge.x;
      if (a > b) { const int16_t t = a; a = b; b = t; }
      drawFastHLine(a, y, (int16_t)(b - a + 1), color);
      longEdge.step();
      shortEdge.step();
    }
    endWrite();
  }

  /// Draw a generated image. See TinyGFX/Image.h.
  ///
  /// Declared here rather than there so a sketch calls it on the TinyGFX it
  /// already has. The struct types live in Image.h, so this is a template on
  /// the reference type - which costs nothing and keeps Gfx.h from having to
  /// know what an image is.
  template <class Ref>
  void drawImage(const Ref* img, int16_t x, int16_t y) {
    if (img != nullptr && img->ops != nullptr) img->ops->draw(*this, img->image, x, y);
  }

  // ---- 1bpp bitmaps ----------------------------------------------------
  /// Draw a 1bpp bitmap - an icon, a logo, a sprite sheet cell.
  ///
  /// Bits are MSB first and **every row starts on a byte boundary**, so a row
  /// is `(w + 7) / 8` bytes. That is the layout every icon converter emits and
  /// the one Adafruit_GFX, U8g2 and LovyanGFX all take.
  ///
  /// A 1 is painted in `color`; a 0 is left alone. To paint the background as
  /// well, fill the rectangle first - it costs less than carrying a second
  /// argument through the loop.
  ///
  /// **The data is read the way font data is** (`tinygfx_rd8`), so on AVR it
  /// must be in PROGMEM. Everywhere else that is a plain dereference and costs
  /// nothing (docs/DECISIONS.ja.md D19).
  ///
  /// ```cpp
  /// static const uint8_t icon[] TINYGFX_FONT_PROGMEM = {
  ///   0x18, 0x24, 0x42, 0x81, 0x81, 0x42, 0x24, 0x18,
  /// };
  /// lcd.drawBitmap(10, 10, icon, 8, 8, TFT_WHITE);
  /// ```
  ///
  /// Runs of set bits become one fillRect each, the same way a glyph is drawn,
  /// so a panel that took the fillRect seam serves this too.
  void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h,
                  uint16_t color) {
    if (bitmap == nullptr || w <= 0 || h <= 0) return;
    const int16_t bytesPerRow = (int16_t)((w + 7) >> 3);
    startWrite();
    for (int16_t r = 0; r < h; ++r) {
      const uint8_t* src = bitmap + (int32_t)r * bytesPerRow;
      int16_t runStart = 0;
      bool cur = ((tinygfx_rd8(src) >> 7) & 1) != 0;
      for (int16_t c = 1; c < w; ++c) {
        const bool on = ((tinygfx_rd8(&src[c >> 3]) >> (7 - (c & 7))) & 1) != 0;
        if (on != cur) {
          if (cur) {
            fillRect((int16_t)(x + runStart), (int16_t)(y + r), (int16_t)(c - runStart), 1, color);
          }
          runStart = c;
          cur = on;
        }
      }
      if (cur) {
        fillRect((int16_t)(x + runStart), (int16_t)(y + r), (int16_t)(w - runStart), 1, color);
      }
    }
    endWrite();
  }

  // ---- images ----------------------------------------------------------
  void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data) {
    if (w <= 0 || h <= 0 || data == nullptr) return;
    int16_t sx = 0, sy = 0;
    // 32 bits for the far edge - see fillRect for why.
    int32_t x1 = (int32_t)x + w - 1, y1 = (int32_t)y + h - 1;
    if (x < _clipX0) { sx = (int16_t)(_clipX0 - x); x = _clipX0; }
    if (y < _clipY0) { sy = (int16_t)(_clipY0 - y); y = _clipY0; }
    if (x1 > _clipX1) x1 = _clipX1;
    if (y1 > _clipY1) y1 = _clipY1;
    if (x > x1 || y > y1) return;
    const int16_t cw = (int16_t)(x1 - x + 1);
    const int16_t ch = (int16_t)(y1 - y + 1);
    startWrite();
    if (cw == w && sx == 0) {  // whole rows survive: push them in one window
      const uint16_t* src = data + (uint32_t)(uint16_t)sy * (uint32_t)(uint16_t)w;
      _panel->setWindow((uint16_t)x, (uint16_t)y, (uint16_t)x1, (uint16_t)y1);
      _panel->writePixels(src, (uint32_t)(uint16_t)cw * (uint32_t)(uint16_t)ch);
    } else {
      const uint16_t* src = data + (uint32_t)(uint16_t)sy * (uint32_t)(uint16_t)w + (uint16_t)sx;
      for (int16_t row = 0; row < ch; ++row) {
        _panel->setWindow((uint16_t)x, (uint16_t)(y + row), (uint16_t)x1, (uint16_t)(y + row));
        _panel->writePixels(src, (uint32_t)(uint16_t)cw);
        src += w;
      }
    }
    endWrite();
  }

  /// The version that skips pixels matching `transparent`, pushing only the
  /// runs in between.
  void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data,
                 uint16_t transparent) {
    if (w <= 0 || h <= 0 || data == nullptr) return;
    startWrite();
    const uint16_t* src = data;
    for (int16_t row = 0; row < h; ++row) {
      int16_t c = 0;
      while (c < w) {
        if (src[c] == transparent) { ++c; continue; }
        int16_t run = 1;
        while (c + run < w && src[c + run] != transparent) ++run;
        pushImage((int16_t)(x + c), (int16_t)(y + row), run, 1, src + c);
        c = (int16_t)(c + run);
      }
      src += w;
    }
    endWrite();
  }

  // ---- text ------------------------------------------------------------
  //
  // The core knows nothing about font formats; the font carries the code that
  // draws it (TinyGFXFontOps). A decoder for a format you do not use is
  // referenced by nothing and so is never linked.

  /// Set the font. Pass the head of the chain.
  void setFont(const TinyGFXFontRef* font) { _font = font; }
  const TinyGFXFontRef* getFont() const { return _font; }
  void setCursor(int16_t x, int16_t y) { _cursorX = x; _cursorY = y; }
  int16_t getCursorX() const { return _cursorX; }
  int16_t getCursorY() const { return _cursorY; }
  void setTextColor(uint16_t fg) { _textFg = fg; _textHasBg = false; }
  void setTextColor(uint16_t fg, uint16_t bg) { _textFg = fg; _textBg = bg; _textHasBg = true; }
  void setTextSize(uint8_t size) { _textSize = size ? size : 1; }

  // drawing state the decoders read
  uint8_t getTextSize() const { return _textSize; }
  uint16_t getTextColor() const { return _textFg; }
  uint16_t getTextBgColor() const { return _textBg; }
  bool hasTextBg() const { return _textHasBg; }

  int16_t fontHeight() const {
    return (int16_t)((uint16_t)getTextLineHeight() * _textSize);
  }

  // Line metrics the decoders read. Always the chain head's, never each
  // font's own: fonts in a chain may differ in height and yOffset, but they
  // share a baseline, so using per-font values would misalign both the line
  // spacing and the background cell.
  uint8_t getTextLineHeight() const {
    return (_font == nullptr) ? 0 : _font->ops->lineHeight(_font->data);
  }
  int16_t getTextAscent() const {
    return (_font == nullptr) ? 0 : _font->ops->ascent(_font->data);
  }

  int16_t textWidth(const char* str) const {
    if (_font == nullptr || str == nullptr) return 0;
    int16_t total = 0;
    while (*str) {
      const int16_t a = advanceOf((uint8_t)*str++);
      if (a > 0) total = (int16_t)(total + a);
    }
    return total;
  }

  /// Draw one glyph. `y` is the top of the line. Returns the advance,
  /// multiplier included.
  ///
  /// An uncovered code falls back to U+FFFD, once, after the decoder has
  /// searched its whole chain (CellFont spec 7.2 and 15.2). The fallback lives
  /// here rather than in the decoder for exactly that reason.
  /// With no notdef either, this returns 0: nothing drawn, pen not advanced.
  int16_t drawChar(uint16_t ch, int16_t x, int16_t y) {
    if (_font == nullptr) return 0;
    const int16_t base = (int16_t)(y + (int16_t)(getTextAscent() * _textSize));
    int16_t a = _font->ops->draw(*this, _font->data, ch, x, base);
    if (a < 0 && ch != TINYGFX_NOTDEF) {
      a = _font->ops->draw(*this, _font->data, TINYGFX_NOTDEF, x, base);
    }
    return (a < 0) ? 0 : (int16_t)(a * _textSize);
  }

  /// Draw a string and return the width drawn. Newlines are not interpreted.
  /// Draw `str` centred on `cx`. Returns the width drawn.
  ///
  /// LovyanGFX offers this twice - as its own call, and as
  /// setTextDatum(TC_DATUM) followed by drawString(). Only the call is here,
  /// and the reason is the price: a datum is state that drawString has to
  /// consult on every call, which drags textWidth() in whether or not anyone
  /// ever centres anything. **Measured on a CH32V003: the datum costs 204
  /// bytes to a sketch that only ever draws text left-aligned. These cost
  /// nothing at all until called** (116 for one, 232 for both), because an
  /// inline member nobody calls is never emitted.
  int16_t drawCenterString(const char* str, int16_t cx, int16_t y) {
    return drawString(str, (int16_t)(cx - textWidth(str) / 2), y);
  }

  /// Draw `str` with its right edge at `rx`. Returns the width drawn.
  int16_t drawRightString(const char* str, int16_t rx, int16_t y) {
    return drawString(str, (int16_t)(rx - textWidth(str)), y);
  }

  int16_t drawString(const char* str, int16_t x, int16_t y) {
    if (str == nullptr) return 0;
    const int16_t x0 = x;
    startWrite();
    while (*str) {
      x = (int16_t)(x + drawChar((uint8_t)*str++, x, y));
    }
    endWrite();
    return (int16_t)(x - x0);
  }

 protected:
  /// The advance, multiplier included. Falls back to U+FFFD, then -1.
  /// It falls back exactly as drawChar does, so textWidth and the width
  /// actually drawn cannot disagree.
  int16_t advanceOf(uint16_t ch) const {
    if (_font == nullptr) return -1;
    int16_t a = _font->ops->advance(_font->data, ch);
    if (a < 0 && ch != TINYGFX_NOTDEF) a = _font->ops->advance(_font->data, TINYGFX_NOTDEF);
    return (a < 0) ? -1 : (int16_t)(a * _textSize);
  }

  static void swap16(int16_t& a, int16_t& b) {
    const int16_t t = a; a = b; b = t;
  }

  /// An edge that steps x once per scanline, without dividing.
  struct Edge {
    int16_t x = 0, dx = 0, dy = 1, sx = 1, err = 0;
    void init(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
      x = x0;
      dx = (int16_t)(x1 > x0 ? x1 - x0 : x0 - x1);
      sx = (int16_t)(x1 > x0 ? 1 : -1);
      dy = (int16_t)(y1 - y0);
      if (dy <= 0) dy = 1;
      err = (int16_t)(dy >> 1);
    }
    void step() {
      err = (int16_t)(err + dx);
      while (err >= dy) { err = (int16_t)(err - dy); x = (int16_t)(x + sx); }
    }
  };

  TinyGFXPanel* _panel;
  const TinyGFXFontRef* _font = nullptr;
  int16_t _clipX0 = 0, _clipY0 = 0, _clipX1 = 0, _clipY1 = 0;
  int16_t _cursorX = 0, _cursorY = 0;
  uint16_t _textFg = 0xFFFF, _textBg = 0x0000;
  uint8_t _rotation = 0, _txn = 0, _textSize = 1;
  bool _textHasBg = false;
};
