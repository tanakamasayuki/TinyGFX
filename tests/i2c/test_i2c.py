"""I2C のモノクロ OLED（SSD1306）を通しで検証する。

ホストの Wire 観測フックで、本番の `TinyGFXBusI2C` が実際に流したバイトを
拾い、SSD1306 の模型で組み立て直している。コアは SSD1306 も I2C も知らない。

SPI のカラーパネルとの違いを見るのが主眼:
  - フレームバッファを持つ（RGB565 を「0 でなければ点灯」で 1bpp に落とす）
  - `display()` を呼ぶまで転送されない
  - 変更のあったページだけ流す
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
W, H = 128, 64
PAGE_BYTES = W  # 1 ページ = 横 128 バイト（縦 8 画素）
WHITE, BLACK = tc.WHITE, tc.BLACK


def test_i2c(dut):
    dut.expect("TEST start i2c", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- 初期化がトランザクションとして流れていること -----------------------
    assert r["init_transactions"] >= 20, (
        f"初期化列が短すぎる: {r['init_transactions']} トランザクション")

    # --- display() を呼ぶまで 1 バイトも流れない ----------------------------
    assert r["bytes_before_display"] == 0, (
        f"display() 前に {r['bytes_before_display']} バイト流れている")
    assert r["bytes_full"] == PAGE_BYTES * 8, (
        f"全面転送が {r['bytes_full']} バイト（{PAGE_BYTES * 8} のはず）")

    # --- 変わったページだけ流れる -------------------------------------------
    assert r["bytes_one_page"] == PAGE_BYTES, (
        f"1 ページぶんの変更で {r['bytes_one_page']} バイト流れている"
        f"（{PAGE_BYTES} のはず）。ダーティ判定が効いていない")

    # --- 回転 ---------------------------------------------------------------
    assert (r["rot1_w"], r["rot1_h"]) == (H, W), (
        f"回転 1 で {r['rot1_w']}x{r['rot1_h']}（{H}x{W} のはず）")

    # --- 画 -----------------------------------------------------------------
    full = tc.image(SKETCH, "full")
    assert tc.colors(full) == {WHITE: W * H}, (
        f"全面塗りが一色になっていない: {tc.colors(full)}")

    one = tc.image(SKETCH, "onepage").load()
    assert one[8, 8] == WHITE and one[39, 15] == WHITE, "矩形が出ていない"
    assert one[7, 8] == BLACK and one[40, 8] == BLACK, "矩形が横へはみ出している"
    assert one[8, 7] == BLACK and one[8, 16] == BLACK, "矩形が縦へはみ出している"

    scene = tc.image(SKETCH, "scene").load()
    assert scene[0, 0] == WHITE and scene[W - 1, H - 1] == WHITE, "枠の角が無い"
    assert scene[96, 32] == WHITE, "円の中心が塗られていない"
    assert scene[96, 32 - 13] == BLACK, "円が半径より大きい"
    assert scene[64, 40] == BLACK, "何も無いはずの場所が塗られている"

    # 回転 1 の左上 (0,0)-(7,23) は、パネル座標では x=0..23 / y=56..63
    rot = tc.image(SKETCH, "rot1").load()
    assert rot[0, 63] == WHITE and rot[23, 56] == WHITE, "回転後の矩形の位置が違う"
    assert rot[24, 63] == BLACK and rot[0, 55] == BLACK, "回転後の矩形がはみ出している"
