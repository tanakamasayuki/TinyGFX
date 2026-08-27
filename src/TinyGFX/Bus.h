// TinyGFX - bus interface
#pragma once
#include <stddef.h>
#include <stdint.h>

/// 転送層。SPI の叩き方と CS / DC を持つ。
///
/// 純粋仮想にしていないのは __cxa_pure_virtual を持ち込まないため。
/// 仮想デストラクタも置かない（vtable のスロットと operator delete を避ける）。
class TinyGFXBus {
 public:
  virtual void init() {}
  virtual void beginTransaction() {}                            // CS Low
  virtual void endTransaction() {}                              // CS High
  virtual void writeCommand(uint8_t cmd) { (void)cmd; }         // DC Low
  virtual void writeData(const uint8_t* data, size_t len) { (void)data; (void)len; }  // DC High
  virtual void writeColor(uint16_t color, uint32_t count) { (void)color; (void)count; }
  virtual void writePixels(const uint16_t* data, uint32_t count) { (void)data; (void)count; }

 protected:
  ~TinyGFXBus() = default;  // 非仮想・protected。トリビアルなまま保つ
};
