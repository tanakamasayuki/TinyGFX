// TinyGFX - Hello World
//
// ST7789 に矩形と文字を出すだけの最小例。
//
// バスはソフト SPI（pinMode / digitalWrite だけ）。どの Arduino Core でも動く。
// ハードウェア SPI が使える環境なら HardwareSPI の例を見ること。
//
// フォントはこのスケッチに同梱している。TinyGFX はフォントデータを持たない。
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/PanelST7789.h>

#include "tinygfx_font5x7.h"

// 配線に合わせて変える
static const int8_t PIN_SCK = 5;
static const int8_t PIN_MOSI = 6;
static const int8_t PIN_DC = 3;
static const int8_t PIN_CS = 4;
static const int8_t PIN_RST = 2;  // 無ければ -1

TinyGFXBusSoftSPI bus(PIN_SCK, PIN_MOSI, PIN_DC, PIN_CS);
TinyGFXPanelST7789 panel(bus, 240, 240, PIN_RST);
TinyGFX lcd(panel);

void setup() {
  lcd.begin();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BLACK);

  lcd.fillRect(8, 8, 100, 24, TFT_NAVY);
  lcd.drawRect(8, 8, 100, 24, TFT_WHITE);

  lcd.setFont(&tinygfxFont5x7);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.drawString("12:34", 16, 14);
}

void loop() {}
