"""SPI に繋いだページ方式パネルが、バスの作法を守るか。

**2026-08-29 の設計レビューで見つかった P0 の再発防止。**
`TinyGFXPanelPaged` の `beginTransaction()` / `endTransaction()` が空で、
`cmd()` もバスのトランザクションを通っていなかった。I2C は転送ごとに開始と
停止をするので露見せず、SPI のテストが無かったので誰も気づかなかった。

SPI では致命的で、二重に悪い。

- `SPI.beginTransaction()` を通らないとクロックもモードも決まらない
- CS が落ちないので、そもそもパネルが聞いていない
- 同じ線に SD カードが居ると、そちらの転送に割り込む

`docs/DEVELOPMENT_PLAN.ja.md` §1.1 の P0。
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
        pytest.skip("バス観測口を持たないホストコア")
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- **本命。** 1 バイトもトランザクションの外に出ていないこと -----------
    for phase in ("init", "draw", "invert", "sh"):
        assert r[f"{phase}_bytes"] > 0, f"{phase}: 何も出ていない"
        assert r[f"{phase}_outside_txn"] == 0, (
            f"{phase}: {r[f'{phase}_outside_txn']} バイトが "
            f"SPI.beginTransaction() の外に出ている（全 {r[f'{phase}_bytes']}）")
        assert r[f"{phase}_cs_high"] == 0, (
            f"{phase}: {r[f'{phase}_cs_high']} バイトが CS=HIGH のまま出ている")

    # --- SPISettings が意図どおり -------------------------------------------
    assert r["spi_clock"] == 8000000, f"クロックが違う: {r['spi_clock']}"
    assert r["spi_bitorder"] == SPI_MSBFIRST
    assert r["spi_mode"] == SPI_MODE0

    # --- 転送量。変更のあったページだけ流れる -------------------------------
    assert r["draw_data_bytes"] == W * H // 8, (
        f"全ページ流れていない: {r['draw_data_bytes']}")

    # --- 絵になっていること（単色でない） ------------------------------------
    assert 0 < r["lit"] < W * H, f"絵が単色（点灯 {r['lit']} / {W * H}）"

    # --- 終わったら線を手放していること --------------------------------------
    assert r["cs_idle_high"] == 1, "CS が LOW のまま残っている"
    assert r["in_transaction_at_end"] == 0, "トランザクションが開いたまま残っている"
