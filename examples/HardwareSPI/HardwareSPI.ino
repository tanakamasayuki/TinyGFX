// TinyGFX - HardwareSPI
//
// The hardware SPI version. Only the bus declaration changes; the drawing
// code is identical.
//
// Note that SCK and MOSI are not given here - they are left to the core's SPI
// library, because some cores have an SPI.begin() that takes no pins. To
// choose them yourself, call SPI.begin(...) before lcd.begin() and pass false
// for the constructor's initSpi.
//
// Some CH32V003 cores cannot use hardware SPI at all today; use BusSoftSPI
// there instead, as in the HelloWorld example.
#include <TinyGFX.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/PanelST7789.h>

TinyGFXBusSPI bus(/*dc*/3, /*cs*/4, /*freq*/24000000UL);
TinyGFXPanelST7789 panel(bus, 240, 240, /*rst*/2);
TinyGFX lcd(panel);

void setup() {
  lcd.begin();
  lcd.fillScreen(TFT_BLACK);

  // Wrapping a run of drawing in startWrite / endWrite cuts the CS toggling
  lcd.startWrite();
  for (int16_t i = 0; i < 120; i += 8) {
    lcd.drawRect(i, i, 240 - i * 2, 240 - i * 2, lcd.color565(i, 255 - i, 128));
  }
  lcd.endWrite();
}

void loop() {}
