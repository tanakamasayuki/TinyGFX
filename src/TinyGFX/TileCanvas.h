// TinyGFX - tiled virtual canvas (flicker-free drawing without a full framebuffer)
//
// 画面を横帯に分け、小さな RAM バッファへ 1 帯ずつ描いてから転送する。
// 描画コールバックは帯の数だけ呼ばれるが、座標は常に画面全体のもの
// （オフセットとクリップはこちらで隠す）。
//
// コアには一切手を入れていない。TinyGFXPanel を実装した PanelMemory を
// 挟んでいるだけ（docs/DECISIONS.ja.md D16）。
// このヘッダを include しなければ 1 バイトもリンクされない。
//
// 必要 RAM = 画面幅 × 帯の行数 × 2 バイト。バッファは利用者が用意する。
#pragma once
#include <stdint.h>

#include "Gfx.h"
#include "Panel.h"
#include "PanelMemory.h"

class TinyGFXTileCanvas {
 public:
  /// 描画コールバック。帯ごとに呼ばれる。座標は画面全体のもの。
  typedef void (*DrawFn)(TinyGFX& gfx, void* ctx);

  /// buffer は bufferPixels 画素ぶん。帯の行数は幅から自動で決まる。
  TinyGFXTileCanvas(TinyGFXPanel& target, uint16_t* buffer, uint32_t bufferPixels)
      : _target(&target),
        _mem(buffer, target.width(), target.height()),
        _gfx(_mem),
        _bufPixels(bufferPixels) {}

  bool begin() {
    if (!_target->init()) return false;
    _mem.init();
    return recalc();
  }

  void setRotation(uint8_t r) {
    _target->setRotation(r);
    _mem.setRotation(r);
    _gfx.resetClipRect();
    recalc();
  }

  void setBackgroundColor(uint16_t color) { _bg = color; }
  void setAutoClear(bool on) { _autoClear = on; }

  /// 帯 1 本の行数。バッファが 1 行ぶんも無ければ 0。
  int16_t tileRows() const { return _rows; }

  /// この描画面の設定（フォント・色など）はここで行う。render() 間で保持される。
  TinyGFX& gfx() { return _gfx; }

  /// 1 フレーム描く。draw は帯の数だけ呼ばれる。
  bool render(DrawFn draw, void* ctx = nullptr) {
    if (_rows <= 0 || draw == nullptr) return false;
    const int16_t w = _target->width();
    const int16_t h = _target->height();
    _target->beginTransaction();
    for (int16_t y = 0; y < h; y = (int16_t)(y + _rows)) {
      int16_t rows = _rows;
      if ((int16_t)(y + rows) > h) rows = (int16_t)(h - y);
      _mem.setBufferRegion(y, rows);
      if (_autoClear) _mem.fillBuffer(_bg);
      _gfx.setClipRect(0, y, w, rows);
      draw(_gfx, ctx);
      _target->setWindow(0, (uint16_t)y, (uint16_t)(w - 1), (uint16_t)(y + rows - 1));
      _target->writePixels(_mem.buffer(), (uint32_t)(uint16_t)w * (uint32_t)(uint16_t)rows);
    }
    _target->endTransaction();
    _gfx.resetClipRect();
    return true;
  }

 private:
  /// 帯の行数 = bufPixels / width。除算命令が無い前提なので引き算で求める。
  bool recalc() {
    const int16_t w = _target->width();
    const int16_t h = _target->height();
    _rows = 0;
    if (w <= 0) return false;
    uint32_t left = _bufPixels;
    const uint32_t step = (uint32_t)(uint16_t)w;
    while (left >= step && _rows < h) {
      left -= step;
      ++_rows;
    }
    return _rows > 0;
  }

  TinyGFXPanel* _target;
  TinyGFXPanelMemory _mem;  // _gfx より先に構築されること
  TinyGFX _gfx;
  uint32_t _bufPixels;
  int16_t _rows = 0;
  uint16_t _bg = 0;
  bool _autoClear = true;
};
