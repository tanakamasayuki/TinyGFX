# テスト

> English: [README.md](README.md)

TinyGFX のテスト一式。方針とケース一覧は [../docs/TEST_PLAN.ja.md](../docs/TEST_PLAN.ja.md)。

- [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) + Arduino CLI バックエンド、`uv` で依存管理
- **Tier 0（`footprint/` / `linkprune/`）はスケッチを実行しない。** ビルドしてサイズと
  シンボル表を見るだけなので `dut` を使わない。実機も要らない
- **Tier 1 は `lang-ship:host` 上でホスト実行**し、描いた結果を
  画素で検証する。SDL2 も LovyanGFX も要らない

## 動かす

```sh
uv sync
uv run pytest -v -s          # 全部（-s でフットプリントの表が出る）
uv run pytest linkprune -v   # 1 つだけ
```

初回は arduino-cli がコアを取りにいくので時間がかかる。

**同じスケッチを並列にビルドしない。** arduino-cli はビルドディレクトリを
**スケッチのパスで決める**（`~/.cache/arduino/sketches/<hash>`）ので、
別プロファイルであっても同じスケッチを同時にビルドすると互いのファイルを壊す。
`Image does not have a valid ELF header` や `partitions.csv が無い` はこれ。
pytest を走らせている間に手で `arduino-cli compile` しないこと。

**Tier 0 には CH32 のコアが要る。**入っていなければ skip される。

```sh
arduino-cli core install ch32-riscv-arduino:ch32riscv
```

開発中の新コアで測るときは環境変数で切り替える:

```sh
TINYGFX_FQBN='ch32-riscv-ug:ch32v:CH32V003:pnum=CH32V003F4P6' uv run pytest footprint -s
```

## 構成

```text
tests/
  tinygfx_build.py    arduino-cli を叩いてサイズとシンボルを取る共通ヘルパ
  constructs/         測定用スケッチ。base / a..e / t / p1 / p2（docs/FOOTPRINT.ja.md §4）
  tgfx_check.py       output/ に残ったものを読む小道具（report / image / lit / 色変換）
  common_libs/
    tgfx_test/        PPM 出力と report.txt への値の記録
    tgfx_font/        つなぎのフォント。ライブラリには同梱しない
  footprint/          サイズの回帰。base からの増分が予算内か
  linkprune/          未使用機能が最終バイナリに残っていないか
  capture/            BusCapture が ST7789 のコマンド列から画を復元できるか（Tier 1 の土台）
  window/             ST7789 の回転 MADCTL・幅高さ・原点オフセットの導出
  ili9342/            ILI9342C の MADCTL・色順（BGR）・ミラー
  primitive/          全プリミティブと縮退ケース
  clip/               クリップ内はクリップ無しと同じ、外は無傷（不変条件）
  fill/               転送画素数の過不足
  tile/               帯の行数を変えても直接描画と同じ（不変条件）
  text/               文字。戻り値・倍角・収録外・背景色つき・透過
  fontchain/          CellFont の連鎖・U+FFFD 退避・ベースライン揃え
  utf8/               UTF-8 の復号。列ごとにコードポイントと消費バイト数を直接見る
  image/              pushImage の配置・切り取り・透過・バイト順の入れ替え
  image_fmt/          同じ絵をどの形式で符号化しても 1 画素も違わないこと
  image_oracle/       変換ツールの出力を、ツール自身の期待画像と突き合わせる
  hostbus/            本番の SPI バスが実際に流したバイトを拾って画に戻す
  fillchunk/          まとめ書きを有効にしても線に出るバイトが変わらないこと
  clifont/            **本番の生成器が出した CellFont** を描けること
  scene/              実機と突き合わせるゴールデンをホストで作る
  hw/m5stack/         **実機（Tier 3）。** .env を渡したときだけ走る
  u8g2/               u8g2 形式フォントのデコード
  i2c/                I2C + SSD1306（モノクロ・ページ転送・dirty ページ・寸法契約）
  monospi/            SPI + SSD1306 / SH1106。トランザクションと CS の作法
  softi2c/            ビットバン I2C の波形を復号してバイト列に戻す
  sh1106/             SH1106 の配線。SSD1306 と同じ絵になること
  build_matrix/       examples が ch32v003 / uno / esp32 / m5stack でビルドできるか（実行はしない）
  manual/m5stack/     **実機検証スケッチ。** pytest では走らない（ビルドだけ検査する）
```

## 値の受け渡し

スケッチは `output/report.txt` に `key=value` を書き、pytest は `tgfx_check.report()`
でそれを読む。**シリアルには進行（`TEST start` / `SCENE <name>` / `TEST done`）だけ流す。**
`dut.expect` で値まで拾うとテストが不安定になるため。

## 期待画像は持たない

`clip/` と `tile/` は**不変条件**で書いてある — 「クリップ内はクリップ無しと 1 画素も
違わない」「帯の行数を変えても直接描画と同じ」。シーンを変えてもテストが壊れないので、
期待画像のメンテが要らない。

## `linkprune/` が見ているもの

**未使用機能が落ちること自体はリンカ（`--gc-sections`）の仕事。** ここで見るのは
**落ちるはずのものが参照の連鎖で残っていないか**。

たとえば構成 A（`fillScreen` しか呼ばないスケッチ）のバイナリに `drawCircle` や
フォントデータや `Print` が 1 つでも出たら fail する。落ちた場合は
[../docs/CORE_DESIGN.ja.md](../docs/CORE_DESIGN.ja.md) §7.4 の R1〜R9 のどれかを破っている。

判定は **base（空スケッチ）との差**で行う。`_malloc_r` のようにコアが最初から
持ち込んでいるものを TinyGFX のせいにしないため。

## `footprint/` が見ているもの

構成ごとの Flash / RAM が、base からの増分で予算内か。**予算内でも数値は必ず出す**ので、
`-s` を付けて増分を眺めるのが正しい使い方。実測は
[../docs/FOOTPRINT.ja.md](../docs/FOOTPRINT.ja.md) §5 に記録する。

`test_float_does_not_fit_on_the_reference_board` は「`println(float)` が CH32V003 に
**載らない**こと」を記録するテスト。載るようになったら skip して、値を採り直せと言う。
