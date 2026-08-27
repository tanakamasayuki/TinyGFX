// 回転と原点オフセットが CASET / RASET / MADCTL にどう出るか。
//
// 回転はコントローラの MADCTL でやる設計（docs/DECISIONS.ja.md D7）なので、
// ソフト側で確かめられるのは「幅と高さが入れ替わるか」「MADCTL の値」
// 「ウィンドウにオフセットが正しく乗るか」の 3 つ。
// MADCTL の値そのものが正しいかは実機でしか分からない（MANUAL_TEST M2）。
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>

static uint16_t gram[8 * 8];
// 135x240 の ST7789 モジュールを模す: GRAM 240x320、可視域は (52, 40) から
TinyGFXBusCapture bus(gram, 8, 8);
TinyGFXPanelST7789 panel(bus, 135, 240);
TinyGFX lcd(panel);

static void probe(const char* prefix, uint8_t r) {
  char key[16];
  snprintf(key, sizeof(key), "%s%d", prefix, (int)r);
  panel.setRotation(r);
  tgfxReport2(key, "madctl", (long)bus.lastCommandArg());
  tgfxReport2(key, "w", (long)lcd.width());
  tgfxReport2(key, "h", (long)lcd.height());
  lcd.setAddrWindow(0, 0, 1, 1);
  tgfxReport2(key, "xs", (long)bus.windowXs());
  tgfxReport2(key, "ys", (long)bus.windowYs());
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("window");
  lcd.begin();

  panel.setGramSize(240, 320);
  panel.setOffset(52, 40);
  for (uint8_t r = 0; r < 4; ++r) probe("rot", r);

  // オフセットなしのパネルでは全回転で 0 のままであること
  panel.setGramSize(135, 240);
  panel.setOffset(0, 0);
  for (uint8_t r = 0; r < 4; ++r) probe("zero", r);

  // 回転すると clip も新しい大きさに戻ること
  panel.setRotation(0);
  lcd.setRotation(1);
  tgfxReport("clip_w", (long)lcd.width());
  tgfxReport("clip_h", (long)lcd.height());
  lcd.setRotation(0);

  tgfxTestDone();
}
void loop() {}
