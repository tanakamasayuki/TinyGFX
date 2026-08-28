// Construct O-band: the same drawing as o_d, but with one page of buffer.
//
// A monochrome panel without a full framebuffer. The scene is drawn once per
// page with the clip set to that page, and each page is pushed as it is
// finished. 128 bytes of buffer instead of 1,024.
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/FontCell.h>
#include <TinyGFX/PanelSSD1306.h>
#include <tgfx_digits.h>

static const int16_t W = 128, H = 64;
static const int16_t BAND_PAGES = 1;
static uint8_t band[W * BAND_PAGES];

TinyGFXBusI2C bus(Wire, 0x3C);
TinyGFXPanelSSD1306 panel(bus, band, W, H, BAND_PAGES);
TinyGFX lcd(panel);

static const TinyGFXFontRef digitsFont = {&tgfxDigits, &tinygfxFontCellOps};

static void scene(TinyGFX& g) {
  g.fillRect(10, 10, 40, 40, TFT_WHITE);
  g.drawPixel(1, 1, TFT_WHITE);
  g.drawFastHLine(0, 5, 20, TFT_WHITE);
  g.drawFastVLine(5, 0, 20, TFT_WHITE);
  g.drawLine(0, 0, 30, 20, TFT_WHITE);
  g.drawRect(2, 2, 10, 10, TFT_WHITE);
  g.drawCircle(20, 20, 8, TFT_WHITE);
  g.fillCircle(40, 20, 8, TFT_WHITE);
  g.drawRoundRect(5, 40, 30, 20, 4, TFT_WHITE);
  g.fillRoundRect(50, 40, 30, 20, 4, TFT_WHITE);
  g.drawTriangle(0, 60, 20, 90, 40, 60, TFT_WHITE);
  g.fillTriangle(50, 60, 70, 90, 90, 60, TFT_WHITE);
  g.setFont(&digitsFont);
  g.setTextColor(TFT_WHITE);
  g.drawString("12345", 8, 24);
}

void setup() {
  Wire.begin();
  lcd.begin();
  for (int16_t p = 0; p < H / 8; p += BAND_PAGES) {
    panel.setBandPage(p);
    panel.clearBuffer();
    lcd.setClipRect(0, (int16_t)(p * 8), W, (int16_t)(BAND_PAGES * 8));
    scene(lcd);
    panel.display();
  }
  lcd.resetClipRect();
}
void loop() {}
