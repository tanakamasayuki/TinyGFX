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
| [E8](#e8) | LGFXFontToolJs | **`--format u8g2` の C 出力が LovyanGFX 依存。** cellfont と揃えてほしい | 中 | しない |
| [E5](#e5) | arduino-library-release-toolkit | リリース資産の同期（既に取り込み済み） | 低 | しない |
| [E6](#e6) | 外部レジストリ | ライブラリ名 `TinyGFX` の重複確認 → **確認済み・問題なし** | 済 | 解消 |

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
TinyGFXPanelST7789 panel(bus, 240, 240);        // 本番と同じパネル
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
TinyGFX → PanelST7789 → 本番の TinyGFXBusSoftSPI / TinyGFXBusSPI → 線
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

## E8. LGFXFontToolJs — `--format u8g2` の C 出力を描画器に依存しない形に {#e8}

**優先度: 中。** これが無いと TinyGFX は u8g2 のフォントを CLI から受け取れない。

### 何が起きているか

`--format u8g2` の C 出力は、バイト列に加えて **LovyanGFX の型を宣言する。**

```c
// Include LovyanGFX (or M5GFX / M5Unified) before this header so lgfx::U8g2font is available.
static const uint8_t u8g2Cjk_data[103] LGFXFT_PROGMEM = { ... };
static const lgfx::U8g2font u8g2Cjk(u8g2Cjk_data);   // <- ここ
```

**LovyanGFX が無いとコンパイルが通らない。** TinyGFX には u8g2 のデコーダがあるので
バイト列だけあれば使えるのだが、この 1 行のためにヘッダをそのまま置けない。

### 依頼

**`--format cellfont` と揃えてほしい。** cellfont の出力は
「バイト列と `CellFont` 構造体だけ、描画器の型は 1 つも出さない」形になっていて、
利用者が 1 行包む。u8g2 だけ流儀が違う。

```cpp
// TinyGFX ならこう包む。cellfont と同じ手数
static const TinyGFXFontRef myFont = {u8g2Cjk_data, &tinygfxFontU8g2Ops, nullptr};
```

LovyanGFX の利用者も 1 行増えるだけなので、**同じツールの中で形式ごとに
流儀が違うことのほうが分かりにくい**と思う。

外し方はいくつかある。判断はそちらで:

| 案 | |
| --- | --- |
| **常に出さない**（cellfont と同じ） | 一番きれい。LovyanGFX 利用者に 1 行増える |
| `#if __has_include(<LovyanGFX.hpp>)` で包む | 既存の利用者は無変更。ただし include 順に依存する |
| フラグで選ぶ（`--no-helper` など） | 確実だが、既定をどちらにするかの問題が残る |

### バイト列そのものは一致している

`lgfxJapanGothic_8` の `"0123456789ABCabc"` を CLI と手元の生成器の両方で出して、
**163 バイトが完全一致**することを確認した（2026-08-28）。形式の実装に問題は無い。

### それまでどうしているか

`tools/gen_u8g2_ref.mjs` が**CLI と同じ形**（`<name>_data` のバイト列だけ）で
`tests/u8g2/` のヘッダを出している。**これはつなぎ**で、E8 が通ったら
CLI の出力に差し替えて生成器からヘッダ出力を消す。
参照画像（`.ref.txt`）だけは引き続きこの生成器が要る。

### ついでに 1 点 — `CELLFONT_PROGMEM` を使ってほしい

cellfont の出力は `LGFXFT_PROGMEM` を自前で定義して（`PROGMEM` から）使っている。
仕様 §12.1 は **`CELLFONT_PROGMEM` を描画器が提供する**と決めていて、
描画器がそこにセクション属性などを入れたときに効かなくなる。
いまは AVR で `PROGMEM` に展開されるので動いているが、**仕様どおりなら
`CELLFONT_PROGMEM` を使うほうが安全。** 優先度は低い。
