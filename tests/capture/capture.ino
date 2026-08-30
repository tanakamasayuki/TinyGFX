// The Tier 1 spike: can BusCapture rebuild a picture from an ST7789 command
// stream?
//
// Nothing else in the drawing tests can be written until this passes.
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/DriverST7789.h>
#include <tgfx_test.h>

static const int W = 64;
static const int H = 64;
static uint16_t gram[W * H];

TinyGFXBusCapture bus(gram, W, H);
TinyGFXDriverST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static void scene(const char* name) {
  tgfxReport2(name, "pixels", (long)bus.pixelCount());
  tgfxShot(name, gram, W, H);
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("capture");

  lcd.begin();
  tgfxReport("init_commands", (long)bus.commandCount());

  // 1) a full fill: exactly W*H pixels
  bus.fill(0);
  bus.resetCounters();
  lcd.fillScreen(0x001F);
  scene("fillscreen");

  // 2) a rectangle: exactly 8*8 pixels
  bus.fill(0);
  bus.resetCounters();
  lcd.fillRect(4, 4, 8, 8, 0xF800);
  scene("fillrect");

  // 3) one pixel
  bus.fill(0);
  bus.resetCounters();
  lcd.drawPixel(9, 3, 0x07E0);
  scene("pixel");

  // 4) the window values (are CASET / RASET coming out right?)
  bus.resetCounters();
  lcd.setAddrWindow(3, 5, 10, 7);
  tgfxReport("win_xs", (long)bus.windowXs());
  tgfxReport("win_ys", (long)bus.windowYs());
  tgfxReport("win_xe", (long)bus.windowXe());
  tgfxReport("win_ye", (long)bus.windowYe());

  // 5) does the panel's origin offset reach the window?
  panel.setOffset(2, 1);
  panel.setRotation(0);
  lcd.setAddrWindow(0, 0, 4, 4);
  tgfxReport("off_xs", (long)bus.windowXs());
  tgfxReport("off_ys", (long)bus.windowYs());
  panel.setOffset(0, 0);
  panel.setRotation(0);

  // 6) has the startWrite / endWrite nesting unwound?
  tgfxReport("txn_depth", (long)bus.txnDepth());

  tgfxTestDone();
}

void loop() {}
