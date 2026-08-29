"""**同じ絵をどの形式で符号化しても、1 画素も違わないこと。**

変換ツール（`tools/img2h.py`）は絵ごとに最小の形式を総当たりで選ぶ。
生 RGB565 / RLE / RLE+パレット / 1bpp 横詰め / 1bpp 縦詰め —— **どれを
選んだかがスケッチから見えてはいけない。** それを固定するのがこのテスト。

CellFont で「3 通りの符号化が同じ画素を描く」（`tests/text/`）を固定したのと
同じ考え方。符号化はツールの都合であって、利用者の関心事ではない。

形式の選び方と実測は [docs/IMAGE_FORMAT.ja.md](../../docs/IMAGE_FORMAT.ja.md)。
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
W = H = 32


def test_image_fmt(dut):
    dut.expect("TEST start image_fmt", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- 絵が出ていること（全部真っ黒で「一致」しても意味がない） ------------
    assert 0 < r["raw_lit"] < W * H, f"生 RGB565 の絵が単色（点灯 {r['raw_lit']}）"
    assert 0 < r["mono_lit"] < W * H, f"1bpp の絵が単色（点灯 {r['mono_lit']}）"

    # --- **本命。** 形式が違っても絵は同じ -----------------------------------
    assert r["rle565_diff"] == 0, f"RLE が生と {r['rle565_diff']} 画素違う"
    assert r["rlepal4_diff"] == 0, f"RLE+パレットが生と {r['rlepal4_diff']} 画素違う"
    assert r["mono_v_diff"] == 0, (
        f"1bpp の縦詰めが横詰めと {r['mono_v_diff']} 画素違う")

    # --- クリップが効くこと ---------------------------------------------------
    assert r["clip_outside"] == 0, (
        f"クリップの外に {r['clip_outside']} 画素描いている")

    # --- 画面外は 1 画素も送らない -------------------------------------------
    assert r["offscreen_pixels"] == 0, (
        f"完全に画面外の画像で {r['offscreen_pixels']} 画素送っている")

    # --- raw565 は 1 行に窓を 1 つしか開かない ---------------------------------
    #
    # 写真は連長が 1 なので、連ごとに fillRect を呼ぶと**画素ごとに窓が開く**。
    # 実測でそうなっていた: 32x32 の雑音で **1,024 画素に対しコマンド 3,072 個**、
    # 1 画素 13 バイト。1 行 1 窓にして 96 個になった（docs/IMAGE_FORMAT.ja.md）。
    #
    # コマンドは窓 1 つにつき 3 個（CASET / RASET / RAMWR）。
    assert r["photo_pixels"] == W * H, (
        f"64x64 を 32x32 に描いて {r['photo_pixels']} 画素。{W*H} のはず")
    assert r["photo_cmds"] == 3 * H, (
        f"窓が {r['photo_cmds'] / 3:.0f} 個。行数 {H} と同じでなければならない")

    # 左上へはみ出しても同じ。自前でクリップする経路なので、**画面外に書かない**
    # ことと画素数が変わらないことを見る。
    assert r["photo_off_pixels"] == W * H, (
        f"はみ出させて {r['photo_off_pixels']} 画素。{W*H} のはず")
    assert r["photo_off_cmds"] == 3 * H

    # クリップ内はクリップ無しと 1 画素も違わず、外は 1 画素も触られないこと。
    assert r["photo_clip_in_diff"] == 0, (
        f"クリップ内が {r['photo_clip_in_diff']} 画素違う")
    assert r["photo_clip_out_lit"] == 0, (
        f"クリップの外に {r['photo_clip_out_lit']} 画素描いている")
    assert r["photo_clip_pixels"] == 16 * 16, (
        f"16x16 のクリップで {r['photo_clip_pixels']} 画素送っている")
    assert r["photo_clip_cmds"] == 3 * 16

    # 極端な座標では 1 画素も送らないこと。`clipX0 - x` の桁溢れで c0 が負に
    # 化けると、**画像データの手前を読んで**画面に出る。
    assert r["photo_extreme_pixels"] == 0, (
        f"完全に画面外なのに {r['photo_extreme_pixels']} 画素送っている")
    assert r["photo_extreme_cmds"] == 0, (
        f"完全に画面外なのに窓を {r['photo_extreme_cmds'] / 3:.0f} 個開いている")

    # --- 透過 -----------------------------------------------------------------
    #
    # 同じ画像を透過あり／無しで描く。**透過色の画素だけが下地を残し、
    # それ以外は 1 画素も違わないこと。**
    #
    # 透過を見るかは形式ではなく ops が決める（生成ヘッダが指す）ので、
    # 透過の無い画像しか使わないスケッチには判定のコードが載らない。
    # 実費は 24〜66 B（形式と MCU による。docs/IMAGE_FORMAT.ja.md）。
    assert r["opaque_bg_left"] == 0, (
        f"透過なしなのに下地が {r['opaque_bg_left']} 画素残っている")
    assert r["trans_bg_left"] > 0, "透過ありなのに下地が 1 画素も残っていない"
    assert r["trans_bg_left"] < W * H, "透過ありで全部下地のまま（何も描けていない）"

    # 違うのは透過色の画素だけ。それ以外は同じ絵。
    assert r["trans_differ"] == r["trans_bg_left"], (
        f"透過色以外も違っている: 差 {r['trans_differ']} / "
        f"下地の残り {r['trans_bg_left']}")
