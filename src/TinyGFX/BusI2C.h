// TinyGFX - I2C bus (Arduino Wire)
//
// The SSD1306 style of "control byte + payload". The control bytes are
// configurable, so other controllers that follow the same convention
// (SH1106, SSD1327, ...) work too.
//
// How this differs from SPI:
//   - I2C starts and stops on every transfer, so beginTransaction and
//     endTransaction are empty here
//   - Wire has a bounded transmit buffer (32 bytes on AVR), so long payloads
//     are split into chunks
//   - writeColor and writePixels go unused; a monochrome panel only needs
//     writeData
#pragma once
#include <Arduino.h>
#include <Wire.h>

#include "Bus.h"

class TinyGFXBusI2C : public TinyGFXBus {
 public:
  /// `chunk` is how many payload bytes go into one transmission.
  /// AVR's Wire buffer is 32 bytes in total, so the default is kept modest.
  TinyGFXBusI2C(uint8_t address = 0x3C, uint8_t chunk = 16, TwoWire& wire = Wire,
                bool initWire = true)
      : _wire(&wire), _addr(address), _chunk(chunk), _initWire(initWire) {}

  /// Control bytes. Defaults are the SSD1306 family (command 0x00, data 0x40).
  void setControlBytes(uint8_t cmd, uint8_t data) { _cmdCtrl = cmd; _dataCtrl = data; }

  void init() override {
    if (_initWire) _wire->begin();
  }

  void writeCommand(uint8_t cmd) override {
    _wire->beginTransmission(_addr);
    _wire->write(_cmdCtrl);
    _wire->write(cmd);
    _wire->endTransmission();
  }

  void writeData(const uint8_t* data, size_t len) override {
    while (len != 0) {
      const uint8_t take = (len < _chunk) ? (uint8_t)len : _chunk;
      _wire->beginTransmission(_addr);
      _wire->write(_dataCtrl);
      for (uint8_t i = 0; i < take; ++i) _wire->write(data[i]);
      _wire->endTransmission();
      data += take;
      len -= take;
    }
  }

 private:
  TwoWire* _wire;
  uint8_t _addr;
  uint8_t _chunk;
  uint8_t _cmdCtrl = 0x00;
  uint8_t _dataCtrl = 0x40;
  bool _initWire;
};
