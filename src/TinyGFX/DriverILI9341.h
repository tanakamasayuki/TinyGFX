// TinyGFX - ILI9341 panel (the 2.4"/2.8" SPI breakouts)
//
// The ILI9342C's sibling with a portrait GRAM: 240x320 instead of 320x240.
// Everything it shares with the other DCS controllers is in DriverDcs.h.
//
// Three things make it an ILI9341 rather than an ILI9342C:
//   1. 240x320 GRAM
//   2. inversion **off** at power-on (an ILI9341 is not normally inverted,
//      unlike an ST7789)
//   3. mirrored on X at rotation 0
//
// The mirror deserves a word. The usual breakout is mounted so that the
// conventional rotation 0 - portrait, ribbon at the bottom - wants MADCTL
// MX|BGR (0x48), which is what every other library for this part sends. The
// base class builds rotation 0 as plain MADCTL 0, so the MX is supplied here
// as the module's mounting.
//
// **Not yet confirmed on real glass** (docs/MANUAL_TEST.ja.md M5). Everything
// below the mounting is shared with the ILI9342C, which is verified, so what
// is untested is exactly these three lines. If a picture comes out mirrored or
// inverted, one call after begin() fixes it:
//
//   panel.setMirror(false, false);   // or whichever way it needs
//   panel.invertDisplay(true);
//   panel.setRgbOrder(false);
#pragma once
#include <stdint.h>

#include "DriverDcs.h"

// Marks that this driver is in the build. A panel header refuses to be the
// second one for the same driver (docs/GLOSSARY.md 3).
#define TINYGFX_DRIVER_ILI9341_INCLUDED 1

class TinyGFXDriverILI9341 : public TinyGFXDriverDcs {
 public:
  TinyGFXDriverILI9341(TinyGFXBus& bus, int16_t w = 240, int16_t h = 320, int8_t rst = -1)
      : TinyGFXDriverDcs(bus, w, h, rst, /*bgr*/ true, /*invert*/ false) {
    setMirror(true, false);
  }
};
