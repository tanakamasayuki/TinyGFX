// pushImage: placement, cropping, and the transparent form.
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>

static const int W = 16, H = 16;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static const uint16_t R = 0xF800, G = 0x07E0, B = 0x001F, WH = 0xFFFF;
static const uint16_t img4[16] = {
    R, R, G, G,
    R, R, G, G,
    B, B, WH, WH,
    B, B, WH, WH,
};
static void reset() { bus.fill(0); bus.resetCounters(); }

// A 1bpp bitmap whose width is not a multiple of 8. **Every row starts on a
// byte boundary**, so a width of 5 is still one byte a row; the right 3 bits
// are padding.
static const uint8_t bmp5x4[4] TINYGFX_FONT_PROGMEM = {
    0xF8,  // 11111
    0x88,  // 10001
    0x88,  // 10001
    0xF8,  // 11111
};
// Exactly 8 wide, with several breaks part way along a row
static const uint8_t bmp8x3[3] TINYGFX_FONT_PROGMEM = {
    0xA5,  // 10100101
    0x00,  // 00000000
    0xFF,  // 11111111
};

/// The same picture drawn with drawPixel (the other side of the invariant)
static void byPixel(TinyGFX& g, int16_t x, int16_t y, const uint8_t* bm,
                    int16_t w, int16_t h, uint16_t c) {
  const int16_t bpr = (int16_t)((w + 7) >> 3);
  for (int16_t r = 0; r < h; ++r) {
    for (int16_t col = 0; col < w; ++col) {
      if ((tinygfx_rd8(&bm[r * bpr + (col >> 3)]) >> (7 - (col & 7))) & 1) {
        g.drawPixel((int16_t)(x + col), (int16_t)(y + r), c);
      }
    }
  }
}

/// Compare the whole gram
static long diffFrom(const uint16_t* other) {
  long d = 0;
  for (int i = 0; i < W * H; ++i) {
    if (gram[i] != other[i]) ++d;
  }
  return d;
}
static uint16_t snap[W * H];

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("image");
  lcd.begin();

  reset(); lcd.pushImage(2, 2, 4, 4, img4);
  tgfxReport("plain_pixels", (long)bus.pixelCount());
  tgfxShot("plain", gram, W, H);

  reset(); lcd.pushImage(-2, -2, 4, 4, img4);
  tgfxReport("topleft_pixels", (long)bus.pixelCount());
  tgfxShot("topleft", gram, W, H);

  reset(); lcd.pushImage(W - 2, H - 2, 4, 4, img4);
  tgfxReport("bottomright_pixels", (long)bus.pixelCount());
  tgfxShot("bottomright", gram, W, H);

  reset(); lcd.setClipRect(4, 4, 4, 4); lcd.pushImage(2, 2, 4, 4, img4); lcd.clearClipRect();
  tgfxReport("clipped_pixels", (long)bus.pixelCount());
  tgfxShot("clipped", gram, W, H);

  reset(); lcd.pushImage(2, 2, 4, 4, img4, R);
  tgfxReport("transparent_pixels", (long)bus.pixelCount());
  tgfxShot("transparent", gram, W, H);

  reset(); lcd.pushImage(100, 100, 4, 4, img4);
  tgfxReport("offscreen_pixels", (long)bus.pixelCount());

  reset(); lcd.pushImage(2, 2, 0, 4, img4);
  tgfxReport("zero_pixels", (long)bus.pixelCount());

  // Extreme coordinates. If the clipping arithmetic (`_clipX0 - x`) overflows
  // int16_t, it reads in front of the source image. **Not one pixel may be sent.**
  reset();
  lcd.pushImage(-32768, 0, 4, 4, img4);
  lcd.pushImage(32767, 0, 4, 4, img4);
  lcd.pushImage(0, -32768, 4, 4, img4);
  lcd.pushImage(0, 32767, 4, 4, img4);
  lcd.pushImage(-32768, -32768, 4, 4, img4);
  tgfxReport("extreme_pixels", (long)bus.pixelCount());

  // --- byte swapping --------------------------------------------------------
  //
  // TinyGFX has no setSwapBytes() (DECISIONS.ja.md D29); `tinygfx_swapBytes565`
  // takes its place. Two things are checked: that **applying it twice restores
  // the original**, and that drawing a swapped array differs from the original
  // (i.e. that it does anything at all).
  {
    reset(); lcd.pushImage(2, 2, 4, 4, img4);
    for (int i = 0; i < W * H; ++i) snap[i] = gram[i];

    uint16_t tmp[16];
    for (int i = 0; i < 16; ++i) tmp[i] = img4[i];
    tinygfx_swapBytes565(tmp, 16);
    reset(); lcd.pushImage(2, 2, 4, 4, tmp);
    tgfxReport("swapped_diff", diffFrom(snap));

    tinygfx_swapBytes565(tmp, 16);
    long same = 0;
    for (int i = 0; i < 16; ++i) { if (tmp[i] == img4[i]) ++same; }
    tgfxReport("swap_roundtrip", same);

    // A length of 0 must touch nothing
    uint16_t one = 0x1234;
    tinygfx_swapBytes565(&one, 0);
    tgfxReport("swap_zero_kept", one == 0x1234 ? 1 : 0);
  }

  // --- 1bpp bitmaps ---------------------------------------------------------
  //
  // **Stated as an invariant.** Throwing a fillRect per run must not differ by
  // a pixel from placing each pixel with drawPixel. How runs get coalesced is a
  // question of speed; the picture may not change.
  //
  // Both a width of 5 (short of a byte, padded on the right) and a width of 8
  // (several breaks part way along a row) go through here.
  {
    reset(); lcd.drawBitmap(2, 3, bmp5x4, 5, 4, WH);
    for (int i = 0; i < W * H; ++i) snap[i] = gram[i];
    const long litA = (long)bus.pixelCount();
    reset(); byPixel(lcd, 2, 3, bmp5x4, 5, 4, WH);
    tgfxReport("bmp5_diff", diffFrom(snap));
    tgfxReport("bmp5_run_pixels", litA);
    tgfxReport("bmp5_px_pixels", (long)bus.pixelCount());

    reset(); lcd.drawBitmap(1, 1, bmp8x3, 8, 3, WH);
    for (int i = 0; i < W * H; ++i) snap[i] = gram[i];
    reset(); byPixel(lcd, 1, 1, bmp8x3, 8, 3, WH);
    tgfxReport("bmp8_diff", diffFrom(snap));

    // A 0 bit is left alone (transparent)
    reset();
    lcd.fillRect(0, 0, W, H, R);
    lcd.drawBitmap(1, 1, bmp8x3, 8, 3, WH);
    long red = 0;
    for (int i = 0; i < W * H; ++i) {
      if (gram[i] == R) ++red;
    }
    tgfxReport("bmp_kept_bg", red);

    // Off screen must not fall over
    reset();
    lcd.drawBitmap(-3, -2, bmp8x3, 8, 3, WH);
    lcd.drawBitmap((int16_t)(W - 2), (int16_t)(H - 1), bmp8x3, 8, 3, WH);
    tgfxReport("bmp_offscreen_ok", 1);
  }

  tgfxTestDone();
}
void loop() {}
