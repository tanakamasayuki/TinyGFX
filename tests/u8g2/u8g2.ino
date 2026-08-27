// u8g2 形式デコーダ（測定用の試作）の正しさ。
//
// LGFXFontToolJs が同じフォント・同じ文字列を描いた絵と一致すること。
// 一致しないデコーダのコード量を測っても意味がないので、まずここを通す。
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>
#include <tgfx_u8g2.h>

#include "u8g2_ascii.h"
#include "u8g2_cjk.h"

static const int W = 80, H = 24;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static const uint16_t FG = 0xFFFF;

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("u8g2");
  lcd.begin();

  bus.fill(0);
  bus.resetCounters();
  tgfxReport("ascii_width", (long)tgfxU8g2DrawString(lcd, u8g2_ascii, "0123456789ABCabc", 2, 16, FG));
  tgfxReport("ascii_pixels", (long)bus.pixelCount());
  tgfxShot("ascii", gram, W, H);

  bus.fill(0);
  bus.resetCounters();
  tgfxReport("cjk_width", (long)tgfxU8g2DrawString(lcd, u8g2_cjk, "日本語表示", 2, 16, FG));
  tgfxReport("cjk_pixels", (long)bus.pixelCount());
  tgfxShot("cjk", gram, W, H);

  // 収録外の文字は何も描かず 0 を返す
  bus.fill(0);
  bus.resetCounters();
  tgfxReport("missing_adv", (long)tgfxU8g2DrawChar(lcd, u8g2_ascii, 'Z', 2, 16, FG));
  tgfxReport("missing_pixels", (long)bus.pixelCount());

  tgfxTestDone();
}
void loop() {}
