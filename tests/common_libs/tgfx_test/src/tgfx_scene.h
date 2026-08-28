// ホストと実機で**同じ絵**を描くための共通シーン。
//
// ホスト側（tests/scene/）が BusCapture で描いてゴールデンを作り、
// 実機側（tests/hw/m5stack/）がパネルに描いて読み戻し、同じかを見る。
// **1 箇所にしか書かない**ので、片方だけ変わって嘘の一致になることがない。
//
// **使う色は全チャンネルが 0 か満のものだけ。** パネルの読み戻しは 1 画素 3 バイトの
// RGB666 で返るので、中間の階調は往復で 1 LSB ずれうる。飽和した色なら
// 5bit 31 -> 6bit 63 -> 5bit 31 とちょうど戻るので、**バイト一致で比較できる。**
#pragma once
#include <TinyGFX.h>

static const int16_t TGFX_SCENE_W = 64;
static const int16_t TGFX_SCENE_H = 48;

/// フォントは呼び出し側で setFont() / setTextColor() しておくこと。
inline void tgfxGoldenScene(TinyGFX& g) {
  g.startWrite();
  g.fillRect(0, 0, TGFX_SCENE_W, TGFX_SCENE_H, TFT_BLACK);
  g.drawRect(0, 0, TGFX_SCENE_W, TGFX_SCENE_H, TFT_WHITE);

  g.fillRect(2, 2, 12, 12, TFT_RED);
  g.fillRect(16, 2, 12, 12, TFT_GREEN);
  g.fillRect(30, 2, 12, 12, TFT_BLUE);
  g.fillCircle(52, 8, 6, TFT_YELLOW);

  g.drawLine(2, 18, 61, 26, TFT_CYAN);
  g.fillTriangle(4, 44, 13, 30, 22, 44, TFT_MAGENTA);
  g.drawRoundRect(26, 30, 20, 14, 4, TFT_WHITE);

  g.setTextSize(1);
  g.drawString("0123", 30, 18);
  g.endWrite();
}
