// **実機の自動テスト。** M5Stack Core / BASIC を標準の検証機として使う。
//
// **実機で作った絵を、ホストで作ったゴールデンと突き合わせる。**
//
//   ホスト  tests/scene/      共通シーンを BusCapture で描く -> golden/scene.ppm
//   実機    ここ              同じシーンを**実機の上で**描いて artifact で送る
//   pytest  test_hw_m5stack   両者を突き合わせる
//
// 絵の取り方は 2 通り。**M5Stack で使えるのは 1 だけ**（2 は 2026-08-28 実測で不可）。
//
//   1. TinyGFXBusCapture で取る（常に使える）
//      実機のコンパイラ・実機の int 幅・実機の PROGMEM を通った結果が見える。
//      **パネルドライバのコマンド列まで**は本物。線から先は見ない
//   2. パネルの GRAM から読み戻す（SDO が来ているパネルだけ）
//      線から先も見られる。**M5Stack Core は SDO が GPIO19 に来ていないので不可**
//
// シーンの定義は tgfx_scene.h の 1 箇所だけ。**片方だけ変わることがない。**
//
// シリアルの頭は取りこぼすので、ArduTest の HELLO で同期してから流す。
#include <ArduTest.h>
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/FontCell.h>
#include <TinyGFX/PanelILI9342.h>
#include <tgfx_scene.h>
#include <tgfx_digits.h>

static const int8_t PIN_DC = 27, PIN_CS = 14, PIN_RST = 33, PIN_BL = 32, PIN_SD_CS = 4;

TinyGFXBusSPI bus(SPI, PIN_DC, PIN_CS, /*freq*/ 24000000UL);
TinyGFXPanelILI9342 panel(bus, 320, 240, PIN_RST);
TinyGFX lcd(panel);

static const TinyGFXFontRef digitsFont = {&tgfxDigits, &tinygfxFontCellOps};
// **読み戻すのは上 2 行だけ。** 読み出しは 1 回ごとに線を張り替えるので、
// 何十回も回すとボードが固まる（実測）。デバッグ用途なので小さく取る。
// Read-back costs about 150us a pixel, so the golden comparison takes the
// top strip rather than the whole scene. 64x8 is ~75ms and still covers the
// border, the three colour blocks and the circle.
static const int16_t TGFX_READBACK_H = 8;
static uint16_t readBuf[TGFX_SCENE_W * TGFX_SCENE_H];
static uint16_t capBuf[TGFX_SCENE_W * TGFX_SCENE_H];

static void hex2(char* out, uint8_t v) {
  static const char* D = "0123456789ABCDEF";
  out[0] = D[v >> 4];
  out[1] = D[v & 15];
}

/// 読み戻しの線が繋がっているか。**ここが落ちたら以下は全部意味がない。**
/// Can this panel be read back at all? Reports, never judges - a panel that
/// cannot read is not a failure, so pytest turns this into a skip.
///
/// Judged on RAMRD rather than an ID register: this panel answers FF to
/// 0x04 and friends while reading its GRAM perfectly well, and the GRAM is
/// what we actually want.
TEST_CASE(test_panel_readable) {
  static const uint16_t probe[3] = {TFT_RED, TFT_GREEN, TFT_BLUE};
  lcd.startWrite();
  for (int16_t i = 0; i < 3; ++i) lcd.fillRect((int16_t)(200 + i), 200, 1, 1, probe[i]);
  lcd.endWrite();

  char text[64];
  int n = 0;
  bool ok = true;
  for (int16_t i = 0; i < 3; ++i) {
    uint16_t got = 0;
    panel.readPixels((uint16_t)(200 + i), 200, 1, 1, &got);
    hex2(&text[n], (uint8_t)(got >> 8)); n += 2;
    hex2(&text[n], (uint8_t)got); n += 2;
    text[n++] = ' ';
    if (got != probe[i]) ok = false;
  }
  text[n] = 0;
  ArduTest.attachText("readback_probe.txt", text);
  ArduTest.reportMetric("readable", ok ? 1 : 0);
}

/// Do written colours come back unchanged? Skipped by pytest when the panel
/// cannot read.
TEST_CASE(test_readback_solid) {
  static const uint16_t want[4] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
  lcd.startWrite();
  for (int16_t i = 0; i < 4; ++i) lcd.fillRect((int16_t)(i * 4), 0, 4, 4, want[i]);
  lcd.endWrite();
  for (int16_t i = 0; i < 4; ++i) {
    uint16_t got = 0;
    panel.readPixels((uint16_t)(i * 4 + 1), 1, 1, 1, &got);
    ArduTest.reportMetric("readback", (long)got);
    ASSERT_EQ((long)want[i], (long)got);
  }
}

/// **回転が MADCTL に正しく降りているか。** 読み戻しでしか見られない。
///
/// 素直に「回転 N のまま描いて回転 N のまま読む」では何も分からない。RAMRD は
/// 書き込みと同じアドレスカウンタを通るので、MADCTL が間違っていても自己
/// 無矛盾になって読めてしまう。
/// Reading in the same rotation you drew in proves nothing: RAMRD goes through
/// the same address counter as the write, so a wrong MADCTL reads back
/// consistently wrong.
///
/// So: draw in rotation N, then go back to rotation 0 and read. Changing
/// MADCTL remaps the access order, it does not move what is already in GRAM,
/// so rotation 0 gives a fixed frame to look at what physically landed where.
///
/// Derived from the MADCTL table in PanelILI9342.h (MV transposes, MX flips
/// the column, MY flips the row), for a 320x240 landscape GRAM:
///
///   rotation 0  MADCTL 0        (lx, ly)
///   rotation 1  MADCTL MV|MX    (W-1-ly, lx)
///   rotation 2  MADCTL MX|MY    (W-1-lx, H-1-ly)
///   rotation 3  MADCTL MV|MY    (ly, H-1-lx)
///
/// (5, 2) puts the four markers in four different corners, so a marker cannot
/// be mistaken for another rotation's, and each gets its own colour so a stale
/// pixel cannot fake a pass.
///
/// **What this locks is rotations 1-3 against rotation 0.** Rotation 0 being
/// the right way up is not something read-back can see - that was checked by
/// eye once (docs/MANUAL_TEST.ja.md M0).
TEST_CASE(test_rotation_maps) {
  static const uint16_t MARK[4] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
  static const int16_t LX = 5, LY = 2;
  static const int16_t W = 320, H = 240;

  lcd.setRotation(0);
  lcd.fillScreen(TFT_BLACK);
  for (uint8_t r = 0; r < 4; ++r) {
    lcd.setRotation(r);
    lcd.fillRect(LX, LY, 1, 1, MARK[r]);
  }
  lcd.setRotation(0);  // back to the fixed frame

  const uint16_t px[4] = {(uint16_t)LX, (uint16_t)(W - 1 - LY),
                          (uint16_t)(W - 1 - LX), (uint16_t)LY};
  const uint16_t py[4] = {(uint16_t)LY, (uint16_t)LX,
                          (uint16_t)(H - 1 - LY), (uint16_t)(H - 1 - LX)};

  char text[96];
  int n = 0;
  for (uint8_t r = 0; r < 4; ++r) {
    uint16_t got = 0;
    panel.readPixels(px[r], py[r], 1, 1, &got);
    ArduTest.reportMetric("rot_found", (long)got);
    hex2(&text[n], (uint8_t)(got >> 8)); n += 2;
    hex2(&text[n], (uint8_t)got); n += 2;
    text[n++] = ' ';
  }
  text[n] = 0;
  ArduTest.attachText("rotation_marks.txt", text);
}

/// **本命。** 実機の上で共通シーンを描き、そのまま送る。
///
/// 実機のコンパイラ・実機の int 幅・実機の PROGMEM を通った結果が見える。
/// パネルドライバ（ILI9342）のコマンド列も本物で、それを BusCapture が
/// 仮想 GRAM に組み直す。**線から先だけが範囲外。**
TEST_CASE(test_capture_scene) {
  TinyGFXBusCapture cap(capBuf, TGFX_SCENE_W, TGFX_SCENE_H);
  TinyGFXPanelILI9342 capPanel(cap, TGFX_SCENE_W, TGFX_SCENE_H);
  TinyGFX g(capPanel);
  g.begin();
  cap.fill(0);
  g.setFont(&digitsFont);
  g.setTextColor(TFT_WHITE);
  tgfxGoldenScene(g);

  ArduTest.reportMetric("capture_pixels", (long)cap.pixelCount());
  // RGB565 のリトルエンディアン（このボードのメモリの並びそのまま）
  ArduTest.attachBinary("scene.rgb565", "application/octet-stream",
                        (const uint8_t*)capBuf, sizeof(capBuf));

  // ついでに実物にも出しておく（目で見たいときのため）
  lcd.setFont(&digitsFont);
  lcd.setTextColor(TFT_WHITE);
  tgfxGoldenScene(lcd);
}

/// The same scene, this time read back out of the panel's own GRAM.
/// **This is the only test that sees past the wire.**
TEST_CASE(test_readback_scene) {
  lcd.setFont(&digitsFont);
  lcd.setTextColor(TFT_WHITE);
  tgfxGoldenScene(lcd);
  // The top strip only - see TGFX_READBACK_H.
  for (int i = 0; i < TGFX_SCENE_W * TGFX_READBACK_H; ++i) readBuf[i] = 0;
  const uint32_t n = panel.readPixels(0, 0, TGFX_SCENE_W, TGFX_READBACK_H, readBuf);
  ArduTest.reportMetric("scene_pixels", (long)n);
  ArduTest.attachBinary("readback.rgb565", "application/octet-stream",
                        (const uint8_t*)readBuf,
                        (size_t)TGFX_SCENE_W * TGFX_READBACK_H * 2);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);

  SPI.begin();  // the sketch owns the bus; TinyGFX never begins it
  lcd.begin();
  // **M5Stack はデータ線が 1 本**（SDA=GPIO23）。SPI の MISO(19) には何も来ていない
  bus.setReadPins(/*sck*/ 18, /*sda*/ 23);
  lcd.fillScreen(TFT_BLACK);

  ArduTest.begin();
}

void loop() {
  ArduTest.poll();
}
