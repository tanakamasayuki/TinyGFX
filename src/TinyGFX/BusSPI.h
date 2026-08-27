// TinyGFX - Arduino SPI bus (default, portable)
//
// SCK / MOSI は Arduino Core の SPI に任せる。ピンを指定したい環境では
// lcd.begin() より前に自分で SPI.begin(...) を呼び、initSpi=false にする。
#pragma once
#include <Arduino.h>
#include <SPI.h>

#include "Bus.h"

#ifndef TINYGFX_FILL_CHUNK
#define TINYGFX_FILL_CHUNK 0
#endif

class TinyGFXBusSPI : public TinyGFXBus {
 public:
  TinyGFXBusSPI(int8_t dc, int8_t cs = -1, uint32_t freq = 24000000UL, SPIClass& spi = SPI,
                bool initSpi = true)
      : _spi(&spi), _freq(freq), _dc(dc), _cs(cs), _initSpi(initSpi) {}

  void init() override {
    pinMode(_dc, OUTPUT);
    digitalWrite(_dc, HIGH);
    if (_cs >= 0) {
      pinMode(_cs, OUTPUT);
      digitalWrite(_cs, HIGH);
    }
    if (_initSpi) _spi->begin();
  }

  void beginTransaction() override {
    _spi->beginTransaction(SPISettings(_freq, MSBFIRST, SPI_MODE0));
    if (_cs >= 0) digitalWrite(_cs, LOW);
  }

  void endTransaction() override {
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    _spi->endTransaction();
  }

  void writeCommand(uint8_t cmd) override {
    digitalWrite(_dc, LOW);
    _spi->transfer(cmd);
    digitalWrite(_dc, HIGH);
  }

  void writeData(const uint8_t* data, size_t len) override {
    while (len--) _spi->transfer(*data++);
  }

  void writeColor(uint16_t color, uint32_t count) override {
#if TINYGFX_FILL_CHUNK > 0
    uint8_t buf[TINYGFX_FILL_CHUNK * 2];  // スタック上。静的 RAM は増やさない
    const uint8_t hi = (uint8_t)(color >> 8), lo = (uint8_t)color;
    for (uint16_t i = 0; i < TINYGFX_FILL_CHUNK; ++i) { buf[i * 2] = hi; buf[i * 2 + 1] = lo; }
    while (count >= TINYGFX_FILL_CHUNK) {
      writeData(buf, sizeof(buf));
      count -= TINYGFX_FILL_CHUNK;
    }
    while (count--) { _spi->transfer(hi); _spi->transfer(lo); }
#else
    const uint8_t hi = (uint8_t)(color >> 8), lo = (uint8_t)color;
    while (count--) { _spi->transfer(hi); _spi->transfer(lo); }
#endif
  }

  void writePixels(const uint16_t* data, uint32_t count) override {
    while (count--) {
      const uint16_t c = *data++;
      _spi->transfer((uint8_t)(c >> 8));
      _spi->transfer((uint8_t)c);
    }
  }

 private:
  SPIClass* _spi;
  uint32_t _freq;
  int8_t _dc;
  int8_t _cs;
  bool _initSpi;
};
