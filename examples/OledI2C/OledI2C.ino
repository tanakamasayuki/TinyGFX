// TinyGFX - OledI2C
//
// I2C のモノクロ OLED（SSD1306 128x64）。
//
// SPI のカラーパネルとの違いは 2 つ。
//   1. **フレームバッファが要る。** 128x64 で 1,024 バイト。利用者が用意する
//   2. **display() を呼ぶまで画面は変わらない。** 変更のあったページだけ流す
//
// 描画 API はカラーパネルと同じ。色は「0 でなければ点灯」で 1bpp に落ちる。
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/PanelSSD1306.h>

#include "tinygfx_font5x7.h"

static const int16_t WIDTH = 128;
static const int16_t HEIGHT = 64;

// フレームバッファ。128x32 のパネルなら半分（512 バイト）で済む。
static uint8_t framebuffer[WIDTH * HEIGHT / 8];

TinyGFXBusI2C bus(/*address*/ 0x3C);
TinyGFXPanelSSD1306 panel(bus, framebuffer, WIDTH, HEIGHT);
TinyGFX lcd(panel);

void setup() {
  lcd.begin();

  lcd.drawRect(0, 0, WIDTH, HEIGHT, TFT_WHITE);
  lcd.fillRect(8, 8, 40, 16, TFT_WHITE);
  lcd.drawCircle(96, 32, 20, TFT_WHITE);

  lcd.setFont(&tinygfxFont5x7);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("12:34", 12, 40);

  panel.display();   // ここで初めて転送される
}

void loop() {}
