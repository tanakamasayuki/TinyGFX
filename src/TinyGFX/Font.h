// TinyGFX - font description (Adafruit GFXfont compatible)
//
// LGFXFontToolJs が出力する GFXfont 形式をそのまま食える形にしている。
// 既存の Adafruit GFX 用フォントヘッダも同じ型でそのまま使える。
//
// ビットマップは「行を連結した MSB first のビット列、グリフ間はバイト境界揃え」。
// TinyGFX 自身はフォントデータを 1 つも同梱しない（docs/DECISIONS.ja.md D17）。
#pragma once
#include <stdint.h>

// Adafruit gfxfont.h と同じガード名。先に include されていればそちらを使う。
#ifndef _GFXFONT_H_
#define _GFXFONT_H_

typedef struct {
  uint16_t bitmapOffset;  // bitmap 先頭からのバイトオフセット
  uint8_t width;          // グリフの幅（画素）
  uint8_t height;         // グリフの高さ（画素）
  uint8_t xAdvance;       // 送り幅
  int8_t xOffset;         // 描画開始位置（左）
  int8_t yOffset;         // 描画開始位置（ベースラインから上が負）
} GFXglyph;

typedef struct {
  uint8_t* bitmap;
  GFXglyph* glyph;
  uint16_t first;
  uint16_t last;
  uint8_t yAdvance;  // 行送り
} GFXfont;

#endif  // _GFXFONT_H_

typedef GFXglyph TinyGFXGlyph;
typedef GFXfont TinyGFXFont;
