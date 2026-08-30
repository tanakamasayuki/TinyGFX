// ILI9341 - **the one panel that had no test** (added 2026-08-29).
//
// It is 21 lines, and only three things separate it from an ILI9342C:
//   1. a portrait GRAM (240x320)
//   2. **inversion off** (an ILI9342C and an ST7789 have it on)
//   3. **an X mirror in the implementation** (an ordinary breakout wants
//      MADCTL 0x48 at rotation 0)
//
// All three show up in **the command stream init() emits**. And yet
// **nothing was watching that stream.** BusCapture silently discards commands
// it does not know, so dropping SLPOUT or changing COLMOD left every host test
// passing.
//
// So this test holds two things:
//   A. that an ILI9341 is an ILI9341 (the three points above)
//   B. **the init sequence of all three DCS panels** - ST7789 / ILI9341 /
//      ILI9342 run on the same bus, pinning that only one byte differs
//
// **Whether that MADCTL is right on real glass cannot be known here**
// (MANUAL_TEST M5). What this holds is that the implementation follows the table.
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/DriverILI9341.h>
#include <TinyGFX/DriverILI9342.h>
#include <TinyGFX/DriverST7789.h>
#include <tgfx_test.h>

// A BusCapture that remembers the command stream. **The library is left
// alone**: only this test needs the recording, so it derives here.
static const int SEQ_MAX = 16;

class RecordingBus : public TinyGFXBusCapture {
 public:
  RecordingBus(uint16_t* gram, uint16_t w, uint16_t h) : TinyGFXBusCapture(gram, w, h) {}

  void writeCommand(uint8_t cmd) override {
    if (_n < SEQ_MAX) { _cmd[_n] = cmd; _arg[_n] = 0; _hasArg[_n] = false; }
    ++_n;
    TinyGFXBusCapture::writeCommand(cmd);
  }
  void writeData(const uint8_t* data, size_t len) override {
    if (len > 0 && _n > 0 && _n <= SEQ_MAX && !_hasArg[_n - 1]) {
      _arg[_n - 1] = data[0];
      _hasArg[_n - 1] = true;
    }
    TinyGFXBusCapture::writeData(data, len);
  }

  void clearSeq() { _n = 0; }
  int count() const { return (_n < SEQ_MAX) ? (int)_n : SEQ_MAX; }
  uint8_t cmdAt(int i) const { return _cmd[i]; }
  uint8_t argAt(int i) const { return _arg[i]; }

 private:
  uint8_t _cmd[SEQ_MAX] = {0};
  uint8_t _arg[SEQ_MAX] = {0};
  bool _hasArg[SEQ_MAX] = {false};
  int _n = 0;
};

static const int W = 32, H = 16;
static uint16_t gram[W * H];
RecordingBus bus(gram, W, H);

/// Report the command stream init() emitted, verbatim.
static void reportInit(const char* name, TinyGFXTarget& panel) {
  bus.clearSeq();
  panel.init();
  char key[24];
  snprintf(key, sizeof(key), "%s_len", name);
  tgfxReport(key, (long)bus.count());
  for (int i = 0; i < bus.count(); ++i) {
    snprintf(key, sizeof(key), "%s_cmd%d", name, i);
    tgfxReport(key, (long)bus.cmdAt(i));
    snprintf(key, sizeof(key), "%s_arg%d", name, i);
    tgfxReport(key, (long)bus.argAt(i));
  }
}

static void probe(TinyGFXTarget& panel, const char* prefix, uint8_t r) {
  char key[16];
  snprintf(key, sizeof(key), "%s%d", prefix, (int)r);
  panel.setRotation(r);
  tgfxReport2(key, "madctl", (long)bus.lastCommandArg());
  tgfxReport2(key, "w", (long)panel.width());
  tgfxReport2(key, "h", (long)panel.height());
  panel.setWindow(0, 0, 0, 0);
  tgfxReport2(key, "xs", (long)bus.windowXs());
  tgfxReport2(key, "ys", (long)bus.windowYs());
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("ili9341");

  // --- B. the init sequence of all three DCS panels -------------------------
  // Run on the same footing: the same bus, the default dimensions.
  {
    TinyGFXDriverST7789 st(bus, 240, 320);
    reportInit("st7789", st);
  }
  {
    TinyGFXDriverILI9341 ili41(bus);
    reportInit("ili9341", ili41);
  }
  {
    TinyGFXDriverILI9342 ili42(bus);
    reportInit("ili9342", ili42);
  }

  // --- A. it is an ILI9341 --------------------------------------------------
  // The default size is 240x320, portrait. The GRAM can stay small: all that
  // is read here is MADCTL, the width and height, and the window origin.
  {
    TinyGFXDriverILI9341 panel(bus);
    panel.init();
    for (uint8_t r = 0; r < 4; ++r) probe(panel, "def", r);

    // The X mirror the implementation carries must be removable when the
    // hardware turns out to be mounted the other way (the escape hatch the
    // header documents). Removing it leaves the bare table plus BGR.
    panel.setMirror(false, false);
    for (uint8_t r = 0; r < 4; ++r) probe(panel, "nomir", r);

    // The colour order comes off too
    panel.setMirror(true, false);
    panel.setRgbOrder(false);
    for (uint8_t r = 0; r < 4; ++r) probe(panel, "rgb", r);
  }

  // --- colour really reaches the GRAM ---------------------------------------
  {
    TinyGFXDriverILI9341 small(bus, W, H);
    TinyGFX lcd(small);
    lcd.begin();
    lcd.setRotation(0);
    bus.fill(0);
    lcd.fillRect(2, 3, 4, 5, TFT_RED);
    tgfxReport("hit", (long)bus.pixel(3, 4));
    tgfxReport("miss", (long)bus.pixel(1, 3));
    tgfxReport("edge", (long)bus.pixel(5, 7));
    tgfxReport("past", (long)bus.pixel(6, 8));
  }

  tgfxTestDone();
}
void loop() {}
