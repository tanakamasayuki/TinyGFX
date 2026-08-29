"""Footprint regression (Tier 0).

Builds constructs base..t for a CH32V003 and checks the increment over base
against the budget. The budgets and the measurements are in
docs/FOOTPRINT.ja.md 5. **The numbers are always printed**, so the increments
stay visible even when everything is within budget.

p1 and p2 are reference constructs, there to produce a price tag. They are
outside the budget, and p2 (float) exists only to record that it does not fit
on the reference board.
"""

import pytest

import tinygfx_build as tb

pytestmark = [
    pytest.mark.slow,
    pytest.mark.skipif(not tb.have_arduino_cli(), reason="no arduino-cli"),
    pytest.mark.skipif(not tb.have_core(tb.CH32V003), reason="the CH32 core is not installed"),
]

# The ceiling on the increment over base. docs/FOOTPRINT.ja.md 5
# The budget must hold **on every core** - the default one and the
# in-development one alike.
BUDGET = {
    "a": (1900, 96),    # up to fillScreen
    "b": (2200, 96),    # + rectangles, pixels, H/V lines
    "c": (5200, 96),    # + every primitive
    "d": (6400, 96),    # + text (including 384 B of font data)
    "e": (7200, 96),    # + pushImage
    "t": (8400, 700),   # + TileCanvas (including a 480 B band of 240px x 1 row)
}

# Outside the budget. Recorded, not enforced.
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
    assert not isinstance(base, tb.BuildError), f"base failed to build: {base}"

    rows = [f"FQBN: {tb.CH32V003}",
            f"{'build':<6}{'flash':>8}{'Δflash':>8}{'budget':>8}"
            f"{'ram':>7}{'Δram':>7}{'budget':>7}"]
    rows.append(f"{'base':<6}{base['flash']:>8}{'-':>8}{'-':>8}{base['ram']:>7}{'-':>7}{'-':>7}")
    failures = []

    for name, (flash_budget, ram_budget) in BUDGET.items():
        b = builds[name]
        if isinstance(b, tb.BuildError):
            failures.append(f"{name}: build failed")
            rows.append(f"{name:<6}{'BUILD FAILED':>38}")
            continue
        dflash = b["flash"] - base["flash"]
        dram = b["ram"] - base["ram"]
        rows.append(
            f"{name:<6}{b['flash']:>8}{dflash:>8}{flash_budget:>8}"
            f"{b['ram']:>7}{dram:>7}{ram_budget:>7}"
        )
        if dflash > flash_budget:
            failures.append(f"{name}: flash +{dflash} > budget {flash_budget}")
        if dram > ram_budget:
            failures.append(f"{name}: RAM +{dram} > budget {ram_budget}")

    for name in REFERENCE:
        b = builds[name]
        if isinstance(b, tb.BuildError):
            rows.append(f"{name:<6}{'does not fit (reference)':>30}")
        else:
            rows.append(
                f"{name:<6}{b['flash']:>8}{b['flash'] - base['flash']:>8}{'ref':>8}"
                f"{b['ram']:>7}{b['ram'] - base['ram']:>7}{'ref':>7}"
            )

    print("\n" + "\n".join(rows))
    assert not failures, "over budget: " + "; ".join(failures)


def test_float_does_not_fit_on_the_reference_board(builds):
    """A construct that prints a float does not fit on the reference board.

    That is **the specification**, not a failure. This test exists to record
    the price tag (docs/DECISIONS.ja.md D6). If it ever starts fitting, take the
    numbers again.
    """
    b = builds["p2"]
    if not isinstance(b, tb.BuildError):
        pytest.skip(f"p2 fits now: flash={b['flash']}. Update FOOTPRINT.ja.md")
    assert "FLASH" in str(b) or "will not fit" in str(b), f"failed for an unexpected reason: {b}"
