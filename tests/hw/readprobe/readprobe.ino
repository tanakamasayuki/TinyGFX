// Isolating what actually makes panel read-back work on an M5Stack.
//
// The ILI9342C here has a single data line (SDA on GPIO23, MOSI and MISO both)
// and the SPI peripheral's MISO pin is connected to nothing, so reading means
// turning the line around and clocking it by hand.
//
// A one-shot probe that bit-banged the whole exchange read perfectly. Going
// through TinyGFXBusSPI did not. This sketch runs the candidate strategies
// side by side, in one flash, so the difference stops being guesswork.
//
// Each case writes four known colours and reads them back:
//   F800 red / 07E0 green / 001F blue / FFFF white
// which come back as RGB666: FC 00 00 / 00 FC 00 / 00 00 FC / FC FC FC.
#include <ArduTest.h>
#include <SPI.h>
#include <TinyGFX.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/PanelILI9342.h>

static const int8_t PIN_DC = 27, PIN_CS = 14, PIN_RST = 33, PIN_BL = 32, PIN_SD_CS = 4;
static const uint8_t PIN_SCK = 18, PIN_SDA = 23;
static const uint32_t WRITE_HZ = 24000000UL;

TinyGFXBusSPI bus(SPI, PIN_DC, PIN_CS, WRITE_HZ);
TinyGFXPanelILI9342 panel(bus, 320, 240, PIN_RST);
TinyGFX lcd(panel);

static const uint16_t WANT[4] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};

// ---------------------------------------------------------------------------
// Hand-clocked primitives. Nothing here touches the SPI peripheral.
// ---------------------------------------------------------------------------
static void bbTake() {
  // Do NOT call SPI.end() here. Measured: with it, every read comes back FF;
  // without it the panel answers, and pinMode alone is enough to take the pins.
  pinMode(PIN_SCK, OUTPUT);
  pinMode(PIN_SDA, OUTPUT);
  digitalWrite(PIN_SCK, LOW);
}

/// Hand the pins back to the SPI peripheral.
///
/// **end() then begin(), in that order.** ESP32's SPI.begin() returns early
/// when the bus is already started, so begin() on its own leaves the pins in
/// GPIO mode and every later write goes nowhere - silently, because writing
/// the same pixels again looks identical.
static void bbRelease() {
  SPI.end();
  SPI.begin();
}

static void bbWrite(uint8_t v, bool isCmd) {
  digitalWrite(PIN_DC, isCmd ? LOW : HIGH);
  for (int8_t i = 7; i >= 0; --i) {
    digitalWrite(PIN_SDA, (v >> i) & 1);
    digitalWrite(PIN_SCK, HIGH);
    digitalWrite(PIN_SCK, LOW);
  }
  digitalWrite(PIN_DC, HIGH);
}

static uint8_t bbRead() {
  uint8_t v = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    // Tight on purpose. Inserting even delayMicroseconds(1) per edge makes
    // every read come back FF - measured. Do not "make this safer".
    digitalWrite(PIN_SCK, HIGH);
    v = (uint8_t)((v << 1) | (digitalRead(PIN_SDA) ? 1 : 0));
    digitalWrite(PIN_SCK, LOW);
  }
  return v;
}

/// Read one pixel, hand-clocked. The pins must already be taken.
static uint16_t bbReadOne(uint16_t x, uint16_t y) {
  digitalWrite(PIN_CS, LOW);
  bbWrite(0x2A, true);
  bbWrite((uint8_t)(x >> 8), false); bbWrite((uint8_t)x, false);
  bbWrite((uint8_t)(x >> 8), false); bbWrite((uint8_t)x, false);
  bbWrite(0x2B, true);
  bbWrite((uint8_t)(y >> 8), false); bbWrite((uint8_t)y, false);
  bbWrite((uint8_t)(y >> 8), false); bbWrite((uint8_t)y, false);
  bbWrite(0x2E, true);
  pinMode(PIN_SDA, INPUT);
  bbRead();  // the ILI934x dummy byte
  const uint8_t r = bbRead(), g = bbRead(), b = bbRead();
  digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_SDA, OUTPUT);
  return tinygfx_color565(r, g, b);
}

/// Read `n` pixels starting at (x, y).
///
/// Two things this has to work around, both measured on an M5Stack:
///
///   - One window and one RAMRD **per pixel**. A single RAMRD over a run does
///     not advance the column address; every pixel comes back as the first.
///   - **Read each pixel until two attempts agree.** Roughly one byte in
///     twenty comes back with a bit flipped, and the clock cannot be slowed to
///     fix it - inserting any delay makes the panel stop driving the line
///     altogether.
///
/// The pins are taken once here and left in bit-bang mode; the caller hands
/// them back with SPI.begin() when it is done reading.
static long g_retries = 0;

static void bbReadPixels(uint16_t x, uint16_t y, uint16_t n, uint16_t* out) {
  bbTake();
  for (uint16_t i = 0; i < n; ++i) {
    const uint16_t cx = (uint16_t)(x + i);
    uint16_t a = bbReadOne(cx, y);
    for (uint8_t tries = 0; tries < 4; ++tries) {
      const uint16_t b = bbReadOne(cx, y);
      if (a == b) break;
      ++g_retries;
      a = b;
    }
    out[i] = a;
  }
  digitalWrite(PIN_SCK, LOW);
}

static void paint() {
  lcd.startWrite();
  for (int16_t i = 0; i < 4; ++i) lcd.fillRect((int16_t)(i * 4), 0, 4, 4, WANT[i]);
  lcd.endWrite();
}

/// Read four pixels and report how many matched. 4 means the strategy works.
static long check(const char* name) {
  uint16_t got[4];
  long ok = 0;
  char text[48];
  int n = 0;
  for (int16_t i = 0; i < 4; ++i) {
    got[i] = 0;
    bbReadPixels((uint16_t)(i * 4 + 1), 1, 1, &got[i]);
    if (got[i] == WANT[i]) ++ok;
    const char* D = "0123456789ABCDEF";
    text[n++] = D[(got[i] >> 12) & 15]; text[n++] = D[(got[i] >> 8) & 15];
    text[n++] = D[(got[i] >> 4) & 15];  text[n++] = D[got[i] & 15];
    text[n++] = ' ';
  }
  text[n] = 0;
  ArduTest.log(text);
  ArduTest.reportMetric(name, ok);
  return ok;
}

// ---------------------------------------------------------------------------
// The candidates.
// ---------------------------------------------------------------------------

/// The recipe, now that it is pinned down: take SCK and SDA with pinMode,
/// clock the whole exchange by hand with no delays, hand the pins back with
/// SPI.begin(). Read four known colours.
TEST_CASE(test_four_colours) {
  paint();
  const long ok = check("matched");
  bbRelease();
  ASSERT_EQ(4L, ok);
}

/// Twenty times over. This is what used to hang.
TEST_CASE(test_repeated) {
  long worst = 4;
  for (int i = 0; i < 20; ++i) {
    paint();
    uint16_t got[4];
    long ok = 0;
    for (int16_t k = 0; k < 4; ++k) {
      got[k] = 0;
      bbReadPixels((uint16_t)(k * 4 + 1), 1, 1, &got[k]);
      if (got[k] == WANT[k]) ++ok;
    }
    if (ok < worst) worst = ok;
  }
  bbRelease();
  ArduTest.reportMetric("worst_of_20", worst);
  ArduTest.reportMetric("retries", g_retries);
  ASSERT_EQ(4L, worst);
}

/// A run of pixels - the shape a golden comparison needs. Dump what came back
/// so a shift can be told apart from noise.
TEST_CASE(test_run) {
  static const uint16_t PAT[8] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE,
                                  TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, TFT_NAVY};
  lcd.startWrite();
  for (int16_t x = 0; x < 8; ++x) lcd.fillRect(x, 8, 1, 1, PAT[x]);
  lcd.endWrite();

  uint16_t got[8];
  bbReadPixels(0, 8, 8, got);
  bbRelease();

  static const char* D = "0123456789ABCDEF";
  char text[80];
  int n = 0;
  long ok = 0;
  for (int16_t x = 0; x < 8; ++x) {
    if (got[x] == PAT[x]) ++ok;
    text[n++] = D[(got[x] >> 12) & 15]; text[n++] = D[(got[x] >> 8) & 15];
    text[n++] = D[(got[x] >> 4) & 15];  text[n++] = D[got[x] & 15];
    text[n++] = ' ';
  }
  text[n] = 0;
  ArduTest.log(text);
  ArduTest.reportMetric("run_matched", ok);
  ArduTest.reportMetric("retries", g_retries);
  ASSERT_EQ(8L, ok);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);
  SPI.begin();
  lcd.begin();
  lcd.fillScreen(TFT_BLACK);
  ArduTest.begin();
}

void loop() { ArduTest.poll(); }
