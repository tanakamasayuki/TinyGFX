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

    # 速い経路（バイト単位の塗り）が 1 画素ずつ描いた結果と一致すること。
    # 6 種類の矩形 x 回転 4 通り。**速いだけで絵が変わってはいけない。**
    assert r["fillrect_fastpath_diff"] == 0, (
        f"fillRect の速い経路で {r['fillrect_fastpath_diff']} バイト違う")

    # --- ページ方式パネルの寸法とバッファの契約（2026-08-29 レビューの P1）---
    #
    # 初期化列は 128x64 決め打ちだったのに、コンストラクタは任意の w / h を
    # 受けていた。128x32 に 64 行ぶんの多重比（0x3F）を送ると絵がガラスの
    # 半分に潰れる。**高さから導出する**ようにしたので、それを固定する。
    assert r["mux64"] == 0x3F, f"128x64 の多重比が {r['mux64']:#04x}"
    assert r["com64"] == 0x12, f"128x64 の COM ピンが {r['com64']:#04x}"
    assert r["mux32"] == 0x1F, f"128x32 の多重比が {r['mux32']:#04x}（64 行ぶんのまま？）"
    assert r["com32"] == 0x02, f"128x32 の COM ピンが {r['com32']:#04x}（64 行ぶんのまま？）"

    # --- begin() の契約 ------------------------------------------------------
    #
    # **返り値は「設定が使えるものか」**であって「線の向こうにパネルが
    # 本当に居るか」ではない（書き込み専用のパネルでは分からない）。
    # バッファが null、高さが 8 の倍数でない、幅が 0 —— どれもコンパイル時に
    # 分からないので、ここで落とす。
    assert r["begin_ok"] == 1, "まっとうな設定で begin() が false を返している"
    assert r["begin_null_buffer"] == 0, "バッファが null でも begin() が true"
    assert r["begin_odd_height"] == 0, "高さが 8 の倍数でなくても begin() が true"
    assert r["begin_zero_width"] == 0, "幅が 0 でも begin() が true"
