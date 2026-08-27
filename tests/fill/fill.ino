// 転送量の過不足。ウィンドウ計算がずれると、多く送るか足りなくなる。
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>

static const int W = 40, H = 30;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static void count(const char* name) { tgfxReport2("n", name, (long)bus.pixelCount()); }
static void reset() { bus.fill(0); bus.resetCounters(); }

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("fill");
  lcd.begin();

  reset(); lcd.fillScreen(0x1234);              count("fillscreen");
  reset(); lcd.fillRect(0, 0, 1, 1, 1);         count("r1x1");
  reset(); lcd.fillRect(0, 0, W, 1, 1);         count("rowfull");
  reset(); lcd.fillRect(0, 0, 1, H, 1);         count("colfull");
  reset(); lcd.fillRect(5, 5, 7, 11, 1);        count("r7x11");
  reset(); lcd.drawFastHLine(3, 3, 13, 1);      count("hline13");
  reset(); lcd.drawFastVLine(3, 3, 9, 1);       count("vline9");
  reset(); lcd.fillRect(W - 3, H - 3, 10, 10, 1); count("corner_clip");   // 3x3 のはず
  reset(); lcd.fillRect(-5, -5, 10, 10, 1);     count("topleft_clip");    // 5x5 のはず
  reset(); lcd.setClipRect(4, 4, 8, 8);
           lcd.fillScreen(1); lcd.clearClipRect(); count("clipped_screen"); // 8x8 のはず

  // 回転すると幅と高さが入れ替わるので、全面塗りの画素数は同じ
  reset(); lcd.setRotation(1); lcd.fillScreen(1); count("fillscreen_rot1");
  lcd.setRotation(0);

  tgfxTestDone();
}
void loop() {}
