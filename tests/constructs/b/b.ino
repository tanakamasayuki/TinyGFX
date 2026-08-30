#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/DriverST7789.h>

TinyGFXBusSoftSPI bus(/*sck*/5, /*mosi*/6, /*dc*/3, /*cs*/4);
TinyGFXDriverST7789 panel(bus, 240, 240, /*rst*/2);
TinyGFX lcd(panel);

void setup() {
  lcd.begin();
  lcd.fillScreen(TFT_BLACK);
  lcd.fillRect(10, 10, 40, 40, TFT_RED);
  lcd.drawPixel(1, 1, TFT_WHITE);
  lcd.drawFastHLine(0, 5, 20, TFT_RED);
  lcd.drawFastVLine(5, 0, 20, TFT_RED);
}
void loop() {}
