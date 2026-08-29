#!/usr/bin/env python3
"""Convert an image into a .h for TinyGFX - **a minimal, experimental one.**

The real tool is going to live on the LGFXFontToolJs side (browser and CLI).
This is the scaffold that comes first, so that "how much does each format
actually save" can be measured. Its job is to produce the evidence behind
docs/IMAGE_FORMAT.ja.md.

Same idea as choosing a PNG filter: **brute-force the encodings and take the
smallest.** The user does not pick a format (--format can force one).

    uv run python tools/img2h.py icon.png --name myIcon --out icon.h
    uv run python tools/img2h.py icon.png --report      # just the sizes, per format

The formats line up with the table in docs/IMAGE_FORMAT.ja.md:

    raw565      raw RGB565
    pal4        4bpp palette (16 colours or fewer)
    pal8        8bpp palette (256 colours or fewer)
    rle565      RLE: 1 byte of length, 2 of colour
    rlepal4     RLE with a 4-bit palette: 4 bits of length (1..16), 4 of index
    bitmap1h    1bpp packed horizontally (the order drawBitmap wants)
    bitmap1v    1bpp packed vertically (an SSD1306 / SH1106 page, exactly)
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("PIL is required: uv run --with pillow python tools/img2h.py ...")


def to565(rgb):
    r, g, b = rgb[:3]
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def load(path, mono=None):
    """The image as a 2-D array of RGB565.

    Passing `mono` thresholds it at that luminance (0..255). **Required for a
    monochrome panel**: a PNG source usually has antialiased edges on its text,
    which can push it past 50 colours and make 1bpp impossible to encode.
    """
    im = Image.open(path).convert("RGB")
    w, h = im.size
    if mono is not None:
        g = im.convert("L").load()
        return w, h, [[0xFFFF if g[x, y] >= mono else 0x0000
                       for x in range(w)] for y in range(h)]
    px = im.load()
    return w, h, [[to565(px[x, y]) for x in range(w)] for y in range(h)]


# --- encoders --------------------------------------------------------------
# Each returns (bytes, palette or None), or None when it cannot be done.

def enc_raw565(w, h, rows, _pal):
    out = bytearray()
    for row in rows:
        for c in row:
            out += bytes((c >> 8, c & 0xFF))   # big endian: the order it goes on the wire
    return bytes(out), None


def enc_pal(w, h, rows, pal, bits):
    if pal is None or len(pal) > (1 << bits):
        return None
    idx = {c: i for i, c in enumerate(pal)}
    out = bytearray()
    for row in rows:
        if bits == 8:
            out += bytes(idx[c] for c in row)
        else:  # 4-bit. **Each row starts on a byte boundary**, which keeps rows easy to cut
            b, half = 0, False
            for c in row:
                b = (b << 4) | idx[c]
                if half:
                    out.append(b & 0xFF); b, half = 0, False
                else:
                    half = True
            if half:
                out.append((b << 4) & 0xFF)
    return bytes(out), pal


def runs_of(w, h, rows):
    """The runs, continued across row boundaries, as (length, colour)."""
    flat = [c for row in rows for c in row]
    out, cur, n = [], flat[0], 1
    for c in flat[1:]:
        if c == cur:
            n += 1
        else:
            out.append((n, cur)); cur, n = c, 1
    out.append((n, cur))
    return out


def enc_rle565(w, h, rows, _pal):
    out = bytearray()
    for n, c in runs_of(w, h, rows):
        while n > 0:
            take = min(n, 255)
            out += bytes((take, c >> 8, c & 0xFF))
            n -= take
    return bytes(out), None


def enc_rlepal4(w, h, rows, pal):
    if pal is None or len(pal) > 16:
        return None
    idx = {c: i for i, c in enumerate(pal)}
    out = bytearray()
    for n, c in runs_of(w, h, rows):
        i = idx[c]
        while n > 0:
            take = min(n, 16)
            out.append(((take - 1) << 4) | i)   # lengths 1..16 are stored as 0..15
            n -= take
    return bytes(out), pal


def enc_bitmap1h(w, h, rows, pal):
    if pal is None or len(pal) != 2:
        return None
    on = pal[1]
    out = bytearray()
    for row in rows:                            # every row starts on a byte boundary
        b, k = 0, 0
        for c in row:
            b = (b << 1) | (1 if c == on else 0); k += 1
            if k == 8:
                out.append(b); b, k = 0, 0
        if k:
            out.append((b << (8 - k)) & 0xFF)
    return bytes(out), pal


def enc_bitmap1v(w, h, rows, pal):
    """Packed vertically: one byte is 8 pixels tall, LSB at the top. Exactly an
    SSD1306 page."""
    if pal is None or len(pal) != 2:
        return None
    on = pal[1]
    pages = (h + 7) // 8
    out = bytearray(w * pages)
    for y in range(h):
        for x in range(w):
            if rows[y][x] == on:
                out[(y >> 3) * w + x] |= 1 << (y & 7)
    return bytes(out), pal


# pal4 / pal8 (uncompressed palettes) are **not implemented.** They lost to
# rlepal4 on every image measured (2,054 against 416 on a 64x64 UI icon). If
# they are ever needed, put enc_pal and DECODER_BYTES back and measure again.
ENCODERS = [
    ("raw565",   enc_raw565),
    ("rle565",   enc_rle565),
    ("rlepal4",  enc_rlepal4),
    ("bitmap1h", enc_bitmap1h),
    ("bitmap1v", enc_bitmap1v),
]


# What a decoder actually costs. **Measured against the real implementation in
# src/TinyGFX/Image.h** (2026-08-29, -Os).
#
# **The ranking changes with the architecture.** AVR is 8-bit, so 16-bit
# arithmetic is expensive there; on an ESP32 it is the bit-extraction loop that
# costs. That is why --mcu exists.
#
#                ch32v003   avr  esp32
#   raw565            400   638    492   <- cheapest on AVR
#   rle565            387   663    467   <- cheapest on ch32 / esp32
#   rlepal4           395   679    483
#   bitmap1h          400   654    592
#   bitmap1v          392   690    592
#
# Only "data plus decoder" minimises the total (the same point as CellFont
# spec 13.4). **And a decoder is paid for once per format, not once per image**,
# so minimising each image on its own scatters the formats and costs more.
# --batch brute-forces the set of formats to use.
DECODER_BYTES = {
    "ch32v003": {"raw565": 400, "rle565": 387, "rlepal4": 395,
                 "bitmap1h": 400, "bitmap1v": 392},
    "avr":      {"raw565": 638, "rle565": 663, "rlepal4": 679,
                 "bitmap1h": 654, "bitmap1v": 690},
    "esp32":    {"raw565": 492, "rle565": 467, "rlepal4": 483,
                 "bitmap1h": 592, "bitmap1v": 592},
}

# **Formats that share a decoder.** 1bpp packed horizontally and vertically are
# the same loop, differing by one line in how the bit is taken, so they are one
# decoder. The direction is a constant at the call site (the generated header
# knows the format), so using only one costs only one. **"Supporting both is
# wasteful" never happens here.**
SHARED = {
    "ch32v003": {("bitmap1h", "bitmap1v"): 504},
    "avr":      {("bitmap1h", "bitmap1v"): 864},
    "esp32":    {("bitmap1h", "bitmap1v"): 696},
}

# A page-aligned vertical bitmap is a paged panel's buffer, exactly, so
# TinyGFXPanelPaged::pushVBitmap() can blit it. **An extra fast path, not a
# replacement for the general one.**
#
# **244 B, measured on a CH32V003.** The estimate was "about 24 B, it is just a
# memcpy"; the real implementation came out ten times that, because the page
# alignment, rotation, band and off-panel checks and the dirty tracking all
# remain. The general path is 408 B, so **it only saves 164 B** and can lose to
# rlepal4 on size. **The value of the fast path is speed, not size** - a byte
# copy instead of expanding runs.
ALIGNED_VBLIT = {"ch32v003": 244, "avr": 244, "esp32": 244}


def group_cost(formats, mcu):
    """What a set of formats costs in decoders, counting a shared pair once."""
    rest, total = set(formats), 0
    for combo, cost in SHARED[mcu].items():
        hit = rest & set(combo)
        if len(hit) > 1:
            total += cost
            rest -= hit
    return total + sum(DECODER_BYTES[mcu][f] for f in rest)


def encode_all(w, h, rows):
    uniq = sorted({c for row in rows for c in row})
    pal = uniq if len(uniq) <= 256 else None
    out = {}
    for name, fn in ENCODERS:
        got = fn(w, h, rows, pal)
        if got is not None:
            data, p = got
            total = len(data) + (len(p) * 2 if p else 0)
            out[name] = (data, p, total)
    return out, uniq


# --- output ----------------------------------------------------------------

def carray(name, data, per_line=16, typ="uint8_t"):
    body = ",\n  ".join(
        ", ".join(f"0x{b:02X}" for b in data[i:i + per_line])
        for i in range(0, len(data), per_line))
    return f"static const {typ} {name}[{len(data)}] TINYGFX_IMAGE_PROGMEM = {{\n  {body}\n}};\n"


def emit(name, w, h, fmt, data, pal, uniq, argv, transparent=None):
    ops = {"raw565": "Raw565", "rle565": "Rle565", "rlepal4": "Rlepal4",
           "bitmap1h": "Bitmap1h", "bitmap1v": "Bitmap1v"}[fmt]
    # Transparency is carried by a field in the struct rather than by the ops
    # (it is a different kind of thing from the format). The reasoning, with
    # the measurements, is on CellImage::transparent in src/TinyGFX/Image.h.
    L = []
    L.append(f"// {name} — a TinyGFX image\n//")
    L.append("// Generated by tools/img2h.py (**experimental**; the real tool will replace it)")
    L.append(f"// Format     : {fmt}")
    L.append(f"// Size       : {w}x{h}")
    L.append(f"// Colours    : {len(uniq)}")
    if transparent is not None:
        L.append(f"// Transparent: 0x{transparent:04X}"
                 + (" (palette index)" if pal else " (colour)"))
    L.append(f"// Data       : {len(data)} bytes" + (f" + {len(pal)*2} of palette" if pal else ""))
    L.append(f"//\n// Rebuild with:\n//   python3 tools/img2h.py {' '.join(argv)}\n")
    L.append("#pragma once")
    L.append("#include <stdint.h>\n")
    L.append("#if !defined(TINYGFX_IMAGE_SPEC_VERSION)")
    L.append('#error "Include the renderer\'s TinyGFX/Image.h before this file"')
    L.append("#endif\n")
    L.append(carray(f"{name}Data", data))
    if pal:
        pb = ",\n  ".join(", ".join(f"0x{c:04X}" for c in pal[i:i + 8])
                          for i in range(0, len(pal), 8))
        L.append(f"\nstatic const uint16_t {name}Palette[{len(pal)}] "
                 f"TINYGFX_IMAGE_PROGMEM = {{\n  {pb}\n}};\n")
    tr = transparent if transparent is not None else 0
    L.append(f"\nstatic const CellImage {name} TINYGFX_IMAGE_PROGMEM = {{")
    L.append(f"  {name}Data,")
    L.append(f"  {name}Palette," if pal else "  NULL,")
    L.append(f"  {w}, {h},")
    L.append(f"  {len(data)},")
    L.append(f"  0x{tr:04X},  // transparent")
    L.append(f"  {len(pal) if pal else 0},")
    L.append(f"  {1 if transparent is not None else 0},")
    L.append("};")
    # **Only the decoder for the format in use gets linked.** Pointing at the
    # ops is enough; no macro has to enable it in advance (measured: an unused
    # one is +0 B).
    L.append(f"\nstatic const TinyGFXImageRef {name}Ref = "
             f"{{&{name}, &tinygfxImage{ops}Ops}};")
    return "\n".join(L) + "\n"


def pick_set(per_image, dec):
    """**Choose across the whole set, not one image at a time.**

    A decoder is paid for once per format, not once per image. Five images each
    picking their own format means carrying five decoders, and settling on one
    format usually wins on the total even when the data grows a little.

    There are few formats, so this **brute-forces the set of formats to use**
    and takes whichever total is smallest. Returns (format per image, total,
    the set of formats used).
    """
    names = [n for n, _ in ENCODERS]
    best = None
    for mask in range(1, 1 << len(names)):
        allowed = [names[i] for i in range(len(names)) if mask & (1 << i)]
        total, choice, ok = 0, {}, True
        for key, enc in per_image.items():
            cand = [f for f in allowed if f in enc]
            if not cand:
                ok = False
                break
            f = min(cand, key=lambda k: enc[k][2])
            choice[key] = f
            total += enc[f][2]
        if not ok:
            continue
        used = sorted({choice[k] for k in choice})
        total += dec(used)
        if best is None or total < best[1]:
            best = (choice, total, used)
    return best


def batch(a):
    """Process a whole folder at once."""
    def dec(fs):
        if a.aligned and set(fs) == {"bitmap1v"}:
            return ALIGNED_VBLIT[a.mcu]     # aligned and full width means a byte copy
        return group_cost(fs, a.mcu)

    files = sorted(p for p in Path(a.batch).iterdir()
                   if p.suffix.lower() in (".png", ".bmp", ".gif", ".jpg", ".jpeg"))
    if not files:
        sys.exit(f"no images in {a.batch}")
    per_image, meta = {}, {}
    for f in files:
        w, h, rows = load(f, a.mono)
        enc, uniq = encode_all(w, h, rows)
        per_image[f.name] = enc
        meta[f.name] = (f, w, h, uniq)

    naive_fmt = {k: min(v, key=lambda x: v[x][2]) for k, v in per_image.items()}
    naive_data = sum(per_image[k][naive_fmt[k]][2] for k in per_image)
    naive_dec = dec({naive_fmt[k] for k in naive_fmt})
    choice, total, used = pick_set(per_image, dec)
    set_data = total - dec(used)

    print(f"MCU: {a.mcu}   {len(files)} image(s)"
          + (f"   (thresholded at --mono {a.mono})" if a.mono is not None else "")
          + ("   （--aligned）" if a.aligned else ""))
    print()
    print(f"  {'image':<20} {'alone':<10} {'together':<10} {'data':>7}")
    for k in per_image:
        n, s = naive_fmt[k], choice[k]
        mark = "" if n == s else "  <- changed"
        print(f"  {k:<20} {n:<10} {s:<10} {per_image[k][s][2]:>7}{mark}")
    print()
    print(f"  smallest alone:    data {naive_data} + decoders {naive_dec}"
          f" ({len({naive_fmt[k] for k in naive_fmt})} formats) = {naive_data + naive_dec}")
    print(f"  smallest together: data {set_data} + decoders {total - set_data}"
          f" ({len(used)} formats: {', '.join(used)}) = {total}")
    d = (naive_data + naive_dec) - total
    print(f"  -> together is {d:+} B" if d else "  -> the same")

    if a.out:
        outdir = Path(a.out); outdir.mkdir(parents=True, exist_ok=True)
        for k in per_image:
            f, w, h, uniq = meta[k]
            fmt = choice[k]
            data, pal, _ = per_image[k][fmt]
            name = a.prefix + "".join(c if c.isalnum() else "_" for c in f.stem)
            (outdir / (f.stem + ".h")).write_text(
                emit(name, w, h, fmt, data, pal, uniq, [str(f)]))
        print(f"  -> wrote {len(per_image)} headers into {outdir}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", nargs="?")
    ap.add_argument("--batch", metavar="FOLDER",
                    help="process a folder at once, minimising **across formats**")
    ap.add_argument("--mcu", choices=["ch32v003", "avr", "esp32"], default="ch32v003",
                    help="decoder costs vary by architecture - **the ranking itself changes**")
    ap.add_argument("--aligned", action="store_true",
                    help="assume vertical bitmaps are only ever blitted full width "
                         "on a page boundary; on a paged panel that is a memcpy, "
                         "which brings the decoder down to 24 B")
    ap.add_argument("--prefix", default="", help="symbol prefix for batch output")
    ap.add_argument("--transparent", metavar="RRGGBB",
                    help="make this colour transparent (six hex digits). **Leave it "
                         "off if none of your images need transparency** - the test "
                         "then costs nothing")
    ap.add_argument("--name", default="image")
    ap.add_argument("--out")
    ap.add_argument("--format", choices=[n for n, _ in ENCODERS],
                    help="force this format instead of brute-forcing")
    ap.add_argument("--report", action="store_true", help="just print the size of every format")
    ap.add_argument("--mono", nargs="?", type=int, const=128, metavar="THRESHOLD",
                    help="threshold by luminance (default 128), for a monochrome panel")
    ap.add_argument("--prefer", choices=["h", "v"], default="v",
                    help="which 1bpp packing wins a tie. **Vertical (v) by default**, "
                         "because it blits straight onto an SSD1306 / SH1106. Use h if "
                         "the panel is not page addressed")
    a = ap.parse_args()
    if a.batch:
        batch(a)
        return
    if not a.image:
        ap.error("an image or --batch is required")

    w, h, rows = load(a.image, a.mono)
    enc, uniq = encode_all(w, h, rows)

    if a.report:
        note = f" (thresholded at --mono {a.mono})" if a.mono is not None else ""
        print(f"{Path(a.image).name}  {w}x{h}  {len(uniq)} colours{note}")
        print(f"  {'format':<10} {'data':>8} {'decoder':>8} {'total':>8}")
        dec = DECODER_BYTES[a.mcu]
        best_d = min(v[2] for v in enc.values())
        best_t = min(v[2] + dec[k] for k, v in enc.items())
        for name, _ in ENCODERS:
            if name in enc:
                _, _p, total = enc[name]
                marks = ("<- smallest data" if total == best_d else "") + \
                        (" <- smallest total" if total + dec[name] == best_t else "")
                print(f"  {name:<10} {total:>8} {dec[name]:>8} "
                      f"{total + dec[name]:>8}  {marks}")
            else:
                print(f"  {name:<10} {'—':>8}")
        print("  note: decoder sizes are measured on a CH32V003. **Choose by total "
              "for a single image, by data when several images share a format**")
        return

    # Keep ties resolved consistently. 1bpp horizontal and vertical always come
    # out the same size, so **the data cannot decide - the target panel does.**
    order = {n: i for i, (n, _) in enumerate(ENCODERS)}
    if a.prefer == "v":
        order["bitmap1v"] = -1
    else:
        order["bitmap1h"] = -1
    fmt = a.format or min(enc, key=lambda k: (enc[k][2], order[k]))
    if fmt not in enc:
        sys.exit(f"{fmt} cannot encode this ({len(uniq)} colours)")
    data, pal, total = enc[fmt]
    tr = None
    if a.transparent:
        c565 = to565(tuple(int(a.transparent[i:i+2], 16) for i in (0, 2, 4)))
        if pal:
            if c565 not in pal:
                sys.exit(f"the transparent colour {a.transparent} does not appear in the image")
            tr = pal.index(c565)
        else:
            tr = c565
    text = emit(a.name, w, h, fmt, data, pal, uniq, sys.argv[1:], tr)
    if a.out:
        Path(a.out).write_text(text)
        print(f"{a.out}  {fmt}  {total} B  ({w}x{h}, {len(uniq)} colours)")
    else:
        print(text)


if __name__ == "__main__":
    main()
