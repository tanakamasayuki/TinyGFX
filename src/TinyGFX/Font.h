// TinyGFX - TinyFont: 組込み向けの最小ビットマップフォント形式
//
// **対象は高さ 16 画素以下のピクセルグリッドフォント。** それより大きいと
// bbox / RLE / u8g2 の 3 つが同時に有利へ反転するので、素直に u8g2 を使うほうがよい。
// 根拠は docs/FONT_FORMAT.ja.md §6。
//
// 設計の要点は「選択を実行時ではなく**生成時**に済ませ、結果をデータで表す」こと。
//
//   索引     連続（first からの通し）か、疎（昇順のコード表）か
//   レコード 持つ（可変ピッチ）か、持たない（固定ピッチ）か
//   連鎖     半角と全角を別フォントに割り、next で繋ぐか
//
// どちらもポインタが nullptr かどうかで決まる。デコーダ側の分岐は
// それぞれ 1 か所だけで、使わない側もリンクされるが数十バイトに収まる。
// 詳しくは docs/FONT_FORMAT.ja.md。
//
// ビットマップは「行を連結した MSB first のビット列、グリフ間はバイト境界揃え」。
// GFXfont（Adafruit）と同じ並びなので、既存資産はツール側で変換できる。
//
// TinyGFX 自身はフォントデータを 1 つも同梱しない（docs/DECISIONS.ja.md D17）。
#pragma once
#include <stdint.h>

/// 可変ピッチのときのグリフ 1 件。**4 バイト固定。**
/// 高さ・xOffset・yOffset はフォント全体で共通なので持たない。
struct TinyGFXGlyph {
  uint8_t offsetLo;   // bitmap 先頭からのバイトオフセット（16bit）
  uint8_t offsetHi;
  uint8_t width;      // グリフの幅（画素）
  uint8_t xAdvance;   // 送り幅
};

struct TinyGFXFont {
  const uint8_t* bitmap;

  /// nullptr なら**固定ピッチ**（全グリフが width / xAdvance / bytesPerGlyph 共通）。
  const TinyGFXGlyph* glyphs;

  /// nullptr なら**連続索引**（first から count 個）。
  /// 非 nullptr なら**疎索引**（昇順に並んだコード表。count 個）。
  const uint16_t* codes;

  uint16_t first;         // 連続索引のときの開始コード
  uint16_t count;         // 収録グリフ数
  uint8_t width;          // 固定ピッチのときのグリフ幅
  uint8_t height;         // 全グリフ共通
  uint8_t xAdvance;       // 固定ピッチのときの送り幅
  uint8_t yAdvance;       // 行送り
  int8_t xOffset;         // 全グリフ共通
  int8_t yOffset;         // **行の上端からのグリフ上端**（下が正）。全グリフ共通
  uint8_t bytesPerGlyph;  // 固定ピッチのとき。実行時に除算しないため持っておく

  /// この字を持っていないとき次に探すフォント。nullptr で終端。
  ///
  /// 半角と全角を別フォントに割ると**両方が固定ピッチになり、グリフ表が丸ごと消える**。
  /// 割った 2 つをこれで繋ぐ（日本語混在 190 字・8px で 2,349 → 1,471 B の実測あり）。
  const TinyGFXFont* next;
};

// ---------------------------------------------------------------------------
// フォントデータの読み出し
//
// AVR はフラッシュとデータのアドレス空間が別なので、PROGMEM に置いたデータは
// pgm_read_* でしか読めない。それ以外のアーキテクチャでは素の参照に展開され、
// 1 命令も増えない。
//
// **AVR ではフォントを PROGMEM に置くこと。** 置かないと化ける。
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
