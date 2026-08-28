// CellFont の連鎖と U+FFFD 退避。
//
// 生成されたフォントでは踏めない道を、手で組んだ小さな CellFont で通す。
//
//   1. 退避は**連鎖を全部引き終えてから** 1 度だけ（CellFont 仕様 §7.2 / §15.2）
//      前段に豆腐があるだけで後段の字が潰れる、という事故を捕まえる
//   2. ベースラインは**連鎖の先頭のメトリクス**で決まる（仕様 §8）
//      高さの違うフォントを繋いでも字面が揃うこと
//   3. 疎索引の頭ブロックと、first より小さいコードのしっぽ（仕様 §7.1）
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/FontCell.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>

static const int W = 16, H = 12;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

// --- フォント 1: 'A' と U+FFFD を持つ疎索引（頭ブロック 1、しっぽ 1）------------
// 3x5 のセル。グリフ 0 = 'A'（全部塗る） / グリフ 1 = U+FFFD（上 1 行だけ）
static const uint8_t anBits[4] CELLFONT_PROGMEM = {0xFF, 0xFE, 0xE0, 0x00};
static const uint16_t anCodes[1] CELLFONT_PROGMEM = {0xFFFD};
static const CellFont fontAN CELLFONT_PROGMEM = {
    anBits, nullptr, anCodes, nullptr,
    0x41, 2,   // first='A', count=2
    3, 5,      // width, height
    4, 6,      // xAdvance, yAdvance
    0, -5,     // xOffset, yOffset（ascent=5）
    2,         // bytesPerGlyph
    1,         // headCount（'A' だけが頭。U+FFFD はしっぽ）
};

// --- フォント 2: 'B' だけ。**高さが違う**（3 画素、ascent 3）------------------
static const uint8_t bBits[2] CELLFONT_PROGMEM = {0xFF, 0x80};  // 3x3 全部塗る
static const CellFont fontB CELLFONT_PROGMEM = {
    bBits, nullptr, nullptr, nullptr,
    0x42, 1,   // first='B', count=1
    3, 3,      // width, height
    4, 6,      // xAdvance, yAdvance
    0, -3,     // xOffset, yOffset（ascent=3。fontAN と違う）
    2,         // bytesPerGlyph
    0,
};

// --- フォント 3: first より小さいコードをしっぽに持つ疎索引 ---------------------
// 頭ブロックを 'B'（0x42）に取り、しっぽに 'A'（0x41）を置く。
// 仕様 §7.1 が「c < first で打ち切ってよいのは連続索引のときだけ」と警告している道。
static const uint16_t loCodes[1] CELLFONT_PROGMEM = {0x0041};
static const CellFont fontLo CELLFONT_PROGMEM = {
    anBits, nullptr, loCodes, nullptr,
    0x42, 2,   // first='B'（頭）、count=2
    3, 5, 4, 6, 0, -5, 2,
    1,         // headCount=1
};

static const TinyGFXFontRef refB = {&fontB, &tinygfxFontCellOps, nullptr};
static const TinyGFXFontRef refAN = {&fontAN, &tinygfxFontCellOps, nullptr};
static const TinyGFXFontRef refChain = {&fontAN, &tinygfxFontCellOps, &refB};
static const TinyGFXFontRef refLo = {&fontLo, &tinygfxFontCellOps, nullptr};

static void clear() {
  bus.fill(0);
}

static long lit() {
  long n = 0;
  for (int i = 0; i < W * H; ++i)
    if (gram[i] != 0) ++n;
  return n;
}

/// 点いている行の範囲。無ければ -1 / -1。
static void litRows(long* top, long* bottom) {
  *top = -1;
  *bottom = -1;
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      if (gram[y * W + x] != 0) {
        if (*top < 0) *top = y;
        *bottom = y;
        break;
      }
}

static void scene(const char* name, const TinyGFXFontRef* font, uint16_t ch) {
  clear();
  lcd.setFont(font);
  const long ret = lcd.drawChar(ch, 0, 0);
  long top, bottom;
  litRows(&top, &bottom);
  tgfxReport2(name, "ret", ret);
  tgfxReport2(name, "lit", lit());
  tgfxReport2(name, "top", top);
  tgfxReport2(name, "bottom", bottom);
  Serial.print("SCENE ");
  Serial.println(name);
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("fontchain");
  lcd.begin();
  lcd.setTextColor(TFT_WHITE);

  scene("a", &refAN, 'A');            // 収録あり: 3x5 を全部塗る = 15
  scene("nd", &refAN, 'Z');           // 収録なし -> U+FFFD へ退避: 上 1 行 = 3
  scene("ndself", &refAN, 0xFFFD);    // 要求が U+FFFD 自身: 再検索しない
  scene("chain", &refChain, 'B');     // **前段の豆腐に潰されず、後段の 'B' が出ること**
  scene("miss", &refB, 'Z');          // どこにも無い: 何も描かず送り 0
  scene("lo", &refLo, 'A');           // first より小さいコードがしっぽに居る
  scene("lohead", &refLo, 'B');       // 同じフォントの頭ブロック

  // 行送りと ascent は**連鎖の先頭**のもの
  lcd.setFont(&refChain);
  tgfxReport("chain_line", (long)lcd.fontHeight());
  tgfxReport("chain_ascent", (long)lcd.getTextAscent());
  lcd.setFont(&refB);
  tgfxReport("b_ascent", (long)lcd.getTextAscent());

  tgfxTestDone();
}
void loop() {}
