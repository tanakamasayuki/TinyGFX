"""**Does bit-banged I2C put the same bytes on the wire as Wire does?**

The waveform can be watched without hardware. The host core's pin hooks are
used to **model the I2C bus itself**:

- open drain: `OUTPUT`+`LOW` pulls down, `INPUT` lets go
- **the read hook plays the pull-up** - a released pin reads `HIGH`
- SDA is sampled on the rising edge of SCL; START / STOP are detected and the
  bytes reassembled

Those bytes are fed to a model of an SSD1306 and **compared against the same
picture drawn through Wire.** One byte of difference means the implementations
differ.

`TinyGFXBusSoftI2C` exists for the pins, not for the size. Hardware I2C only
comes out on fixed pins, and on a part as pin-poor as a CH32V003 those may be
wanted for something else. **On AVR it happens to be 1,444 B smaller too**,
because AVR's Wire carries a buffer and an interrupt-driven state machine.
"""

from pathlib import Path

import pytest

import tgfx_check as tc

SKETCH = Path(__file__).parent
W, H = 128, 64


def test_softi2c(dut):
    dut.expect("TEST start softi2c|TEST skip softi2c", timeout=20)
    if not (SKETCH / "output").exists():
        pytest.skip("this host core has no pin hooks")
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- it is a well-formed waveform ---------------------------------------
    assert r["soft_starts"] > 0, "not a single START was seen"
    assert r["soft_stops"] == r["soft_starts"], (
        f"{r['soft_starts']} STARTs against {r['soft_stops']} STOPs")
    assert r["soft_bytes_seen"] > 0, "not one byte could be read back"

    # --- the same amount of traffic as Wire ---------------------------------
    assert r["soft_data_bytes"] == r["wire_data_bytes"], (
        f"different pixel byte counts: soft {r['soft_data_bytes']} / "
        f"Wire {r['wire_data_bytes']}")

    # --- **the point.** The same picture ------------------------------------
    assert r["soft_vs_wire_diff"] == 0, (
        f"soft I2C and Wire differ by {r['soft_vs_wire_diff']} bytes")

    # "Matching" on a flat picture would prove nothing
    assert 0 < r["lit"] < W * H, f"the picture is one flat colour ({r['lit']} lit of {W * H})"
