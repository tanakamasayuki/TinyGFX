// TinyGFX - Arduino SPI bus (default, portable)
//
// Hand it an SPI bus you have already begun. TinyGFX never calls SPI.begin()
// and never picks the SCK / MOSI pins: owning the bus is the sketch's job, so
// an SD card or anything else on the same wires keeps working. Sharing is done
// the standard Arduino way, with beginTransaction / endTransaction around
// every burst.
//
// DC and CS are different - those belong to this panel, so this class does
// drive them.
#pragma once
#include <Arduino.h>
#include <SPI.h>

#include "Bus.h"

// Block-write size in pixels; 0 disables it.
//
// When enabled, writeColor and writePixels use the standard Arduino
// SPI.transfer(buf, len) block transfer, which lets the core push bytes in
// bulk instead of one transfer() call each. The only RAM cost is size * 2
// bytes of stack.
//
// How many bytes a read may be and still be retried until two attempts agree.
// Only the read path uses this, and only on the stack.
#ifndef TINYGFX_READ_AGREE_MAX
#define TINYGFX_READ_AGREE_MAX 8
#endif

// This only helps on cores that actually implement a block transfer; elsewhere
// it is a wash or marginally slower. 32 means 64 bytes; on a CH32V003 (2 KB of
// RAM) 8 to 16 is about the ceiling.
#ifndef TINYGFX_FILL_CHUNK
#define TINYGFX_FILL_CHUNK 0
#endif

class TinyGFXBusSPI : public TinyGFXBus {
 public:
  /// `spi` must already be begun. Call SPI.begin() - with whatever pins your
  /// board needs - before lcd.begin().
  TinyGFXBusSPI(SPIClass& spi, int8_t dc, int8_t cs = -1, uint32_t freq = 24000000UL)
      : _spi(&spi), _freq(freq), _dc(dc), _cs(cs) {}

  /// Only DC and CS are set up here. The SPI bus is yours.
  void init() override {
    pinMode(_dc, OUTPUT);
    digitalWrite(_dc, HIGH);
    if (_cs >= 0) {
      pinMode(_cs, OUTPUT);
      digitalWrite(_cs, HIGH);
    }
  }

  void beginTransaction() override {
    if (_rdActive) endRead();  // hand the line back to the peripheral first
    _spi->beginTransaction(SPISettings(_freq, MSBFIRST, SPI_MODE0));
    if (_cs >= 0) digitalWrite(_cs, LOW);
  }

  void endTransaction() override {
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    _spi->endTransaction();
  }

  /// For panels with a single shared data line (the ILI9342C on an M5Stack).
  ///
  /// On some boards SDA doubles as MOSI and MISO, and the SPI peripheral's
  /// MISO pin is connected to nothing. A plain transfer() cannot read there.
  /// Given SCK and SDA, the whole exchange is clocked by hand instead, and the
  /// pins are handed back to the peripheral before the next draw.
  ///
  /// For an M5Stack Core / BASIC: bus.setReadPins(18, 23);
  ///
  /// Read-back is for debugging and verification. It is slow - roughly 150us
  /// per pixel - and it is not something normal drawing should touch.
  ///
  /// Read outside startWrite() / endWrite(): it takes the line independently
  /// of the drawing transaction.
  void setReadPins(int8_t sck, int8_t sda) {
    _rdSck = sck;
    _rdSda = sda;
  }

  /// Hand the bit-banged line back to the SPI peripheral.
  ///
  /// You do not have to call this - the next draw does it from
  /// beginTransaction(). **end() then begin(), in that order**: ESP32's
  /// SPI.begin() returns early when the bus is already started, so begin()
  /// alone leaves the pins in GPIO mode and every later write silently goes
  /// nowhere (measured).
  void endRead() {
    if (!_rdActive) return;
    _rdActive = false;
    digitalWrite(_rdSck, LOW);
    pinMode(_rdSda, OUTPUT);
    _spi->end();
    _spi->begin();
  }

  /// Send commands, then read back. CS is asserted and released in here.
  ///
  /// Reads the same thing twice and keeps going until two attempts agree.
  /// About one byte in twenty comes back with a bit flipped, and the clock
  /// cannot be slowed to fix it: **any delay at all and the panel stops
  /// driving the line**, so every byte reads FF (all measured).
  void readSequence(const uint8_t* script, uint8_t scriptLen, uint8_t dummy, uint8_t* buf,
                    size_t len) override {
    for (size_t i = 0; i < len; ++i) buf[i] = 0;
    if (_rdSck < 0) return;  // this bus cannot read

    if (!_rdActive) {
      // Take the pins with pinMode alone. Calling SPI.end() first looks tidier
      // but kills it - every read then comes back FF (measured).
      pinMode(_rdSck, OUTPUT);
      pinMode(_rdSda, OUTPUT);
      digitalWrite(_rdSck, LOW);
      _rdActive = true;
    }
    bbSequence(script, scriptLen, dummy, buf, len);

    if (len <= TINYGFX_READ_AGREE_MAX) {
      uint8_t again[TINYGFX_READ_AGREE_MAX];
      for (uint8_t t = 0; t < 4; ++t) {
        bbSequence(script, scriptLen, dummy, again, len);
        bool same = true;
        for (size_t i = 0; i < len; ++i) {
          if (buf[i] != again[i]) same = false;
        }
        if (same) return;
        for (size_t i = 0; i < len; ++i) buf[i] = again[i];
      }
    }
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
    const uint8_t hi = (uint8_t)(color >> 8);
    const uint8_t lo = (uint8_t)color;
#if TINYGFX_FILL_CHUNK > 0
    if (count >= TINYGFX_FILL_CHUNK) {
      uint8_t buf[TINYGFX_FILL_CHUNK * 2];  // on the stack; no static RAM added
      do {
        // Refill every time: transfer(buf, n) overwrites buf with what came
        // back. Even so this beats sending a byte at a time, because the core
        // can push the block in bulk.
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
    // Tiled rendering (TileCanvas) and pushImage both come through here.
    // The wire format is big endian, so swap while packing.
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
  // Hand-clocked, and deliberately tight. Inserting so much as a
  // delayMicroseconds(1) per edge makes the panel stop answering (measured).
  void bbOut(uint8_t v, bool isCmd) {
    digitalWrite(_dc, isCmd ? LOW : HIGH);
    for (int8_t i = 7; i >= 0; --i) {
      digitalWrite(_rdSda, (v >> i) & 1);
      digitalWrite(_rdSck, HIGH);
      digitalWrite(_rdSck, LOW);
    }
    digitalWrite(_dc, HIGH);
  }

  uint8_t bbIn() {
    uint8_t v = 0;
    for (uint8_t i = 0; i < 8; ++i) {  // mode 0: sample on the rising edge
      digitalWrite(_rdSck, HIGH);
      v = (uint8_t)((v << 1) | (digitalRead(_rdSda) ? 1 : 0));
      digitalWrite(_rdSck, LOW);
    }
    return v;
  }

  void bbSequence(const uint8_t* script, uint8_t scriptLen, uint8_t dummy, uint8_t* buf,
                  size_t len) {
    pinMode(_rdSda, OUTPUT);
    if (_cs >= 0) digitalWrite(_cs, LOW);
    uint8_t i = 0;
    while (i < scriptLen) {
      const uint8_t cmd = script[i++];
      const uint8_t n = script[i++];
      bbOut(cmd, true);
      for (uint8_t k = 0; k < n; ++k) bbOut(script[i + k], false);
      i = (uint8_t)(i + n);
    }
    pinMode(_rdSda, INPUT);  // hand the line to the panel
    for (uint8_t d = 0; d < dummy; ++d) bbIn();
    for (size_t k = 0; k < len; ++k) buf[k] = bbIn();
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    pinMode(_rdSda, OUTPUT);
  }

  SPIClass* _spi;
  uint32_t _freq;
  int8_t _rdSck = -1;
  int8_t _rdSda = -1;
  bool _rdActive = false;
  int8_t _dc;
  int8_t _cs;
};
