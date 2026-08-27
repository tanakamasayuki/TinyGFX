// TinyGFX - drawing core
//
// このクラスには virtual を 1 つも置かない（docs/DECISIONS.ja.md D1）。
// すべてヘッダ内 inline なので、呼ばれないメソッドはそもそも生成されない。
// コアは <Arduino.h> も Serial も参照しない（docs/CORE_DESIGN.ja.md §7.4 R4）。
// 除算・剰余を使わない（CH32V003 は rv32ec で除算命令を持たない）。
#pragma once
#include <stdint.h>

#include "Color.h"
#include "Font.h"
#include "Panel.h"

class TinyGFX {
 public:
  explicit TinyGFX(TinyGFXPanel& panel) : _panel(&panel) {}

  // ---- 基本 ------------------------------------------------------------
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

  // ---- 転送制御 --------------------------------------------------------
  void startWrite() {
    if (_txn++ == 0) _panel->beginTransaction();
  }
  void endWrite() {
    if (_txn != 0 && --_txn == 0) _panel->endTransaction();
  }

  // ---- クリップ --------------------------------------------------------
  void setClipRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    int16_t x1 = (int16_t)(x + w - 1);
    int16_t y1 = (int16_t)(y + h - 1);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    const int16_t mx = (int16_t)(width() - 1);
    const int16_t my = (int16_t)(height() - 1);
    if (x1 > mx) x1 = mx;
    if (y1 > my) y1 = my;
    _clipX0 = x; _clipY0 = y; _clipX1 = x1; _clipY1 = y1;
  }
  void resetClipRect() {
    _clipX0 = 0; _clipY0 = 0;
    _clipX1 = (int16_t)(width() - 1);
    _clipY1 = (int16_t)(height() - 1);
  }
  void clearClipRect() { resetClipRect(); }

  // ---- 低レベル転送 ----------------------------------------------------
  void setAddrWindow(int16_t x, int16_t y, int16_t w, int16_t h) {
    _panel->setWindow((uint16_t)x, (uint16_t)y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
  }
  void writeColor(uint16_t color, uint32_t count) { _panel->writeColor(color, count); }
  void writePixels(const uint16_t* data, uint32_t count) { _panel->writePixels(data, count); }

  // ---- プリミティブ ----------------------------------------------------
  void drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < _clipX0 || x > _clipX1 || y < _clipY0 || y > _clipY1) return;
    startWrite();
    _panel->setWindow((uint16_t)x, (uint16_t)y, (uint16_t)x, (uint16_t)y);
    _panel->writeColor(color, 1);
    endWrite();
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    int16_t x1 = (int16_t)(x + w - 1);
    int16_t y1 = (int16_t)(y + h - 1);
    if (x < _clipX0) x = _clipX0;
    if (y < _clipY0) y = _clipY0;
    if (x1 > _clipX1) x1 = _clipX1;
    if (y1 > _clipY1) y1 = _clipY1;
    if (x > x1 || y > y1) return;
    const uint32_t n = (uint32_t)(uint16_t)(x1 - x + 1) * (uint32_t)(uint16_t)(y1 - y + 1);
    startWrite();
    _panel->setWindow((uint16_t)x, (uint16_t)y, (uint16_t)x1, (uint16_t)y1);
    _panel->writeColor(color, n);
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

  /// 除算なしの三角形塗り。辺を Bresenham で走らせて走査線ごとに水平線を引く。
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                    uint16_t color) {
    // y で昇順に並べ替え
    if (y0 > y1) { swap16(x0, x1); swap16(y0, y1); }
    if (y1 > y2) { swap16(x1, x2); swap16(y1, y2); }
    if (y0 > y1) { swap16(x0, x1); swap16(y0, y1); }
    if (y0 == y2) {  // 退化: 水平線
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

  // ---- 画像 ------------------------------------------------------------
  void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data) {
    if (w <= 0 || h <= 0 || data == nullptr) return;
    int16_t sx = 0, sy = 0;
    int16_t x1 = (int16_t)(x + w - 1), y1 = (int16_t)(y + h - 1);
    if (x < _clipX0) { sx = (int16_t)(_clipX0 - x); x = _clipX0; }
    if (y < _clipY0) { sy = (int16_t)(_clipY0 - y); y = _clipY0; }
    if (x1 > _clipX1) x1 = _clipX1;
    if (y1 > _clipY1) y1 = _clipY1;
    if (x > x1 || y > y1) return;
    const int16_t cw = (int16_t)(x1 - x + 1);
    const int16_t ch = (int16_t)(y1 - y + 1);
    startWrite();
    if (cw == w && sx == 0) {  // 行が丸ごと入る: 1 回のウィンドウで流し込む
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

  /// transparent と一致する画素を飛ばす版。連続するランだけ転送する。
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

  // ---- 文字 ------------------------------------------------------------
  /// GFXfont（Adafruit / LGFXFontToolJs 出力）を設定する。
  /// ascent はここで一度だけ求める。描画のたびに全グリフを走査しないため。
  void setFont(const TinyGFXFont* font) {
    _font = font;
    _ascent = 0;
    if (font == nullptr) return;
    const TinyGFXGlyph* g = (const TinyGFXGlyph*)tinygfx_rdptr(&font->glyph);
    if (g == nullptr) return;
    const uint16_t n = (uint16_t)(tinygfx_rd16(&font->last) - tinygfx_rd16(&font->first) + 1);
    int8_t minY = 0;
    for (uint16_t i = 0; i < n; ++i) {
      const int8_t yo = (int8_t)tinygfx_rd8(&g[i].yOffset);
      if (yo < minY) minY = yo;
    }
    _ascent = (int8_t)(-minY);
  }
  const TinyGFXFont* getFont() const { return _font; }
  void setCursor(int16_t x, int16_t y) { _cursorX = x; _cursorY = y; }
  int16_t getCursorX() const { return _cursorX; }
  int16_t getCursorY() const { return _cursorY; }
  void setTextColor(uint16_t fg) { _textFg = fg; _textHasBg = false; }
  void setTextColor(uint16_t fg, uint16_t bg) { _textFg = fg; _textBg = bg; _textHasBg = true; }
  void setTextSize(uint8_t size) { _textSize = size ? size : 1; }
  uint8_t getTextSize() const { return _textSize; }

  int16_t fontHeight() const {
    if (_font == nullptr) return 0;
    return (int16_t)((uint16_t)tinygfx_rd8(&_font->yAdvance) * _textSize);
  }
  int16_t textWidth(const char* str) const {
    if (_font == nullptr || str == nullptr) return 0;
    const TinyGFXGlyph* g = (const TinyGFXGlyph*)tinygfx_rdptr(&_font->glyph);
    const uint16_t first = tinygfx_rd16(&_font->first);
    const uint16_t last = tinygfx_rd16(&_font->last);
    int16_t total = 0;
    while (*str) {
      const uint8_t c = (uint8_t)*str++;
      if (c < first || c > last) continue;
      const uint16_t adv = (uint16_t)tinygfx_rd8(&g[c - first].xAdvance) * _textSize;
      total = (int16_t)(total + (int16_t)adv);
    }
    return total;
  }

  /// 1 文字描く。y は行の上端（LovyanGFX 流。Adafruit のベースライン基準ではない）。
  /// 戻り値は送り幅。
  int16_t drawChar(uint16_t ch, int16_t x, int16_t y) {
    const TinyGFXFont* f = _font;
    if (f == nullptr) return 0;
    const uint16_t first = tinygfx_rd16(&f->first);
    if (ch < first || ch > tinygfx_rd16(&f->last)) return 0;
    const TinyGFXGlyph* gp = (const TinyGFXGlyph*)tinygfx_rdptr(&f->glyph);
    const uint8_t* bm = (const uint8_t*)tinygfx_rdptr(&f->bitmap);
    if (gp == nullptr || bm == nullptr) return 0;
    const TinyGFXGlyph* g = &gp[ch - first];

    const uint8_t sz = _textSize;
    const int16_t adv = (int16_t)((uint16_t)tinygfx_rd8(&g->xAdvance) * sz);

    startWrite();
    if (_textHasBg) {  // セル全体を背景で塗ってから前景だけ描く
      fillRect(x, y, adv, (int16_t)((uint16_t)tinygfx_rd8(&f->yAdvance) * sz), _textBg);
    }
    const uint8_t gw = tinygfx_rd8(&g->width);
    const uint8_t gh = tinygfx_rd8(&g->height);
    if (gw != 0 && gh != 0) {
      const uint8_t* src = bm + tinygfx_rd16(&g->bitmapOffset);
      const int16_t gx = (int16_t)(x + (int16_t)((int8_t)tinygfx_rd8(&g->xOffset) * sz));
      int16_t py = (int16_t)(y + (int16_t)((int16_t)(_ascent + (int8_t)tinygfx_rd8(&g->yOffset)) * sz));
      uint32_t bit = 0;
      for (uint8_t r = 0; r < gh; ++r) {
        int16_t px = gx;
        uint8_t runStart = 0;
        bool cur = false;
        for (uint8_t c = 0; c < gw; ++c) {
          const bool on = ((tinygfx_rd8(&src[bit >> 3]) >> (7 - (bit & 7))) & 1) != 0;
          ++bit;
          if (c == 0) { cur = on; continue; }
          if (on != cur) {
            const int16_t runW = (int16_t)((uint16_t)(c - runStart) * sz);
            if (cur) fillRect(px, py, runW, sz, _textFg);
            px = (int16_t)(px + runW);
            runStart = c;
            cur = on;
          }
        }
        if (cur) {
          fillRect(px, py, (int16_t)((uint16_t)(gw - runStart) * sz), sz, _textFg);
        }
        py = (int16_t)(py + sz);
      }
    }
    endWrite();
    return adv;
  }

  /// 文字列を描く。戻り値は描いた幅。改行は解釈しない。
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
  static void swap16(int16_t& a, int16_t& b) {
    const int16_t t = a; a = b; b = t;
  }

  /// 走査線ごとに x を進める辺。除算を使わない。
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
  const TinyGFXFont* _font = nullptr;
  int16_t _clipX0 = 0, _clipY0 = 0, _clipX1 = 0, _clipY1 = 0;
  int16_t _cursorX = 0, _cursorY = 0;
  uint16_t _textFg = 0xFFFF, _textBg = 0x0000;
  uint8_t _rotation = 0, _txn = 0, _textSize = 1;
  int8_t _ascent = 0;
  bool _textHasBg = false;
};
