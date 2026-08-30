"""Does a page-addressed panel on SPI observe the bus etiquette?

**Stops the P0 found in the 2026-08-29 design review from coming back.**
`TinyGFXDriverPaged::beginTransaction()` and `endTransaction()` were empty, and
`cmd()` did not go through a bus transaction either. I2C starts and stops on
every transfer so nothing showed, and there was no SPI test, so nobody noticed.

On SPI it is fatal, and wrong twice over.

- without `SPI.beginTransaction()` neither the clock nor the mode is set
- CS never drops, so the panel is not listening in the first place
- an SD card on the same wires gets its transfer cut into

`docs/DEVELOPMENT_PLAN.ja.md` 1.1, P0.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
W, H = 128, 64

SPI_MSBFIRST = 1
SPI_MODE0 = 0


def test_monospi(dut):
    dut.expect("TEST start monospi|TEST skip monospi", timeout=20)
    if not (SKETCH / "output").exists():
        import pytest
        pytest.skip("this host core has no bus probe")
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- **the point.** Not one byte outside a transaction ------------------
    for phase in ("init", "draw", "invert", "sh"):
        assert r[f"{phase}_bytes"] > 0, f"{phase}: nothing was sent at all"
        assert r[f"{phase}_outside_txn"] == 0, (
            f"{phase}: {r[f'{phase}_outside_txn']} bytes went out outside "
            f"SPI.beginTransaction() (of {r[f'{phase}_bytes']})")
        assert r[f"{phase}_cs_high"] == 0, (
            f"{phase}: {r[f'{phase}_cs_high']} bytes went out with CS still HIGH")

    # --- SPISettings as intended --------------------------------------------
    assert r["spi_clock"] == 8000000, f"wrong clock: {r['spi_clock']}"
    assert r["spi_bitorder"] == SPI_MSBFIRST
    assert r["spi_mode"] == SPI_MODE0

    # --- traffic: only the pages that changed go out ------------------------
    assert r["draw_data_bytes"] == W * H // 8, (
        f"not every page was sent: {r['draw_data_bytes']}")

    # --- it is a picture, not one flat colour -------------------------------
    assert 0 < r["lit"] < W * H, f"the picture is one flat colour ({r['lit']} lit of {W * H})"

    # --- the wires are released when it is done -----------------------------
    assert r["cs_idle_high"] == 1, "CS was left LOW"
    assert r["in_transaction_at_end"] == 0, "a transaction was left open"
