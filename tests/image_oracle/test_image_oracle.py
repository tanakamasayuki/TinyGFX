"""**Does a generated image header draw exactly the converter's expected image?**

The oracle specified by GfxImageToolJs spec 15.2. TinyGFX draws the header the
converter produced and it is compared against **the picture the converter
produced**.

**A round trip through your own encoder and decoder proves nothing.** If both
sides share a misunderstanding they agree with each other. So the expected image
comes separately, as "the pixels after conversion", and this side only draws.

Two conversions have no second opinion available at all:

- **dithering** - error diffusion is an implementation, not a formula. Nothing
  here can say which pixels should have come out lit
- **colour reduction** - which 16 colours the quantiser keeps is its choice

Everything else could in principle be recomputed from the source, and used to
be. **These two are why the tool has to emit the expected image at all.**

## How to use it

Drop a `.png` in `sources/`, name it in `sources/.imagesconfig`, run
`regen.py`. **No code needed** - `gen_sketch.py` assembles the sketch at
collection time from whatever `generated/images.h` holds.

`generated/` and `expected/` are committed, so the comparison runs without the
converter installed. `regen.py --check` is what notices they have gone stale.
"""

import sys
from pathlib import Path

import pytest

SKETCH = Path(__file__).parent
sys.path.insert(0, str(SKETCH))
import gen_sketch  # noqa: E402
import regen  # noqa: E402

# **Assemble the sketch at collection time**, before the dut compiles it
CASES = gen_sketch.build()

import tgfx_check as tc  # noqa: E402

# Every decoder TinyGFX ships. **A case per decoder, or the suite is measuring
# less than it looks like it is.** They are not interchangeable: rlepal4 reads a
# palette, bitmap1v walks the rows eight at a time, raw565 does neither.
DECODERS = {"Raw565", "Rle565", "Rlepal4", "Bitmap1h", "Bitmap1v"}


def to565(im):
    """PIL RGB as a list of RGB565 values. **The comparison happens in 565**:
    that is as much colour as TinyGFX has, and comparing in RGB888 invents
    differences out of how the converter expanded 5 bits back to 8."""
    w, h = im.size
    px = im.load()
    return w, h, [((px[x, y][0] & 0xF8) << 8) | ((px[x, y][1] & 0xFC) << 3)
                  | (px[x, y][2] >> 3)
                  for y in range(h) for x in range(w)]


@pytest.mark.skipif(not CASES, reason="generated/images.h is empty; run regen.py")
def test_every_decoder_is_covered():
    """**Not a property of TinyGFX - a property of this test folder.**

    The converter chooses a format per image, and it chooses globally: a
    decoder is paid for once, so one image needing rle565 can pull every other
    image onto rle565 too. That is correct behaviour and it would quietly leave
    three of the five decoders undrawn. `.imagesconfig` pins each format; this
    is what notices if a pin stops working.
    """
    got = {ops for _, _, _, ops in CASES}
    assert DECODERS <= got, f"no case draws {sorted(DECODERS - got)}"


def test_committed_output_is_current():
    """**The comparison is only as good as the files it compares.**

    `generated/` and `expected/` are committed so the suite runs without the
    converter installed - and so a change in what the converter emits would
    otherwise go unnoticed, the test still passing against yesterday's answer.
    Skipped where the converter is not present, which is most machines.
    """
    import subprocess
    if not regen.DEFAULT_TOOL.exists():
        pytest.skip(f"gfx-image-tool.js not at {regen.DEFAULT_TOOL}")
    r = subprocess.run([sys.executable, str(SKETCH / "regen.py"), "--check"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


@pytest.mark.skipif(not CASES, reason="generated/images.h is empty; run regen.py")
def test_image_oracle(dut):
    dut.expect("TEST start image_oracle", timeout=20)
    dut.expect("TEST done", timeout=90)

    from PIL import Image

    problems = []
    for name, w, h, ops in CASES:
        want_w, want_h, want = to565(
            Image.open(SKETCH / "expected" / f"{name}.png").convert("RGB"))
        # The canvas is sized to the largest image, so crop the top left
        got = tc.image(SKETCH, name).crop((0, 0, want_w, want_h))
        _, _, cut = to565(got)

        diff = [i for i, (a, b) in enumerate(zip(want, cut)) if a != b]
        # An all-one-colour picture would match an all-one-colour reference and
        # prove nothing. Count the colours rather than the lit pixels: a
        # photograph has no black in it at all, and "every pixel is non-zero"
        # is not the same as "every pixel is the same".
        if len(set(cut)) < 2:
            problems.append(f"{name} ({ops}): the picture is one flat colour")
        if diff:
            i = diff[0]
            problems.append(
                f"{name} ({ops}): {len(diff)}/{len(want)} pixels differ. "
                f"First at ({i % want_w},{i // want_w}): "
                f"expected {want[i]:#06x}, got {cut[i]:#06x}")

    assert not problems, "conversion and drawing disagree:\n  " + "\n  ".join(problems)
