// TinyGFX - panel interface
#pragma once
#include <stdint.h>

#include "Bus.h"

/// An LCD controller: init sequence, MADCTL, origin offset and address window.
///
/// Only seven virtual methods. Things that are handy but not worth charging
/// everyone for - invertDisplay, sleep and friends - live as non-virtual
/// methods on the concrete panels instead.
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

  int16_t width() const { return _width; }    // after rotation
  int16_t height() const { return _height; }  // after rotation

 protected:
  ~TinyGFXPanel() = default;

  int16_t _width = 0;
  int16_t _height = 0;
};
