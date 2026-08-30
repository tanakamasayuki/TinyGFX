#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/panels/SSD1306_128x64.h>
#include <TinyGFX/panels/SSD1306_128x32.h>

TinyGFXBusI2C bus(Wire, 0x3C);
static uint8_t fb[TinyGFXPanelSSD1306_128x64::kBufferBytes];
TinyGFXPanelSSD1306_128x64 panel(bus, fb);
TinyGFX lcd(panel);
void setup() {
  lcd.begin();
  lcd.fillScreen(TFT_BLACK);
  lcd.fillRect(2, 2, 20, 20, TFT_WHITE);
  panel.display();
}
void loop() {}
