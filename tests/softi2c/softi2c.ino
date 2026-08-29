// **ビット叩きの I2C が、Wire と同じバイト列を出すか。**
//
// 実機が無くても線の上の波形は見られる。ホストコアのピンフックで
// **I2C のバスそのものを模す**:
//
//   - オープンドレイン: OUTPUT+LOW で引き下げ、INPUT で手放す
//   - **読み出しフックがプルアップの役** — INPUT のピンは HIGH に見える
//   - SCL の立ち上がりで SDA を読み、START / STOP を検出してバイトに戻す
//
// 戻したバイト列を SSD1306 の模型に流し、**Wire 経由で同じ絵を描いた結果と
// 突き合わせる。** 1 画素でも違えば実装が違う。
#define TGFX_HOST_PROBE_SPI 1
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/BusSoftI2C.h>
#include <TinyGFX/PanelSSD1306.h>
#include <tgfx_test.h>
#include <tgfx_host_probe.h>
#include <Wire.h>

static const int W = 128, H = 64, PAGES = H / 8;
static const uint8_t ADDR = 0x3C;
static const uint8_t PIN_SDA = 4, PIN_SCL = 5;

static uint8_t fbSoft[W * H / 8], fbWire[W * H / 8];

// ---- SSD1306 の模型（tests/i2c と同じ） ---------------------------------
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
/// 1 転送ぶんのバイト列を SSD1306 として解釈する（先頭はアドレス）。
static uint8_t ctrl = 0xFF, bytePos = 0;
static void feedTransferByte(uint8_t b) {
  if (bytePos == 0) { bytePos = 1; return; }             // アドレス
  if (bytePos == 1) { ctrl = b; bytePos = 2; return; }   // 制御バイト
  if (ctrl == 0x00) feedCmd(b);
  else { putByte(b); ++dataBytes; }
}

// ---- I2C バスの模型（プルアップつき） -----------------------------------
static uint8_t sdaLevel() {
  // オープンドレイン: OUTPUT なら書いた値、INPUT ならプルアップで HIGH
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

/// ピンの状態が変わるたびに呼ばれ、波形を追う。
static void sample() {
  const uint8_t scl = sclLevel(), sda = sdaLevel();
  if (scl == 1 && prevScl == 1 && sda != prevSda) {
    // SCL が高いまま SDA が動いたら START / STOP
    if (sda == 0) { ++starts; inFrame = true; bits = 0; acc = 0; bytePos = 0; }
    else          { ++stops;  inFrame = false; }
  } else if (scl == 1 && prevScl == 0 && inFrame) {
    // 立ち上がりで 1 ビット読む。9 ビット目は ACK なので捨てる
    if (bits < 8) { acc = (uint8_t)((acc << 1) | sda); }
    if (++bits == 9) { feedTransferByte(acc); ++bytesSeen; bits = 0; acc = 0; }
  }
  prevScl = scl; prevSda = sda;
}
static void onWrite(uint8_t, uint8_t, void*) { sample(); }
static void onMode(uint8_t, uint8_t, void*) { sample(); }
static int onRead(uint8_t pin, uint8_t held, void*) {
  (void)held;
  // プルアップ: 手放されたピンは HIGH に見える
  return (HostArduino::pinModeOf(pin) == OUTPUT) ? HostArduino::pinValue(pin) : 1;
}

// ---- Wire 側を覗く -------------------------------------------------------
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

  // --- ビット叩き -----------------------------------------------------------
  for (int i = 0; i < W * PAGES; ++i) model[i] = 0;
  {
    TinyGFXBusSoftI2C bus(PIN_SDA, PIN_SCL, ADDR);
    TinyGFXPanelSSD1306 panel(bus, fbSoft, W, H);
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
    TinyGFXPanelSSD1306 panel(bus, fbWire, W, H);
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
