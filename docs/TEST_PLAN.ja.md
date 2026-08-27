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

TinyGFX は LovyanGFX に描くのではなく**自分で SPI に喋る**ので、兄弟プロジェクトの「ホストの SDL2 で描いて PNG を撮る」手はそのまま使えない。ホストコア（`lang-ship:host`）は `SPI` 未実装、`digitalWrite` も no-op。

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

### 2.1 ホストコアに SPI スタブを足す案（後回し）

ホストコア（`host-arduino-core`）も自分たちのリポジトリなので、**転送バイトを記録する `SPI` スタブ**を足せば `TinyGFXBusSPI` **そのもの**をホストで検証できる。CS / DC の叩き順まで見られるので価値は高い。

**ただし Phase 0 では採らない。** ホストコア側のリリース待ちが TinyGFX の初期実装をブロックするため。`TinyGFXBusCapture` で先に進み、**Phase 4 以降に検討する**。それまで `TinyGFXBusSPI` はコンパイル通過（Tier 2）と実機（手動）で守る。

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

### `capture/` — Bus 差し替えのスパイク【未実装】

`TinyGFXBusCapture` が ST7789 のコマンド列から画を復元できることを確認する。
**Tier 1 を書く前提。次にやる。**

## 4. Tier 1 — 描画の正しさ

いずれも `TinyGFXBusCapture` の出力を Pillow で検証する。

| ディレクトリ | 見るもの |
| --- | --- |
| `primitive/` | 点・線・矩形・円・角丸・三角。**幅 0 / 高さ 0 / 負値 / 1px** の縮退ケースを必ず入れる |
| `clip/` | クリップ矩形の内外。画面外へのはみ出しが 1 画素も出ないこと。クリップを変えても内側の絵が変わらないこと |
| `rotation/` | 回転 0..3。**同じ図形を回転して描いた結果が、回転なしの結果を回したものと一致すること**（内部整合の不変条件。golden 画像を持たずに済む） |
| `window/` | `setAddrWindow` が出す `CASET` / `RASET` の値。**パネル原点オフセット × 回転**の 4 通りが正しいこと |
| `text/` | `drawChar` / `drawString` / `textWidth` / `setTextSize` の整数倍。戻り値が実際に描いた幅と一致すること。**AVR では PROGMEM 経由でも同じ絵になること**（D19） |
| `image/` | `pushImage` の境界、部分クリップ、transparent 版 |
| `fill/` | `writeColor` の転送画素数が**ちょうど** `w*h` であること。`TINYGFX_FILL_CHUNK` の有無で出力が 1 バイトも変わらないこと |
| `tile/` | `TinyGFXTileCanvas`: **帯の行数を変えても出力が 1 画素も変わらないこと**（LGFXVirtualCanvas の `parity` と同じ不変条件）。端数帯、`setBackgroundColor`、`setAutoClear(false)` |

**不変条件を使えるところは golden 画像を持たない。** 「回転して描く = 描いて回す」「チャンクサイズを変えても同じ」のような形にすると、期待画像のメンテが要らなくなる（LGFXVirtualCanvas の `parity` と同じ発想）。

## 5. Tier 2 — 移植性

| ディレクトリ | 見るもの |
| --- | --- |
| `build_matrix/` | 下表のとおり。**`TinyGFXBusSPI` / `TinyGFXBusSoftSPI` の実コードを守るのはここ** |
| `noalloc/` | `linkprune/` に統合済み（base との差で判定） |
| `examples_compile/` | `examples/` 全部がビルドできること |

| FQBN | `BusSoftSPI` | `BusSPI` | 備考 |
| --- | --- | --- | --- |
| `ch32-riscv-arduino:ch32riscv:CH32V003_EVT` | ○（基準機） | ✕ | SPI ライブラリが無い（E2） |
| `arduino:avr:uno` | ○ | ○（未確認） | フォントは PROGMEM 必須（D19） |
| `esp32:esp32:*` | ○ | ○ | 未着手 |
| `ch32-riscv-ug:ch32v:CH32V003`（新コア） | ○ | ○ | **CI に載せられない**（symlink + 外部ツールチェーンが要る）。E7 |

`build_matrix/` は**実行しない。コンパイルのみ。** ホストで動かせない `TinyGFXBusSPI` の型エラー・API 変更を捕まえるのが目的。

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
