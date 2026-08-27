// u8g2 形式のグリフデコーダ（**測定用の試作**）
//
// 目的は 1 つだけ: 「TinyGFX が u8g2 形式を食えるようにしたら、コードは何バイト増えるか」
// を実測すること（docs/FONT_FORMAT.ja.md §7）。ライブラリ本体には入れていない。
//
// 描画は TinyFont の経路と同じく fillRect のランで出すので、比較が公平になる。
#pragma once
#include <TinyGFX.h>
#include <stdint.h>

/// u8g2 のビット列リーダ。LSB 詰め、1 回の読み出しは高々 2 バイトにまたがる。
struct TgfxU8g2Bits {
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
struct TgfxU8g2Font {
  const uint8_t* data;

  uint8_t bitsPer0() const { return tinygfx_rd8(data + 2); }
  uint8_t bitsPer1() const { return tinygfx_rd8(data + 3); }
  uint8_t bitsW() const { return tinygfx_rd8(data + 4); }
  uint8_t bitsH() const { return tinygfx_rd8(data + 5); }
  uint8_t bitsX() const { return tinygfx_rd8(data + 6); }
  uint8_t bitsY() const { return tinygfx_rd8(data + 7); }
  uint8_t bitsD() const { return tinygfx_rd8(data + 8); }
  int8_t ascent() const { return (int8_t)tinygfx_rd8(data + 13); }
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

/// 1 文字描く。y は**ベースライン**（u8g2 の流儀）。戻り値は送り幅。
inline int16_t tgfxU8g2DrawChar(TinyGFX& g, const uint8_t* fontData, uint16_t ch,
                                int16_t x, int16_t y, uint16_t color) {
  const TgfxU8g2Font f = {fontData};
  const uint8_t* gd = f.glyphData(ch);
  if (gd == nullptr) return 0;

  TgfxU8g2Bits bits = {gd, 0};
  const uint8_t gw = bits.get(f.bitsW());
  const uint8_t gh = bits.get(f.bitsH());
  const int8_t gx = bits.getSigned(f.bitsX());
  const int8_t gy = bits.getSigned(f.bitsY());
  const int8_t adv = bits.getSigned(f.bitsD());
  if (gw == 0 || gh == 0) return adv;

  const int16_t left = (int16_t)(x + gx);
  const int16_t top = (int16_t)(y - gh - gy);
  const uint8_t b0 = f.bitsPer0();
  const uint8_t b1 = f.bitsPer1();

  uint8_t cx = 0, cy = 0;
  g.startWrite();
  do {
    const uint8_t zeros = bits.get(b0);
    const uint8_t ones = bits.get(b1);
    do {
      // 0 のラン: 進めるだけ
      uint8_t cnt = zeros;
      while (cnt > 0) {
        const uint8_t rem = (uint8_t)(gw - cx);
        const uint8_t run = rem < cnt ? rem : cnt;
        cx = (uint8_t)(cx + run);
        if (cx >= gw) { cx = 0; ++cy; }
        cnt = (uint8_t)(cnt - run);
      }
      // 1 のラン: 行ごとに水平線で描く
      cnt = ones;
      while (cnt > 0) {
        const uint8_t rem = (uint8_t)(gw - cx);
        const uint8_t run = rem < cnt ? rem : cnt;
        g.drawFastHLine((int16_t)(left + cx), (int16_t)(top + cy), (int16_t)run, color);
        cx = (uint8_t)(cx + run);
        if (cx >= gw) { cx = 0; ++cy; }
        cnt = (uint8_t)(cnt - run);
      }
    } while (bits.get(1) != 0);
  } while (cy < gh);
  g.endWrite();
  return adv;
}

/// UTF-8 の文字列を描く。y はベースライン。戻り値は描いた幅。
inline int16_t tgfxU8g2DrawString(TinyGFX& g, const uint8_t* fontData, const char* str,
                                  int16_t x, int16_t y, uint16_t color) {
  const int16_t x0 = x;
  while (*str) {
    uint16_t ch = (uint8_t)*str++;
    if (ch >= 0xF0) { ch = 0xFFFD; str += 3; }
    else if (ch >= 0xE0) {
      ch = (uint16_t)(((ch & 0x0F) << 12) | ((uint16_t)(str[0] & 0x3F) << 6) | (str[1] & 0x3F));
      str += 2;
    } else if (ch >= 0xC0) {
      ch = (uint16_t)(((ch & 0x1F) << 6) | (str[0] & 0x3F));
      str += 1;
    }
    x = (int16_t)(x + tgfxU8g2DrawChar(g, fontData, ch, x, y, color));
  }
  return (int16_t)(x - x0);
}
