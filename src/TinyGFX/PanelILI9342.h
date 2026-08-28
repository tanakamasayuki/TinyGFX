// TinyGFX - ILI9342C panel (M5Stack Core / BASIC など)
//
// ILI9341 の兄弟だが、**GRAM が最初から横長（320x240）**。
// オフセットのあるモジュールが無いので setOffset / setGramSize は持たない。
//
// ST7789 との実質的な差は次の 3 点だけ。どれも実機でしか合っているか
// 分からないので、**1 行で直せる**ようにしてある。
//   1. 色順が BGR（既定）           -> setRgbOrder(false) で RGB
//   2. 反転が要る（既定 INVON）      -> invertDisplay(false)
//   3. ガラスの貼り付き向き          -> setMirror(mx, my)
//
// ガンマ・電源の長い初期化列は**わざと入れていない**。ILI934x は電源投入時の
// 既定値でちゃんと出る。色味を追い込みたくなったら足す。
#pragma once
#include <stdint.h>

#include "Panel.h"

class TinyGFXPanelILI9342 : public TinyGFXPanel {
 public:
  TinyGFXPanelILI9342(TinyGFXBus& bus, int16_t w = 320, int16_t h = 240, int8_t rst = -1)
      : _bus(&bus), _natW(w), _natH(h), _rst(rst) {
    _width = w;
    _height = h;
  }

  /// 色順。ILI9342C のモジュールはたいてい BGR なので既定は true。
  /// 赤と青が入れ替わって見えたら false にする。
  void setRgbOrder(bool bgr) { _bgr = bgr; }

  /// ガラスの貼り付き向き。全回転の MADCTL に XOR される。
  /// 絵が上下・左右にひっくり返って出たらここで直す。
  void setMirror(bool mirrorX, bool mirrorY) {
    _flip = (uint8_t)((mirrorX ? MADCTL_MX : 0) | (mirrorY ? MADCTL_MY : 0));
  }

  bool init() override;
  void setRotation(uint8_t r) override;
  void setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) override;
  void writeColor(uint16_t color, uint32_t count) override { _bus->writeColor(color, count); }
  void writePixels(const uint16_t* data, uint32_t count) override { _bus->writePixels(data, count); }
  void beginTransaction() override { _bus->beginTransaction(); }
  void endTransaction() override { _bus->endTransaction(); }

  // 仮想にしない（全員が払うほどではない。docs/DECISIONS.ja.md Q7）
  void invertDisplay(bool invert) { cmd(invert ? 0x21 : 0x20); }
  void setSleep(bool sleep) { cmd(sleep ? 0x10 : 0x11); }
  void displayOn(bool on) { cmd(on ? 0x29 : 0x28); }

 private:
  enum : uint8_t {
    MADCTL_MY = 0x80, MADCTL_MX = 0x40, MADCTL_MV = 0x20, MADCTL_BGR = 0x08,
  };

  void cmd(uint8_t c) {
    _bus->beginTransaction();
    _bus->writeCommand(c);
    _bus->endTransaction();
  }
  void cmdData(uint8_t c, const uint8_t* d, uint8_t n) {
    _bus->writeCommand(c);
    if (n) _bus->writeData(d, n);
  }

  TinyGFXBus* _bus;
  int16_t _natW, _natH;
  int8_t _rst;
  uint8_t _flip = 0;
  bool _bgr = true;
};

inline bool TinyGFXPanelILI9342::init() {
  _bus->init();
  if (_rst >= 0) {
#if defined(ARDUINO)
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, HIGH);
    delay(10);
    digitalWrite(_rst, LOW);
    delay(10);
    digitalWrite(_rst, HIGH);
    delay(120);
#endif
  }
  _bus->beginTransaction();
  cmdData(0x01, nullptr, 0);  // SWRESET
  _bus->endTransaction();
#if defined(ARDUINO)
  delay(150);
#endif
  _bus->beginTransaction();
  cmdData(0x11, nullptr, 0);  // SLPOUT
  _bus->endTransaction();
#if defined(ARDUINO)
  delay(120);
#endif
  _bus->beginTransaction();
  const uint8_t colmod = 0x55;  // 16bit/pixel
  cmdData(0x3A, &colmod, 1);
  cmdData(0x21, nullptr, 0);  // INVON（M5Stack の ILI9342C は反転が要る）
  cmdData(0x13, nullptr, 0);  // NORON
  _bus->endTransaction();
  setRotation(0);
  _bus->beginTransaction();
  cmdData(0x29, nullptr, 0);  // DISPON
  _bus->endTransaction();
  return true;
}

inline void TinyGFXPanelILI9342::setRotation(uint8_t r) {
  r = (uint8_t)(r & 3);
  // 表そのものは ST7789 と同じ（GRAM が横長なので回転 0 が 320x240 になる）。
  // ガラスの向きの差は _flip で吸収する。
  uint8_t madctl;
  switch (r) {
    case 0: madctl = 0; _width = _natW; _height = _natH; break;
    case 1: madctl = (uint8_t)(MADCTL_MV | MADCTL_MX); _width = _natH; _height = _natW; break;
    case 2: madctl = (uint8_t)(MADCTL_MX | MADCTL_MY); _width = _natW; _height = _natH; break;
    default: madctl = (uint8_t)(MADCTL_MV | MADCTL_MY); _width = _natH; _height = _natW; break;
  }
  madctl ^= _flip;
  if (_bgr) madctl |= MADCTL_BGR;
  _bus->beginTransaction();
  cmdData(0x36, &madctl, 1);
  _bus->endTransaction();
}

inline void TinyGFXPanelILI9342::setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
  uint8_t a[4];
  a[0] = (uint8_t)(xs >> 8); a[1] = (uint8_t)xs; a[2] = (uint8_t)(xe >> 8); a[3] = (uint8_t)xe;
  cmdData(0x2A, a, 4);  // CASET
  a[0] = (uint8_t)(ys >> 8); a[1] = (uint8_t)ys; a[2] = (uint8_t)(ye >> 8); a[3] = (uint8_t)ye;
  cmdData(0x2B, a, 4);  // RASET
  cmdData(0x2C, nullptr, 0);  // RAMWR
}
