// TinyGFX - OledI2C
//
// A monochrome OLED over I2C (SSD1306, 128x64).
//
// Two things differ from a colour SPI panel.
//   1. It needs a framebuffer - 1,024 bytes for 128x64, supplied by you
//   2. Nothing reaches the screen until display(), and only the pages that
//      changed are sent
//
// The drawing API is the same as for a colour panel; colours collapse to 1bpp
// as "non-zero lights up".
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/PanelSSD1306.h>

#include <TinyGFX/FontCell.h>
#include "tgfx_clock.h"

static const TinyGFXFontRef clockFont = {&tgfxClock, &tinygfxFontCellOps};

static const int16_t WIDTH = 128;
static const int16_t HEIGHT = 64;

// The framebuffer. A 128x32 panel needs half of this (512 bytes).
static uint8_t framebuffer[WIDTH * HEIGHT / 8];

TinyGFXBusI2C bus(Wire, /*address*/ 0x3C);
TinyGFXPanelSSD1306 panel(bus, framebuffer, WIDTH, HEIGHT);
TinyGFX lcd(panel);

void setup() {
  Wire.begin();  // the sketch owns the bus; TinyGFX never begins it
  lcd.begin();

  lcd.drawRect(0, 0, WIDTH, HEIGHT, TFT_WHITE);
  lcd.fillRect(8, 8, 40, 16, TFT_WHITE);
  lcd.drawCircle(96, 32, 20, TFT_WHITE);

  lcd.setFont(&clockFont);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("12:34", 12, 40);

  panel.display();   // nothing was sent until this line
}

void loop() {}
