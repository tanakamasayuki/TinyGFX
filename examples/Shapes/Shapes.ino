// TinyGFX - Shapes
//
// 使えるプリミティブを一通り。
//
// 使わない図形はフラッシュに載らないので、必要なものだけ呼べばよい。
// この例のように全部呼ぶと、CH32V003 でフラッシュが約 4.9KB 増える
// （docs/FOOTPRINT.ja.md の構成 C）。
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/PanelST7789.h>

TinyGFXBusSoftSPI bus(/*sck*/5, /*mosi*/6, /*dc*/3, /*cs*/4);
TinyGFXPanelST7789 panel(bus, 240, 240, /*rst*/2);
TinyGFX lcd(panel);

void setup() {
  lcd.begin();
  lcd.fillScreen(TFT_BLACK);

  lcd.drawLine(0, 0, 239, 239, TFT_DARKGREY);
  lcd.drawRect(10, 10, 60, 40, TFT_WHITE);
  lcd.fillRect(80, 10, 60, 40, TFT_RED);
  lcd.drawRoundRect(150, 10, 60, 40, 8, TFT_YELLOW);
  lcd.fillRoundRect(10, 60, 60, 40, 8, TFT_GREEN);
  lcd.drawCircle(110, 80, 20, TFT_CYAN);
  lcd.fillCircle(180, 80, 20, TFT_MAGENTA);
  lcd.drawTriangle(10, 180, 60, 120, 110, 180, TFT_WHITE);
  lcd.fillTriangle(130, 180, 180, 120, 230, 180, TFT_ORANGE);

  // クリップ矩形の外へは 1 画素も出ない
  lcd.setClipRect(10, 200, 100, 30);
  lcd.fillCircle(60, 215, 60, TFT_BLUE);
  lcd.clearClipRect();
}

void loop() {}
