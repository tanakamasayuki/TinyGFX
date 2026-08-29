// **SPI に繋いだページ方式パネル（SSD1306 / SH1106）が、バスの作法を守るか。**
//
// I2C では見えない層をここで見る。TinyGFXBusI2C は転送ごとに開始と停止を
// するので beginTransaction / endTransaction が空でも動く。**SPI は違う。**
// SPI.beginTransaction() でクロックとモードを決め、CS を落とし、終わったら
// 戻さないと、絵が出ないうえに同じ線に繋がった SD カードを壊す。
//
// 2026-08-29 のレビューで、TinyGFXPanelPaged がこれを通していないことが
// 見つかった（P0）。I2C のテストしか無かったので誰も気づかなかった。
// **このテストがその再発を止める。**
//
// 見るのは 4 つ:
//   1. 全部のバイトがトランザクションの中で出ていること
//   2. 全部のバイトが CS を落とした状態で出ていること
//   3. コマンドは DC=LOW、画素は DC=HIGH で出ていること
//   4. 出たバイト列が SSD1306 の仕様どおりの絵になること
#define TGFX_HOST_PROBE_SPI 1
#include <TinyGFX.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/PanelSH1106.h>
#include <TinyGFX/PanelSSD1306.h>
#include <tgfx_test.h>
#include <tgfx_host_probe.h>
#include <SPI.h>

static const int W = 128, H = 64, PAGES = H / 8;
static const uint8_t PIN_DC = 5, PIN_CS = 15;

static uint8_t fb[W * H / 8];

// ---- 受け側の模型（SSD1306。0x21 / 0x22 で範囲を受けて流し込まれる） -----
static uint8_t model[W * PAGES];
static uint16_t colStart = 0, colEnd = W - 1, curCol = 0, curPage = 0;
static uint8_t pendingCmd = 0, argIndex = 0, args[2] = {0, 0};

// ---- 作法の監視 -----------------------------------------------------------
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
  return 0xFF;  // ディスプレイは書き込み専用
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
  TinyGFXPanelSSD1306 panel(bus, fb, W, H);
  TinyGFX lcd(panel);

  SPI.setTransferHook(onByte, nullptr);

  // --- init() もトランザクションの中で出ること ----------------------------
  lcd.begin();
  tgfxReport("init_bytes", bytesTotal);
  tgfxReport("init_outside_txn", bytesOutsideTxn);
  tgfxReport("init_cs_high", bytesWithCsHigh);

  // --- 描画と display() ----------------------------------------------------
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

  // --- SPISettings が意図どおりか -----------------------------------------
  const SPISettings s = SPI.settings();
  tgfxReport("spi_clock", (long)s.clock());
  tgfxReport("spi_bitorder", (long)s.bitOrder());
  tgfxReport("spi_mode", (long)s.dataMode());

  // --- 単発コマンド（invertDisplay）もトランザクションを開くこと -----------
  bytesTotal = bytesOutsideTxn = bytesWithCsHigh = 0;
  panel.invertDisplay(true);
  tgfxReport("invert_bytes", bytesTotal);
  tgfxReport("invert_outside_txn", bytesOutsideTxn);
  tgfxReport("invert_cs_high", bytesWithCsHigh);

  // --- SH1106 も同じ作法であること ----------------------------------------
  {
    static uint8_t fb2[W * H / 8];
    TinyGFXPanelSH1106 sh(bus, fb2, W, H);
    TinyGFX shLcd(sh);
    bytesTotal = bytesOutsideTxn = bytesWithCsHigh = 0;
    shLcd.begin();
    shLcd.fillRect(8, 8, 40, 16, TFT_WHITE);
    sh.display();
    tgfxReport("sh_bytes", bytesTotal);
    tgfxReport("sh_outside_txn", bytesOutsideTxn);
    tgfxReport("sh_cs_high", bytesWithCsHigh);
  }

  // --- 終わったら CS も DC も HIGH に戻っていること ------------------------
  tgfxReport("cs_idle_high", digitalRead(PIN_CS) == HIGH ? 1 : 0);
  tgfxReport("in_transaction_at_end", SPI.inTransaction() ? 1 : 0);

  SPI.setTransferHook(nullptr);
  tgfxTestDone();
#endif
}
void loop() {}
