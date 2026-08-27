"""テストスケッチが output/ に残したものを読むための小道具。"""

from pathlib import Path

from PIL import Image

BG = (0, 0, 0)


def rgb(c565: int) -> tuple:
    """RGB565 を、スケッチが PPM に書くのと同じ RGB888 に展開する。"""
    return ((c565 >> 8) & 0xF8, (c565 >> 3) & 0xFC, (c565 << 3) & 0xF8)


BLACK = rgb(0x0000)
WHITE = rgb(0xFFFF)
RED = rgb(0xF800)
GREEN = rgb(0x07E0)
BLUE = rgb(0x001F)


def report(sketch_dir) -> dict:
    """output/report.txt を key -> int の辞書で返す。"""
    path = Path(sketch_dir) / "output" / "report.txt"
    assert path.exists(), f"missing {path}（スケッチが tgfxTestDone() を呼んでいない？）"
    out = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            out[k.strip()] = int(v.strip())
    return out


def image(sketch_dir, name) -> Image.Image:
    path = Path(sketch_dir) / "output" / f"{name}.ppm"
    assert path.exists(), f"missing {path}"
    return Image.open(path).convert("RGB")


def lit(im, bg=BG) -> set:
    """背景でない画素の座標の集合。"""
    w, h = im.size
    px = im.load()
    return {(x, y) for y in range(h) for x in range(w) if px[x, y] != bg}


def colors(im) -> dict:
    """色 -> 画素数。"""
    return {c: n for n, c in im.getcolors(maxcolors=1 << 20)}
