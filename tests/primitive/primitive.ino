// The primitives, weighted towards the last pixel at each edge and the
// degenerate cases.
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/DriverST7789.h>
#include <tgfx_test.h>

static const int W = 64, H = 64;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXDriverST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static const uint16_t FG = 0xFFFF;

static void shot(const char* name) { tgfxShot(name, gram, W, H); }
static void clearGram() { bus.fill(0); bus.resetCounters(); }
static void degenerate(const char* name) { tgfxReport2("degen", name, (long)bus.pixelCount()); }

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("primitive");
  lcd.begin();

  clearGram(); lcd.drawRect(8, 8, 16, 12, FG);             shot("drawrect");
  clearGram(); lcd.drawFastHLine(4, 30, 20, FG);           shot("hline");
  clearGram(); lcd.drawFastVLine(30, 4, 20, FG);           shot("vline");
  clearGram(); lcd.drawLine(2, 2, 40, 30, FG);             shot("line");
  clearGram(); lcd.drawCircle(32, 32, 20, FG);             shot("circle");
  clearGram(); lcd.fillCircle(32, 32, 20, FG);             shot("fillcircle");
  clearGram(); lcd.drawRoundRect(6, 6, 40, 30, 8, FG);     shot("roundrect");
  clearGram(); lcd.fillRoundRect(6, 6, 40, 30, 8, FG);     shot("fillroundrect");
  clearGram(); lcd.drawTriangle(4, 60, 32, 4, 60, 60, FG); shot("triangle");
  clearGram(); lcd.fillTriangle(4, 60, 32, 4, 60, 60, FG); shot("filltriangle");
  clearGram(); lcd.fillRect(10, 10, 1, 1, FG);             shot("onepixel");

  // --- degenerate: not one pixel may be sent --------------------------------
  clearGram(); lcd.fillRect(10, 10, 0, 5, FG);     degenerate("w0");
  clearGram(); lcd.fillRect(10, 10, 5, 0, FG);     degenerate("h0");
  clearGram(); lcd.fillRect(10, 10, -5, 5, FG);    degenerate("wneg");
  clearGram(); lcd.drawFastHLine(10, 10, 0, FG);   degenerate("hline0");
  clearGram(); lcd.drawRect(10, 10, 0, 0, FG);     degenerate("rect0");
  clearGram(); lcd.fillRect(-100, -100, 5, 5, FG); degenerate("offscreen");
  clearGram(); lcd.fillRect(W + 10, 0, 5, 5, FG);  degenerate("rightout");
  clearGram(); lcd.drawPixel(-1, 0, FG);           degenerate("pixelneg");
  clearGram(); lcd.drawPixel(W, 0, FG);            degenerate("pixelover");
  clearGram(); lcd.drawCircle(32, 32, -1, FG);     degenerate("rneg");

  // --- degenerate, but pixels do come out -----------------------------------
  clearGram(); lcd.drawCircle(32, 32, 0, FG);      degenerate("r0");
  clearGram(); lcd.drawLine(5, 5, 5, 5, FG);       degenerate("dot_line");

  // --- does what hangs off the edge get cropped -----------------------------
  clearGram(); lcd.fillRect(-4, -4, 10, 10, FG);   degenerate("clipped_rect");
  shot("clipped_rect");

  tgfxTestDone();
}
void loop() {}
