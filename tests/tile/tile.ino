// The tiled-rendering invariant:
//   whatever the band height, not one pixel differs from drawing directly
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <TinyGFX/PanelMemory.h>
#include <TinyGFX/TileCanvas.h>
#include <tgfx_test.h>

static const int W = 48, H = 48;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static uint16_t band[W * 8];
static const uint16_t BG = 0x0841;

static void scene(TinyGFX& g, void* ctx) {
  (void)ctx;
  g.fillCircle(24, 24, 18, 0x001F);
  g.drawLine(0, 0, 47, 47, 0xF800);
  g.fillRect(4, 20, 40, 6, 0x07E0);
  g.drawRect(1, 1, 46, 46, 0xFFE0);
  g.fillTriangle(2, 46, 24, 10, 46, 40, 0xF81F);
}

static const int ROWS[] = {1, 2, 3, 5, 7, 8};

static uint16_t ref[W * H];
static void snapshot() { for (int i = 0; i < W * H; ++i) ref[i] = gram[i]; }
static long diffFromSnapshot() {
  long d = 0;
  for (int i = 0; i < W * H; ++i) { if (gram[i] != ref[i]) ++d; }
  return d;
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("tile");
  lcd.begin();

  // Drawn directly (the baseline)
  bus.fill(0);
  bus.resetCounters();
  lcd.fillScreen(BG);
  scene(lcd, nullptr);
  tgfxReport("direct_pixels", (long)bus.pixelCount());
  tgfxShot("direct", gram, W, H);

  for (unsigned i = 0; i < sizeof(ROWS) / sizeof(ROWS[0]); ++i) {
    const int rows = ROWS[i];
    TinyGFXTileCanvas canvas(panel, band, (uint32_t)(W * rows));
    canvas.setBackgroundColor(BG);
    canvas.begin();

    char key[16], name[16];
    snprintf(key, sizeof(key), "rows%d", rows);
    snprintf(name, sizeof(name), "tile%d", rows);

    tgfxReport2(key, "tilerows", (long)canvas.tileRows());
    bus.fill(0);
    bus.resetCounters();
    const bool ok = canvas.render(scene);
    tgfxReport2(key, "ok", ok ? 1 : 0);
    tgfxReport2(key, "pixels", (long)bus.pixelCount());
    tgfxShot(name, gram, W, H);
  }

  // A lambda must draw exactly what the function pointer + void* form draws.
  // The code path differs (a trampoline sits in between), so pin it here.
  {
    TinyGFXTileCanvas c(panel, band, (uint32_t)(W * 4));
    c.setBackgroundColor(BG);
    c.begin();
    bus.fill(0);
    bus.resetCounters();
    c.render(scene);
    tgfxReport("fnptr_pixels", (long)bus.pixelCount());
    snapshot();

    // With a capture. Count the calls too, and check they match the bands.
    int bands = 0;
    TinyGFXTileCanvas c2(panel, band, (uint32_t)(W * 4));
    c2.setBackgroundColor(BG);
    c2.begin();
    bus.fill(0);
    bus.resetCounters();
    c2.render([&](TinyGFX& g) { ++bands; scene(g, nullptr); });
    tgfxReport("lambda_pixels", (long)bus.pixelCount());
    tgfxReport("lambda_diff", diffFromSnapshot());
    tgfxReport("lambda_bands", (long)bands);
    tgfxReport("lambda_tilerows", (long)c2.tileRows());
  }

  // A buffer too small for even one row cannot draw
  {
    TinyGFXTileCanvas tiny(panel, band, 4);
    tiny.begin();
    tgfxReport("toosmall_rows", (long)tiny.tileRows());
    tgfxReport("toosmall_ok", tiny.render(scene) ? 1 : 0);
  }

  // With auto-clear off, the background is not repainted
  {
    TinyGFXTileCanvas c(panel, band, (uint32_t)(W * 4));
    c.setBackgroundColor(BG);
    c.begin();
    c.setAutoClear(false);
    bus.fill(0);
    bus.resetCounters();
    c.render(scene);
    tgfxShot("noautoclear", gram, W, H);
  }

  tgfxTestDone();
}
void loop() {}
