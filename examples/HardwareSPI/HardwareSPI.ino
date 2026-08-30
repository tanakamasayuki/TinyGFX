// TinyGFX - HardwareSPI
//
// The hardware SPI version. Only the bus declaration changes; the drawing
// code is identical.
//
// The bus belongs to the sketch. Call SPI.begin() yourself - with whatever
// pins your board needs - before lcd.begin(). TinyGFX never begins or ends the
// SPI bus, which is what lets an SD card share the same wires: both sides just
// use beginTransaction / endTransaction around their own bursts.
//
// Some CH32V003 cores cannot use hardware SPI at all today; use BusSoftSPI
// there instead, as in the HelloWorld example.
#include <TinyGFX.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/panels/ST7789_240x240.h>

TinyGFXBusSPI bus(SPI, /*dc*/3, /*cs*/4, /*freq*/24000000UL);
TinyGFXPanelST7789_240x240 panel(bus, /*rst*/2);
TinyGFX lcd(panel);

void setup() {
  SPI.begin();  // the sketch owns the bus; TinyGFX never begins it
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
