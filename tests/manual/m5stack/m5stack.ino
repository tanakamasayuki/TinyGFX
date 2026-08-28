// TinyGFX - 実機検証（M5Stack Core / BASIC）
//
// **自動テストで守れないものだけ**を 1 本に詰めたスケッチ。配線は要らない。
// 手順とチェック項目は docs/MANUAL_TEST.ja.md の M0b。
//
// ページは 4 秒で自動送り。Btn A（左）で次、Btn C（右）で前に戻る。
// 何を見ればいいかはシリアル（115200）にも出る。
//
//   0-3 回転 0..3      MADCTL の表が実機で正しいか（回転 0 だけ確認済み）
//   4   文字           CellFont 移行後の描画。倍角・背景セル・収録外
//   5   連鎖           高さの違う 2 フォントがベースラインで揃うか
//   6   クリップ       外に 1 画素も漏れないか
//   7   直接描画       同じ動きを消してから描く。**ちらつくはず**
//   8   帯レンダリング 同じ動きを TileCanvas で。**ちらつかないはず**
//   9   速度           シリアルに ms を出す
//  10   読み出し       **パネルから読み戻せるか。** できれば実機の自動検証に道が開ける
#include <TinyGFX.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/FontCell.h>
#include <TinyGFX/PanelILI9342.h>
#include <TinyGFX/TileCanvas.h>

#include "tinygfx_font5x7.h"

// 内蔵 LCD
static const int8_t PIN_DC = 27, PIN_CS = 14, PIN_RST = 33, PIN_BL = 32, PIN_SD_CS = 4;
// ボタン A / B / C（active LOW、外部プルアップあり）
static const int8_t BTN_A = 39, BTN_C = 37;

TinyGFXBusSPI bus(PIN_DC, PIN_CS, /*freq*/ 24000000UL);
TinyGFXPanelILI9342 panel(bus, 320, 240, PIN_RST);
TinyGFX lcd(panel);

static const TinyGFXFontRef font5x7 = {&tinygfxFont5x7, &tinygfxFontCellOps, nullptr};

// --- 連鎖の相手。**高さが違う**（3 画素、ascent 3）。'A'..'C' が塗り潰しの帯 ---
// 正しければ数字の**下端に揃う**。デコーダが自分の ascent で換算すると上端に出る。
static const uint8_t barBits[6] CELLFONT_PROGMEM = {0xFF, 0xFE, 0xFF, 0xFE, 0xFF, 0xFE};
static const CellFont fontBar CELLFONT_PROGMEM = {
    barBits, nullptr, nullptr, nullptr,
    0x41, 3,   // 'A'..'C'
    5, 3,      // width, height
    6, 8,      // xAdvance, yAdvance（行送りは連鎖先頭と揃える）
    0, -3,     // xOffset, yOffset（ascent=3）
    2, 0,
};
static const TinyGFXFontRef refBar = {&fontBar, &tinygfxFontCellOps, nullptr};
static const TinyGFXFontRef refChain = {&tinygfxFont5x7, &tinygfxFontCellOps, &refBar};

// 帯レンダリング用。ESP32 なので広めに取る（320 x 20 x 2 = 12,800 B）
static const int16_t BAND_ROWS = 20;
static uint16_t band[320 * BAND_ROWS];
TinyGFXTileCanvas canvas(panel, band, sizeof(band) / sizeof(band[0]));

static const uint8_t PAGES = 11;
static uint8_t page = 0;
static uint32_t shownAt = 0;

// ---------------------------------------------------------------------------

static void header(uint8_t n) {
  lcd.fillScreen(TFT_BLACK);
  lcd.drawRect(0, 0, lcd.width(), lcd.height(), TFT_WHITE);
  lcd.setFont(&font5x7);
  lcd.setTextColor(TFT_YELLOW);
  lcd.setTextSize(2);
  char s[4];
  snprintf(s, sizeof(s), "%d", (int)n);
  lcd.drawString(s, 6, 6);
  lcd.setTextSize(1);
}

/// 回転 0..3。原点に赤、右下に青、数字は必ず読める向きで出ること。
static void pageRotation(uint8_t r) {
  lcd.setRotation(r);
  header(r);
  const int16_t w = lcd.width(), h = lcd.height();
  lcd.fillRect(2, 2, 24, 24, TFT_RED);                 // 論理原点 (0,0)
  lcd.fillRect((int16_t)(w - 26), (int16_t)(h - 26), 24, 24, TFT_BLUE);
  lcd.drawLine(0, 0, (int16_t)(w - 1), (int16_t)(h - 1), TFT_DARKGREY);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(3);
  lcd.drawString("0123456789", 30, (int16_t)(h / 2 - 12));
  lcd.setTextSize(1);
  // 幅と高さを数字で（回転 1/3 で入れ替わること）
  char s[16];
  snprintf(s, sizeof(s), "%d:%d", (int)w, (int)h);
  lcd.setTextColor(TFT_CYAN);
  lcd.setTextSize(2);
  lcd.drawString(s, 30, 8);
  lcd.setTextSize(1);
}

/// 文字。CellFont に移してから 1 度も実機に出していない。
static void pageText() {
  lcd.setRotation(0);
  header(4);
  lcd.setFont(&font5x7);
  int16_t y = 40;
  for (uint8_t sz = 1; sz <= 4; ++sz) {          // 倍角
    lcd.setTextSize(sz);
    lcd.setTextColor(TFT_WHITE);
    lcd.drawString("0123456789", 10, y);
    y = (int16_t)(y + 8 * sz + 6);
  }
  lcd.setTextSize(2);
  lcd.setTextColor(TFT_BLACK, TFT_GREEN);        // 背景セル。字の下に隙間なく敷かれること
  lcd.drawString("0123", 10, y);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString("45", 130, y);                  // 背景なし（透過）に戻らないこと
  lcd.setTextColor(TFT_RED, TFT_BLACK);
  lcd.drawString("<=>?", 190, y);
  lcd.setTextSize(1);
  // 収録外（英字はこのフォントに無い）。**豆腐も無いので何も出ず、詰まって見える**
  lcd.setTextColor(TFT_DARKGREY);
  lcd.setTextSize(2);
  lcd.drawString("01ABC23", 10, (int16_t)(y + 30));
  lcd.setTextSize(1);
}

/// 連鎖。高さの違う 2 フォントがベースラインで揃うか。
static void pageChain() {
  lcd.setRotation(0);
  header(5);
  lcd.setFont(&refChain);
  lcd.setTextColor(TFT_WHITE);
  for (uint8_t sz = 1; sz <= 4; ++sz) {
    lcd.setTextSize(sz);
    // 'A'..'C' は 3 画素の帯。数字の**下端**に揃っていれば正しい
    lcd.drawString("012ABC345", 10, (int16_t)(40 + (sz - 1) * 46));
  }
  lcd.setTextSize(1);
  // 目安線: 1 倍のときのベースライン
  lcd.drawFastHLine(4, 47, 312, TFT_DARKGREY);
  lcd.setFont(&font5x7);
}

/// クリップ。外に 1 画素も漏れないこと。
static void pageClip() {
  lcd.setRotation(0);
  header(6);
  lcd.drawRect(59, 59, 202, 122, TFT_DARKGREY);   // クリップ枠の外側 1 画素
  lcd.setClipRect(60, 60, 200, 120);
  lcd.fillCircle(160, 120, 200, TFT_NAVY);        // 画面より大きい円
  for (int16_t i = -200; i < 400; i += 24) {
    lcd.drawLine(i, 0, (int16_t)(i + 120), 239, TFT_CYAN);
  }
  lcd.setTextSize(3);
  lcd.setTextColor(TFT_YELLOW);
  lcd.drawString("0123456789", 20, 100);          // 左右にはみ出す文字列
  lcd.setTextSize(1);
  lcd.resetClipRect();
}

/// 動く絵。**消す処理は入れない**（帯側は autoClear、直接側は fillScreen が消す）。
/// 帯ごとに呼ばれるので、ここに重い処理を書かない。
static void ballScene(TinyGFX& g, void* ctx) {
  const int16_t t = *(const int16_t*)ctx;
  g.drawRect(0, 0, 320, 240, TFT_DARKGREY);
  g.fillCircle(t, 120, 40, TFT_YELLOW);
  g.fillRect((int16_t)(320 - t - 30), 40, 60, 160, TFT_MAGENTA);
  g.fillTriangle(20, 220, 60, 150, 100, 220, TFT_CYAN);
}

/// 直接描画。消してから描くのでちらつく。**これが比較対象。**
static void pageDirect() {
  lcd.setRotation(0);
  const uint32_t until = millis() + 6000;
  int16_t t = 60;
  int8_t dir = 6;
  while (millis() < until) {
    lcd.startWrite();
    lcd.fillScreen(TFT_NAVY);
    ballScene(lcd, &t);
    lcd.endWrite();
    t = (int16_t)(t + dir);
    if (t > 260 || t < 60) dir = (int8_t)-dir;
  }
}

/// 帯レンダリング。**同じ絵・同じ動きで、こちらはちらつかない。**
static void pageTiled() {
  canvas.setRotation(0);
  canvas.setBackgroundColor(TFT_NAVY);
  const uint32_t until = millis() + 6000;
  int16_t t = 60;
  int8_t dir = 6;
  while (millis() < until) {
    canvas.render(&ballScene, &t);
    t = (int16_t)(t + dir);
    if (t > 260 || t < 60) dir = (int8_t)-dir;
  }
  lcd.setRotation(0);
}

/// 速度。**まだ 1 度も測っていない。** シリアルに出す。
static void pageBench() {
  lcd.setRotation(0);
  header(9);
  struct { const char* name; uint32_t ms; } r[6];
  uint32_t t0;

  t0 = millis();
  for (int i = 0; i < 5; ++i) lcd.fillScreen(i & 1 ? TFT_BLACK : TFT_NAVY);
  r[0] = {"fillScreen x5", millis() - t0};

  t0 = millis();
  for (int i = 0; i < 200; ++i) lcd.fillRect(10, 10, 100, 100, (uint16_t)(i * 37));
  r[1] = {"fillRect 100x100 x200", millis() - t0};

  t0 = millis();
  for (int i = 0; i < 200; ++i) lcd.drawLine(0, 0, 319, 239, (uint16_t)(i * 37));
  r[2] = {"drawLine diag x200", millis() - t0};

  t0 = millis();
  for (int i = 0; i < 200; ++i) lcd.fillCircle(160, 120, 100, (uint16_t)(i * 37));
  r[3] = {"fillCircle r100 x200", millis() - t0};

  lcd.setFont(&font5x7);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  t0 = millis();
  for (int i = 0; i < 200; ++i) lcd.drawString("0123456789", 10, 60);
  r[4] = {"drawString 10 chars x200 (size2)", millis() - t0};

  canvas.setRotation(0);
  canvas.setBackgroundColor(TFT_NAVY);
  int16_t bt = 100;
  t0 = millis();
  for (int i = 0; i < 20; ++i) canvas.render(&ballScene, &bt);
  r[5] = {"TileCanvas full frame x20", millis() - t0};
  lcd.setRotation(0);

  lcd.setTextSize(1);
  header(9);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  Serial.println(F("--- speed (M5Stack Core, 24MHz SPI, TINYGFX_FILL_CHUNK off) ---"));
  int16_t y = 40;
  for (uint8_t i = 0; i < 6; ++i) {
    char s[12];
    snprintf(s, sizeof(s), "%lu", (unsigned long)r[i].ms);
    lcd.drawString(s, 12, y);
    y = (int16_t)(y + 30);
    Serial.print("  ");
    Serial.print(r[i].name);
    Serial.print(": ");
    Serial.print(r[i].ms);
    Serial.println(" ms");
  }
  lcd.setTextSize(1);
}


/// パネルから読み戻せるか。**できるかどうかを決める実験。**
///
/// できれば「描いて読み戻して比べる」で実機を自動検証できる。ホストのテストと
/// 同じ厳しさが実機に持ち込める。まずは線が繋がっているかを見る。
///
/// ILI934x の作法は 3 つ。どれも外すと化けるので、生のバイトをそのまま出す。
///   1. 読み出しは書き込みより低いクロック（ここでは 8MHz）
///   2. RAMRD(0x2E) の先頭にダミーが 1 バイト入る
///   3. 16bpp で書いても**読み出しは 1 画素 3 バイト**（RGB666 が上詰め）
static void dump(const char* label, uint8_t cmd, uint8_t n) {
  uint8_t buf[16];
  for (uint8_t i = 0; i < n && i < 16; ++i) buf[i] = 0;
  panel.readRegister(cmd, buf, n);
  Serial.print("  ");
  Serial.print(label);
  Serial.print(" (0x");
  if (cmd < 16) Serial.print('0');
  Serial.print(cmd, HEX);
  Serial.print("):");
  for (uint8_t i = 0; i < n && i < 16; ++i) {
    Serial.print(' ');
    if (buf[i] < 16) Serial.print('0');
    Serial.print(buf[i], HEX);
  }
  Serial.println();
}

static void pageReadback() {
  lcd.setRotation(0);
  header(10);
  // **M5Stack はデータ線が 1 本**（SDA=GPIO23）。SPI の MISO(19) には何も来ていない
  bus.setReadPins(/*sck*/ 18, /*sda*/ 23);

  // 読み戻す先に、見分けのつく色を置く
  lcd.fillRect(0, 100, 2, 1, TFT_RED);      // (0,100) 赤
  lcd.drawPixel(1, 100, TFT_GREEN);         // (1,100) 緑

  Serial.println(F("--- read-back probe ---"));
  dump("RDDID ", 0x04, 5);   // ILI9341 なら 00 00 93 41 ... 全部 00 なら MISO が来ていない
  dump("RDID4 ", 0xD3, 5);
  dump("RDDST ", 0x09, 6);

  // (0,100)-(1,100) の 2 画素を読む。ダミー 1 + 3 バイト x 2 = 7 バイト
  uint8_t a[4];
  bus.beginTransaction();
  a[0] = 0; a[1] = 0; a[2] = 0; a[3] = 1;
  bus.writeCommand(0x2A);
  bus.writeData(a, 4);                       // CASET 0..1
  a[0] = 0; a[1] = 100; a[2] = 0; a[3] = 100;
  bus.writeCommand(0x2B);
  bus.writeData(a, 4);                       // RASET 100..100
  bus.endTransaction();

  Serial.println(F("  wrote: (0,100)=F800 red  (1,100)=07E0 green"));
  dump("RAMRD ", 0x2E, 8);   // 期待: dummy, F8 00 00, 00 FC 00 （上位 6bit が有効）

  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.drawString("0123456789", 12, 140);     // 画面側は「見に行った」目印だけ
  lcd.setTextSize(1);
  Serial.println(F("  NOTE: this panel returns FF for ID registers but the GRAM reads fine"));
}

static const char* PAGE_HINT[PAGES] = {
    "rot 0 : red square top-left, digits upright, 320:240",
    "rot 1 : same picture turned 90 deg, 240:320",
    "rot 2 : turned 180 deg, 320:240",
    "rot 3 : turned 270 deg, 240:320",
    "text  : sizes 1-4, green background cell flush under glyphs, A-C blank",
    "chain : A-C bars must sit on the BOTTOM of the digits (shared baseline)",
    "clip  : nothing outside the grey frame",
    "direct: same motion, cleared each frame -- SHOULD flicker",
    "tiled : same motion via TileCanvas -- should NOT flicker",
    "bench : timings on serial",
    "read  : can the panel be read back? raw bytes on serial",
};

static void show(uint8_t n) {
  switch (n) {
    case 0: case 1: case 2: case 3: pageRotation(n); break;
    case 4: pageText(); break;
    case 5: pageChain(); break;
    case 6: pageClip(); break;
    case 7: pageDirect(); break;
    case 8: pageTiled(); break;
    case 9: pageBench(); break;
    default: pageReadback(); break;
  }
  Serial.print(n);
  Serial.print(F(" | "));
  Serial.println(PAGE_HINT[n]);
  shownAt = millis();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);
  pinMode(BTN_A, INPUT);
  pinMode(BTN_C, INPUT);

  lcd.begin();
  canvas.begin();  // 帯レンダリング側。パネルの init が 2 回走るが害はない

  // 古い世代の BASIC で色が反転するときはここを外す（M0 と同じ）。
  // **canvas.begin() より後**でないと、パネルの再初期化で INVON に戻される。
  // panel.invertDisplay(false);

  Serial.println();
  Serial.println(F("TinyGFX manual check (M5Stack). BtnA=next BtnC=prev, auto every 4s"));
  show(0);
}

void loop() {
  bool next = false, prev = false;
  if (digitalRead(BTN_A) == LOW) { next = true; while (digitalRead(BTN_A) == LOW) delay(10); }
  if (digitalRead(BTN_C) == LOW) { prev = true; while (digitalRead(BTN_C) == LOW) delay(10); }
  if (!next && !prev && millis() - shownAt < 4000) { delay(20); return; }
  if (prev) page = (uint8_t)((page + PAGES - 1) % PAGES);
  else page = (uint8_t)((page + 1) % PAGES);
  show(page);
}
