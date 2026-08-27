# フットプリント

内部の記録。日本語のみ。**予算・測り方・実測値の台帳。**

TinyGFX の存在理由はここ。**数字が入っていない主張は書かない。**

## 1. 何のための文書か

1. 「この機能を足すと CH32V003 でフラッシュが何バイト増えるか」を**実測して残す**
2. 増分が予算を超えたときに気づく
3. 利用者向け GUIDE に載せる**値札の元データ**にする

**測っていない機能は追加しない**（[DECISIONS.ja.md](DECISIONS.ja.md) D6）。

## 2. 基準

| 項目 | 値 |
| --- | --- |
| 基準機 | **CH32V003**（Flash 16KB / SRAM 2KB） |
| FQBN | `ch32-riscv-arduino:ch32riscv:CH32V003_EVT`（最適化は既定の `-Os`） |
| アーキ | **rv32ec / ilp32e — 乗除算命令を持たない。** 除算はソフトウェアルーチン呼び出しになる |
| ビルドオプション | `-ffunction-sections -fdata-sections -Wl,--gc-sections` はコア既定で有効（確認済み）。`-fno-rtti -fno-exceptions -fno-threadsafe-statics` も既定 |
| Bus | `TinyGFXBusSoftSPI`（このコアに SPI ライブラリが無い。[EXTERNAL_REQUESTS.ja.md](EXTERNAL_REQUESTS.ja.md) E2） |
| 参考機 | ESP32（`esp32:esp32`）、AVR UNO（`arduino:avr:uno`） |
| パネル | ST7789 240x240 |
| 測る対象 | **スケッチ全体のビルドサイズ**（Arduino Core 込み）。ライブラリ単体ではない |

**ライブラリ単体のサイズは測らない。** 実際に効くのは「そのスケッチが載るか」だけなので、常に完成品の数字で判断する。

## 3. 測り方

```sh
arduino-cli compile --fqbn <FQBN> --output-dir build --warnings none <sketch>
```

- `arduino-cli compile` の出力する Flash / RAM の数値を記録する
- 差分を見るときは**必ず同じコア・同じバージョン・同じ最適化オプション**で測る。コアを上げたら基準行を測り直す
- シンボル単位で追うときはマップまたは `nm --size-sort` を使う

**自動化**: `tests/footprint/` がこの手順を回して、予算超過で fail する（[TEST_PLAN.ja.md](TEST_PLAN.ja.md) Tier 0）。

## 4. 構成の定義

積み上げで測る。**各行は「前の行 + その機能」**で、増分が読めるようにする。
実体は `tests/constructs/<name>/<name>.ino`。

| 構成 | 内容 |
| --- | --- |
| **base** | 空の `setup`/`loop` のみ（コアだけの下駄） |
| **A** | base + BusSoftSPI + PanelST7789 + `begin` + `fillScreen` |
| **B** | A + `fillRect` / `drawPixel` / `drawFastHLine` / `drawFastVLine` |
| **C** | B + `drawLine` / `drawRect` / 円 / 角丸 / 三角 |
| **D** | C + `setFont` / `drawString`（フォントデータ 384 B を含む） |
| **E** | D + `pushImage`（不透明版と transparent 版） |
| **T** | E + `TinyGFXTileCanvas`（240px × 1 行の帯バッファ 480 B を含む） |
| **P1** | D + `TinyGFX/Print.h` の `println(const char*)` / `println(int)` |
| **P2** | P1 + `println(float)` |

**A〜T がコア。P1 / P2 は「値札」を出すための構成**で、予算の対象外（超えて当然）。
ただし**数字は必ず載せる。**

## 5. 予算と実測（CH32V003）

**2026-08-27 実測。** `tests/footprint/` が毎回この表と同じ数字を出す。

| 構成 | Flash | Δ Flash | Δ 予算 | RAM | Δ RAM | Δ 予算 | 判定 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| base | 5,892 | — | — | 504 | — | — | — |
| A | 7,492 | **+1,600** | 1,800 | 572 | +68 | 96 | OK |
| B | 7,772 | +1,880 | 2,100 | 572 | +68 | 96 | OK |
| C | 10,664 | +4,772 | 5,200 | 572 | +68 | 96 | OK |
| D | 11,740 | +5,848 | 6,400 | 572 | +68 | 96 | OK |
| E | 12,376 | +6,484 | 7,200 | 572 | +68 | 96 | OK |
| T | 13,412 | +7,520 | 8,400 | 1,132 | +628 | 700 | OK |
| P1 | 12,000 | +6,108 | 参考 | 580 | +76 | 参考 | 参考値 |
| P2 | **リンク失敗** | — | 参考 | — | — | 参考 | **FLASH を 4,280 B 超過** |

### 読み取れること

- **フル機能（構成 E）で +6,476 B。** 16KB のうち base が 5,892 B を食っているので、
  残り 10,492 B に対して 62%。**載る。**
- **いちばん高いのは構成 C（全プリミティブ）で +2,892 B。** 円・角丸・三角で
  ライブラリのコードの約 45% を使っている。使わない人はここを払わない
  （`linkprune` で確認済み）。
- **文字は +1,076 B**（うちフォントデータ 384 B、コード 692 B）。想像より安い。
- **`pushImage` は +628 B。**
- **TileCanvas は +1,044 B / RAM +560 B。** ちらつき対策の値札としては妥当。
  RAM は帯バッファ（240×1 行 = 480 B）が支配的で、幅と行数で線形に決まる。
- **`Print`（float なし）は +260 B**（P1 − D）。安い。
- **`println(float)` は約 8.6 KB。** 12,012 B の P1 に足して 4,280 B 溢れたので、
  増分は 4,280 + (16,384 − 12,000) ≒ **8,664 B**。
  **CH32V003 のフラッシュの半分以上。** 実測でこの通りになった。

### 予算改定の記録

- 2026-08-27 初版。当初の予算（総量ベースで A=4KB など）は**間違い**だった。
  base だけで 5,892 B あるので、総量では意味がある数字にならない。
  **base からの増分**に置き換えた。予算値は実測 +10〜15% を上限として置いている。

## 6. 他のターゲット（実測）

### 6.1 開発中の新コア — `ch32-riscv-ug/ArduinoCore-CH32`

**同じ CH32V003 で base が 624 B。**（いま基準にしているコアは 5,892 B）

| 構成 | Flash | Δ | RAM | Δ |
| --- | --- | --- | --- | --- |
| base | 624 | — | 4 | — |
| A | 2,296 | +1,672 | 72 | +68 |
| C | 5,400 | +4,776 | 72 | +68 |
| E | 7,072 | **+6,448** | 80 | +76 |
| T | 8,048 | +7,424 | 640 | +636 |
| P1 | 7,184 | +6,560 | 80 | +76 |
| P2 | **400 B 超過** | 約 +9,200 | — | — |

読み取れること:

- **TinyGFX 側の増分はほぼ同じ**（E で +6,448 vs +6,476）。増分はコアに依らない = 我々の数字として信用できる
- **コアの下駄が 5,268 B 減る。** 使える余地が 10,492 B → 15,760 B に増える（1.5 倍）
- **`TinyGFXBusSPI`（ハードウェア SPI）がリンクできる。** [EXTERNAL_REQUESTS.ja.md](EXTERNAL_REQUESTS.ja.md) E2 / E3 はこのコアで解消する
- **`println(float)` があと 400 B で入る。** 依然として重いが「絶対に無理」ではなくなる

**このコアで測るには**（プレリリースなのでインストールではなく symlink + ツールチェーン指定）:

```sh
ln -s ~/dev_wch/ArduinoCore-CH32 "$(arduino-cli config get directories.user)/hardware/ch32-riscv-ug/ch32v"
arduino-cli compile --fqbn ch32-riscv-ug:ch32v:CH32V003 \
  --library <TinyGFX> \
  --build-property "compiler.path=$HOME/dev_wch/tools/xpack-riscv-none-elf-gcc/14.3.0-1/bin/" \
  --build-property "compiler.cpp.extra_flags=-I<TinyGFX>/tests/fonts" \
  tests/constructs/e
```

**自動テストの基準機はまだ `ch32-riscv-arduino` のまま。** 上の手順は CI に載せられないため。
新コアがリリースされたら基準機を移す。

### 6.2 Arduino Uno R3（`arduino:avr:uno`）

**全構成ビルドが通る。** フラッシュ 32KB / RAM 2KB。

| 構成 | Flash | Δ | RAM | Δ | RAM（PROGMEM 対応前） |
| --- | --- | --- | --- | --- | --- |
| base | 444 | — | 9 | — | 9 |
| C | 5,652 | +5,208 | 107 | +98 | 107 |
| D | 6,886 | +6,442 | **119** | +110 | 511 |
| E | 7,658 | +7,214 | **127** | +118 | 519 |
| T | 9,236 | +8,792 | 695 | +686 | 1,087 |

読み取れること:

- **フラッシュは余裕。** 構成 E で 32KB の 23%
- **効くのは RAM。** フォントを PROGMEM に置く対応（[DECISIONS.ja.md](DECISIONS.ja.md) D19）で
  構成 D の RAM が **511 → 119 B** になった。これが無いと 95 文字のフォントで
  1.2KB（2KB の 60%）を持っていかれて実用にならない
- 8bit なので同じ機能でもフラッシュは CH32 より 1 割ほど大きい（16bit 演算のぶん）
- 構成 T の RAM 695 B は 240px 幅の帯が支配的。UNO で使うなら 128px の ST7735 あたりで
  帯は 256 B になる

**AVR で残っている制約**: `pushImage` の画像データは RAM 上のもののみ。PROGMEM の画像には未対応
（[DECISIONS.ja.md](DECISIONS.ja.md) Q6）。

### 6.3 未測定

| ターゲット | 状況 |
| --- | --- |
| ESP32 | 未測定（フットプリントが問題になる環境ではないので優先度低） |
| CH32V20x | 未測定 |

## 7. スイッチの効果

**測っていないスイッチは入れない**（[CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) §10）。

| スイッチ | 状況 |
| --- | --- |
| `TINYGFX_STATIC_BUS` / `TINYGFX_STATIC_PANEL` | **未実装。** 構成 E が予算内に収まったので、いまは不要と判断（[DECISIONS.ja.md](DECISIONS.ja.md) D2）。必要になったら測ってから入れる |
| `TINYGFX_NO_CLIP` | **未実装。** 同上。クリップ判定の削減効果を測ってから決める |
| `TINYGFX_FILL_CHUNK` | 実装済み（`TinyGFXBusSPI` のみ）。**効果は未測定** |

## 8. 載っていてはいけないシンボル

**未使用機能が落ちること自体はリンカ（`--gc-sections`）に任せる。** ここで検査するのは、**落ちるはずのものが参照の連鎖で残っていないか**。`tests/linkprune/` と `tests/noalloc/` が最終バイナリの `nm` 出力で守る。

### 8.1 構成別（「まわりまわって載る」の検出）

| 構成 | 使う機能 | このシンボルが出たら fail |
| --- | --- | --- |
| **A** | `fillScreen` のみ | `drawLine` / `drawCircle` / `fillRoundRect` / `fillTriangle` / `drawChar` / フォントデータ / `pushImage` / `TileCanvas` / `TinyGFXPrint` |
| **B** | + 矩形・点・直線系 | 円・三角・文字・フォントデータ・`Print` |
| **C** | + 全プリミティブ | 文字・フォントデータ・`Print` |
| **D** | + 文字 | `Print` / `pushImage` |
| **E** | + `pushImage` | `Print` |

**この表がライブラリ構造の実効的な仕様になる。** 1 行でも落ちたら、原因は [CORE_DESIGN.ja.md](CORE_DESIGN.ja.md) §7.4 の R1〜R9 のどれかを破っている。

### 8.2 全構成共通（A〜E。拡張ヘッダを入れた P1 / P2 は対象外）

**判定は base との差で行う。** `_malloc_r` のようにコア側が最初から持ち込んでいるものを
TinyGFX のせいにしないため（CH32V003 の base には実際に `_malloc_r` が入っている）。

| 分類 | シンボル例 |
| --- | --- |
| 動的確保 | `_Znwj`（operator new）/ `_Znaj`（base に無い場合のみ） |
| 浮動小数点演算 | `__addsf3` / `__mulsf3` / `__divsf3` / `__floatsisf` |
| 浮動小数点書式化 | `printFloat` / `_dtoa` / `vfprintf` |
| シリアル出力 | `Print` / `HardwareSerial`（R4 の担保） |
| C++ ランタイム | `__cxa_guard_acquire`（関数内 static、R9 の担保）/ 例外関連 |

**要確認（Phase 0）**: CH32V003 のコアが乗除算命令を持たない構成なら、`__udivsi3` / `__umodsi3` もこのリストに入れる。描画アルゴリズムから除算を排除できているかの指標になる。

## 9. 削減のときに見る順番

増えてしまったときのチェックリスト。**上から効く。**

1. **virtual を増やしていないか。** vtable 参照は `--gc-sections` を無効化する（[DECISIONS.ja.md](DECISIONS.ja.md) D1）
2. **関数内 `static` を書いていないか。** ガード変数と初期化コードが載る
3. **除算・剰余を使っていないか。** ソフトウェアルーチンが載る
4. **`int` で計算していないか。** `int16_t` で足りる箇所を 32bit で回していないか
5. **同じ処理が inline 展開で複製されていないか。** 大きい関数は `inline` を外す
6. **`const` データが `.rodata` に正しく置かれているか**（AVR は別途 PROGMEM が要る）
7. **拡張ヘッダの中身がコアから参照されていないか。** 参照した瞬間に全員が払う
