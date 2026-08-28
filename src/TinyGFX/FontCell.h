// TinyGFX - CellFont 形式のデコーダ
//
// 形式そのものは TinyGFX の外の仕様（LGFXFontToolJs docs/formats/cellfont.ja.md）。
// ここにあるのは**その仕様の描画器側**だけ。構造体とアクセサは <CellFont.h>。
//
// **H≤16 のピクセルグリッドフォントで最良。** それより大きくても、収録字数が
// 少なければ総量では勝つ（仕様 §13.4。TinyGFX のデコーダ追加コストは実測 693 B）。
//
// 決めることは 3 つ、どれもポインタが nullptr かどうかで決まる:
//   codes  == nullptr  連続索引 / 非 nullptr  疎索引（頭ブロック + 二分探索）
//   glyphs == nullptr  固定ピッチ / 非 nullptr  可変ピッチ（4 バイト/字）
//   next   == nullptr  終端     / 非 nullptr  同じ形式の次のフォントを見る
//
// **U+FFFD への退避はここではやらない**（仕様 §15.2）。TinyGFX は形式をまたぐ連鎖
// （TinyGFXFontRef::next）を外側に持つので、ここで退避すると後段の別形式フォントに
// 載っている字へ到達できなくなる。退避は TinyGFX::drawChar が最外で行う。
#pragma once
#include <stdint.h>

#include <CellFont.h>

#include "Font.h"
#include "Gfx.h"

#if CELLFONT_SPEC_VERSION != 1
#error "TinyGFX implements CellFont spec version 1"
#endif

// 使う変種が決まっているなら、要らない側の分岐を落とせる。
#ifndef TINYGFX_FONT_SPARSE
#define TINYGFX_FONT_SPARSE 1  // 0 にすると疎索引（コード表・頭ブロック）を落とす
#endif
#ifndef TINYGFX_FONT_RECORDS
#define TINYGFX_FONT_RECORDS 1  // 0 にすると可変ピッチ（グリフ表）を落とす
#endif

namespace tinygfx_cell {

/// フォント 1 本を引く。見つかれば true。仕様 §7.1。
///
/// **16bit 環境でも壊れないよう、加算ではなく減算で書く。**
/// `first + count` や `lo + hi` は int が 16bit だと折り返す。
inline bool indexIn(const CellFont* f, uint16_t ch, uint16_t* outIndex) {
  const uint16_t first = CELLFONT_READ_U16(&f->first);
  const uint16_t count = CELLFONT_READ_U16(&f->count);
#if TINYGFX_FONT_SPARSE
  const uint16_t* codes = (const uint16_t*)CELLFONT_READ_PTR(&f->codes);
  if (codes != nullptr) {
    const uint8_t head = CELLFONT_READ_U8(&f->headCount);
    if (head != 0 && ch >= first) {  // 頭ブロック（連続。コード表に載らない）
      const uint16_t rel = (uint16_t)(ch - first);
      if (rel < head) {
        *outIndex = rel;
        return true;
      }
    }
    // しっぽ。**first より小さいコードが混ざりうる**ので ch < first で打ち切れない
    uint16_t lo = 0, hi = (uint16_t)(count - head);
    while (lo < hi) {
      const uint16_t mid = (uint16_t)(lo + ((hi - lo) >> 1));
      const uint16_t v = CELLFONT_READ_U16(&codes[mid]);
      if (v == ch) {
        *outIndex = (uint16_t)(head + mid);
        return true;
      }
      if (v < ch) lo = (uint16_t)(mid + 1);
      else hi = mid;
    }
    return false;
  }
#endif
  if (ch < first) return false;  // 連続
  const uint16_t rel = (uint16_t)(ch - first);
  if (rel >= count) return false;
  *outIndex = rel;
  return true;
}

/// 同じ形式の連鎖を辿る。見つかったフォントを返す。無ければ nullptr。
inline const CellFont* findIn(const CellFont* f, uint16_t ch, uint16_t* outIndex) {
  while (f != nullptr) {
    if (indexIn(f, ch, outIndex)) return f;
    f = (const CellFont*)CELLFONT_READ_PTR(&f->next);
  }
  return nullptr;
}

inline int16_t advance(const void* font, uint16_t ch) {
  uint16_t idx = 0;
  const CellFont* f = findIn((const CellFont*)font, ch, &idx);
  if (f == nullptr) return -1;
#if TINYGFX_FONT_RECORDS
  const CellGlyph* gp = (const CellGlyph*)CELLFONT_READ_PTR(&f->glyphs);
  if (gp != nullptr) return (int16_t)CELLFONT_READ_U8(&gp[idx].xAdvance);
#endif
  return (int16_t)CELLFONT_READ_U8(&f->xAdvance);
}

/// 行送り。連鎖全体で一致している前提（仕様 §8）なので先頭のものでよい。
inline uint8_t lineHeight(const void* font) {
  return CELLFONT_READ_U8(&((const CellFont*)font)->yAdvance);
}

/// ベースラインから箱の上端まで。仕様 §4 の ascent = -yOffset。
inline int16_t ascent(const void* font) {
  return (int16_t)(-(int8_t)CELLFONT_READ_U8(&((const CellFont*)font)->yOffset));
}

/// 1 文字描く。**y はベースライン。** 戻り値は送り幅（倍率をかける前）。収録外は -1。
inline int16_t draw(TinyGFX& g, const void* font, uint16_t ch, int16_t x, int16_t y) {
  uint16_t idx = 0;
  const CellFont* f = findIn((const CellFont*)font, ch, &idx);
  if (f == nullptr) return -1;

  const uint8_t sz = g.getTextSize();
  uint32_t bmOffset;
  uint8_t gw, adv;
#if TINYGFX_FONT_RECORDS
  const CellGlyph* gp = (const CellGlyph*)CELLFONT_READ_PTR(&f->glyphs);
  if (gp != nullptr) {  // 可変ピッチ: グリフ表から引く
    bmOffset = CELLFONT_READ_U16(&gp[idx].offset);
    gw = CELLFONT_READ_U8(&gp[idx].width);
    adv = CELLFONT_READ_U8(&gp[idx].xAdvance);
  } else
#endif
  {  // 固定ピッチ: 表を引かない。**32bit で計算する**（仕様 §15.2）
    bmOffset = (uint32_t)idx * (uint32_t)CELLFONT_READ_U8(&f->bytesPerGlyph);
    gw = CELLFONT_READ_U8(&f->width);
    adv = CELLFONT_READ_U8(&f->xAdvance);
  }

  g.startWrite();
  if (g.hasTextBg()) {
    // 行の箱は**連鎖先頭のメトリクス**で決める。フォントごとの高さで塗ると段がずれる
    const int16_t top = (int16_t)(y - (int16_t)(g.getTextAscent() * sz));
    g.fillRect(x, top, (int16_t)((uint16_t)adv * sz),
               (int16_t)((uint16_t)g.getTextLineHeight() * sz), g.getTextBgColor());
  }
  const uint8_t gh = CELLFONT_READ_U8(&f->height);
  const uint8_t* bm = (const uint8_t*)CELLFONT_READ_PTR(&f->bitmap);
  if (gw != 0 && gh != 0 && bm != nullptr) {
    const uint16_t fg = g.getTextColor();
    const uint8_t* src = bm + bmOffset;
    const int16_t gx = (int16_t)(x + (int16_t)((int8_t)CELLFONT_READ_U8(&f->xOffset) * sz));
    int16_t py = (int16_t)(y + (int16_t)((int8_t)CELLFONT_READ_U8(&f->yOffset) * sz));
    // 行を連結した MSB first のビット列。行の途中ではバイト境界に揃わない
    uint32_t bit = 0;
    for (uint8_t r = 0; r < gh; ++r) {
      int16_t px = gx;
      uint8_t runStart = 0;
      bool cur = false;
      for (uint8_t c = 0; c < gw; ++c) {
        const bool on = ((CELLFONT_READ_U8(&src[bit >> 3]) >> (7 - (bit & 7))) & 1) != 0;
        ++bit;
        if (c == 0) {
          cur = on;
          continue;
        }
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

}  // namespace tinygfx_cell

/// この形式の入口。生成したフォントヘッダが TinyGFXFontRef からここを指す。
static const TinyGFXFontOps tinygfxFontCellOps = {
    &tinygfx_cell::draw,
    &tinygfx_cell::advance,
    &tinygfx_cell::lineHeight,
    &tinygfx_cell::ascent,
};
