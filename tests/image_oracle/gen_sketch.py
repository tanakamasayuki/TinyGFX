"""`pairs/` に置かれた <名前>.h と <名前>.ppm から検証スケッチを組み立てる。

**ペアを足すのにコードを書かなくていい**ようにするための生成器。
pytest の収集時に呼ばれ、`image_oracle.ino` を書き出す。
"""

import re
from pathlib import Path

HERE = Path(__file__).parent
PAIRS = HERE / "pairs"

HEAD = '''// **生成された画像ヘッダが、変換後の期待画像と 1 画素も違わないか。**
//
// GfxImageToolJs 仕様書 §15.2 が指定しているオラクル。ツールが出した .h を
// TinyGFX が実際に描き、ツールが出した期待画像（.ppm）と突き合わせる。
//
// **自作の encode と decode の往復では正しさを証明できない。** 符号化側と
// 復号側が同じ勘違いをしていたら一致してしまう。だから期待画像は
// 「変換後の画素」として別途出してもらい、こちらは描くだけにする。
//
// **このファイルは gen_sketch.py が生成する。手で編集しない。**
// pairs/ に <名前>.h と <名前>.ppm を置けば、次の実行で拾われる。
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelMemory.h>
#include <TinyGFX/Image.h>
#include <tgfx_test.h>
'''

BODY = '''
static const int W = %(w)d, H = %(h)d;
static uint16_t gram[W * H];
TinyGFXPanelMemory panel(gram, W, H);
TinyGFX lcd(panel);

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("image_oracle");
  lcd.begin();
%(cases)s
  tgfxTestDone();
}
void loop() {}
'''

CASE = '''
  // ---- %(name)s ----
  panel.fillBuffer(0x0000);
  lcd.drawImage(&%(sym)s, 0, 0);
  tgfxShot("%(name)s", gram, W, H);
'''


def ppm_size(path):
    """P6 の幅と高さだけ読む。"""
    with open(path, "rb") as f:
        magic = f.readline().strip()
        if magic != b"P6":
            raise ValueError(f"{path}: P6 ではない（{magic!r}）")
        line = f.readline()
        while line.startswith(b"#"):
            line = f.readline()
        w, h = (int(v) for v in line.split())
    return w, h


def find_ref_symbol(header):
    """生成ヘッダから TinyGFXImageRef のシンボル名を拾う。"""
    m = re.search(r"TinyGFXImageRef\s+(\w+)\s*=", Path(header).read_text())
    if not m:
        raise ValueError(f"{header}: TinyGFXImageRef が見つからない")
    return m.group(1)


def build():
    """ペアを集めてスケッチを書く。戻り値は [(名前, 幅, 高さ), ...]。"""
    pairs = []
    for h in sorted(PAIRS.glob("*.h")):
        ppm = h.with_suffix(".ppm")
        if not ppm.exists():
            raise FileNotFoundError(f"{h.name} に対応する {ppm.name} が無い")
        w, hh = ppm_size(ppm)
        pairs.append((h.stem, w, hh, find_ref_symbol(h)))

    if not pairs:
        # ペアが無くてもビルドは通るようにする（skip 判定は pytest 側）
        (HERE / "image_oracle.ino").write_text(
            HEAD + BODY % {"w": 1, "h": 1, "cases": "  // ペアなし"})
        return []

    # **画面はいちばん大きいペアに合わせる。** 小さい画像は左上に描く
    w = max(p[1] for p in pairs)
    h = max(p[2] for p in pairs)
    includes = "".join(f'#include "pairs/{n}.h"\n' for n, _, _, _ in pairs)
    cases = "".join(CASE % {"name": n, "sym": s} for n, _, _, s in pairs)
    (HERE / "image_oracle.ino").write_text(
        HEAD + includes + BODY % {"w": w, "h": h, "cases": cases})
    return [(n, pw, ph) for n, pw, ph, _ in pairs]
