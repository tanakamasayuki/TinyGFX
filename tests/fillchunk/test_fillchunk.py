"""`TINYGFX_FILL_CHUNK` は速さだけを変える — 絵も転送量も変えない。

このスイッチを入れると `TinyGFXBusSPI` が Arduino 標準の
`SPI.transfer(buf, len)`（ブロック転送）を使うようになる。**速さのためだけ**の
もので、線の上に出るバイトが変わってはいけない。

まとめ書きを持たないソフト SPI を基準にして、1 画素も違わないことを見る。
docs/MANUAL_TEST.ja.md M3 の「FILL_CHUNK を付けても絵が変わらない」を
実機を待たずにここで押さえる。
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
        pytest.skip("ホストの観測フックが無い")

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
    assert not diff, f"まとめ書きで絵が変わった: {len(diff)} 画素（先頭 {diff[:5]}）"

    assert r["plain_pixels"] == r["chunk_pixels"], (
        f"転送画素数が違う: {r['plain_pixels']} vs {r['chunk_pixels']}")
    assert r["plain_bytes"] == r["chunk_bytes"], (
        f"線に出たバイト数が違う: {r['plain_bytes']} vs {r['chunk_bytes']}")
