// TinyGFX - M5StackBasic
//
// A hardware bring-up sketch. It puts everything worth checking onto the
// built-in LCD of an M5Stack Core / BASIC (ILI9342C, 320x240) in one screen.
//
// No wiring, fixed pins and solid power make this the surest first thing to
// try on real hardware (docs/MANUAL_TEST.ja.md, M0).
//
// What you should see:
//   1. A one-pixel white border, complete on all four sides
//   2. Red, green and blue bars across the top, in that order
//   3. 0123456789 in the middle, neither mirrored nor upside down
//   4. A circle, a triangle and some lines below that
//
// If something is off, each fix is one line:
//   - Nothing at all      -> is the backlight (GPIO32) driven high?
//   - Black on white      -> panel.invertDisplay(false); AFTER lcd.begin()
//   - Bars read blue-green-red -> panel.setRgbOrder(false);
//   - Digits mirrored or upside down -> panel.setMirror(true, false) or
//     similar, BEFORE lcd.begin()
//   - Edges clipped or shifted -> check the 320x240 constructor arguments
#include <TinyGFX.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/PanelILI9342.h>

#include <TinyGFX/FontCell.h>
#include "tgfx_clock.h"

static const TinyGFXFontRef clockFont = {&tgfxClock, &tinygfxFontCellOps, nullptr};

// The built-in LCD of an M5Stack Core / BASIC
static const int8_t PIN_DC = 27;
static const int8_t PIN_CS = 14;
static const int8_t PIN_RST = 33;
static const int8_t PIN_BL = 32;   // backlight; nothing is visible until this is high
static const int8_t PIN_SD_CS = 4; // the SD card shares this SPI bus; keep it deselected

// SCK and MOSI are not given here: the core's SPI library uses the M5Stack
// defaults (18 and 23). 40 MHz works, but start slower.
TinyGFXBusSPI bus(SPI, PIN_DC, PIN_CS, /*freq*/ 24000000UL);
TinyGFXPanelILI9342 panel(bus, 320, 240, PIN_RST);
TinyGFX lcd(panel);

void setup()
{
  Serial.begin(115200);

  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);

  SPI.begin();  // the sketch owns the bus; TinyGFX never begins it
  lcd.begin();

  // Older M5Stack BASIC units come out inverted (confirmed on hardware).
  // Uncomment this line when yours does.
  //
  // It has to sit after lcd.begin(). Before it, two separate things stop it
  // working:
  //   1. the bus is not up yet - neither SPI.begin() nor pinMode has run
  //   2. even if the bytes got out, init() sends INVON last and overwrites it
  //
  // panel.invertDisplay(false);

  const int16_t w = lcd.width();
  const int16_t h = lcd.height();

  lcd.startWrite();
  lcd.fillScreen(TFT_BLACK);

  // 1. Does it reach the edges?
  lcd.drawRect(0, 0, w, h, TFT_WHITE);

  // 2. Colour order: red, green, blue from the left
  const int16_t bar = (int16_t)((w - 8) / 3);
  lcd.fillRect(4, 12, bar, 40, TFT_RED);
  lcd.fillRect((int16_t)(4 + bar), 12, bar, 40, TFT_GREEN);
  lcd.fillRect((int16_t)(4 + bar * 2), 12, bar, 40, TFT_BLUE);

  // 3. Orientation. Mirrored or upside down shows instantly in the digits
  lcd.setFont(&clockFont);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(3);
  const char *digits = "0123456789";
  lcd.drawString(digits, (int16_t)((w - lcd.textWidth(digits)) / 2), 66);

  // 4. The basic shapes
  lcd.fillCircle(64, 172, 32, TFT_YELLOW);
  lcd.drawCircle(64, 172, 40, TFT_WHITE);
  lcd.fillTriangle(160, 132, 128, 212, 192, 212, TFT_CYAN);
  for (int16_t i = 0; i <= 60; i += 10)
  {
    lcd.drawLine(232, (int16_t)(132 + i), 292, (int16_t)(212 - i), TFT_MAGENTA);
  }
  lcd.endWrite();

  Serial.println();
  Serial.println(F("TinyGFX M5Stack bring-up"));
  Serial.print(F("  size   : "));
  Serial.print(w);
  Serial.print('x');
  Serial.println(h);
  Serial.println(F("  expect : white border / R-G-B bars / 0123456789 / circle+triangle+lines"));
  Serial.println(F("  dark      -> backlight GPIO32"));
  Serial.println(F("  inverted  -> panel.invertDisplay(false) AFTER lcd.begin()"));
  Serial.println(F("  B-G-R     -> panel.setRgbOrder(false)"));
  Serial.println(F("  mirrored  -> panel.setMirror(x, y) before lcd.begin()"));
}

void loop() {}
