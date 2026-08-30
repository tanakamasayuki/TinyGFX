// TinyGFX - Shapes
//
// One of each primitive.
//
// Shapes you never call are not in the flash image, so call only what you
// need. Calling every one of them, as this example does, adds about 4.9 KB on
// a CH32V003 (construct C in docs/FOOTPRINT.ja.md).
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/panels/ST7789_240x240.h>

TinyGFXBusSoftSPI bus(/*sck*/5, /*mosi*/6, /*dc*/3, /*cs*/4);
TinyGFXPanelST7789_240x240 panel(bus, /*rst*/2);
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

  // Not one pixel escapes the clip rectangle
  lcd.setClipRect(10, 200, 100, 30);
  lcd.fillCircle(60, 215, 60, TFT_BLUE);
  lcd.clearClipRect();
}

void loop() {}
