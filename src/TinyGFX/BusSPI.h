// TinyGFX - Arduino SPI bus (default, portable)
//
// SCK / MOSI は Arduino Core の SPI に任せる。ピンを指定したい環境では
// lcd.begin() より前に自分で SPI.begin(...) を呼び、initSpi=false にする。
#pragma once
#include <Arduino.h>
#include <SPI.h>

#include "Bus.h"

// まとめ書きの単位（画素）。0 で無効。
//
// 有効にすると `writeColor` / `writePixels` が **Arduino 標準の
// `SPI.transfer(buf, len)`（ブロック転送）**を使う。1 バイトずつ `transfer()` を
// 呼ぶより、コアがまとめて流せるぶん速い。RAM は「単位 x 2 バイト」の**スタック**だけ。
//
// **効くのはブロック転送を持つコアだけ。** 持たないコアでは同じか少し遅くなる。
// 32 なら 64 バイト。CH32V003（RAM 2KB）では 8〜16 くらいが上限。
#ifndef TINYGFX_FILL_CHUNK
#define TINYGFX_FILL_CHUNK 0
#endif

class TinyGFXBusSPI : public TinyGFXBus {
 public:
  TinyGFXBusSPI(int8_t dc, int8_t cs = -1, uint32_t freq = 24000000UL, SPIClass& spi = SPI,
                bool initSpi = true)
      : _spi(&spi), _freq(freq), _dc(dc), _cs(cs), _initSpi(initSpi) {}

  void init() override {
    pinMode(_dc, OUTPUT);
    digitalWrite(_dc, HIGH);
    if (_cs >= 0) {
      pinMode(_cs, OUTPUT);
      digitalWrite(_cs, HIGH);
    }
    if (_initSpi) _spi->begin();
  }

  void beginTransaction() override {
    if (_rdActive) endRead();  // 読み出しの途中なら線を周辺機に戻してから
    _spi->beginTransaction(SPISettings(_freq, MSBFIRST, SPI_MODE0));
    if (_cs >= 0) digitalWrite(_cs, LOW);
  }

  /// 読み出しで手叩きに移した線を、SPI 周辺機に戻す。
  /// **明示的に呼ばなくてよい**（次の描画が beginTransaction() で自動的に戻す）。
  void endRead() {
    if (!_rdActive) return;
    _rdActive = false;
    digitalWrite(_rdSck, LOW);
    pinMode(_rdSda, OUTPUT);
    // **end() を挟む**（ESP32 の begin() は開始済みなら何もしない）。
    // 前後に間を置くのは安全側に倒すため。読み出しは速度を求める場面ではない。
    delayMicroseconds(50);
    _spi->end();
    delayMicroseconds(50);
    _spi->begin();
    delayMicroseconds(50);
  }

  void endTransaction() override {
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    _spi->endTransaction();
  }

  /// **データ線が 1 本のパネル用**（M5Stack の ILI9342C など）。
  ///
  /// SDA が MOSI と MISO の兼用で、SPI 周辺機の MISO には何も来ていない基板がある。
  /// このとき標準の `transfer()` では読めない（実測: 全ビット 1 が返る）。
  /// SCK と SDA を渡すと、**読み出しのあいだだけ線を入力に向けて手で叩く。**
  /// 書き込みは今までどおり周辺機に任せるので、速度は落ちない。
  ///
  /// 例（M5Stack Core / BASIC）: `bus.setReadPins(18, 23);`
  ///
  /// **制約**: 読み出しのたびに `SPI.end()` / `SPI.begin()` で線を張り直すので、
  /// **既定以外のピンで `SPI.begin(...)` している構成では使えない**（既定に戻る）。
  /// **【実験中。まだ当てにしないこと】**
  ///
  /// 生のプローブ（コマンドの送出も含めて全部を手で叩く）では**確実に読めた** —
  /// 書いた色が `FC 00 00 / 00 FC 00 / 00 00 FC / FC FC FC` としてそのまま返る
  /// （RGB666、ダミー 1 バイト）。ところが**このクラス経由だと再現しない。**
  /// ESP32 の GPIO マトリクスと `SPI` の取り合いが絡んでいて、まだ詰め切れていない。
  /// 詳細と実測は docs/MANUAL_TEST.ja.md の「読み戻し」。
  ///
  /// **`startWrite()` / `endWrite()` の外で読むこと。** 読み出しは描画の
  /// トランザクションとは別に線を取る。
  ///
  /// 読み出しは**デバッグと検証のためのもの**で、通常の描画では使わない。
  /// なので**速さより確実さに全振りしてある。**
  ///
  /// `settleUs` は 1 エッジあたりの待ち。既定の 2µs でおよそ 125kHz。
  /// 速くすると拾い損ねる（実測: 1µs 相当で 3,072 画素中 51 画素が 1 ビット化けた）。
  void setReadPins(int8_t sck, int8_t sda, uint8_t settleUs = 2) {
    _rdSck = sck;
    _rdSda = sda;
    _rdSettle = settleUs;
  }

  /// コマンドを送ってから読み戻す。**CS はここで落として、ここで上げる。**
  ///
  /// 読み出しピンを設定してあれば**全部を手で叩く**。周辺機と手叩きを途中で
  /// 切り替えるとビットがずれるので、コマンドの送出も手で行う（実測で確認）。
  void readSequence(const uint8_t* script, uint8_t scriptLen, uint8_t dummy, uint8_t* buf,
                    size_t len) override {
    for (size_t i = 0; i < len; ++i) buf[i] = 0;
    if (_rdSck < 0) return;  // 読めないバス

    // **コマンドは周辺機で送る。** ESP32 では pinMode(OUTPUT) では GPIO マトリクスから
    // 線を取り戻せず、手で叩いても波形が出ない（実測。全部 FF が返る）。
    // 一方 pinMode(INPUT) は確実に出力を止められるので、**受信だけ手で叩く。**
    beginTransaction();
    uint8_t i = 0;
    while (i < scriptLen) {
      const uint8_t cmd = script[i++];
      const uint8_t n = script[i++];
      writeCommand(cmd);
      if (n) writeData(&script[i], n);
      i = (uint8_t)(i + n);
    }
    _spi->endTransaction();

    digitalWrite(_rdSck, LOW);
    pinMode(_rdSck, OUTPUT);
    digitalWrite(_rdSck, LOW);
    pinMode(_rdSda, INPUT);  // 線を相手に渡す
    _rdActive = true;
    for (uint8_t d = 0; d < dummy; ++d) bbRead();
    for (size_t k = 0; k < len; ++k) buf[k] = bbRead();
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    endRead();  // 1 回ごとに線を戻す。**まとめて戻す作りにすると読めなくなる**（実測）
  }

  void writeCommand(uint8_t cmd) override {
    digitalWrite(_dc, LOW);
    _spi->transfer(cmd);
    digitalWrite(_dc, HIGH);
  }

  void writeData(const uint8_t* data, size_t len) override {
    while (len--) _spi->transfer(*data++);
  }

  void writeColor(uint16_t color, uint32_t count) override {
    const uint8_t hi = (uint8_t)(color >> 8), lo = (uint8_t)color;
#if TINYGFX_FILL_CHUNK > 0
    if (count >= TINYGFX_FILL_CHUNK) {
      uint8_t buf[TINYGFX_FILL_CHUNK * 2];  // スタック上。静的 RAM は増やさない
      do {
        // **毎回詰め直す。** transfer(buf, n) は受信データで buf を上書きするため。
        // それでも 1 バイトずつ送るより速い（コアがまとめて流せる）。
        for (uint16_t i = 0; i < TINYGFX_FILL_CHUNK; ++i) {
          buf[i * 2] = hi;
          buf[i * 2 + 1] = lo;
        }
        _spi->transfer(buf, sizeof(buf));
        count -= TINYGFX_FILL_CHUNK;
      } while (count >= TINYGFX_FILL_CHUNK);
    }
#endif
    while (count--) { _spi->transfer(hi); _spi->transfer(lo); }
  }

  void writePixels(const uint16_t* data, uint32_t count) override {
#if TINYGFX_FILL_CHUNK > 0
    // 帯レンダリング（TileCanvas）と pushImage がここを通る。
    // 送り出しはビッグエンディアンなので、詰め替えるついでに入れ替える。
    if (count >= TINYGFX_FILL_CHUNK) {
      uint8_t buf[TINYGFX_FILL_CHUNK * 2];
      do {
        for (uint16_t i = 0; i < TINYGFX_FILL_CHUNK; ++i) {
          const uint16_t c = data[i];
          buf[i * 2] = (uint8_t)(c >> 8);
          buf[i * 2 + 1] = (uint8_t)c;
        }
        _spi->transfer(buf, sizeof(buf));
        data += TINYGFX_FILL_CHUNK;
        count -= TINYGFX_FILL_CHUNK;
      } while (count >= TINYGFX_FILL_CHUNK);
    }
#endif
    while (count--) {
      const uint16_t c = *data++;
      _spi->transfer((uint8_t)(c >> 8));
      _spi->transfer((uint8_t)c);
    }
  }

 private:
  uint8_t bbRead() {
    uint8_t v = 0;
    for (uint8_t i = 0; i < 8; ++i) {  // モード 0: 立ち上がりで拾う
      digitalWrite(_rdSck, HIGH);
      if (_rdSettle) delayMicroseconds(_rdSettle);
      v = (uint8_t)((v << 1) | (digitalRead(_rdSda) ? 1 : 0));
      digitalWrite(_rdSck, LOW);
      if (_rdSettle) delayMicroseconds(_rdSettle);
    }
    return v;
  }

  SPIClass* _spi;
  uint32_t _freq;
  int8_t _rdSck = -1;
  int8_t _rdSda = -1;
  uint8_t _rdSettle = 2;
  bool _rdActive = false;
  int8_t _dc;
  int8_t _cs;
  bool _initSpi;
};
