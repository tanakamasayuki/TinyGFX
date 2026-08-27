"""本番のバス実装（BusSoftSPI / BusSPI）をホストで通しで検証する。

ホストコアのバス観測口（`HostBus.h` / `SPI.setTransferHook`）を使う。
まだ未リリースなので、観測口を持たないコアでは skip する。

これが通ると、BusCapture では見えなかった層 —— ビット順、DC を落とす
タイミング、トランザクション中の CS —— が検証できたことになる。
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
        pytest.skip("ホストコアにバス観測口がない（未リリース）")
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- 転送量 -------------------------------------------------------------
    # 32x32 全面 + 8x8 + 1 画素 = 1024 + 64 + 1 = 1089 画素
    assert r["soft_pixels"] == 1089, f"ソフト SPI で {r['soft_pixels']} 画素"
    assert r["hw_pixels"] == 1089, f"ハードウェア SPI で {r['hw_pixels']} 画素"

    # 画素 2 バイト + コマンドとウィンドウ指定。画素ぶんは必ず含まれる
    assert r["soft_bytes"] >= 1089 * 2, f"ソフト SPI のバイト数が足りない: {r['soft_bytes']}"
    assert r["hw_bytes"] >= 1089 * 2, f"ハードウェア SPI のバイト数が足りない: {r['hw_bytes']}"

    # --- 2 つのバスは同じ絵を出すこと（本題の不変条件） ----------------------
    soft = tc.image(SKETCH, "soft")
    hw = tc.image(SKETCH, "hw")
    box = ImageChops.difference(soft, hw).getbbox()
    assert box is None, f"ソフト SPI とハードウェア SPI で絵が違う: bbox={box}"

    # --- 画そのもの ----------------------------------------------------------
    px = soft.load()
    assert px[0, 0] == BLUE, f"全面塗りが効いていない: {px[0, 0]}"
    assert px[4, 4] == RED and px[11, 11] == RED, "矩形が出ていない"
    assert px[3, 4] == BLUE and px[12, 11] == BLUE, "矩形がはみ出している"
    assert px[20, 3] == GREEN, "1 画素が出ていない"

    # --- SPI の設定 ----------------------------------------------------------
    assert r["spi_clock"] == 24000000, f"クロックが {r['spi_clock']}"
    assert r["spi_bitorder"] == SPI_MSBFIRST, "MSB first で喋っていない"
    assert r["spi_mode"] == SPI_MODE0, f"SPI モードが {r['spi_mode']}（ST7789 は MODE0）"
    assert r["spi_in_transaction"] == 0, "トランザクションが開いたままになっている"

    # --- アイドル時のピン ----------------------------------------------------
    assert r["cs_idle_high"] == 1, "転送外で CS が LOW のまま"
    assert r["dc_idle_high"] == 1, "転送外で DC が LOW のまま"
