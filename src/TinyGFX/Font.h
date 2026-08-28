// TinyGFX - フォントの受け口（形式に依らない部分）
//
// **コアはフォント形式を 1 つも知らない。** 知っているのはフォント側で、
// データと一緒に「自分をどう描くか」（TinyGFXFontOps）を指している。
//
//   スケッチ ── #include <TinyGFX/FontCell.h> ──▶ CellFont のデコーダ
//            └─ #include <TinyGFX/FontU8g2.h> ──▶ u8g2 のデコーダ
//
// include していない形式のデコーダは**どこからも参照されないのでリンクされない。**
// 形式を増やしてもフットプリントは増えない（使わなければ 0 バイト）。
//
// 連鎖は 2 段ある。**役割が違うので両方要る。**
//
//   TinyGFXFontRef::next  形式をまたぐ連鎖。半角 CellFont → 全角 u8g2 など
//   CellFont::next        同じ形式の中の連鎖。幅クラスで分けて固定ピッチを立てる
//
// **U+FFFD への退避は最外（TinyGFX::drawChar）でだけ行う。** デコーダの中で
// やると、その形式に U+FFFD があるだけで後段の別形式に到達できなくなる
// （CellFont 仕様 §15.2）。デコーダは「見つかった / 見つからない」を返すに留める。
#pragma once
#include <stddef.h>
#include <stdint.h>

class TinyGFX;

/// 形式ごとの入口。「収録外なら -1」で統一する。
struct TinyGFXFontOps {
  /// 1 文字描いて送り幅を返す。**y はベースライン。** 収録外なら -1（何も描かない）。
  int16_t (*draw)(TinyGFX& gfx, const void* font, uint16_t ch, int16_t x, int16_t y);
  /// 描かずに送り幅だけ返す。収録外なら -1。
  int16_t (*advance)(const void* font, uint16_t ch);
  /// 行送り（倍率をかける前）。
  uint8_t (*lineHeight)(const void* font);
  /// ベースラインから行の箱の上端まで（倍率をかける前）。
  /// **コアはこれで「行の上端」をベースラインに直す。** 連鎖の先頭のものだけを使う。
  int16_t (*ascent)(const void* font);
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
// フォントデータの読み出し（CellFont 以外の形式用）
//
// AVR はフラッシュとデータのアドレス空間が別なので、PROGMEM に置いたデータは
// pgm_read_* でしか読めない。それ以外のアーキテクチャでは素の参照に展開され、
// 1 命令も増えない。
//
// **フォントデータ側は 1 つ残らず PROGMEM に置く。** デコーダはこれらを必ず
// tinygfx_rd* で読むので、**どれか 1 つでも外すと AVR で化ける**
// （pgm_read_* が RAM のアドレスをプログラム空間として読むため）。生成器の責任。
//
// 逆に `TinyGFXFontRef` と `TinyGFXFontOps` は素の const（AVR では RAM）に置き、
// 素の参照で読む。合わせて 1 フォントあたり 6〜12 バイトで、そのぶん分岐経路が安い。
//
// CellFont は仕様が名前を決めているので、同じものを `CELLFONT_READ_*` として
// <CellFont.h> が別に持つ（そちらは TinyGFX に依存しない）。
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
