// TinyGFX - bus that decodes the outgoing command stream back into pixels
//
// Interprets the byte stream a panel emits (CASET / RASET / RAMWR) and paints
// it back into a virtual GRAM. Host tests use it to check the bytes that are
// actually being sent. The core never references it (docs/CORE_DESIGN.ja.md
// 7.4, rule R3).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "Bus.h"

class TinyGFXBusCapture : public TinyGFXBus {
 public:
  enum : uint8_t { CMD_CASET = 0x2A, CMD_RASET = 0x2B, CMD_RAMWR = 0x2C };

  /// `gram` holds w*h pixels.
  TinyGFXBusCapture(uint16_t* gram, uint16_t w, uint16_t h) : _gram(gram), _w(w), _h(h) {}

  void init() override { _initCalls++; }
  void beginTransaction() override { _txnDepth++; _beginCalls++; }
  void endTransaction() override { if (_txnDepth) _txnDepth--; _endCalls++; }

  void writeCommand(uint8_t cmd) override {
    _lastCmd = cmd;
    _cmdCount++;
    _argLen = 0;
    if (cmd == CMD_RAMWR) { _cx = _xs; _cy = _ys; _inRamwr = true; }
    else { _inRamwr = false; }
  }

  void writeData(const uint8_t* data, size_t len) override {
    for (size_t i = 0; i < len; ++i) feedArg(data[i]);
  }

  void writeColor(uint16_t color, uint32_t count) override {
    _pixelCount += count;
    while (count--) put(color);
  }

  void writePixels(const uint16_t* data, uint32_t count) override {
    _pixelCount += count;
    while (count--) put(*data++);
  }

  // ---- for tests -------------------------------------------------------
  const uint16_t* gram() const { return _gram; }
  uint16_t pixel(uint16_t x, uint16_t y) const {
    if (x >= _w || y >= _h) return 0;
    return _gram[(uint32_t)y * _w + x];
  }
  void fill(uint16_t color) {
    const uint32_t n = (uint32_t)_w * _h;
    for (uint32_t i = 0; i < n; ++i) _gram[i] = color;
  }
  void resetCounters() { _cmdCount = 0; _pixelCount = 0; _beginCalls = 0; _endCalls = 0; }
  uint32_t commandCount() const { return _cmdCount; }
  uint32_t pixelCount() const { return _pixelCount; }
  uint32_t beginCalls() const { return _beginCalls; }
  uint32_t endCalls() const { return _endCalls; }
  uint8_t txnDepth() const { return _txnDepth; }
  uint8_t lastCommand() const { return _lastCmd; }
  /// First byte after the most recent command; handy for one-argument
  /// commands such as MADCTL.
  uint8_t lastCommandArg() const { return _lastArg0; }
  uint16_t windowXs() const { return _xs; }
  uint16_t windowYs() const { return _ys; }
  uint16_t windowXe() const { return _xe; }
  uint16_t windowYe() const { return _ye; }

 private:
  void feedArg(uint8_t b) {
    if (_inRamwr) {  // after RAMWR the data is pixels, big endian
      if (_argLen == 0) { _hi = b; _argLen = 1; }
      else { put((uint16_t)(((uint16_t)_hi << 8) | b)); _argLen = 0; _pixelCount++; }
      return;
    }
    if (_argLen == 0) _lastArg0 = b;
    if (_argLen < 4) _args[_argLen] = b;
    _argLen++;
    if (_argLen == 4) {
      const uint16_t a = (uint16_t)(((uint16_t)_args[0] << 8) | _args[1]);
      const uint16_t b2 = (uint16_t)(((uint16_t)_args[2] << 8) | _args[3]);
      if (_lastCmd == CMD_CASET) { _xs = a; _xe = b2; }
      else if (_lastCmd == CMD_RASET) { _ys = a; _ye = b2; }
      _argLen = 0;
    }
  }

  void put(uint16_t color) {
    if (_cx < _w && _cy < _h) _gram[(uint32_t)_cy * _w + _cx] = color;
    if (_cx >= _xe) { _cx = _xs; ++_cy; }
    else { ++_cx; }
  }

  uint16_t* _gram;
  uint16_t _w;
  uint16_t _h;
  uint16_t _xs = 0;
  uint16_t _ys = 0;
  uint16_t _xe = 0;
  uint16_t _ye = 0;
  uint16_t _cx = 0;
  uint16_t _cy = 0;
  uint8_t _args[4] = {0, 0, 0, 0};
  uint8_t _argLen = 0;
  uint8_t _hi = 0;
  uint8_t _lastCmd = 0;
  uint8_t _lastArg0 = 0;
  uint8_t _txnDepth = 0;
  bool _inRamwr = false;
  uint32_t _cmdCount = 0;
  uint32_t _pixelCount = 0;
  uint32_t _beginCalls = 0;
  uint32_t _endCalls = 0;
  uint32_t _initCalls = 0;
};
