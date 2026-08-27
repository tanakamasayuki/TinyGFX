// TinyGFX - panel interface
#pragma once
#include <stdint.h>

#include "Bus.h"

/// LCD コントローラ。初期化列・MADCTL・原点オフセット・ウィンドウを持つ。
///
/// 仮想メソッドは 7 本だけ。invertDisplay / sleep のような「あると便利だが
/// 全員が払うほどではない」ものは具象パネル側の非仮想メソッドに置く。
class TinyGFXPanel {
 public:
  virtual bool init() { return false; }
  virtual void setRotation(uint8_t r) { (void)r; }
  virtual void setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
    (void)xs; (void)ys; (void)xe; (void)ye;
  }
  virtual void writeColor(uint16_t color, uint32_t count) { (void)color; (void)count; }
  virtual void writePixels(const uint16_t* data, uint32_t count) { (void)data; (void)count; }
  virtual void beginTransaction() {}
  virtual void endTransaction() {}

  int16_t width() const { return _width; }    // 回転後
  int16_t height() const { return _height; }  // 回転後

 protected:
  ~TinyGFXPanel() = default;

  int16_t _width = 0;
  int16_t _height = 0;
};
