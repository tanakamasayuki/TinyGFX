// TinyGFX - FlickerFree
//
// フレームバッファを持たないので、消してから描くとちらつく。
// TinyGFXTileCanvas は画面を横帯に分け、小さな RAM バッファへ 1 帯ずつ描いてから
// 転送する。必要な RAM は「幅 × 帯の行数 × 2 バイト」だけ。
//
// 描画関数は帯の数だけ呼ばれるが、座標は常に画面全体のものでよい。
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/PanelST7789.h>
#include <TinyGFX/PanelMemory.h>
#include <TinyGFX/TileCanvas.h>

static const int16_t WIDTH = 240;
static const int16_t HEIGHT = 240;

TinyGFXBusSoftSPI bus(/*sck*/5, /*mosi*/6, /*dc*/3, /*cs*/4);
TinyGFXPanelST7789 panel(bus, WIDTH, HEIGHT, /*rst*/2);

// 帯バッファ。必要な RAM は「幅 × 行数 × 2 バイト」だけ。
//
//   1 行  =   480 B  ← CH32V003（RAM 2KB）はここ
//   4 行  = 1,920 B  ← Uno R3 でも厳しい
//  16 行  = 7,680 B  ← ESP32 なら余裕。転送回数が減るぶん速い
//
// **行数を変えても絵は 1 画素も変わらない**（tests/tile/ がそれを検査している）。
// 速さと RAM のトレードオフだけ。
#if defined(__AVR__) || defined(__riscv)
static const int16_t BAND_ROWS = 1;
#else
static const int16_t BAND_ROWS = 16;
#endif
static uint16_t band[WIDTH * BAND_ROWS];
TinyGFXTileCanvas canvas(panel, band, sizeof(band) / sizeof(band[0]));

struct Ball {
  int16_t x, y, dx, dy;
};
static Ball ball = {40, 40, 3, 2};

// 帯ごとに呼ばれる。ここに重い処理を書くと帯の数だけ走るので注意。
static void drawScene(TinyGFX& g, void* ctx) {
  const Ball* b = (const Ball*)ctx;
  g.drawRect(0, 0, WIDTH, HEIGHT, TFT_DARKGREY);
  g.fillCircle(b->x, b->y, 16, TFT_CYAN);
}

void setup() {
  canvas.begin();
  canvas.setBackgroundColor(TFT_BLACK);
}

void loop() {
  ball.x += ball.dx;
  ball.y += ball.dy;
  if (ball.x < 17 || ball.x > WIDTH - 18) ball.dx = -ball.dx;
  if (ball.y < 17 || ball.y > HEIGHT - 18) ball.dy = -ball.dy;

  canvas.render(drawScene, &ball);
  delay(16);
}
