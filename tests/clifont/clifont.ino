// **Can TinyGFX's decoder draw a CellFont that the LGFXFontToolJs CLI produced?**
//
// Not a hand-made interim font - **the real generator's output**, fed in
// unchanged. Only when this passes is "CellFont is supported" a true statement.
//
// This one test walks into three of the awkward parts of the spec:
//   - variable pitch (a glyph table; width / xAdvance / bytesPerGlyph are 0)
//   - a sparse index with a head block (headCount=2, first=0x32)
//   - **codes below `first` living in the tail** (0x20 / 0x2E)
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/FontCell.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>

#include "cli_font.h"

static const TinyGFXFontRef cliFont = {&font, &tinygfxFontCellOps};

static const int W = 128, H = 16;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("clifont");
  lcd.begin();
  bus.fill(0);
  lcd.setFont(&cliFont);
  lcd.setTextColor(TFT_WHITE);

  // Do the line height and ascent match what the generator wrote?
  // (line box 11px, ascent 10)
  tgfxReport("line", (long)lcd.fontHeight());
  tgfxReport("ascent", (long)lcd.getTextAscent());

  // Only characters the font covers. **UTF-8, so this goes by code point**
  const int16_t w = lcd.drawString("温度 23.5℃ 設定完了", 1, 2);
  tgfxReport("width", (long)w);
  tgfxReport("lit", (long)[]() {
    long n = 0;
    for (int i = 0; i < W * H; ++i) if (gram[i] != 0) ++n;
    return n;
  }());
  tgfxShot("clifont", gram, W, H);

  // An uncovered code ('A') draws nothing and advances 0
  bus.fill(0);
  tgfxReport("missing_adv", (long)lcd.drawChar('A', 0, 0));
  long after = 0;
  for (int i = 0; i < W * H; ++i) if (gram[i] != 0) ++after;
  tgfxReport("missing_lit", after);

  tgfxTestDone();
}
void loop() {}
