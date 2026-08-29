// A model of the panel, sitting on top of the host core's bus probe.
//
// The core does not model peripherals. Knowing the ST7789 command stream is
// this side's job, so the pins and bytes are picked up here and fed into
// TinyGFXBusCapture. That makes the whole run checkable end to end:
// sketch -> drawing core -> Panel -> **the real Bus** -> the wire -> the model
// -> pixels (the acceptance criterion of docs/EXTERNAL_REQUESTS.ja.md E1).
//
// On an older host core with no probe, all of this compiles away.
#pragma once
#include <Arduino.h>

#if defined(HOST_ARDUINO_BUS_H)
#define TGFX_HOST_PROBE 1
#else
#define TGFX_HOST_PROBE 0
#endif

#if TGFX_HOST_PROBE

#include <TinyGFX/BusCapture.h>

/// Reassembles bit-banging (TinyGFXBusSoftSPI) from the GPIO writes.
/// MOSI shifts in on the rising edge of SCK, and once 8 bits have gathered, DC
/// says whether they were a command or data.
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
    if (pin != p->_sck || value == 0) return;  // rising edges only
    if (p->_cs >= 0 && digitalRead(p->_cs) != LOW) return;  // ignored while CS is high
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

/// Picks up hardware SPI (TinyGFXBusSPI) a byte at a time.
/// DC is read from what the pin is holding.
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
    return 0xFF;  // the display is write-only and never drives MISO
  }

  TinyGFXBusCapture* _sink;
  uint8_t _dc;
  uint32_t _bytes = 0;
};
#endif  // TGFX_HOST_PROBE_SPI

#endif  // TGFX_HOST_PROBE
