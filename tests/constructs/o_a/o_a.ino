#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/PanelSSD1306.h>

static uint8_t fb[128 * 64 / 8];
TinyGFXBusI2C bus(0x3C);
TinyGFXPanelSSD1306 panel(bus, fb, 128, 64);
TinyGFX lcd(panel);

void setup() {
  lcd.begin();
  lcd.fillScreen(TFT_BLACK);
  panel.display();
}
void loop() {}
