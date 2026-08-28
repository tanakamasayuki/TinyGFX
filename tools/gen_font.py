#!/usr/bin/env python3
"""テスト用の CellFont ヘッダを生成する。

**実運用のフォントは LGFXFontToolJs の CLI が出す。** これはそれが揃うまでの
つなぎで、tests/ と examples/ が外部ツールなしで走るためだけにある。
出力は仕様（cellfont.ja.md §12.2）の形そのままなので、CLI が揃ったら
ファイルを差し替えるだけで済む。


グリフは下の ART に ASCII アートで書く。5x7 を 6x8 のセルに置き、
列優先（1 バイト = 縦 8 画素、bit0 が上）でパックする。

    python3 tools/gen_font.py            # 生成して書き出す
    python3 tools/gen_font.py --check    # 生成せずレンダリングして目視確認

Phase 0 では 0x20-0x3F（記号と数字）だけ収録している。
英字は Phase 3 でここに足す。
"""
import argparse
import pathlib

CELL_W = 6   # 送り幅（グリフ 5 + 空き 1）
GLYPH_W = 5
CELL_H = 8   # ページ 1 枚
GLYPH_H = 7

FIRST = 0x20

ART = r"""
0x20 space
.....
.....
.....
.....
.....
.....
.....
0x21 !
..#..
..#..
..#..
..#..
..#..
.....
..#..
0x22 "
.#.#.
.#.#.
.....
.....
.....
.....
.....
0x23 #
.#.#.
.#.#.
#####
.#.#.
#####
.#.#.
.#.#.
0x24 $
..#..
.####
#.#..
.###.
..#.#
####.
..#..
0x25 %
##..#
##..#
...#.
..#..
.#...
#..##
#..##
0x26 &
.##..
#..#.
#.#..
.#...
#.#.#
#..#.
.##.#
0x27 '
..#..
..#..
.....
.....
.....
.....
.....
0x28 (
...#.
..#..
.#...
.#...
.#...
..#..
...#.
0x29 )
.#...
..#..
...#.
...#.
...#.
..#..
.#...
0x2A *
.....
#.#.#
.###.
#####
.###.
#.#.#
.....
0x2B +
.....
..#..
..#..
#####
..#..
..#..
.....
0x2C ,
.....
.....
.....
.....
.....
..##.
.##..
0x2D -
.....
.....
.....
#####
.....
.....
.....
0x2E .
.....
.....
.....
.....
.....
.##..
.##..
0x2F /
....#
...#.
...#.
..#..
.#...
.#...
#....
0x30 0
.###.
#...#
#..##
#.#.#
##..#
#...#
.###.
0x31 1
..#..
.##..
..#..
..#..
..#..
..#..
.###.
0x32 2
.###.
#...#
....#
...#.
..#..
.#...
#####
0x33 3
#####
...#.
..#..
...#.
....#
#...#
.###.
0x34 4
...#.
..##.
.#.#.
#..#.
#####
...#.
...#.
0x35 5
#####
#....
####.
....#
....#
#...#
.###.
0x36 6
..##.
.#...
#....
####.
#...#
#...#
.###.
0x37 7
#####
....#
...#.
..#..
.#...
.#...
.#...
0x38 8
.###.
#...#
#...#
.###.
#...#
#...#
.###.
0x39 9
.###.
#...#
#...#
.####
....#
...#.
.##..
0x3A :
.....
.##..
.##..
.....
.##..
.##..
.....
0x3B ;
.....
.##..
.##..
.....
.##..
..#..
.#...
0x3C <
...#.
..#..
.#...
#....
.#...
..#..
...#.
0x3D =
.....
.....
#####
.....
#####
.....
.....
0x3E >
.#...
..#..
...#.
....#
...#.
..#..
.#...
0x3F ?
.###.
#...#
....#
...#.
..#..
.....
..#..
"""


def parse():
    glyphs = []
    lines = [ln for ln in ART.splitlines() if ln.strip() != ""]
    i = 0
    while i < len(lines):
        header = lines[i]
        assert header.startswith("0x"), f"header expected: {header!r}"
        code = int(header.split()[0], 16)
        rows = lines[i + 1:i + 1 + GLYPH_H]
        assert len(rows) == GLYPH_H, f"{header}: {len(rows)} rows"
        for r in rows:
            assert len(r) == GLYPH_W, f"{header}: row width {len(r)} != {GLYPH_W} ({r!r})"
        glyphs.append((code, rows))
        i += 1 + GLYPH_H
    for n, (code, _) in enumerate(glyphs):
        assert code == FIRST + n, f"non-contiguous at {code:#04x} (expected {FIRST + n:#04x})"
    return glyphs


def pack(glyphs):
    """行を連結した MSB first のビット列。グリフ間はバイト境界揃え。

    GFXfont / LGFXFontToolJs (src/format/gfxfont.js) と同じ規約。
    """
    bitmap, offsets = [], []
    for _code, rows in glyphs:
        offsets.append(len(bitmap))
        buf, nbits = 0, 0
        for r in range(GLYPH_H):
            for c in range(GLYPH_W):
                buf = (buf << 1) | (1 if rows[r][c] == "#" else 0)
                nbits += 1
                if nbits == 8:
                    bitmap.append(buf & 0xFF)
                    buf, nbits = 0, 0
        if nbits:  # グリフ末尾をバイト境界まで詰める
            bitmap.append((buf << (8 - nbits)) & 0xFF)
    return bitmap, offsets


def render(code, rows):
    print(f"--- {code:#04x} {chr(code)!r}")
    for r in rows:
        print(r)


def hexrows(values, per_line=12, fmt="0x{:02X}"):
    out = []
    for i in range(0, len(values), per_line):
        out.append("    " + ", ".join(fmt.format(v) for v in values[i:i + per_line]) + ",")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="生成せずレンダリングだけする")
    ap.add_argument("--mode", choices=["fixed", "records"], default="fixed",
                    help="fixed=グリフ表を持たない（固定ピッチ） / records=4 バイト/字のグリフ表")
    ap.add_argument("--sparse", action="store_true", help="連続索引ではなく昇順のコード表を持つ")
    ap.add_argument("--symbol", default="tinygfxFont5x7")
    ap.add_argument("--next", dest="next_font", default=None,
                    help="この字を持たないとき次に探すフォントのシンボル名")
    ap.add_argument("--out", default="tests/common_libs/tgfx_font/src/tinygfx_font5x7.h")
    args = ap.parse_args()

    glyphs = parse()
    if args.check:
        for code, rows in glyphs:
            render(code, rows)
        return

    # 出力順を決める（仕様 §6: 頭ブロック -> しっぽ昇順）
    if args.sparse:
        # 疎索引の経路を実際に踏ませるための並び。**頭ブロックを真ん中に取る**ので、
        # しっぽには first より小さいコードが残る。仕様 §7.1 が
        # 「c < first で打ち切ってよいのは連続索引のときだけ」と警告している道を通す。
        #
        # 正準な生成器なら 0x20..0x3f は連続なので codes を持たない。ここは
        # **デコーダの疎経路を試すための治具**であって、正準出力ではない。
        head = [g for g in glyphs if g[0] >= 0x30]
        tail = sorted((g for g in glyphs if g[0] < 0x30), key=lambda g: g[0])
        ordered = head + tail
        first = head[0][0]
        head_count = len(head)
        codes = [c for c, _ in tail]
    else:
        ordered = glyphs
        first = FIRST
        head_count = 0
        codes = None

    bitmap, offsets = pack(ordered)
    bytes_per_glyph = (GLYPH_W * GLYPH_H + 7) // 8
    variable = args.mode == "records"
    sym = args.symbol
    last = FIRST + len(glyphs) - 1

    # 仕様 §4: ペン位置はベースライン。箱の上端はベースラインより ascent だけ上。
    # このフォントは descent を持たない（箱の下端 = ベースライン）ので ascent = 高さ。
    y_offset = -GLYPH_H

    data = len(bitmap) + (len(offsets) * 4 if variable else 0) + (len(codes) * 2 if codes else 0)

    parts = [
        "// CellFont (generated by tools/gen_font.py - do not edit)",
        "//",
        f"// 収録: {FIRST:#04x}..{last:#04x}（{len(glyphs)} 文字）",
        f"// 索引: {'疎（頭ブロック + コード表）' if args.sparse else '連続'} / "
        f"グリフ表: {'あり（4 バイト/字）' if variable else 'なし（固定ピッチ）'}",
        f"// データ量: {data} バイト（+ sizeof(CellFont)）",
        "//",
        "// テスト用のつなぎ。**実運用のフォントは LGFXFontToolJs の CLI で作る。**",
        "// TinyGFX 本体はフォントデータを同梱しない。スケッチ側に置くこと。",
        "//",
        "// 使い方: **TinyGFX/FontCell.h を先に include してから**、",
        "// setFont() に渡すには TinyGFXFontRef で包む（形式の入口を指すため）:",
        f"//   static const TinyGFXFontRef myFont = {{&{sym}, &tinygfxFontCellOps, nullptr}};",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "// 描画器のヘッダ（CellFont / CellGlyph の型を持つもの）を先に include すること",
        "",
        "#if !defined(CELLFONT_SPEC_VERSION)",
        '#error "Include your renderer CellFont header before this font header"',
        "#elif CELLFONT_SPEC_VERSION != 1",
        '#error "This font header requires CellFont spec version 1"',
        "#endif",
        "",
        f"static const uint8_t {sym}Bitmaps[{len(bitmap)}] CELLFONT_PROGMEM = {{",
        hexrows(bitmap),
        "};",
        "",
    ]

    if variable:
        recs = [f"    {{{off}, {GLYPH_W}, {CELL_W}}}," for off in offsets]
        parts += [
            f"static const CellGlyph {sym}Glyphs[{len(offsets)}] CELLFONT_PROGMEM = {{",
            "\n".join(recs),
            "};",
            "",
        ]

    if codes is not None:
        parts += [
            f"static const uint16_t {sym}Codes[{len(codes)}] CELLFONT_PROGMEM = {{",
            hexrows(codes, per_line=10, fmt="0x{:04X}"),
            "};",
            "",
        ]

    # 仕様 §3: 使わないフィールドには 0 を入れる（決定性のため）
    parts += [
        f"static const CellFont {sym} CELLFONT_PROGMEM = {{",
        f"    {sym}Bitmaps,",
        (f"    {sym}Glyphs," if variable else "    nullptr,  // 固定ピッチ"),
        (f"    {sym}Codes," if codes is not None else "    nullptr,  // 連続索引"),
        (f"    &{args.next_font},  // next" if args.next_font else "    nullptr,  // next"),
        f"    {first:#04x},  // first",
        f"    {len(glyphs)},  // count",
        ("    0,  // width（可変ピッチ）" if variable else f"    {GLYPH_W},  // width"),
        f"    {GLYPH_H},  // height",
        ("    0,  // xAdvance（可変ピッチ）" if variable else f"    {CELL_W},  // xAdvance"),
        f"    {CELL_H},  // yAdvance",
        "    0,  // xOffset",
        f"    {y_offset},  // yOffset（ベースラインから箱の上端。ascent = {GLYPH_H}）",
        ("    0,  // bytesPerGlyph（可変ピッチ）" if variable
         else f"    {bytes_per_glyph},  // bytesPerGlyph"),
        f"    {head_count},  // headCount",
        "};",
        "",
    ]

    out = pathlib.Path(__file__).resolve().parent.parent / args.out
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(parts), encoding="utf-8")
    print(f"wrote {out}")
    print(f"  glyphs={len(glyphs)} bitmap={len(bitmap)}B "
          f"table={len(offsets) * 4 if variable else 0}B "
          f"codes={len(codes) * 2 if codes else 0}B  data={data}B (+sizeof(CellFont))")


if __name__ == "__main__":
    main()
