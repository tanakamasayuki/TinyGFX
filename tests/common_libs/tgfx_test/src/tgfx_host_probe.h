// ホストのバス観測口に載せる TinyGFX 側のパネル模型。
//
// コアは周辺機器を模型化しない。ST7789 のコマンド列を知っているのは
// こちら側なので、ピン／バイトを拾って TinyGFXBusCapture へ流し込む。
// これで「スケッチ → 描画コア → Panel → **本番の Bus** → 線 → 模型 → 画素」
// が通しで検証できる（docs/EXTERNAL_REQUESTS.ja.md E1 の受入条件）。
//
// 観測口の無い古いホストコアでは丸ごと無効になる。
#pragma once
#include <Arduino.h>

#if defined(HOST_ARDUINO_BUS_H)
#define TGFX_HOST_PROBE 1
#else
#define TGFX_HOST_PROBE 0
#endif

#if TGFX_HOST_PROBE

#include <TinyGFX/BusCapture.h>

/// ビットバン（TinyGFXBusSoftSPI）を GPIO の書き込みから組み立て直す。
/// SCK の立ち上がりで MOSI をシフトインし、8 bit たまったら DC を見て
/// コマンドかデータかを決める。
class TgfxPinProbe {
 public:
  TgfxPinProbe(TinyGFXBusCapture& sink, uint8_t sck, uint8_t mosi, uint8_t dc, int8_t cs = -1)
      : _sink(&sink), _sck(sck), _mosi(mosi), _dc(dc), _cs(cs) {}

  void attach() { HostArduino::setPinWriteHook(&TgfxPinProbe::onWrite, this); }
  void detach() { HostArduino::setPinWriteHook(nullptr); }

  uint32_t byteCount() const { return _bytes; }
  uint32_t edgeCount() const { return _edges; }
  void resetCounters() { _bytes = 0; _edges = 0; }

 private:
  static void onWrite(uint8_t pin, uint8_t value, void* user) {
    TgfxPinProbe* p = static_cast<TgfxPinProbe*>(user);
    if (pin != p->_sck || value == 0) return;  // 立ち上がりだけ見る
    if (p->_cs >= 0 && digitalRead(p->_cs) != LOW) return;  // CS が上がっている間は無視
    ++p->_edges;
    p->_acc = (uint8_t)((p->_acc << 1) | (digitalRead(p->_mosi) ? 1 : 0));
    if (++p->_bits < 8) return;
    const uint8_t b = p->_acc;
    p->_bits = 0;
    p->_acc = 0;
    ++p->_bytes;
    if (digitalRead(p->_dc) == LOW) p->_sink->writeCommand(b);
    else p->_sink->writeData(&b, 1);
  }

  TinyGFXBusCapture* _sink;
  uint8_t _sck, _mosi, _dc;
  int8_t _cs;
  uint8_t _acc = 0, _bits = 0;
  uint32_t _bytes = 0, _edges = 0;
};

#if defined(TGFX_HOST_PROBE_SPI)
#include <SPI.h>

/// ハードウェア SPI（TinyGFXBusSPI）をバイト単位で拾う。
/// DC はピンの保持値から読む。
class TgfxSpiProbe {
 public:
  TgfxSpiProbe(TinyGFXBusCapture& sink, uint8_t dc) : _sink(&sink), _dc(dc) {}

  void attach() { SPI.setTransferHook(&TgfxSpiProbe::onByte, this); }
  void detach() { SPI.setTransferHook(nullptr); }

  uint32_t byteCount() const { return _bytes; }
  void resetCounters() { _bytes = 0; }

 private:
  static uint8_t onByte(uint8_t out, void* user) {
    TgfxSpiProbe* p = static_cast<TgfxSpiProbe*>(user);
    ++p->_bytes;
    if (digitalRead(p->_dc) == LOW) p->_sink->writeCommand(out);
    else p->_sink->writeData(&out, 1);
    return 0xFF;  // ディスプレイは書き込み専用。MISO は駆動しない
  }

  TinyGFXBusCapture* _sink;
  uint8_t _dc;
  uint32_t _bytes = 0;
};
#endif  // TGFX_HOST_PROBE_SPI

#endif  // TGFX_HOST_PROBE
