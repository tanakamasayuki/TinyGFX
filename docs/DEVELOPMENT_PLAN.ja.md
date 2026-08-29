# 開発計画

内部の記録。日本語のみ。**現在地と残作業。**

## 1. 現在地

**フェーズ 0〜4 の実装、Tier 0〜2 の検証、examples まで完了。残るのはリリース前の設計修正、実機確認、利用者向けドキュメント。**

| 項目 | 状況 |
| --- | --- |
| コンセプトのたたき台（`memo.ja.md`） | **削除済み**（2026-08-28）。12 項目すべてこの docs へ移した |
| 要件・設計・決定の文書化 | **一巡した。** 実測で D2 / Q7 が決着 |
| 外部への依頼 | **[EXTERNAL_REQUESTS.ja.md](EXTERNAL_REQUESTS.ja.md) に E1〜E6 として固めた** |
| リポジトリ整備 | **完了**（§4） |
| 描画コア `src/TinyGFX/Gfx.h` | **実装済み。** 全プリミティブ + クリップ + 回転 + 画像 + 文字 |
| Bus | `BusSoftSPI`（既定）/ `BusSPI` / `BusI2C` / `BusCapture` **実装済み** |
| Panel | `PanelST7789` / `PanelILI9341` / `PanelILI9342` / `PanelSSD1306` / `PanelSH1106` / `PanelMemory` **実装済み** |
| 帯レンダリング `TileCanvas.h` | **実装済み**（D16） |
| `Print.h`（拡張） | **実装済み** |
| フォント | **CellFont v1 へ移行（2026-08-28、D17 再改訂）。** 形式の仕様は LGFXFontToolJs 側に切り出し、TinyGFX はその描画器の 1 実装になった。頭ブロック・形式内連鎖・U+FFFD 退避・ベースライン基準を取り込んで **+248 B**（[FONT_FORMAT.ja.md](FONT_FORMAT.ja.md)） |
| `tests/linkprune/` | **通っている。** 未使用プリミティブ、float、除算、未使用フォント形式を検査 |
| `tests/footprint/` | **通っている（2 件）**。実測は [FOOTPRINT.ja.md](FOOTPRINT.ja.md) §5 |
| Tier 1（描画の正しさ） | **現行テストはすべて通っている。** 描画、画像、文字、フォント、カラー/モノクロパネル、Bus、帯レンダリングをホスト実行で検査 |
| 本番の Bus 実装の検証 | **完了。** ホストコアのバス観測口（E1）で `BusSoftSPI` / `BusSPI` を通しで検証できるようになった |
| 回転オフセットの導出 | `setGramSize()` を追加。135x240 のような GRAM より小さいパネルで回転 2/3 がずれる問題を修正 |
| Tier 2（移植性のコンパイル）| **完了。`build_matrix/` の 11 ビルドが通過**（ch32v003 / uno / esp32 / m5stack × examples） |
| AVR（Uno R3）対応 | **実測で全構成が載る。** フォントを PROGMEM から読むようにした（D19） |
| 開発中の新コアでの確認 | **ハードウェア SPI がリンクでき、base が 624 B**（[FOOTPRINT.ja.md](FOOTPRINT.ja.md) §6.1） |
| ライブラリ名の確認 | **完了。3 レジストリとも競合なし**（E6） |
| 実機確認 | **M0 通過（2026-08-28、M5Stack BASIC）。初めて実機に絵が出た。** 既定の MADCTL / 色順が正しかった。反転は世代差あり（D22）。**M0b / M1〜M4 は未** |
| examples | **完了。6 本**（HelloWorld / Shapes / FlickerFree / HardwareSPI / OledI2C / M5StackBasic）。日英 README つき |
| 利用者向けドキュメント | `README.md` / `README.ja.md` は**あり**。`docs/GUIDE` / `docs/API` は未着手（実機の後） |

### 1.1 リリース前 TODO（2026-08-29 設計レビュー）

細かな速度・サイズ最適化より先に、公開 API と層の境界を固める。コアの
`TinyGFX → Panel → Bus` という構成は維持し、次の局所修正と回帰テストを行う。

| 優先度 | 項目 | 完了条件 |
| --- | --- | --- |
| **P0** | **SSD1306 / SH1106 の SPI トランザクション所有を修正する。** 現在の `PanelPaged::cmd()`、`init()`、`display()` は `BusSPI::beginTransaction()` を通らず、I2C では見えない不整合がある | SSD1306 + `TinyGFXBusSPI` のホストテストを追加し、SPI 設定、CS、DC、転送バイト列を確認する。I2C の既存結果は不変 |
| **P0** | **ST7789 の GRAM サイズ・原点オフセットの設定契約を決める。** `setGramSize()` / `setOffset()` を呼ぶ順序に依存して現在回転の `_offX/_offY` が古いままにならないようにする | setter で現在回転の派生値も更新するか、「`begin()` 前のみ」と API 契約を固定する。240x240 / 135x240 の代表例と全回転テストを更新する |
| **P1** | **ページ方式パネルの対応寸法とバッファ契約を明示する。** 初期化列は 128x64 固定だが、コンストラクタは任意の `w` / `h` を受け取っている | 128x64 専用に API を狭めるか、128x32 等の初期化値を導出する。高さ 8 の倍数、null、必要バイト数の扱いを決めてテストする |
| **P1** | **座標の境界演算を確認する。** `x + w - 1` などが `int16_t` の範囲を越えると、「画面外座標を受けてクリップする」という契約を満たせない | 終端・差分だけ `int32_t` にする案を実装し、極端な負座標・正座標をテストする。CH32V003 の増分も測る |
| **P1** | **初期化失敗のモデルを決める。** 現状の `begin()` は `bool` だが、実パネルは転送失敗を検出せずほぼ常に `true` | best effort として `void` にするか、無効バッファ・I2C NACK 等を伝播するか決定する。少なくとも I2C の `chunk == 0` は拒否または 1 へ正規化する |
| **P2** | **文書と実装の同期を続ける。** `fillRect` 追加後の Panel の virtual 数など、古い記述が残っていた | API 安定化時に CORE_DESIGN / DECISIONS / README / テスト一覧を一巡し、固定のテスト件数はなるべく書かない |

## 2. 実装順序

### フェーズ 0 — スパイク（**完了**）

| # | 確かめたこと | 結果 |
| --- | --- | --- |
| 0-1 | 構成 base / A のサイズ | base 5,892 B / A +1,600 B。**当初の予算（総量ベース）は的外れだった**ので増分方式へ変更 |
| 0-2 | 未使用機能が落ちるか | **落ちる。** `linkprune` が継続して検査しており、D1 / D15 は成立している |
| 0-3 | virtual 2 枚で予算に入るか | **入る。** 構成 E で +6,484 B。**静的結合スイッチは不要**と判断（D2 確定） |
| 0-4 | `--gc-sections` と除算 | gc-sections はコア既定で有効。**rv32ec で除算命令なし**を確認し、除算ルーチンが 1 つも出ていないことをテストで固定 |
| 0-5 | `BusCapture` で画を復元 | **未。** コンパイルは通るが検証テストは未実装 → フェーズ 1 で |

**想定外だったこと**:

1. **CH32V003 で Arduino SPI が使えない**（3 コアとも）。`BusSoftSPI` を第一実装にした（D18、E2 / E3）
2. **base が 5,892 B**。使わない Wire / UART / IPAddress がコアから入ってくる（E2-b）
3. **`println(float)` は「重い」ではなく「載らない」。** 4,280 B 溢れる
4. **開発中の新コア（ArduinoCore-CH32）で 1〜2 が両方とも解消する。** base 624 B、
   ハードウェア SPI もリンク可。E2 / E3 は「直してもらう」より「リリースを待つ」が正解になった（E7）
5. **Uno R3 でも全部載る。** 効くのは RAM だけで、フォントを PROGMEM に置けば実用になる（D19）

> フェーズ 1〜4 の**実装と Tier 1 検証は済んでいる**。**M0（M5Stack）は 2026-08-28 に通過**。
> 各フェーズに残っているのは回転（M0b / M2）と ST7789 実機（M1）。

### フェーズ 1 — 出力の芯

`TinyGFXBus` / `TinyGFXBusSPI` / `TinyGFXPanel` / `TinyGFXPanelST7789` / `setAddrWindow` / `writeColor` / `fillScreen` / `fillRect` / `drawPixel` / `drawFastHLine` / `drawFastVLine`。

**実装済み。`capture/` `window/` `fill/` が通っている。**
**残るのは実機 M1。** 初期化列と SPI モードは実機でしか確かめられない。

### フェーズ 2 — プリミティブとクリップと回転と帯レンダリング

`drawLine` / `drawRect` / 円 / 角丸 / 三角 / `setClipRect` / `setRotation` / `TileCanvas`。
**実装済み。`primitive/` `clip/` `tile/` が通っている。**

**残るのは実機 M2。** MADCTL の値とオフセットの導出が実機で正しいかは、
ホストでは原理的に分からない（`window/` は「実装が表どおりか」しか見ていない）。

### フェーズ 3 — 文字（**実装済み。`text/` が通っている**）

`TinyGFXFont` / 6x8 固定幅フォント / `drawChar` / `drawString` / `textWidth` / `setTextSize`。

テスト: `text/` + 構成 D の `linkprune`（**文字を使わないスケッチにフォントが載らないこと**）。

### フェーズ 4 — 画像と拡張ヘッダ（**実装済み。`image/` が通っている**）

`pushImage`（transparent 版含む）、`TinyGFX/Print.h`。

テスト: `image/` + 構成 E / P1 / P2 のフットプリント測定。
**ここで `Print` と float の値札を実測して [FOOTPRINT.ja.md](FOOTPRINT.ja.md) と GUIDE に書く。**

### フェーズ 5 — パネル追加と Fast Backend

ST7735 / ILI9341。パネル初期化テーブルの整理。CH32V 向け Fast Backend（GPIO / SPI レジスタ、必要なら DMA）。

テスト: `build_matrix/` 拡張、実機 M4。
**Fast Backend の有無で `capture/` の出力が変わらないこと**を不変条件にする。

### フェーズ 6 — 仕上げ（**examples のみ完了**）

- examples — **完了。6 本**
- README / GUIDE / API（日英）— **未着手。次にやる**
- API 安定化、ベンチ — 未

**実機（M0 / M1 / M2）が終わるまで利用者向けドキュメントは書かない。**
配線とオフセットの説明を、動いていないうちに書くと嘘になる。

## 3. リリース方針

**途中リリースをしない。** フェーズ 4 まで終わり、CH32V003 と ESP32 の実機で動いた時点で **1.0.0 として初回リリース**する。

理由: API を LovyanGFX に寄せる方針（[DECISIONS.ja.md](DECISIONS.ja.md) D14）なので、0.x で出すと「寄せ方」を変えづらくなる。フットプリント予算も実測前に約束したくない。

**要確認（リリース前）**: ライブラリ名 `TinyGFX` の重複を GitHub / Arduino Library Registry / PlatformIO Registry で確認する（[DECISIONS.ja.md](DECISIONS.ja.md) Q8）。**リポジトリを公開する前にやる。**

## 4. リポジトリ整備（**完了**）

兄弟ライブラリと同じ構造に揃える。雛形は [`../../arduino-library-release-toolkit`](../../arduino-library-release-toolkit) にある（`src/TestProject.h` / `tests/smoke/` / `examples/test/` の一式）。

| ファイル | 状況 | 備考 |
| --- | --- | --- |
| `LICENSE` | **あり** | |
| `docs/` | **あり**（この文書群） | |
| `library.properties` | **あり** | `name=TinyGFX` `architectures=*` `includes=TinyGFX.h` `depends` なし |
| `keywords.txt` | **あり** | |
| `CHANGELOG.md` | **あり** | 先頭に `## Unreleased` |
| `.gitignore` | **あり** | |
| `tools/bump_version.py` | **あり** | toolkit からコピー。**個別編集しない** |
| `.github/workflows/release.yml` | **あり** | toolkit からコピー。**個別編集しない** |
| `.github/workflows/tests.yml` | **あり** | プロジェクト固有（[TEST_PLAN.ja.md](TEST_PLAN.ja.md) §7） |
| `src/TinyGFX.h` ほか | **あり** | ヘッダオンリー。`.cpp` は 0 |
| `src/tinygfx_version.h` | **あり**（0.0.0） | 以降は `bump_version.py` が生成 |
| `tests/` 一式 | **あり** | Tier 0〜2 と、環境を渡したときだけ動く Tier 3 を含む |

## 5. 決めてから進みたいこと

[DECISIONS.ja.md](DECISIONS.ja.md) §2 の Q1〜Q8 のうち、**フェーズ 0 の前に決めたいのは次の 2 つだけ。** 残りは実装しながらで間に合う。

**先に決めておきたいものは無くなった。** Q8（名前）は確認済み、Q9（サブセット化）は
外部ツールに切り出して取り下げ、Q7 は実測で決着（Panel の virtual は 8 本）。

残っているのは実物を見て決めるもの（Q1 糖衣クラス、Q2 色定数、Q4 `setSwapBytes`、
Q5 CS 共有、Q6 PROGMEM 画像、Q10 コールバックの形、Q11 基準機の移行時期）だけ。
