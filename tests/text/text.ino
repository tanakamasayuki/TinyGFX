// Text drawing. The fonts come from the sketch side (tgfx_font), which is
// where they belong - the library ships none.
//
// The three fonts are the real generator's output for the same ten digits,
// encoded three different ways. See the equivalence check at the bottom.
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>
#include <TinyGFX/FontCell.h>
#include <tgfx_digits.h>
#include <tgfx_digits_sparse.h>
#include <tgfx_digits_chain.h>

static const TinyGFXFontRef digitsFont = {&tgfxDigits, &tinygfxFontCellOps};
static const TinyGFXFontRef sparseFont = {&tgfxDigitsSparse, &tinygfxFontCellOps};
static const TinyGFXFontRef chainFont = {&tgfxDigitsChain, &tinygfxFontCellOps};

static const int W = 64, H = 32;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static const uint16_t FG = 0xFFFF;
static const uint16_t BGC = 0x001F;
static void reset() { bus.fill(0); bus.resetCounters(); }

/// 最初に画素が点いた列。揃え位置の検査用。
static int16_t firstLitColumn() {
  for (int16_t x = 0; x < W; ++x) {
    for (int16_t y = 0; y < H; ++y) {
      if (gram[(int32_t)y * W + x] != 0x0000) return x;
    }
  }
  return -1;
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("text");
  lcd.begin();
  lcd.setFont(&digitsFont);
  lcd.setTextColor(FG);

  tgfxReport("font_height", (long)lcd.fontHeight());
  tgfxReport("text_width", (long)lcd.textWidth("12345"));

  reset();
  tgfxReport("draw_width", (long)lcd.drawString("12345", 2, 3));
  tgfxShot("plain", gram, W, H);

  // double size
  lcd.setTextSize(2);
  tgfxReport("width_x2", (long)lcd.textWidth("12345"));
  tgfxReport("height_x2", (long)lcd.fontHeight());
  reset();
  lcd.drawString("012", 1, 1);
  tgfxShot("double", gram, W, H);
  lcd.setTextSize(1);

  // An uncovered code draws nothing and returns 0
  reset();
  tgfxReport("oor_advance", (long)lcd.drawChar('A', 2, 2));
  tgfxReport("oor_pixels", (long)bus.pixelCount());

  // With a background colour the whole cell is painted
  reset();
  lcd.setTextColor(FG, BGC);
  lcd.drawString("12345", 2, 3);
  tgfxShot("opaque", gram, W, H);
  lcd.setTextColor(FG);

  // Transparent: nothing outside the glyph pixels is touched
  reset();
  lcd.drawString("88888", 2, 3);
  tgfxReport("transparent_pixels", (long)bus.pixelCount());
  tgfxShot("transparent", gram, W, H);

  // --- One glyph set, three encodings -------------------------------------
  // The same digits, encoded three ways by the generator:
  //   digitsFont  fixed pitch, contiguous index, one font
  //   sparseFont  variable pitch, sparse index with a head block
  //   chainFont   fixed pitch, split by cell width class into a next chain
  // All three must draw exactly the same pixels. Which encoding the generator
  // picks is its business, not something a sketch should be able to see.
  lcd.setTextColor(FG);
  reset(); lcd.setFont(&digitsFont);       lcd.drawString("0123456789", 1, 1);
  tgfxShot("var_fixed", gram, W, H);
  reset(); lcd.setFont(&sparseFont);    lcd.drawString("0123456789", 1, 1);
  tgfxShot("var_records", gram, W, H);
  reset(); lcd.setFont(&chainFont); lcd.drawString("0123456789", 1, 1);
  tgfxShot("var_sparse", gram, W, H);
  lcd.setFont(&digitsFont);

  // --- 中央揃え・右揃え -----------------------------------------------------
  //
  // どちらも drawString の上の算術でしかない。**位置が textWidth と
  // 辻褄が合っていること**を見る（drawString の戻り値と突き合わせる）。
  //
  // LovyanGFX はこれを setTextDatum でも提供しているが、TinyGFX は
  // 明示関数だけにした。datum は drawString が毎回参照する状態になるので、
  // 中央揃えを使わない人まで 204 B 払うことになる（CH32V003 で実測）。
  {
    lcd.setTextColor(FG);
    lcd.setFont(&digitsFont);
    const int16_t w = lcd.textWidth("123");

    // **不変条件で見る。** 「点いた最初の列」はグリフの左の余白ぶんずれる
    // ので、絶対座標で期待値を書くとフォントに依存してしまう。
    // 揃え版と「算出位置に置いた drawString」が **1 画素も違わないこと**を
    // 見れば、余白に関係なく揃えの算術だけを検査できる。
    reset();
    const int16_t retC = lcd.drawCenterString("123", 32, 4);
    const int16_t leftC = firstLitColumn();
    reset();
    lcd.drawString("123", (int16_t)(32 - w / 2), 4);
    tgfxReport("center_matches", (leftC == firstLitColumn()) ? 1 : 0);
    tgfxReport("center_ret", (long)retC);

    reset();
    const int16_t retR = lcd.drawRightString("123", 40, 4);
    const int16_t leftR = firstLitColumn();
    reset();
    lcd.drawString("123", (int16_t)(40 - w), 4);
    tgfxReport("right_matches", (leftR == firstLitColumn()) ? 1 : 0);
    tgfxReport("right_ret", (long)retR);

    // 揃えると位置が実際に動いていること（両方 0 で「一致」しても意味がない）
    tgfxReport("center_moved", (leftC != leftR) ? 1 : 0);
    tgfxReport("plain_width", (long)w);
  }

  tgfxTestDone();
}
void loop() {}
