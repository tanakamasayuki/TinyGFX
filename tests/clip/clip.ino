// クリップの不変条件:
//   クリップ内は「クリップ無しで描いた結果」と 1 画素も違わないこと
//   クリップ外は 1 画素も触られないこと
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <tgfx_test.h>

static const int W = 64, H = 64;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static const uint16_t BG = 0x0000;

// クリップ矩形（テスト側と合わせること）
static const int CX = 10, CY = 12, CW = 30, CH = 26;

static void scene() {
  lcd.fillCircle(32, 32, 26, 0x001F);
  lcd.drawLine(0, 0, 63, 63, 0xF800);
  lcd.fillRect(0, 0, 64, 4, 0x07E0);
  lcd.drawRect(2, 2, 60, 60, 0xFFE0);
  lcd.fillTriangle(0, 63, 20, 20, 63, 50, 0xF81F);
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("clip");
  lcd.begin();

  bus.fill(BG);
  bus.resetCounters();
  scene();
  tgfxReport("free_pixels", (long)bus.pixelCount());
  tgfxShot("free", gram, W, H);

  bus.fill(BG);
  bus.resetCounters();
  lcd.setClipRect(CX, CY, CW, CH);
  scene();
  lcd.clearClipRect();
  tgfxReport("clipped_pixels", (long)bus.pixelCount());
  tgfxShot("clipped", gram, W, H);

  // クリップは画面の外へ広がらない
  bus.fill(BG);
  bus.resetCounters();
  lcd.setClipRect(-20, -20, 200, 200);
  lcd.fillScreen(0x07FF);
  lcd.clearClipRect();
  tgfxReport("oversize_clip_pixels", (long)bus.pixelCount());

  // 幅 0 のクリップでは何も描かれない
  bus.fill(BG);
  bus.resetCounters();
  lcd.setClipRect(10, 10, 0, 0);
  scene();
  lcd.clearClipRect();
  tgfxReport("empty_clip_pixels", (long)bus.pixelCount());

  // --- 極端な座標でもクリップして描くこと ---------------------------------
  //
  // **2026-08-29 の設計レビューの P1。** 遠い側の端を int16_t で計算していた
  // ので、x + w - 1 が桁溢れした。x=2 / w=32767 なら 32768 が -32768 に
  // 折り返し、「x > x1」で**画面いっぱいに描くべき矩形が丸ごと消えていた。**
  // 総当たりで 28,441 通りの (x, w) が食い違う。
  //
  // 「画面外の座標を受け取ってクリップする」のは契約なので、そこが守れて
  // いなかったことになる。
  {
    bus.fill(BG);
    bus.resetCounters();
    lcd.fillRect(2, 2, 32767, 32767, TFT_WHITE);   // 画面内から始まる巨大な矩形
    tgfxReport("huge_rect_pixels", (long)bus.pixelCount());
  }
  {
    bus.fill(BG);
    bus.resetCounters();
    lcd.fillRect(-32768, -32768, 32767, 32767, TFT_WHITE);  // 遠い左上から
    tgfxReport("far_negative_pixels", (long)bus.pixelCount());
  }
  {
    bus.fill(BG);
    bus.resetCounters();
    lcd.fillRect(30000, 30000, 30000, 30000, TFT_WHITE);    // 画面外の右下だけ
    tgfxReport("far_positive_pixels", (long)bus.pixelCount());
  }
  {
    // クリップ側の桁溢れも同じ
    bus.fill(BG);
    bus.resetCounters();
    lcd.setClipRect(2, 2, 32767, 32767);
    lcd.fillScreen(TFT_WHITE);
    lcd.clearClipRect();
    tgfxReport("huge_clip_pixels", (long)bus.pixelCount());
  }

  tgfxTestDone();
}
void loop() {}
