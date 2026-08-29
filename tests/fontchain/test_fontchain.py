"""CellFont chaining and the U+FFFD fallback.

Walks paths a generated font cannot reach, using tiny hand-built CellFonts.
Spec (LGFXFontToolJs docs/formats/cellfont.ja.md) 7.1, 7.2, 8 and 15.2.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent


def test_fontchain(dut):
    dut.expect("TEST start fontchain", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # 'A' fills all of its 3x5. Advance 4.
    assert (r["a_ret"], r["a_lit"]) == (4, 15), (
        f"'A' drew {r['a_lit']} pixels, advance {r['a_ret']}")
    assert (r["a_top"], r["a_bottom"]) == (0, 4)

    # Uncovered -> falls back to U+FFFD. That tofu is one row of 3 pixels.
    assert r["nd_lit"] == 3, f"the fallback was not drawn ({r['nd_lit']} pixels)"
    assert r["nd_ret"] == 4, "falling back must still return an advance"
    assert (r["nd_top"], r["nd_bottom"]) == (0, 0)

    # Asking for U+FFFD itself must not search again (no infinite descent)
    assert (r["ndself_lit"], r["ndself_ret"]) == (3, 4)

    # **The real point.** The first link (fontAN) has a U+FFFD, and the 'B' in
    # the second link must still be reached. A decoder that falls back on its
    # own turns this into the tofu (3 pixels, row 0) and fails here.
    assert r["chain_lit"] == 9, (
        f"the 'B' in the second link never appeared ({r['chain_lit']} pixels). "
        "Probably swallowed by the first link's U+FFFD")
    # The baseline comes from the **first** font's ascent (5). fontB has ascent
    # 3, so it lands two rows lower.
    assert (r["chain_top"], r["chain_bottom"]) == (2, 4), (
        f"baselines do not line up (rows {r['chain_top']}..{r['chain_bottom']}). "
        "Probably the decoder used its own ascent")

    # Nowhere in the chain, and no fallback either -> nothing drawn, advance 0
    assert (r["miss_lit"], r["miss_ret"]) == (0, 0)

    # Sparse index: a code below `first`, living in the tail, must still be
    # found (7.1). Glyphs are ordered "head block, then tail ascending" (6), so
    # the head 'B' is index 0 (fully inked, 15 px) and the tail 'A' is index 1
    # (one row, 3 px).
    assert (r["lohead_lit"], r["lohead_ret"]) == (15, 4), "the head block was not found"
    assert (r["lo_lit"], r["lo_ret"]) == (3, 4), "a code below `first` was not found"

    # Line height and ascent come from the head of the chain
    assert r["chain_line"] == 6
    assert r["chain_ascent"] == 5, "the chain's ascent is not the head font's"
    assert r["b_ascent"] == 3
