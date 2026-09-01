"""**However a picture is encoded, not one pixel may differ.**

The converter picks an encoding per picture: raw RGB565, RLE, RLE with a
palette, 1bpp packed horizontally, 1bpp packed vertically. **Which one it picked
must not be visible from the sketch**, and that is what this test pins down.

`images.h` holds one copy of the picture per encoding, each pinned in
`images/.imagesconfig` so every decoder actually runs. Regenerate with
`tests/regen_images.py`.

The same idea as pinning "three encodings draw the same pixels" for CellFont
(`tests/text/`). Encoding is the tool's business, not the user's.

How formats are chosen, with the measurements, is in
[docs/IMAGE_FORMAT.ja.md](../../docs/IMAGE_FORMAT.ja.md).
"""

from pathlib import Path

import tgfx_check as tc

SKETCH = Path(__file__).parent
W = H = 32


def test_image_fmt(dut):
    dut.expect("TEST start image_fmt", timeout=20)
    dut.expect("TEST done", timeout=60)

    r = tc.report(SKETCH)

    # --- there is a picture (all-black matching all-black proves nothing) ----
    assert 0 < r["raw_lit"] < W * H, f"raw RGB565 is one flat colour ({r['raw_lit']} lit)"
    assert 0 < r["mono_lit"] < W * H, f"the 1bpp picture is one flat colour ({r['mono_lit']} lit)"

    # --- **the point.** A different format, the same picture ----------------
    assert r["rle565_diff"] == 0, f"RLE differs from raw by {r['rle565_diff']} pixels"
    assert r["rlepal4_diff"] == 0, f"RLE+palette differs from raw by {r['rlepal4_diff']} pixels"
    assert r["mono_v_diff"] == 0, (
        f"1bpp packed vertically differs from horizontally by {r['mono_v_diff']} pixels")

    # --- clipping works -----------------------------------------------------
    assert r["clip_outside"] == 0, (
        f"{r['clip_outside']} pixels drawn outside the clip")

    # --- entirely off screen sends nothing ----------------------------------
    assert r["offscreen_pixels"] == 0, (
        f"an image entirely off screen sent {r['offscreen_pixels']} pixels")

    # --- raw565 opens one window a row, no more -----------------------------
    #
    # A photograph has runs of one, so a fillRect per run means **a window per
    # pixel**. That is what it used to do: 1,024 pixels of 32x32 noise cost
    # **3,072 commands**, 13 bytes a pixel. One window a row brought it to 96
    # (docs/IMAGE_FORMAT.ja.md).
    #
    # A window is three commands: CASET, RASET, RAMWR.
    assert r["photo_pixels"] == W * H, (
        f"a 64x64 drawn into 32x32 sent {r['photo_pixels']} pixels; want {W*H}")
    assert r["photo_cmds"] == 3 * H, (
        f"{r['photo_cmds'] / 3:.0f} windows opened; it must equal the {H} rows")

    # The same hanging off the top left. This path clips itself, so what matters
    # is **not writing off screen** and the pixel count staying put.
    assert r["photo_off_pixels"] == W * H, (
        f"hanging off the edge sent {r['photo_off_pixels']} pixels; want {W*H}")
    assert r["photo_off_cmds"] == 3 * H

    # Inside the clip must not differ by a pixel from no clip; outside it, not
    # one pixel may be touched.
    assert r["photo_clip_in_diff"] == 0, (
        f"inside the clip differs by {r['photo_clip_in_diff']} pixels")
    assert r["photo_clip_out_lit"] == 0, (
        f"{r['photo_clip_out_lit']} pixels drawn outside the clip")
    assert r["photo_clip_pixels"] == 16 * 16, (
        f"a 16x16 clip sent {r['photo_clip_pixels']} pixels")
    assert r["photo_clip_cmds"] == 3 * 16

    # Extreme coordinates must send nothing. If `clipX0 - x` overflows and c0
    # goes negative, it **reads in front of the image data** and puts that on
    # the screen.
    assert r["photo_extreme_pixels"] == 0, (
        f"entirely off screen, yet {r['photo_extreme_pixels']} pixels were sent")
    assert r["photo_extreme_cmds"] == 0, (
        f"entirely off screen, yet {r['photo_extreme_cmds'] / 3:.0f} windows were opened")

    # --- dataLen is only the RLE terminator -----------------------------------
    #
    # **How big an image may be depends on which decoders read
    # CellImage::dataLen.** It is uint16_t (65,535 bytes) and a 240x240 raw565
    # is 115,200, so the answer decides whether a full screen of the reference
    # panel can be an image at all (docs/EXTERNAL_REQUESTS.ja.md E18).
    #
    # Same bytes, same picture, dataLen zeroed. raw565 and the 1bpp packings
    # walk by dimensions; only the RLE decoders terminate on it.
    assert r["raw565_nolen_diff"] == 0, (
        f"raw565 changed by {r['raw565_nolen_diff']} pixels when dataLen was "
        "zeroed, so it does read it and the uint16 limit applies to it too")
    assert r["bitmap1h_nolen_diff"] == 0, (
        f"bitmap1h changed by {r['bitmap1h_nolen_diff']} pixels when dataLen "
        "was zeroed")
    assert r["rle565_nolen_diff"] > 0, (
        "rle565 drew the same picture with dataLen zeroed, so the two checks "
        "above prove nothing - dataLen is not being read by anything")

    # --- transparency -------------------------------------------------------
    #
    # The same image drawn with and without transparency. **Only the pixels of
    # the transparent colour leave the background showing; nothing else may
    # differ by a pixel.**
    #
    # Whether transparency is honoured is decided by the ops, not the format
    # (the generated header points at one), so a sketch that only uses opaque
    # images does not link the test for it. It costs 24-66 B depending on the
    # format and the MCU (docs/IMAGE_FORMAT.ja.md).
    assert r["opaque_bg_left"] == 0, (
        f"opaque, yet {r['opaque_bg_left']} pixels of background remain")
    assert r["trans_bg_left"] > 0, "transparent, yet no background shows at all"
    assert r["trans_bg_left"] < W * H, (
        "transparent, and everything is still background (nothing was drawn)")

    # Only the transparent pixels differ. Everywhere else is the same picture.
    assert r["trans_differ"] == r["trans_bg_left"], (
        f"something other than the transparent colour differs: {r['trans_differ']} "
        f"changed against {r['trans_bg_left']} left as background")


def test_committed_images_are_current():
    """**The comparison is only as good as the files it compares.**

    `images.h` is committed so the suite runs without the converter installed -
    and so a change in what the converter emits would otherwise go unnoticed,
    the test still passing against yesterday's answer.

    **Nothing is installed to run it**: `regen_images.py` fetches the pinned
    release through npx, so the version cannot drift and a machine with no npx
    or no network simply skips.
    """
    import subprocess
    import sys
    from pathlib import Path
    tests = Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(tests))
    import regen_images
    r = subprocess.run([sys.executable, str(tests / "regen_images.py"),
                        "image_fmt", "--check"], capture_output=True, text=True)
    if r.returncode == regen_images.UNAVAILABLE:
        pytest.skip(r.stderr.strip().splitlines()[0])
    assert r.returncode == 0, r.stdout + r.stderr
