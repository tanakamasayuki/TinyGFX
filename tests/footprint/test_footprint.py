"""フットプリントの回帰テスト（Tier 0）。

構成 base..t を CH32V003 でビルドし、base からの増分が予算内かを見る。
予算と実測は docs/FOOTPRINT.ja.md §5 にある。**数字は常に出力する**
（予算内でも増分が見えるように）。

p1 / p2 は「値札」を出すための参考構成。予算の対象外で、
p2（float）は基準機に載らないことを記録するだけ。
"""

import pytest

import tinygfx_build as tb

pytestmark = [
    pytest.mark.slow,
    pytest.mark.skipif(not tb.have_arduino_cli(), reason="arduino-cli がない"),
    pytest.mark.skipif(not tb.have_core(tb.CH32V003), reason="CH32 コアが入っていない"),
]

# base からの増分の上限。docs/FOOTPRINT.ja.md §5
BUDGET = {
    "a": (1800, 96),    # fillScreen まで
    "b": (2100, 96),    # + 矩形・点・水平垂直線
    "c": (5200, 96),    # + 全プリミティブ
    "d": (6400, 96),    # + 文字（フォントデータ 384 B を含む）
    "e": (7200, 96),    # + pushImage
    "t": (8400, 700),   # + TileCanvas（240px×1 行の帯バッファ 480 B を含む）
}

# 予算の対象外。数字を記録するだけ。
REFERENCE = ["p1", "p2"]


@pytest.fixture(scope="module")
def builds():
    out = {}
    for name in ["base"] + list(BUDGET) + REFERENCE:
        try:
            out[name] = tb.compile_construct(name)
        except tb.BuildError as exc:
            out[name] = exc
    return out


def test_footprint(builds):
    base = builds["base"]
    assert not isinstance(base, tb.BuildError), f"base のビルドに失敗: {base}"

    rows = [f"{'構成':<6}{'flash':>8}{'Δflash':>8}{'予算':>8}{'ram':>7}{'Δram':>7}{'予算':>7}"]
    rows.append(f"{'base':<6}{base['flash']:>8}{'-':>8}{'-':>8}{base['ram']:>7}{'-':>7}{'-':>7}")
    failures = []

    for name, (flash_budget, ram_budget) in BUDGET.items():
        b = builds[name]
        if isinstance(b, tb.BuildError):
            failures.append(f"{name}: ビルド失敗")
            rows.append(f"{name:<6}{'BUILD FAILED':>38}")
            continue
        dflash = b["flash"] - base["flash"]
        dram = b["ram"] - base["ram"]
        rows.append(
            f"{name:<6}{b['flash']:>8}{dflash:>8}{flash_budget:>8}"
            f"{b['ram']:>7}{dram:>7}{ram_budget:>7}"
        )
        if dflash > flash_budget:
            failures.append(f"{name}: flash +{dflash} > 予算 {flash_budget}")
        if dram > ram_budget:
            failures.append(f"{name}: RAM +{dram} > 予算 {ram_budget}")

    for name in REFERENCE:
        b = builds[name]
        if isinstance(b, tb.BuildError):
            rows.append(f"{name:<6}{'載らない（参考値）':>30}")
        else:
            rows.append(
                f"{name:<6}{b['flash']:>8}{b['flash'] - base['flash']:>8}{'参考':>8}"
                f"{b['ram']:>7}{b['ram'] - base['ram']:>7}{'参考':>7}"
            )

    print("\n" + "\n".join(rows))
    assert not failures, "予算超過: " + "; ".join(failures)


def test_float_does_not_fit_on_the_reference_board(builds):
    """float を印字する構成は基準機に載らない。

    これは失敗ではなく**仕様**。値札として記録しておくためのテスト
    （docs/DECISIONS.ja.md D6）。載るようになったら数字を採り直す。
    """
    b = builds["p2"]
    if not isinstance(b, tb.BuildError):
        pytest.skip(f"p2 が載るようになった: flash={b['flash']}。FOOTPRINT.ja.md を更新すること")
    assert "FLASH" in str(b) or "will not fit" in str(b), f"想定外の失敗: {b}"
