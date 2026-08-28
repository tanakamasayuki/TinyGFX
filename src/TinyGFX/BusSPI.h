// TinyGFX - Arduino SPI bus (default, portable)
//
// SCK / MOSI は Arduino Core の SPI に任せる。ピンを指定したい環境では
// lcd.begin() より前に自分で SPI.begin(...) を呼び、initSpi=false にする。
#pragma once
#include <Arduino.h>
#include <SPI.h>

#include "Bus.h"

// まとめ書きの単位（画素）。0 で無効。
//
// 有効にすると `writeColor` / `writePixels` が **Arduino 標準の
// `SPI.transfer(buf, len)`（ブロック転送）**を使う。1 バイトずつ `transfer()` を
// 呼ぶより、コアがまとめて流せるぶん速い。RAM は「単位 x 2 バイト」の**スタック**だけ。
//
// **効くのはブロック転送を持つコアだけ。** 持たないコアでは同じか少し遅くなる。
// 32 なら 64 バイト。CH32V003（RAM 2KB）では 8〜16 くらいが上限。
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

  void readData(uint8_t* buf, size_t len) override {
    _spi->endTransaction();
    _spi->beginTransaction(SPISettings(_readFreq, MSBFIRST, SPI_MODE0));
    while (len--) *buf++ = _spi->transfer(0xFF);
    _spi->endTransaction();
    _spi->beginTransaction(SPISettings(_freq, MSBFIRST, SPI_MODE0));
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
    const uint8_t hi = (uint8_t)(color >> 8), lo = (uint8_t)color;
#if TINYGFX_FILL_CHUNK > 0
    if (count >= TINYGFX_FILL_CHUNK) {
      uint8_t buf[TINYGFX_FILL_CHUNK * 2];  // スタック上。静的 RAM は増やさない
      do {
        // **毎回詰め直す。** transfer(buf, n) は受信データで buf を上書きするため。
        // それでも 1 バイトずつ送るより速い（コアがまとめて流せる）。
        for (uint16_t i = 0; i < TINYGFX_FILL_CHUNK; ++i) {
          buf[i * 2] = hi;
          buf[i * 2 + 1] = lo;
        }
        _spi->transfer(buf, sizeof(buf));
        count -= TINYGFX_FILL_CHUNK;
      } while (count >= TINYGFX_FILL_CHUNK);
    }
#endif
    while (count--) { _spi->transfer(hi); _spi->transfer(lo); }
  }

  void writePixels(const uint16_t* data, uint32_t count) override {
#if TINYGFX_FILL_CHUNK > 0
    // 帯レンダリング（TileCanvas）と pushImage がここを通る。
    // 送り出しはビッグエンディアンなので、詰め替えるついでに入れ替える。
    if (count >= TINYGFX_FILL_CHUNK) {
      uint8_t buf[TINYGFX_FILL_CHUNK * 2];
      do {
        for (uint16_t i = 0; i < TINYGFX_FILL_CHUNK; ++i) {
          const uint16_t c = data[i];
          buf[i * 2] = (uint8_t)(c >> 8);
          buf[i * 2 + 1] = (uint8_t)c;
        }
        _spi->transfer(buf, sizeof(buf));
        data += TINYGFX_FILL_CHUNK;
        count -= TINYGFX_FILL_CHUNK;
      } while (count >= TINYGFX_FILL_CHUNK);
    }
#endif
    while (count--) {
      const uint16_t c = *data++;
      _spi->transfer((uint8_t)(c >> 8));
      _spi->transfer((uint8_t)c);
    }
  }

 private:
  SPIClass* _spi;
  uint32_t _freq;
  uint32_t _readFreq = 8000000UL;
  int8_t _dc;
  int8_t _cs;
  bool _initSpi;
};
