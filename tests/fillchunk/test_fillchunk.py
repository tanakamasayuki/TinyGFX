"""`TINYGFX_FILL_CHUNK` changes speed and nothing else - not the picture, not
the traffic.

Turning it on makes `TinyGFXBusSPI` use Arduino's block transfer,
`SPI.transfer(buf, len)`. It exists **only** to go faster; the bytes that reach
the wire must not change.

Software SPI, which has no block write, is the baseline. Not one pixel may
differ. This pins docs/MANUAL_TEST.ja.md M3 ("FILL_CHUNK does not change the
picture") without waiting for hardware.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent


def test_fill_chunk_changes_nothing(dut):
    dut.expect("TEST start fillchunk", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)
    if "plain_bytes" not in r:
        import pytest
        pytest.skip("the host probe hook is not available")

    assert r["chunk_size"] == 32

    plain = tc.image(SKETCH, "plain")
    chunk = tc.image(SKETCH, "chunk")
    assert plain.size == chunk.size
    diff = [
        (x, y)
        for y in range(plain.size[1])
        for x in range(plain.size[0])
        if plain.getpixel((x, y)) != chunk.getpixel((x, y))
    ]
    assert not diff, f"block writes changed the picture: {len(diff)} pixels (first {diff[:5]})"

    assert r["plain_pixels"] == r["chunk_pixels"], (
        f"different pixel counts: {r['plain_pixels']} vs {r['chunk_pixels']}")
    assert r["plain_bytes"] == r["chunk_bytes"], (
        f"different byte counts on the wire: {r['plain_bytes']} vs {r['chunk_bytes']}")
