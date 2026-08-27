// TinyGFX - software (bit-banged) SPI bus
//
// pinMode / digitalWrite だけで動く。SPI ライブラリを持たない、あるいは
// 対象バリアントで SPI がリンクできないコア（CH32V003 など）でも使える。
// 遅いが、どの Arduino Core でも必ず動くのが取り柄。
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
