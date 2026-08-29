// TinyGFX - bit-banged I2C
//
// **Why this exists when TinyGFXBusI2C already works.**
//
// Not for size - Wire is already in the CH32V003's core and using it costs
// 40 bytes (measured), which this cannot beat. It exists for **pins**.
// Hardware I2C sits on two fixed pins, and on a part with as few as the
// CH32V003 has, those two are often wanted for something else. This one runs
// on any pair of GPIOs, slowly, and that is the whole point.
//
// It is also the answer when a board has one I2C peripheral and you need a
// second bus.
//
// **External pull-ups are required**, as on any I2C bus. The lines are driven
// the open-drain way: LOW is an output driven low, HIGH is the pin let go
// (INPUT) and the pull-up doing the work. Driving a line high would fight
// another device holding it low.
//
// Same "control byte + payload" convention as TinyGFXBusI2C, so panels do not
// know which one they are talking to.
//
// Clock stretching is honoured with a bounded wait: a device that never lets
// SCL go gives up rather than hanging the sketch.
#pragma once
#include <Arduino.h>

#include "Bus.h"

/// How long to wait for a device that is stretching the clock, in loops.
/// A display has no reason to stretch; this is only here so a wiring fault
/// cannot hang the sketch.
#ifndef TINYGFX_SOFTI2C_STRETCH
#define TINYGFX_SOFTI2C_STRETCH 2000
#endif

class TinyGFXBusSoftI2C : public TinyGFXBus {
 public:
  /// Any two GPIOs. **Both need a pull-up to VCC** (4.7k is the usual value).
  TinyGFXBusSoftI2C(int8_t sda, int8_t scl, uint8_t address = 0x3C)
      : _sda(sda), _scl(scl), _addr(address) {}

  /// Control bytes. Defaults are the SSD1306 family (command 0x00, data 0x40).
  void setControlBytes(uint8_t cmd, uint8_t data) { _cmdCtrl = cmd; _dataCtrl = data; }

  /// Both lines released. Nothing is driven until a transfer starts.
  void init() override {
    release(_sda);
    release(_scl);
  }

  void writeCommand(uint8_t cmd) override {
    start();
    writeByte((uint8_t)(_addr << 1));  // address + write
    writeByte(_cmdCtrl);
    writeByte(cmd);
    stop();
  }

  void writeData(const uint8_t* data, size_t len) override {
    if (len == 0) return;
    // No buffer to run out of, unlike Wire - the whole payload goes in one
    // transfer.
    start();
    writeByte((uint8_t)(_addr << 1));
    writeByte(_dataCtrl);
    while (len--) writeByte(*data++);
    stop();
  }

  // I2C starts and stops on every transfer, so there is nothing to hold open.
  void beginTransaction() override {}
  void endTransaction() override {}

 private:
  // Open drain: pull low by driving, release by letting the pull-up do it.
  void drive(int8_t pin) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  void release(int8_t pin) { pinMode(pin, INPUT); }

  /// Let SCL rise, then wait for it to actually be high. A device may be
  /// holding it down (clock stretching); give up rather than hang.
  void clockHigh() {
    release(_scl);
    for (uint16_t i = 0; i < TINYGFX_SOFTI2C_STRETCH; ++i) {
      if (digitalRead(_scl) != LOW) return;
    }
  }

  void start() {
    release(_sda);
    clockHigh();
    drive(_sda);   // SDA falls while SCL is high
    drive(_scl);
  }

  void stop() {
    drive(_sda);
    clockHigh();
    release(_sda);  // SDA rises while SCL is high
  }

  /// Send eight bits, MSB first, then read the acknowledge bit.
  ///
  /// The acknowledge is clocked but **not acted on**: a display is write-only
  /// and there is nothing useful to do about a NAK here. Skipping the clock
  /// altogether would leave the bus out of step, so it is still sent.
  void writeByte(uint8_t v) {
    for (uint8_t i = 0; i < 8; ++i) {
      if (v & 0x80) release(_sda);
      else          drive(_sda);
      clockHigh();
      drive(_scl);
      v = (uint8_t)(v << 1);
    }
    release(_sda);   // let the device pull it down to acknowledge
    clockHigh();
    drive(_scl);
  }

  int8_t _sda, _scl;
  uint8_t _addr;
  uint8_t _cmdCtrl = 0x00;
  uint8_t _dataCtrl = 0x40;
};
