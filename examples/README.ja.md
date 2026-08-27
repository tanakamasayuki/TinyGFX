# examples

> English: [README.md](README.md)

| 例 | 内容 | CH32V003 でのフラッシュ増分 |
| --- | --- | --- |
| [HelloWorld](HelloWorld) | 矩形と文字。**最初に見るのはこれ** | 約 6.0 KB（文字まで） |
| [Shapes](Shapes) | 使えるプリミティブを一通り | 約 4.9 KB |
| [FlickerFree](FlickerFree) | 帯レンダリングでちらつきを消す | 約 7.6 KB / RAM 約 2.0 KB |
| [HardwareSPI](HardwareSPI) | ハードウェア SPI を使う版 | Shapes とほぼ同じ |

増分の根拠は [../docs/FOOTPRINT.ja.md](../docs/FOOTPRINT.ja.md)。
**使わない機能はフラッシュに載らない**ので、必要なものだけ呼べばよい。

## 配線

どの例も先頭でピンを宣言している。手元の配線に合わせて変えること。

| 信号 | 役割 |
| --- | --- |
| SCK / MOSI | SPI。ハードウェア SPI 版では宣言しない（Core に任せる） |
| DC | コマンドとデータの切り替え |
| CS | チップセレクト。専有しているなら -1 でもよい |
| RST | リセット。モジュール側で処理していれば -1 |

## フォントはスケッチ側に置く

**TinyGFX はフォントデータを 1 バイトも同梱していない。**
`HelloWorld` には `tinygfx_font5x7.h` を同梱してあるが、これはつなぎの
5x7（0x20-0x3F の 32 文字）で、実運用のフォントは
[LGFXFontToolJs](https://www.npmjs.com/package/lgfx-font-tool) で作る。

形式は GFXfont（Adafruit GFX 互換）なので、既存の Adafruit 用フォントヘッダも
そのまま `setFont()` に渡せる。

**AVR では PROGMEM に置くこと。** 置かないと RAM を食い、絵も化ける。

## パネルの原点オフセット

240x240 や 135x240 の ST7789 モジュールは、コントローラの GRAM より小さいので
原点がずれている。その場合は 2 つとも設定する。

```cpp
panel.setGramSize(240, 320);   // コントローラの GRAM
panel.setOffset(52, 40);       // 回転 0 のときの可視域の位置
```

回転 1〜3 のぶんは自動で導出される。
