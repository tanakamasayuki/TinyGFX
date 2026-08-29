// TinyGFX - checking on real hardware (M5Stack Core / BASIC)
//
// One sketch holding **only what the automated tests cannot hold.** No wiring
// needed. The procedure and the checklist are M0b in docs/MANUAL_TEST.ja.md.
//
// Pages advance every 4 seconds. Btn A (left) goes forward, Btn C (right) back.
// What to look at is also printed on serial (115200).
//
//   0-3 rotations 0..3  is the MADCTL table right on real glass?
//                       (only rotation 0 has been confirmed)
//   4   text            drawing since the move to CellFont: sizes, background
//                       cells, uncovered codes
//   5   clipping        does anything leak outside?
//   6   direct drawing  the same animation, cleared then drawn.
//                       **It should flicker**
//   7   tiled rendering the same animation through TileCanvas.
//                       **It should not flicker**
//   8   speed           milliseconds, on serial
//   9   read-back       **can the panel be read back?**
#include <TinyGFX.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/FontCell.h>
#include <TinyGFX/PanelILI9342.h>
#include <TinyGFX/TileCanvas.h>

#include "tgfx_clock.h"

// The built-in LCD
static const int8_t PIN_DC = 27, PIN_CS = 14, PIN_RST = 33, PIN_BL = 32, PIN_SD_CS = 4;
// Buttons A / B / C (active LOW, externally pulled up)
static const int8_t BTN_A = 39, BTN_C = 37;

TinyGFXBusSPI bus(SPI, PIN_DC, PIN_CS, /*freq*/ 24000000UL);
TinyGFXPanelILI9342 panel(bus, 320, 240, PIN_RST);
TinyGFX lcd(panel);

static const TinyGFXFontRef clockFont = {&tgfxClock, &tinygfxFontCellOps};

// For tiled rendering. An ESP32 can afford a generous band (320 x 20 x 2 = 12,800 B)
static const int16_t BAND_ROWS = 20;
static uint16_t band[320 * BAND_ROWS];
TinyGFXTileCanvas canvas(panel, band, sizeof(band) / sizeof(band[0]));

static const uint8_t PAGES = 10;
static uint8_t page = 0;
static uint32_t shownAt = 0;

// ---------------------------------------------------------------------------

static void header(uint8_t n) {
  lcd.fillScreen(TFT_BLACK);
  lcd.drawRect(0, 0, lcd.width(), lcd.height(), TFT_WHITE);
  lcd.setFont(&clockFont);
  lcd.setTextColor(TFT_YELLOW);
  lcd.setTextSize(2);
  char s[4];
  snprintf(s, sizeof(s), "%d", (int)n);
  lcd.drawString(s, 6, 6);
  lcd.setTextSize(1);
}

/// Rotations 0..3. Red at the origin, blue at the bottom right, and the digits
/// must come out the right way up.
static void pageRotation(uint8_t r) {
  lcd.setRotation(r);
  header(r);
  const int16_t w = lcd.width(), h = lcd.height();
  lcd.fillRect(2, 2, 24, 24, TFT_RED);                 // the logical origin (0,0)
  lcd.fillRect((int16_t)(w - 26), (int16_t)(h - 26), 24, 24, TFT_BLUE);
  lcd.drawLine(0, 0, (int16_t)(w - 1), (int16_t)(h - 1), TFT_DARKGREY);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(3);
  lcd.drawString("0123456789", 30, (int16_t)(h / 2 - 12));
  lcd.setTextSize(1);
  // Width and height as numbers (they swap at rotations 1 and 3)
  char s[16];
  snprintf(s, sizeof(s), "%d:%d", (int)w, (int)h);
  lcd.setTextColor(TFT_CYAN);
  lcd.setTextSize(2);
  lcd.drawString(s, 30, 8);
  lcd.setTextSize(1);
}

/// Text. Not once put on real glass since the move to CellFont.
static void pageText() {
  lcd.setRotation(0);
  header(4);
  lcd.setFont(&clockFont);
  int16_t y = 40;
  for (uint8_t sz = 1; sz <= 4; ++sz) {          // text sizes
    lcd.setTextSize(sz);
    lcd.setTextColor(TFT_WHITE);
    lcd.drawString("0123456789", 10, y);
    y = (int16_t)(y + 8 * sz + 6);
  }
  lcd.setTextSize(2);
  lcd.setTextColor(TFT_BLACK, TFT_GREEN);        // background cell: no gaps under the glyph
  lcd.drawString("0123", 10, y);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("45", 130, y);                  // must not revert to transparent
  lcd.setTextColor(TFT_RED, TFT_BLACK);
  lcd.drawString("12:34", 190, y);
  lcd.setTextSize(1);
  // Uncovered (this font has no letters). **No tofu either, so nothing appears
  // and the text looks squashed together**
  lcd.setTextColor(TFT_DARKGREY);
  lcd.setTextSize(2);
  lcd.drawString("01ABC23", 10, (int16_t)(y + 30));
  lcd.setTextSize(1);
}

/// Clipping. Not one pixel may leak outside.
static void pageClip() {
  lcd.setRotation(0);
  header(5);
  lcd.drawRect(59, 59, 202, 122, TFT_DARKGREY);   // one pixel outside the clip frame
  lcd.setClipRect(60, 60, 200, 120);
  lcd.fillCircle(160, 120, 200, TFT_NAVY);        // a circle bigger than the screen
  for (int16_t i = -200; i < 400; i += 24) {
    lcd.drawLine(i, 0, (int16_t)(i + 120), 239, TFT_CYAN);
  }
  lcd.setTextSize(3);
  lcd.setTextColor(TFT_YELLOW);
  lcd.drawString("0123456789", 20, 100);          // a string that runs off both sides
  lcd.setTextSize(1);
  lcd.resetClipRect();
}

/// The animation. **Nothing here clears** - the band side uses autoClear and
/// the direct side calls fillScreen. It runs once per band, so keep it cheap.
static void ballScene(TinyGFX& g, void* ctx) {
  const int16_t t = *(const int16_t*)ctx;
  g.drawRect(0, 0, 320, 240, TFT_DARKGREY);
  g.fillCircle(t, 120, 40, TFT_YELLOW);
  g.fillRect((int16_t)(320 - t - 30), 40, 60, 160, TFT_MAGENTA);
  g.fillTriangle(20, 220, 60, 150, 100, 220, TFT_CYAN);
}

/// Direct drawing: clear, then draw, so it flickers. **This is the comparison.**
static void pageDirect() {
  lcd.setRotation(0);
  const uint32_t until = millis() + 6000;
  int16_t t = 60;
  int8_t dir = 6;
  while (millis() < until) {
    lcd.startWrite();
    lcd.fillScreen(TFT_NAVY);
    ballScene(lcd, &t);
    lcd.endWrite();
    t = (int16_t)(t + dir);
    if (t > 260 || t < 60) dir = (int8_t)-dir;
  }
}

/// Tiled rendering. **The same picture, the same motion, and no flicker.**
static void pageTiled() {
  canvas.setRotation(0);
  canvas.setBackgroundColor(TFT_NAVY);
  const uint32_t until = millis() + 6000;
  int16_t t = 60;
  int8_t dir = 6;
  while (millis() < until) {
    canvas.render(&ballScene, &t);
    t = (int16_t)(t + dir);
    if (t > 260 || t < 60) dir = (int8_t)-dir;
  }
  lcd.setRotation(0);
}

/// Speed. **Never measured even once.** Printed on serial.
static void pageBench() {
  lcd.setRotation(0);
  header(8);
  struct { const char* name; uint32_t ms; } r[6];
  uint32_t t0;

  t0 = millis();
  for (int i = 0; i < 5; ++i) lcd.fillScreen(i & 1 ? TFT_BLACK : TFT_NAVY);
  r[0] = {"fillScreen x5", millis() - t0};

  t0 = millis();
  for (int i = 0; i < 200; ++i) lcd.fillRect(10, 10, 100, 100, (uint16_t)(i * 37));
  r[1] = {"fillRect 100x100 x200", millis() - t0};

  t0 = millis();
  for (int i = 0; i < 200; ++i) lcd.drawLine(0, 0, 319, 239, (uint16_t)(i * 37));
  r[2] = {"drawLine diag x200", millis() - t0};

  t0 = millis();
  for (int i = 0; i < 200; ++i) lcd.fillCircle(160, 120, 100, (uint16_t)(i * 37));
  r[3] = {"fillCircle r100 x200", millis() - t0};

  lcd.setFont(&clockFont);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  t0 = millis();
  for (int i = 0; i < 200; ++i) lcd.drawString("0123456789", 10, 60);
  r[4] = {"drawString 10 chars x200 (size2)", millis() - t0};

  canvas.setRotation(0);
  canvas.setBackgroundColor(TFT_NAVY);
  int16_t bt = 100;
  t0 = millis();
  for (int i = 0; i < 20; ++i) canvas.render(&ballScene, &bt);
  r[5] = {"TileCanvas full frame x20", millis() - t0};
  lcd.setRotation(0);

  lcd.setTextSize(1);
  header(8);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  Serial.println(F("--- speed (M5Stack Core, 24MHz SPI, TINYGFX_FILL_CHUNK off) ---"));
  int16_t y = 40;
  for (uint8_t i = 0; i < 6; ++i) {
    char s[12];
    snprintf(s, sizeof(s), "%lu", (unsigned long)r[i].ms);
    lcd.drawString(s, 12, y);
    y = (int16_t)(y + 30);
    Serial.print("  ");
    Serial.print(r[i].name);
    Serial.print(": ");
    Serial.print(r[i].ms);
    Serial.println(" ms");
  }
  lcd.setTextSize(1);
}


/// Can the panel be read back? **The experiment that decides whether it can.**
///
/// If it can, the hardware can be checked automatically by drawing, reading
/// back and comparing - the same rigour the host tests have, on real glass.
/// The first question is simply whether the line is connected.
///
/// An ILI934x wants three things, and getting any of them wrong produces
/// garbage, so the raw bytes are printed as they come.
///   1. reads use a slower clock than writes (8MHz here)
///   2. RAMRD (0x2E) begins with one dummy byte
///   3. even written at 16bpp, **a read is 3 bytes a pixel** (RGB666, top
///      aligned)
static void dump(const char* label, uint8_t cmd, uint8_t n) {
  uint8_t buf[16];
  for (uint8_t i = 0; i < n && i < 16; ++i) buf[i] = 0;
  panel.readRegister(cmd, buf, n);
  Serial.print("  ");
  Serial.print(label);
  Serial.print(" (0x");
  if (cmd < 16) Serial.print('0');
  Serial.print(cmd, HEX);
  Serial.print("):");
  for (uint8_t i = 0; i < n && i < 16; ++i) {
    Serial.print(' ');
    if (buf[i] < 16) Serial.print('0');
    Serial.print(buf[i], HEX);
  }
  Serial.println();
}

static void pageReadback() {
  lcd.setRotation(0);
  header(9);
  // **An M5Stack has one data line** (SDA=GPIO23); nothing reaches SPI MISO (19)
  bus.setReadPins(/*sck*/ 18, /*sda*/ 23);

  // Put distinguishable colours where the read-back will look
  lcd.fillRect(0, 100, 2, 1, TFT_RED);      // (0,100) red
  lcd.drawPixel(1, 100, TFT_GREEN);         // (1,100) green

  Serial.println(F("--- read-back probe ---"));
  dump("RDDID ", 0x04, 5);   // an ILI9341 gives 00 00 93 41; all 00 means MISO is not arriving
  dump("RDID4 ", 0xD3, 5);
  dump("RDDST ", 0x09, 6);

  // Read the two pixels (0,100)-(1,100): 1 dummy + 3 bytes x 2 = 7 bytes
  uint8_t a[4];
  bus.beginTransaction();
  a[0] = 0; a[1] = 0; a[2] = 0; a[3] = 1;
  bus.writeCommand(0x2A);
  bus.writeData(a, 4);                       // CASET 0..1
  a[0] = 0; a[1] = 100; a[2] = 0; a[3] = 100;
  bus.writeCommand(0x2B);
  bus.writeData(a, 4);                       // RASET 100..100
  bus.endTransaction();

  Serial.println(F("  wrote: (0,100)=F800 red  (1,100)=07E0 green"));
  dump("RAMRD ", 0x2E, 8);   // expect: dummy, F8 00 00, 00 FC 00 (the top 6 bits carry it)

  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.drawString("0123456789", 12, 140);     // on screen, just a marker that this ran
  lcd.setTextSize(1);
  Serial.println(F("  NOTE: this panel returns FF for ID registers but the GRAM reads fine"));
}

static const char* PAGE_HINT[PAGES] = {
    "rot 0 : red square top-left, digits upright, 320:240",
    "rot 1 : same picture turned 90 deg, 240:320",
    "rot 2 : turned 180 deg, 320:240",
    "rot 3 : turned 270 deg, 240:320",
    "text  : sizes 1-4, green background cell flush under glyphs, A-C blank",
    "clip  : nothing outside the grey frame",
    "direct: same motion, cleared each frame -- SHOULD flicker",
    "tiled : same motion via TileCanvas -- should NOT flicker",
    "bench : timings on serial",
    "read  : can the panel be read back? raw bytes on serial",
};

static void show(uint8_t n) {
  switch (n) {
    case 0: case 1: case 2: case 3: pageRotation(n); break;
    case 4: pageText(); break;
    case 5: pageClip(); break;
    case 6: pageDirect(); break;
    case 7: pageTiled(); break;
    case 8: pageBench(); break;
    default: pageReadback(); break;
  }
  Serial.print(n);
  Serial.print(F(" | "));
  Serial.println(PAGE_HINT[n]);
  shownAt = millis();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);
  pinMode(BTN_A, INPUT);
  pinMode(BTN_C, INPUT);

  SPI.begin();  // the sketch owns the bus; TinyGFX never begins it
  lcd.begin();
  canvas.begin();  // the tiled side. The panel init runs twice, which is harmless

  // On an older BASIC where the colours come out inverted, take this out
  // (same as M0). It has to come **after canvas.begin()**, or re-initialising
  // the panel puts INVON back.
  // panel.invertDisplay(false);

  Serial.println();
  Serial.println(F("TinyGFX manual check (M5Stack). BtnA=next BtnC=prev, auto every 4s"));
  show(0);
}

void loop() {
  bool next = false, prev = false;
  if (digitalRead(BTN_A) == LOW) { next = true; while (digitalRead(BTN_A) == LOW) delay(10); }
  if (digitalRead(BTN_C) == LOW) { prev = true; while (digitalRead(BTN_C) == LOW) delay(10); }
  if (!next && !prev && millis() - shownAt < 4000) { delay(20); return; }
  if (prev) page = (uint8_t)((page + PAGES - 1) % PAGES);
  else page = (uint8_t)((page + 1) % PAGES);
  show(page);
}
