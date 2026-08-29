// How an ILI9342C assembles MADCTL, colour order and mirroring.
//
// The ILI9342's GRAM is landscape from the start (320x240), so rotation 0 is
// 320x240. No module has an origin offset, so the window should pass through.
//
// **Whether that MADCTL is right on real glass cannot be known here**
// (MANUAL_TEST M0). What this holds is that the implementation follows the
// table, and that setRgbOrder / setMirror act on it correctly.
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

  for (uint8_t r = 0; r < 4; ++r) probe("bgr", r);   // BGR is the default

  panel.setRgbOrder(false);
  for (uint8_t r = 0; r < 4; ++r) probe("rgb", r);
  panel.setRgbOrder(true);

  panel.setMirror(true, true);
  for (uint8_t r = 0; r < 4; ++r) probe("flip", r);
  panel.setMirror(false, false);

  // Colour really reaches the GRAM (through the ST7789's own 0x2A/0x2B/0x2C)
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
