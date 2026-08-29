// The shared scene, so the host and the hardware draw **the same picture**.
//
// The host side (tests/scene/) draws it through BusCapture to make the golden;
// the hardware side (tests/hw/m5stack/) draws it on a panel, reads it back, and
// compares. **It is written in exactly one place**, so one side cannot drift
// and produce a false match.
//
// **Every colour has all its channels either 0 or full.** Reading a panel back
// returns RGB666, three bytes a pixel, so an intermediate level can come back
// 1 LSB off. A saturated colour goes 5-bit 31 -> 6-bit 63 -> 5-bit 31 and lands
// exactly where it started, which is what makes **a byte-for-byte comparison**
// possible.
#pragma once
#include <TinyGFX.h>

static const int16_t TGFX_SCENE_W = 64;
static const int16_t TGFX_SCENE_H = 48;

/// The caller is expected to have called setFont() and setTextColor().
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
