# テスト

TinyGFX のテスト一式。方針とケース一覧は [../docs/TEST_PLAN.ja.md](../docs/TEST_PLAN.ja.md)。

- [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) + Arduino CLI バックエンド、`uv` で依存管理
- **Tier 0（`footprint/` / `linkprune/`）はスケッチを実行しない。** ビルドしてサイズと
  シンボル表を見るだけなので `dut` を使わない。実機も要らない
- Tier 1（これから）は `lang-ship:host` 上でホスト実行し、描いた結果を画素で検証する

## 動かす

```sh
uv sync
uv run pytest -v -s          # 全部（-s でフットプリントの表が出る）
uv run pytest linkprune -v   # 1 つだけ
```

初回は arduino-cli がコアを取りにいくので時間がかかる。

**Tier 0 には CH32 のコアが要る。**入っていなければ skip される。

```sh
arduino-cli core install ch32-riscv-arduino:ch32riscv
```

## 構成

```text
tests/
  tinygfx_build.py    arduino-cli を叩いてサイズとシンボルを取る共通ヘルパ
  constructs/         測定用スケッチ。base / a..e / t / p1 / p2（docs/FOOTPRINT.ja.md §4）
  fonts/              つなぎのフォント（tools/gen_font.py が生成）。ライブラリには同梱しない
  footprint/          サイズの回帰。base からの増分が予算内か
  linkprune/          未使用機能が最終バイナリに残っていないか
```

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
