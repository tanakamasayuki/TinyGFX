// TinyGFX - u8g2 形式のフォント
//
// **高さ 16 画素を超えて、かつ収録字数が多いフォントはこちらのほうが小さい。**
// RLE とグリフごとの bbox が効き始めるため（docs/FONT_FORMAT.ja.md §0）。
// H≤16 なら、字数が少なければ H>16 でも CellFont（FontCell.h）のほうが小さい。
//
// デコーダのコード量は CellFont とほぼ同じ（693 B vs 684 B、実測）。
// **include しなければ 1 バイトもリンクされない。**
//
// 正しさは LGFXFontToolJs が描いた絵との一致で確認している（tests/u8g2/）。
#pragma once
#include <stdint.h>

#include "Font.h"
#include "Gfx.h"

/// u8g2 のビット列リーダ。LSB 詰め、1 回の読み出しは高々 2 バイトにまたがる。
struct TinyGFXU8g2Bits {
  const uint8_t* ptr;
  uint8_t bitPos;

  uint8_t get(uint8_t cnt) {
    uint8_t val = tinygfx_rd8(ptr);
    val = (uint8_t)(val >> bitPos);
    const uint8_t prev = bitPos;
    bitPos = (uint8_t)(bitPos + cnt);
    if (bitPos >= 8) {
      ++ptr;
      val = (uint8_t)(val | (uint8_t)(tinygfx_rd8(ptr) << (8 - prev)));
      bitPos = (uint8_t)(bitPos - 8);
    }
    return (uint8_t)(val & ((1u << cnt) - 1));
  }

  int8_t getSigned(uint8_t cnt) {
    return (int8_t)(get(cnt) - (int8_t)((1u << cnt) >> 1));
  }
};

/// u8g2 フォントのヘッダ（23 バイト）のうち、描画に要るものだけ。
struct TinyGFXFontU8g2 {
  const uint8_t* data;

  uint8_t bitsPer0() const { return tinygfx_rd8(data + 2); }
  uint8_t bitsPer1() const { return tinygfx_rd8(data + 3); }
  uint8_t bitsW() const { return tinygfx_rd8(data + 4); }
  uint8_t bitsH() const { return tinygfx_rd8(data + 5); }
  uint8_t bitsX() const { return tinygfx_rd8(data + 6); }
  uint8_t bitsY() const { return tinygfx_rd8(data + 7); }
  uint8_t bitsD() const { return tinygfx_rd8(data + 8); }
  int8_t ascentPara() const { return (int8_t)tinygfx_rd8(data + 15); }
  int8_t descentPara() const { return (int8_t)tinygfx_rd8(data + 16); }
  uint16_t startUpperA() const { return word(17); }
  uint16_t startLowerA() const { return word(19); }
  uint16_t startUnicode() const { return word(21); }

  uint16_t word(uint16_t off) const {
    return (uint16_t)(((uint16_t)tinygfx_rd8(data + off) << 8) | tinygfx_rd8(data + off + 1));
  }

  /// グリフのビット列の先頭。無ければ nullptr。
  const uint8_t* glyphData(uint16_t enc) const {
    const uint8_t* p = data + 23;
    if (enc <= 255) {
      if (enc >= 'a') p += startLowerA();
      else if (enc >= 'A') p += startUpperA();
      for (;;) {
        const uint8_t step = tinygfx_rd8(p + 1);
        if (step == 0) return nullptr;
        if (tinygfx_rd8(p) == (uint8_t)enc) return p + 2;
        p += step;
      }
    }
    // Unicode 区画: 4 バイトの飛び先表（次ブロックまでのバイト数 / そのブロックの最終コード）
    const uint8_t* table = data + 23 + startUnicode();
    p = table;
    uint16_t lastInBlock;
    do {
      const uint16_t skip = (uint16_t)(((uint16_t)tinygfx_rd8(table) << 8) | tinygfx_rd8(table + 1));
      lastInBlock = (uint16_t)(((uint16_t)tinygfx_rd8(table + 2) << 8) | tinygfx_rd8(table + 3));
      if (skip == 0) return nullptr;
      p = table + skip;
      table += 4;
    } while (lastInBlock < enc);
    for (;;) {
      const uint16_t e = (uint16_t)(((uint16_t)tinygfx_rd8(p) << 8) | tinygfx_rd8(p + 1));
      if (e == 0) return nullptr;
      if (e == enc) return p + 3;
      p += tinygfx_rd8(p + 2);
    }
  }
};

namespace tinygfx_u8g2 {

/// 行送り。u8g2 の ascent_para - descent_para。
inline uint8_t lineHeight(const void* font) {
  const TinyGFXFontU8g2 f = {(const uint8_t*)font};
  return (uint8_t)(f.ascentPara() - f.descentPara());
}

/// ベースラインから行の箱の上端まで。u8g2 の ascent_para。
inline int16_t ascent(const void* font) {
  const TinyGFXFontU8g2 f = {(const uint8_t*)font};
  return (int16_t)f.ascentPara();
}

/// 描かずに送り幅だけ返す。収録外は -1。
inline int16_t advance(const void* font, uint16_t ch) {
  const TinyGFXFontU8g2 f = {(const uint8_t*)font};
  const uint8_t* gd = f.glyphData(ch);
  if (gd == nullptr) return -1;
  TinyGFXU8g2Bits bits = {gd, 0};
  bits.get(f.bitsW());
  bits.get(f.bitsH());
  bits.getSigned(f.bitsX());
  bits.getSigned(f.bitsY());
  return (int16_t)bits.getSigned(f.bitsD());
}

/// 1 文字描く。**y はベースライン**（u8g2 本来の基準と同じ）。
/// 戻り値は送り幅（倍率をかける前）。収録外は -1。
inline int16_t draw(TinyGFX& g, const void* font, uint16_t ch, int16_t x, int16_t y) {
  const TinyGFXFontU8g2 f = {(const uint8_t*)font};
  const uint8_t* gd = f.glyphData(ch);
  if (gd == nullptr) return -1;

  TinyGFXU8g2Bits bits = {gd, 0};
  const uint8_t gw = bits.get(f.bitsW());
  const uint8_t gh = bits.get(f.bitsH());
  const int8_t gx = bits.getSigned(f.bitsX());
  const int8_t gy = bits.getSigned(f.bitsY());
  const int8_t adv = bits.getSigned(f.bitsD());

  const uint8_t sz = g.getTextSize();
  if (g.hasTextBg()) {
    // 行の箱は**連鎖先頭のメトリクス**で決める（フォントごとに変えると段がずれる）
    const int16_t cellTop = (int16_t)(y - (int16_t)(g.getTextAscent() * sz));
    g.fillRect(x, cellTop, (int16_t)((int16_t)adv * sz),
               (int16_t)((uint16_t)g.getTextLineHeight() * sz), g.getTextBgColor());
  }
  if (gw == 0 || gh == 0) return (int16_t)adv;

  const int16_t left = (int16_t)(x + (int16_t)(gx * sz));
  const int16_t top = (int16_t)(y - (int16_t)((gh + gy) * sz));
  const uint8_t b0 = f.bitsPer0();
  const uint8_t b1 = f.bitsPer1();
  const uint16_t fg = g.getTextColor();

  uint8_t cx = 0, cy = 0;
  g.startWrite();
  do {
    const uint8_t zeros = bits.get(b0);
    const uint8_t ones = bits.get(b1);
    do {
      uint8_t cnt = zeros;  // 0 のラン: 進めるだけ
      while (cnt > 0) {
        const uint8_t rem = (uint8_t)(gw - cx);
        const uint8_t run = rem < cnt ? rem : cnt;
        cx = (uint8_t)(cx + run);
        if (cx >= gw) { cx = 0; ++cy; }
        cnt = (uint8_t)(cnt - run);
      }
      cnt = ones;  // 1 のラン: 行ごとに矩形で描く
      while (cnt > 0) {
        const uint8_t rem = (uint8_t)(gw - cx);
        const uint8_t run = rem < cnt ? rem : cnt;
        g.fillRect((int16_t)(left + (int16_t)(cx * sz)), (int16_t)(top + (int16_t)(cy * sz)),
                   (int16_t)((uint16_t)run * sz), sz, fg);
        cx = (uint8_t)(cx + run);
        if (cx >= gw) { cx = 0; ++cy; }
        cnt = (uint8_t)(cnt - run);
      }
    } while (bits.get(1) != 0);
  } while (cy < gh);
  g.endWrite();
  return (int16_t)adv;
}

}  // namespace tinygfx_u8g2

/// この形式の入口。生成したフォントヘッダが TinyGFXFontRef からここを指す。
static const TinyGFXFontOps tinygfxFontU8g2Ops = {
    &tinygfx_u8g2::draw,
    &tinygfx_u8g2::advance,
    &tinygfx_u8g2::lineHeight,
    &tinygfx_u8g2::ascent,
};
