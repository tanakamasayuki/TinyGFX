#!/usr/bin/env python3
"""画像を TinyGFX 用の .h に変換する —— **実験用の最小実装。**

正式なツールは LGFXFontToolJs 側（ブラウザ + CLI）に作る予定で、これはその
前に「どの形式がどれだけ効くか」を実測するための踏み台。docs/IMAGE_FORMAT.ja.md
の判断材料を作るのが仕事。

PNG のフィルタ選択と同じ考え方で、**符号化を総当たりして一番小さいものを
選ぶ。** 利用者は形式を指定しない（--format で強制はできる）。

    uv run python tools/img2h.py icon.png --name myIcon --out icon.h
    uv run python tools/img2h.py icon.png --report      # 全形式の大きさだけ出す

対応する形式は docs/IMAGE_FORMAT.ja.md の表と対応:

    raw565      生 RGB565
    pal4        4bpp パレット（色数 16 以下）
    pal8        8bpp パレット（色数 256 以下）
    rle565      RLE。長さ 1B + 色 2B
    rlepal4     RLE + 4bit パレット。長さ 4bit(1..16) + 索引 4bit
    bitmap1h    1bpp 横詰め（drawBitmap と同じ並び）
    bitmap1v    1bpp 縦詰め（SSD1306 / SH1106 のページ形式そのもの）
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("PIL が要る: uv run --with pillow python tools/img2h.py ...")


def to565(rgb):
    r, g, b = rgb[:3]
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def load(path, mono=None):
    """画像を RGB565 の 2 次元配列にする。

    `mono` を渡すと、その閾値（0..255 の輝度）で 2 値化する。**モノクロ
    パネル向けには必須。** 元素材が PNG だと文字の縁がアンチエイリアスされて
    いて、そのままでは 50 色を超えることがあり、1bpp に符号化できない。
    """
    im = Image.open(path).convert("RGB")
    w, h = im.size
    if mono is not None:
        g = im.convert("L").load()
        return w, h, [[0xFFFF if g[x, y] >= mono else 0x0000
                       for x in range(w)] for y in range(h)]
    px = im.load()
    return w, h, [[to565(px[x, y]) for x in range(w)] for y in range(h)]


# --- 符号化 ---------------------------------------------------------------
# どれも (バイト列, パレット or None) を返す。無理なら None。

def enc_raw565(w, h, rows, _pal):
    out = bytearray()
    for row in rows:
        for c in row:
            out += bytes((c >> 8, c & 0xFF))   # ビッグエンディアン（線に出る順）
    return bytes(out), None


def enc_pal(w, h, rows, pal, bits):
    if pal is None or len(pal) > (1 << bits):
        return None
    idx = {c: i for i, c in enumerate(pal)}
    out = bytearray()
    for row in rows:
        if bits == 8:
            out += bytes(idx[c] for c in row)
        else:  # 4bit。**行ごとにバイト境界へ揃える**（行の切り出しが楽になる）
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
    """行をまたいで連続させたラン列。(長さ, 色) を返す。"""
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
            out.append(((take - 1) << 4) | i)   # 長さは 1..16 を 0..15 で持つ
            n -= take
    return bytes(out), pal


def enc_bitmap1h(w, h, rows, pal):
    if pal is None or len(pal) != 2:
        return None
    on = pal[1]
    out = bytearray()
    for row in rows:                            # 各行がバイト境界から始まる
        b, k = 0, 0
        for c in row:
            b = (b << 1) | (1 if c == on else 0); k += 1
            if k == 8:
                out.append(b); b, k = 0, 0
        if k:
            out.append((b << (8 - k)) & 0xFF)
    return bytes(out), pal


def enc_bitmap1v(w, h, rows, pal):
    """縦詰め。1 バイト = 縦 8 画素、LSB が上。SSD1306 のページ形式そのもの。"""
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


# pal4 / pal8（非圧縮パレット）は**実装していない。** 測った全画像で
# rlepal4 に負けたため（64x64 の UI アイコンで 2,054 対 416）。必要になったら
# enc_pal と DECODER_BYTES を戻して測り直す。
ENCODERS = [
    ("raw565",   enc_raw565),
    ("rle565",   enc_rle565),
    ("rlepal4",  enc_rlepal4),
    ("bitmap1h", enc_bitmap1h),
    ("bitmap1v", enc_bitmap1v),
]


# デコーダの実費。**src/TinyGFX/Image.h の本実装で実測**（2026-08-29、-Os）。
#
# **アーキテクチャで順位が入れ替わる。** AVR は 8 ビットなので 16 ビットの
# 演算が高く、ESP32 は逆にビット取り出しのループが高い。だから --mcu が要る。
#
#                ch32v003   avr  esp32
#   raw565            400   638    492   ← AVR ではこれが最安
#   rle565            387   663    467   ← ch32 / esp32 ではこれが最安
#   rlepal4           395   679    483
#   bitmap1h          400   654    592
#   bitmap1v          392   690    592
#
# 「データ + デコーダ」で比べないと総量が最小にならない（CellFont 仕様
# §13.4 と同じ話）。**しかもデコーダ代は画像ごとではなく形式ごとに 1 回**
# なので、画像を 1 枚ずつ最小化すると形式が散らばって損をする。
# --batch では「使う形式の組み合わせ」を総当たりする。
DECODER_BYTES = {
    "ch32v003": {"raw565": 400, "rle565": 387, "rlepal4": 395,
                 "bitmap1h": 400, "bitmap1v": 392},
    "avr":      {"raw565": 638, "rle565": 663, "rlepal4": 679,
                 "bitmap1h": 654, "bitmap1v": 690},
    "esp32":    {"raw565": 492, "rle565": 467, "rlepal4": 483,
                 "bitmap1h": 592, "bitmap1v": 592},
}

# **同じデコーダを共有する形式の組。** 1bpp の横詰めと縦詰めは、ビットの
# 取り出し方が 1 行違うだけの同じループなので 1 本にまとめられる。向きは
# 呼び出し側で定数（生成ヘッダが形式を知っている）なので、片方しか使わな
# ければ片方ぶんの代金しか出ない。**「両対応にすると無駄」は起きない。**
SHARED = {
    "ch32v003": {("bitmap1h", "bitmap1v"): 504},
    "avr":      {("bitmap1h", "bitmap1v"): 864},
    "esp32":    {("bitmap1h", "bitmap1v"): 696},
}

# ページ境界に揃った縦詰めは、ページ方式パネルのバッファそのものなので
# TinyGFXPanelPaged::pushVBitmap() がバイト複写で貼れる。**汎用経路の
# 代わりではなく追加の速い経路。**
#
# **244 B（CH32V003 実測）。** 「memcpy だけだから 24 B 程度」と見積もって
# いたが本実装では 10 倍だった —— ページ境界・回転・帯・パネル外の判定と
# dirty の追跡が残るため。汎用経路が 408 B なので**縮むのは 164 B だけ**で、
# サイズでは rlepal4 に負けることがある。**速い経路の価値はサイズではなく
# 速度**（ラン展開ではなくバイト複写）。
ALIGNED_VBLIT = {"ch32v003": 244, "avr": 244, "esp32": 244}


def group_cost(formats, mcu):
    """使う形式の集合に対するデコーダ総額。共有する組は 1 本ぶんにまとめる。"""
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


# --- 出力 -----------------------------------------------------------------

def carray(name, data, per_line=16, typ="uint8_t"):
    body = ",\n  ".join(
        ", ".join(f"0x{b:02X}" for b in data[i:i + per_line])
        for i in range(0, len(data), per_line))
    return f"static const {typ} {name}[{len(data)}] TINYGFX_IMAGE_PROGMEM = {{\n  {body}\n}};\n"


def emit(name, w, h, fmt, data, pal, uniq, argv, transparent=None):
    ops = {"raw565": "Raw565", "rle565": "Rle565", "rlepal4": "Rlepal4",
           "bitmap1h": "Bitmap1h", "bitmap1v": "Bitmap1v"}[fmt]
    # 透過は ops ではなく構造体の値で表す（形式とは扱いが違う）。理由は
    # src/TinyGFX/Image.h の CellImage::transparent のコメントにある実測。
    L = []
    L.append(f"// {name} — TinyGFX 画像\n//")
    L.append("// tools/img2h.py が生成（**実験用**。正式なツールに置き換わる）")
    L.append(f"// 形式   : {fmt}")
    L.append(f"// 寸法   : {w}x{h}")
    L.append(f"// 色数   : {len(uniq)}")
    if transparent is not None:
        L.append(f"// 透過   : 0x{transparent:04X}"
                 + ("（パレット索引）" if pal else "（色）"))
    L.append(f"// データ : {len(data)} バイト" + (f" + パレット {len(pal)*2}" if pal else ""))
    L.append(f"//\n// 再生成:\n//   python3 tools/img2h.py {' '.join(argv)}\n")
    L.append("#pragma once")
    L.append("#include <stdint.h>\n")
    L.append("#if !defined(TINYGFX_IMAGE_SPEC_VERSION)")
    L.append('#error "描画器の TinyGFX/Image.h を先に include すること"')
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
    # **使う形式のデコーダだけがリンクされる。** ops を指すだけでよく、
    # マクロでの事前有効化は要らない（実測で確認済み: 未使用は +0 B）。
    L.append(f"\nstatic const TinyGFXImageRef {name}Ref = "
             f"{{&{name}, &tinygfxImage{ops}Ops}};")
    return "\n".join(L) + "\n"


def pick_set(per_image, dec):
    """**画像 1 枚ずつではなく、まとめて選ぶ。**

    デコーダ代は「形式ごとに 1 回」であって「画像ごとに 1 回」ではない。
    5 枚がそれぞれ別の形式を選ぶとデコーダを 5 本積むことになり、多少
    データが増えても 1 つの形式に揃えたほうが総量で勝つことが多い。

    形式は少ないので、**使う形式の組み合わせを総当たり**して総量が最小の
    ものを採る。返すのは (画像ごとの形式, 総量, 使う形式の集合)。
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
    """フォルダを一括で処理する。"""
    def dec(fs):
        if a.aligned and set(fs) == {"bitmap1v"}:
            return ALIGNED_VBLIT[a.mcu]     # 揃った全幅ならバイト複写で済む
        return group_cost(fs, a.mcu)

    files = sorted(p for p in Path(a.batch).iterdir()
                   if p.suffix.lower() in (".png", ".bmp", ".gif", ".jpg", ".jpeg"))
    if not files:
        sys.exit(f"画像が無い: {a.batch}")
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

    print(f"MCU: {a.mcu}   画像 {len(files)} 枚"
          + (f"   （--mono {a.mono} で 2 値化）" if a.mono is not None else "")
          + ("   （--aligned）" if a.aligned else ""))
    print()
    print(f"  {'画像':<20} {'1枚ずつ':<10} {'まとめて':<10} {'データ':>7}")
    for k in per_image:
        n, s = naive_fmt[k], choice[k]
        mark = "" if n == s else "  ←変わった"
        print(f"  {k:<20} {n:<10} {s:<10} {per_image[k][s][2]:>7}{mark}")
    print()
    print(f"  1 枚ずつ最小: データ {naive_data} + デコーダ {naive_dec}"
          f"（{len({naive_fmt[k] for k in naive_fmt})} 形式） = {naive_data + naive_dec}")
    print(f"  まとめて最小: データ {set_data} + デコーダ {total - set_data}"
          f"（{len(used)} 形式: {', '.join(used)}） = {total}")
    d = (naive_data + naive_dec) - total
    print(f"  → まとめたほうが {d:+} B" if d else "  → 同じ")

    if a.out:
        outdir = Path(a.out); outdir.mkdir(parents=True, exist_ok=True)
        for k in per_image:
            f, w, h, uniq = meta[k]
            fmt = choice[k]
            data, pal, _ = per_image[k][fmt]
            name = a.prefix + "".join(c if c.isalnum() else "_" for c in f.stem)
            (outdir / (f.stem + ".h")).write_text(
                emit(name, w, h, fmt, data, pal, uniq, [str(f)]))
        print(f"  → {outdir} に {len(per_image)} 本出力")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", nargs="?")
    ap.add_argument("--batch", metavar="フォルダ",
                    help="フォルダを一括処理し、**形式をまとめて**最小化する")
    ap.add_argument("--mcu", choices=["ch32v003", "avr", "esp32"], default="ch32v003",
                    help="デコーダ代はアーキテクチャで変わる。**順位まで入れ替わる**")
    ap.add_argument("--aligned", action="store_true",
                    help="縦詰めをページ境界の全幅にだけ貼る前提にする。"
                         "ページ方式パネルなら memcpy で済むのでデコーダが 24 B になる")
    ap.add_argument("--prefix", default="", help="一括出力のシンボル接頭辞")
    ap.add_argument("--transparent", metavar="RRGGBB",
                    help="この色を透過にする（16 進 6 桁）。**透過の無い画像しか"
                         "使わないなら指定しないこと** — 判定のコードが要らなくなる")
    ap.add_argument("--name", default="image")
    ap.add_argument("--out")
    ap.add_argument("--format", choices=[n for n, _ in ENCODERS],
                    help="総当たりせず指定の形式にする")
    ap.add_argument("--report", action="store_true", help="全形式の大きさだけ出す")
    ap.add_argument("--mono", nargs="?", type=int, const=128, metavar="閾値",
                    help="輝度で 2 値化する（既定 128）。モノクロパネル向け")
    ap.add_argument("--prefer", choices=["h", "v"], default="v",
                    help="1bpp が同点のときどちらを採るか。**縦詰め(v)が既定** — "
                         "SSD1306 / SH1106 に直接貼れるため。ページ方式でないなら h")
    a = ap.parse_args()
    if a.batch:
        batch(a)
        return
    if not a.image:
        ap.error("画像か --batch のどちらかが要る")

    w, h, rows = load(a.image, a.mono)
    enc, uniq = encode_all(w, h, rows)

    if a.report:
        note = f"（--mono {a.mono} で 2 値化）" if a.mono is not None else ""
        print(f"{Path(a.image).name}  {w}x{h}  色数 {len(uniq)}{note}")
        print(f"  {'形式':<10} {'データ':>8} {'デコーダ':>8} {'総量':>8}")
        dec = DECODER_BYTES[a.mcu]
        best_d = min(v[2] for v in enc.values())
        best_t = min(v[2] + dec[k] for k, v in enc.items())
        for name, _ in ENCODERS:
            if name in enc:
                _, _p, total = enc[name]
                marks = ("←データ最小" if total == best_d else "") + \
                        (" ←総量最小" if total + dec[name] == best_t else "")
                print(f"  {name:<10} {total:>8} {dec[name]:>8} "
                      f"{total + dec[name]:>8}  {marks}")
            else:
                print(f"  {name:<10} {'—':>8}")
        print("  ※ デコーダは CH32V003 実測。**1 枚しか使わないなら総量、"
              "同じ形式を何枚も使うならデータ量**で選ぶ")
        return

    # 同点のときの順序を安定させる。1bpp の横／縦は必ず同じ大きさになるので、
    # **データ量では決まらない。貼る先のパネルで決まる。**
    order = {n: i for i, (n, _) in enumerate(ENCODERS)}
    if a.prefer == "v":
        order["bitmap1v"] = -1
    else:
        order["bitmap1h"] = -1
    fmt = a.format or min(enc, key=lambda k: (enc[k][2], order[k]))
    if fmt not in enc:
        sys.exit(f"{fmt} では符号化できない（色数 {len(uniq)}）")
    data, pal, total = enc[fmt]
    tr = None
    if a.transparent:
        c565 = to565(tuple(int(a.transparent[i:i+2], 16) for i in (0, 2, 4)))
        if pal:
            if c565 not in pal:
                sys.exit(f"透過色 {a.transparent} が画像に無い")
            tr = pal.index(c565)
        else:
            tr = c565
    text = emit(a.name, w, h, fmt, data, pal, uniq, sys.argv[1:], tr)
    if a.out:
        Path(a.out).write_text(text)
        print(f"{a.out}  {fmt}  {total} B  ({w}x{h}, 色数 {len(uniq)})")
    else:
        print(text)


if __name__ == "__main__":
    main()
