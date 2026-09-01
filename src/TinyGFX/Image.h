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
///
/// **互換番号ではない。値は誰も比べない**（D35）。比べる必要が無いのは、
/// 生成ヘッダが型と記号を名前で書いているので、**食い違いは C++ 側が先に
/// 落とすから**（実測、CH32V003）:
///
///   | 生成ヘッダ側の変化      | コンパイラが言うこと                          |
///   | 知らない形式を使う      | 'tinygfxImageRlepal9Ops' was not declared     |
///   | CellImage に項目が増えた | too many initializers for 'const CellImage'   |
///   | この include を忘れた   | 'CellImage' does not name a type              |
///
/// **番号は「上げ忘れる」が、名前は忘れようがない。** 唯一 C++ に見えないのは
/// 同じ型の項目を入れ替えた場合なので、**そのときは型を改名する**
/// （`CellImage` → `CellImage2`）。上の 3 行目と同じ形の失敗になる。
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
  uint16_t width;
  uint16_t height;
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
  int16_t w;
  int16_t h;
  uint16_t len;
  uint16_t transparent;
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
  int16_t row;
  int16_t col;
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
  const uint8_t palCount = tinygfx_rd8(&im->paletteCount);
  Cursor c = {0, 0};
  g.startWrite();
  for (uint16_t i = 0; i < d.len && c.row < d.h; ++i) {
    const uint8_t b = tinygfx_rd8(&d.data[i]);
    // **索引はパレットの外へ出さない。** 壊れたヘッダや手で書いた
    // データでも、配列の外を読まないようにする。ここを外すと
    // `pal[15]` のような読みがそのまま通ってしまう（実際に踏んだ）。
    uint8_t idx = (uint8_t)(b & 0x0F);
    if (idx >= palCount) idx = 0;
    const bool skip = d.hasTransparent && idx == (uint8_t)d.transparent;
    emitRun(g, x, y, d.w, d.h, c, (int16_t)((b >> 4) + 1),
            tinygfx_rd16(&d.palette[idx]), skip);
  }
  g.endWrite();
}

// ---- 生 RGB565 -----------------------------------------------------------

/// Pixel `c` of a raw565 row. Big endian, read through tinygfx_rd8 so the
/// data may sit in PROGMEM.
inline uint16_t px(const uint8_t* row, int16_t c) {
  return (uint16_t)(((uint16_t)tinygfx_rd8(&row[c * 2]) << 8) | tinygfx_rd8(&row[c * 2 + 1]));
}


/// 写真など、圧縮が効かない絵向け。**基準機では 64x64 で 8KB なので現実的
/// ではない** - ESP32 のような余裕のある環境用。
///
/// This one does not go through fillRect, and that is the whole point.
///
/// Every other decoder here emits runs, and a run through fillRect costs one
/// window (CASET + RASET + RAMWR = 11 bytes) plus the pixels. That is the
/// right trade when runs are long, which is exactly what an RLE format
/// guarantees. raw565 guarantees the opposite: a photograph has runs of one.
/// Measured on a 32x32 of pure noise, the run form sent **3,072 commands for
/// 1,024 pixels** - 13 bytes a pixel to move a 2,048-byte image.
///
/// So this opens one window per span and streams into it. Same picture, and
/// the same 32x32 costs one window a row.
///
/// A span ends where a transparent pixel starts, because a window has to be
/// filled completely once it is open - skipping a pixel inside one would shift
/// everything after it. With no transparent colour the span is the whole row.
///
/// The window path does not clip, so the clipping is done here, against
/// g.clipX0() and friends.
inline void drawRaw565(TinyGFX& g, const CellImage* im, int16_t x, int16_t y) {
  const Head d = head(im);
  if (d.data == nullptr || d.w <= 0 || d.h <= 0) return;
  // The surviving column span. The same for every row, so it is found once.
  //
  // In 32 bits, because clipX0 - x overflows int16_t for coordinates that are
  // still perfectly legal to pass: x = -32768 makes the difference 32768.
  // Taking coordinates from outside the screen and clipping them is part of
  // the contract (see TinyGFX::fillRect). Both are back inside int16_t by the
  // time they are used, because the clip rectangle is.
  int32_t c0 = 0;
  int32_t c1 = (int32_t)d.w - 1;
  if ((int32_t)x + c0 < g.clipX0()) c0 = (int32_t)g.clipX0() - x;
  if ((int32_t)x + c1 > g.clipX1()) c1 = (int32_t)g.clipX1() - x;
  if (c0 > c1 || c1 < 0 || c0 >= (int32_t)d.w) return;
  g.startWrite();
  for (int16_t r = 0; r < d.h; ++r) {
    const int32_t py32 = (int32_t)y + r;
    if (py32 < g.clipY0() || py32 > g.clipY1()) continue;
    const int16_t py = (int16_t)py32;
    const uint8_t* s = d.data + (int32_t)r * d.w * 2;
    int16_t c = (int16_t)c0;
    const int16_t cEnd = (int16_t)c1;
    while (c <= cEnd) {
      uint16_t col = px(s, c);
      if (d.hasTransparent && col == d.transparent) { ++c; continue; }
      // How far the opaque span reaches.
      int16_t e = (int16_t)(c + 1);
      while (e <= cEnd) {
        const uint16_t n = px(s, e);
        if (d.hasTransparent && n == d.transparent) break;
        ++e;
      }
      g.setAddrWindow((int16_t)(x + c), py, (int16_t)(e - c), 1);
      // Equal neighbours still collapse into one writeColor - free here, and
      // it is what makes a flat area cheap without a second code path.
      uint16_t cur = col;
      uint16_t run = 1;
      for (int16_t i = (int16_t)(c + 1); i < e; ++i) {
        const uint16_t n = px(s, i);
        if (n == cur) { ++run; continue; }
        g.writeColor(cur, run);
        cur = n;
        run = 1;
      }
      g.writeColor(cur, run);
      c = e;
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
