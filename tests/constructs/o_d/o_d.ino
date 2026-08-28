#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/PanelSSD1306.h>
#include <TinyGFX/FontCell.h>
#include <tinygfx_font5x7.h>

static const TinyGFXFontRef font5x7 = {&tinygfxFont5x7, &tinygfxFontCellOps, nullptr};


static uint8_t fb[128 * 64 / 8];
TinyGFXBusI2C bus(Wire, 0x3C);
TinyGFXPanelSSD1306 panel(bus, fb, 128, 64);
TinyGFX lcd(panel);

void setup() {
  Wire.begin();  // the sketch owns the bus; TinyGFX never begins it
  lcd.begin();
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
  lcd.setFont(&font5x7);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("0123456789", 0, 40);
  panel.display();
}
void loop() {}
