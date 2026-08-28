// TinyGFX - ILI9342C panel (M5Stack Core / BASIC and friends)
//
// A sibling of the ILI9341, except its GRAM is landscape from the start
// (320x240). There are no modules with an origin offset, so setOffset() and
// setGramSize() - inherited from PanelDcs - are left alone here.
//
// Everything it shares with the other DCS controllers is in PanelDcs.h. What
// is left here is what makes it an ILI9342C: a landscape 320x240 GRAM, BGR
// order, and inversion on.
//
// Two things can only be confirmed on real glass, and each is a one-line fix
// after begin():
//   1. Colour order   -> panel.setRgbOrder(false) if red and blue are swapped
//   2. Inversion      -> panel.invertDisplay(false) on older BASIC units
//   3. How it is mounted -> panel.setMirror(mx, my)
//
// The long gamma and power init sequences are left out on purpose. An ILI934x
// comes up fine on its power-on defaults; add them when you want to chase the
// colour rendition.
#pragma once
#include <stdint.h>

#include "PanelDcs.h"

class TinyGFXPanelILI9342 : public TinyGFXPanelDcs {
 public:
  TinyGFXPanelILI9342(TinyGFXBus& bus, int16_t w = 320, int16_t h = 240, int8_t rst = -1)
      : TinyGFXPanelDcs(bus, w, h, rst, /*bgr*/ true, /*invert*/ true) {}
};
