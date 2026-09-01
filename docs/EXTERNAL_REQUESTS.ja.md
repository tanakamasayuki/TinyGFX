# 外部への依頼

内部の記録。日本語のみ。**TinyGFX の外側に手を入れてもらう必要があるもの。**

早めに内容を固めておくための一覧。**どれも TinyGFX 側の作業をブロックしない**ように
回避策を用意してある（「それまでどうするか」欄）。

| # | 相手 | 内容 | 優先 | ブロックするか |
| --- | --- | --- | --- | --- |
| [E1](#e1) | host-arduino-core | バスを覗ける口 → **1.5.0 でリリース済み。取り込み完了** | 済 | — |
| [E2](#e2) | ch32-riscv-ug/arduino_core_ch32_riscv_arduino | SPI ライブラリが無い。base が 5.9KB → **新コアで解消見込み** | 低 | しない |
| [E3](#e3) | openwch / YuukiUmeta-UIAP コア | CH32V00x の `PinMap_SPI_*` が無い → **新コアで解消見込み** | 低 | しない |
| [E7](#e7) | **ArduinoCore-CH32（開発中）** | **リリースを待つ。それまで測定は symlink 運用** | 高 | しない |
| [E4](#e4) | LGFXFontToolJs | **CellFont の CLI。** 公開済み（`lgfx-font-tool` 2.0.0）。**閉じてよい** | 中 | しない |
| [E8](#e8) | LGFXFontToolJs | ~~`--format u8g2` の C 出力が LovyanGFX 依存~~ **解決（2.2.2 の `--no-wrapper`）** | — | しない |
| [E5](#e5) | arduino-library-release-toolkit | リリース資産の同期（既に取り込み済み） | 低 | しない |
| [E6](#e6) | 外部レジストリ | ライブラリ名 `TinyGFX` の重複確認 → **確認済み・問題なし** | 済 | 解消 |
| [E9](#e9) | GfxImageToolJs | ~~一括モードで形式が総当たりされない~~ **解決（フォルダ出力の修正）。実測 8,720 → 4,184 B** | — | しない |
| [E10](#e10) | GfxImageToolJs | ~~「変換後の画素」を出す口が無い~~ **解決（`--preview`）。ディザと減色を検証できるようになった** | — | しない |
| [E11](#e11) | GfxImageToolJs | ~~フォルダ変換だと透過が黙って落ちる~~ **解決。既定で保たれるようになった** | — | しない |
| [E12](#e12) | GfxImageToolJs | ~~相対 `--out` の基準がずれる~~ **解決。`[preview] output_dir` も入った** | — | しない |
| [E13](#e13) | GfxImageToolJs | ~~元画像を消しても出力が残る~~ **解決。マニフェストで追跡し、`build` が消す** | — | しない |
| [E14](#e14) | GfxImageToolJs | ~~数字で始まる名前が `_2nd` になる~~ **解決（`img_2nd`）** | — | しない |
| [E15](#e15) | GfxImageToolJs | ~~マニフェストが無いとき全行 `upToDate` なのに落ちる~~ **解決** | — | しない |
| [E16](#e16) | GfxImageToolJs | ~~preview のマニフェストだけ出力先に残る~~ **解決。cache に揃った** | — | しない |
| [E17](#e17) | GfxImageToolJs | ~~生成物にツールのバージョンが無い~~ **解決。`--json` に `tool`** | — | しない |
| [E18](#e18) | GfxImageToolJs | ~~データ長の制限が全形式に掛かる~~ **解決。RLE 2 形式だけになった** | — | しない |
| [E19](#e19) | GfxImageToolJs | ~~設定の綴り間違いが黙って無視される~~ **解決。4 種類とも警告する** | — | しない |
| [E20](#e20) | GfxImageToolJs | ~~GUIDE の例が落ちる／設定の違いを言わない~~ **解決。両方** | — | しない |
| [E21](#e21) | GfxImageToolJs | ~~最初の一歩と、最後の 1 枚~~ **解決。両方** | — | しない |

---

## E1. host-arduino-core — バスを覗ける口 {#e1}

> **2026-08-28: `lang-ship:host` 1.5.0 としてリリースされ、TinyGFX 側の取り込みも終わった。**
> 詳細は §「実装の確認」（末尾）。以下は依頼時点の記述をそのまま残してある。


### 現状

- `SPI` は未実装（README の API 表で 🔲）
- `pinMode` / `digitalWrite` / `digitalRead` は no-op スタブ（🟡）。`digitalRead` は常に 0

このため `TinyGFXBusSPI` と `TinyGFXBusSoftSPI`（実際にディスプレイへ喋るコード）を
ホストで検証できない。

### 結論 — 頼みたいのは「デバイス」ではなく「覗き口」

**`SPI` クラスを足すだけでは足りない。** 送ったバイトがどこにも残らないので、
テストから見えるものが何も増えない。

**かといって SD や LCD をコアに実装してもらうのも違う。** プロトコルを知っているのは
その周辺機器を扱うライブラリ側で、コアがデバイスを抱え始めると際限がない。

欲しいのは中間、**「バスに流れたものを外から観測し、応答を差し込める口」**。
デバイスの模型はライブラリ側が持つ。

```text
スケッチ ──SPI.transfer()──▶ コアの SPI ─────┐
                                              ├──▶ 観測・応答フック
スケッチ ──digitalWrite()──▶ コアの GPIO ────┘         │
                             （書き込み通知）           │  ライブラリ側が実装する
                                                        ▼  デバイスの模型
                                              例: TinyGFX の ST7789 模型
                                                        │
                                                        ▼
                                                  仮想 GRAM → PPM/PNG
```

TinyGFX は既に `TinyGFXBusCapture` で ST7789 のコマンド列を解釈して仮想 GRAM に
書き戻す模型を持っている。**それを Bus の下ではなく SPI の下に付け替えられれば、
スケッチ → 描画コア → Panel → Bus → 線 → パネル模型 → 画素、まで通しで検証できる。**

### 部品は 3 つ

#### (1) SPI が動いて、流れたバイトを観測できる

```cpp
SPI.begin();
SPI.beginTransaction(SPISettings(24000000, MSBFIRST, SPI_MODE0));
SPI.transfer(0x2A);
SPI.endTransaction();
```

がリンクして動き、**転送された 1 バイトごとにフックが呼べる**こと。
`SPISettings` の中身（クロック・ビット順・モード）も取れると、
「ST7789 に MODE0 で喋っているか」が見られる。

フックの形は任せるが、応答（MISO 相当）を返せると SD やセンサにも使える。
**TinyGFX には応答は要らない**（ディスプレイは書き込み専用）ので、
そこは優先度を落としてよい。

#### (2) GPIO の書き込みを観測できる ← **これが本命**

```cpp
digitalWrite(SCK, HIGH);   // ← これが通知されてほしい
```

**理由: TinyGFX の主ターゲット CH32V003 では、既定のバスがソフト SPI（ビットバン）。**
`digitalWrite` で SCK と MOSI を直接叩くので、**SPI クラスを経由しない。**
(1) だけでは、いちばん検証したい経路がまるごと素通りする。

ピンの書き込みが観測できれば、ライブラリ側で SCK の立ち上がりを数えてバイトに
組み直せる。DC / CS も同じ仕組みで見える。

**この口は TinyGFX 以外にも効くはず**（ビットバンでやるものすべて — ソフト I2C、
IR のパルス、WS2812 など）。時刻（`micros()` 相当）が一緒に取れると、
そういう用途では価値がさらに上がる。

#### (3) `digitalRead` が直前に書いた値を返す

いまは常に 0。**これだけでデバイス模型が DC を読めるようになる。**

```cpp
uint8_t MyPanelModel::feedByte(uint8_t b) {
  if (digitalRead(_dc) == LOW) { command(b); } else { data(b); }
}
```

コア側に「ピン変化を模型へ通知する」仕組みを足さなくて済むので、**いちばん安い解**。
`digitalWrite` → `transfer` の順序も、同期的に呼ばれる以上、自然に保たれる。

### 優先度

| | 内容 | TinyGFX にとって |
| --- | --- | --- |
| **P0** | (2) GPIO 書き込みの観測、(3) `digitalRead` の値保持 | **これだけでソフト SPI 経路が全部検証できる。最小で最大** |
| **P1** | (1) SPI が動いて転送バイトを観測できる | ハードウェア SPI 経路の検証に要る |
| P2 | `SPISettings` の中身の取得 | モード・クロックの取り違えを見たい |
| P3 | フックが応答バイトを返せる（MISO） | **TinyGFX には不要。** SD やセンサ向け |
| P3 | 観測にタイムスタンプを付ける | TinyGFX には不要。IR など向け |

**P0 だけでも依頼を出す価値がある。** 実装は「`digitalWrite` の中でコールバックを 1 本呼ぶ」
「書いた値を配列に覚えて `digitalRead` で返す」で、コアの負担は小さいはず。

### コアに入れないでほしいもの

- **SD カードの模型** — SD の SPI プロトコルを知っているのは SD ライブラリ側
- **LCD の模型** — ST7789 / ILI9341 の初期化列とコマンドを知っているのは表示ライブラリ側。
  **TinyGFX が自分で持つ**（もう持っている）
- **特定のピン配置の前提** — 観測は「ピン番号 → 値」の粒度でよい

コアが持つべきなのは**バスと口だけ**。デバイスはライブラリが持ち込む。
そうすればコアはデバイスが増えても太らないし、各ライブラリは自分のプロトコルを
自分でテストできる。

### あるとうれしい（P1 の派生）

**「ライブラリ側が作った画像をコアが画面に出す」口**があると、手動確認が一気に楽になる。

`lang-ship:host:display` の SDL2 ウィンドウは既にあるので、
「フレームバッファを渡すと表示される」API があれば、TinyGFX のパネル模型が
そこへ流し込むだけで**ホストで実際の描画が目で見られる**。

ここでもプロトコルはライブラリ側、画面はコア側、という分担は変わらない。

### 検収条件

ホスト上で次のスケッチが動き、`output/scene.ppm` に期待した絵が出れば満たされている。

```cpp
TinyGFXBusSoftSPI  bus(SCK, MOSI, DC, CS);      // 本番と同じバス
TinyGFXDriverST7789 panel(bus, 240, 240);        // 本番と同じパネル
TinyGFX            lcd(panel);

MyST7789Model model(SCK, MOSI, DC, CS, gram);   // ライブラリ側の模型。(2)(3) で作る

void setup() {
  lcd.begin();
  lcd.fillRect(10, 10, 40, 40, TFT_RED);
  writePpm("output/scene.ppm", gram, 240, 240);  // 模型が組み立てた画素
}
```

**いま `TinyGFXBusCapture` で Bus の上までは検証できている。**
この依頼で増えるのは Bus 自身（`BusSoftSPI` / `BusSPI`、合わせて 160 行ほど）の
カバレッジだが、**そこは間違えても実機に挿すまで気づけない層**（ビット順、DC を落とす
タイミング、トランザクション中の CS）なので、価値はある。

### それまでどうするか

`TinyGFXBusCapture`（`src/TinyGFX/BusCapture.h`）を使う。Bus インターフェースを
実装したテスト用の Bus で、パネルが出すコマンド列を解釈して仮想 GRAM に書き戻す。
**Panel より上（描画コア全部）はこれで検証できる。**

### 実装の確認（2026-08-27）

**依頼した 3 つがすべて入っていた。** しかも P3 まで。

| 依頼 | 実装 |
| --- | --- |
| (1) SPI が動いて転送バイトを観測できる | `SPI.setTransferHook`。**戻り値がそのまま MISO** |
| (2) GPIO の書き込みを観測できる | `HostArduino::setPinWriteHook`（`cores/host/HostBus.h`） |
| (3) `digitalRead` が直前に書いた値を返す | ✅。加えて `INPUT_PULLUP` で HIGH をシード |
| P2: `SPISettings` の中身 | `setTransactionHook` + `SPI.settings()` |
| P3: 応答（MISO） | SPI は転送フックの戻り値、GPIO は `setPinValue` / `setPinReadHook` |
| P3: タイムスタンプ | **意図的に無し**（`micros()` を呼べばよい）。妥当な判断 |
| 依頼外 | `Wire` のフック、`BusObserve` example、3 本のテスト |

「コアは周辺機器を模型化しない。デバイス模型はライブラリ側」という切り分けも
そのまま採られている。

#### 受入条件を通した — `tests/hostbus/`

依頼書に書いた受入条件のスケッチを実際に書いて動かした。**API の不足も回避策も無し。**

```text
TinyGFX → DriverST7789 → 本番の TinyGFXBusSoftSPI / TinyGFXBusSPI → 線
        → TgfxPinProbe / TgfxSpiProbe（TinyGFX 側の模型）→ 仮想 GRAM → PPM
```

検証できたこと:

| 項目 | 結果 |
| --- | --- |
| ソフト SPI とハードウェア SPI が**同じ絵**を出す | 一致（bbox 差分なし） |
| 転送バイト数 | 両方 2,211 B / 1,089 画素（過不足なし） |
| ビット順・クロック・モード | MSB first / 24 MHz / MODE0 |
| 転送外の CS / DC | どちらも HIGH に戻っている |
| トランザクションの閉じ忘れ | 無し（`inTransaction() == 0`） |

**これが BusCapture では見えなかった層。** ビット順、DC を落とすタイミング、
トランザクション中の CS は、ここで初めて機械的に守れるようになった。

#### 実装を読んで気づいたこと（不具合ではない）

1. **一括転送もすべてフックに届く。** `transfer16` / `transfer32` / `transferBytes` /
   `writeBytes` / `writePixels` が全部 `transfer()` を通っているので、
   バイト単位で呼んでいない利用者も取りこぼさない。**確認済み。**
2. **フックが 1 スロットしかない。** 「共有したい模型はピン番号で振り分ければよい」と
   書いてあるが、**2 つのライブラリを同じスケッチでテストするときは共存できない。**
   TinyGFX は 1 本ずつテストするので困らない。チェインは確保が要るので今の判断で妥当だと思う。
3. **`SPISettings` のフィールドが `_clock` / `_bitOrder` / `_dataMode`。**
   arduino-esp32 に合わせた public なので読めるが、テストのコードに `s._clock` と書くと
   private を触っているように見える。`clock()` / `bitOrder()` / `dataMode()` の
   読み取り専用アクセサがあると読みやすい（**些細。無くても困らない**）。
4. **スレッドの注意書きが効くのは `mode=lgfx` / `display` のとき。** TinyGFX は素の
   `host` で使うので該当しない。この注意書きは残しておくべき。

#### 取り込み完了（2026-08-28、1.5.0）

- `tests/hostbus/sketch.yaml` の platform を **`lang-ship:host (1.5.0)`** に固定した。
  他のテストの sketch.yaml も 1.4.7 → 1.5.0 に上げた
- 指摘していた `SPISettings` の読み取り専用アクセサ（`clock()` / `bitOrder()` / `dataMode()`）が
  1.5.0 で入ったので、テストもそちらを使うように直した。
  `s._clock` と書かずに済むようになった
- **依頼はこれで完了。** 残っている観測点は「フックが 1 スロットで、
  2 つのライブラリを同じスケッチでテストするときは共存できない」ことだけ（実害なし）

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

### 2026-08-27 追記 — 開発中の新コアで両方とも解消する

`ch32-riscv-ug/ArduinoCore-CH32`（プレリリース）で同じ測定をしたところ:

- **`TinyGFXBusSPI`（ハードウェア SPI）がリンクできた**（E2-a 解消）
- **base が 624 B**。5,892 B → 624 B で **5,268 B の改善**（E2-b 解消）

つまり **E2 は「現行コアを直す」より「新コアのリリースを待つ」が正解**。
優先度を下げ、代わりに [E7](#e7) を立てた。数字は
[FOOTPRINT.ja.md](FOOTPRINT.ja.md) §6.1。

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

## E4. LGFXFontToolJs — CellFont の CLI {#e4}

### 決着した（2026-08-28）

**形式は CellFont v1 として確定し、仕様書は LGFXFontToolJs 側にある。**

> `docs/formats/cellfont.ja.md`（+ `cellfont.en.md`）

当初この節でお願いしていた「TinyFont エンコーダ」の中身は、**ほぼそのまま仕様に
入っている** — 索引とグリフ表の自動選択（§10.3）、刈り込まずセルで出す判断（§10.1）、
幅クラスで割って `next` で繋ぐ（§10.2）、PROGMEM を 4 つすべてに（§12.2）、
決定的な出力（§10.4）。**依頼としては閉じてよい。**

TinyGFX 側は **2026-08-28 に CellFont v1 の描画器を実装済み**
（`src/TinyGFX/CellFont.h` + `src/TinyGFX/FontCell.h`）。

### 仕様に取り込まれた指摘（TinyGFX 側から出したもの）

| # | 指摘 | 反映 |
| --- | --- | --- |
| 1 | 「`headCount` はアライメントの余りに収まる」は 16bit ABI では成り立たない（AVR で 19 → 20 バイト。実測） | §3 に ABI 別の表として反映 |
| 2 | 頭ブロックの閾値は 2 でなく **1** でよい（`first` / `headCount` は元からあるので追加コスト 0、`codes` が 2 バイト減る） | §10.3 / §15.1 が `headCount >= 1` に |
| 3 | **連鎖が入れ子になる描画器**では、U+FFFD 退避をデコーダの中に置いてはならない | §15.2 に注意書きとして追加 |
| 4 | 生成ヘッダの `#include <CellFont.h>` が**大域の include 名前空間を占める** | §12.1 / §12.2 改訂。**ファイル名を仕様から外し、生成ヘッダは include しない**形に |

### 逆に仕様から学んだこと（TinyGFX 側の不具合 3 件）

仕様 §12.2 / §15.2 / §7.1 が名指ししている失敗を、**TinyGFX が実際にやっていた。**

1. **フォント構造体だけ PROGMEM が抜けていた** — AVR で文字が化ける。`avr-nm` で確認して修正
2. 固定ピッチのビットマップオフセットが 16bit — 大きな集合で折り返す
3. 二分探索の中央値が加算形 — 16bit 環境で折り返す

### CLI は公開された（2026-08-28）

`lgfx-font-tool` **2.0.0** で `lgfx-font build` が使える。**依頼としては閉じてよい。**

```sh
npx -p lgfx-font-tool lgfx-font build --google "Noto Sans JP" --em 12 \
    --chars "温度設定完了 23.5℃" --format cellfont --out font.h
```

**`-p` を付けないと環境によって 404 になる。** パッケージ名（`lgfx-font-tool`）と
コマンド名（`lgfx-font`）が違うためで、npx のキャッシュに入っていれば素の
`npx lgfx-font` でも引けるが、当てにできない（`node_modules` の有無でも変わる）。
**README に `-p` 付きの形を書いておくと親切だと思う。**

出力を `tests/clifont/` にそのまま置いて回帰検査にした。**ローカルの checkout と
公開版でデータはバイト一致**（違いはシンボル名だけ。出力ファイル名から取るため）。

TinyGFX 側のつなぎ `tools/gen_font.py` は、外部ツールなしで `tests/` と
`examples/` が走るために残す。**出力は仕様 §12.2 の形なので、置き換えたければ
ファイルを差し替えるだけ。**

### CLI 草案へのコメント

- **`--format` の既定を `cellfont` にしてよいか**（§13 の未決事項）— TinyGFX の
  利用者から見れば `cellfont` 既定がありがたいが、**汎用ツールとしては `--format` 必須の
  ほうが素直**だと思う。既定を置くなら「対象機に載せるなら cellfont」と 1 行出るとよい
- **`--px` が墨面の高さであること**（§13）— これは**出力ヘッダのコメントに実際の
  `height` と `yAdvance` を書く**のが一番伝わる。利用者は数字を見て納得する
- **`--check` の終了コード 2** — CI で使う。TinyGFX 側でも同じ運用にする

### サブセット化は TinyGFX の対象外

「プロジェクトで使う文字だけを埋め込む」仕組みは**完全に外部のツール**として作る。
CLI 側も §1 で「ソースコードの走査はやらない」としており、認識は一致している。

## E5. arduino-library-release-toolkit — リリース資産の同期 {#e5}

`tools/bump_version.py` と `.github/workflows/release.yml` は取り込み済み（コピー）。

依頼というより運用上の確認:

- toolkit 側の `tools/sync_release_assets.py` は親ディレクトリの兄弟リポジトリを走査するので、
  **TinyGFX も自動的に同期対象に入る**はず。初回リリース前に一度流して差分が無いことを確認する
- TinyGFX は `tests/constructs/*.ino` を持つが、リリースブランチでは `tests/` ごと削除されるので影響なし

---

## E6. ライブラリ名 `TinyGFX` の重複確認 {#e6}

**2026-08-27 確認。問題なし。`TinyGFX` を採用してよい。**

| 確認先 | 方法 | 結果 |
| --- | --- | --- |
| Arduino Library Registry | `arduino-cli lib search tinygfx`（index 更新後） | **0 件** |
| PlatformIO Registry | `api.registry.platformio.org/v3/search?query=tinygfx` | **0 件** |
| GitHub | リポジトリ名検索 | 同名 3 件あるが**すべて無関係**（Rust の学習用、OpenGL/Vulkan のエンジン）。star はいずれも 0、**Arduino ライブラリは 1 つも無い** |

Arduino 界隈に競合が無いので、名前の衝突は実害なしと判断する。

## E7. ArduinoCore-CH32（開発中）— リリース待ち {#e7}

**窓口**: `~/dev_wch/ArduinoCore-CH32`（`ch32-riscv-ug`、`version=0.0.1` のプロトタイプ）

### なぜ待つのか

[FOOTPRINT.ja.md](FOOTPRINT.ja.md) §6.1 のとおり、このコアなら
**base が 624 B**（現行 5,892 B）で、**ハードウェア SPI もリンクできる**。
TinyGFX にとっては E2 と E3 が一度に解消する。

### 2026-08-27 追記 — Boards Manager から入るようになった

この環境では `~/.arduino15/packages/ch32-riscv-ug` に入っていて、symlink も
`compiler.path` の指定も要らなくなった。

```sh
arduino-cli compile --fqbn ch32-riscv-ug:ch32v:CH32V003:pnum=CH32V003F4P6 --library <TinyGFX> <sketch>
```

TinyGFX 側は `TINYGFX_FQBN` でテストの基準機を切り替えられるようにした:

```sh
TINYGFX_FQBN='ch32-riscv-ug:ch32v:CH32V003:pnum=CH32V003F4P6' uv run pytest footprint -s
```

**予算表は 1 本のまま、両方のコアで通ることを確認済み**（[FOOTPRINT.ja.md](FOOTPRINT.ja.md) §6.1）。

まだ開発中でどの環境でも入るとは限らないので、**自動テストの既定は現行コアのまま**。
安定して配布されるようになったら、次をやる:

- `tests/tinygfx_build.py` の `CH32V003` を新 FQBN に差し替え
- [FOOTPRINT.ja.md](FOOTPRINT.ja.md) §5 の予算表を測り直す（base が変わるので増分は同じでも総量が変わる）
- `TinyGFXBusSPI` を Tier 2 の `build_matrix/` に追加（現行コアではビルドすらできない）
- `TinyGFXBusSoftSPI` は残す。**AVR と、SPI が無い環境の保険**として要る

### 聞いておきたいこと

- CH32V003 の `SPI.begin()` はピン引数を取らない形か（`TinyGFXBusSPI` の前提）
- `SPISettings` のクロック上限（ST7789 は 40MHz 級まで受けるので、どこまで出せるか）

## E8. LGFXFontToolJs — `--format u8g2` の C 出力（**解決済み**） {#e8}

**2026-08-28 に `lgfx-font-tool` 2.2.2 で `--no-wrapper` が入り、解決した。**

### 何が問題だったか

`--format u8g2` の C 出力が、バイト列に加えて `lgfx::U8g2font` を宣言していた。
**LovyanGFX が無いとコンパイルが通らない**ので、u8g2 のデコーダを持つ TinyGFX でも
ヘッダをそのまま置けなかった。

### どう解決したか

```sh
npx -p lgfx-font-tool lgfx-font build --font lgfxJapanGothic_8 \
    --chars "日本語表示" --format u8g2 --no-wrapper --name u8g2Cjk --out u8g2_cjk.h
```

`--no-wrapper` でデータ配列だけが出る。シンボルは `<name>`（ラッパーが無いので
`_data` の接尾辞も付かない）。**TinyGFX 側は 1 行包むだけ**で、cellfont と同じ手数になった。

```cpp
static const TinyGFXFontRef u8g2CjkFont = {u8g2Cjk, &tinygfxFontU8g2Ops};
```

**バイト列は以前と完全に一致**（ascii 163 B / cjk 103 B）。`tests/u8g2/` のヘッダは
CLI の出力に差し替え、`tools/gen_u8g2_ref.mjs` からヘッダ出力を外した。
**同じ生成器が残っているのは参照画像（`.ref.txt`）のためだけ** — これは
「LGFXFontToolJs が描いた絵」であって、フォントデータではない。

### 残っている小さな話 — `CELLFONT_PROGMEM` を使ってほしい

cellfont の出力は `LGFXFT_PROGMEM` を自前で定義して（`PROGMEM` から）使っている。
仕様 §12.1 は **`CELLFONT_PROGMEM` を描画器が提供する**と決めていて、
描画器がそこにセクション属性などを入れたときに効かなくなる。
いまは AVR で `PROGMEM` に展開されるので動いている。**優先度は低い。**

---

## E9. GfxImageToolJs — 一括モードで形式が総当たりされない {#e9}

> **2026-08-31: 解決。** フォルダ出力が修正され、**一括モードでも全形式が総当たり
> される**ようになった。同じ 4 枚で **8,720 → 4,184 B**、個別ビルドの合計と 1 バイト
> も違わない。報告の食い違いも直っている（下の「確認したこと」）。以下は依頼時点の
> 記述をそのまま残してある。

**2026-08-30、リリース前の CLI（`bin/gfx-image-tool.js`）で確認。**

### 症状

**1 枚ずつビルドすると形式を総当たりして選ぶが、フォルダを渡すと全部 raw565 になる。**

```sh
# 1 枚ずつ
gfx-image-tool build icon.png  --target tinygfx --json   # -> rlepal4, 158 B
gfx-image-tool build alpha.png --target tinygfx --json   # -> rlepal4,  61 B
gfx-image-tool build mono.png  --target tinygfx --json   # -> rlepal4,  93 B

# フォルダごと
gfx-image-tool inspect ./src --target tinygfx
#   icon.png    raw565  2048 B
#   alpha.png   raw565  1152 B
#   mono.png    raw565  2048 B
```

| 画像 | 一括 | 個別 |
| --- | ---: | ---: |
| alpha 24x24 | raw565 1,152 | **rlepal4 61** |
| icon 32x32 | raw565 2,048 | **rlepal4 158** |
| mono 64x16 | raw565 2,048 | **rlepal4 93** |
| photo 48x32 | raw565 3,072 | raw565 3,072 |
| データ計 | **8,320** | **3,384** |
| デコーダ | 400 | 800（2 形式） |
| **総計** | **8,720** | **4,184** |

**2.08 倍。** `--target tinygfx` を `.imagesconfig` に書いても、`[color] format` 行を
コメントアウトしても変わらなかった。

### さらに、報告そのものが食い違う

一括モードは各行にこう出す:

```
optimize icon.png: raw565:2048 -> raw565 (+0 B vs individual)
```

**「個別なら raw565」と言っているが、個別ビルドは rlepal4 を選ぶ。**
比較対象の「individual」も総当たりを通っていないように見える。

### なぜ重要か

**一括で選ぶことが、この形式群の設計の前提**である。デコーダ代は画像ごとではなく
**形式ごとに 1 回**なので、1 枚ずつ最小化すると形式が散らばって総量で損をする ——
それを避けるために一括最適化を依頼した（[IMAGE_FORMAT.ja.md](IMAGE_FORMAT.ja.md)）。
いまはその逆で、**一括のほうが 2 倍大きい。**

### 単体ビルドは正しく動いている

`--decoder-cost` は効く（100 にすると total が 493 → 193）。`--json` は
`format` / `bytes` / `decoderBytes` / `totalBytes` に加えて
`vblit: {selected, alignedBytes, genericBytes}` まで返す。**必要な情報は揃っている。**

### 確認したこと（2026-08-31）

同じ 4 枚のフォルダを渡した結果:

```
  alpha.png                    rlepal4          61 B
  icon.png                     rlepal4          158 B
  mono.png                     rlepal4          93 B
  photo.png                    raw565           3072 B
  decoders: raw565, rlepal4; data 3384 B + decoder 800 B = 4184 B
  optimize alpha.png: rlepal4:61, bitmap1h:76, bitmap1v:76, rle565:123, raw565:1152 -> rlepal4 (+0 B)
```

| | 修正前 | 修正後 |
| --- | ---: | ---: |
| データ計 | 8,320 | **3,384** |
| デコーダ | 400 | 800 |
| **総計** | **8,720** | **4,184** |

**候補が 1 行に全部出るようになった**ので、何と比べて選ばれたのかが読める。
`(+0 B vs individual)` も正しくなっていて、個別より大きくなる場合は
**その差を出す**（例: `+62 B vs individual`）。

**一括で選ぶことが本来の設計だったことも、そのまま観測できる。** rle565 でしか
表せない画像を 1 枚足すと、alpha / icon / mono が rlepal4 から rle565 に移る ——
3 つ目のデコーダ 400 B を払うより、少し大きい符号化のほうが安いため。
`tests/image_oracle/sources/.imagesconfig` はこれを承知のうえで形式を固定している。

## E10. GfxImageToolJs — 「変換後の画素」を出す口 {#e10}

> **2026-08-31: 解決。** `--preview <path>` が付いた（フォルダなら出力先ディレクトリ）。
> 中身は**変換後の画素**で、透過は黒に潰れ、RGB565 は 8 bit に戻して書かれる
> （切り捨てれば元の 5/6 bit に戻るので、比較は 565 のまま成立する）。
> **これで検証できなかった 2 つが検証できるようになった** ——
> ディザ（127 階調 → 2、誤差拡散のパターンごと一致）と
> 減色（2,304 色 → 16、どの 16 色かはツールの選択）。以下は依頼時点の記述。


仕様書 §15.2 のオラクルは、**ツールが出した期待画像**と TinyGFX が描いた結果を
突き合わせる形になっている。**自作の encode と decode の往復では、両者が同じ
勘違いをしていたら一致してしまう**ため。

いまの CLI に画像を書き出す口が無いので、`--emit-reference <path.ppm>` のような
ものが要る。中身は**減色・2 値化・ディザのあとの画素**（元画像ではない）。

### いま何ができていて、何ができていないか

**代わりに「変換元から期待画像を作る」方法で 5 形式を検証し、すべて一致した**
（`tests/image_oracle/`。raw565 / rlepal4 ×3 / bitmap1h / bitmap1v）。
RGB888→RGB565 も 1bpp の閾値も決定的な変換なので、符号化器を再実装せずに
期待画像を作れる。

**この方法が使えないのは 2 つ。**

- **ディザ**（`--dither floyd-steinberg` など）—— 誤差拡散の実装依存
- **減色**（色数がパレットを超えるとき）—— どの色を残すかの選択

**そこはツールの出力が無いと検証できない。**

### 確認したこと（2026-08-31）

`tests/image_oracle/` を **`--preview` の出力を期待画像にする形に組み直した。**
`sources/` に PNG を置いて `regen.py` を 1 回走らせると、比較の両側 ——
TinyGFX がコンパイルする `generated/images.h` と、突き合わせる `expected/*.png` ——
が同時に出る。**フォルダを 1 回渡すだけ**なので、E9 の一括最適化もここで動く。

**8 枚に増やし、5 つのデコーダを全部通した**（従来は 5 枚・4 デコーダで、
rle565 が未検証だった）。

| 画像 | 形式 | 何を見ているか |
| --- | --- | --- |
| photo | raw565 | 圧縮しない道 |
| bands | rle565 | **従来は未検証だった** |
| icon | rlepal4 | パレット |
| alpha | rlepal4 | 透過色がヘッダに乗るか |
| mono_h / mono_v | bitmap1h / bitmap1v | 同じ絵を両方の詰め方で |
| **grad** | bitmap1h | **ディザ**（127 階調 → 2、Floyd-Steinberg） |
| **quant** | rlepal4 | **減色**（2,304 色 → 16） |

**全 8 枚が 1 画素も違わずに一致した。** 壊れることも確認してある ——
`gradData` の 1 バイトを反転すると 8/4096 画素、`quantPalette` の 1 色を
`0x1483 → 0x14A3` にずらすと 128/2304 画素の差を検出する。

`regen.py --check` が、ツールの出力が変わったのに `generated/` と `expected/`
が古いままの状態を落とす。**committed のまま比較を続けて「昨日の答え」に
通ってしまう**のを防ぐため。ツールが入っていない環境では skip する。

---

## E11. GfxImageToolJs — フォルダ変換だと透過が黙って落ちる {#e11}

> **2026-08-31: 解決。** フォルダ変換でも、アルファチャンネルを持つ入力は
> **既定で透過が保たれる**ようになった（`.imagesconfig` 無しで `hasTransparent = 1`）。
> `tests/image_oracle/` からは回避の `alpha_mode = color-key` を外してある ——
> **既定が戻ったら 268/576 画素の差で落ちる**ので、いまはテストが既定を見張る側。


**2026-08-31 確認。** 同じ PNG が、**ファイルとして渡すか、フォルダとして渡すかで
違う結果になる。**

```sh
# ファイルとして渡す -> 透過が残る
gfx-image-tool build alpha.png --target tinygfx --out a.h
#   -> CellImage.hasTransparent = 1

# フォルダとして渡す -> 透過が落ちる（.imagesconfig 無しでも同じ）
gfx-image-tool build ./src --target tinygfx --out gen
#   -> CellImage.hasTransparent = 0
```

`[alpha] mode` の既定が `none` で、**アルファ値を持つ PNG でも黙って matte
（既定は黒）に合成される。** 警告も出ない。

### なぜ効くか

**透過は「絵の見た目」ではなく「背景を残すかどうか」なので、絵を見ても気づけない。**
黒い背景に描いている限り、透過が落ちていても同じ絵に見える。**別の色の上に
重ねた瞬間に、初めて四角い黒地が出てくる。**

`tests/image_oracle/` でこれを踏んだ。透過つきの 1 枚を入れていたのに
`hasTransparent = 0` で通っていて、**「透過色がヘッダに乗るか」を見ているつもりで
何も見ていなかった。** いまは `alpha_mode = color-key` を明示したうえで、
**マゼンタの上に描いて**突き合わせている（黒の上では、透過を無視するデコーダでも
一致してしまうため）。

### 頼みたいこと

**フォルダでも単体と同じ既定にしてほしい** —— アルファチャンネルを持つ入力なら
`color-key` を既定にする。それが仕様として難しければ、せめて
**「アルファを落とした」と警告を出す**だけでも踏まなくなる。

`[alpha] mode = color-key` を書けば正しく出るので、**回避はできている。**

## E12. GfxImageToolJs — フォルダ変換で相対 `--out` の基準がずれる {#e12}

> **2026-08-31: 解決。** 相対パスはどちらもカレント基準に揃った。あわせて
> **`[preview] output_dir` が入った**ので、`regen.py` は `--out` も `--preview` も
> 渡さず `.imagesconfig` だけで走る。`--check` もツールのものをそのまま使っている
> （ヘッダとプレビューの両方を見て、食い違ったファイル名を出し、2 で終了する）。


**2026-08-31 確認。** 1 つのコマンドラインに書いた 2 つの相対パスが、
**別々の場所を基準に解決される。**

```sh
cd /tmp/work
gfx-image-tool build sources --out outdir --preview prevdir
#   outdir/images.h        written   -> 実際は /tmp/work/sources/outdir/images.h
#   ../prevdir/alpha.png   written   -> /tmp/work/prevdir/alpha.png
```

**`--out` は入力フォルダ基準、`--preview` はカレント基準。** 単体変換では
両方ともカレント基準なので、フォルダ変換だけの挙動。

表示も紛らわしい。他の行は入力フォルダからの相対（`../prevdir/`）で出るのに、
ヘッダの行だけ `outdir/images.h` と出るので、**カレントに書かれたように読める。**

### 実害

`--check` が通らなくなる。同じ相対パスで `build` した直後に `--check` すると
**`missingOutput`** と言われる（`build` が書いた場所と `--check` が見る場所が違う）。
絶対パスにすれば直るので、`tests/image_oracle/regen.py` は絶対パスで呼んでいる。

**カレント基準に揃えてほしい**（`--preview` と、単体変換に合わせる形）。

---

## E13. GfxImageToolJs — 元画像を消しても出力が残り、`--check` が「最新」と言う {#e13}

> **2026-08-31: 解決。** 出力先にマニフェスト
> （`.gfx-image-tool-headers.json` / `.gfx-image-tool-previews.json`）を置いて
> 自分が作ったものを覚えるようになった。`--check` は `stale` と出して 2 で終了し、
> `build` は `removed` と出して消す。
>
> **消すのは自分が作ったものだけ。** 出力先に置いた `README.md`・手書きの `.h`・
> サブディレクトリはそのまま残ることを確認した。**マニフェストの中身は相対名だけ**
> （絶対パスも日時も無い）なので、commit しても機械依存にならない。


**2026-08-31 確認。** 変換済みのフォルダから元画像を 1 枚消して、もう一度
`build` しても、**その画像の出力が消えない。**

```sh
# icon.png と mono_h.png を変換したあと
rm sources/mono_h.png
gfx-image-tool build sources --out generated
#   generated/ には mono_h.h が残ったまま
#   previews/  には mono_h.png が残ったまま

gfx-image-tool build sources --out generated --check
#   -> exit 0（「最新」と言う）
```

`output_mode = split` だと**ヘッダが残る**ので実害が大きい。**消したはずの画像が
そのままスケッチに入り続ける** —— 使っていないつもりの絵にフラッシュを払っていて、
かつそれが見えない。bundle モードでもプレビューは残る。

### なぜ効くか

**生成物を commit する運用と噛み合わない。** TinyGFX 側は `generated/` と
`expected/` を commit して `--check` で古さを見ている（`tests/image_oracle/`）。
いまの `--check` は**「作られるべきものがあるか」しか見ておらず、「作られない
はずのものが残っていないか」を見ていない**ので、消し忘れが永久に残る。

### 頼みたいこと

**出力先に、どの元画像からも作られないファイルがあったら報告してほしい。**
`--check` は 2 で終了、`build` は削除するか、せめて警告を出す。

（`--check` は新しい元画像の追加は検出できている —— bundle のヘッダが変わるため。
検出できないのは「消したとき」だけ。）

## E14. GfxImageToolJs — 数字で始まる名前が `_2nd` になる {#e14}

> **2026-08-31: 解決。** `2nd.png` → `img_2ndRef`。


**2026-08-31 確認。** `2nd.png` から `_2ndRef` という記号が出る。

C++ では、**アンダースコアで始まる識別子はグローバル名前空間では処理系の予約**
（[lex.name]）。実害が出ることは滅多にないが、生成コードなので避けられる。
`img_2nd` や `n2nd` のように、**先頭にアンダースコア以外を置いてほしい。**

記号の衝突検査そのものは正しく効いていた:

```
gfx-image-tool: C symbol collision: my_icon (my icon.png and my_icon.png)
```

**両方のファイル名が出るので、どちらを直せばいいかすぐ分かる。**
`sub/icon.png` は `sub_icon` になり、階層も潰れずに済んでいる。

---

## E15. GfxImageToolJs — マニフェストが無いとき、全行 `upToDate` なのに落ちる {#e15}

> **2026-08-31: 解決。** `--check` が
> `../generated/.gfx-image-tool-headers.json  missing manifest` と**名前で出す**
> ようになり、まとめの 1 行も
> `generated output or manifest is stale, different, or missing` に変わって
> 上の行と矛盾しなくなった。`build` は作り直したうえで
> `warning: header manifest was missing; stale headers could not be detected on this build.`
> と出す —— **安全側の挙動はそのままで、黙っていないだけ**という頼んだとおりの形。


**2026-08-31 確認。** [E13](#e13) で入ったマニフェストが無い状態で `--check` すると、
**12 行すべてが `upToDate` と出たあとで失敗する。**

```
../generated/images.h  upToDate
../expected/alpha.png  upToDate  preview
   ... （全部 upToDate）...
gfx-image-tool: --check: generated output differs or does not exist
exit 2
```

**何も differ していないし、何も存在しない訳でもない。** 足りないのはマニフェスト
だけなのに、それを指す行が 1 つも出ないので、**この状態から原因にたどり着けない。**

### 踏みやすい

マニフェストは**ドットファイル**なので、`cp dir/* other/` では付いてこないし、
`.gitignore` に `.*` があると commit から漏れる。**生成物を commit する運用では
必ず一度は踏む**（TinyGFX 側は `tests/image_oracle/generated/` と `expected/` を
commit しているので、この 2 つも一緒に commit する必要がある）。

### 頼みたいこと

**足りないものを名前で出してほしい。**

```
../generated/.gfx-image-tool-headers.json  missing manifest
```

`build` のほうは**黙って作り直す**ので、そちらは実害が無い。**`--check` の
まとめの 1 行が、その上の全行と矛盾しているのが問題。**

### 安全側の挙動は正しかった

マニフェストが無いときに `build` が**知らないファイルを消さない**のは正しい
（何を作ったか分からないので）。**そこは変えなくていい** —— 「マニフェストが
無いので古いファイルは検出できない」と一言出るだけで十分。

---

## GfxImageToolJs — 依頼はすべて解決（2026-09-01 時点）

**E9〜E21 の 13 件、全部閉じた。TinyGFX 側に回避は 1 つも残っていない。**

`--preview-layout both`（利用者からの依頼で追加）も確認した。`<名前>.png` と
`<名前>.comparison.png`（元画像と並べた 2 倍幅）の両方が出て、**マニフェストが
2 つとも追跡する。**

- `both` → `converted` に戻すと `.comparison.png` が `removed` になる
- 委託済みの層と違う `--preview-layout` で `--check` すると `missingOutput` と
  `mismatch manifest` を出して 2 で終了する
- `icon.comparison.png` という名前の元画像を `icon.png` と並べて置くと
  `preview output collision: ... (icon.comparison.png and icon.png)` で止まる

**`tests/image_oracle/` は `converted`（既定）のまま。** 比較画像はテストが
読まないうえ、`sources/` と `expected/` が並んで commit されているので、
**目で見るぶんには 2 つのファイルを開けば足りる。** 生成物を倍にする理由が無い。

---

## E16. GfxImageToolJs — preview のマニフェストだけ出力先に残る {#e16}

> **2026-09-01: 解決。** `images/.gfx-image-tool/previews.json` に揃った。
> **出力先にツールのファイルは 1 つも残らない。**
>
> 旧い置き場のドットファイル（`expected/.gfx-image-tool-previews.json`）は
> 新しいツールが読みも書きもしないので残る。TinyGFX 側は消した。**ツール側に
> 移行処理は入れない** —— リリース前で、旧いバージョンの利用者がいないため。


**2026-08-31、フォルダ構成の変更後に確認。** [E13](#e13) で入ったマニフェストの
置き場が、**header と preview で分かれている。**

```
images/.gfx-image-tool/headers.json          <- 使い捨て cache。init が .gitignore に入れる
../shots/.gfx-image-tool-previews.json       <- 出力先にドットファイルで残る
```

header 側は cache に移って、**利用者が触るディレクトリから消えた。** これは
[E15](#e15) の踏み方（ドットファイルの commit 漏れ）ごと無くなるので良い変更。
**preview 側だけ元のまま**なので、そちらでは E15 と同じことが起きる。

### 効くのは preview を commit するとき

プレビューを使い捨て cache に置くなら問題にならないが、**変換結果を人が見る／
レビューする目的なら出力先は commit するディレクトリになる。** TinyGFX 側は
まさにそれで、`tests/image_oracle/expected/` が突き合わせの片側なので commit
している。そこにツールのドットファイルが 1 つ混じる。

**`images/.gfx-image-tool/previews.json` に揃えてほしい。** header と同じ理屈が
そのまま当てはまる。

### ついでに小さい話 — `[preview]` の既定

`init` が置く雛形は `layout = converted` だけ有効で `output_dir` はコメント:

```ini
[preview]
# output_dir = .gfx-image-tool/previews
layout = converted
```

この状態で `--preview-layout` を渡すと
`--preview-layout requires --preview or [preview] output_dir.` で止まる。
**コメントされた行が既定値に見える**が、実際はプレビューそのものが無効。
エラーメッセージが正確なので実害は小さい。

---

## E17. GfxImageToolJs — 生成物のどこにもツールのバージョンが無い {#e17}

> **2026-09-01: 解決。** `--json` に `"tool": {"name", "version"}` が入った。
> `build`（単体・プロジェクト）・`--check`・`inspect` の全部で出る。
> **生成バイトは変わっていない**（commit 済みヘッダを再生成して 1 バイト一致）。


**2026-09-01 確認。** 生成ヘッダにも `--json` にもマニフェストにも、
**どのバージョンが出したものかが残らない。**

### 症状

`--check` を CI で回す運用（TinyGFX がやっている形）で、こうなる:

1. A が 0.1.0 で生成して commit する
2. ツールが上がって、符号化が 1 バイト変わる
3. B の手元で `--check` が `../images.h  mismatch` で落ちる

**元画像を変えたときと、区別が付かない。** B は「誰かが画像を差し替えたのか」と
`images/` を疑いに行くが、変わっているのはツールのほうしかない。

### 頼みたいこと（軽いほうだけで足りる）

**`--json` にツール名とバージョンを入れてほしい。**

```json
{ "tool": { "name": "gfx-image-tool", "version": "0.1.0" }, ... }
```

生成バイトに影響しないので、**差分が増えない。** プロジェクト側はこれを記録
できるし、CI で固定したバージョンと突き合わせられる。

ヘッダにコメントで焼き込む案もあるが、**バージョンを上げるたび全ヘッダの差分が
動く**ので、そちらは勧めない。判断はお任せする。

### `TINYGFX_IMAGE_SPEC_VERSION` はいまのままでいい

生成ヘッダの `#if !defined(TINYGFX_IMAGE_SPEC_VERSION)` について、
**値を比べる形に変えてほしい、とは頼まない。** TinyGFX 側で検討して
**やめた**（[DECISIONS.ja.md](DECISIONS.ja.md) D35）。

理由は、**食い違いが全部いま既にコンパイルエラーになる**ため。生成ヘッダが型と
記号を名前で書いているので、C++ が先に落とす:

| | |
| --- | --- |
| 知らない形式 | `'tinygfxImageRlepal9Ops' was not declared in this scope` |
| `CellImage` に項目が増えた | `too many initializers for 'const CellImage'` |
| include 忘れ | `'CellImage' does not name a type` |

**番号が足せるものが無い。** 唯一 C++ に見えない「同じ型の項目の入れ替え」は
TinyGFX 側で型を改名して対処する。**いまの 3 行はそのままにしてほしい** ——
include 忘れのエラーを読めるものにする役目があるので。

### 併せて — npm への公開

`npm install --global gfx-image-tool` は現時点で 404。リリース前なので当然だが、
**バージョンを固定する手段がこれ待ち**になっている。TinyGFX 側は Arduino
ライブラリで `package.json` を持たないので、固定は CI のワークフローに書く形に
なる（そこにバージョンを直接書けるようになるのが公開後）。

---

## E18. GfxImageToolJs — データ長 65,535 の制限が全形式に掛かる {#e18}

> **2026-09-01: 解決。** データ長の制限は **rle565 と rlepal4 だけ**になった。
> 240x240（115,200 B）も 320x240（153,600 B）も raw565 で通る。
> **`dataLen` には 0 が入る** —— raw565 は読まないので、切り捨てた値より正しい。
>
> RLE を明示したときの断り方も良くなった。**ファイル名・形式・実際の値・上限・
> 逃げ道が 1 行に出る**:
>
> ```
> splash240.png: TinyGFX RLE data length must fit uint16_t (65535 bytes),
> but rle565 is 172125 bytes. Select format auto or raw565, or reduce the
> image dimensions.
> ```
>
> **TinyGFX 側でも実測して固定した**（`tests/image_fmt/`）。同じバイト列を
> `dataLen` 0 で描くと、raw565 と bitmap1h は **0 画素差**、rle565 は
> **616 画素差**。読む形式と読まない形式の切り分けが正しいことの裏取り。


**2026-09-01、リリース前点検で確認。** これがいちばん効く。

```sh
gfx-image-tool build splash240.png --target tinygfx --format raw565 --out x.h
#   gfx-image-tool: TinyGFX width, height, and data length must fit uint16_t.
```

**240x240 が変換できない。** これは TinyGFX が preset を同梱している
ST7789 の代表的な寸法で（`panels/ST7789_240x240.h`）、**自社の看板パネルの
全画面絵が作れない**状態。ESP32（フラッシュ数 MB）では普通にやりたいこと。

180x180 は通る（raw565 で 64,800 B）。境界は 65,535 B。

### TinyGFX 側の制限ではない

`CellImage.dataLen` は `uint16_t` だが、**読んでいるのは 5 つのうち 2 つだけ。**

| デコーダ | `dataLen` を読むか |
| --- | --- |
| `drawRle565` | **読む**（RLE の終端判定） |
| `drawRlePal4` | **読む**（同上） |
| `drawRaw565` | 読まない |
| `drawBitmap1H` | 読まない |
| `drawBitmap1V` | 読まない |

`src/TinyGFX/Image.h` の `d.len` 参照は 184 行目と 201 行目の 2 箇所だけで、
どちらも RLE のループ。**raw565 と 1bpp は寸法だけで走査する**ので、
115,200 B の raw565 を渡しても正しく描く。

ツール側は `src/target/csource.js:54` で、幅・高さ・データ長を**一括で**
弾いている（`EncodeConstraintError('TINYGFX_FIELD_OVERFLOW', ...)`）。

### 頼みたいこと

**データ長の制限を形式ごとにしてほしい** —— rle565 と rlepal4 だけに掛ける。
幅・高さの uint16 は全形式でそのままでいい。

しかも**形式選択の仕組みに載せられる。** 「rlepal4 は色数が足りないので不可」を
既に扱っているのと同じ扱いで、「この形式ではデータ長が入らないので不可」に
すればいい。全部の形式が不可なら、いまと同じく
`No allowed TinyGFX format can encode every image.` になる。

**メッセージが画像名を言わないのも直してほしい。** フォルダに 2 枚入れて
片方だけ大きいときも、どちらのことか出ない。

## E19. GfxImageToolJs — 設定の綴り間違いが全部黙って無視される {#e19}

> **2026-09-01: 解決。** 4 種類とも警告が出るようになった。
>
> ```
> warning: .imagesconfig: unknown key "fromat" in [image "icon.png"]; it was ignored.
> warning: .imagesconfig: [image "iocn.png"] did not match any input image; check the path after renaming files.
> warning: .imagesconfig: unknown key "nosuchkey" in [general]; it was ignored.
> warning: .imagesconfig: unknown section [nosuchsection]; its settings were ignored.
> ```
>
> **改名で無効になる場合をわざわざ書いてある**のが効く。


**2026-09-01 確認。** `.imagesconfig` の書き間違いが、**どれも警告なしで通る。**

| 書いたもの | 結果 |
| --- | --- |
| `[image "icon.png"]` + `format = rle565` | rle565（正しく効く） |
| `[image "icon.png"]` + **`fromat`** = rle565 | **rlepal4。無言** |
| **`[image "iocn.png"]`**（ファイル名の打ち間違い） | **rlepal4。無言** |
| `[general]` に `nosuchkey = 3` | **無言** |
| `[nosuchsection]` | **無言** |

全部 exit 0。

### なぜ効くか

**設定ファイルが再現の要**である以上、効かない設定に気づけないのは痛い。
とくに `[image "..."]` は**ファイル名を書く**ので、画像を改名した瞬間に
黙って無効になる。TinyGFX 側は `tests/image_oracle/` で 11 枚の形式を
`[image]` で固定しているので、**改名 1 回で検査の網が静かに緩む。**

キー名も当てにくい —— 画像ごとの透過設定は `alpha_mode` だが、`[alpha]`
セクションでは `mode`。**`config.js` を読んで初めて分かった。**

### 頼みたいこと

**知らないキー・知らないセクション・どの画像にも当たらない `[image]` グロブを
警告してほしい。** `warnings[]` も `warning:` 行も既にあるので、そこに乗せる
だけで足りる。落とす必要はない。

## E20. GfxImageToolJs — GUIDE の例をそのまま実行すると落ちる {#e20}

> **2026-09-01: 解決。両方。** GUIDE（en/ja）の `--check` に `--target tinygfx`
> が付いて、書いてある順に実行して通るようになった。そして本体のほうも:
>
> ```
> warning: generation setting changed: target: tinygfx -> generic-c
> warning: generation setting changed: color: {...} -> {...}
> ```
>
> **食い違ったときに、何の設定が違うかが出る。**


**2026-09-01 確認。** `docs/GUIDE.md` 184〜187（`GUIDE.ja.md` も同じ）を、
**書いてある順にそのまま実行すると失敗する。**

```sh
gfx-image-tool init ./MySketch
# Put images under ./MySketch/images/
gfx-image-tool build ./MySketch --target tinygfx     # exit 0
gfx-image-tool build ./MySketch --check              # exit 2
#   ../images.h  mismatch
#   gfx-image-tool: --check: generated output is stale, different, or missing
```

原因は、**`--target tinygfx` が build にだけ付いていて check に無い**こと。
`init` が置く設定の `target` は `generic-c` なので、`--check` は generic-c で
作り直して食い違う。README の同じ流れ（57〜60 行）は `--target` がどちらにも
無いので通る。

**入門ガイドの、しかも「再現できる」と説明している節**なので、最初に踏む。

### 直しかたは 2 つあって、両方やってほしい

1. **例を直す。** `.imagesconfig` に `target = tinygfx` を書く形にする
   （この節の主旨が再現性なので、設定に置くほうが教え方としても合っている）
2. **`--check` が設定の違いを言う。** いまは「stale, different, or missing」
   だけで、**ツールは両方の target を知っているのに黙っている。**
   `settings differ from the ones that produced this output (target: tinygfx -> generic-c)`
   のように出れば、5 秒で分かる

2 のほうが本体。**バイトは比べているのに、設定は比べていない。**

## E21. GfxImageToolJs — 最初の一歩と、最後の 1 枚 {#e21}

> **2026-09-01: 解決。両方。**
>
> ```
> Run gfx-image-tool init /tmp/bare, then move or copy the source images into
> /tmp/bare/images; images beside images/ are not scanned.
> ```
>
> 最後の 1 枚を消したときも `../images.h  removed` と掃除される。


**2026-09-01 確認。** 小さい話 2 つ。

### 画像だけ入ったフォルダを渡したとき

```sh
gfx-image-tool build ./bare --target tinygfx
#   gfx-image-tool: Image project directory not found: /tmp/bare/images.
#   Run gfx-image-tool init /tmp/bare first.
```

**「まず持っている画像フォルダを変換してみる」が最初の一歩**として自然だが、
言われたとおり `init` しても `bare/images/` が空でできるだけで、**画像は
外に残ったまま。** もう一度同じエラーになる。

`images/` に置く規約自体は文書化されているので変えなくていい。
**「画像を `images/` へ移してください」まで言ってほしい**（1 枚だけなら
`build bare/icon.png` で足りることも）。

### 最後の 1 枚を消したとき

```sh
rm images/icon.png          # 最後の 1 枚
gfx-image-tool build .      # exit 1: no matching images
ls                          # images.h が残っている
```

[E13](#e13) で入った掃除が、**N→0 のときだけ走らない**（1→0 of N は正しく
`removed` になる）。exit 1 なので見えはするが、**マニフェストは `images.h` を
自分の作ったものとして知っている**ので、消せるはず。
