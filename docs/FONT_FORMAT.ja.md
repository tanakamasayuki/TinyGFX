# フォント — TinyGFX 側の話

内部の記録。日本語のみ。

## 0. 位置づけ — **形式そのものはここには無い**

TinyGFX が使うビットマップフォント形式は **CellFont**。仕様は TinyGFX の外にある。

> **LGFXFontToolJs `docs/formats/cellfont.ja.md`（v1 確定）**
> 生成器と描画器の取り決めはすべてそちら。構造体・ビットの並び・索引・
> エンコーダ規範・適合性まで書いてある。

**この文書に残すのは TinyGFX 側だけ** — 実装の置き場所、実測したフラッシュ増分、
未使用形式が落ちることの証拠、まだやっていないこと。

以前はここに形式そのものを書いていた（TinyFont）。**2026-08-28 に CellFont へ移した。**
名前が変わったのは、TinyFont が他所で使われている語だったため。経緯は
[DECISIONS.ja.md](DECISIONS.ja.md) D17。

## 1. TinyGFX 側の置き場所

| ファイル | 役割 |
| --- | --- |
| `src/TinyGFX/CellFont.h` | **仕様 §12.1 が描画器に求めるもの。** 構造体・`CELLFONT_PROGMEM`・アクセサ・版番号。**TinyGFX に依存しない**。`TinyGFX.h` が既定で連れてくる（実測 0 バイト） |
| `src/TinyGFX/FontCell.h` | CellFont のデコーダと `tinygfxFontCellOps` |
| `src/TinyGFX/FontU8g2.h` | u8g2 のデコーダと `tinygfxFontU8g2Ops` |
| `src/TinyGFX/Font.h` | 形式に依らない受け口（`TinyGFXFontOps` / `TinyGFXFontRef`） |

**仕様が決めているのはマクロと型の名前だけで、ファイル名は自由**（§12.1、2026-08-28 改訂）。
なので大域の include 名前空間を汚さないよう `TinyGFX/` の下に置いてある。

**生成されたフォントヘッダは描画器のヘッダを include しない。** 型が無ければ
`#error` で止まる（§12.2）。つまり「描画器のヘッダを先に」という順序が要るのだが、
**`TinyGFX.h` が `CellFont.h` を既定で連れてくる**ので、利用者が順序を気にすることはない。
構造体とマクロだけなので**コードもデータも 1 バイトも増えない**（構成 base〜T の全部で
実測値が変わらないことを確認）。

同じ仕様を実装した別ライブラリと同居しても壊れないよう、`CELLFONT_SPEC_VERSION` で
ファイル全体を守ってある（先に定義したほうが勝つ。仕様が同じなので構造体も同じ）。

### 連鎖は形式の中だけ

`CellFont::next` の 1 段だけ。**セル幅クラスで分けて固定ピッチを立てる**ための仕組みで
（仕様 §10.2）、8px 混在 190 字で 690 B 減る。**生成器が勝手に作る**ので、
利用者が意識することはない（CLI の既定は `--max-chain 2`）。

**形式をまたぐ連鎖は持たない。** 以前は `TinyGFXFontRef::next` で
「半角 CellFont + 全角 u8g2」のような組み方ができたが、**2026-08-28 に外した。**

- **フォントツールが 1 本にまとめてくれる。** 半角と全角を混ぜたいなら、
  その文字集合をまるごと 1 回生成すればよい。ツールが中で幅クラスに割る
- 代金は **40 バイト + `TinyGFXFontRef` 1 本あたり 4 バイト**（実測）
- 使う場面が「u8g2 の既製フォント資産に自作グリフを足す」くらいしか無く、
  それもツールで作り直せば済む

**外して困ったら戻せばよい**が、戻すのは「ツールでは表せない組み合わせ」が
実際に出てきたときにする。

### U+FFFD への退避は最外だけ

**デコーダの中では退避しない。** CellFont 側に U+FFFD があるだけで、後段の別形式
フォントに載っている字へ到達できなくなる（仕様 §7.2 が警告している事故が 1 段上で
起きる）。デコーダは「見つかった / 見つからない」を返すに留め、退避は
`TinyGFX::drawChar` が連鎖を全部引き終えてから 1 度だけ行う。

**この注意書きは TinyGFX 側から仕様へ持ち込んだもの**で、仕様 §15.2 に入っている。
形式をまたぐ連鎖を外したいまも、**形式の中の連鎖で同じ事故が起きる**
（前のリンクの豆腐が次のリンクの字を潰す）ので、退避の位置は変えていない。
`tests/fontchain/` がこれを検査していて、デコーダ側で退避すると落ちる。

### `y` はベースライン（デコーダの中では）

公開 API（`drawString` / `drawChar`）の `y` は**行の上端**のまま。コアが
**連鎖の先頭フォントの ascent** でベースラインに直してからデコーダを呼ぶ。

連鎖する各フォントは高さも `yOffset` も別々でよく（仕様 §8）、揃っているのは
ベースラインだけなので、**各デコーダが自分のメトリクスで換算すると字面がずれる**。
背景セルの箱も同じ理由で先頭のメトリクスから引く（`getTextAscent()` /
`getTextLineHeight()`）。

## 2. 実測 — CellFont への移行でいくら増えたか

CH32V003 / `-Os` / `tests/constructs/d`（構成 C + 文字）。

| | flash | 文字機能ぶん | データ | **コード** |
| --- | --- | --- | --- | --- |
| C（文字なし） | 10,772 | — | — | — |
| （旧）TinyFont | 11,808 | +1,036 | 208 | **828** |
| **CellFont（現在）** | **12,056** | **+1,284** | 216 | **1,068** |

**+248 B。** 内訳はデータ +8 B（ops 表に `ascent` が増えて +4、構造体が 24 → 28 で +4）、
コード +240 B。

コードの増分でいちばん大きいのは**測れたものが 1 つ**:

| 何 | 実測 |
| --- | --- |
| **U+FFFD への退避**（仕様 §7.2 / §15.2 の義務） | **64 B** |
| 残り（形式内連鎖の走査・頭ブロックの分岐・ascent の受け渡し・32bit オフセット） | 176 B |

### スイッチで 164 B 戻る

変種が決まっているなら分岐を落とせる。

```sh
-DTINYGFX_FONT_SPARSE=0    # 疎索引（コード表・頭ブロック）を落とす
-DTINYGFX_FONT_RECORDS=0   # 可変ピッチ（グリフ表）を落とす
```

| | flash | 差 |
| --- | --- | --- |
| 既定（両方あり） | 12,056 | — |
| 両方なし | **11,892** | **−164 B** |

**既定は両方入れる。** 落とすと疎索引が使えず、CJK が入らない。

### 割に合っているか — **合う**

+248 B の代わりに手に入るのが、`CellFont::next` による**幅クラス連鎖**（仕様 §10.2）。

| 4x8 / 8x9 の混在 190 字 | データ量 |
| --- | --- |
| 単一フォント | 2,163 B |
| **幅クラスで 2 本に分けて連鎖** | **1,473 B** |

**−690 B。** 最初の実フォント 1 本で 2.8 倍返ってくる。頭ブロック（`headCount`）も
ASCII 95 字 + 記号 6 字で −190 B（仕様 §7）。

## 3. u8g2 デコーダのコスト — 仕様 §13.4 の `D`

仕様は「別形式へ移すとデコーダをもう 1 つ積む。その追加コストを `D` とすると、
データの差が `D` を超えるまでは CellFont のほうが総量で安い」としている。

**TinyGFX の `D` は実測できる。** CH32V003 / `-Os`:

| 構成 | flash | 文字機能ぶん | データ | **コード** |
| --- | --- | --- | --- | --- |
| C（文字なし） | 10,772 | — | — | — |
| d（CellFont / `drawString`） | 11,644 相当 | — | 188 | **684**（旧 TinyFont での測定） |
| d_u8g2c（u8g2 / `drawChar` のみ） | 11,628 | +856 | 163 | **693** |
| d_u8g2（u8g2 / `drawString`、UTF-8 込み） | 11,772 | +1,000 | 163 | 837 |

**u8g2 のデコーダは 693 B。** 仕様 §13.4 の表では `D=500` と `D=800` の間を読めばよい:

| フォント | H | CellFont のほうが総量で安い上限字数 |
| --- | --- | --- |
| DejaVu 9px | 10 | 96+ |
| FreeSans 9pt | 18 | 77〜96 |
| FreeSans 12pt | 23 | 47〜53 |
| FreeSans 18pt | 35 | 21〜31 |
| DejaVu 56px | 58 | 5〜8 |

時計の `0-9:` のような数字だけの用途なら、**24pt でも 56px でも CellFont のほうが安い。**

正しさは LGFXFontToolJs が描いた絵との一致で確認してある（`tests/u8g2/`。ASCII と CJK）。

## 4. 複数のフォント形式 — **未使用分は 1 バイトも載らない**

**コアはフォント形式を 1 つも知らない。** フォント側が自分のデコーダ（`TinyGFXFontOps`）を
指していて、コアは連鎖をたどって呼ぶだけ。**include していない形式のデコーダは
どこからも参照されないので落ちる。**

`nm` で最終バイナリのシンボルを数えたもの。`tests/linkprune/` が毎回検査する。

| 構成 | CellFont のシンボル | u8g2 のシンボル |
| --- | --- | --- |
| d（CellFont だけ） | 3 以上 | **0** |
| d_u8g2（u8g2 だけ） | **0** | 3 以上 |
| d_both | あり | あり |

**CellFont のスケッチに u8g2 を足すと約 +1,180 B。使わなければ +0 B。**

## 5. まだやっていないこと — UTF-8 の入口

**実装済み**（2026-08-29、D26）。`drawString` / `textWidth` / `TinyGFXPrint::print` は
`char*` を UTF-8 として読む。形式に依らない話なので、CellFont でも u8g2 でも同じ。

実測 **+164 B**（CH32V003。AVR +134、ESP32 +116）。`TinyGFXPrint` まで含めると
+292 B —— `Print::write(uint8_t)` が 1 バイトずつ渡してくるので、組み立て中の
コードポイントを状態として持つ必要があるため。

`TINYGFX_FONT_UTF8=0` でバイト単位（Latin-1）に戻り、**入れる前と 1 バイトも
違わない値になる**（[DECISIONS.ja.md](DECISIONS.ja.md) D26）。

## 6. フォントをどう作るか

**実運用のフォントは LGFXFontToolJs の CLI が出す**（`lgfx-font-tool` 2.0.0 で公開済み）。

```sh
npx -p lgfx-font-tool lgfx-font build --google "Noto Sans JP" --em 12 \
    --chars "温度設定完了 23.5℃" --format cellfont --out src/font.h
```

出力は仕様 §12.2 の形（`static const CellFont Name CELLFONT_PROGMEM = {...}`）で、
**描画器のヘッダを include しない**。TinyGFX 側で 1 行包んで `setFont()` に渡す:

```cpp
#include <TinyGFX.h>
#include <TinyGFX/FontCell.h>   // デコーダ。使う形式だけ
#include "font.h"               // CLI が出したもの。手を入れない

static const TinyGFXFontRef myFont = {&Name, &tinygfxFontCellOps, nullptr};
```

**TinyGFX 本体はフォントデータを 1 バイトも同梱しない。**

### テストと examples のフォント

**すべて CLI が出したものをそのまま置いてある**（2026-08-28 に切り替えた）。
手書きのつなぎ生成器 `tools/gen_font.py` は役目を終えたので削除した。

| 置き場所 | 生成 | 何を踏むか |
| --- | --- | --- |
| `tests/common_libs/tgfx_font/src/tgfx_digits.h` | `--sets digits` | 固定ピッチ・連続索引 |
| 〃 `tgfx_digits_chain.h` | `--sets digits --chars "℃"` | **形式内の連鎖**（セル幅クラスで 2 本に割れる） |
| 〃 `tgfx_digits_sparse.h` | 同上 `--max-chain 1` | **可変ピッチ + 疎索引 + 頭ブロック** |
| 〃 `tgfx_ascii.h` | `--sets ascii` | ASCII 95 字 |
| `examples/*/tgfx_clock.h` | `--sets digits --chars ":. "` | 疎索引。**しっぽに `first` より小さいコード**（0x20 / 0x2E） |
| `tests/clifont/cli_font.h` | `--google "Noto Sans JP" --em 12 --chars ...` | 可変ピッチ + 頭ブロック + 全角 |
| `tests/u8g2/u8g2_ascii.h` / `u8g2_cjk.h` | `--format u8g2 --no-wrapper` | u8g2 のデコーダ |

いずれも書体は `lgfxJapanGothic_8`（`--google` のものを除く）。

**上の 3 つは同じ 10 字を 3 通りに符号化したもの**で、`tests/text/` が
「どれで描いても絵が一致すること」を検査している。どの符号化を選ぶかは
生成器の裁量であって、**スケッチから見えてはいけない。**

### u8g2 も CLI から

`--format u8g2 --no-wrapper` でデータ配列だけが出る（`lgfx-font-tool` 2.2.2 で入った。
[EXTERNAL_REQUESTS.ja.md](EXTERNAL_REQUESTS.ja.md) E8）。cellfont と同じ手数で、
包む 1 行もほぼ同じ。

```cpp
static const TinyGFXFontRef myFont = {u8g2Cjk, &tinygfxFontU8g2Ops};
```

`tools/gen_u8g2_ref.mjs` が残っているのは**参照画像（`.ref.txt`）のため**だけ。
これは「LGFXFontToolJs が描いた絵」で、`tests/u8g2/` がデコーダの出力と突き合わせる。

## 7. 経緯

| 日付 | 形式 | 何が変わったか |
| --- | --- | --- |
| 2026-08-26 | GFXfont | 既存資産をそのまま食う案。ASCII で 653 B 損し、CJK は表せない |
| 2026-08-27 | TinyFont（独自） | 索引とグリフ表を生成時に選ぶ。GFXfont より 236 B 小さい |
| 2026-08-27 | + 形式の差し替え | `TinyGFXFontOps` / `TinyGFXFontRef`。u8g2 を足しても未使用なら 0 B |
| **2026-08-28** | **CellFont v1** | **形式を外部仕様に切り出し、頭ブロック・形式内連鎖・U+FFFD 退避・ベースライン基準を取り込んだ** |

CellFont 移行で一緒に直った不具合が 3 件ある。詳細は
[DECISIONS.ja.md](DECISIONS.ja.md) D17 / D19。

1. **フォント構造体が PROGMEM に載っていなかった**（AVR で文字が化ける）
2. 固定ピッチのビットマップオフセットが 16bit（大きな集合で折り返す）
3. 二分探索の中央値が加算形（16bit 環境で折り返す）
