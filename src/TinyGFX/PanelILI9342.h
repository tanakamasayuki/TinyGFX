// TinyGFX - ILI9342C panel (M5Stack Core / BASIC など)
//
// ILI9341 の兄弟だが、**GRAM が最初から横長（320x240）**。
// オフセットのあるモジュールが無いので setOffset / setGramSize は持たない。
//
// ST7789 との実質的な差は次の 3 点だけ。どれも実機でしか合っているか
// 分からないので、**1 行で直せる**ようにしてある。
//   1. 色順が BGR（既定）           -> setRgbOrder(false) で RGB
//   2. 反転が要る（既定 INVON）      -> invertDisplay(false)（**begin() の後で**）
//   3. ガラスの貼り付き向き          -> setMirror(mx, my)
//
// ガンマ・電源の長い初期化列は**わざと入れていない**。ILI934x は電源投入時の
// 既定値でちゃんと出る。色味を追い込みたくなったら足す。
#pragma once
#include <stdint.h>

// 1 度の読み出しで扱う画素数。**大きいほど速い**（線の張り替えが減る）。
// スタックを 3 バイト/画素 使う。既定 64 で 192 バイト。
#ifndef TINYGFX_READ_CHUNK
#define TINYGFX_READ_CHUNK 64
#endif

#include "Color.h"
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
  //
  // **どれも init()（= lcd.begin()）の後に呼ぶこと。** 前に呼ぶとバスがまだ
  // 初期化されておらず、通っても init() の初期化列に上書きされる。
  void invertDisplay(bool invert) { cmd(invert ? 0x21 : 0x20); }
  void setSleep(bool sleep) { cmd(sleep ? 0x10 : 0x11); }
  void displayOn(bool on) { cmd(on ? 0x29 : 0x28); }

  // ---- 読み戻し ---------------------------------------------------------
  //
  // **呼ばなければ 1 バイトも載らない。** inline なメンバ関数は、どこからも
  // 呼ばれなければコードが生成されない（リンカが落とすのではなく、そもそも出ない）。
  // バス側の `readData` を足す代金だけは全員が払うが、実測 +8 バイト。
  //
  // 読めるのは `TinyGFXBusSPI` だけ。ソフト SPI は MISO の線を持たないので
  // 何もせず返る（バッファは 0 のまま）。

  /// コントローラの ID を読む。`out` に 4 バイト（先頭 1 バイトはダミー）。
  /// ILI9341 系なら `00 00 93 41` のような並びが返る。
  /// **全部 00 か全部 FF なら MISO が繋がっていない。**
  void readId(uint8_t* out3) { readRegister(0x04, out3, 3); }

  /// 任意のレジスタを読む。ダミー 1 バイトは読み飛ばす。
  void readRegister(uint8_t reg, uint8_t* out, uint8_t n) {
    const uint8_t script[2] = {reg, 0};
    _bus->readSequence(script, 2, 1, out, n);
  }

  /// `0xD3` RDID4。ILI9341 は `00 93 41`。
  void readId4(uint8_t* out4) { readRegister(0xD3, out4, 4); }

  /// GRAM を読み戻す。戻り値は読んだ画素数。
  ///
  /// **16bpp で書いても読み出しは 1 画素 3 バイト**（RGB666 が各バイトの上位に入る）。
  /// 先頭にダミーが 1 バイト入るのも ILI934x の作法。ここで両方吸収する。
  ///
  /// 反転（INVON / INVOFF）は表示側の処理なので **GRAM の中身には出ない。**
  /// 読み戻しで反転の有無は判定できない。
  uint32_t readPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* out);



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

inline uint32_t TinyGFXPanelILI9342::readPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                                uint16_t* out) {
  if (w == 0 || h == 0 || out == nullptr) return 0;
  const uint16_t xe = (uint16_t)(x + w - 1), ye = (uint16_t)(y + h - 1);
  // {CASET,4,args, RASET,4,args, RAMRD,0} を 1 本の手順として渡す。
  // **途中で周辺機と手叩きを切り替えないため**（切り替えるとビットがずれる）。
  const uint8_t script[16] = {
      0x2A, 4, (uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(xe >> 8), (uint8_t)xe,
      0x2B, 4, (uint8_t)(y >> 8), (uint8_t)y, (uint8_t)(ye >> 8), (uint8_t)ye,
      0x2E, 0, 0, 0,
  };
  uint8_t buf[3 * TINYGFX_READ_CHUNK];
  uint32_t left = (uint32_t)w * (uint32_t)h;
  const uint32_t total = left;
  // 1 度に読み切る。分けると切り替えが増えて不安定になる
  const uint16_t k = (left > TINYGFX_READ_CHUNK) ? TINYGFX_READ_CHUNK : (uint16_t)left;
  _bus->readSequence(script, 14, 1, buf, (size_t)k * 3);
  uint32_t i = 0;
  for (uint16_t j = 0; j < k; ++j) {
    out[i++] = tinygfx_color565(buf[j * 3], buf[j * 3 + 1], buf[j * 3 + 2]);
  }
  left -= k;
  // 残りは窓をずらして読み直す（連続読み出しの途中で線を戻すと続きが取れない）
  while (left != 0) {
    const uint32_t doneRows = i / w;
    const uint16_t ry = (uint16_t)(y + doneRows);
    const uint16_t rx = (uint16_t)(x + (i - doneRows * w));
    uint32_t n2 = (uint32_t)(w - (rx - x));
    if (n2 > TINYGFX_READ_CHUNK) n2 = TINYGFX_READ_CHUNK;
    if (n2 > left) n2 = left;
    const uint16_t rxe = (uint16_t)(rx + n2 - 1);
    const uint8_t s2[16] = {
        0x2A, 4, (uint8_t)(rx >> 8), (uint8_t)rx, (uint8_t)(rxe >> 8), (uint8_t)rxe,
        0x2B, 4, (uint8_t)(ry >> 8), (uint8_t)ry, (uint8_t)(ye >> 8), (uint8_t)ye,
        0x2E, 0, 0, 0,
    };
    _bus->readSequence(s2, 14, 1, buf, (size_t)n2 * 3);
    for (uint16_t j = 0; j < (uint16_t)n2; ++j) {
      out[i++] = tinygfx_color565(buf[j * 3], buf[j * 3 + 1], buf[j * 3 + 2]);
    }
    left -= n2;
  }
  return total;
}

inline void TinyGFXPanelILI9342::setWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
  uint8_t a[4];
  a[0] = (uint8_t)(xs >> 8); a[1] = (uint8_t)xs; a[2] = (uint8_t)(xe >> 8); a[3] = (uint8_t)xe;
  cmdData(0x2A, a, 4);  // CASET
  a[0] = (uint8_t)(ys >> 8); a[1] = (uint8_t)ys; a[2] = (uint8_t)(ye >> 8); a[3] = (uint8_t)ye;
  cmdData(0x2B, a, 4);  // RASET
  cmdData(0x2C, nullptr, 0);  // RAMWR
}
