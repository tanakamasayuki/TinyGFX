// **First light on a new panel.** Three steps at the top, then look at the
// glass. Each one is marked STEP n of 3.
//
// Every symptom in docs/PANEL_TUNING.ja.md 2 is meant to be visible in one
// picture, so that the answer to "what is wrong with it" is a glance rather
// than a debugging session:
//
//   the 1px border      is the whole glass addressed, and where does it start
//   the corner block    which corner is the origin (mirroring, rotation)
//   the diagonal        orientation, and whether both axes are the right length
//   the colour bars     colour order (RGB or BGR)  [colour panels only]
//   inverted or not     black background, white border
//
// Serial says what the sketch thinks it is driving, so **a blank screen still
// tells you something**: if the banner prints, the sketch is alive and the
// problem is the panel, the wiring or the bus.
//
// This is a manual sketch. pytest only compiles it (tests/build_matrix/).

// ===========================================================================
// STEP 1 of 3 - the panel. **Change this one line.**
//
// Pick a header from src/TinyGFX/panels/. Including it also names it, as
// `TINYGFX_PANEL`, so the class name is never written twice.
// ===========================================================================
#include <TinyGFX/panels/ST7789_240x240.h>

// ===========================================================================
// STEP 2 of 3 - the bus. **Exactly one line without `//` in front.**
//
// The one that is live now is USE_SPI. To use I2C instead, put `//` in front
// of USE_SPI and take the `//` off USE_I2C:
//
//     // #define USE_SPI
//     #define USE_I2C
//
//   USE_SPI       the board's hardware SPI. Fastest, fixed SCK/MOSI pins
//   USE_SOFT_SPI  bit-banged on any four pins. Works where a core has no SPI
//   USE_I2C       two wires. **Monochrome OLEDs (SSD1306 / SH1106) only**
// ===========================================================================
#define USE_SPI
// #define USE_SOFT_SPI
// #define USE_I2C

// ===========================================================================
// STEP 3 of 3 - the pins. **Only the ones your bus uses matter.**
//
//   USE_SPI       PIN_DC, PIN_CS, PIN_RST    (SCK and MOSI are the board's)
//   USE_SOFT_SPI  the same, plus PIN_SCK and PIN_MOSI
//   USE_I2C       I2C_ADDR only              (SDA and SCL are the board's)
//
// The numbers below are for an ESP32 dev board. **A module with no reset pin
// takes PIN_RST = -1**, and TinyGFX then leaves reset alone.
// ===========================================================================
static const int8_t PIN_DC = 2;
static const int8_t PIN_CS = 15;
static const int8_t PIN_RST = 4;                   // -1 if the module has none
static const int8_t PIN_SCK = 18;                  // USE_SOFT_SPI only
static const int8_t PIN_MOSI = 23;                 // USE_SOFT_SPI only
static const uint8_t I2C_ADDR = 0x3C;              // 0x3D on some modules

// ===========================================================================
// Nothing below here needs editing.
// ===========================================================================

#include <TinyGFX.h>

// A page-addressed panel owns a framebuffer and sends nothing until display().
#if defined(TINYGFX_DRIVER_SSD1306_INCLUDED) || defined(TINYGFX_DRIVER_SH1106_INCLUDED)
#define PAGED 1
static uint8_t fb[TINYGFX_PANEL::kBufferBytes];
#else
#define PAGED 0
#endif

#if defined(USE_SPI)
#include <SPI.h>
#include <TinyGFX/BusSPI.h>
TinyGFXBusSPI bus(SPI, PIN_DC, PIN_CS);
#elif defined(USE_SOFT_SPI)
#include <TinyGFX/BusSoftSPI.h>
TinyGFXBusSoftSPI bus(PIN_SCK, PIN_MOSI, PIN_DC, PIN_CS);
#elif defined(USE_I2C)
#include <Wire.h>
#include <TinyGFX/BusI2C.h>
TinyGFXBusI2C bus(Wire, I2C_ADDR);
#else
#error "Uncomment one of USE_SPI / USE_SOFT_SPI / USE_I2C above"
#endif

#if PAGED
TINYGFX_PANEL panel(bus, fb);
#else
TINYGFX_PANEL panel(bus, PIN_RST);
#endif
TinyGFX lcd(panel);

// **The picture.** Drawn in the current rotation, so it is also the rotation
// test: whatever the rotation, the block belongs at the top left and the
// diagonal must run to the opposite corner.
static void scene() {
  const int16_t w = lcd.width();
  const int16_t h = lcd.height();
  lcd.fillScreen(TFT_BLACK);

  // The exact edge. **A stripe of rubbish down one side, or a missing edge,
  // is the column offset** (PANEL_TUNING 2).
  lcd.drawRect(0, 0, w, h, TFT_WHITE);

  // The origin corner, and nowhere else.
  const int16_t m = (w < h ? w : h) / 5;
  lcd.fillRect(2, 2, m, m, TFT_WHITE);

  // Both axes at full length.
  lcd.drawLine(0, 0, (int16_t)(w - 1), (int16_t)(h - 1), TFT_WHITE);

#if !PAGED
  // Colour order. **Red first**: if the top bar is blue, call setRgbOrder().
  const int16_t bh = h / 12 > 2 ? h / 12 : 2;
  lcd.fillRect((int16_t)(w / 2), (int16_t)(h / 2), (int16_t)(w / 2 - 2), bh, TFT_RED);
  lcd.fillRect((int16_t)(w / 2), (int16_t)(h / 2 + bh), (int16_t)(w / 2 - 2), bh, TFT_GREEN);
  lcd.fillRect((int16_t)(w / 2), (int16_t)(h / 2 + bh * 2), (int16_t)(w / 2 - 2), bh, TFT_BLUE);
#endif

#if PAGED
  panel.display();   // nothing has reached the glass until this
#endif
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("TinyGFX bring-up"));
  Serial.print(F("  panel  "));
  Serial.print((int)TINYGFX_PANEL::kWidth);
  Serial.print('x');
  Serial.println((int)TINYGFX_PANEL::kHeight);
  Serial.print(F("  bus    "));
#if defined(USE_SPI)
  Serial.println(F("hardware SPI"));
  SPI.begin();                 // **the sketch owns the bus** (DECISIONS D24)
#elif defined(USE_SOFT_SPI)
  Serial.println(F("software SPI"));
#else
  Serial.print(F("I2C 0x"));
  Serial.println(I2C_ADDR, HEX);
  Wire.begin();
#endif

  const bool ok = lcd.begin();
  Serial.print(F("  begin  "));
  Serial.println(ok ? F("true") : F("false - the configuration is unusable"));
  if (!ok) return;   // a null buffer, a height that is not a multiple of 8, a zero dimension

  scene();
  Serial.println(F("  rotation 0 drawn. It advances every 3 s."));
}

void loop() {
  static uint8_t rot = 0;
  delay(3000);
  rot = (uint8_t)((rot + 1) & 3);
  lcd.setRotation(rot);
  scene();
  Serial.print(F("  rotation "));
  Serial.print(rot);
  Serial.print(F("  "));
  Serial.print(lcd.width());
  Serial.print('x');
  Serial.println(lcd.height());
}
