// pushImage の配置と切り取り、transparent 版。
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

// 幅が 8 の倍数でない 1bpp ビットマップ。**各行がバイト境界から始まる**ので
// 5 幅でも 1 行 1 バイト。右の 3 ビットは詰め物。
static const uint8_t bmp5x4[4] TINYGFX_FONT_PROGMEM = {
    0xF8,  // 11111
    0x88,  // 10001
    0x88,  // 10001
    0xF8,  // 11111
};
// 8 幅ちょうど。行の途中で切れ目が複数ある
static const uint8_t bmp8x3[3] TINYGFX_FONT_PROGMEM = {
    0xA5,  // 10100101
    0x00,  // 00000000
    0xFF,  // 11111111
};

/// 同じ絵を drawPixel で描く（不変条件の相手側）
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

/// gram を丸ごと比べる
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

  // --- 1bpp ビットマップ ---------------------------------------------------
  //
  // **不変条件で見る。** ラン単位で fillRect を投げる実装が、1 画素ずつ
  // drawPixel した結果と 1 画素も違わないこと。ランのまとめ方は速度の話で
  // あって、絵が変わってはいけない。
  //
  // 幅 5（バイト境界に満たない・右に詰め物）と幅 8（行の途中に切れ目が複数）
  // の両方を通す。
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

    // 0 のビットは触らないこと（透過）
    reset();
    lcd.fillRect(0, 0, W, H, R);
    lcd.drawBitmap(1, 1, bmp8x3, 8, 3, WH);
    long red = 0;
    for (int i = 0; i < W * H; ++i) {
      if (gram[i] == R) ++red;
    }
    tgfxReport("bmp_kept_bg", red);

    // 画面外でも落ちないこと
    reset();
    lcd.drawBitmap(-3, -2, bmp8x3, 8, 3, WH);
    lcd.drawBitmap((int16_t)(W - 2), (int16_t)(H - 1), bmp8x3, 8, 3, WH);
    tgfxReport("bmp_offscreen_ok", 1);
  }

  tgfxTestDone();
}
void loop() {}
