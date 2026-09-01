// TinyGFX - FlickerFree
//
// With no framebuffer, clearing and then drawing flickers. TinyGFXTileCanvas
// splits the screen into horizontal bands, draws one band at a time into a
// small RAM buffer and pushes it. All it needs is width * band rows * 2 bytes.
//
// The drawing function runs once per band, but its coordinates are always
// whole-screen.
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/panels/ST7789_240x240.h>
#include <TinyGFX/MemoryTarget.h>
#include <TinyGFX/TileCanvas.h>

static const int16_t WIDTH = 240;
static const int16_t HEIGHT = 240;

TinyGFXBusSoftSPI bus(/*sck*/5, /*mosi*/6, /*dc*/3, /*cs*/4);
TinyGFXPanelST7789_240x240 panel(bus, /*rst*/2);

// The band buffer. All it costs is width * rows * 2 bytes.
//
//    1 row  =   480 B  <- where a CH32V003 (2 KB of RAM) has to sit
//    4 rows = 1,920 B  <- already tight on an Uno R3
//   16 rows = 7,680 B  <- comfortable on an ESP32, and faster: fewer transfers
//
// Changing the row count does not change the image by a single pixel
// (tests/tile/ checks exactly that). It trades speed against RAM, nothing more.
#if defined(__AVR__) || defined(__riscv)
static const int16_t BAND_ROWS = 1;
#else
static const int16_t BAND_ROWS = 16;
#endif
static uint16_t band[WIDTH * BAND_ROWS];
TinyGFXTileCanvas canvas(panel, band, sizeof(band) / sizeof(band[0]));

struct Ball {
  int16_t x;
  int16_t y;
  int16_t dx;
  int16_t dy;
};
static Ball ball = {40, 40, 3, 2};


void setup() {
  canvas.begin();
  canvas.setBackgroundColor(TFT_BLACK);
}

void loop() {
  ball.x += ball.dx;
  ball.y += ball.dy;
  if (ball.x < 17 || ball.x > WIDTH - 18) ball.dx = -ball.dx;
  if (ball.y < 17 || ball.y > HEIGHT - 18) ball.dy = -ball.dy;

  // Runs once per band. Anything expensive in here runs that many times.
  //
  // A lambda can just read `ball`; nothing has to be packed behind a void*.
  // The other form, canvas.render(fn, &ctx) with
  // `void fn(TinyGFX&, void*)`, is still there for code that would rather
  // have a named function. They cost the same.
  canvas.render([&](TinyGFX& g) {
    g.drawRect(0, 0, WIDTH, HEIGHT, TFT_DARKGREY);
    g.fillCircle(ball.x, ball.y, 16, TFT_CYAN);
  });
  delay(16);
}
