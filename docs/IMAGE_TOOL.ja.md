# 画像埋め込みツールの調査

**TinyGFX の外側の話。** 画像を C の配列に変換して埋め込むツールを、
**TinyGFX 専用ではなく汎用**（LovyanGFX や他の GFX でも使える）に作る
ための下調べ。開発中の文書なので日本語のみ。

TinyGFX 自身の画像形式とデコーダの実測は [IMAGE_FORMAT.ja.md](IMAGE_FORMAT.ja.md)。
こちらは**ツール側の設計**を扱う。

## 他の GFX が受け取る形式（2026-08-29 調査）

Adafruit_GFX / U8g2 / LovyanGFX 1.2.26 はソース、Arduino_GFX / TFT_eSPI は
ドキュメントで確認。

| 形式 | Adafruit | U8g2 | LovyanGFX | Arduino_GFX | TFT_eSPI |
| --- | :-: | :-: | :-: | :-: | :-: |
| **1bpp MSB first** | `drawBitmap` | `drawBitmap` | ✓ | ✓ | ✓ |
| **1bpp LSB first（XBM）** | `drawXBitmap` | `drawXBM` / `drawXBMP` | ✓ | ✓ | ✓ |
| 8bpp グレースケール | `drawGrayscaleBitmap` | — | `grayscale_t` | ✓ | Sprite 8bpp |
| 8bpp 索引 + パレット | — | — | ✓ | `drawIndexedBitmap` | Sprite |
| RGB332（8bpp 直接色） | — | — | `rgb332_t` | ✓ | ✓ |
| **RGB565 リトルエンディアン** | `drawRGBBitmap` | — | `rgb565_t` | `draw16bitRGBBitmap` | ✓ |
| **RGB565 ビッグエンディアン** | — | — | `swap565_t` | `draw16bitBeRGBBitmap` | `setSwapBytes` |
| RGB888 / 24bit | — | — | `rgb888_t` | `draw24bitRGBBitmap` | — |
| ARGB8888 | — | — | `argb8888_t` | — | — |
| マスク（別配列） | ✓ | — | `pushAlphaImage` | ✓ | — |
| 透過色 | — | — | ✓ | `WithTranColor` | ✓ |
| BMP / JPG / PNG / QOI のデコード | — | — | ✓ | — | — |

### ここから読めること

**1. 1bpp MSB と RGB565 が全社共通。** この 2 つを押さえれば大半が動く。

**2. バイト順が割れている。** RGB565 のリトル／ビッグは library ごとに違い、
呼び名も `swap565_t` / `draw16bitBeRGBBitmap` / `setSwapBytes` とバラバラ。
**汎用ツールの必須オプション。**

**3. 圧縮を受けるのは TinyGFX だけ。** 他は全部「生の画素配列」。
だから汎用ツールは**非圧縮を吐くのが基本**で、圧縮（RLE / パレット RLE）は
**描画器固有の追加機能**という位置づけになる。

**4. 1bpp のビット順も割れている。** MSB first（Adafruit `drawBitmap`）と
LSB first（XBM）が両方ある。これもオプション。

**5. 縦詰めは誰も持っていない。** SSD1306 のページ形式に直接載る並びは、
調べた範囲ではどのライブラリの API にも無い（U8g2 は内部で使うが受け取ら
ない）。TinyGFX の `bitmap1v` は独自。

## asset ツールと分けるか → **分けた（2026-08-29 決定）**

- **asset**: ライブラリ + CLI ツールとして分離。VSCode 拡張はそれを読み込む
- **画像**: 別のライブラリ + CLI ツールとして作る

判断の根拠は下記。種類が違う。

| | asset 埋め込み | 画像変換 |
| --- | --- | --- |
| 入出力の関係 | **バイトが保存される**（gzip も可逆） | **不可逆**（減色・2 値化・ディザ） |
| 出力の意味 | 元のファイルそのもの | 特定の描画器のための画素配列 |
| 設定項目 | 圧縮するか、どこに出すか | 形式・色数・パレット・ディザ・バイト順・対象ライブラリ |
| **「正しい出力」** | **一意**（バイトが一致すれば正解） | **測らないと決まらない**（データ + デコーダ、MCU 依存） |
| 消費側 | 任意（HTTP で配る、パースする、そのまま書き出す） | 描画ライブラリだけ |
| UI | 要らない | **プレビューが要る** |

最後の 2 つが効く。asset は「入れたら出る」バッチだが、画像は**総当たりして
比較する**道具で、本質的に対話的。

### ただし呼び出し方は揃える

どちらも「コンパイル前にフォルダを見て `.h` を再生成」で、フォルダに設定
ファイルを置く形。**`.assetsconfig` にパターンを書いて画像だけ画像ツールへ
回す**連携点を作れば、利用者からは 1 つの仕組みに見える。

参考: VSCode 拡張（`vscode-arduino-cli-wrapper`）の asset 機能は
`assets/` や `assets_*/` を `<フォルダー名>_embed.h` に展開し、
`.assetsignore`（gitignore 書式）と `.assetsconfig`（INI: 出力先・
シンボル接頭辞・minify・gzip・タイムスタンプ・ハッシュ）を持つ。
**画像ツールも同じ形の設定ファイルにすると学習コストが下がる。**

## 名前

フォントツールが `LGFXFontToolJs` / npm `lgfx-font-tool` なので、対になる
形がいい。ただし「汎用にしたい」なら LGFX を外すのが筋。

| 候補 | |
| --- | --- |
| **`gfx-image-tool`**（`GfxImageToolJs`） | **推し。** フォントツールと対になり、特定ライブラリに縛られず、何をするか一目で分かる |
| `img2gfx` | CLI として短く明快 |
| `PixelPack` | 覚えやすいが、何をするかは名前から分からない |

asset 側は `embed-asset-tool`（`EmbedAssetToolJs`）で揃う。

将来 `lgfx-font-tool` も汎用名に寄せるなら、
**`gfx-font-tool` / `gfx-image-tool` / `gfx-asset-tool`** の 3 本立てが
いちばんきれい。CLI 名をパッケージ名に合わせた実績があるので、同じ整理を
横に広げる形になる。

## ツールに要る機能（TinyGFX 側の実測から）

[IMAGE_FORMAT.ja.md](IMAGE_FORMAT.ja.md) の「正式ツールへの申し送り」と
重なるが、汎用ツールとして整理し直すとこうなる。

### 全ライブラリ共通

1. **入力**: PNG / BMP / GIF / JPEG
2. **出力形式の指定**: 1bpp（MSB / LSB）、8bpp グレー、8bpp 索引 + パレット、
   RGB332、RGB565（LE / BE）、RGB888
3. **2 値化とディザリング**: 閾値、Floyd-Steinberg / Bayer / なし。
   **PNG の文字はアンチエイリアスされるので必須**（128x64 のスプラッシュが
   54 色になった実例がある）
4. **減色**: 目標色数を指定してパレットを作る
5. **出力の形**: 対象ライブラリごとに C の書き方が違う
   （`const uint8_t PROGMEM name[]` / `const uint16_t name[]` / 構造体）
6. **透過**: 透過色の指定、またはマスク配列の生成

### TinyGFX 向けの追加

7. **圧縮**（RLE / パレット RLE）と**総当たりでの選択**
8. **一括変換と形式の集合最適化** — デコーダ代は形式ごとに 1 回なので、
   1 枚ずつ最小を選ぶと損をする（実測 343〜597 B）
9. **MCU の指定** — デコーダ代の順位がアーキテクチャで入れ替わる
10. **縦詰め 1bpp** — ページ方式パネル用

**7〜10 は他のライブラリでは意味を持たない。** ツールの中で「TinyGFX
ターゲット」として分けるのが素直だと思う。

## 状況（2026-08-31）

| | |
| --- | --- |
| asset ツール | **分離してライブラリ + CLI 化。** VSCode 拡張はそれを読み込む構成に |
| 画像ツール | **GfxImageToolJs（リリース前）。上の 1〜10 が動くことを確認済み** |
| TinyGFX 側 | `src/TinyGFX/Image.h` は**実装済み・テスト済み。** ツールの完成を待たずに使える |

### 本番 CLI で確認したこと

**フォルダを 1 回渡すだけで、5 デコーダ全部の出力が得られる。**

```sh
gfx-image-tool build sources --out generated --preview expected
```

| 項目 | 結果 |
| --- | --- |
| 8 の一括最適化 | **動く。** 同じ 4 枚で 8,720 → 4,184 B（個別の合計と一致） |
| 7 の総当たり | 候補が全部 1 行に出る（`rlepal4:61, bitmap1h:76, ..., raw565:1152`） |
| 3 のディザ | Floyd-Steinberg / Bayer 2・4・8 |
| 4 の減色 | `--mode indexed --colors 16` |
| `--preview` | **変換後の画素**を PNG で出す。フォルダならディレクトリごと |

`--preview` は**検証の要**だった。ディザと減色は変換元から計算し直せないので、
ツールが出した画素と突き合わせるしかない（`tests/image_oracle/`、
[EXTERNAL_REQUESTS.ja.md](EXTERNAL_REQUESTS.ja.md) E9・E10）。

ほかに確かめたこと:

| 項目 | 結果 |
| --- | --- |
| **共有デコーダの割引** | **実装済み。** bitmap1h + bitmap1v で 800 ではなく **520 B**（400 × 1.3）。[IMAGE_FORMAT.ja.md](IMAGE_FORMAT.ja.md) §2 で頼んだとおり |
| **出力の再現性** | 2 回走らせて **1 バイトも違わない**。絶対パスも日時もヘッダに入らない ——「生成物を commit して `--check` で古さを見る」運用が成立する |
| フォルダ変換の `--json` | 候補・個別最小・選択が全部出る |
| `--preview-layout comparison` | 元画像と並べた 2 倍幅の PNG。**目で見る用**で、オラクルには使えない |
| **`--check`** | ヘッダとプレビューの両方を見て、食い違ったファイル名を出し、2 で終了する |
| 記号の衝突検査 | **両方のファイル名を出す**（`my icon.png and my_icon.png`）。`sub/icon.png` は `sub_icon` |
| 8 の倍数でない寸法 | 31x17・13x13・23x7 が全形式で通る（`tests/image_oracle/`） |

| **古い出力の掃除** | 出力先のマニフェストで自分の作ったものを追跡する。元画像を消すと `build` が消し、`--check` は `stale` で 2 で終了。**置いてある `README.md` や手書きの `.h` には触らない** |

**回避は 1 つも要らなくなった。** 透過も出力先も `--check` も古い出力の掃除も、
ツールの既定と設定ファイルだけで済んでいる
（[E11](EXTERNAL_REQUESTS.ja.md#e11)〜[E14](EXTERNAL_REQUESTS.ja.md#e14) 解決済み）。

**構成が変わった（2026-08-31）。** プロジェクトルートの直下に `images/` を置き、
元画像と `.imagesconfig` をそこに入れる。既定の出力は `images/` の隣の `images.h`。

```
MySketch/
  images.h          <- バンドル出力
  images/
    .imagesconfig
    .gitignore      <- .gfx-image-tool/ を除外（init が置く）
    .gfx-image-tool/  <- 使い捨て cache。header のマニフェストはここ
    <元画像>
```

**マニフェストは header も preview も cache に移った**ので、commit の対象から
外れた。**出力先にツールのファイルは 1 つも残らない。**

`--preview-layout both` は `<名前>.png` と `<名前>.comparison.png`（元画像と
並べた 2 倍幅）を両方出す。**`tests/image_oracle/` は `converted`（既定）のまま
にしてある** —— 比較画像はテストが読まないし、`sources/` と `expected/` が並んで
commit されているので、目で見るには 2 つ開けば足りる。

**依頼（E9〜E15）は全部解決した。TinyGFX 側に回避は 1 つも残っていない。**

## いまあるもの

`tools/img2h.py` が**実験用の最小実装**。総当たり・一括・集合最適化・
MCU 指定・2 値化・透過まで動く。**`tests/image_fmt/*.h` の生成にまだ使っている**
（`tests/image_oracle/` は本番 CLI に移した）。**PNG 以外の入力もディザリングも
減色も持っていない。**
