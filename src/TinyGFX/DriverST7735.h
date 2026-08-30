// TinyGFX - ST7735 driver (the 1.8 / 1.44 / 0.96 inch SPI breakouts)
//
// A Sitronix relative of the ST7789 with a smaller, oddly shaped memory:
// 132x162, where the glass is usually 128x160 or 128x128 and sits somewhere
// inside it. **Which is why an ST7735 needs a panel from TinyGFX/panels/ more
// than any other controller here** - the same chip is sold behind at least
// four different pieces of glass, at three offsets and two colour orders.
//
// **Nothing but the power-on defaults is sent**, as for every other DCS
// controller here. Other libraries send frame-rate, power and gamma blocks;
// those are tuning, not the datasheet's reset values, and the same reasoning
// that put the SSD1306 on its reset values applies (docs/DECISIONS.ja.md D34).
//
// If a module turns out to need them, DriverDcs would want a hook to send a
// sequence between SLPOUT and COLMOD. That was prototyped and measured at
// **0 bytes for drivers that do not use it**, so it can be added when there is
// evidence rather than on suspicion.
//
// Include a panel from TinyGFX/panels/ rather than this header directly.
#pragma once
#include <stdint.h>

#include "DriverDcs.h"

// Marks that this driver is in the build. A panel header refuses to be the
// second one for the same driver (docs/GLOSSARY.md 3).
#define TINYGFX_DRIVER_ST7735_INCLUDED 1

class TinyGFXDriverST7735 : public TinyGFXDriverDcs {
 public:
  TinyGFXDriverST7735(TinyGFXBus& bus, int16_t w = 128, int16_t h = 160, int8_t rst = -1)
      : TinyGFXDriverDcs(bus, w, h, rst, /*bgr*/ true, /*invert*/ false) {}
};
