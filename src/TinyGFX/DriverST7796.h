// TinyGFX - ST7796 driver (the 3.5 inch 320x480 SPI breakouts)
//
// A Sitronix sibling of the ST7789 with a larger memory, and it speaks the
// same DCS command set. Everything it shares with the other DCS controllers is
// in DriverDcs.h; what is left here is its character - 320x480 of memory, BGR
// order, and inversion off.
//
// **It comes up on its own power-on defaults**, like the ST7789 and the
// ILI934x, which is why this file is a constructor and nothing else. A
// controller that needs its frame-rate and power blocks sent (an ST7735, say)
// would need more.
//
// Include a panel from TinyGFX/panels/ rather than this header directly.
#pragma once
#include <stdint.h>

#include "DriverDcs.h"

// Marks that this driver is in the build. A panel header refuses to be the
// second one for the same driver (docs/GLOSSARY.md 3).
#define TINYGFX_DRIVER_ST7796_INCLUDED 1

class TinyGFXDriverST7796 : public TinyGFXDriverDcs {
 public:
  TinyGFXDriverST7796(TinyGFXBus& bus, int16_t w = 320, int16_t h = 480, int8_t rst = -1)
      : TinyGFXDriverDcs(bus, w, h, rst, /*bgr*/ true, /*invert*/ false) {}
};
