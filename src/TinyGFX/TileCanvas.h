// TinyGFX - tiled virtual canvas (flicker-free drawing without a full framebuffer)
//
// Splits the screen into horizontal bands, draws one band at a time into a
// small RAM buffer, then pushes it. The drawing callback runs once per band,
// but always in whole-screen coordinates - the offset and the clip are hidden
// in here.
//
// The core was not touched to make this work. It is just PanelMemory, which
// implements TinyGFXPanel, slotted in front (docs/DECISIONS.ja.md D16).
// Not including this header links not one byte of it.
//
// RAM needed = screen width * band rows * 2 bytes, supplied by the caller.
#pragma once
#include <stdint.h>

#include "Gfx.h"
#include "Panel.h"
#include "PanelMemory.h"

class TinyGFXTileCanvas {
 public:
  /// The drawing callback. Runs once per band, in whole-screen coordinates.
  typedef void (*DrawFn)(TinyGFX& gfx, void* ctx);

  /// `buffer` holds bufferPixels pixels. The band height follows from the width.
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

  /// Rows in one band, or 0 when the buffer cannot even hold a single row.
  int16_t tileRows() const { return _rows; }

  /// Configure this surface (font, colours, ...) here; the settings persist
  /// across render() calls.
  TinyGFX& gfx() { return _gfx; }

  /// Draw one frame with any callable taking `(TinyGFX&)` - typically a
  /// lambda, which can capture what the scene needs instead of packing it
  /// into a struct behind a void*.
  ///
  ///     canvas.render([&](TinyGFX& g) { g.fillCircle(ball.x, ball.y, 16, c); });
  ///
  /// The band loop is not duplicated per lambda: this hands the callable to
  /// the function-pointer form through a one-line trampoline, so what a second
  /// call site costs is that trampoline, not another copy of the loop.
  template <class F>
  bool render(const F& draw) {
    return render(&callThrough<F>, const_cast<void*>(static_cast<const void*>(&draw)));
  }

  /// Draw one frame. `draw` runs once per band.
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
  template <class F>
  static void callThrough(TinyGFX& g, void* ctx) { (*(const F*)ctx)(g); }

  /// Band rows = bufPixels / width, worked out by subtraction because a
  /// divide instruction cannot be assumed.
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
  TinyGFXPanelMemory _mem;  // must be constructed before _gfx
  TinyGFX _gfx;
  uint32_t _bufPixels;
  int16_t _rows = 0;
  uint16_t _bg = 0;
  bool _autoClear = true;
};
