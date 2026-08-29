// **LGFXFontToolJs の CLI が出した CellFont を、TinyGFX のデコーダで描けるか。**
//
// 手で作ったつなぎのフォント（tools/gen_font.py）ではなく、**本番の生成器の出力**を
// そのまま食わせる。ここが通って初めて「CellFont に対応した」と言える。
//
// この 1 本で仕様の難所を 3 つ踏む:
//   - 可変ピッチ（グリフ表あり、width / xAdvance / bytesPerGlyph は 0）
//   - 疎索引の頭ブロック（headCount=2、first=0x32）
//   - **first より小さいコードがしっぽに居る**（0x20 / 0x2E）
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

  // 行送りと ascent が生成器の書いたとおりか（Line box 11px / ascent 10）
  tgfxReport("line", (long)lcd.fontHeight());
  tgfxReport("ascent", (long)lcd.getTextAscent());

  // 収録されている字だけを並べる。**UTF-8 なのでコードポイントで送る**
  const int16_t w = lcd.drawString("温度 23.5℃ 設定完了", 1, 2);
  tgfxReport("width", (long)w);
  tgfxReport("lit", (long)[]() {
    long n = 0;
    for (int i = 0; i < W * H; ++i) if (gram[i] != 0) ++n;
    return n;
  }());
  tgfxShot("clifont", gram, W, H);

  // 収録外（'A'）は何も描かず送り 0
  bus.fill(0);
  tgfxReport("missing_adv", (long)lcd.drawChar('A', 0, 0));
  long after = 0;
  for (int i = 0; i < W * H; ++i) if (gram[i] != 0) ++after;
  tgfxReport("missing_lit", after);

  tgfxTestDone();
}
void loop() {}
