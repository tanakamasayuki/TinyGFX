"""Catching things that get linked in by a chain of references (Tier 0).

Dropping unused code is the linker's job (--gc-sections). What is checked here
is **whether something that should have dropped is being held alive by a
reference.** The rules are docs/CORE_DESIGN.ja.md 7.4 (R1-R9); the table is
docs/FOOTPRINT.ja.md 8.

Everything is judged as a difference from base, so that things the core brings
in by itself - `_malloc_r` and friends - are not blamed on TinyGFX.
"""

from pathlib import Path

import pytest

import tinygfx_build as tb

pytestmark = [
    pytest.mark.slow,
    pytest.mark.skipif(not tb.have_arduino_cli(), reason="no arduino-cli"),
    pytest.mark.skipif(not tb.have_core(tb.CH32V003), reason="the CH32 core is not installed"),
]

# Names that must not appear, per construct (substring of the mangled name)
FORBIDDEN = {
    "a": ["drawLine", "drawCircle", "fillRoundRect", "fillTriangle", "drawChar",
          "tgfxDigits", "pushImage", "TileCanvas", "TinyGFXPrint"],
    "b": ["drawLine", "drawCircle", "fillRoundRect", "fillTriangle", "drawChar",
          "tgfxDigits", "pushImage", "TileCanvas", "TinyGFXPrint"],
    "c": ["drawChar", "tgfxDigits", "pushImage", "TileCanvas", "TinyGFXPrint"],
    "d": ["pushImage", "TileCanvas", "TinyGFXPrint"],
    "e": ["TileCanvas", "TinyGFXPrint"],
}

# Forbidden in every construct: absent from base, so appearing means TinyGFX did it
COMMON_FORBIDDEN = [
    "__addsf3", "__mulsf3", "__divsf3", "__floatsisf",  # floating point
    "printFloat", "_dtoa",                              # float formatting
    "__udivsi3", "__umodsi3", "__divsi3",               # division (rv32ec has no instruction)
    "__cxa_guard_acquire",                              # function-local statics (R9)
]


# One decoder symbol per font format. An unused format must not appear at all.
FORMAT_SYMBOLS = {
    "cell": "tinygfx_cell",
    "u8g2": "tinygfx_u8g2",
}

# construct -> the formats it uses
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
    """nm works. An empty set would make every test below pass vacuously."""
    assert syms["base"], "no symbols read from base (check where nm is)"
    assert tb.contains(syms["e"], "fillRect"), "no fillRect in e; the check itself is wrong"


@pytest.mark.parametrize("construct", list(FORBIDDEN))
def test_unused_features_are_not_linked(syms, construct):
    base = syms["base"]
    names = syms[construct]
    leaked = [
        f for f in FORBIDDEN[construct]
        if tb.contains(names, f) and not tb.contains(base, f)
    ]
    assert not leaked, (
        f"construct {construct} links unused features: {leaked}. "
        "One of R1-R9 in docs/CORE_DESIGN.ja.md 7.4 is being broken"
    )


@pytest.mark.parametrize("construct", list(FORBIDDEN))
def test_no_float_no_division(syms, construct):
    base = syms["base"]
    names = syms[construct]
    leaked = [
        f for f in COMMON_FORBIDDEN
        if tb.contains(names, f) and not tb.contains(base, f)
    ]
    assert not leaked, f"construct {construct} links forbidden symbols: {leaked}"


@pytest.mark.parametrize("construct", list(FORMAT_USED))
def test_unused_font_formats_are_not_linked(syms, construct):
    """**A font format you do not use must not be linked.**

    The core knows nothing about font formats; a font points at its own decoder
    (docs/CORE_DESIGN.ja.md 9). A format that is not included is referenced by
    nothing and therefore drops. This is what actually holds up the claim that
    adding formats does not grow the footprint.
    """
    names = syms[construct]
    used = FORMAT_USED[construct]
    problems = []
    for key, sym in FORMAT_SYMBOLS.items():
        present = tb.contains(names, sym)
        if key in used and not present:
            problems.append(f"{key} is in use but its symbol is missing (the check is wrong)")
        if key not in used and present:
            problems.append(f"{key} is not used yet is linked")
    assert not problems, f"construct {construct}: " + "; ".join(problems)


# --- a converter bundle -----------------------------------------------------
#
# GfxImageToolJs puts every image of a project into one header, alongside index
# arrays (`images_file_refs`, `images_file_names`, ...). **A sketch includes the
# bundle and draws one picture**, so what the other ten cost is the question.

SKETCHES = Path(__file__).parent / "sketches"
BUNDLE = Path(__file__).parent.parent / "image_oracle"

# Data symbols belonging to images the sketch never draws
UNDRAWN = ["photoData", "gradData", "bandsData", "quantData", "odd_rleData"]


@pytest.fixture(scope="module")
def bundle():
    out = {}
    for name in ("img_one", "img_index"):
        build = tb.compile_sketch(SKETCHES / name, tb.CH32V003,
                                  extra_include=tb.FONTS + [BUNDLE])
        out[name] = (tb.symbols(build), build["flash"])
    return out


def test_undrawn_images_in_a_bundle_are_not_linked(bundle):
    """**Including a bundle must cost only the pictures you draw.**

    The converter emits every image of a project into one header and says so in
    its README. This is the measurement behind that claim: `img_one` draws one
    of eleven images and touches no index array.
    """
    names, _ = bundle["img_one"]
    assert tb.contains(names, "iconData"), (
        "the image the sketch does draw is missing; the check is wrong")
    leaked = [s for s in UNDRAWN if tb.contains(names, s)]
    assert not leaked, (
        f"a bundle of 11 images keeps {leaked} that nothing draws. "
        "Either --gc-sections is off or an index array is holding them alive")


def test_a_runtime_index_keeps_the_whole_bundle(bundle):
    """**The other half of the same fact**, so the test above is not vacuous.

    Index the bundle with a value the compiler cannot fold and every image has
    to stay. Measured at +6,420 B on this board, 39% of its 16,384
    (docs/IMAGE_FORMAT.ja.md). Not a defect - it is what indexing means - but it
    is the difference between free and a third of the flash.
    """
    names, idx_flash = bundle["img_index"]
    _, one_flash = bundle["img_one"]
    missing = [s for s in UNDRAWN if not tb.contains(names, s)]
    assert not missing, (
        f"a runtime index dropped {missing}, which cannot be right: "
        "any of them could be the one selected")
    print(f"\nbundle index: {one_flash} -> {idx_flash} "
          f"({idx_flash - one_flash:+} B for all 11 images)")
    assert idx_flash > one_flash
