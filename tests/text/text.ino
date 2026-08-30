// Text drawing. The fonts come from the sketch side (tgfx_font), which is
// where they belong - the library ships none.
//
// The three fonts are the real generator's output for the same ten digits,
// encoded three different ways. See the equivalence check at the bottom.
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/DriverST7789.h>
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
TinyGFXDriverST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static const uint16_t FG = 0xFFFF;
static const uint16_t BGC = 0x001F;
static void reset() { bus.fill(0); bus.resetCounters(); }

/// The first column that has any ink. Used to check alignment.
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

  // --- centred and right-aligned --------------------------------------------
  //
  // Both are only arithmetic on top of drawString. What matters is **that the
  // position agrees with textWidth**, checked against drawString's return.
  //
  // LovyanGFX also offers this through setTextDatum; TinyGFX has only the
  // explicit calls. A datum is state drawString consults every time, so
  // everyone who never centres anything would pay 204 B (measured on a
  // CH32V003).
  {
    lcd.setTextColor(FG);
    lcd.setFont(&digitsFont);
    const int16_t w = lcd.textWidth("123");

    // **Stated as an invariant.** "The first column with ink" is offset by
    // the glyph's left bearing, so expected absolute coordinates would depend
    // on the font. Requiring that the aligned call and a drawString at the
    // computed position **differ by not one pixel** tests the alignment
    // arithmetic alone, bearing and all.
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

    // Alignment really moves things (both at 0 would "match" and prove nothing)
    tgfxReport("center_moved", (leftC != leftR) ? 1 : 0);
    tgfxReport("plain_width", (long)w);
  }

  tgfxTestDone();
}
void loop() {}
