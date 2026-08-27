// TinyGFX - panel that renders into a caller-supplied RAM buffer
//
// 二役ある:
//   1) ホストテストの検証先（描いた結果を画素で直接見る）
//   2) TileCanvas の帯バッファ（docs/DECISIONS.ja.md D16）
// バッファは利用者が用意する。このクラスは動的確保をしない。
#pragma once
#include <stdint.h>

#include "Panel.h"

class TinyGFXPanelMemory : public TinyGFXPanel {
 public:
  /// buffer は「現在の幅 × rows」画素ぶん必要。rows の既定は h（全画面）。
  TinyGFXPanelMemory(uint16_t* buffer, int16_t w, int16_t h)
      : _buf(buffer), _natW(w), _natH(h), _bufY0(0), _bufRows(h) {
    _width = w;
    _height = h;
  }

  bool init() override { return _buf != nullptr; }

  void setRotation(uint8_t r) override {
    if (r & 1) { _width = _natH; _height = _natW; }
    else       { _width = _natW; _height = _natH; }
    _bufY0 = 0;
    _bufRows = _height;
  }

  /// バッファが受け持つ行範囲を指定する（帯レンダリング用）。
  void setBufferRegion(int16_t y0, int16_t rows) {
    _bufY0 = y0;
    _bufRows = rows;
  }
  int16_t bufferY0() const { return _bufY0; }
  int16_t bufferRows() const { return _bufRows; }
  const uint16_t* buffer() const { return _buf; }
  uint16_t* buffer() { return _buf; }

  /// バッファ全体を単色で埋める。
  void fillBuffer(uint16_t color) {
    const uint32_t n = (uint32_t)(uint16_t)_width * (uint32_t)(uint16_t)_bufRows;
    for (uint32_t i = 0; i < n; ++i) _buf[i] = color;
  }

  /// 論理座標の 1 画素を読む。範囲外・バッファ外は 0。
  uint16_t readPixel(int16_t x, int16_t y) const {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return 0;
    const int16_t by = (int16_t)(y - _bufY0);
    if (by < 0 || by >= _bufRows) return 0;
    return _buf[(uint32_t)(uint16_t)by * (uint32_t)(uint16_t)_width + (uint16_t)x];
  }

  void setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) override {
    _xs = xs; _ys = ys; _xe = xe; _ye = ye;
    _cx = xs; _cy = ys;
  }

  void writeColor(uint16_t color, uint32_t count) override {
    while (count--) put(color);
  }

  void writePixels(const uint16_t* data, uint32_t count) override {
    while (count--) put(*data++);
  }

 private:
  void put(uint16_t color) {
    const int16_t by = (int16_t)((int16_t)_cy - _bufY0);
    if (by >= 0 && by < _bufRows && (int16_t)_cx < _width && (int16_t)_cy < _height) {
      _buf[(uint32_t)(uint16_t)by * (uint32_t)(uint16_t)_width + _cx] = color;
    }
    if (_cx >= _xe) { _cx = _xs; ++_cy; }
    else { ++_cx; }
  }

  uint16_t* _buf;
  int16_t _natW, _natH;
  int16_t _bufY0, _bufRows;
  uint16_t _xs = 0, _ys = 0, _xe = 0, _ye = 0;
  uint16_t _cx = 0, _cy = 0;
};
