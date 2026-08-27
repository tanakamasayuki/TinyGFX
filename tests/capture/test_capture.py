"""BusCapture が ST7789 のコマンド列から画を復元できること（Tier 1 の土台）。

これが通らないと、以降の描画テストは書けない。
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
BLUE, RED, GREEN, BLACK = tc.BLUE, tc.RED, tc.GREEN, tc.BLACK


def test_capture(dut):
    dut.expect("TEST start capture", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    assert r["init_commands"] > 5, f"初期化列が短すぎる: {r['init_commands']}"

    # --- 転送画素数は過不足なく一致すること ---------------------------------
    assert r["fillscreen_pixels"] == 64 * 64, f"fillScreen が {r['fillscreen_pixels']} 画素"
    assert r["fillrect_pixels"] == 8 * 8, f"fillRect(8x8) が {r['fillrect_pixels']} 画素"
    assert r["pixel_pixels"] == 1, f"drawPixel が {r['pixel_pixels']} 画素"

    # --- ウィンドウの値 -----------------------------------------------------
    assert (r["win_xs"], r["win_ys"]) == (3, 5)
    assert (r["win_xe"], r["win_ye"]) == (12, 11)  # x+w-1, y+h-1
    assert (r["off_xs"], r["off_ys"]) == (2, 1), "原点オフセットがウィンドウに乗っていない"

    assert r["txn_depth"] == 0, "startWrite / endWrite が釣り合っていない"

    # --- 復元された画 -------------------------------------------------------
    img = tc.image(SKETCH, "fillscreen")
    assert img.size == (64, 64)
    assert tc.colors(img) == {BLUE: 64 * 64}, f"全面が青一色でない: {tc.colors(img)}"

    img = tc.image(SKETCH, "fillrect")
    assert img.getpixel((4, 4)) == RED and img.getpixel((11, 11)) == RED
    for p in [(3, 4), (12, 11), (4, 3), (4, 12)]:
        assert img.getpixel(p) == BLACK, f"矩形が {p} へはみ出している"
    assert tc.colors(img)[RED] == 64

    img = tc.image(SKETCH, "pixel")
    assert img.getpixel((9, 3)) == GREEN
    assert tc.colors(img)[GREEN] == 1
