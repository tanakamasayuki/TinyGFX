// TinyGFX - SSD1306 (monochrome OLED, I2C or SPI)
//
// Everything an SSD1306 shares with the other page-addressed 1bpp panels - the
// framebuffer, the band, dirty tracking, rotation, and the byte-wise fill - is
// in DriverPaged.h. What is left here is the init sequence and the transfer.
//
// The transfer is what actually distinguishes this part from an SH1106: an
// SSD1306 takes a column range (0x21) and a page range (0x22) and then accepts
// the whole run in one stream, which is why horizontal addressing mode (0x20
// 0x00) is set at init.
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
#ifndef TINYGFX_SSD1306_COM_PINS
#define TINYGFX_SSD1306_COM_PINS 0x12   // datasheet reset: alternative
#endif
#ifndef TINYGFX_SSD1306_PRECHARGE
#define TINYGFX_SSD1306_PRECHARGE 0x22  // datasheet reset: phase1 = 2, phase2 = 2
#endif
#ifndef TINYGFX_SSD1306_VCOMH
#define TINYGFX_SSD1306_VCOMH 0x20      // datasheet reset: 0.77 x Vcc
#endif
#ifndef TINYGFX_SSD1306_CONTRAST
#define TINYGFX_SSD1306_CONTRAST 0xCF
#endif
#ifndef TINYGFX_SSD1306_CLOCKDIV
#define TINYGFX_SSD1306_CLOCKDIV 0x80
#endif

// Marks that this driver is in the build. A panel header refuses to be the
// second one for the same driver (docs/GLOSSARY.md 3).
#define TINYGFX_DRIVER_SSD1306_INCLUDED 1


class TinyGFXDriverSSD1306 : public TinyGFXDriverPaged {
 public:
  /// `buffer` is w * h / 8 bytes - 1,024 for 128x64. See TinyGFXDriverPaged for
  /// what `bufferPages` does.
  TinyGFXDriverSSD1306(TinyGFXBus& bus, uint8_t* buffer, int16_t w = 128, int16_t h = 64,
                      int16_t bufferPages = 0)
      : TinyGFXDriverPaged(bus, buffer, w, h, bufferPages, 128) {}

  bool init() override;

  /// Push only the pages that changed. Nothing reaches the screen until this
  /// is called.
  void display();

  // Deliberately not virtual - not worth charging everyone for
  void invertDisplay(bool invert) { cmd(invert ? 0xA7 : 0xA6); }
  void setSleep(bool sleep) { cmd(sleep ? 0xAE : 0xAF); }
  void setContrast(uint8_t value) { cmd(0x81); cmd(value); }
};

inline bool TinyGFXDriverSSD1306::init() {
  // Split in three because two of the bytes depend on how tall the glass is.
  // A 128x32 wants multiplex 0x1F and COM pins 0x02 where a 128x64 wants 0x3F
  // and 0x12; sending the 64-row pair to a 32-row panel squeezes the picture
  // into half the glass, which is the usual symptom of a library that
  // hardcodes them.
  static const uint8_t kHead[] = {
      0xAE,        // display off
      0xD5, TINYGFX_SSD1306_CLOCKDIV,  // clock
  };
  static const uint8_t kMid[] = {
      0xD3, 0x00,  // display offset
      0x40,        // start line 0
      0x8D, 0x14,  // charge pump on
      0x20, 0x00,  // horizontal addressing
      0xA1,        // segment remap
      0xC8,        // com scan dec
  };
  static const uint8_t kTail[] = {
      0x81, TINYGFX_SSD1306_CONTRAST,  // contrast
      0xD9, TINYGFX_SSD1306_PRECHARGE,  // precharge
      0xDB, TINYGFX_SSD1306_VCOMH,  // vcom detect
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
  _bus->writeCommand(TINYGFX_SSD1306_COM_PINS);
  for (uint8_t i = 0; i < sizeof(kTail); ++i) _bus->writeCommand(kTail[i]);
  _bus->endTransaction();
  clearBuffer(false);
  return true;
}

inline void TinyGFXDriverSSD1306::display() {
  if (_dirtyHi < _dirtyLo) return;  // nothing changed
  // Dirty pages are tracked in buffer space; the screen may be further down.
  const int16_t base = (_bandPages != 0) ? _pageFirst : 0;
  _bus->beginTransaction();
  _bus->writeCommand(0x21);
  _bus->writeCommand(_col0);
  _bus->writeCommand((uint8_t)(_col0 + _natW - 1));            // column range
  _bus->writeCommand(0x22);
  _bus->writeCommand((uint8_t)(base + _dirtyLo));
  _bus->writeCommand((uint8_t)(base + _dirtyHi));              // page range
  _bus->writeData(&_buf[(int32_t)_dirtyLo * _natW],
                  (size_t)((int32_t)(_dirtyHi - _dirtyLo + 1) * _natW));
  _bus->endTransaction();
  _dirtyLo = 32767;
  _dirtyHi = -1;
}
