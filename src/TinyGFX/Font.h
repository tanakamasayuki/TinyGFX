// TinyGFX - フォントの受け口（形式に依らない部分）
//
// **コアはフォント形式を 1 つも知らない。** 知っているのはフォント側で、
// データと一緒に「自分をどう描くか」（TinyGFXFontOps）を指している。
//
//   スケッチ ── #include <TinyGFX/FontTiny.h> ──▶ TinyFont のデコーダ
//            └─ #include <TinyGFX/FontU8g2.h> ──▶ u8g2 のデコーダ
//
// include していない形式のデコーダは**どこからも参照されないのでリンクされない。**
// 形式を増やしてもフットプリントは増えない（使わなければ 0 バイト）。
//
// 連鎖（next）は形式をまたげる。半角を TinyFont、全角を別形式、も書ける。
#pragma once
#include <stddef.h>
#include <stdint.h>

class TinyGFX;

/// 形式ごとの入口。値は「収録外なら -1」で統一する。
struct TinyGFXFontOps {
  /// 1 文字描いて送り幅を返す。収録外なら -1（何も描かない）。
  int16_t (*draw)(TinyGFX& gfx, const void* font, uint16_t ch, int16_t x, int16_t y);
  /// 描かずに送り幅だけ返す。収録外なら -1。
  int16_t (*advance)(const void* font, uint16_t ch);
  /// 行送り（倍率をかける前）。
  uint8_t (*lineHeight)(const void* font);
};

/// スケッチが `setFont()` に渡すもの。フォントデータと入口の組。
struct TinyGFXFontRef {
  const void* data;
  const TinyGFXFontOps* ops;
  /// この字を持っていないとき次に探すフォント。nullptr で終端。
  /// **形式が違っていてもよい。**
  const TinyGFXFontRef* next;
};

// ---------------------------------------------------------------------------
// フォントデータの読み出し
//
// AVR はフラッシュとデータのアドレス空間が別なので、PROGMEM に置いたデータは
// pgm_read_* でしか読めない。それ以外のアーキテクチャでは素の参照に展開され、
// 1 命令も増えない。
//
// **PROGMEM に置くのはビットマップ・グリフ表・コード表だけ。**
// `TinyGFXFontRef` と `TinyGFXFontOps` は素の const（AVR では RAM）に置く。
// 合わせて 1 フォントあたり 24 バイト程度で、そのぶん分岐経路が素の参照で済む。
// ---------------------------------------------------------------------------
#if defined(__AVR__)
#include <avr/pgmspace.h>
#define TINYGFX_FONT_PROGMEM PROGMEM
inline uint8_t tinygfx_rd8(const void* p) { return pgm_read_byte(p); }
inline uint16_t tinygfx_rd16(const void* p) { return pgm_read_word(p); }
inline const void* tinygfx_rdptr(const void* p) { return (const void*)pgm_read_ptr(p); }
#else
#define TINYGFX_FONT_PROGMEM
inline uint8_t tinygfx_rd8(const void* p) { return *(const uint8_t*)p; }
inline uint16_t tinygfx_rd16(const void* p) { return *(const uint16_t*)p; }
inline const void* tinygfx_rdptr(const void* p) { return *(const void* const*)p; }
#endif
