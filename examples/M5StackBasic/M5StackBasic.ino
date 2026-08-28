// TinyGFX - M5StackBasic
//
// **実機の立ち上げ用スケッチ。** M5Stack Core / BASIC の内蔵 LCD（ILI9342C
// 320x240）に、確かめたいことを 1 画面に全部出す。
//
// 配線が要らない・ピンが決まっている・電源が安定している、という理由で
// 最初の実機確認にはこれが一番確実（docs/MANUAL_TEST.ja.md M0）。
//
// 出るはずのもの:
//   1. 画面の**外周 1 ドット**に白い枠。四辺とも切れていないこと
//   2. 上に **赤・緑・青**の帯（左から順に）
//   3. まん中に **0123456789**。鏡像でも上下逆でもないこと
//   4. 下に円・三角・線
//
// 違っていたときの直し方（どれも 1 行）:
//   - 真っ暗            -> バックライト（GPIO32）が HIGH になっているか
//   - 白地に黒          -> panel.invertDisplay(false);
//   - 帯が青・緑・赤    -> panel.setRgbOrder(false);
//   - 数字が鏡像/逆さま -> panel.setMirror(true, false) など。lcd.begin() より前に
//   - 端が欠ける/ずれる -> 320x240 になっているか（コンストラクタの引数）
#include <TinyGFX.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/PanelILI9342.h>

#include "tinygfx_font5x7.h"

// M5Stack Core / BASIC の内蔵 LCD
static const int8_t PIN_DC = 27;
static const int8_t PIN_CS = 14;
static const int8_t PIN_RST = 33;
static const int8_t PIN_BL = 32;   // バックライト。HIGH にしないと何も見えない
static const int8_t PIN_SD_CS = 4; // SD と SPI を共有している。HIGH で切っておく

// SCK / MOSI は指定しない。M5Stack の既定（18 / 23）を Arduino Core の SPI が使う。
// 40MHz でも動くが、最初は落としておく。
TinyGFXBusSPI bus(PIN_DC, PIN_CS, /*freq*/ 24000000UL);
TinyGFXPanelILI9342 panel(bus, 320, 240, PIN_RST);
TinyGFX lcd(panel);

void setup()
{
  Serial.begin(115200);

  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);

  lcd.begin();

  const int16_t w = lcd.width();
  const int16_t h = lcd.height();

  lcd.startWrite();
  lcd.fillScreen(TFT_BLACK);

  // 1. 端まで届いているか
  lcd.drawRect(0, 0, w, h, TFT_WHITE);

  // 2. 色順（左から 赤・緑・青）
  const int16_t bar = (int16_t)((w - 8) / 3);
  lcd.fillRect(4, 12, bar, 40, TFT_RED);
  lcd.fillRect((int16_t)(4 + bar), 12, bar, 40, TFT_GREEN);
  lcd.fillRect((int16_t)(4 + bar * 2), 12, bar, 40, TFT_BLUE);

  // 3. 向き。鏡像・上下逆なら数字を見ればすぐ分かる
  lcd.setFont(&tinygfxFont5x7);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(3);
  const char *digits = "0123456789";
  lcd.drawString(digits, (int16_t)((w - lcd.textWidth(digits)) / 2), 66);

  // 4. 基本図形
  lcd.fillCircle(64, 172, 32, TFT_YELLOW);
  lcd.drawCircle(64, 172, 40, TFT_WHITE);
  lcd.fillTriangle(160, 132, 128, 212, 192, 212, TFT_CYAN);
  for (int16_t i = 0; i <= 60; i += 10)
  {
    lcd.drawLine(232, (int16_t)(132 + i), 292, (int16_t)(212 - i), TFT_MAGENTA);
  }
  lcd.endWrite();

  Serial.println();
  Serial.println(F("TinyGFX M5Stack bring-up"));
  Serial.print(F("  size   : "));
  Serial.print(w);
  Serial.print('x');
  Serial.println(h);
  Serial.println(F("  expect : white border / R-G-B bars / 0123456789 / circle+triangle+lines"));
  Serial.println(F("  dark      -> backlight GPIO32"));
  Serial.println(F("  inverted  -> panel.invertDisplay(false)"));
  Serial.println(F("  B-G-R     -> panel.setRgbOrder(false)"));
  Serial.println(F("  mirrored  -> panel.setMirror(x, y) before lcd.begin()"));
}

void loop() {}
