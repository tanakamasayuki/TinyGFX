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

  /// Hand the bit-banged data line back to the SPI peripheral.
  /// You do not have to call this: the next draw does it from
  /// beginTransaction().
  void endRead() {
    if (!_rdActive) return;
    _rdActive = false;
    digitalWrite(_rdSck, LOW);
    pinMode(_rdSda, OUTPUT);
    // end() has to come first - ESP32's begin() does nothing when the bus is
    // already started. The pauses around it are pure caution; nothing about
    // read-back is speed-critical.
    delayMicroseconds(50);
    _spi->end();
    delayMicroseconds(50);
    _spi->begin();
    delayMicroseconds(50);
  }

  void endTransaction() override {
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    _spi->endTransaction();
  }

  /// For panels with a single shared data line (the ILI9342C on an M5Stack).
  ///
  /// WARNING: this is the one path that touches the bus lifecycle. Turning the
  /// line around needs SPI.end() and SPI.begin(), which re-establishes the bus
  /// on its default pins. Do not use it when another device shares the wires.
  /// Nothing happens unless you call this, so the normal path is unaffected.
  ///
  /// On some boards SDA doubles as MOSI and MISO, and the SPI peripheral's
  /// MISO pin is connected to nothing. A plain transfer() cannot read there -
  /// every bit comes back as 1 (measured). Given SCK and SDA, the line is
  /// turned around and clocked by hand for the duration of a read. Writing
  /// still goes through the peripheral, so nothing gets slower.
  ///
  /// For an M5Stack Core / BASIC: bus.setReadPins(18, 23);
  ///
  /// Limitation: every read re-establishes the line with SPI.end() and
  /// SPI.begin(), so this cannot be used together with SPI.begin(...) on
  /// non-default pins - they would revert to the defaults.
  ///
  /// EXPERIMENTAL - do not rely on this yet.
  ///
  /// A raw probe that bit-bangs the whole exchange, commands included, reads
  /// reliably: colours written come back as FC 00 00 / 00 FC 00 / 00 00 FC /
  /// FC FC FC (RGB666, one dummy byte). Going through this class does not
  /// reproduce that. Something about how the ESP32 GPIO matrix and SPI share
  /// the pin is still unresolved. Measurements are in docs/MANUAL_TEST.ja.md,
  /// under the read-back section.
  ///
  /// Read outside startWrite() / endWrite(). Read-back takes the line
  /// independently of the drawing transaction.
  ///
  /// Read-back exists for debugging and verification, not for normal drawing,
  /// so it is tuned entirely for certainty over speed.
  ///
  /// `settleUs` is the wait per clock edge. The default of 2us is roughly
  /// 125 kHz. Going faster misses bits: at the equivalent of 1us, 51 of 3,072
  /// pixels came back with a bit flipped.
  void setReadPins(int8_t sck, int8_t sda, uint8_t settleUs = 2) {
    _rdSck = sck;
    _rdSda = sda;
    _rdSettle = settleUs;
  }

  /// Send commands, then read back. CS is asserted and released in here.
  ///
  /// Switching between the peripheral and bit-banging part way through shifts
  /// the bits, so once read pins are configured the whole exchange is handled
  /// consistently (measured).
  void readSequence(const uint8_t* script, uint8_t scriptLen, uint8_t dummy, uint8_t* buf,
                    size_t len) override {
    for (size_t i = 0; i < len; ++i) buf[i] = 0;
    if (_rdSck < 0) return;  // this bus cannot read

    // Commands go out through the peripheral. On ESP32, pinMode(OUTPUT) does
    // not win the pin back from the GPIO matrix, so bit-banging produces no
    // waveform at all (measured: everything reads FF). pinMode(INPUT), on the
    // other hand, reliably stops the output - so only the receive half is
    // clocked by hand.
    beginTransaction();
    uint8_t i = 0;
    while (i < scriptLen) {
      const uint8_t cmd = script[i++];
      const uint8_t n = script[i++];
      writeCommand(cmd);
      if (n) writeData(&script[i], n);
      i = (uint8_t)(i + n);
    }
    _spi->endTransaction();

    digitalWrite(_rdSck, LOW);
    pinMode(_rdSck, OUTPUT);
    digitalWrite(_rdSck, LOW);
    pinMode(_rdSda, INPUT);  // hand the line to the panel
    _rdActive = true;
    for (uint8_t d = 0; d < dummy; ++d) bbRead();
    for (size_t k = 0; k < len; ++k) buf[k] = bbRead();
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    endRead();  // give the line back every time; batching it stops the reads working (measured)
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
  uint8_t bbRead() {
    uint8_t v = 0;
    for (uint8_t i = 0; i < 8; ++i) {  // mode 0: sample on the rising edge
      digitalWrite(_rdSck, HIGH);
      if (_rdSettle) delayMicroseconds(_rdSettle);
      v = (uint8_t)((v << 1) | (digitalRead(_rdSda) ? 1 : 0));
      digitalWrite(_rdSck, LOW);
      if (_rdSettle) delayMicroseconds(_rdSettle);
    }
    return v;
  }

  SPIClass* _spi;
  uint32_t _freq;
  int8_t _rdSck = -1;
  int8_t _rdSda = -1;
  uint8_t _rdSettle = 2;
  bool _rdActive = false;
  int8_t _dc;
  int8_t _cs;
};
