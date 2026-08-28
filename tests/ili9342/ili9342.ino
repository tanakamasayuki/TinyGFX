// ILI9342C パネルの MADCTL・色順・ミラーの組み立て。
//
// ILI9342 は GRAM が最初から横長（320x240）なので、回転 0 が 320x240 になる。
// オフセットのあるモジュールが無いので、ウィンドウは素通しのはず。
//
// **MADCTL の値そのものが実機で正しいかはここでは分からない**（MANUAL_TEST M0）。
// ここが守るのは「実装が表どおりに動くこと」と「setRgbOrder / setMirror が
// 表に正しく効くこと」。
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelILI9342.h>
#include <tgfx_test.h>

static uint16_t gram[32 * 16];
TinyGFXBusCapture bus(gram, 32, 16);
TinyGFXPanelILI9342 panel(bus, 32, 16);
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
  tgfxTestBegin("ili9342");
  lcd.begin();

  for (uint8_t r = 0; r < 4; ++r) probe("bgr", r);   // 既定は BGR

  panel.setRgbOrder(false);
  for (uint8_t r = 0; r < 4; ++r) probe("rgb", r);
  panel.setRgbOrder(true);

  panel.setMirror(true, true);
  for (uint8_t r = 0; r < 4; ++r) probe("flip", r);
  panel.setMirror(false, false);

  // 実際に GRAM へ色が乗ること（ST7789 と同じ 0x2A/0x2B/0x2C を使う）
  panel.setRotation(0);
  lcd.setRotation(0);
  bus.fill(0);
  lcd.fillRect(2, 3, 4, 5, TFT_RED);
  tgfxReport("hit", (long)bus.pixel(3, 4));
  tgfxReport("miss", (long)bus.pixel(1, 3));
  tgfxReport("edge", (long)bus.pixel(5, 7));
  tgfxReport("past", (long)bus.pixel(6, 8));

  tgfxTestDone();
}
void loop() {}
