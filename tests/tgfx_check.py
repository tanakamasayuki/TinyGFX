"""Small tools for reading what a test sketch left in output/."""

from pathlib import Path

from PIL import Image

BG = (0, 0, 0)


def rgb(c565: int) -> tuple:
    """Expand RGB565 the same way the sketch does when it writes the PPM."""
    return ((c565 >> 8) & 0xF8, (c565 >> 3) & 0xFC, (c565 << 3) & 0xF8)


BLACK = rgb(0x0000)
WHITE = rgb(0xFFFF)
RED = rgb(0xF800)
GREEN = rgb(0x07E0)
BLUE = rgb(0x001F)


def report(sketch_dir) -> dict:
    """output/report.txt as a key -> int dictionary."""
    path = Path(sketch_dir) / "output" / "report.txt"
    assert path.exists(), f"missing {path} (did the sketch call tgfxTestDone()?)"
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
    """The coordinates of every pixel that is not the background."""
    w, h = im.size
    px = im.load()
    return {(x, y) for y in range(h) for x in range(w) if px[x, y] != bg}


def colors(im) -> dict:
    """Colour -> pixel count."""
    return {c: n for n, c in im.getcolors(maxcolors=1 << 20)}
