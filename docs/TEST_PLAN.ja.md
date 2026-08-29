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

### `linkprune/` — 「まわりまわって載る」の検出

構成ごとに「載っていてはいけない名前」が最終バイナリに無いことを `nm` で見る。
表は [FOOTPRINT.ja.md](FOOTPRINT.ja.md) §9。

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

### `footprint/` — サイズの回帰

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
| `window/` | 回転 0..3 の MADCTL・幅高さの入れ替え・オフセットの導出（135x240 / GRAM 240x320 を模す）。オフセット無しなら全回転で 0 のままであること。**`setOffset()` / `setGramSize()` が呼び出し順に依存しないこと** — どちらが先でも、`begin()` の前後どちらでも効く（2026-08-29 のレビューの P0） | 通過 |
| `clifont/` | **本番の生成器（LGFXFontToolJs の CLI）が出した CellFont を描けること。** つなぎの `gen_font.py` ではなく配布される形のヘッダをそのまま食わせる。可変ピッチ・頭ブロック・**`first` より小さいコードがしっぽに居る**（仕様 §7.1 の落とし穴）を 1 本で踏む | 通過 |
| `scene/` | 実機と突き合わせるゴールデンをホストで作って凍結する。**飽和した色しか使っていないこと**も検査する（読み戻しの往復でずれないため） | 通過 |
| `fillchunk/` | **`TINYGFX_FILL_CHUNK` は速さだけを変える。** まとめ書きを持たないソフト SPI を基準に、有効にした `TinyGFXBusSPI` と絵・転送画素数・線に出たバイト数が完全一致すること。M3 の「FILL_CHUNK を付けても絵が変わらない」を実機を待たずに押さえる。**詰め直しを外すと 885 画素壊れて落ちることを確認済み** | 通過 |
| `fontchain/` | **CellFont の連鎖と U+FFFD 退避。** 手で組んだ小さな CellFont で、生成フォントでは踏めない道を通す — 前段に豆腐があっても後段の字に到達すること、ベースラインが連鎖先頭の ascent で決まること、`first` より小さいコードがしっぽに居る疎索引（仕様 §7.1 / §7.2 / §8 / §15.2）。**わざと壊すと両方落ちることを確認済み** | 通過 |
| `ili9342/` | ILI9342C の MADCTL・色順（BGR ビット）・`setMirror` の XOR。両軸ミラーが回転 2 と一致すること。オフセットが素通しであること。**表が実機で正しいかは M0 で確かめる**（D22） | 通過 |
| `primitive/` | 全プリミティブ。端の 1 画素、枠と塗りの違い、**縮退ケース 10 通りが 1 画素も送らないこと** | 通過 |
| `clip/` | **不変条件。** クリップ内はクリップ無しと 1 画素も違わず、外は 1 画素も触られないこと。画面より大きいクリップ、空のクリップ。**極端な座標** — `x + w - 1` が `int16_t` で桁溢れしても契約どおりクリップすること（2026-08-29 のレビューの P1） | 通過 |
| `fill/` | 転送画素数の過不足を 11 ケースで固定。クリップ後・回転後も含む | 通過 |
| `tile/` | **不変条件。** 帯の行数（1/2/3/5/7/8）を変えても直接描画と 1 画素も違わないこと。端数帯、バッファ不足、`setAutoClear(false)` | 通過 |
| `text/` | `drawString` の戻り値が `textWidth` と一致すること、はみ出さないこと、倍角、収録外の文字、背景色つきのセル塗り、透過 | 通過 |
| `utf8/` | **文字列は UTF-8**（D26）。`TinyGFX::nextCode` を画素ではなく**コードポイントと消費バイト数**で直接見る — 正しいコードポイントを返しても消費が違えば後ろが全部ずれるので、2 つ目の数字も同じだけ重要。1/2/3 バイト、U+FFFF 超（**4 バイト食って notdef**）、途中で切れた列（**終端を飛び越えないこと**）、先導のない継続バイト、冗長符号化。`drawString` と `TinyGFXPrint` が同じ絵になること（Print は 1 バイトずつ来るので状態を持つ）、列の途中の改行。**`TINYGFX_FONT_UTF8=0` にすると落ちることを確認済み** | 通過 |
| `image/` | `pushImage` の配置、四隅の切り取り、クリップとの重なり、transparent 版、画面外 | 通過 |
| `i2c/` | **I2C + モノクロ OLED**（SSD1306）。ホストの Wire 観測フックで本番の `TinyGFXBusI2C` が流したバイトを拾い、SSD1306 の模型で組み立て直す。`display()` まで転送されないこと、変更のあったページだけ流れること、回転。**寸法とバッファの契約** — 128x32 で多重比と COM ピンが変わること、null バッファ・8 の倍数でない高さ・寸法 0 で `begin()` が false を返すこと | 通過 |
| `monospi/` | **SPI に繋いだページ方式パネル**（SSD1306 / SH1106）が、1 バイトも `SPI.beginTransaction()` の外に出さず、CS を落とした状態で出すこと。SPISettings、アイドル時の CS、終了時にトランザクションが残っていないこと。**I2C では見えない層** — 2026-08-29 のレビューで見つかった P0 の再発防止 | 通過 |
| `image_fmt/` | **同じ絵をどの形式で符号化しても 1 画素も違わないこと。** 生 RGB565 / RLE / RLE+パレット / 1bpp 横 / 1bpp 縦。**どれが選ばれたかがスケッチから見えてはいけない**（`tests/text/` の「3 通りの符号化が同じ画素を描く」と同じ考え方）。クリップ、画面外、透過も。**`raw565` は 1 行に窓を 1 つしか開かないこと** —— 写真は連長が 1 なので、連ごとに `fillRect` を呼ぶと画素ごとに窓が開き、1 画素 13 バイトになる（実測 3,072 コマンド／1,024 画素）。窓の数が行数と一致すること、はみ出しとクリップでも画面外に書かないことを固定。**元の実装に戻すと落ちることを確認済み** | 通過 |
| `image_oracle/` | **変換ツールの出力が、ツール自身の期待画像と 1 画素も違わないか。** `pairs/` に `<名前>.h` と `<名前>.ppm`（**変換後**の画素）を置くと、収集時に `gen_sketch.py` がスケッチを組み立てて描き、突き合わせる。**ペアを足すのにコードは書かない。** GfxImageToolJs 仕様書 §15.2 が指定するオラクル —— 自作の encode と decode の往復では、両者が同じ勘違いをしていたら一致してしまうので、期待画像は変換側から別途もらう | 通過 |
| `sh1106/` | **SH1106 の配線。** 132 カラム RAM のオフセット、ページごとのカーソル設定（0x21 / 0x22 が使えない）。**同じ絵を SSD1306 で描いて 1 ビットも違わないこと**、ガラスの外に書かないこと、`setColumnOffset()` が効くこと。**縦詰めの速い経路** `pushVBitmap()` が汎用の `drawImage()` と 1 ビットも違わず、非整列・パネル外・回転中は描かずに false を返すこと | 通過 |
| `u8g2/` | u8g2 形式のデコーダが LGFXFontToolJs の描いた絵と一致すること（ASCII / CJK） | 通過 |
| `hostbus/` | **本番の Bus 実装**（§2.1）。ソフト SPI とハードウェア SPI が同じ絵を出すこと、ビット順・クロック・モード、アイドル時の CS / DC | 通過 |

**テストの件数はここに書かない。** 増えるたびに古くなるので、`uv run pytest` の
出力を見ること。

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

## 4.5 Tier 3 — 実機【**通っている**】

`tests/hw/m5stack/` が **M5Stack Core / BASIC を標準の検証機**として使う。

**実機の上で描いた絵を、ホストで作ったゴールデンと 1 画素も違わないか見る。**
ホストのテストが原理的に守れないのはここ — 実機のコンパイラ、実機の `int` 幅、
実機の PROGMEM。**AVR でフォント構造体が PROGMEM に載っていなかった不具合
（D19）はまさにこの類**で、ホストでは絶対に出なかった。

| | |
| --- | --- |
| 絵の取り方 | 実機の上で `TinyGFXBusCapture` に描いて artifact で送る |
| ゴールデン | `tests/scene/golden/scene.ppm`。**ホストで作ったものが正** |
| シーンの定義 | `tests/common_libs/tgfx_test/src/tgfx_scene.h` の 1 箇所だけ |
| 同期 | ArduTest の HELLO。**シリアルの頭は取りこぼす**ので握手が要る |
| 入口 | `.env` を渡したときだけ走る。素の `uv run pytest` は実機を焼かない |

```sh
cp .env.example .env     # 自分のポートを書く
uv run --env-file .env pytest hw --profile m5stack
```

**ゴールデンを実機の出力から作らない。** 実機がおかしくても「一致」してしまう。
ホストで描いたものを正とし、実機がそれに合わせる。

**線から先は範囲外。** パネルの GRAM 読み戻しができれば線の先まで見えるが、
M5Stack では再現できていない（[MANUAL_TEST.ja.md](MANUAL_TEST.ja.md) の「読み戻し」）。

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

### `build_matrix/test_headers.py` — ヘッダが単体で成立するか

**すべての公開ヘッダ（25 本）が、すべての対象コアで `<TinyGFX.h>` の後に
1 本だけ足してビルドできること。** 実行しない。

examples を回す `test_example_builds` と補い合う。あちらは「組み合わせが
動くか」、こちらは「部品が単体で成立するか」。これが無いと 2 つ抜ける。

- **別のサブヘッダにうっかり依存していないか。** `Progmem.h` を切り出した
  ときがまさにその形だった（パネルが `tinygfx_rd8` を使うのに `Font.h` 経由で
  しか手に入らなかった）
- **ホスト以外で通るか。** ホストテストは `lang-ship:host` でしか動かさない。
  新しいヘッダはそこだけ通って満足しがちで、AVR の PROGMEM や CH32V003 の
  16 ビット int で初めて壊れる

**通過: 25 本 × 3 コア**（CH32V003 はコアに SPI が無いので `BusSPI.h` のみ skip）。

**なぜ `<TinyGFX.h>` を先に足すのか。** サブヘッダだけを include しても
arduino-cli はライブラリを引かない（ライブラリ名と同じ `TinyGFX.h` を見て
解決するため。`library.properties` の `includes=TinyGFX.h` がそれ）。
**それが実際の契約なので、テストもそれに合わせている。**

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
