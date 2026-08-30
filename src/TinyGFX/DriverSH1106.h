// TinyGFX - SH1106 (monochrome OLED, I2C or SPI)
//
// Sold as an "SSD1306" more often than not, and mostly is one - same 128x64
// glass, same page-addressed 1bpp memory, nearly the same init. Everything it
// shares with the SSD1306 is in DriverPaged.h.
//
// Two things are genuinely different, and both are here.
//
//   1. **132 columns of RAM behind a 128-column panel.** The glass sits in the
//      middle, so column 0 of the picture is column 2 of the memory. Get this
//      wrong and the image is shifted two pixels with a stripe of rubbish down
//      one edge - the classic "my SSD1306 library almost works on this screen".
//   2. **No column/page range commands.** An SSD1306 is handed 0x21 / 0x22 and
//      then streamed the whole run. An SH1106 has neither, so the cursor is
//      set per page (0xB0 | page, then the column split across 0x00 / 0x10)
//      and each page is written on its own. Costs three command bytes a page -
//      24 bytes on a full 128x64 frame, against 1,024 bytes of pixels.
//
// It also uses a different charge pump command (0xAD 0x8B, against the
// SSD1306's 0x8D 0x14) and wants its pump voltage set.
//
// **Not yet confirmed on real glass** (docs/MANUAL_TEST.ja.md M6). The shared
// half is exercised by the SSD1306 tests; what is untested is this file. If
// the picture is shifted, setColumnOffset() is the one line to try.
#pragma once
#include <stdint.h>

#include "DriverPaged.h"

// ---- what a panel header sets ---------------------------------------------
//
// A panel header (TinyGFX/panels/) defines these before including this file.
// Every default below is **the controller's own reset value**, taken from the
// datasheet - not the 0xF1 / 0x40 pair that circulates through most libraries,
// which appears in no datasheet table at all (docs/GLOSSARY.md 4).
//
// COM_PINS is the one value that cannot be derived or defaulted away: it says
// how the glass's COM lines are wired, sequential (0x02) or alternative
// (0x12). **Every other row coming out as a stripe is what the wrong one looks
// like.**
#ifndef TINYGFX_SH1106_COM_PINS
#define TINYGFX_SH1106_COM_PINS 0x12   // datasheet reset: alternative
#endif
#ifndef TINYGFX_SH1106_PRECHARGE
#define TINYGFX_SH1106_PRECHARGE 0x22  // datasheet reset: phase1 = 2, phase2 = 2
#endif
#ifndef TINYGFX_SH1106_VCOMH
#define TINYGFX_SH1106_VCOMH 0x20      // datasheet reset: 0.77 x Vcc
#endif
#ifndef TINYGFX_SH1106_CONTRAST
#define TINYGFX_SH1106_CONTRAST 0xCF
#endif
#ifndef TINYGFX_SH1106_CLOCKDIV
#define TINYGFX_SH1106_CLOCKDIV 0x80
#endif

// Marks that this driver is in the build. A panel header refuses to be the
// second one for the same driver (docs/GLOSSARY.md 3).
#define TINYGFX_DRIVER_SH1106_INCLUDED 1


class TinyGFXDriverSH1106 : public TinyGFXDriverPaged {
 public:
  /// `buffer` is w * h / 8 bytes - 1,024 for 128x64. See TinyGFXDriverPaged for
  /// what `bufferPages` does.
  TinyGFXDriverSH1106(TinyGFXBus& bus, uint8_t* buffer, int16_t w = 128, int16_t h = 64,
                     int16_t bufferPages = 0)
      : TinyGFXDriverPaged(bus, buffer, w, h, bufferPages, 132) {}

  bool init() override;

  /// Push only the pages that changed. Nothing reaches the screen until this
  /// is called.
  void display();

  // Deliberately not virtual - not worth charging everyone for
  void invertDisplay(bool invert) { cmd(invert ? 0xA7 : 0xA6); }
  void setSleep(bool sleep) { cmd(sleep ? 0xAE : 0xAF); }
  void setContrast(uint8_t value) { cmd(0x81); cmd(value); }
};

inline bool TinyGFXDriverSH1106::init() {
  // Split in three because two of the bytes depend on how tall the glass is.
  // A 128x32 wants multiplex 0x1F and COM pins 0x02 where a 128x64 wants 0x3F
  // and 0x12; sending the 64-row pair to a 32-row panel squeezes the picture
  // into half the glass, which is the usual symptom of a library that
  // hardcodes them.
  static const uint8_t kHead[] = {
      0xAE,        // display off
      0xD5, TINYGFX_SH1106_CLOCKDIV,  // clock
  };
  static const uint8_t kMid[] = {
      0xD3, 0x00,  // display offset
      0x40,        // start line 0
      0xAD, 0x8B,  // DC-DC on (the SSD1306 spells this 0x8D 0x14)
      0x33,        // pump voltage 9.0V - SH1106 only
      0xA1,        // segment remap
      0xC8,        // com scan dec
  };
  static const uint8_t kTail[] = {
      0x81, TINYGFX_SH1106_CONTRAST,  // contrast
      0xD9, TINYGFX_SH1106_PRECHARGE,  // precharge
      0xDB, TINYGFX_SH1106_VCOMH,  // vcom detect
      0xA4,        // resume from RAM
      0xA6,        // normal (not inverted)
      0xAF,        // display on
  };
  if (!configOk()) return false;
  _bus->init();
  // One transaction around the whole sequence; SPI.beginTransaction() does not
  // nest, so cmd() cannot be used here.
  _bus->beginTransaction();
  for (uint8_t i = 0; i < sizeof(kHead); ++i) _bus->writeCommand(kHead[i]);
  _bus->writeCommand(0xA8);
  _bus->writeCommand(multiplexRatio());
  for (uint8_t i = 0; i < sizeof(kMid); ++i) _bus->writeCommand(kMid[i]);
  _bus->writeCommand(0xDA);
  _bus->writeCommand(TINYGFX_SH1106_COM_PINS);
  for (uint8_t i = 0; i < sizeof(kTail); ++i) _bus->writeCommand(kTail[i]);
  _bus->endTransaction();
  clearBuffer(false);
  return true;
}

inline void TinyGFXDriverSH1106::display() {
  if (_dirtyHi < _dirtyLo) return;  // nothing changed
  // Dirty pages are tracked in buffer space; the screen may be further down.
  const int16_t base = (_bandPages != 0) ? _pageFirst : 0;
  _bus->beginTransaction();
  for (int16_t p = _dirtyLo; p <= _dirtyHi; ++p) {
    _bus->writeCommand((uint8_t)(0xB0 | (uint8_t)(base + p)));  // page
    _bus->writeCommand((uint8_t)(0x00 | (_col0 & 0x0F)));       // column, low nibble
    _bus->writeCommand((uint8_t)(0x10 | (_col0 >> 4)));         // column, high nibble
    _bus->writeData(&_buf[(int32_t)p * _natW], (size_t)_natW);
  }
  _bus->endTransaction();
  _dirtyLo = 32767;
  _dirtyHi = -1;
}
