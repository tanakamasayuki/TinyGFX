// TinyGFX - bus interface
#pragma once
#include <stddef.h>
#include <stdint.h>

/// The transport layer: how bytes reach the panel, plus CS and DC.
///
/// None of these are pure virtual, so that __cxa_pure_virtual never gets
/// pulled in. There is no virtual destructor either - that would cost a vtable
/// slot and drag in operator delete.
class TinyGFXBus {
 public:
  virtual void init() {}
  virtual void beginTransaction() {}                            // CS low
  virtual void endTransaction() {}                              // CS high
  virtual void writeCommand(uint8_t cmd) { (void)cmd; }         // DC low
  virtual void writeData(const uint8_t* data, size_t len) { (void)data; (void)len; }  // DC high
  virtual void writeColor(uint16_t color, uint32_t count) { (void)color; (void)count; }
  virtual void writePixels(const uint16_t* data, uint32_t count) { (void)data; (void)count; }

  /// Send commands, then read back. A bus that cannot read leaves `buf` alone.
  ///
  /// `script` is a sequence of `{command, argument count, arguments...}`.
  /// After the last command has gone out, `dummy` bytes are discarded and then
  /// `len` bytes are read.
  ///
  /// Send and receive are deliberately one method. On panels with a single
  /// shared data line (the ILI9342C on an M5Stack, for instance) the whole
  /// exchange has to be driven the same way; switching between the SPI
  /// peripheral and bit-banging part way through shifts the bits (measured).
  /// Splitting this in two would make that impossible to express.
  virtual void readSequence(const uint8_t* script, uint8_t scriptLen, uint8_t dummy,
                            uint8_t* buf, size_t len) {
    (void)script; (void)scriptLen; (void)dummy; (void)buf; (void)len;
  }

 protected:
  ~TinyGFXBus() = default;  // non-virtual and protected: keep this type trivial
};
