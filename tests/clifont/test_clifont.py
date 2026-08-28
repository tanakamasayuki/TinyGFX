"""**本番の生成器（LGFXFontToolJs の CLI）が出した CellFont を描けること。**

つなぎの `tools/gen_font.py` ではなく、実際に配布される形のヘッダをそのまま食わせる。
この 1 本で仕様の難所を 3 つ踏む。

- 可変ピッチ（グリフ表あり、`width` / `xAdvance` / `bytesPerGlyph` は 0）
- 疎索引の頭ブロック（`headCount=2`、`first=0x32`）
- **`first` より小さいコードがしっぽに居る**（0x20 / 0x2E）— 仕様 §7.1 の落とし穴
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent


def test_cli_font_renders(dut):
    dut.expect("TEST start clifont", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # 生成器がヘッダのコメントに書いた値と合っていること（Line box 11px / ascent 10）
    assert r["line"] == 11, f"行送りが {r['line']}"
    assert r["ascent"] == 10, f"ascent が {r['ascent']}"

    # 12 字ぶんの送り幅。全角 12 + 半角 6 相当が並ぶ
    assert r["width"] > 0, "何も進んでいない"
    assert r["lit"] > 200, f"描かれた画素が少なすぎる: {r['lit']}"

    # 収録外は何も描かず送り 0（豆腐が入っていないフォント）
    assert r["missing_adv"] == 0, f"収録外なのに {r['missing_adv']} 進んだ"
    assert r["missing_lit"] == 0, f"収録外なのに {r['missing_lit']} 画素描いた"

    im = tc.image(SKETCH, "clifont")
    assert im.size == (128, 16)
