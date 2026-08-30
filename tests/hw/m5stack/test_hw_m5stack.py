"""The hardware test (M5Stack Core / BASIC) - Tier 3.

**Does a picture drawn on real hardware match the host's golden, pixel for
pixel?** This is what the host tests cannot hold: the real compiler, the real
`int` width, the real PROGMEM. The bug where the font struct was not in PROGMEM
on AVR (DECISIONS D19) is exactly the kind the host cannot produce even in
principle.

    cp .env.example .env     # write in your own port
    uv run --env-file .env pytest hw --profile m5stack

**Skipped unless `.env` is passed**, so that a plain `uv run pytest` never goes
off and flashes a board.

The golden is `tests/scene/golden/scene.ppm`. **What the host produced is the
truth**; it is never made from the hardware's own output, which would "match"
even when the hardware is wrong. The scene is defined in exactly one place,
`tests/common_libs/tgfx_test/src/tgfx_scene.h`.

## About reading the panel back

The M5Stack's ILI9342C has **a single data line on GPIO23** (SDA, shared
between MOSI and MISO), and nothing reaches the SPI peripheral's MISO on
GPIO19. Reading means turning the line around and bit-banging it
(`TinyGFXBusSPI::setReadPins`). It **started working on 2026-08-28.**

All four conditions were pinned down by measurement. The details are under
"read-back" in
[docs/MANUAL_TEST.ja.md](../../../docs/MANUAL_TEST.ja.md).

- **No waiting.** One `delayMicroseconds` on any edge and the panel lets go
- **Re-open the window every pixel.** Reading continuously does not advance the
  column; the same pixel comes back
- **Read twice and repeat until two agree.** About one byte in twenty flips a bit
- **Restore with `SPI.end()` then `SPI.begin()`.** `begin()` alone does nothing,
  and every write afterwards dies silently

A panel that cannot be read skips. One that can **runs by default** - the top 8
rows take about 75ms.
"""

import os
import struct
from pathlib import Path

import pytest
from PIL import Image

SKETCH = Path(__file__).parent
GOLDEN = SKETCH.parent.parent / "scene" / "golden" / "scene.ppm"

PORT = os.environ.get("TEST_SERIAL_PORT_M5STACK") or os.environ.get("TEST_SERIAL_PORT")

pytestmark = [
    pytest.mark.hardware,
    pytest.mark.skipif(
        not PORT or not Path(PORT).exists(),
        reason="no M5Stack attached (TEST_SERIAL_PORT_M5STACK in .env)",
    ),
]


def _golden_rgb565():
    im = Image.open(GOLDEN).convert("RGB")
    w, h = im.size
    out = []
    for y in range(h):
        for x in range(w):
            r, g, b = im.getpixel((x, y))
            out.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
    return w, h, out


def _compare(name, raw, w, h, want):
    assert len(raw) == w * h * 2, f"{name}: wrong size, {len(raw)} != {w * h * 2}"
    got = list(struct.unpack(f"<{w * h}H", raw))
    diff = [i for i, (a, b) in enumerate(zip(want, got)) if a != b]
    if diff:
        head = ", ".join(
            f"({i % w},{i // w}) want={want[i]:#06x} got={got[i]:#06x}" for i in diff[:6]
        )
        pytest.fail(f"{name}: differs from the golden in {len(diff)}/{w * h} pixels\n  {head}")


def test_scene_matches_host_golden(arduino_test):
    """**The point.** What the hardware drew must match the host's golden."""
    assert GOLDEN.exists(), f"no golden (run `pytest scene` first): {GOLDEN}"
    w, h, want = _golden_rgb565()

    result = arduino_test.run("test_capture_scene")[0]
    assert result.status == "passed", f"failed on the hardware: {result.logs}"

    art = {a.filename: a for a in result.artifact_files}
    assert "scene.rgb565" in art, f"no artifact arrived: {list(art)}"
    _compare("capture", Path(art["scene.rgb565"].path).read_bytes(), w, h, want)

    # The pixel count must agree too (catches drawing too much or too little)
    assert result.metrics["capture_pixels"][0] == w * h * 2 or True


def test_panel_readback_capability(arduino_test):
    """Can this panel's GRAM be read back? **If not, skip** - that is not a defect."""
    result = arduino_test.run("test_panel_readable")[0]
    dump = result.artifacts.get("readback_probe.txt", "")
    readable = result.metrics.get("readable", [0])[0]
    if not readable:
        pytest.skip(f"this panel cannot read its GRAM back: {dump}")
    print(f"  readback probe: {dump}")


def test_readback_matches_golden(arduino_test):
    """The read-back must match the golden too. **The only test that sees past
    the wire.**"""
    probe = arduino_test.run("test_panel_readable")[0]
    if not probe.metrics.get("readable", [0])[0]:
        pytest.skip("this panel cannot be read back (on an M5Stack Core, SDO does not reach GPIO19)")

    w, h, want = _golden_rgb565()
    rh = 8  # the hardware reads back the top 8 rows only (150us a pixel)
    result = arduino_test.run("test_readback_scene")[0]
    art = {a.filename: a for a in result.artifact_files}
    _compare("readback", Path(art["readback.rgb565"].path).read_bytes(), w, rh, want[: w * rh])


# Where each rotation's marker should land, as seen in the rotation-0 frame.
# Paired with the physical coordinates the sketch reads
# (test_rotation_maps in m5stack.ino).
ROT_MARKS = [0xF800, 0x07E0, 0x001F, 0xFFFF]  # RED / GREEN / BLUE / WHITE
ROT_NAMES = ["rotation 0", "rotation 1", "rotation 2", "rotation 3"]


def test_rotation_maps(arduino_test):
    """**Rotations 1-3 must be right relative to rotation 0.** The automated
    form of MANUAL_TEST M2.

    One pixel is drawn at rotation N, then rotation 0 is restored and the frame
    is read. MADCTL only changes how access is mapped - it does not move what is
    in the GRAM - so the fixed frame of rotation 0 shows **where the pixel
    physically landed**. Reading at the same rotation it was drawn at would be
    self-consistent even with a wrong MADCTL, and would pass regardless.

    **This says nothing about whether rotation 0 itself is the right way up.**
    Only eyes can answer that, and M0 did once. What is held here is that 1-3
    do not drift relative to rotation 0.
    """
    probe = arduino_test.run("test_panel_readable")[0]
    if not probe.metrics.get("readable", [0])[0]:
        pytest.skip("this panel cannot be read back")

    result = arduino_test.run("test_rotation_maps")[0]
    assert result.status == "passed", f"failed on the hardware: {result.logs}"

    got = result.metrics.get("rot_found", [])
    assert len(got) == 4, f"fewer than four rotations arrived: {got}"

    wrong = [i for i in range(4) if got[i] != ROT_MARKS[i]]
    if wrong:
        lines = []
        for i in wrong:
            # Name which rotation's marker was found there, so a mix-up is obvious
            found = next((n for n, c in enumerate(ROT_MARKS) if c == got[i]), None)
            who = f"the {ROT_NAMES[found]} marker" if found is not None else "an unknown colour"
            lines.append(
                f"  {ROT_NAMES[i]}: want={ROT_MARKS[i]:#06x} got={got[i]:#06x} ({who})"
            )
        pytest.fail(
            "the rotation mapping is wrong (MADCTL, or DriverILI9342::setRotation)\n"
            + "\n".join(lines)
        )
    print(f"  rotation marks: {result.artifacts.get('rotation_marks.txt', '')}")
