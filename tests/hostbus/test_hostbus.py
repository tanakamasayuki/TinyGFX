"""The real bus implementations (BusSoftSPI / BusSPI), end to end on the host.

Uses the host core's bus probe (`HostBus.h` / `SPI.setTransferHook`). That is
not released yet, so a core without the probe skips.

Passing this means the layer BusCapture could not see has been checked: bit
order, when DC drops, and CS during a transaction.
"""

from pathlib import Path

from PIL import ImageChops

import tgfx_check as tc

SKETCH = Path(__file__).parent
BLUE, RED, GREEN, BLACK = tc.BLUE, tc.RED, tc.GREEN, tc.BLACK

SPI_MSBFIRST = 1
SPI_MODE0 = 0


def test_hostbus(dut, request):
    dut.expect("TEST start hostbus|TEST skip hostbus", timeout=20)
    if not (SKETCH / "output").exists():
        import pytest
        pytest.skip("this host core has no bus probe (unreleased)")
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- traffic ------------------------------------------------------------
    # a 32x32 fill + an 8x8 + one pixel = 1024 + 64 + 1 = 1089 pixels
    assert r["soft_pixels"] == 1089, f"software SPI sent {r['soft_pixels']} pixels"
    assert r["hw_pixels"] == 1089, f"hardware SPI sent {r['hw_pixels']} pixels"

    # Two bytes a pixel plus commands and window setup; the pixels are the floor
    assert r["soft_bytes"] >= 1089 * 2, f"software SPI sent too few bytes: {r['soft_bytes']}"
    assert r["hw_bytes"] >= 1089 * 2, f"hardware SPI sent too few bytes: {r['hw_bytes']}"

    # --- the two buses must draw the same picture (the invariant) -----------
    soft = tc.image(SKETCH, "soft")
    hw = tc.image(SKETCH, "hw")
    box = ImageChops.difference(soft, hw).getbbox()
    assert box is None, f"software and hardware SPI drew different pictures: bbox={box}"

    # --- the picture itself -------------------------------------------------
    px = soft.load()
    assert px[0, 0] == BLUE, f"the screen fill did not take: {px[0, 0]}"
    assert px[4, 4] == RED and px[11, 11] == RED, "the rectangle is missing"
    assert px[3, 4] == BLUE and px[12, 11] == BLUE, "the rectangle spilled"
    assert px[20, 3] == GREEN, "the single pixel is missing"

    # --- the SPI settings ---------------------------------------------------
    assert r["spi_clock"] == 24000000, f"the clock is {r['spi_clock']}"
    assert r["spi_bitorder"] == SPI_MSBFIRST, "not speaking MSB first"
    assert r["spi_mode"] == SPI_MODE0, f"SPI mode is {r['spi_mode']} (an ST7789 wants MODE0)"
    assert r["spi_in_transaction"] == 0, "a transaction was left open"

    # --- the pins when idle -------------------------------------------------
    assert r["cs_idle_high"] == 1, "CS stayed LOW outside a transfer"
    assert r["dc_idle_high"] == 1, "DC stayed LOW outside a transfer"
