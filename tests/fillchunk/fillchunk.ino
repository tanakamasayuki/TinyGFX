// TINYGFX_FILL_CHUNK を有効にしても**線の上のバイトが変わらない**こと。
//
// このスイッチは速さのためだけのもので、絵も転送量も変えてはいけない。
// ソフト SPI（まとめ書きを持たない）と、まとめ書きを有効にしたハードウェア SPI で
// 同じ絵を描き、**画素もバイト数も一致すること**を見る。
//
// ホストの観測フックはブロック転送も 1 バイトずつ拾うので、
// `SPI.transfer(buf, len)` に変えても数え方は変わらない。
#define TGFX_HOST_PROBE_SPI 1
#define TINYGFX_FILL_CHUNK 32  // **BusSPI.h より前に定義する**
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>
#include <tgfx_host_probe.h>

static const int W = 32, H = 32;
static const uint8_t PIN_SCK = 18, PIN_MOSI = 23, PIN_DC = 5, PIN_CS = 15;

static uint16_t gram[W * H];
TinyGFXBusCapture sink(gram, W, H);

// まとめ書きの単位（32 画素）に対して、割り切れる・余る・足りない場面を混ぜる。
static uint16_t image[7 * 5];

static void draw(TinyGFX& lcd) {
  lcd.fillScreen(0x001F);              // 1,024 画素 = 32 の倍数
  lcd.fillRect(4, 4, 9, 7, 0xF800);    // 63 画素 = 単位 1 回 + 端数 31
  lcd.fillRect(20, 20, 3, 3, 0x07E0);  // 9 画素 = 単位に足りない
  lcd.drawPixel(31, 0, 0xFFFF);        // 1 画素
  lcd.pushImage(1, 24, 7, 5, image);   // writePixels 経由（35 画素）
}

void setup() {
  Serial.begin(115200);
#if !TGFX_HOST_PROBE
  Serial.println("TEST skip fillchunk");
  Serial.println("TEST done");
  return;
#else
  tgfxTestBegin("fillchunk");
  SPI.begin();  // the sketch owns the bus; TinyGFX never begins it
  for (int i = 0; i < 7 * 5; ++i) image[i] = (uint16_t)(i * 1493 + 7);

  // ---- ソフト SPI（まとめ書きを持たない。これが基準） --------------------
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
    draw(lcd);
    tgfxReport("plain_bytes", (long)probe.byteCount());
    tgfxReport("plain_pixels", (long)sink.pixelCount());
    tgfxShot("plain", gram, W, H);
    probe.detach();
  }

  // ---- ハードウェア SPI + まとめ書き ------------------------------------
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
    draw(lcd);
    tgfxReport("chunk_bytes", (long)probe.byteCount());
    tgfxReport("chunk_pixels", (long)sink.pixelCount());
    tgfxShot("chunk", gram, W, H);
    probe.detach();
  }

  tgfxReport("chunk_size", TINYGFX_FILL_CHUNK);
  tgfxTestDone();
#endif
}
void loop() {}
