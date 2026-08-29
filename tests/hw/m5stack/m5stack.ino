// **The hardware test.** An M5Stack Core / BASIC is the standard board for it.
//
// **What the hardware drew is compared against the host's golden.**
//
//   host      tests/scene/      draws the shared scene through BusCapture
//                               -> golden/scene.ppm
//   hardware  here              draws the same scene **on the board** and sends
//                               it as an artifact
//   pytest    test_hw_m5stack   compares the two
//
// There are two ways to get the picture. **Only the first works on an M5Stack**
// (the second was measured impossible on 2026-08-28).
//
//   1. Through TinyGFXBusCapture (always available)
//      Shows the result of the real compiler, the real int width and the real
//      PROGMEM. Genuine **as far as the panel driver's command stream**;
//      nothing past the wire is seen
//   2. By reading the panel's GRAM back (only on panels where SDO is wired)
//      Sees past the wire too. **Not possible on an M5Stack Core, where SDO
//      does not reach GPIO19**
//
// The scene is defined in exactly one place, tgfx_scene.h, so the two sides
// **cannot drift apart.**
//
// The first serial bytes get dropped, so everything is sent after
// synchronising on ArduTest's HELLO.
#include <ArduTest.h>
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/FontCell.h>
#include <TinyGFX/PanelILI9342.h>
#include <tgfx_scene.h>
#include <tgfx_digits.h>

static const int8_t PIN_DC = 27, PIN_CS = 14, PIN_RST = 33, PIN_BL = 32, PIN_SD_CS = 4;

TinyGFXBusSPI bus(SPI, PIN_DC, PIN_CS, /*freq*/ 24000000UL);
TinyGFXPanelILI9342 panel(bus, 320, 240, PIN_RST);
TinyGFX lcd(panel);

static const TinyGFXFontRef digitsFont = {&tgfxDigits, &tinygfxFontCellOps};
// **Only the top strip is read back.** Every read turns the line around, and
// running that dozens of times locks the board up (measured). Since this is for
// debugging, keep it small.
// Read-back costs about 150us a pixel, so the golden comparison takes the
// top strip rather than the whole scene. 64x8 is ~75ms and still covers the
// border, the three colour blocks and the circle.
static const int16_t TGFX_READBACK_H = 8;
static uint16_t readBuf[TGFX_SCENE_W * TGFX_SCENE_H];
static uint16_t capBuf[TGFX_SCENE_W * TGFX_SCENE_H];

static void hex2(char* out, uint8_t v) {
  static const char* D = "0123456789ABCDEF";
  out[0] = D[v >> 4];
  out[1] = D[v & 15];
}

/// Is the read-back line connected at all? **If this fails, nothing below
/// means anything.**
/// Can this panel be read back at all? Reports, never judges - a panel that
/// cannot read is not a failure, so pytest turns this into a skip.
///
/// Judged on RAMRD rather than an ID register: this panel answers FF to
/// 0x04 and friends while reading its GRAM perfectly well, and the GRAM is
/// what we actually want.
TEST_CASE(test_panel_readable) {
  static const uint16_t probe[3] = {TFT_RED, TFT_GREEN, TFT_BLUE};
  lcd.startWrite();
  for (int16_t i = 0; i < 3; ++i) lcd.fillRect((int16_t)(200 + i), 200, 1, 1, probe[i]);
  lcd.endWrite();

  char text[64];
  int n = 0;
  bool ok = true;
  for (int16_t i = 0; i < 3; ++i) {
    uint16_t got = 0;
    panel.readPixels((uint16_t)(200 + i), 200, 1, 1, &got);
    hex2(&text[n], (uint8_t)(got >> 8)); n += 2;
    hex2(&text[n], (uint8_t)got); n += 2;
    text[n++] = ' ';
    if (got != probe[i]) ok = false;
  }
  text[n] = 0;
  ArduTest.attachText("readback_probe.txt", text);
  ArduTest.reportMetric("readable", ok ? 1 : 0);
}

/// Do written colours come back unchanged? Skipped by pytest when the panel
/// cannot read.
TEST_CASE(test_readback_solid) {
  static const uint16_t want[4] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
  lcd.startWrite();
  for (int16_t i = 0; i < 4; ++i) lcd.fillRect((int16_t)(i * 4), 0, 4, 4, want[i]);
  lcd.endWrite();
  for (int16_t i = 0; i < 4; ++i) {
    uint16_t got = 0;
    panel.readPixels((uint16_t)(i * 4 + 1), 1, 1, 1, &got);
    ArduTest.reportMetric("readback", (long)got);
    ASSERT_EQ((long)want[i], (long)got);
  }
}

/// **Is the rotation reaching MADCTL correctly?** Only a read-back can tell.
///
/// The obvious approach - draw at rotation N and read at rotation N - shows
/// nothing. RAMRD goes through the same address counter as the write, so a
/// wrong MADCTL is self-consistent and reads back fine.
/// Reading in the same rotation you drew in proves nothing: RAMRD goes through
/// the same address counter as the write, so a wrong MADCTL reads back
/// consistently wrong.
///
/// So: draw in rotation N, then go back to rotation 0 and read. Changing
/// MADCTL remaps the access order, it does not move what is already in GRAM,
/// so rotation 0 gives a fixed frame to look at what physically landed where.
///
/// Derived from the MADCTL table in PanelILI9342.h (MV transposes, MX flips
/// the column, MY flips the row), for a 320x240 landscape GRAM:
///
///   rotation 0  MADCTL 0        (lx, ly)
///   rotation 1  MADCTL MV|MX    (W-1-ly, lx)
///   rotation 2  MADCTL MX|MY    (W-1-lx, H-1-ly)
///   rotation 3  MADCTL MV|MY    (ly, H-1-lx)
///
/// (5, 2) puts the four markers in four different corners, so a marker cannot
/// be mistaken for another rotation's, and each gets its own colour so a stale
/// pixel cannot fake a pass.
///
/// **What this locks is rotations 1-3 against rotation 0.** Rotation 0 being
/// the right way up is not something read-back can see - that was checked by
/// eye once (docs/MANUAL_TEST.ja.md M0).
TEST_CASE(test_rotation_maps) {
  static const uint16_t MARK[4] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
  static const int16_t LX = 5, LY = 2;
  static const int16_t W = 320, H = 240;

  lcd.setRotation(0);
  lcd.fillScreen(TFT_BLACK);
  for (uint8_t r = 0; r < 4; ++r) {
    lcd.setRotation(r);
    lcd.fillRect(LX, LY, 1, 1, MARK[r]);
  }
  lcd.setRotation(0);  // back to the fixed frame

  const uint16_t px[4] = {(uint16_t)LX, (uint16_t)(W - 1 - LY),
                          (uint16_t)(W - 1 - LX), (uint16_t)LY};
  const uint16_t py[4] = {(uint16_t)LY, (uint16_t)LX,
                          (uint16_t)(H - 1 - LY), (uint16_t)(H - 1 - LX)};

  char text[96];
  int n = 0;
  for (uint8_t r = 0; r < 4; ++r) {
    uint16_t got = 0;
    panel.readPixels(px[r], py[r], 1, 1, &got);
    ArduTest.reportMetric("rot_found", (long)got);
    hex2(&text[n], (uint8_t)(got >> 8)); n += 2;
    hex2(&text[n], (uint8_t)got); n += 2;
    text[n++] = ' ';
  }
  text[n] = 0;
  ArduTest.attachText("rotation_marks.txt", text);
}

/// **The point.** Draws the shared scene on the hardware and sends it as is.
///
/// Shows the result of the real compiler, the real int width and the real
/// PROGMEM. The panel driver's (ILI9342's) command stream is genuine too, and
/// BusCapture rebuilds it into a virtual GRAM. **Only what happens past the
/// wire is out of scope.**
TEST_CASE(test_capture_scene) {
  TinyGFXBusCapture cap(capBuf, TGFX_SCENE_W, TGFX_SCENE_H);
  TinyGFXPanelILI9342 capPanel(cap, TGFX_SCENE_W, TGFX_SCENE_H);
  TinyGFX g(capPanel);
  g.begin();
  cap.fill(0);
  g.setFont(&digitsFont);
  g.setTextColor(TFT_WHITE);
  tgfxGoldenScene(g);

  ArduTest.reportMetric("capture_pixels", (long)cap.pixelCount());
  // RGB565, little endian - exactly how this board holds it in memory
  ArduTest.attachBinary("scene.rgb565", "application/octet-stream",
                        (const uint8_t*)capBuf, sizeof(capBuf));

  // Put it on the real screen too, for when someone wants to look at it
  lcd.setFont(&digitsFont);
  lcd.setTextColor(TFT_WHITE);
  tgfxGoldenScene(lcd);
}

/// The same scene, this time read back out of the panel's own GRAM.
/// **This is the only test that sees past the wire.**
TEST_CASE(test_readback_scene) {
  lcd.setFont(&digitsFont);
  lcd.setTextColor(TFT_WHITE);
  tgfxGoldenScene(lcd);
  // The top strip only - see TGFX_READBACK_H.
  for (int i = 0; i < TGFX_SCENE_W * TGFX_READBACK_H; ++i) readBuf[i] = 0;
  const uint32_t n = panel.readPixels(0, 0, TGFX_SCENE_W, TGFX_READBACK_H, readBuf);
  ArduTest.reportMetric("scene_pixels", (long)n);
  ArduTest.attachBinary("readback.rgb565", "application/octet-stream",
                        (const uint8_t*)readBuf,
                        (size_t)TGFX_SCENE_W * TGFX_READBACK_H * 2);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);

  SPI.begin();  // the sketch owns the bus; TinyGFX never begins it
  lcd.begin();
  // **An M5Stack has one data line** (SDA=GPIO23); nothing reaches SPI MISO (19)
  bus.setReadPins(/*sck*/ 18, /*sda*/ 23);
  lcd.fillScreen(TFT_BLACK);

  ArduTest.begin();
}

void loop() {
  ArduTest.poll();
}
