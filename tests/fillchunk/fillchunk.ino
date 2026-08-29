// Turning TINYGFX_FILL_CHUNK on must **not change a byte on the wire**.
//
// The switch exists only to go faster; neither the picture nor the traffic may
// change. The same picture is drawn on software SPI (which has no block write)
// and on hardware SPI with block writes on, and **both the pixels and the byte
// count must match**.
//
// The host probe picks up a block transfer one byte at a time, so switching to
// `SPI.transfer(buf, len)` does not change how anything is counted.
#define TGFX_HOST_PROBE_SPI 1
#define TINYGFX_FILL_CHUNK 32  // **must come before BusSPI.h**
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

// Against the block size of 32 pixels: cases that divide evenly, leave a
// remainder, and fall short of one block.
static uint16_t image[7 * 5];

static void draw(TinyGFX& lcd) {
  lcd.fillScreen(0x001F);              // 1,024 pixels: a multiple of 32
  lcd.fillRect(4, 4, 9, 7, 0xF800);    // 63 pixels: one block plus 31
  lcd.fillRect(20, 20, 3, 3, 0x07E0);  // 9 pixels: short of one block
  lcd.drawPixel(31, 0, 0xFFFF);        // one pixel
  lcd.pushImage(1, 24, 7, 5, image);   // through writePixels (35 pixels)
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

  // ---- software SPI: no block write, and the baseline --------------------
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

  // ---- hardware SPI with block writes ------------------------------------
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
