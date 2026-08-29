// TinyGFX - SSD1306 (monochrome OLED, I2C or SPI)
//
// Everything an SSD1306 shares with the other page-addressed 1bpp panels - the
// framebuffer, the band, dirty tracking, rotation, and the byte-wise fill - is
// in PanelPaged.h. What is left here is the init sequence and the transfer.
//
// The transfer is what actually distinguishes this part from an SH1106: an
// SSD1306 takes a column range (0x21) and a page range (0x22) and then accepts
// the whole run in one stream, which is why horizontal addressing mode (0x20
// 0x00) is set at init.
#pragma once
#include <stdint.h>

#include "PanelPaged.h"

class TinyGFXPanelSSD1306 : public TinyGFXPanelPaged {
 public:
  /// `buffer` is w * h / 8 bytes - 1,024 for 128x64. See TinyGFXPanelPaged for
  /// what `bufferPages` does.
  TinyGFXPanelSSD1306(TinyGFXBus& bus, uint8_t* buffer, int16_t w = 128, int16_t h = 64,
                      int16_t bufferPages = 0)
      : TinyGFXPanelPaged(bus, buffer, w, h, bufferPages) {}

  bool init() override;

  /// Push only the pages that changed. Nothing reaches the screen until this
  /// is called.
  void display();

  // Deliberately not virtual - not worth charging everyone for
  void invertDisplay(bool invert) { cmd(invert ? 0xA7 : 0xA6); }
  void setSleep(bool sleep) { cmd(sleep ? 0xAE : 0xAF); }
  void setContrast(uint8_t value) { cmd(0x81); cmd(value); }
};

inline bool TinyGFXPanelSSD1306::init() {
  static const uint8_t kInit[] = {
      0xAE,        // display off
      0xD5, 0x80,  // clock
      0xA8, 0x3F,  // multiplex（128x64）
      0xD3, 0x00,  // display offset
      0x40,        // start line 0
      0x8D, 0x14,  // charge pump on
      0x20, 0x00,  // horizontal addressing
      0xA1,        // segment remap
      0xC8,        // com scan dec
      0xDA, 0x12,  // com pins
      0x81, 0xCF,  // contrast
      0xD9, 0xF1,  // precharge
      0xDB, 0x40,  // vcom detect
      0xA4,        // resume from RAM
      0xA6,        // normal (not inverted)
      0xAF,        // display on
  };
  _bus->init();
  // One transaction around the whole sequence; SPI.beginTransaction() does not
  // nest, so cmd() cannot be used here.
  _bus->beginTransaction();
  for (uint8_t i = 0; i < sizeof(kInit); ++i) _bus->writeCommand(kInit[i]);
  _bus->endTransaction();
  clearBuffer(false);
  return true;
}

inline void TinyGFXPanelSSD1306::display() {
  if (_dirtyHi < _dirtyLo) return;  // nothing changed
  // Dirty pages are tracked in buffer space; the screen may be further down.
  const int16_t base = (_bandPages != 0) ? _pageFirst : 0;
  _bus->beginTransaction();
  _bus->writeCommand(0x21);
  _bus->writeCommand(0);
  _bus->writeCommand((uint8_t)(_natW - 1));                    // column range
  _bus->writeCommand(0x22);
  _bus->writeCommand((uint8_t)(base + _dirtyLo));
  _bus->writeCommand((uint8_t)(base + _dirtyHi));              // page range
  _bus->writeData(&_buf[(int32_t)_dirtyLo * _natW],
                  (size_t)((int32_t)(_dirtyHi - _dirtyLo + 1) * _natW));
  _bus->endTransaction();
  _dirtyLo = 32767;
  _dirtyHi = -1;
}
