// 共通シーンをホストで描いてゴールデンを作る。
//
// 実機（tests/hw/m5stack/）はパネルに同じものを描いて読み戻し、ここで作った
// ゴールデンと比べる。**シーンの定義は tgfx_scene.h の 1 箇所だけ。**
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/FontCell.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>
#include <tgfx_scene.h>
#include <tgfx_digits.h>

static const TinyGFXFontRef digitsFont = {&tgfxDigits, &tinygfxFontCellOps};

static uint16_t gram[TGFX_SCENE_W * TGFX_SCENE_H];
TinyGFXBusCapture bus(gram, TGFX_SCENE_W, TGFX_SCENE_H);
TinyGFXPanelST7789 panel(bus, TGFX_SCENE_W, TGFX_SCENE_H);
TinyGFX lcd(panel);

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("scene");
  lcd.begin();
  bus.fill(0);
  lcd.setFont(&digitsFont);
  lcd.setTextColor(TFT_WHITE);
  tgfxGoldenScene(lcd);
  tgfxShot("scene", gram, TGFX_SCENE_W, TGFX_SCENE_H);

  // 実機の読み戻しはバイト一致で比べる。**飽和した色しか使っていないこと**を
  // ここで担保しておく（中間の階調が混ざると往復で 1 LSB ずれる）。
  long unsaturated = 0;
  for (int i = 0; i < TGFX_SCENE_W * TGFX_SCENE_H; ++i) {
    const uint16_t c = gram[i];
    const uint8_t r = (uint8_t)(c >> 11), g5 = (uint8_t)((c >> 5) & 0x3F), b = (uint8_t)(c & 0x1F);
    if ((r != 0 && r != 31) || (g5 != 0 && g5 != 63) || (b != 0 && b != 31)) ++unsaturated;
  }
  tgfxReport("unsaturated", unsaturated);
  tgfxReport("pixels", (long)(TGFX_SCENE_W * TGFX_SCENE_H));
  tgfxTestDone();
}
void loop() {}
