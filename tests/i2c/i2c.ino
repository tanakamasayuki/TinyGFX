// I2C のモノクロ OLED（SSD1306）を通しで検証する。
//
//   TinyGFX → PanelSSD1306 → 本番の TinyGFXBusI2C → Wire
//           → ホストの Wire 観測フック → SSD1306 の模型 → ビットマップ → PPM
//
// SPI のカラーパネルとの違いを見るのが目的:
//   - フレームバッファを持つこと（1bpp。RGB565 を「0 でなければ点灯」で落とす）
//   - display() を呼ぶまで転送されないこと
//   - 変更のあったページだけ流すこと
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/PanelSSD1306.h>
#include <tgfx_test.h>
#include <TinyGFX/FontCell.h>
#include <tinygfx_font5x7.h>

static const TinyGFXFontRef font5x7 = {&tinygfxFont5x7, &tinygfxFontCellOps, nullptr};
#include <Wire.h>

static const int W = 128, H = 64;
static const uint8_t ADDR = 0x3C;

static uint8_t fb[W * H / 8];
TinyGFXBusI2C bus(ADDR);
TinyGFXPanelSSD1306 panel(bus, fb, W, H);
TinyGFX lcd(panel);

// ---- 受け側の模型（TinyGFX 側が持つ。コアは SSD1306 を知らない） ----------
static uint8_t model[W * H / 8];
static uint16_t colStart = 0, colEnd = W - 1, pageStart = 0, pageEnd = 7;
static uint16_t curCol = 0, curPage = 0;
static uint8_t pendingCmd = 0, argIndex = 0, args[2] = {0, 0};
static uint32_t dataBytes = 0, txCount = 0;

static void feedCmd(uint8_t c) {
  if (pendingCmd != 0) {
    args[argIndex++] = c;
    if (argIndex == 2) {
      if (pendingCmd == 0x21) { colStart = args[0]; colEnd = args[1]; curCol = colStart; }
      else                    { pageStart = args[0]; pageEnd = args[1]; curPage = pageStart; }
      pendingCmd = 0;
      argIndex = 0;
    }
    return;
  }
  if (c == 0x21 || c == 0x22) { pendingCmd = c; argIndex = 0; }
}

static void putByte(uint8_t b) {
  if (curPage < (uint16_t)(H / 8) && curCol < (uint16_t)W) {
    model[(uint32_t)curPage * W + curCol] = b;
  }
  if (curCol >= colEnd) { curCol = colStart; ++curPage; }
  else { ++curCol; }
}

static uint8_t onWire(uint8_t addr, const uint8_t* d, size_t len, bool stop, void* user) {
  (void)stop; (void)user;
  if (addr != ADDR || len == 0) return 2;  // アドレス NACK
  ++txCount;
  if (d[0] == 0x00) {
    for (size_t i = 1; i < len; ++i) feedCmd(d[i]);
  } else if (d[0] == 0x40) {
    for (size_t i = 1; i < len; ++i) { putByte(d[i]); ++dataBytes; }
  }
  return 0;
}

static uint16_t image[W * H];
static void shot(const char* name) {
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const uint8_t bits = model[(uint32_t)(y >> 3) * W + x];
      image[(uint32_t)y * W + x] = (bits >> (y & 7)) & 1 ? 0xFFFF : 0x0000;
    }
  }
  tgfxShot(name, image, W, H);
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("i2c");
  Wire.setWriteHook(onWire, nullptr);

  lcd.begin();
  tgfxReport("init_transactions", (long)txCount);

  // --- display() を呼ぶまで何も流れない ------------------------------------
  dataBytes = 0;
  lcd.fillScreen(TFT_WHITE);
  tgfxReport("bytes_before_display", (long)dataBytes);
  panel.display();
  tgfxReport("bytes_full", (long)dataBytes);
  shot("full");

  // --- 一部だけ変えると、変わったページだけ流れる ---------------------------
  panel.clearBuffer(false);
  panel.display();
  dataBytes = 0;
  lcd.fillRect(8, 8, 32, 8, TFT_WHITE);   // ページ 1 だけに収まる
  panel.display();
  tgfxReport("bytes_one_page", (long)dataBytes);
  shot("onepage");

  // --- 図形と文字 -----------------------------------------------------------
  panel.clearBuffer(false);
  panel.display();
  dataBytes = 0;
  lcd.drawRect(0, 0, W, H, TFT_WHITE);
  lcd.drawLine(0, 0, W - 1, H - 1, TFT_WHITE);
  lcd.fillCircle(96, 32, 12, TFT_WHITE);
  lcd.setFont(&font5x7);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("12:34", 8, 24);
  panel.display();
  tgfxReport("bytes_scene", (long)dataBytes);
  tgfxReport("transactions_scene", (long)txCount);
  shot("scene");

  // --- 回転すると幅と高さが入れ替わる ---------------------------------------
  lcd.setRotation(1);
  tgfxReport("rot1_w", (long)lcd.width());
  tgfxReport("rot1_h", (long)lcd.height());
  panel.clearBuffer(false);
  panel.display();
  dataBytes = 0;
  lcd.fillRect(0, 0, 8, 24, TFT_WHITE);   // 回転後の左上
  panel.display();
  shot("rot1");
  lcd.setRotation(0);

  tgfxTestDone();
}
void loop() {}
