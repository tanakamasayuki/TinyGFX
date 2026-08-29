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
#include <tgfx_digits.h>

static const TinyGFXFontRef digitsFont = {&tgfxDigits, &tinygfxFontCellOps};
#include <Wire.h>

static const int W = 128, H = 64;
static const uint8_t ADDR = 0x3C;

static uint8_t fb[W * H / 8];
static uint8_t fast[W * H / 8];
static uint8_t whole[W * H / 8];
static uint8_t bandBuf[W];  // one page
TinyGFXBusI2C bus(Wire, ADDR);
TinyGFXPanelSSD1306 panel(bus, fb, W, H);
TinyGFX lcd(panel);

// The same panel again, but told its buffer is a single page. Same bus, so the
// model on the other end sees both.
TinyGFXPanelSSD1306 bandPanel(bus, bandBuf, W, H, 1);
TinyGFX bandLcd(bandPanel);

// ---- 受け側の模型（TinyGFX 側が持つ。コアは SSD1306 を知らない） ----------
static uint8_t model[W * H / 8];
static uint16_t colStart = 0, colEnd = W - 1, pageStart = 0, pageEnd = 7;
static uint16_t curCol = 0, curPage = 0;
static uint8_t pendingCmd = 0, argIndex = 0, args[2] = {0, 0};
static uint32_t dataBytes = 0, txCount = 0;

// 初期化列のうち、パネルの高さで変わる 2 つを拾う（0xA8 多重比、0xDA COM ピン）
static uint8_t muxArg = 0xFF, comArg = 0xFF, prevCmd = 0;

static void feedCmd(uint8_t c) {
  if (prevCmd == 0xA8) muxArg = c;
  else if (prevCmd == 0xDA) comArg = c;
  prevCmd = c;
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

/// The scene both the whole-buffer and the banded runs draw.
static void monoScene(TinyGFX& g) {
  g.drawRect(0, 0, W, H, TFT_WHITE);
  g.fillRect(8, 8, 40, 16, TFT_WHITE);
  g.drawCircle(96, 32, 20, TFT_WHITE);
  g.fillTriangle(20, 60, 40, 34, 60, 60, TFT_WHITE);
  g.drawLine(0, 0, W - 1, H - 1, TFT_WHITE);
  g.setFont(&digitsFont);
  g.setTextColor(TFT_WHITE);
  g.drawString("12345", 70, 4);
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

  Wire.begin();  // the sketch owns the bus; TinyGFX never begins it
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
  lcd.setFont(&digitsFont);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("12345", 8, 24);
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

  // --- fillRect の速い経路が、1 画素ずつ描いた結果と一致すること ------------
  //
  // モノクロパネルは 8 画素を 1 バイトに詰めるので、窓が丸ごと 1 色なら
  // バイト単位で塗る。**速いだけで、絵は 1 画素も変わってはいけない。**
  // ページの境界をまたぐ / またがない、回転あり / なしを混ぜて確かめる。
  {
    static const int16_t R[6][4] = {
        {0, 0, W, H},            // 全面
        {8, 8, 32, 8},           // ページ 1 枚にちょうど収まる
        {3, 5, 17, 11},          // ページ境界をまたぐ、幅も高さも半端
        {0, 62, 5, 2},           // 最終ページの端
        {W - 3, 0, 3, 1},        // 右上の 1 行
        {60, 30, 1, 1},          // 1 画素
    };
    long diff = 0;
    for (uint8_t rot = 0; rot < 4; ++rot) {
      lcd.setRotation(rot);
      for (uint8_t i = 0; i < 6; ++i) {
        int16_t x = R[i][0], y = R[i][1], w = R[i][2], h = R[i][3];
        if (x + w > lcd.width() || y + h > lcd.height()) continue;
        panel.clearBuffer();
        lcd.fillRect(x, y, w, h, TFT_WHITE);
        for (int32_t k = 0; k < (int32_t)sizeof(fb); ++k) fast[k] = fb[k];
        panel.clearBuffer();
        lcd.startWrite();
        for (int16_t py = y; py < y + h; ++py) {
          for (int16_t px = x; px < x + w; ++px) lcd.drawPixel(px, py, TFT_WHITE);
        }
        lcd.endWrite();
        for (int32_t k = 0; k < (int32_t)sizeof(fb); ++k) {
          if (fast[k] != fb[k]) ++diff;
        }
      }
    }
    lcd.setRotation(0);
    panel.clearBuffer();
    tgfxReport("fillrect_fastpath_diff", diff);
  }

  // --- 帯で描いても、全面バッファと同じ絵になること ------------------------
  //
  // フレームバッファを持たない構成（1 ページぶん 128 バイトだけ）。
  // シーンをページごとに描き直してその都度流す。**RAM が 1/8 で済むが、
  // 毎回全ページ流すことになる。** 絵は 1 画素も変わってはいけない。
  {
    lcd.setRotation(0);
    panel.clearBuffer();
    for (int i = 0; i < W * H / 8; ++i) model[i] = 0;
    dataBytes = 0;
    monoScene(lcd);
    panel.display();
    tgfxReport("bytes_whole_buffer", (long)dataBytes);
    for (int i = 0; i < W * H / 8; ++i) whole[i] = model[i];

    for (int i = 0; i < W * H / 8; ++i) model[i] = 0;
    dataBytes = 0;
    for (int16_t page = 0; page < H / 8; ++page) {
      bandPanel.setBandPage(page);
      bandPanel.clearBuffer();
      bandLcd.setClipRect(0, (int16_t)(page * 8), W, 8);
      monoScene(bandLcd);
      bandPanel.display();
    }
    bandLcd.resetClipRect();
    tgfxReport("bytes_banded", (long)dataBytes);

    long diff = 0;
    for (int i = 0; i < W * H / 8; ++i) {
      if (whole[i] != model[i]) ++diff;
    }
    tgfxReport("banded_diff", diff);
  }

  // --- パネルの高さで初期化列が変わること ---------------------------------
  //
  // **2026-08-29 の設計レビューの P1。** 初期化列は 128x64 決め打ちだったのに
  // コンストラクタは任意の w / h を受けていた。128x32 に 64 行ぶんの多重比を
  // 送ると、絵がガラスの半分に潰れる。
  {
    static uint8_t fb32[128 * 32 / 8];
    TinyGFXPanelSSD1306 p32(bus, fb32, 128, 32);
    TinyGFX g32(p32);
    muxArg = comArg = 0xFF;
    g32.begin();
    tgfxReport("mux32", (long)muxArg);
    tgfxReport("com32", (long)comArg);

    muxArg = comArg = 0xFF;
    TinyGFXPanelSSD1306 p64(bus, fb, 128, 64);
    TinyGFX g64(p64);
    g64.begin();
    tgfxReport("mux64", (long)muxArg);
    tgfxReport("com64", (long)comArg);
  }

  // --- 使えない設定は begin() が false を返すこと --------------------------
  //
  // どれもコンパイル時には分からない（バッファは利用者が持つポインタ、
  // 寸法はコンストラクタ引数）。**黙って壊れた絵を出すより落とす。**
  {
    TinyGFXPanelSSD1306 pnull(bus, nullptr, 128, 64);
    TinyGFX g(pnull);
    tgfxReport("begin_null_buffer", g.begin() ? 1 : 0);
  }
  {
    static uint8_t fb60[128 * 8];
    TinyGFXPanelSSD1306 podd(bus, fb60, 128, 60);  // 8 の倍数でない
    TinyGFX g(podd);
    tgfxReport("begin_odd_height", g.begin() ? 1 : 0);
  }
  {
    static uint8_t fbz[128];
    TinyGFXPanelSSD1306 pzero(bus, fbz, 0, 64);
    TinyGFX g(pzero);
    tgfxReport("begin_zero_width", g.begin() ? 1 : 0);
  }
  {
    // まっとうな設定は通ること（上の 3 つが「常に false」で通らないように）
    static uint8_t fbok[128 * 64 / 8];
    TinyGFXPanelSSD1306 pok(bus, fbok, 128, 64);
    TinyGFX g(pok);
    tgfxReport("begin_ok", g.begin() ? 1 : 0);
  }

  tgfxTestDone();
}
void loop() {}
