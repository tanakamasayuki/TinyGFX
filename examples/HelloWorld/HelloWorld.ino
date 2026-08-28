// TinyGFX - Hello World
//
// The smallest thing that puts a rectangle and some text on an ST7789.
//
// The bus is software SPI - nothing but pinMode and digitalWrite - so it runs
// on every Arduino core. Where hardware SPI is available, see the HardwareSPI
// example instead.
//
// The font is bundled with this sketch; TinyGFX ships no font data itself.
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/PanelST7789.h>

#include <TinyGFX/FontCell.h>
#include "tgfx_clock.h"

static const TinyGFXFontRef clockFont = {&tgfxClock, &tinygfxFontCellOps, nullptr};

// Change these to match your wiring
static const int8_t PIN_SCK = 5;
static const int8_t PIN_MOSI = 6;
static const int8_t PIN_DC = 3;
static const int8_t PIN_CS = 4;
static const int8_t PIN_RST = 2;  // -1 if the module has no reset pin

TinyGFXBusSoftSPI bus(PIN_SCK, PIN_MOSI, PIN_DC, PIN_CS);
TinyGFXPanelST7789 panel(bus, 240, 240, PIN_RST);
TinyGFX lcd(panel);

void setup() {
  lcd.begin();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BLACK);

  lcd.fillRect(8, 8, 100, 24, TFT_NAVY);
  lcd.drawRect(8, 8, 100, 24, TFT_WHITE);

  lcd.setFont(&clockFont);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.drawString("12:34", 16, 14);
}

void loop() {}
