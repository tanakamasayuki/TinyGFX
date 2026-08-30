"""**Does a generated image header draw exactly the converter's expected image?**

The oracle specified by GfxImageToolJs spec 15.2. TinyGFX draws the `.h` the
converter produced and it is compared against **the `.ppm` the converter
produced**.

**A round trip through your own encoder and decoder proves nothing.** If both
sides share a misunderstanding they agree with each other. So the expected image
comes separately, as "the pixels after conversion", and this side only draws.

## How to use it

Drop `<name>.h` and `<name>.ppm` into `pairs/`. **No code needed** -
`gen_sketch.py` assembles the sketch at collection time.

- the `.h` is a generated header holding one `TinyGFXImageRef`
- the `.ppm` is **P6, and holds the pixels after conversion** - not the source
  image, but the result of quantising, thresholding and dithering. Write the
  colours as RGB888 after they have been reduced to RGB565

What is in there now comes from `tools/img2h.py` (the experimental converter),
which doubles as proof that the mechanism works. Swap in the real tool's output
and it becomes the verification it is meant to be.
"""

import struct
import sys
from pathlib import Path

import pytest

SKETCH = Path(__file__).parent
sys.path.insert(0, str(SKETCH))
import gen_sketch  # noqa: E402

# **Assemble the sketch at collection time**, before the dut compiles it
PAIRS = gen_sketch.build()

import tgfx_check as tc  # noqa: E402


def to565(im):
    """PIL RGB as a list of RGB565 values. **The comparison happens in 565**:
    that is as much colour as TinyGFX has, and comparing in RGB888 invents
    differences out of the PPM rounding."""
    w, h = im.size
    px = im.load()
    return w, h, [((px[x, y][0] & 0xF8) << 8) | ((px[x, y][1] & 0xFC) << 3)
                  | (px[x, y][2] >> 3)
                  for y in range(h) for x in range(w)]


@pytest.mark.skipif(not PAIRS, reason="no pairs in pairs/")
def test_image_oracle(dut):
    dut.expect("TEST start image_oracle", timeout=20)
    dut.expect("TEST done", timeout=90)

    problems = []
    for name, w, h in PAIRS:
        from PIL import Image
        want_w, want_h, want = to565(
            Image.open(SKETCH / "pairs" / f"{name}.ppm").convert("RGB"))
        # The canvas is sized to the largest pair, so crop the top left
        got = tc.image(SKETCH, name).crop((0, 0, want_w, want_h))
        _, _, cut = to565(got)

        diff = [i for i, (a, b) in enumerate(zip(want, cut)) if a != b]
        # An all-one-colour picture would match an all-one-colour reference and
        # prove nothing. Count the colours rather than the lit pixels: a
        # photograph has no black in it at all, and "every pixel is non-zero"
        # is not the same as "every pixel is the same".
        if len(set(cut)) < 2:
            problems.append(f"{name}: the picture is one flat colour")
        if diff:
            i = diff[0]
            problems.append(
                f"{name}: {len(diff)}/{len(want)} pixels differ. "
                f"First at ({i % want_w},{i // want_w}): "
                f"expected {want[i]:#06x}, got {cut[i]:#06x}")

    assert not problems, "conversion and drawing disagree:\n  " + "\n  ".join(problems)
