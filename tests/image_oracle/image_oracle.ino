// **生成された画像ヘッダが、変換後の期待画像と 1 画素も違わないか。**
//
// GfxImageToolJs 仕様書 §15.2 が指定しているオラクル。ツールが出した .h を
// TinyGFX が実際に描き、ツールが出した期待画像（.ppm）と突き合わせる。
//
// **自作の encode と decode の往復では正しさを証明できない。** 符号化側と
// 復号側が同じ勘違いをしていたら一致してしまう。だから期待画像は
// 「変換後の画素」として別途出してもらい、こちらは描くだけにする。
//
// **このファイルは gen_sketch.py が生成する。手で編集しない。**
// pairs/ に <名前>.h と <名前>.ppm を置けば、次の実行で拾われる。
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelMemory.h>
#include <TinyGFX/Image.h>
#include <tgfx_test.h>
#include "pairs/icon_raw565.h"
#include "pairs/icon_rle565.h"
#include "pairs/icon_rlepal4.h"
#include "pairs/mono_h.h"
#include "pairs/mono_v.h"

static const int W = 32, H = 32;
static uint16_t gram[W * H];
TinyGFXPanelMemory panel(gram, W, H);
TinyGFX lcd(panel);

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("image_oracle");
  lcd.begin();

  // ---- icon_raw565 ----
  panel.fillBuffer(0x0000);
  lcd.drawImage(&icon_raw565Ref, 0, 0);
  tgfxShot("icon_raw565", gram, W, H);

  // ---- icon_rle565 ----
  panel.fillBuffer(0x0000);
  lcd.drawImage(&icon_rle565Ref, 0, 0);
  tgfxShot("icon_rle565", gram, W, H);

  // ---- icon_rlepal4 ----
  panel.fillBuffer(0x0000);
  lcd.drawImage(&icon_rlepal4Ref, 0, 0);
  tgfxShot("icon_rlepal4", gram, W, H);

  // ---- mono_h ----
  panel.fillBuffer(0x0000);
  lcd.drawImage(&mono_hRef, 0, 0);
  tgfxShot("mono_h", gram, W, H);

  // ---- mono_v ----
  panel.fillBuffer(0x0000);
  lcd.drawImage(&mono_vRef, 0, 0);
  tgfxShot("mono_v", gram, W, H);

  tgfxTestDone();
}
void loop() {}
