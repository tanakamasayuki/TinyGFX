// 文字描画。フォントはスケッチ側（tgfx_font）に置いてある。
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>
#include <tinygfx_font5x7.h>

static const int W = 64, H = 32;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static const uint16_t FG = 0xFFFF;
static const uint16_t BGC = 0x001F;
static void reset() { bus.fill(0); bus.resetCounters(); }

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("text");
  lcd.begin();
  lcd.setFont(&tinygfxFont5x7);
  lcd.setTextColor(FG);

  tgfxReport("font_height", (long)lcd.fontHeight());
  tgfxReport("text_width", (long)lcd.textWidth("12:34"));

  reset();
  tgfxReport("draw_width", (long)lcd.drawString("12:34", 2, 3));
  tgfxShot("plain", gram, W, H);

  // 倍角
  lcd.setTextSize(2);
  tgfxReport("width_x2", (long)lcd.textWidth("12:34"));
  tgfxReport("height_x2", (long)lcd.fontHeight());
  reset();
  lcd.drawString("012", 1, 1);
  tgfxShot("double", gram, W, H);
  lcd.setTextSize(1);

  // 収録されていない文字は何も描かず 0 を返す
  reset();
  tgfxReport("oor_advance", (long)lcd.drawChar('A', 2, 2));
  tgfxReport("oor_pixels", (long)bus.pixelCount());

  // 背景色つき: セル全体が塗られる
  reset();
  lcd.setTextColor(FG, BGC);
  lcd.drawString("12:34", 2, 3);
  tgfxShot("opaque", gram, W, H);
  lcd.setTextColor(FG);

  // 透過: グリフの画素以外は触らない
  reset();
  lcd.drawString("88888", 2, 3);
  tgfxReport("transparent_pixels", (long)bus.pixelCount());
  tgfxShot("transparent", gram, W, H);

  tgfxTestDone();
}
void loop() {}
