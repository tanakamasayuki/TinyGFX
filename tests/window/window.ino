// How rotation and the origin offset come out in CASET / RASET / MADCTL.
//
// Rotation is done by the controller's MADCTL by design (docs/DECISIONS.ja.md
// D7), so what can be checked in software is three things: that width and
// height swap, the MADCTL value, and that the offset reaches the window.
// Whether that value is right can only be found out on hardware (MANUAL_TEST M2).
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/DriverST7789.h>
#include <tgfx_test.h>

static uint16_t gram[8 * 8];
// Models a 135x240 ST7789 module: a 240x320 GRAM, visible from (52, 40)
TinyGFXBusCapture bus(gram, 8, 8);
TinyGFXDriverST7789 panel(bus, 135, 240);
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

  // On a panel with no offset it must stay 0 at every rotation
  panel.setGramSize(135, 240);
  panel.setOffset(0, 0);
  for (uint8_t r = 0; r < 4; ++r) probe("zero", r);

  // Rotating must also reset the clip to the new size
  panel.setRotation(0);
  lcd.setRotation(1);
  tgfxReport("clip_w", (long)lcd.width());
  tgfxReport("clip_h", (long)lcd.height());
  lcd.setRotation(0);

  // --- the setters must not depend on call order ---------------------------
  //
  // **Stops the P0 found in the 2026-08-29 design review from coming back.**
  // The offset was only derived inside setRotation(), so calling setGramSize()
  // or setOffset() after begin() had no effect at all - and a sketch that never
  // rotates would never get one. The probe() above goes through setRotation()
  // every time, which is how this path stayed unexamined.
  {
    TinyGFXDriverST7789 late(bus, 135, 240);
    TinyGFX g(late);
    g.begin();                     // this gets as far as setRotation(0)
    late.setGramSize(240, 320);
    late.setOffset(52, 40);
    g.setAddrWindow(0, 0, 1, 1);   // **no setRotation in between**
    tgfxReport("late_xs", (long)bus.windowXs());
    tgfxReport("late_ys", (long)bus.windowYs());
  }
  {
    // The same in the other order (whichever comes second must not derive
    // from the older pair)
    TinyGFXDriverST7789 swap(bus, 135, 240);
    TinyGFX g(swap);
    g.begin();
    swap.setOffset(52, 40);
    swap.setGramSize(240, 320);
    g.setAddrWindow(0, 0, 1, 1);
    tgfxReport("swap_xs", (long)bus.windowXs());
    tgfxReport("swap_ys", (long)bus.windowYs());
  }
  {
    // Calling them before begin() must work too (they touch no bus, so the
    // order is free)
    TinyGFXDriverST7789 early(bus, 135, 240);
    TinyGFX g(early);
    early.setGramSize(240, 320);
    early.setOffset(52, 40);
    g.begin();
    g.setAddrWindow(0, 0, 1, 1);
    tgfxReport("early_xs", (long)bus.windowXs());
    tgfxReport("early_ys", (long)bus.windowYs());
  }

  tgfxTestDone();
}
void loop() {}
