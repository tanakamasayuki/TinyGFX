// **Does bit-banged I2C put the same bytes on the wire as Wire does?**
//
// The waveform can be watched without hardware. The host core's pin hooks are
// used to **model the I2C bus itself**:
//
//   - open drain: OUTPUT+LOW pulls down, INPUT lets go
//   - **the read hook plays the pull-up** - a pin set to INPUT reads HIGH
//   - SDA is sampled on the rising edge of SCL; START / STOP are detected and
//     the bytes reassembled
//
// Those bytes are fed to a model of an SSD1306 and **compared against the same
// picture drawn through Wire.** One pixel of difference means the
// implementations differ.
#define TGFX_HOST_PROBE_SPI 1
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/BusSoftI2C.h>
#include <TinyGFX/DriverSSD1306.h>
#include <tgfx_test.h>
#include <tgfx_host_probe.h>
#include <Wire.h>

static const int W = 128, H = 64, PAGES = H / 8;
static const uint8_t ADDR = 0x3C;
static const uint8_t PIN_SDA = 4, PIN_SCL = 5;

static uint8_t fbSoft[W * H / 8], fbWire[W * H / 8];

// ---- a model of an SSD1306 (the same one tests/i2c uses) -----------------
static uint8_t model[W * PAGES];
static uint16_t colStart = 0, colEnd = W - 1, curCol = 0, curPage = 0;
static uint8_t pendingCmd = 0, argIndex = 0, args[2] = {0, 0};
static uint32_t dataBytes = 0;

static void feedCmd(uint8_t c) {
  if (pendingCmd != 0) {
    args[argIndex++] = c;
    if (argIndex == 2) {
      if (pendingCmd == 0x21) { colStart = args[0]; colEnd = args[1]; curCol = colStart; }
      else                    { curPage = args[0]; }
      pendingCmd = 0; argIndex = 0;
    }
    return;
  }
  if (c == 0x21 || c == 0x22) { pendingCmd = c; argIndex = 0; }
}
static void putByte(uint8_t b) {
  if (curPage < (uint16_t)PAGES && curCol < (uint16_t)W) model[curPage * W + curCol] = b;
  if (curCol >= colEnd) { curCol = colStart; ++curPage; } else { ++curCol; }
}
/// Read one transfer's bytes as an SSD1306 would (the address comes first).
static uint8_t ctrl = 0xFF, bytePos = 0;
static void feedTransferByte(uint8_t b) {
  if (bytePos == 0) { bytePos = 1; return; }             // the address
  if (bytePos == 1) { ctrl = b; bytePos = 2; return; }   // the control byte
  if (ctrl == 0x00) feedCmd(b);
  else { putByte(b); ++dataBytes; }
}

// ---- a model of the I2C bus, pull-ups and all ---------------------------
static uint8_t sdaLevel() {
  // Open drain: OUTPUT gives what was written, INPUT gives HIGH from the pull-up
  return (HostArduino::pinModeOf(PIN_SDA) == OUTPUT)
             ? HostArduino::pinValue(PIN_SDA) : 1;
}
static uint8_t sclLevel() {
  return (HostArduino::pinModeOf(PIN_SCL) == OUTPUT)
             ? HostArduino::pinValue(PIN_SCL) : 1;
}

static uint8_t prevScl = 1, prevSda = 1, acc = 0, bits = 0;
static bool inFrame = false;
static long starts = 0, stops = 0, bytesSeen = 0;

/// Called on every pin change; follows the waveform.
static void sample() {
  const uint8_t scl = sclLevel(), sda = sdaLevel();
  if (scl == 1 && prevScl == 1 && sda != prevSda) {
    // SDA moving while SCL is high is a START or a STOP
    if (sda == 0) { ++starts; inFrame = true; bits = 0; acc = 0; bytePos = 0; }
    else          { ++stops;  inFrame = false; }
  } else if (scl == 1 && prevScl == 0 && inFrame) {
    // One bit on the rising edge. The ninth is the ACK, and is discarded
    if (bits < 8) { acc = (uint8_t)((acc << 1) | sda); }
    if (++bits == 9) { feedTransferByte(acc); ++bytesSeen; bits = 0; acc = 0; }
  }
  prevScl = scl; prevSda = sda;
}
static void onWrite(uint8_t, uint8_t, void*) { sample(); }
static void onMode(uint8_t, uint8_t, void*) { sample(); }
static int onRead(uint8_t pin, uint8_t held, void*) {
  (void)held;
  // The pull-up: a released pin reads HIGH
  return (HostArduino::pinModeOf(pin) == OUTPUT) ? HostArduino::pinValue(pin) : 1;
}

// ---- watching the Wire side ----------------------------------------------
static uint8_t onWire(uint8_t addr, const uint8_t* d, size_t len, bool, void*) {
  if (addr != ADDR || len == 0) return 2;
  if (d[0] == 0x00) { for (size_t i = 1; i < len; ++i) feedCmd(d[i]); }
  else if (d[0] == 0x40) { for (size_t i = 1; i < len; ++i) { putByte(d[i]); ++dataBytes; } }
  return 0;
}

static void scene(TinyGFX& g) {
  g.drawRect(0, 0, W, H, TFT_WHITE);
  g.fillRect(8, 8, 40, 16, TFT_WHITE);
  g.drawCircle(96, 32, 20, TFT_WHITE);
  g.drawLine(0, 0, W - 1, H - 1, TFT_WHITE);
}
static uint8_t viaSoft[W * PAGES], viaWire[W * PAGES];

void setup() {
  Serial.begin(115200);
#if !TGFX_HOST_PROBE
  Serial.println("TEST skip softi2c");
  Serial.println("TEST done");
  return;
#else
  tgfxTestBegin("softi2c");

  // --- bit-banged -----------------------------------------------------------
  for (int i = 0; i < W * PAGES; ++i) model[i] = 0;
  {
    TinyGFXBusSoftI2C bus(PIN_SDA, PIN_SCL, ADDR);
    TinyGFXDriverSSD1306 panel(bus, fbSoft, W, H);
    TinyGFX lcd(panel);
    HostArduino::setPinReadHook(onRead, nullptr);
    HostArduino::setPinWriteHook(onWrite, nullptr);
    HostArduino::setPinModeHook(onMode, nullptr);
    dataBytes = 0;
    lcd.begin();
    scene(lcd);
    panel.display();
    HostArduino::clearPinHooks();
    tgfxReport("soft_starts", starts);
    tgfxReport("soft_stops", stops);
    tgfxReport("soft_bytes_seen", bytesSeen);
    tgfxReport("soft_data_bytes", (long)dataBytes);
  }
  for (int i = 0; i < W * PAGES; ++i) viaSoft[i] = model[i];

  // --- Wire -----------------------------------------------------------------
  for (int i = 0; i < W * PAGES; ++i) model[i] = 0;
  colStart = 0; colEnd = W - 1; curCol = 0; curPage = 0; pendingCmd = 0; argIndex = 0;
  {
    TinyGFXBusI2C bus(Wire, ADDR);
    TinyGFXDriverSSD1306 panel(bus, fbWire, W, H);
    TinyGFX lcd(panel);
    Wire.setWriteHook(onWire, nullptr);
    Wire.begin();
    dataBytes = 0;
    lcd.begin();
    scene(lcd);
    panel.display();
    Wire.setWriteHook(nullptr, nullptr);
    tgfxReport("wire_data_bytes", (long)dataBytes);
  }
  for (int i = 0; i < W * PAGES; ++i) viaWire[i] = model[i];

  long diff = 0, lit = 0;
  for (int i = 0; i < W * PAGES; ++i) {
    if (viaSoft[i] != viaWire[i]) ++diff;
    for (int b = 0; b < 8; ++b) { if ((viaWire[i] >> b) & 1) ++lit; }
  }
  tgfxReport("soft_vs_wire_diff", diff);
  tgfxReport("lit", lit);

  tgfxTestDone();
#endif
}
void loop() {}
