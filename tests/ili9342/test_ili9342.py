"""ILI9342C の MADCTL・色順・ミラーの組み立て。

ILI9342 は GRAM が最初から横長なので、回転 0 が 320x240（=ネイティブ）になる。
表そのものは ST7789 と同じで、差は BGR ビットと setMirror の XOR だけ。

**この表が実機で正しいかはここでは分からない**（docs/MANUAL_TEST.ja.md M0）。
ここが守るのは「実装が表どおりに動くこと」。実機でずれたら表を直し、
ここの期待値も一緒に直す。
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent

MY, MX, MV, BGR = 0x80, 0x40, 0x20, 0x08
RED565 = 0xF800

# 色順を抜いた素の表（ST7789 と同じ）
BASE = {
    0: 0,
    1: MV | MX,
    2: MX | MY,
    3: MV | MY,
}
SIZE = {0: (32, 16), 1: (16, 32), 2: (32, 16), 3: (16, 32)}


def test_ili9342(dut):
    dut.expect("TEST start ili9342", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    problems = []
    for rot, base in BASE.items():
        w, h = SIZE[rot]
        checks = {
            f"bgr{rot}_madctl": base | BGR,
            f"rgb{rot}_madctl": base,
            f"flip{rot}_madctl": (base ^ (MX | MY)) | BGR,
            f"bgr{rot}_w": w,
            f"bgr{rot}_h": h,
            # ILI9342 にオフセットのあるモジュールは無い。素通しであること
            f"bgr{rot}_xs": 0,
            f"bgr{rot}_ys": 0,
        }
        for key, want in checks.items():
            got = r[key]
            if got != want:
                problems.append(f"{key}: {got:#04x} != {want:#04x}"
                                if key.endswith("madctl") else f"{key}: {got} != {want}")
    assert not problems, "ILI9342 の表と違う: " + "; ".join(problems)

    madctls = {r[f"bgr{rot}_madctl"] for rot in range(4)}
    assert len(madctls) == 4, f"MADCTL が重複している: {sorted(madctls)}"

    # 両軸ミラーは 180 度回転と同じ。表の入れ替えとして現れる
    # （片軸だけのミラーは回転では作れない。だから setMirror が要る）
    assert r["flip0_madctl"] == r["bgr2_madctl"], "両軸ミラーが回転 2 と一致しない"
    assert r["flip2_madctl"] == r["bgr0_madctl"], "両軸ミラーが回転 0 と一致しない"

    # 描いたものが GRAM に乗るか（コマンド列が ST7789 と同じで通ること）
    assert r["hit"] == RED565, f"塗った内側が {r['hit']:#06x}"
    assert r["edge"] == RED565, f"塗った右下端が {r['edge']:#06x}"
    assert r["miss"] == 0, f"塗っていない左が {r['miss']:#06x}"
    assert r["past"] == 0, f"塗っていない右下が {r['past']:#06x}"
