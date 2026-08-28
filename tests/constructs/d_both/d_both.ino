#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/PanelST7789.h>
#include <TinyGFX/FontCell.h>
#include <tinygfx_font5x7.h>

static const TinyGFXFontRef font5x7 = {&tinygfxFont5x7, &tinygfxFontCellOps, nullptr};
#include <TinyGFX/FontU8g2.h>
#include <u8g2_ascii.h>

TinyGFXBusSoftSPI bus(/*sck*/5, /*mosi*/6, /*dc*/3, /*cs*/4);
TinyGFXPanelST7789 panel(bus, 240, 240, /*rst*/2);
TinyGFX lcd(panel);

void setup() {
  lcd.begin();
  lcd.fillScreen(TFT_BLACK);
  lcd.fillRect(10, 10, 40, 40, TFT_RED);
  lcd.drawPixel(1, 1, TFT_WHITE);
  lcd.drawFastHLine(0, 5, 20, TFT_RED);
  lcd.drawFastVLine(5, 0, 20, TFT_RED);
  lcd.drawLine(0, 0, 30, 20, TFT_GREEN);
  lcd.drawRect(2, 2, 10, 10, TFT_BLUE);
  lcd.drawCircle(20, 20, 8, TFT_CYAN);
  lcd.fillCircle(40, 20, 8, TFT_CYAN);
  lcd.drawRoundRect(5, 40, 30, 20, 4, TFT_YELLOW);
  lcd.fillRoundRect(50, 40, 30, 20, 4, TFT_YELLOW);
  lcd.drawTriangle(0, 60, 20, 90, 40, 60, TFT_WHITE);
  lcd.fillTriangle(50, 60, 70, 90, 90, 60, TFT_WHITE);
  lcd.setTextColor(TFT_WHITE);
  lcd.setFont(&font5x7);
  lcd.drawString("0123456789", 0, 100);
  lcd.setFont(&u8g2_ascii);
  lcd.drawString("0123456789", 0, 110);
}
void loop() {}
