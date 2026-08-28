// TinyGFX - TinyFont: 高さ 16 画素以下のピクセルグリッドフォント向けの形式
//
// **対象は H≤16。** それより大きいと bbox / RLE / u8g2 が同時に有利へ反転するので、
// そちらは u8g2 形式（FontU8g2.h）を使うほうが小さい。根拠は docs/FONT_FORMAT.ja.md §0。
//
// 設計の要点は「選択を実行時ではなく**生成時**に済ませ、結果をデータで表す」こと。
//
//   索引     連続（first からの通し）か、疎（昇順のコード表）か
//   レコード 持つ（可変ピッチ）か、持たない（固定ピッチ）か
//
// どちらもポインタが nullptr かどうかで決まる。
//
// ビットマップは「行を連結した MSB first のビット列、グリフ間はバイト境界揃え」。
#pragma once
#include <stdint.h>

#include "Font.h"
#include "Gfx.h"

// 使う変種が決まっているなら、要らない側の分岐を落とせる。
#ifndef TINYGFX_FONT_SPARSE
#define TINYGFX_FONT_SPARSE 1  // 0 にすると疎索引（コード表）を落とす
#endif
#ifndef TINYGFX_FONT_RECORDS
#define TINYGFX_FONT_RECORDS 1  // 0 にすると可変ピッチ（グリフ表）を落とす
#endif

/// 可変ピッチのときのグリフ 1 件。**4 バイト固定。**
/// 高さ・xOffset・yOffset はフォント全体で共通なので持たない。
struct TinyGFXGlyph {
  uint8_t offsetLo;
  uint8_t offsetHi;
  uint8_t width;
  uint8_t xAdvance;
};

struct TinyGFXFontTiny {
  const uint8_t* bitmap;
  const TinyGFXGlyph* glyphs;  // nullptr = 固定ピッチ
  const uint16_t* codes;       // nullptr = 連続索引
  uint16_t first;
  uint16_t count;
  uint8_t width;          // 固定ピッチのときのグリフ幅
  uint8_t height;         // 全グリフ共通
  uint8_t xAdvance;       // 固定ピッチのときの送り幅
  uint8_t yAdvance;       // 行送り
  int8_t xOffset;         // 全グリフ共通
  int8_t yOffset;         // 行の上端からのグリフ上端（下が正）
  uint8_t bytesPerGlyph;  // 固定ピッチのとき。実行時に除算しないため持つ
};

namespace tinygfx_tiny {

/// 文字コード -> グリフ番号。収録外は -1。
inline int32_t indexOf(const TinyGFXFontTiny* f, uint16_t ch) {
  const uint16_t n = tinygfx_rd16(&f->count);
#if TINYGFX_FONT_SPARSE
  const uint16_t* codes = (const uint16_t*)tinygfx_rdptr(&f->codes);
  if (codes != nullptr) {  // 疎（昇順のコード表を二分探索）
    uint16_t lo = 0, hi = n;
    while (lo < hi) {
      const uint16_t mid = (uint16_t)((lo + hi) >> 1);
      const uint16_t v = tinygfx_rd16(&codes[mid]);
      if (v == ch) return (int32_t)mid;
      if (v < ch) lo = (uint16_t)(mid + 1);
      else hi = mid;
    }
    return -1;
  }
#endif
  const uint16_t first = tinygfx_rd16(&f->first);  // 連続
  if (ch < first) return -1;
  const uint16_t off = (uint16_t)(ch - first);
  return (off < n) ? (int32_t)off : -1;
}

inline int16_t advance(const void* font, uint16_t ch) {
  const TinyGFXFontTiny* f = (const TinyGFXFontTiny*)font;
  const int32_t idx = indexOf(f, ch);
  if (idx < 0) return -1;
#if TINYGFX_FONT_RECORDS
  const TinyGFXGlyph* gp = (const TinyGFXGlyph*)tinygfx_rdptr(&f->glyphs);
  if (gp != nullptr) return (int16_t)tinygfx_rd8(&gp[idx].xAdvance);
#endif
  return (int16_t)tinygfx_rd8(&f->xAdvance);
}

inline uint8_t lineHeight(const void* font) {
  return tinygfx_rd8(&((const TinyGFXFontTiny*)font)->yAdvance);
}

inline int16_t draw(TinyGFX& g, const void* font, uint16_t ch, int16_t x, int16_t y) {
  const TinyGFXFontTiny* f = (const TinyGFXFontTiny*)font;
  const int32_t idx = indexOf(f, ch);
  if (idx < 0) return -1;

  const uint8_t sz = g.getTextSize();
  uint16_t bmOffset;
  uint8_t gw, adv;
#if TINYGFX_FONT_RECORDS
  const TinyGFXGlyph* gp = (const TinyGFXGlyph*)tinygfx_rdptr(&f->glyphs);
  if (gp != nullptr) {  // 可変ピッチ: グリフ表から引く
    const TinyGFXGlyph* gl = &gp[idx];
    bmOffset = (uint16_t)(tinygfx_rd8(&gl->offsetLo) | ((uint16_t)tinygfx_rd8(&gl->offsetHi) << 8));
    gw = tinygfx_rd8(&gl->width);
    adv = tinygfx_rd8(&gl->xAdvance);
  } else
#endif
  {  // 固定ピッチ: グリフ表を持たない
    bmOffset = (uint16_t)((uint16_t)idx * tinygfx_rd8(&f->bytesPerGlyph));
    gw = tinygfx_rd8(&f->width);
    adv = tinygfx_rd8(&f->xAdvance);
  }

  g.startWrite();
  if (g.hasTextBg()) {  // セル全体を背景で塗ってから前景だけ描く
    g.fillRect(x, y, (int16_t)((uint16_t)adv * sz),
               (int16_t)((uint16_t)tinygfx_rd8(&f->yAdvance) * sz), g.getTextBgColor());
  }
  const uint8_t gh = tinygfx_rd8(&f->height);
  const uint8_t* bm = (const uint8_t*)tinygfx_rdptr(&f->bitmap);
  if (gw != 0 && gh != 0 && bm != nullptr) {
    const uint16_t fg = g.getTextColor();
    const uint8_t* src = bm + bmOffset;
    const int16_t gx = (int16_t)(x + (int16_t)((int8_t)tinygfx_rd8(&f->xOffset) * sz));
    int16_t py = (int16_t)(y + (int16_t)((int8_t)tinygfx_rd8(&f->yOffset) * sz));
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
          if (cur) g.fillRect(px, py, runW, sz, fg);
          px = (int16_t)(px + runW);
          runStart = c;
          cur = on;
        }
      }
      if (cur) g.fillRect(px, py, (int16_t)((uint16_t)(gw - runStart) * sz), sz, fg);
      py = (int16_t)(py + sz);
    }
  }
  g.endWrite();
  return (int16_t)adv;
}

}  // namespace tinygfx_tiny

/// この形式の入口。生成したフォントヘッダが TinyGFXFontRef からここを指す。
static const TinyGFXFontOps tinygfxFontTinyOps = {
    &tinygfx_tiny::draw,
    &tinygfx_tiny::advance,
    &tinygfx_tiny::lineHeight,
};
