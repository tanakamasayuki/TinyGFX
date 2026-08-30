// TinyGFX - software (bit-banged) SPI bus
//
// Runs on nothing but pinMode and digitalWrite, so it works on cores that
// ship no SPI library at all, or where SPI will not link for the target
// variant (the CH32V003 cores, for instance). It is slow, but it is the one
// bus that works on every Arduino core.
//
// **A pin bus, so the pins are TinyGFX's** - the opposite of TinyGFXBusSPI,
// where you hand over a peripheral you already began and TinyGFX only drives
// DC and CS. Here all four pins are made outputs and driven from here, so
// nothing else may use them (docs/GLOSSARY.md).
//
// "Soft" means TinyGFX does the bit-banging. It does not mean TinyGFXBusSPI is
// necessarily hardware: that one takes whatever the core calls SPI, and
// whether the core implements it in hardware is not something TinyGFX can see
// or needs to.
#pragma once
#include <Arduino.h>

#include "Bus.h"

class TinyGFXBusSoftSPI : public TinyGFXBus {
 public:
  TinyGFXBusSoftSPI(int8_t sck, int8_t mosi, int8_t dc, int8_t cs = -1)
      : _sck(sck), _mosi(mosi), _dc(dc), _cs(cs) {}

  void init() override {
    pinMode(_sck, OUTPUT);
    pinMode(_mosi, OUTPUT);
    pinMode(_dc, OUTPUT);
    digitalWrite(_sck, LOW);
    digitalWrite(_dc, HIGH);
    if (_cs >= 0) {
      pinMode(_cs, OUTPUT);
      digitalWrite(_cs, HIGH);
    }
  }

  void beginTransaction() override {
    if (_cs >= 0) digitalWrite(_cs, LOW);
  }
  void endTransaction() override {
    if (_cs >= 0) digitalWrite(_cs, HIGH);
  }

  /// Cannot read: this bus is never told a MISO pin, so there is no wire.
  /// Use TinyGFXBusSPI when you need read-back.
  /// It is left empty rather than pure so that __cxa_pure_virtual stays out
  /// of the build (same reason as Bus.h).
  void readSequence(const uint8_t* script, uint8_t scriptLen, uint8_t dummy, uint8_t* buf,
                    size_t len) override {
    (void)script;
    (void)scriptLen;
    (void)dummy;
    (void)buf;
    (void)len;
  }

  void writeCommand(uint8_t cmd) override {
    digitalWrite(_dc, LOW);
    shiftOutByte(cmd);
    digitalWrite(_dc, HIGH);
  }

  void writeData(const uint8_t* data, size_t len) override {
    while (len--) shiftOutByte(*data++);
  }

  void writeColor(uint16_t color, uint32_t count) override {
    const uint8_t hi = (uint8_t)(color >> 8), lo = (uint8_t)color;
    while (count--) { shiftOutByte(hi); shiftOutByte(lo); }
  }

  void writePixels(const uint16_t* data, uint32_t count) override {
    while (count--) {
      const uint16_t c = *data++;
      shiftOutByte((uint8_t)(c >> 8));
      shiftOutByte((uint8_t)c);
    }
  }

 private:
  void shiftOutByte(uint8_t b) {
    for (uint8_t i = 0; i < 8; ++i) {
      digitalWrite(_mosi, (b & 0x80) ? HIGH : LOW);
      digitalWrite(_sck, HIGH);
      b = (uint8_t)(b << 1);
      digitalWrite(_sck, LOW);
    }
  }

  int8_t _sck, _mosi, _dc, _cs;
};
