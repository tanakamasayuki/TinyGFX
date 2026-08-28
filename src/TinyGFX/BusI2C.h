// TinyGFX - I2C バス（Arduino Wire）
//
// SSD1306 系の「制御バイト + ペイロード」形式。制御バイトは変えられるので、
// 同じ流儀の他のコントローラ（SH1106 / SSD1327 など）にも使える。
//
// SPI との違い:
//   - I2C は転送ごとに start/stop するので beginTransaction / endTransaction は空
//   - Wire の送信バッファに上限がある（AVR は 32 バイト）ので、長い転送は分割する
//   - writeColor / writePixels は使わない。モノクロパネルは writeData だけ使う
#pragma once
#include <Arduino.h>
#include <Wire.h>

#include "Bus.h"

class TinyGFXBusI2C : public TinyGFXBus {
 public:
  /// chunk は 1 回の送信に載せるペイロードのバイト数。
  /// AVR の Wire は全体で 32 バイトなので、既定は控えめにしてある。
  TinyGFXBusI2C(uint8_t address = 0x3C, uint8_t chunk = 16, TwoWire& wire = Wire,
                bool initWire = true)
      : _wire(&wire), _addr(address), _chunk(chunk), _initWire(initWire) {}

  /// 制御バイト。既定は SSD1306 系（コマンド 0x00 / データ 0x40）。
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
