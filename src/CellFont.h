// CellFont — ビットマップフォント形式 v1
//
// 形式の仕様書（生成器と描画器の取り決め）は TinyGFX の外にある:
//   https://github.com/tanakamasayuki/LGFXFontToolJs  docs/formats/cellfont.ja.md
//
// このファイルは仕様 §12.1 が描画器に求めるものだけを置く。
// **生成されたフォントヘッダはこれを `#include <CellFont.h>` で拾う**ので、
// ライブラリのルート（src/）に、この名前で無ければならない。
//
// TinyGFX には依存しない。構造体とアクセサだけで、コードを 1 バイトも生まない。
//
// 同じ仕様を実装した別のライブラリと同居したときのために、
// **CELLFONT_SPEC_VERSION で全体を守っている。** 先に定義したほうが勝ち、
// 生成ヘッダはどちらの定義でも同じように読める（仕様が同じなので構造体も同じ）。
#ifndef CELLFONT_SPEC_VERSION

/// この実装が従う仕様の版。生成ヘッダが #error で照合する。
#define CELLFONT_SPEC_VERSION 1

#include <stdint.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
/// フォントデータに付ける属性。**ビットマップ・グリフ表・コード表・構造体の 4 つとも**。
/// 1 つでも抜けると、下のアクセサが RAM のアドレスをプログラム空間として読んで化ける。
#define CELLFONT_PROGMEM PROGMEM
#define CELLFONT_READ_U8(p) pgm_read_byte(p)
#define CELLFONT_READ_U16(p) pgm_read_word(p)
#define CELLFONT_READ_PTR(p) ((const void*)pgm_read_ptr(p))
#else
#define CELLFONT_PROGMEM
#define CELLFONT_READ_U8(p) (*(const uint8_t*)(p))
#define CELLFONT_READ_U16(p) (*(const uint16_t*)(p))
#define CELLFONT_READ_PTR(p) (*(const void* const*)(p))
#endif

/// 可変ピッチのときのグリフ 1 件。**4 バイト固定**（仕様 §3）。
/// 高さ・xOffset・yOffset は全グリフ共通なので持たない。
typedef struct CellGlyph {
  uint16_t offset;   ///< bitmap 先頭からのバイトオフセット
  uint8_t width;     ///< グリフの幅（画素）。0 も合法
  uint8_t xAdvance;  ///< 送り幅（画素）
} CellGlyph;

/// フォント 1 本。**フィールドの並びは仕様 §3 のとおりでなければならない**
/// （生成ヘッダが位置初期化子で埋めるため）。
typedef struct CellFont {
  const uint8_t* bitmap;        ///< グリフのビット列
  const CellGlyph* glyphs;      ///< NULL = 固定ピッチ
  const uint16_t* codes;        ///< NULL = 連続索引。長さ count - headCount
  const struct CellFont* next;  ///< NULL = 連鎖の終端
  uint16_t first;               ///< 連続索引の先頭 / 疎索引では頭ブロックの先頭
  uint16_t count;               ///< 収録グリフ数
  uint8_t width, height;        ///< width は固定ピッチ用。height は全グリフ共通
  uint8_t xAdvance, yAdvance;   ///< xAdvance は固定ピッチ用。yAdvance は行送り
  int8_t xOffset, yOffset;      ///< 全グリフ共通。yOffset は通常負（箱はベースラインの上）
  uint8_t bytesPerGlyph;        ///< 固定ピッチ用。実行時に除算しないため
  uint8_t headCount;            ///< 疎索引の頭ブロック長（1 以上）。連続索引では 0
} CellFont;

#if defined(__cplusplus)
// 仕様 §3: 大きさは ABI 依存なので、対象 ABI で確かめる。
// CellGlyph だけは 4 バイトでなければならない（グリフ表の刻み幅が 4 前提）。
static_assert(sizeof(CellGlyph) == 4, "CellGlyph must be 4 bytes");
#endif

#endif  // CELLFONT_SPEC_VERSION
