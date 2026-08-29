// TinyGFX - ST7789 panel
//
// Everything an ST7789 shares with the other DCS controllers is in PanelDcs.h.
// What is left here is what makes it an ST7789: a portrait 240x320 GRAM, RGB
// order, and inversion on (these panels are normally inverted).
//
// Modules vary a lot in how the glass sits on that GRAM, so most of them need
// one or two lines. Order does not matter, and either side of begin() works:
//
//   240x240  panel.setGramSize(240, 320);
//   135x240  panel.setGramSize(240, 320); panel.setOffset(52, 40);
#pragma once
#include <stdint.h>

#include "PanelDcs.h"

class TinyGFXPanelST7789 : public TinyGFXPanelDcs {
 public:
  TinyGFXPanelST7789(TinyGFXBus& bus, int16_t w, int16_t h, int8_t rst = -1)
      : TinyGFXPanelDcs(bus, w, h, rst, /*bgr*/ false, /*invert*/ true) {}
};
