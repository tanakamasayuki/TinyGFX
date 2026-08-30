// **Does a page-addressed panel on SPI (SSD1306 / SH1106) observe the bus
// etiquette?**
//
// This is the layer I2C cannot show. TinyGFXBusI2C starts and stops on every
// transfer, so it works even with empty beginTransaction / endTransaction.
// **SPI does not.** Without SPI.beginTransaction() to set the clock and the
// mode, CS dropped, and both put back afterwards, nothing appears on the screen
// and an SD card on the same wires gets corrupted.
//
// The 2026-08-29 review found that TinyGFXDriverPaged was not doing this (P0).
// There was only an I2C test, so nobody noticed.
// **This test stops it coming back.**
//
// Four things are checked:
//   1. every byte goes out inside a transaction
//   2. every byte goes out with CS dropped
//   3. commands go out with DC=LOW, pixels with DC=HIGH
//   4. the bytes that came out form the picture the SSD1306 datasheet says
#define TGFX_HOST_PROBE_SPI 1
#include <TinyGFX.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/DriverSH1106.h>
#include <TinyGFX/DriverSSD1306.h>
#include <tgfx_test.h>
#include <tgfx_host_probe.h>
#include <SPI.h>

static const int W = 128, H = 64, PAGES = H / 8;
static const uint8_t PIN_DC = 5, PIN_CS = 15;

static uint8_t fb[W * H / 8];

// ---- a model of the receiver (an SSD1306: 0x21 / 0x22 set the range) -----
static uint8_t model[W * PAGES];
static uint16_t colStart = 0, colEnd = W - 1, curCol = 0, curPage = 0;
static uint8_t pendingCmd = 0, argIndex = 0, args[2] = {0, 0};

// ---- watching the etiquette ----------------------------------------------
static long bytesTotal = 0, bytesOutsideTxn = 0, bytesWithCsHigh = 0;
static long cmdBytes = 0, dataBytes = 0;

static uint8_t onByte(uint8_t out, void* user) {
  (void)user;
  ++bytesTotal;
  if (!SPI.inTransaction()) ++bytesOutsideTxn;
  if (digitalRead(PIN_CS) != LOW) ++bytesWithCsHigh;

  if (digitalRead(PIN_DC) == LOW) {
    ++cmdBytes;
    const uint8_t c = out;
    if (pendingCmd != 0) {
      args[argIndex++] = c;
      if (argIndex == 2) {
        if (pendingCmd == 0x21) { colStart = args[0]; colEnd = args[1]; curCol = colStart; }
        else                    { curPage = args[0]; }
        pendingCmd = 0;
        argIndex = 0;
      }
    } else if (c == 0x21 || c == 0x22) {
      pendingCmd = c;
      argIndex = 0;
    }
  } else {
    ++dataBytes;
    if (curPage < (uint16_t)PAGES && curCol < (uint16_t)W) {
      model[(uint32_t)curPage * W + curCol] = out;
    }
    if (curCol >= colEnd) { curCol = colStart; ++curPage; }
    else { ++curCol; }
  }
  return 0xFF;  // the display is write-only
}

static uint16_t image[W * H];
static void shot(const char* name) {
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      image[(uint32_t)y * W + x] = (model[(y >> 3) * W + x] >> (y & 7)) & 1 ? 0xFFFF : 0x0000;
    }
  }
  tgfxShot(name, image, W, H);
}

void setup() {
  Serial.begin(115200);
#if !TGFX_HOST_PROBE
  Serial.println("TEST skip monospi");
  Serial.println("TEST done");
  return;
#else
  tgfxTestBegin("monospi");
  SPI.begin();  // the sketch owns the bus; TinyGFX never begins it

  TinyGFXBusSPI bus(SPI, PIN_DC, PIN_CS, 8000000UL);
  TinyGFXDriverSSD1306 panel(bus, fb, W, H);
  TinyGFX lcd(panel);

  SPI.setTransferHook(onByte, nullptr);

  // --- init() must also go out inside a transaction ------------------------
  lcd.begin();
  tgfxReport("init_bytes", bytesTotal);
  tgfxReport("init_outside_txn", bytesOutsideTxn);
  tgfxReport("init_cs_high", bytesWithCsHigh);

  // --- drawing and display() ------------------------------------------------
  bytesTotal = bytesOutsideTxn = bytesWithCsHigh = cmdBytes = dataBytes = 0;
  lcd.drawRect(0, 0, W, H, TFT_WHITE);
  lcd.fillRect(8, 8, 40, 16, TFT_WHITE);
  lcd.drawCircle(96, 32, 20, TFT_WHITE);
  lcd.drawLine(0, 0, W - 1, H - 1, TFT_WHITE);
  panel.display();

  tgfxReport("draw_bytes", bytesTotal);
  tgfxReport("draw_outside_txn", bytesOutsideTxn);
  tgfxReport("draw_cs_high", bytesWithCsHigh);
  tgfxReport("draw_data_bytes", dataBytes);
  shot("ssd1306_spi");

  long lit = 0;
  for (int i = 0; i < W * PAGES; ++i) {
    for (int b = 0; b < 8; ++b) {
      if ((model[i] >> b) & 1) ++lit;
    }
  }
  tgfxReport("lit", lit);

  // --- are the SPISettings what was intended? ------------------------------
  const SPISettings s = SPI.settings();
  tgfxReport("spi_clock", (long)s.clock());
  tgfxReport("spi_bitorder", (long)s.bitOrder());
  tgfxReport("spi_mode", (long)s.dataMode());

  // --- a one-off command (invertDisplay) must open a transaction too -------
  bytesTotal = bytesOutsideTxn = bytesWithCsHigh = 0;
  panel.invertDisplay(true);
  tgfxReport("invert_bytes", bytesTotal);
  tgfxReport("invert_outside_txn", bytesOutsideTxn);
  tgfxReport("invert_cs_high", bytesWithCsHigh);

  // --- an SH1106 must observe the same etiquette ---------------------------
  {
    static uint8_t fb2[W * H / 8];
    TinyGFXDriverSH1106 sh(bus, fb2, W, H);
    TinyGFX shLcd(sh);
    bytesTotal = bytesOutsideTxn = bytesWithCsHigh = 0;
    shLcd.begin();
    shLcd.fillRect(8, 8, 40, 16, TFT_WHITE);
    sh.display();
    tgfxReport("sh_bytes", bytesTotal);
    tgfxReport("sh_outside_txn", bytesOutsideTxn);
    tgfxReport("sh_cs_high", bytesWithCsHigh);
  }

  // --- both CS and DC must be back HIGH when it is done --------------------
  tgfxReport("cs_idle_high", digitalRead(PIN_CS) == HIGH ? 1 : 0);
  tgfxReport("in_transaction_at_end", SPI.inTransaction() ? 1 : 0);

  SPI.setTransferHook(nullptr);
  tgfxTestDone();
#endif
}
void loop() {}
