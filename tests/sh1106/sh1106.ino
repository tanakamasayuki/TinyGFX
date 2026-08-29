// SH1106 の配線を、実機なしで検証する。
//
//   TinyGFX → PanelSH1106 → 本番の TinyGFXBusI2C → Wire
//           → ホストの Wire 観測フック → SH1106 の模型 → ビットマップ
//
// SH1106 が SSD1306 と違うのは 2 点だけで、どちらもここで見る。
//
//   1. **132 カラムの RAM に 128 カラムのガラス。** 絵の左端は RAM の
//      カラム 2。ずれると 2 画素ずれた絵になる
//   2. **カラム／ページの範囲指定コマンドが無い。** 0x21 / 0x22 が使えず、
//      ページごとにカーソルを置いて（0xB0|page、0x00 / 0x10 でカラム）
//      1 ページずつ流す
//
// **同じ絵を SSD1306 でも描いて、復号した結果が 1 ビットも違わないことを見る。**
// 共有部分（PanelPaged）が同じなので、ここで差が出たら転送側の不具合。
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/PanelSH1106.h>
#include <TinyGFX/PanelSSD1306.h>
#include <tgfx_test.h>
#include <TinyGFX/FontCell.h>
#include <tgfx_digits.h>
#include <Wire.h>

static const TinyGFXFontRef digitsFont = {&tgfxDigits, &tinygfxFontCellOps};

static const int W = 128, H = 64, PAGES = H / 8;
static const int RAM_W = 132;  // SH1106 が実際に持っているカラム数
static const uint8_t ADDR = 0x3C;

static uint8_t fbA[W * H / 8], fbB[W * H / 8];
TinyGFXBusI2C bus(Wire, ADDR);
TinyGFXPanelSH1106 sh(bus, fbA, W, H);
TinyGFXPanelSSD1306 ssd(bus, fbB, W, H);
TinyGFX shLcd(sh);
TinyGFX ssdLcd(ssd);

// ---- 受け側の模型 ---------------------------------------------------------
// SH1106 は 132 カラム、SSD1306 は 128 カラムぶんだけ使う。
static uint8_t model[RAM_W * PAGES];
static uint16_t curCol = 0, curPage = 0;
static uint16_t colStart = 0, colEnd = W - 1;
static uint8_t pendingCmd = 0, argIndex = 0, args[2] = {0, 0};
static bool sh1106Mode = true;
static uint32_t dataBytes = 0;

static void feedCmd(uint8_t c) {
  if (sh1106Mode) {
    // ページごとにカーソルを置く方式。範囲指定は無い
    if ((c & 0xF0) == 0xB0)      curPage = (uint16_t)(c & 0x0F);
    else if ((c & 0xF0) == 0x00) curCol = (uint16_t)((curCol & 0xF0) | (c & 0x0F));
    else if ((c & 0xF0) == 0x10) curCol = (uint16_t)((curCol & 0x0F) | ((c & 0x0F) << 4));
    return;
  }
  if (pendingCmd != 0) {
    args[argIndex++] = c;
    if (argIndex == 2) {
      if (pendingCmd == 0x21) { colStart = args[0]; colEnd = args[1]; curCol = colStart; }
      else                    { curPage = args[0]; }
      pendingCmd = 0;
      argIndex = 0;
    }
    return;
  }
  if (c == 0x21 || c == 0x22) { pendingCmd = c; argIndex = 0; }
}

static void putByte(uint8_t b) {
  const uint16_t ramW = sh1106Mode ? (uint16_t)RAM_W : (uint16_t)W;
  if (curPage < (uint16_t)PAGES && curCol < ramW) {
    model[(uint32_t)curPage * RAM_W + curCol] = b;
  }
  if (sh1106Mode) {
    // カラムはページ内で進み、端で折り返す。ページはまたがない
    curCol = (uint16_t)((curCol + 1) % ramW);
  } else {
    if (curCol >= colEnd) { curCol = colStart; ++curPage; }
    else { ++curCol; }
  }
}

static uint8_t onWire(uint8_t addr, const uint8_t* d, size_t len, bool stop, void* user) {
  (void)stop; (void)user;
  if (addr != ADDR || len == 0) return 2;
  if (d[0] == 0x00)      { for (size_t i = 1; i < len; ++i) feedCmd(d[i]); }
  else if (d[0] == 0x40) { for (size_t i = 1; i < len; ++i) { putByte(d[i]); ++dataBytes; } }
  return 0;
}

static void scene(TinyGFX& g) {
  g.drawRect(0, 0, W, H, TFT_WHITE);
  g.fillRect(8, 8, 40, 16, TFT_WHITE);
  g.drawCircle(96, 32, 20, TFT_WHITE);
  g.fillTriangle(20, 60, 40, 34, 60, 60, TFT_WHITE);
  g.drawLine(0, 0, W - 1, H - 1, TFT_WHITE);
  g.setFont(&digitsFont);
  g.setTextColor(TFT_WHITE);
  g.drawString("12345", 70, 4);
}

/// 模型の RAM から、見えている 128 カラムぶんを取り出す。
static void extract(uint8_t* out, int colOffset) {
  for (int p = 0; p < PAGES; ++p) {
    for (int x = 0; x < W; ++x) out[p * W + x] = model[p * RAM_W + colOffset + x];
  }
}

static uint16_t image[W * H];
static void shot(const char* name, const uint8_t* bits) {
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      image[(uint32_t)y * W + x] = (bits[(y >> 3) * W + x] >> (y & 7)) & 1 ? 0xFFFF : 0x0000;
    }
  }
  tgfxShot(name, image, W, H);
}

static uint8_t fromSh[W * H / 8], fromSsd[W * H / 8];

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("sh1106");
  Wire.setWriteHook(onWire, nullptr);
  Wire.begin();

  // --- SH1106 -------------------------------------------------------------
  sh1106Mode = true;
  for (int i = 0; i < RAM_W * PAGES; ++i) model[i] = 0;
  shLcd.begin();
  dataBytes = 0;
  scene(shLcd);
  sh.display();
  tgfxReport("sh_bytes", (long)dataBytes);
  extract(fromSh, 2);  // 既定のカラムオフセット
  shot("sh1106", fromSh);

  // 絵が単色でないこと。真っ白どうし・真っ黒どうしで「一致」しても意味がない
  long lit = 0;
  for (int i = 0; i < W * H / 8; ++i) {
    for (int b = 0; b < 8; ++b) {
      if ((fromSh[i] >> b) & 1) ++lit;
    }
  }
  tgfxReport("sh_lit", lit);

  // ガラスの外（カラム 0,1 と 130,131）には何も書かれていないこと
  long outside = 0;
  for (int p = 0; p < PAGES; ++p) {
    if (model[p * RAM_W + 0]) ++outside;
    if (model[p * RAM_W + 1]) ++outside;
    if (model[p * RAM_W + 130]) ++outside;
    if (model[p * RAM_W + 131]) ++outside;
  }
  tgfxReport("outside_glass", outside);

  // --- 同じ絵を SSD1306 で ------------------------------------------------
  sh1106Mode = false;
  for (int i = 0; i < RAM_W * PAGES; ++i) model[i] = 0;
  ssdLcd.begin();
  dataBytes = 0;
  scene(ssdLcd);
  ssd.display();
  tgfxReport("ssd_bytes", (long)dataBytes);
  extract(fromSsd, 0);
  shot("ssd1306", fromSsd);

  long diff = 0;
  for (int i = 0; i < W * H / 8; ++i) {
    if (fromSh[i] != fromSsd[i]) ++diff;
  }
  tgfxReport("sh_vs_ssd_diff", diff);

  // --- カラムオフセットを変えると絵がずれること ---------------------------
  sh1106Mode = true;
  for (int i = 0; i < RAM_W * PAGES; ++i) model[i] = 0;
  sh.setColumnOffset(0);
  sh.clearBuffer();
  scene(shLcd);
  sh.display();
  static uint8_t shifted[W * H / 8];
  extract(shifted, 0);
  long same = 0;
  for (int i = 0; i < W * H / 8; ++i) {
    if (shifted[i] == fromSsd[i]) ++same;
  }
  tgfxReport("offset0_matches_ssd", same == W * H / 8 ? 1 : 0);
  sh.setColumnOffset(2);

  tgfxTestDone();
}
void loop() {}
