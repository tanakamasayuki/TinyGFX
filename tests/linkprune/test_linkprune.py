"""「まわりまわって載る」の検出（Tier 0）。

未使用機能が落ちること自体はリンカ（--gc-sections）の仕事。ここで見るのは
**落ちるはずのものが参照の連鎖で残っていないか**。
規則は docs/CORE_DESIGN.ja.md §7.4（R1〜R9）、表は docs/FOOTPRINT.ja.md §8。

判定は base との差で行う。`_malloc_r` のようにコア側が最初から
持ち込んでいるものを TinyGFX のせいにしないため。
"""

import pytest

import tinygfx_build as tb

pytestmark = [
    pytest.mark.slow,
    pytest.mark.skipif(not tb.have_arduino_cli(), reason="arduino-cli がない"),
    pytest.mark.skipif(not tb.have_core(tb.CH32V003), reason="CH32 コアが入っていない"),
]

# 構成ごとに「出てはいけない」名前（マングル名への部分一致）
FORBIDDEN = {
    "a": ["drawLine", "drawCircle", "fillRoundRect", "fillTriangle", "drawChar",
          "tgfxDigits", "pushImage", "TileCanvas", "TinyGFXPrint"],
    "b": ["drawLine", "drawCircle", "fillRoundRect", "fillTriangle", "drawChar",
          "tgfxDigits", "pushImage", "TileCanvas", "TinyGFXPrint"],
    "c": ["drawChar", "tgfxDigits", "pushImage", "TileCanvas", "TinyGFXPrint"],
    "d": ["pushImage", "TileCanvas", "TinyGFXPrint"],
    "e": ["TileCanvas", "TinyGFXPrint"],
}

# 全構成に共通で出てはいけないもの（base に無いのに現れたら fail）
COMMON_FORBIDDEN = [
    "__addsf3", "__mulsf3", "__divsf3", "__floatsisf",  # 浮動小数点演算
    "printFloat", "_dtoa",                              # 浮動小数点書式化
    "__udivsi3", "__umodsi3", "__divsi3",               # 除算（rv32ec は命令が無い）
    "__cxa_guard_acquire",                              # 関数内 static（R9）
]


# フォント形式ごとのデコーダのシンボル。使っていない形式は 1 つも出てはいけない。
FORMAT_SYMBOLS = {
    "cell": "tinygfx_cell",
    "u8g2": "tinygfx_u8g2",
}

# 構成 -> 使っている形式
FORMAT_USED = {
    "d": {"cell"},
    "d_u8g2": {"u8g2"},
    "d_both": {"cell", "u8g2"},
}


@pytest.fixture(scope="module")
def syms():
    out = {}
    for name in ["base"] + list(FORBIDDEN) + list(FORMAT_USED):
        build = tb.compile_construct(name)
        out[name] = tb.symbols(build)
    return out


def test_symbols_are_available(syms):
    """nm が動いていること。空だと以降のテストが素通りしてしまう。"""
    assert syms["base"], "base のシンボルが取れていない（nm の場所を確認すること）"
    assert tb.contains(syms["e"], "fillRect"), "e に fillRect が無い。判定方法がおかしい"


@pytest.mark.parametrize("construct", list(FORBIDDEN))
def test_unused_features_are_not_linked(syms, construct):
    base = syms["base"]
    names = syms[construct]
    leaked = [
        f for f in FORBIDDEN[construct]
        if tb.contains(names, f) and not tb.contains(base, f)
    ]
    assert not leaked, (
        f"構成 {construct} に未使用の機能が載っている: {leaked}。"
        " docs/CORE_DESIGN.ja.md §7.4 の R1〜R9 のどれかを破っている"
    )


@pytest.mark.parametrize("construct", list(FORBIDDEN))
def test_no_float_no_division(syms, construct):
    base = syms["base"]
    names = syms[construct]
    leaked = [
        f for f in COMMON_FORBIDDEN
        if tb.contains(names, f) and not tb.contains(base, f)
    ]
    assert not leaked, f"構成 {construct} に載ってはいけないシンボル: {leaked}"


@pytest.mark.parametrize("construct", list(FORMAT_USED))
def test_unused_font_formats_are_not_linked(syms, construct):
    """**使っていないフォント形式のデコーダはリンクされないこと。**

    コアはフォント形式を知らず、フォント側が自分のデコーダを指す
    （docs/CORE_DESIGN.ja.md §9）。include していない形式はどこからも
    参照されないので落ちる。形式を増やしてもフットプリントは増えない、
    という設計の実効的な担保。
    """
    names = syms[construct]
    used = FORMAT_USED[construct]
    problems = []
    for key, sym in FORMAT_SYMBOLS.items():
        present = tb.contains(names, sym)
        if key in used and not present:
            problems.append(f"{key} を使っているのにシンボルが無い（判定方法がおかしい）")
        if key not in used and present:
            problems.append(f"{key} を使っていないのにリンクされている")
    assert not problems, f"構成 {construct}: " + "; ".join(problems)
