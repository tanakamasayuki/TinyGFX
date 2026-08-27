# 外部への依頼

内部の記録。日本語のみ。**TinyGFX の外側に手を入れてもらう必要があるもの。**

早めに内容を固めておくための一覧。**どれも TinyGFX 側の作業をブロックしない**ように
回避策を用意してある（「それまでどうするか」欄）。

| # | 相手 | 内容 | 優先 | ブロックするか |
| --- | --- | --- | --- | --- |
| [E1](#e1) | host-arduino-core | ホストで SPI と GPIO の**順序付きログ**を取れるようにする | 中 | しない |
| [E2](#e2) | ch32-riscv-ug/arduino_core_ch32_riscv_arduino | **SPI ライブラリが無い**。あと base が 5.9KB ある | 高 | しない |
| [E3](#e3) | openwch / YuukiUmeta-UIAP コア | CH32V00x の `PinMap_SPI_*` が無くリンクできない | 中 | しない |
| [E4](#e4) | LGFXFontToolJs | TinyGFX で使えるフォントヘッダの出力 | 中 | しない |
| [E5](#e5) | arduino-library-release-toolkit | リリース資産の同期（既に取り込み済み） | 低 | しない |
| [E6](#e6) | 外部レジストリ | ライブラリ名 `TinyGFX` の重複確認 | 高 | **公開をブロック** |

---

## E1. host-arduino-core — SPI と GPIO の順序付きログ {#e1}

### 現状

- `SPI` は未実装（README の API 表で 🔲、`Wire` と同じ「init は成功、デバイス無し」形を予定）
- `pinMode` / `digitalWrite` / `digitalRead` は no-op スタブ（🟡）

このため `TinyGFXBusSPI` と `TinyGFXBusSoftSPI`（実際にディスプレイへ喋るコード）を
ホストで検証できない。

### 依頼内容

**「転送したバイト」と「ピンの変化」を、1 本の順序付きイベントログに記録できるようにしてほしい。**

ディスプレイドライバの検証で効くのは各 API の戻り値ではなく**順序**なので、
SPI と GPIO が別々のログだと意味がない。DC を Low にしてからコマンドバイトを送っているか、
CS を落としてから転送しているか、が見たい。

期待するイベント列の例（ST7789 の `CASET` を送ったとき）:

```text
pin  CS  0
pin  DC  0
spi  2A
pin  DC  1
spi  00 00 00 EF
```

API の形は任せるが、次が満たせれば十分:

| 要件 | 内容 |
| --- | --- |
| R1 | `SPI.begin()` / `beginTransaction(SPISettings)` / `transfer(uint8_t)` / `transfer(buf,len)` / `transfer16()` / `endTransaction()` がリンクでき、転送バイトを記録する |
| R2 | `pinMode` / `digitalWrite` がピン番号とレベルを**同じログに**記録する |
| R3 | スケッチからログを読める、またはファイルへ吐ける（テストが読める形なら何でもよい） |
| R4 | ログのリセットができる（テストのフェーズを区切るため） |
| R5 | `digitalRead` が直前に `digitalWrite` した値を返す（今は常に 0） |

`SPISettings` の中身（クロック・ビット順・モード）も記録できるとなお良い。
ST7789 は MODE0 前提なので、間違ったモードで喋っていないかを見たい。

### それまでどうするか

`TinyGFXBusCapture`（`src/TinyGFX/BusCapture.h`）を使う。Bus インターフェースを
実装したテスト用の Bus で、パネルが出すコマンド列を解釈して仮想 GRAM に書き戻す。
**Panel より上（描画コア全部）はこれで検証できる。** 検証できないのは
`TinyGFXBusSPI` / `TinyGFXBusSoftSPI` 自身だけで、そこは実機とコンパイル通過で守る。

---

## E2. ch32-riscv-arduino コア — SPI が無い、base が大きい {#e2}

**窓口**: [ch32-riscv-ug/arduino_core_ch32_riscv_arduino](https://github.com/ch32-riscv-ug/arduino_core_ch32_riscv_arduino)
（`arduino-cli config` の additional_urls から判明）。

### E2-a. SPI ライブラリが無い（実測）

```
$ ls ~/.arduino15/packages/ch32-riscv-arduino/hardware/ch32riscv/1.4.0/libraries/
ArduinoCoreAPI  EVT
```

`#include <SPI.h>` が `fatal error: SPI.h: No such file or directory` になる。
**主ターゲットである CH32V003 で、Arduino 標準の SPI が使えない。**

依頼: SPI ライブラリの追加。無理なら「このコアに SPI は無い」と README に明記してほしい
（利用者が判断できるように）。

### E2-b. 空スケッチが 5,892 バイト（実測）

```
空の setup/loop:  Flash 5,892 / 16,384 (35%)   RAM 504 / 2,048 (24%)
```

`nm` で見ると、使っていない `TwoWire` / `UartClass` / `IPAddress::printTo` / `_malloc_r` /
`I2C_Init` / `USART_Init` がリンクされている（該当シンボル 45 個）。
`platform.txt` はコアのアーカイブを `-Wl,--whole-archive` でリンクしているため、
`--gc-sections` があっても**グローバルオブジェクトから参照されているものは落ちない**。

**16KB のうち 5.9KB（36%）が、ディスプレイのスケッチが一度も使わないもので埋まっている。**

依頼:
1. `Serial` / `Wire` のグローバルオブジェクトを、参照されたときだけリンクされる形にできないか
   （遅延生成、weak 定義、`--whole-archive` を外す、など）
2. `IPAddress::printTo`（458 バイト）が入ってくる経路を切れないか

これが解決すると **TinyGFX が使える余地が 1.5〜2 倍**になる。TinyGFX 単体の工夫では届かない領域。

### それまでどうするか

- SPI: `TinyGFXBusSoftSPI`（`pinMode` / `digitalWrite` だけのビットバン）を既定にする。
  遅いが確実に動く。**このコアで唯一動く経路**なので、いずれにせよ持っておく価値がある
  （[DECISIONS.ja.md](DECISIONS.ja.md) D18）
- base の大きさ: 予算を「base からの増分」で管理する（[FOOTPRINT.ja.md](FOOTPRINT.ja.md) §5）。
  コア側が改善したらそのぶん丸ごと余裕になる

---

## E3. WCH:ch32v / UIAP:ch32v — CH32V00x の SPI がリンクできない {#e3}

**窓口**: [openwch/board_manager_files](https://github.com/openwch/board_manager_files) /
[YuukiUmeta-UIAP/board_manager_files](https://github.com/YuukiUmeta-UIAP/board_manager_files)

### 現状（実測）

両コアとも `libraries/SPI` を持っているが、CH32V00x でリンクすると失敗する。

```
undefined reference to `PinMap_SPI_SCLK'
undefined reference to `PinMap_SPI_MOSI'
undefined reference to `PinMap_SPI_MISO'
undefined reference to `PinMap_SPI_SSEL'
```

`variants/` を見ると `PinMap_SPI_*` は **CH32V20x / CH32V30x にしか定義されていない**。
CH32V00x の `PeripheralPins.c` に SPI のピンマップが無い。

### 依頼内容

`variants/CH32V00x/*/PeripheralPins.c` に `PinMap_SPI_MOSI` / `MISO` / `SCLK` / `SSEL` を追加してほしい。
CH32V003 の SPI1 は PC5(SCK) / PC6(MOSI) / PC7(MISO) / PC1(NSS) が該当する（**要確認**）。

### それまでどうするか

E2 と同じ。`TinyGFXBusSoftSPI` を使う。

---

## E4. LGFXFontToolJs — TinyGFX で使えるフォントヘッダの出力 {#e4}

### 前提

TinyGFX のフォント形式は **GFXfont（Adafruit GFX 互換）に合わせた**
（[DECISIONS.ja.md](DECISIONS.ja.md) D17）。理由はデコーダが小さいこと、
そして LGFXFontToolJs が既にこの形式をエンコードできること。

`src/format/gfxfont.js` の規約「行を連結した MSB first のビット列、グリフ間はバイト境界揃え」
に合わせて `TinyGFX::drawChar` を実装済み。

### 依頼内容

1. **自己完結したヘッダを出せるようにしてほしい。**
   いまの `encodeCSource({format:'gfx'})` は `GFXglyph` / `GFXfont` の型が
   Adafruit_GFX か LovyanGFX 側にある前提。TinyGFX だけを使う人はどちらも入れない。
   - 案 A: 型定義を出力ヘッダに含めるオプション（`#ifndef _GFXFONT_H_` ガード付き）
   - 案 B: `#include <TinyGFX/Font.h>` を出す `target: 'tinygfx'` オプション

   TinyGFX 側は `_GFXFONT_H_` ガードで同じ型を提供しているので、**案 A なら TinyGFX 側の変更は不要**。

2. **プロジェクトで使う文字だけを埋め込むワークフロー。**
   `subset()` は既にあるので、あとは「ソースを走査して使っている文字を集める」側。
   - スケッチの文字列リテラルを拾ってサブセットを作る CLI があると、そのまま CI に載せられる
   - TinyGFX 側でやるべき仕事なら、こちらで書く。**どちらが持つべきか相談したい**

3. **CLI / npx で叩けるか。** CI でフォントを再生成して差分を検査したい
   （`tests/gencheck` 相当。PaperCanvas でやっているのと同じ形）。

4. **確認したいこと**: `encodeCSource` の GFXfont 出力で `bitmapOffset` は
   `uint16_t` か `uint32_t` か。Adafruit 互換なら 16bit だが、大きなフォントで溢れないか。

### それまでどうするか

`tools/gen_font.py` に ASCII アートで書いた 5x7 フォント（0x20-0x3F、32 文字）を
つなぎとして使う。**測定用であって製品ではない。** 出力先も `tests/fonts/` で、
ライブラリには同梱しない。

---

## E5. arduino-library-release-toolkit — リリース資産の同期 {#e5}

`tools/bump_version.py` と `.github/workflows/release.yml` は取り込み済み（コピー）。

依頼というより運用上の確認:

- toolkit 側の `tools/sync_release_assets.py` は親ディレクトリの兄弟リポジトリを走査するので、
  **TinyGFX も自動的に同期対象に入る**はず。初回リリース前に一度流して差分が無いことを確認する
- TinyGFX は `tests/constructs/*.ino` を持つが、リリースブランチでは `tests/` ごと削除されるので影響なし

---

## E6. ライブラリ名 `TinyGFX` の重複確認 {#e6}

**リポジトリを公開する前に必ずやる。** これだけは公開をブロックする。

| 確認先 | 見るもの |
| --- | --- |
| GitHub | 同名の Arduino ライブラリ |
| Arduino Library Registry | `name=TinyGFX` の登録済みライブラリ |
| PlatformIO Registry | 同名パッケージ |

`TinyGFX` / `tinygfx` は別分野で使われた例があるため、Arduino 界隈で強い競合が無ければ採用でよい。
競合があれば `TinyGFXCore` / `TinyDisplayGFX` などに寄せる。**名前が変わると
`src/tinygfx_version.h` のマクロ名と `library.properties` も変わる**ので、実装が進む前に決めたい。
