"""Produce the golden on the host and freeze it, for the hardware to be checked
against.

**The golden is never made from the hardware's own output.** That would
"match" even when the hardware is wrong. What the host draws
(`TinyGFXBusCapture`) is the truth; the hardware side (`tests/hw/m5stack/`) is
held to it. The scene itself is defined in exactly one place,
`tests/common_libs/tgfx_test/src/tgfx_scene.h`.
"""

from pathlib import Path

import pytest
import tgfx_check as tc

SKETCH = Path(__file__).parent
GOLDEN = SKETCH / "golden" / "scene.ppm"


def test_scene_matches_golden(dut):
    dut.expect("TEST start scene", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)
    # Reading back from real glass returns RGB666. Restricting the scene to
    # saturated colours is what makes a byte-for-byte comparison possible.
    assert r["unsaturated"] == 0, (
        f"{r['unsaturated']} pixels are not a saturated colour. Those come back "
        "1 LSB off through the hardware read-back, so keep them out of the scene")

    produced = (SKETCH / "output" / "scene.ppm").read_bytes()
    if not GOLDEN.exists():
        GOLDEN.parent.mkdir(parents=True, exist_ok=True)
        GOLDEN.write_bytes(produced)
        pytest.fail(
            f"no golden existed, so one was written: {GOLDEN}\n"
            "**Look at it before committing it.** Once frozen, a change means "
            "deciding whether the scene changed or something broke")

    assert produced == GOLDEN.read_bytes(), (
        "the scene differs from the golden. If the scene was changed on purpose, "
        "update golden/scene.ppm and re-run the hardware side (tests/hw/m5stack/)")
