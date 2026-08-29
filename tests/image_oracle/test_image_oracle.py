"""**生成された画像ヘッダが、変換後の期待画像と 1 画素も違わないか。**

GfxImageToolJs 仕様書 §15.2 が指定しているオラクル。変換ツールが出した `.h`
を TinyGFX が実際に描き、**ツールが出した期待画像（`.ppm`）**と突き合わせる。

**自作の encode と decode の往復では正しさを証明できない。** 符号化側と復号側が
同じ勘違いをしていたら一致してしまう。だから期待画像は「変換後の画素」として
別途出してもらい、こちらは描くだけにする。

## 使い方

`pairs/` に `<名前>.h` と `<名前>.ppm` を置くだけ。**コードは書かなくていい** ——
`gen_sketch.py` が収集時にスケッチを組み立てる。

- `.h` は `TinyGFXImageRef` を 1 つ持つ生成ヘッダ
- `.ppm` は **P6 形式で、変換後の画素**（元画像ではない。減色・2 値化・
  ディザの結果）。RGB565 に落ちた後の色を RGB888 で書く

いま入っているのは `tools/img2h.py`（実験用）の出力で、**仕組みが動くことの
確認**を兼ねている。正式ツールの出力に差し替えれば、そのまま検証になる。
"""

import struct
import sys
from pathlib import Path

import pytest

SKETCH = Path(__file__).parent
sys.path.insert(0, str(SKETCH))
import gen_sketch  # noqa: E402

# **収集時にスケッチを組み立てる。** dut が compile する前に済ませる必要がある
PAIRS = gen_sketch.build()

import tgfx_check as tc  # noqa: E402


def to565(im):
    """PIL の RGB を RGB565 の並びにする。**比較は 565 の空間でやる** ——
    TinyGFX が扱う色はそこまでしか無く、RGB888 で比べると PPM の丸めの
    往復で偽の差が出る。"""
    w, h = im.size
    px = im.load()
    return w, h, [((px[x, y][0] & 0xF8) << 8) | ((px[x, y][1] & 0xFC) << 3)
                  | (px[x, y][2] >> 3)
                  for y in range(h) for x in range(w)]


@pytest.mark.skipif(not PAIRS, reason="pairs/ にペアが無い")
def test_image_oracle(dut):
    dut.expect("TEST start image_oracle", timeout=20)
    dut.expect("TEST done", timeout=90)

    problems = []
    for name, w, h in PAIRS:
        from PIL import Image
        want_w, want_h, want = to565(
            Image.open(SKETCH / "pairs" / f"{name}.ppm").convert("RGB"))
        # スケッチの画面はいちばん大きいペアに合わせてあるので、左上を切り出す
        got = tc.image(SKETCH, name).crop((0, 0, want_w, want_h))
        _, _, cut = to565(got)

        diff = [i for i, (a, b) in enumerate(zip(want, cut)) if a != b]
        lit = sum(1 for c in cut if c)
        if not (0 < lit < want_w * want_h):
            problems.append(f"{name}: 絵が単色（点灯 {lit} / {want_w * want_h}）")
        if diff:
            i = diff[0]
            problems.append(
                f"{name}: {len(diff)}/{len(want)} 画素違う。"
                f"最初は ({i % want_w},{i // want_w}) "
                f"期待 {want[i]:#06x} / 実際 {cut[i]:#06x}")

    assert not problems, "変換結果と描画結果が違う:\n  " + "\n  ".join(problems)
