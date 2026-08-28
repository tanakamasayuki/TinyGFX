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
  /// コマンドを送ってから読み戻す。**読めないバスでは何もしない**（buf は触らない）。
  ///
  /// `script` は `{コマンド, 引数の数, 引数...}` を並べたもの。最後のコマンドを
  /// 送り終えたところから、ダミーを `dummy` バイト読み飛ばして `len` バイト読む。
  ///
  /// **送信と受信を 1 本の仮想メソッドにまとめてある。** データ線が 1 本の
  /// パネル（M5Stack の ILI9342C など）では、コマンドの送出も含めて全部を
  /// 手で叩かないと安定しない（周辺機と手叩きを途中で切り替えるとビットがずれる。
  /// 実測で確認）。分けて置くとこれが書けない。
  virtual void readSequence(const uint8_t* script, uint8_t scriptLen, uint8_t dummy,
                            uint8_t* buf, size_t len) {
    (void)script; (void)scriptLen; (void)dummy; (void)buf; (void)len;
  }

 protected:
  ~TinyGFXBus() = default;  // 非仮想・protected。トリビアルなまま保つ
};
