// **同じ絵をどの形式で符号化しても、1 画素も違わないこと。**
//
// 変換ツール（tools/img2h.py）は絵ごとに最小の形式を総当たりで選ぶ。
// **どれを選んだかがスケッチから見えてはいけない**ので、ここでそれを固定する。
// CellFont の「3 通りの符号化が同じ画素を描く」（tests/text/）と同じ考え方。
//
// 縦詰めと横詰ても同様。データの並びは全く違うが、絵は同じでなければならない。
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <TinyGFX/Image.h>
#include <tgfx_test.h>
#include "same_raw565.h"
#include "same_rle565.h"
#include "same_rlepal4.h"
#include "mono_h.h"
#include "mono_v.h"
#include "trans_icon.h"

static const int W = 32, H = 32;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static uint16_t ref[W * H];
static void snap() { for (int i = 0; i < W * H; ++i) ref[i] = gram[i]; }
static long diff() {
  long d = 0;
  for (int i = 0; i < W * H; ++i) { if (gram[i] != ref[i]) ++d; }
  return d;
}
static long lit() {
  long n = 0;
  for (int i = 0; i < W * H; ++i) { if (gram[i] != 0) ++n; }
  return n;
}
static void reset() { bus.fill(0); bus.resetCounters(); }

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("image_fmt");
  lcd.begin();

  // --- 同じ絵、3 つの形式 ---------------------------------------------------
  reset(); lcd.drawImage(&same_raw565Ref, 0, 0);
  snap();
  tgfxReport("raw_lit", lit());
  tgfxReport("raw_pixels", (long)bus.pixelCount());
  tgfxShot("raw565", gram, W, H);

  reset(); lcd.drawImage(&same_rle565Ref, 0, 0);
  tgfxReport("rle565_diff", diff());
  tgfxShot("rle565", gram, W, H);

  reset(); lcd.drawImage(&same_rlepal4Ref, 0, 0);
  tgfxReport("rlepal4_diff", diff());
  tgfxShot("rlepal4", gram, W, H);

  // --- 1bpp の横詰めと縦詰て -----------------------------------------------
  reset(); lcd.drawImage(&mono_hRef, 0, 0);
  snap();
  tgfxReport("mono_lit", lit());
  tgfxShot("mono_h", gram, W, H);

  reset(); lcd.drawImage(&mono_vRef, 0, 0);
  tgfxReport("mono_v_diff", diff());
  tgfxShot("mono_v", gram, W, H);

  // --- クリップと画面外 -----------------------------------------------------
  reset();
  lcd.setClipRect(8, 8, 8, 8);
  lcd.drawImage(&same_rlepal4Ref, 0, 0);
  lcd.clearClipRect();
  long outside = 0;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if ((x < 8 || x >= 16 || y < 8 || y >= 16) && gram[y * W + x] != 0) ++outside;
    }
  }
  tgfxReport("clip_outside", outside);

  reset();
  lcd.drawImage(&same_rlepal4Ref, -40, -40);
  lcd.drawImage(&same_rlepal4Ref, 40, 40);
  tgfxReport("offscreen_pixels", (long)bus.pixelCount());

  // --- 透過 -----------------------------------------------------------------
  //
  // 同じ画像を、透過ありと無しで描く。**透過色の画素だけが下地を残し、
  // それ以外は 1 画素も違わないこと。**
  //
  // 透過を見るかどうかは形式ではなく ops が決める（生成ヘッダが指す）ので、
  // 透過の無い画像しか使わないスケッチには判定のコードが載らない。
  {
    reset();
    lcd.fillRect(0, 0, W, H, 0xF81F);       // 下地。マゼンタ
    lcd.drawImage(&same_rlepal4Ref, 0, 0);  // 透過なし: 全部塗る
    long bgLeftOpaque = 0;
    for (int i = 0; i < W * H; ++i) { if (gram[i] == 0xF81F) ++bgLeftOpaque; }
    tgfxReport("opaque_bg_left", bgLeftOpaque);
    snap();

    reset();
    lcd.fillRect(0, 0, W, H, 0xF81F);
    lcd.drawImage(&trans_iconRef, 0, 0);    // 透過あり: 黒を飛ばす
    long bgLeftTrans = 0, differ = 0;
    for (int i = 0; i < W * H; ++i) {
      if (gram[i] == 0xF81F) ++bgLeftTrans;
      if (gram[i] != ref[i]) ++differ;
    }
    tgfxReport("trans_bg_left", bgLeftTrans);
    tgfxReport("trans_differ", differ);
    tgfxShot("trans", gram, W, H);
  }

  tgfxTestDone();
}
void loop() {}
