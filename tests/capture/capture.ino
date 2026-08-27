// Tier 1 スパイク: BusCapture が ST7789 のコマンド列から画を復元できるか。
//
// これが通らないと以降の描画テストが全部書けない。
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>

static const int W = 64;
static const int H = 64;
static uint16_t gram[W * H];

TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
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

  // 1) 全面塗り: ちょうど W*H 画素のはず
  bus.fill(0);
  bus.resetCounters();
  lcd.fillScreen(0x001F);
  scene("fillscreen");

  // 2) 矩形: ちょうど 8*8 画素
  bus.fill(0);
  bus.resetCounters();
  lcd.fillRect(4, 4, 8, 8, 0xF800);
  scene("fillrect");

  // 3) 1 画素
  bus.fill(0);
  bus.resetCounters();
  lcd.drawPixel(9, 3, 0x07E0);
  scene("pixel");

  // 4) ウィンドウの値（CASET / RASET が正しく出ているか）
  bus.resetCounters();
  lcd.setAddrWindow(3, 5, 10, 7);
  tgfxReport("win_xs", (long)bus.windowXs());
  tgfxReport("win_ys", (long)bus.windowYs());
  tgfxReport("win_xe", (long)bus.windowXe());
  tgfxReport("win_ye", (long)bus.windowYe());

  // 5) パネル原点オフセットがウィンドウに乗るか
  panel.setOffset(2, 1);
  panel.setRotation(0);
  lcd.setAddrWindow(0, 0, 4, 4);
  tgfxReport("off_xs", (long)bus.windowXs());
  tgfxReport("off_ys", (long)bus.windowYs());
  panel.setOffset(0, 0);
  panel.setRotation(0);

  // 6) startWrite / endWrite の入れ子が戻っているか
  tgfxReport("txn_depth", (long)bus.txnDepth());

  tgfxTestDone();
}

void loop() {}
