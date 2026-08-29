// The clipping invariant:
//   inside the clip, not one pixel differs from drawing without a clip
//   outside it, not one pixel is touched
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>

static const int W = 64, H = 64;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static const uint16_t BG = 0x0000;

// The clip rectangle (keep in step with the test)
static const int CX = 10, CY = 12, CW = 30, CH = 26;

static void scene() {
  lcd.fillCircle(32, 32, 26, 0x001F);
  lcd.drawLine(0, 0, 63, 63, 0xF800);
  lcd.fillRect(0, 0, 64, 4, 0x07E0);
  lcd.drawRect(2, 2, 60, 60, 0xFFE0);
  lcd.fillTriangle(0, 63, 20, 20, 63, 50, 0xF81F);
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("clip");
  lcd.begin();

  bus.fill(BG);
  bus.resetCounters();
  scene();
  tgfxReport("free_pixels", (long)bus.pixelCount());
  tgfxShot("free", gram, W, H);

  bus.fill(BG);
  bus.resetCounters();
  lcd.setClipRect(CX, CY, CW, CH);
  scene();
  lcd.clearClipRect();
  tgfxReport("clipped_pixels", (long)bus.pixelCount());
  tgfxShot("clipped", gram, W, H);

  // A clip never extends past the screen
  bus.fill(BG);
  bus.resetCounters();
  lcd.setClipRect(-20, -20, 200, 200);
  lcd.fillScreen(0x07FF);
  lcd.clearClipRect();
  tgfxReport("oversize_clip_pixels", (long)bus.pixelCount());

  // A clip of width 0 draws nothing
  bus.fill(BG);
  bus.resetCounters();
  lcd.setClipRect(10, 10, 0, 0);
  scene();
  lcd.clearClipRect();
  tgfxReport("empty_clip_pixels", (long)bus.pixelCount());

  // --- extreme coordinates must still clip and draw ------------------------
  //
  // **P1 of the 2026-08-29 design review.** The far edge was computed in
  // int16_t, so x + w - 1 overflowed: with x=2 and w=32767, 32768 wraps to
  // -32768, "x > x1" fires and **a rectangle that should have covered the
  // whole screen vanished instead.** Exhaustively, 28,441 (x, w) pairs
  // disagreed.
  //
  // Taking coordinates from outside the screen and clipping them is part of
  // the contract, so that contract was being broken.
  {
    bus.fill(BG);
    bus.resetCounters();
    lcd.fillRect(2, 2, 32767, 32767, TFT_WHITE);   // enormous, starting on screen
    tgfxReport("huge_rect_pixels", (long)bus.pixelCount());
  }
  {
    bus.fill(BG);
    bus.resetCounters();
    lcd.fillRect(-32768, -32768, 32767, 32767, TFT_WHITE);  // from far off the top left
    tgfxReport("far_negative_pixels", (long)bus.pixelCount());
  }
  {
    bus.fill(BG);
    bus.resetCounters();
    lcd.fillRect(30000, 30000, 30000, 30000, TFT_WHITE);    // wholly past the bottom right
    tgfxReport("far_positive_pixels", (long)bus.pixelCount());
  }
  {
    // The same overflow, this time in the clip rectangle
    bus.fill(BG);
    bus.resetCounters();
    lcd.setClipRect(2, 2, 32767, 32767);
    lcd.fillScreen(TFT_WHITE);
    lcd.clearClipRect();
    tgfxReport("huge_clip_pixels", (long)bus.pixelCount());
  }

  tgfxTestDone();
}
void loop() {}
