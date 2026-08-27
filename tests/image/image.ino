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

  tgfxTestDone();
}
void loop() {}
