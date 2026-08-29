// TinyGFX - images
//
// **形式は 1 つではない。** 画像は絵によって最小の符号化が変わるので、
// 変換ツールが総当たりして選び、ここはその描画器になる。
// docs/IMAGE_FORMAT.ja.md に実測と判断の経緯がある。
//
// **どの形式を使うかを利用者が宣言する必要はない。** 生成された .h が
// 自分の ops を指すので、**使う形式のデコーダだけがリンクされる**
// （このヘッダが全部 include していても、未使用は 0 バイト。実測済み）。
// フォントの TinyGFXFontOps と同じ機構。
//
//   #include <TinyGFX/Image.h>
//   #include "my_icon.h"          // 生成物。自分で ops を指す
//   lcd.drawImage(&myIconRef, 10, 10);
//
// 形式の選び方は変換ツールの仕事で、利用者は考えなくていい。ただし
// **画像を何枚も使うならまとめて変換すること** - デコーダ代は形式ごとに
// 1 回なので、1 枚ずつ最小を選ぶと形式が散らばって損をする。
#pragma once
#include <stdint.h>

#include "Font.h"   // tinygfx_rd8 / tinygfx_rd16 / TINYGFX_FONT_PROGMEM
#include "Gfx.h"

/// 生成ヘッダはこれを見て、描画器が先に include されているか確かめる。
#define TINYGFX_IMAGE_SPEC_VERSION 1
#define TINYGFX_IMAGE_PROGMEM TINYGFX_FONT_PROGMEM

/// 1 枚の画像。**形式はここに書かない** - ops ポインタが表す。
///
/// CellFont が「連続／疎」「固定／可変」をポインタが null かどうかで表して
/// いるのと同じ考え方。形式タグを持って実行時に分岐すると、使わない分岐まで
/// リンクされてしまう。
struct CellImage {
  const void* data;         ///< 画素データ
  const uint16_t* palette;  ///< NULL なら直接色
  uint16_t width, height;
  uint16_t dataLen;         ///< data のバイト数。RLE の終端判定に要る
  /// 透過。直接色なら色、パレットなら索引。`hasTransparent` が 0 なら無視。
  ///
  /// **形式と違って、これは実行時に見る。** 形式は ops ポインタで表して
  /// 使わないデコーダを落としているのに透過だけ実行時なのは、実測すると
  /// 差が付かなかったから（CH32V003、rlepal4）:
  ///
  ///   |                | 透過なしのみ | 透過ありのみ | 両方 |
  ///   | 実行時に見る    |      576 |      576 |  784 |
  ///   | ops で分ける    |      544 |      560 |  796 |
  ///
  /// **どの使い方でも 32 バイト以内。** 均一なら分割が僅かに得、混在なら
  /// 実行時が僅かに得。差が小さいので、デコーダが 1 本で済むほうを採った。
  ///
  /// 判定そのものの実費は形式と MCU による: 直接色 +32〜66、RLE +24〜54。
  /// **1bpp は 0 が元から透過**なので、この値を見ない。
  uint16_t transparent;
  uint8_t paletteCount;
  uint8_t hasTransparent;
};

/// 1 形式ぶんの入口。フォントの TinyGFXFontOps と同じ形。
struct TinyGFXImageOps {
  void (*draw)(TinyGFX& gfx, const CellImage* img, int16_t x, int16_t y);
};

/// スケッチが drawImage() に渡すもの。
struct TinyGFXImageRef {
  const CellImage* image;
  const TinyGFXImageOps* ops;
};

namespace tinygfx_image {

/// 画像のヘッダを読む。PROGMEM 越しなので毎回読み直さずまとめて取る。
struct Head {
  const uint8_t* data;
  const uint16_t* palette;
  int16_t w, h;
  uint16_t len, transparent;
  bool hasTransparent;
};

inline Head head(const CellImage* im) {
  Head d;
  d.data = (const uint8_t*)tinygfx_rdptr(&im->data);
  d.palette = (const uint16_t*)tinygfx_rdptr(&im->palette);
  d.w = (int16_t)tinygfx_rd16(&im->width);
  d.h = (int16_t)tinygfx_rd16(&im->height);
  d.len = tinygfx_rd16(&im->dataLen);
  d.transparent = tinygfx_rd16(&im->transparent);
  d.hasTransparent = tinygfx_rd8(&im->hasTransparent) != 0;
  return d;
}

/// RLE の走査位置。**行と列を持ち回る。**
///
/// 通し位置から `pos / w` で行を出すほうが素直だが、**除算が高い。**
/// CH32V003 は rv32ec で除算命令が無く、AVR は 8 ビットなので 32 ビットの
/// 除算がソフトウェアルーチンになる。行をまたぐたびに 1 つ足すだけで
/// 済むので、除算は要らない。
struct Cursor {
  int16_t row, col;
};

/// 行をまたがない範囲で塗り、カーソルを進める。RLE 系が共有する。
inline void emitRun(TinyGFX& g, int16_t x, int16_t y, int16_t w, int16_t h,
                    Cursor& c, int16_t len, uint16_t color, bool skip) {
  while (len > 0 && c.row < h) {
    int16_t run = (int16_t)(w - c.col);
    if (run > len) run = len;
    if (!skip) {
      g.fillRect((int16_t)(x + c.col), (int16_t)(y + c.row), run, 1, color);
    }
    c.col = (int16_t)(c.col + run);
    if (c.col >= w) { c.col = 0; ++c.row; }
    len = (int16_t)(len - run);
  }
}

// ---- 1bpp ---------------------------------------------------------------
//
// 横詰めと縦詰てはビットの取り出し方が 1 行違うだけなので 1 本にまとめる。
// **向きは呼び出し側で定数**（生成ヘッダが形式を知っている）なので、
// コンパイラが特殊化して使わない側を消す。実測: 片方だけ使えば片方ぶんの
// 代金しか出ず、別関数 2 本にするより 156 バイト小さい。
inline void drawBitmap1(TinyGFX& g, const CellImage* im, int16_t x, int16_t y, bool vert) {
  const Head d = head(im);
  if (d.data == nullptr || d.w <= 0 || d.h <= 0) return;
  const uint16_t fg = (d.palette != nullptr) ? tinygfx_rd16(&d.palette[1]) : 0xFFFF;
  const int16_t bpr = (int16_t)((d.w + 7) >> 3);
  g.startWrite();
  for (int16_t r = 0; r < d.h; ++r) {
    const uint8_t* s = vert ? (d.data + (int32_t)(r >> 3) * d.w)
                            : (d.data + (int32_t)r * bpr);
    const uint8_t sh = (uint8_t)(r & 7);
    int16_t runStart = 0;
    bool cur = vert ? (((tinygfx_rd8(s) >> sh) & 1) != 0)
                    : (((tinygfx_rd8(s) >> 7) & 1) != 0);
    for (int16_t c = 1; c < d.w; ++c) {
      const bool on = vert ? (((tinygfx_rd8(&s[c]) >> sh) & 1) != 0)
                           : (((tinygfx_rd8(&s[c >> 3]) >> (7 - (c & 7))) & 1) != 0);
      if (on != cur) {
        if (cur) {
          g.fillRect((int16_t)(x + runStart), (int16_t)(y + r),
                     (int16_t)(c - runStart), 1, fg);
        }
        runStart = c;
        cur = on;
      }
    }
    if (cur) {
      g.fillRect((int16_t)(x + runStart), (int16_t)(y + r),
                 (int16_t)(d.w - runStart), 1, fg);
    }
  }
  g.endWrite();
}
inline void drawBitmap1H(TinyGFX& g, const CellImage* im, int16_t x, int16_t y) {
  drawBitmap1(g, im, x, y, false);
}
inline void drawBitmap1V(TinyGFX& g, const CellImage* im, int16_t x, int16_t y) {
  drawBitmap1(g, im, x, y, true);
}

// ---- RLE ----------------------------------------------------------------

/// 長さ 1 バイト + 色 2 バイト。色数が多くて面がフラットな絵向け。
inline void drawRle565(TinyGFX& g, const CellImage* im, int16_t x, int16_t y) {
  const Head d = head(im);
  if (d.data == nullptr || d.w <= 0 || d.h <= 0) return;
  Cursor c = {0, 0};
  g.startWrite();
  for (uint16_t i = 0; i + 2 < d.len && c.row < d.h; i += 3) {
    const uint16_t col = (uint16_t)((tinygfx_rd8(&d.data[i + 1]) << 8) |
                                    tinygfx_rd8(&d.data[i + 2]));
    const bool skip = d.hasTransparent && col == d.transparent;
    emitRun(g, x, y, d.w, d.h, c, tinygfx_rd8(&d.data[i]), col, skip);
  }
  g.endWrite();
}

/// 1 バイトに 長さ(上位 4bit、1..16) + パレット索引(下位 4bit)。
/// **UI アートではこれが最小になることが多い。**
inline void drawRlePal4(TinyGFX& g, const CellImage* im, int16_t x, int16_t y) {
  const Head d = head(im);
  if (d.data == nullptr || d.palette == nullptr || d.w <= 0 || d.h <= 0) return;
  Cursor c = {0, 0};
  g.startWrite();
  for (uint16_t i = 0; i < d.len && c.row < d.h; ++i) {
    const uint8_t b = tinygfx_rd8(&d.data[i]);
    const uint8_t idx = (uint8_t)(b & 0x0F);
    const bool skip = d.hasTransparent && idx == (uint8_t)d.transparent;
    emitRun(g, x, y, d.w, d.h, c, (int16_t)((b >> 4) + 1),
            tinygfx_rd16(&d.palette[idx]), skip);
  }
  g.endWrite();
}

// ---- 生 RGB565 -----------------------------------------------------------

/// 写真など、圧縮が効かない絵向け。**基準機では 64x64 で 8KB なので現実的
/// ではない** - ESP32 のような余裕のある環境用。
inline void drawRaw565(TinyGFX& g, const CellImage* im, int16_t x, int16_t y) {
  const Head d = head(im);
  if (d.data == nullptr || d.w <= 0 || d.h <= 0) return;
  g.startWrite();
  for (int16_t r = 0; r < d.h; ++r) {
    const uint8_t* s = d.data + (int32_t)r * d.w * 2;
    int16_t runStart = 0;
    uint16_t cur = (uint16_t)((tinygfx_rd8(s) << 8) | tinygfx_rd8(&s[1]));
    for (int16_t c = 1; c < d.w; ++c) {
      const uint16_t col = (uint16_t)((tinygfx_rd8(&s[c * 2]) << 8) |
                                      tinygfx_rd8(&s[c * 2 + 1]));
      if (col != cur) {
        if (!(d.hasTransparent && cur == d.transparent)) {
          g.fillRect((int16_t)(x + runStart), (int16_t)(y + r),
                     (int16_t)(c - runStart), 1, cur);
        }
        runStart = c;
        cur = col;
      }
    }
    if (!(d.hasTransparent && cur == d.transparent)) {
      g.fillRect((int16_t)(x + runStart), (int16_t)(y + r),
                 (int16_t)(d.w - runStart), 1, cur);
    }
  }
  g.endWrite();
}

}  // namespace tinygfx_image

// 生成ヘッダはこのどれかを指す。**指されたものだけがリンクされる。**
static const TinyGFXImageOps tinygfxImageBitmap1hOps = {&tinygfx_image::drawBitmap1H};
static const TinyGFXImageOps tinygfxImageBitmap1vOps = {&tinygfx_image::drawBitmap1V};
static const TinyGFXImageOps tinygfxImageRle565Ops = {&tinygfx_image::drawRle565};
static const TinyGFXImageOps tinygfxImageRlepal4Ops = {&tinygfx_image::drawRlePal4};
static const TinyGFXImageOps tinygfxImageRaw565Ops = {&tinygfx_image::drawRaw565};
