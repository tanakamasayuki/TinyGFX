// TinyGFX - panel interface
#pragma once
#include <stdint.h>

#include "Bus.h"

/// An LCD controller: init sequence, MADCTL, origin offset and address window.
///
/// Eight virtual methods. Things that are handy but not worth charging
/// everyone for - invertDisplay, sleep and friends - live as non-virtual
/// methods on the concrete panels instead.
///
/// fillRect is the one seam a panel can take over. Every filled shape in the
/// core reaches the panel through it - rectangles, spans of circles and
/// triangles, and every run of every glyph - so a panel whose memory is laid
/// out differently from "a window you stream pixels into" gets to say so once,
/// here, instead of paying a translation on every pixel. The default keeps the
/// window protocol, so a panel that has nothing better to offer writes nothing.
class TinyGFXPanel {
 public:
  virtual bool init() { return false; }
  virtual void setRotation(uint8_t r) { (void)r; }
  virtual void setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
    (void)xs; (void)ys; (void)xe; (void)ye;
  }
  virtual void writeColor(uint16_t color, uint32_t count) { (void)color; (void)count; }
  virtual void writePixels(const uint16_t* data, uint32_t count) { (void)data; (void)count; }

  /// Fill a rectangle, already clipped to the panel. Override to bypass the
  /// address window; see the note above the class.
  virtual void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    setWindow(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
    writeColor(color, (uint32_t)w * (uint32_t)h);
  }

  virtual void beginTransaction() {}
  virtual void endTransaction() {}

  int16_t width() const { return _width; }    // after rotation
  int16_t height() const { return _height; }  // after rotation

 protected:
  ~TinyGFXPanel() = default;

  int16_t _width = 0;
  int16_t _height = 0;
};
