// The u8g2-format decoder.
//
// Must match what LGFXFontToolJs draws for the same font and the same string.
// Also shows that a different format goes through setFont just the same.
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/DriverST7789.h>
#include <TinyGFX/FontU8g2.h>
#include <tgfx_test.h>

#include "u8g2_ascii.h"
#include "u8g2_cjk.h"

static const TinyGFXFontRef u8g2AsciiFont = {u8g2Ascii, &tinygfxFontU8g2Ops};
static const TinyGFXFontRef u8g2CjkFont = {u8g2Cjk, &tinygfxFontU8g2Ops};

static const int W = 80, H = 24;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXDriverST7789 panel(bus, W, H);
TinyGFX lcd(panel);

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("u8g2");
  lcd.begin();
  lcd.setTextColor(TFT_WHITE);

  lcd.setFont(&u8g2AsciiFont);
  tgfxReport("line_height", (long)lcd.fontHeight());
  bus.fill(0);
  bus.resetCounters();
  tgfxReport("ascii_width", (long)lcd.drawString("0123456789ABCabc", 2, 4));
  tgfxReport("ascii_measured", (long)lcd.textWidth("0123456789ABCabc"));
  tgfxShot("ascii", gram, W, H);

  lcd.setFont(&u8g2CjkFont);
  bus.fill(0);
  bus.resetCounters();
  tgfxReport("cjk_width", (long)lcd.drawString("日本語表示", 2, 4));
  tgfxShot("cjk", gram, W, H);

  // An uncovered code draws nothing and returns 0
  lcd.setFont(&u8g2AsciiFont);
  bus.fill(0);
  bus.resetCounters();
  tgfxReport("missing_adv", (long)lcd.drawChar('Z', 2, 4));
  tgfxReport("missing_pixels", (long)bus.pixelCount());

  tgfxTestDone();
}
void loop() {}
