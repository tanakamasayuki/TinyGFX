// TinyGFX - ST7789 panel
//
// Everything an ST7789 shares with the other DCS controllers is in DriverDcs.h.
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

#include "DriverDcs.h"

// Marks that this driver is in the build. A panel header refuses to be the
// second one for the same driver (docs/GLOSSARY.md 3).
#define TINYGFX_DRIVER_ST7789_INCLUDED 1

class TinyGFXDriverST7789 : public TinyGFXDriverDcs {
 public:
  TinyGFXDriverST7789(TinyGFXBus& bus, int16_t w, int16_t h, int8_t rst = -1)
      : TinyGFXDriverDcs(bus, w, h, rst, /*bgr*/ false, /*invert*/ true) {}
};
