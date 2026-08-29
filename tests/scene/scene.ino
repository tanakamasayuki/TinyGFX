// Draws the shared scene on the host to make the golden.
//
// The hardware side (tests/hw/m5stack/) draws the same thing on a panel, reads
// it back and compares against this golden. **The scene is defined in exactly
// one place, tgfx_scene.h.**
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

  // The hardware read-back is compared byte for byte, so guarantee here that
  // **only saturated colours are used** - an intermediate level comes back
  // 1 LSB off through the round trip.
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
