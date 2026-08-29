"""UTF-8 decoding.

A sketch's strings are UTF-8 because that is how they were saved, not because
anyone chose it (the Arduino IDE saves them that way). So the thing worth
checking, before "does CJK work", is **whether an ordinary string with one
degree sign in it comes out as one character**.

`TinyGFX::nextCode` is a pure function, so this looks at **the code point and
the number of bytes consumed** directly rather than at pixels. Returning the
right code point while consuming the wrong number of bytes shifts everything
after it, so the second number matters as much as the first.

The font is `tgfx_utf8`: one-, two- and three-byte characters in a single font.
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent

NOTDEF = 0xFFFD


def test_utf8(dut):
    dut.expect("TEST start utf8", timeout=20)
    dut.expect("TEST done", timeout=60)
    r = tc.report(SKETCH)

    # --- the decoder itself -------------------------------------------------
    # (code point, bytes consumed)
    expected = {
        "ascii":    (0x41, 1),
        "two":      (0x00B0, 2),      # U+00B0 DEGREE SIGN
        "three":    (0x2103, 3),      # U+2103 DEGREE CELSIUS
        "four":     (NOTDEF, 4),      # U+1F600: past U+FFFF, so notdef - but all 4 bytes go
        "stray":    (NOTDEF, 1),      # a continuation byte with no lead
        "ff":       (NOTDEF, 1),      # a byte UTF-8 never produces
        "cut_end":  (NOTDEF, 1),      # lead, then the terminator. **Must not step past it**
        "cut_mid":  (NOTDEF, 2),      # lead plus one continuation, then it stops
        "overlong": (0x0000, 3),      # overlong forms are decoded, not rejected - as specified
    }
    for name, (cp, used) in expected.items():
        assert r[f"{name}_cp"] == cp, (
            f"{name}: code point {r[f'{name}_cp']:#06x}, expected {cp:#06x}")
        assert r[f"{name}_used"] == used, (
            f"{name}: consumed {r[f'{name}_used']} bytes, expected {used}")

    # After a truncated sequence, reading resumes at the byte that broke it
    assert r["recover_bad"] == NOTDEF
    assert r["recover_next"] == 0x41, "the 'A' after the truncated sequence was lost"
    assert r["recover_used"] == 2, "two bytes of input, and the consumption does not add up"

    # --- strings ------------------------------------------------------------
    # "0°℃" is three characters in six bytes, and must measure as the sum of
    # three advances. This font has no U+FFFD, so read byte by byte it would
    # come to just the 4 of the '0'.
    total = r["w_c0"] + r["w_deg"] + r["w_cel"]
    assert r["w_mixed"] == total, (
        f'textWidth("0°℃") is {r["w_mixed"]}; the sum per character is {total}')
    assert r["w_mixed"] > r["w_c0"], "not one multi-byte character was counted"
    assert r["draw_mixed"] == r["w_mixed"], (
        f"drawString returned {r['draw_mixed']} against textWidth {r['w_mixed']}")

    # An uncovered four-byte character draws nothing but must still eat four
    # bytes. Failing to would shift the '1' that follows.
    assert r["astral_end"] == r["plain_end"], (
        f"the position after a four-byte character is {r['astral_end']}, "
        f"against {r['plain_end']} without it")

    # --- Print --------------------------------------------------------------
    # Print delivers one byte at a time, so it holds state between calls. Same
    # picture either way.
    assert r["print_last_col"] == r["mixed_last_col"], (
        f"print ends at {r['print_last_col']}, drawString at {r['mixed_last_col']}")
    assert tc.image(SKETCH, "print") == tc.image(SKETCH, "mixed"), (
        "print and drawString drew different pictures")

    assert r["print_astral_end"] == r["plain_end"], (
        f"through print, the position after a four-byte character is "
        f"{r['print_astral_end']}, against {r['plain_end']} without it")

    # A newline arriving mid-sequence is still a newline
    assert r["print_cut_nl_x"] > 0, "the '0' after the newline was not drawn"
    assert r["print_cut_nl_y"] == r["line_height"], (
        f"the newline moved y to {r['print_cut_nl_y']}; the line height is {r['line_height']}")
