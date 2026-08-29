// 色定数の 2 つの綴り（DECISIONS.ja.md D30）。
//
// **すべてコンパイル時の検査。** 実行するものは何もない。
// `TFT_RED` を先に定義した状態でもビルドし、
//   - 先に定義したほうが生き残ること（TinyGFX が上書きしないこと）
//   - `TINYGFX_RED` はそれに関係なく TinyGFX の値であること
// を見る。build_matrix が 2 通りでコンパイルする。
#include <TinyGFX.h>

static_assert(TINYGFX_BLACK == 0x0000, "TINYGFX_BLACK");
static_assert(TINYGFX_WHITE == 0xFFFF, "TINYGFX_WHITE");
static_assert(TINYGFX_RED == 0xF800, "TINYGFX_RED は他ライブラリに奪われない");
static_assert(TINYGFX_DARKGREY == 0x7BEF, "TINYGFX_DARKGREY");

#if defined(TGFX_FOREIGN_RED)
static_assert(TFT_RED == TGFX_FOREIGN_RED, "先に定義された TFT_RED を上書きしている");
static_assert(TFT_BLACK == TINYGFX_BLACK, "1 つ奪われただけで他まで欠けている");
#else
static_assert(TFT_RED == TINYGFX_RED, "2 つの綴りが一致していない");
static_assert(TFT_PINK == TINYGFX_PINK, "2 つの綴りが一致していない");
#endif

void setup() {}
void loop() {}
