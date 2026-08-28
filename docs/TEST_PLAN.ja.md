# テスト方針

内部の記録。日本語のみ。**何をどう守るか。**

たたき台。ケース一覧は実装しながら増える。

## 1. 方針

守りたいものが 2 系統ある。**どちらも同じ重みで扱う。**

| 系統 | 何を守るか | 壊れたときの見え方 |
| --- | --- | --- |
| **サイズ** | 構成ごとのフラッシュ / RAM、未使用機能が落ちること | 気づかない。ある日 V003 に載らなくなる |
| **描画** | 出力されるコマンド列と画素が正しいこと | 画面がおかしい |

サイズのほうが**壊れても気づけない**ので、先に作る。

ハーネスは兄弟プロジェクト（LGFXVirtualCanvas / PaperCanvas / BarcodeKit）と揃える。

- [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) + Arduino CLI バックエンド、`uv` で依存管理
- テスト 1 本 = 1 ディレクトリ = `<name>.ino` + `sketch.yaml` + `test_<name>.py`
- 成果物を出すテストは `output/` に書き、`conftest.py` が毎テスト前に消す
- **実機を必要とするテストは自動テストに入れない**（`manual/` に手順を置く）

## 2. 描画をホストで検証する方法

TinyGFX は LovyanGFX に描くのではなく**自分で SPI に喋る**ので、兄弟プロジェクトの「ホストの SDL2 で描いて PNG を撮る」手はそのまま使えない。ホストコア（`lang-ship:host`）は `SPI` 未実装、`digitalWrite` も no-op、`digitalRead` は常に 0。
**しかも既定バスはソフト SPI（ビットバン）で SPI クラスを経由しない**ので、
SPI クラスを足してもらうだけでは足りない（[EXTERNAL_REQUESTS.ja.md](EXTERNAL_REQUESTS.ja.md) E1）。

そこで **Bus を差し替える**。

```text
TinyGFX → Panel(ST7789) → TinyGFXBusCapture
                              |
                              +-- ST7789 のコマンド列を解釈
                              +-- RAMWR の画素を仮想 GRAM へ書き戻す
                              +-- output/<name>.ppm に吐く
                                        |
                                        v
                                   pytest 側で Pillow で比較
```

- **Bus 分離がそのままテストの差し込み口になる。** ホストコアに手を入れずに済む
- 出力は **PPM (P6)** にする。RGB565 → RGB888 して素のバイト列を書くだけで、PNG エンコーダを持ち込まなくてよい（PaperCanvas が PBM で同じことをしている）
- 素の `lang-ship:host:host` で動く。SDL2 も LovyanGFX も不要
- **`TinyGFXBusCapture` は `src/` に置く**（テスト専用ディレクトリに隠さない）。利用者が自分のパネルを検証するのにも使えるため。ただし**コアからは参照しない**（[CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) §7.4 R3）

### 2.1 本番のバス実装も検証する — `hostbus/`

ホストコアに**バス観測口**が入った（`HostArduino::setPinWriteHook` /
`SPI.setTransferHook`。[EXTERNAL_REQUESTS.ja.md](EXTERNAL_REQUESTS.ja.md) E1）ので、
`TinyGFXBusSoftSPI` / `TinyGFXBusSPI` **そのもの**をホストで検証できるようになった。

```text
TinyGFX → PanelST7789 → 本番の Bus → 線
        → TgfxPinProbe / TgfxSpiProbe（TinyGFX 側の模型）→ 仮想 GRAM → PPM
```

模型は `tests/common_libs/tgfx_test/src/tgfx_host_probe.h`。SCK の立ち上がりで
MOSI をシフトインし、DC を読んでコマンドとデータを分け、`TinyGFXBusCapture` へ流す。
**コアは ST7789 を知らない。** 知っているのはこちら側だけ。

これで守れるようになったもの: ビット順、DC を落とすタイミング、
トランザクション中の CS、`SPISettings`（クロック・モード）、
**ソフト SPI とハードウェア SPI が同じ絵を出すこと**。

**観測口はまだ未リリース。** `sketch.yaml` は platform をバージョン無しで宣言して
sketchbook の作業コピーを見ており、観測口の無いコアでは skip する。
リリースされたらバージョン付きに戻す。

## 3. Tier 0 — 土台。**実装済み・通っている**

スケッチを**実行しない**。ビルドしてサイズとシンボル表を見るだけなので `dut` は使わない。
共通ヘルパは `tests/tinygfx_build.py`、構成スケッチは `tests/constructs/<name>/`。

### `linkprune/` — 「まわりまわって載る」の検出【通過: 11 件】

構成ごとに「載っていてはいけない名前」が最終バイナリに無いことを `nm` で見る。
表は [FOOTPRINT.ja.md](FOOTPRINT.ja.md) §8。

例: 構成 A（`fillScreen` のみ）に `drawCircle` / `drawChar` / フォントデータ /
`TileCanvas` / `TinyGFXPrint` のいずれかが出たら fail。

**判定は base との差で行う。** `_malloc_r` のようにコア側が最初から持ち込んでいるものを
TinyGFX のせいにしないため。浮動小数点・除算ルーチン（`__udivsi3` 等）も同じ方法で見ている。

**これが TinyGFX の設計そのもののテスト。** [CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) §7.4 の
R1〜R9 を破ると、まずここが落ちる。

> インライン化されて名前が消えるのでは、という懸念は実測で否定された。
> `-Os` でも `_ZN7TinyGFX10drawCircleEssst` のような weak シンボルが残るので判定できる。
> ただし**完全にインライン化された場合は名前が消える**ので、`footprint/` の
> サイズ判定と併せて 2 重に見る。

### `footprint/` — サイズの回帰【通過: 2 件】

構成 base..T をビルドし、base からの増分が予算内かを見る。**数値は常に出力する**
（予算内でも増分が見えるように）。P1 / P2 は参考値。

`test_float_does_not_fit_on_the_reference_board` は「float 版が基準機に載らないこと」を
**記録するためのテスト**。載るようになったら skip して値を採り直せと言う。

### 基準機の切り替え

既定は `ch32-riscv-arduino:ch32riscv:CH32V003_EVT`。開発中の新コアで測るときは:

```sh
TINYGFX_FQBN='ch32-riscv-ug:ch32v:CH32V003:pnum=CH32V003F4P6' uv run pytest footprint -s
```

**予算表は 1 本で、どちらのコアでも通ること**を条件にしている
（[FOOTPRINT.ja.md](FOOTPRINT.ja.md) §6.1）。

## 4. Tier 1 — 描画の正しさ【**実装済み・通っている**】

`lang-ship:host:host` でホスト実行する。SDL2 も LovyanGFX も要らない。
スケッチが `output/` に成果物を書き、pytest がそれを読む。

| ディレクトリ | 見ているもの | 状況 |
| --- | --- | --- |
| `capture/` | **土台。** `TinyGFXBusCapture` が ST7789 のコマンド列から画を復元できること。転送画素数がちょうど一致すること、ウィンドウの値、原点オフセット、`startWrite`/`endWrite` の釣り合い | 通過 |
| `window/` | 回転 0..3 の MADCTL・幅高さの入れ替え・オフセットの導出（135x240 / GRAM 240x320 を模す）。オフセット無しなら全回転で 0 のままであること | 通過 |
| `fontchain/` | **CellFont の連鎖と U+FFFD 退避。** 手で組んだ小さな CellFont で、生成フォントでは踏めない道を通す — 前段に豆腐があっても後段の字に到達すること、ベースラインが連鎖先頭の ascent で決まること、`first` より小さいコードがしっぽに居る疎索引（仕様 §7.1 / §7.2 / §8 / §15.2）。**わざと壊すと両方落ちることを確認済み** | 通過 |
| `ili9342/` | ILI9342C の MADCTL・色順（BGR ビット）・`setMirror` の XOR。両軸ミラーが回転 2 と一致すること。オフセットが素通しであること。**表が実機で正しいかは M0 で確かめる**（D22） | 通過 |
| `primitive/` | 全プリミティブ。端の 1 画素、枠と塗りの違い、**縮退ケース 10 通りが 1 画素も送らないこと** | 通過 |
| `clip/` | **不変条件。** クリップ内はクリップ無しと 1 画素も違わず、外は 1 画素も触られないこと。画面より大きいクリップ、空のクリップ | 通過 |
| `fill/` | 転送画素数の過不足を 11 ケースで固定。クリップ後・回転後も含む | 通過 |
| `tile/` | **不変条件。** 帯の行数（1/2/3/5/7/8）を変えても直接描画と 1 画素も違わないこと。端数帯、バッファ不足、`setAutoClear(false)` | 通過 |
| `text/` | `drawString` の戻り値が `textWidth` と一致すること、はみ出さないこと、倍角、収録外の文字、背景色つきのセル塗り、透過 | 通過 |
| `image/` | `pushImage` の配置、四隅の切り取り、クリップとの重なり、transparent 版、画面外 | 通過 |
| `i2c/` | **I2C + モノクロ OLED**（SSD1306）。ホストの Wire 観測フックで本番の `TinyGFXBusI2C` が流したバイトを拾い、SSD1306 の模型で組み立て直す。`display()` まで転送されないこと、変更のあったページだけ流れること、回転 | 通過 |
| `u8g2/` | u8g2 形式のデコーダが LGFXFontToolJs の描いた絵と一致すること（ASCII / CJK） | 通過 |
| `hostbus/` | **本番の Bus 実装**（§2.1）。ソフト SPI とハードウェア SPI が同じ絵を出すこと、ビット順・クロック・モード、アイドル時の CS / DC | 通過 |

**期待画像は持たない。** 「クリップ内 == クリップ無し」「帯を変えても同じ」のような
**不変条件**にしてあるので、絵を変えてもテストは壊れない
（LGFXVirtualCanvas の `parity` と同じ考え方）。

### 検証の道具は 2 つ

- **`TinyGFXBusCapture`** — パネルが出すバイト列を解釈して仮想 GRAM に書き戻す。
  「実際に何を送ったか」まで見える。転送画素数の検査はこれでしかできない
- **`TinyGFXPanelMemory`** — RAM バッファへ直接書くパネル。`TileCanvas` の帯バッファと
  同じもの。コマンド列の解釈を挟まないぶん単純

### 共通部品

- `tests/common_libs/tgfx_test/` — PPM 出力と `output/report.txt` への値の記録
- `tests/common_libs/tgfx_font/` — つなぎのフォント。**ライブラリには同梱しない**（D17）
- `tests/tgfx_check.py` — `report()` / `image()` / `lit()` / RGB565→RGB888 変換

**値はシリアルではなくファイルで受け渡す。** `dut.expect` の取りこぼしでテストが
不安定になるため。シリアルには `TEST start` / `TEST done` / `SCENE <name>` の進行だけ流す。

## 5. Tier 2 — 移植性【**実装済み・通っている**】

`build_matrix/` が `examples/` をそのままビルドする。**実行しない。コンパイルのみ。**
ホストで動かせない `TinyGFXBusSPI` / `TinyGFXBusSoftSPI` の型エラーや API 変更を
捕まえるのが目的。

examples は `sketch.yaml` にプロファイル（`ch32v003` / `uno` / `esp32` / `m5stack`）を持つので、
`--fqbn` ではなく `--profile` でビルドする（プロファイルがあるスケッチに
`--fqbn` を渡すと「そのプラットフォームは宣言されていない」と怒られる）。

| プロファイル | 対象 | 実測 flash / RAM |
| --- | --- | --- |
| `ch32v003` | HelloWorld / Shapes / FlickerFree | 8,940 / 576、10,920 / 576、9,076 / 1,112 |
| `uno` | 上記 + HardwareSPI | 4,732 / 107、5,926 / 111、4,814 / 646、3,512 / 108 |
| `esp32` | HelloWorld のみ（重いわりに他で拾えない問題が少ない） | 260,516 / 22,172 |

**`HardwareSPI` は CH32V003 では対象外。** そのコアに SPI ライブラリが無いため
（[EXTERNAL_REQUESTS.ja.md](EXTERNAL_REQUESTS.ja.md) E2）。
`test_hardware_spi_still_fails_on_ch32` が「まだ通らない」ことを記録していて、
通るようになったら気づけるようにしてある。

`noalloc` は `linkprune/` に統合した（base との差で判定するほうが正確なため）。

## 6. 手動テスト（実機）

`docs/MANUAL_TEST.ja.md` に手順を書く。自動テストには含めない。

| # | 内容 | 機材 |
| --- | --- | --- |
| M1 | CH32V003 + ST7789 240x240 で構成 B が正しく出る | 実機 |
| M2 | 回転 0..3 が実機で正しい向き（オフセット込み） | 実機 |
| M3 | ESP32 で同じスケッチがピン定義変更だけで動く | 実機 |
| M4 | Fast Backend 有無で見た目が変わらない | 実機（Phase 5） |

**M1 と M2 は Phase 1 / 2 の完了条件。** ホストのテストが通っていても、実機で初めて分かる（初期化列の待ち時間、SPI モード、オフセット）ものがある。

## 7. CI

`.github/workflows/tests.yml`。兄弟プロジェクトと同形。

1. arduino-cli セットアップ、必要なコアのインストール
2. `uv sync`
3. `uv run pytest -v --html=report.html --self-contained-html`
4. `output/**` と `report.html` を artifact に上げる
5. **フットプリントの数値をジョブサマリに出す**（増分がプルリクで見えるように）

SDL2 は不要（LovyanGFX に依存しないため）。

**要確認**: CH32 コアの platform index が CI から取得できるか。取れなければ Tier 0 の CH32 部分はローカル専用にする。

## 8. ディレクトリ規約

```text
tests/
  pyproject.toml        uv 管理。pytest / pytest-embedded / pytest-embedded-arduino-cli / pytest-html / pillow
  uv.lock
  conftest.py           output/ の掃除
  .env.example
  README.md / README.ja.md
  <name>/
    <name>.ino
    sketch.yaml         profiles: host（lang-ship:host:host）／compile 用に fqbn 別プロファイル
    test_<name>.py
    output/             生成物（.gitignore）
```

`sketch.yaml` のライブラリ指定は開発中 `dir: ../../`。リリース時にワークフローが `TinyGFX (x.y.z)` へ書き換える。
