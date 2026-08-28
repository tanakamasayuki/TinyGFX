// 本番のバス実装をホストで通しで検証する。
//
//   TinyGFX → PanelST7789 → **TinyGFXBusSoftSPI / TinyGFXBusSPI** → 線
//            → TinyGFX 側のパネル模型 → 仮想 GRAM → PPM
//
// ここまで来て初めて、ビット順・DC を落とすタイミング・トランザクション中の
// CS が検証できる。BusCapture だけでは Bus の手前までしか見えない。
#define TGFX_HOST_PROBE_SPI 1
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>
#include <tgfx_host_probe.h>

static const int W = 32, H = 32;
static const uint8_t PIN_SCK = 18, PIN_MOSI = 23, PIN_DC = 5, PIN_CS = 15;

static uint16_t gram[W * H];
TinyGFXBusCapture sink(gram, W, H);

void setup() {
  Serial.begin(115200);
#if !TGFX_HOST_PROBE
  Serial.println("TEST skip hostbus");
  Serial.println("TEST done");
  return;
#else
  tgfxTestBegin("hostbus");
  SPI.begin();  // the sketch owns the bus; TinyGFX never begins it

  // ---- ソフト SPI（CH32V003 の既定バス） --------------------------------
  {
    TinyGFXBusSoftSPI bus(PIN_SCK, PIN_MOSI, PIN_DC, PIN_CS);
    TinyGFXPanelST7789 panel(bus, W, H);
    TinyGFX lcd(panel);
    TgfxPinProbe probe(sink, PIN_SCK, PIN_MOSI, PIN_DC, PIN_CS);
    probe.attach();
    lcd.begin();
    sink.fill(0);
    sink.resetCounters();
    probe.resetCounters();
    lcd.fillScreen(0x001F);
    lcd.fillRect(4, 4, 8, 8, 0xF800);
    lcd.drawPixel(20, 3, 0x07E0);
    tgfxReport("soft_bytes", (long)probe.byteCount());
    tgfxReport("soft_pixels", (long)sink.pixelCount());
    tgfxShot("soft", gram, W, H);
    probe.detach();
  }

  // ---- ハードウェア SPI ---------------------------------------------------
  {
    TinyGFXBusSPI bus(SPI, PIN_DC, PIN_CS, 24000000UL);
    TinyGFXPanelST7789 panel(bus, W, H);
    TinyGFX lcd(panel);
    TgfxSpiProbe probe(sink, PIN_DC);
    probe.attach();
    lcd.begin();
    sink.fill(0);
    sink.resetCounters();
    probe.resetCounters();
    lcd.fillScreen(0x001F);
    lcd.fillRect(4, 4, 8, 8, 0xF800);
    lcd.drawPixel(20, 3, 0x07E0);
    tgfxReport("hw_bytes", (long)probe.byteCount());
    tgfxReport("hw_pixels", (long)sink.pixelCount());
    tgfxShot("hw", gram, W, H);

    // SPISettings が意図どおりか（ST7789 は MODE0 / MSB first）
    // 1.5.0 で読み取り専用アクセサが入ったので、そちらを使う
    const SPISettings s = SPI.settings();
    tgfxReport("spi_clock", (long)s.clock());
    tgfxReport("spi_bitorder", (long)s.bitOrder());
    tgfxReport("spi_mode", (long)s.dataMode());
    tgfxReport("spi_in_transaction", SPI.inTransaction() ? 1 : 0);
    probe.detach();
  }

  // CS はトランザクションの外で HIGH に戻っていること
  tgfxReport("cs_idle_high", digitalRead(PIN_CS) == HIGH ? 1 : 0);
  tgfxReport("dc_idle_high", digitalRead(PIN_DC) == HIGH ? 1 : 0);

  tgfxTestDone();
#endif
}
void loop() {}
