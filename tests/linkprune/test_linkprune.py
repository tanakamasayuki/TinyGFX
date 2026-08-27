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
          "tinygfxFont5x7", "pushImage", "TileCanvas", "TinyGFXPrint"],
    "b": ["drawLine", "drawCircle", "fillRoundRect", "fillTriangle", "drawChar",
          "tinygfxFont5x7", "pushImage", "TileCanvas", "TinyGFXPrint"],
    "c": ["drawChar", "tinygfxFont5x7", "pushImage", "TileCanvas", "TinyGFXPrint"],
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


@pytest.fixture(scope="module")
def syms():
    out = {}
    for name in ["base"] + list(FORBIDDEN):
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
