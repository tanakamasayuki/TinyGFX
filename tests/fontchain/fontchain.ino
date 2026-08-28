// CellFont::next chaining and the U+FFFD fallback.
//
// Small CellFonts written by hand, to walk paths a generated font cannot
// reach. Three things are pinned here:
//
//   1. The fallback happens once, after the whole chain has been searched
//      (CellFont spec 7.2 and 15.2). This catches the accident where a notdef
//      in the first link hides a glyph the second link actually has.
//   2. The baseline comes from the chain head's metrics (spec 8), so links of
//      different heights still sit on one line.
//   3. A sparse index's head block, and a tail holding codes lower than
//      `first` (spec 7.1).
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/FontCell.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>

static const int W = 16, H = 12;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

// --- Font 2 of the chain: only 'B', and a different height (3 px, ascent 3) --
// Declared first because the link before it has to point at it.
static const uint8_t bBits[2] CELLFONT_PROGMEM = {0xFF, 0x80};  // 3x3, solid
static const CellFont fontB CELLFONT_PROGMEM = {
    bBits, nullptr, nullptr, nullptr,
    0x42, 1,   // first='B', count=1
    3, 3,      // width, height
    4, 6,      // xAdvance, yAdvance
    0, -3,     // xOffset, yOffset (ascent=3, unlike the head)
    2,         // bytesPerGlyph
    0,
};

// --- Font 1: sparse, holding 'A' and U+FFFD (head block 1, tail 1) -----------
// A 3x5 cell. Glyph 0 = 'A' (solid), glyph 1 = U+FFFD (top row only).
static const uint8_t anBits[4] CELLFONT_PROGMEM = {0xFF, 0xFE, 0xE0, 0x00};
static const uint16_t anCodes[1] CELLFONT_PROGMEM = {0xFFFD};
static const CellFont fontAN CELLFONT_PROGMEM = {
    anBits, nullptr, anCodes, nullptr,
    0x41, 2,   // first='A', count=2
    3, 5,      // width, height
    4, 6,      // xAdvance, yAdvance
    0, -5,     // xOffset, yOffset (ascent=5)
    2,         // bytesPerGlyph
    1,         // headCount ('A' is the head; U+FFFD is the tail)
};

// The same font again, this time chained to fontB. This is the pair that
// catches the accident: fontAN has a notdef, fontB has 'B'.
static const CellFont fontANChain CELLFONT_PROGMEM = {
    anBits, nullptr, anCodes, &fontB,
    0x41, 2, 3, 5, 4, 6, 0, -5, 2, 1,
};

// --- Font 3: sparse, with a tail code lower than `first` ---------------------
// The head block is 'B' (0x42) and the tail holds 'A' (0x41). This is the path
// spec 7.1 warns about: only a contiguous index may bail out on `c < first`.
static const uint16_t loCodes[1] CELLFONT_PROGMEM = {0x0041};
static const CellFont fontLo CELLFONT_PROGMEM = {
    anBits, nullptr, loCodes, nullptr,
    0x42, 2,   // first='B'（頭）、count=2
    3, 5, 4, 6, 0, -5, 2,
    1,         // headCount=1
};

static const TinyGFXFontRef refB = {&fontB, &tinygfxFontCellOps};
static const TinyGFXFontRef refAN = {&fontAN, &tinygfxFontCellOps};
static const TinyGFXFontRef refChain = {&fontANChain, &tinygfxFontCellOps};
static const TinyGFXFontRef refLo = {&fontLo, &tinygfxFontCellOps};

static void clear() {
  bus.fill(0);
}

static long lit() {
  long n = 0;
  for (int i = 0; i < W * H; ++i)
    if (gram[i] != 0) ++n;
  return n;
}

/// The range of rows with ink, or -1 / -1 when there is none.
static void litRows(long* top, long* bottom) {
  *top = -1;
  *bottom = -1;
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      if (gram[y * W + x] != 0) {
        if (*top < 0) *top = y;
        *bottom = y;
        break;
      }
}

static void scene(const char* name, const TinyGFXFontRef* font, uint16_t ch) {
  clear();
  lcd.setFont(font);
  const long ret = lcd.drawChar(ch, 0, 0);
  long top, bottom;
  litRows(&top, &bottom);
  tgfxReport2(name, "ret", ret);
  tgfxReport2(name, "lit", lit());
  tgfxReport2(name, "top", top);
  tgfxReport2(name, "bottom", bottom);
  Serial.print("SCENE ");
  Serial.println(name);
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("fontchain");
  lcd.begin();
  lcd.setTextColor(TFT_WHITE);

  scene("a", &refAN, 'A');          // covered: a solid 3x5 = 15 pixels
  scene("nd", &refAN, 'Z');         // uncovered -> U+FFFD: the top row = 3
  scene("ndself", &refAN, 0xFFFD);  // asking for U+FFFD itself: no second search
  scene("chain", &refChain, 'B');   // the notdef in link 1 must not hide 'B' in link 2
  scene("miss", &refB, 'Z');        // nowhere at all: nothing drawn, advance 0
  scene("lo", &refLo, 'A');         // a tail code lower than `first`
  scene("lohead", &refLo, 'B');     // the head block of the same font

  // Line advance and ascent both come from the head of the chain
  lcd.setFont(&refChain);
  tgfxReport("chain_line", (long)lcd.fontHeight());
  tgfxReport("chain_ascent", (long)lcd.getTextAscent());
  lcd.setFont(&refB);
  tgfxReport("b_ascent", (long)lcd.getTextAscent());

  tgfxTestDone();
}
void loop() {}
