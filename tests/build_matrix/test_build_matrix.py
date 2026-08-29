"""Tier 2 - portability, by compiling. **Nothing is run; only that it builds.**

Catches type errors and API changes in `TinyGFXBusSPI` / `TinyGFXBusSoftSPI`,
neither of which can run on the host. The examples serve as the material.

The examples carry profiles in sketch.yaml, so they build with `--profile`.
"""

import pytest

import tinygfx_build as tb

EXAMPLES = tb.REPO / "examples"
ALL = ["HelloWorld", "Shapes", "FlickerFree", "HardwareSPI", "OledI2C"]
# Board-specific examples. They cannot build on another core (the pins are fixed)
BOARD_ONLY = [("m5stack", "M5StackBasic")]

# Profile name -> the core it needs. Skip if that core is not installed.
# (A profile does not install the platform it declares, and without this check
#  CI would go and fetch an enormous core.)
PROFILE_CORE = {
    "ch32v003": "ch32-riscv-arduino:ch32riscv:CH32V003_EVT",
    "uno": "arduino:avr:uno",
    "esp32": "esp32:esp32:esp32",
    "m5stack": "esp32:esp32:m5stack_core",
}

# The CH32V003 core has no SPI library, so HardwareSPI is left out
# (docs/EXTERNAL_REQUESTS.ja.md E2).
CASES = (
    [("ch32v003", e) for e in ALL if e != "HardwareSPI"]
    + [("uno", e) for e in ALL]
    # esp32 builds are slow and rarely catch anything the others miss: one only.
    + [("esp32", "HelloWorld")]
    # For bringing hardware up (docs/MANUAL_TEST.ja.md M0). The only ILI9342 build.
    + BOARD_ONLY
)

pytestmark = [
    pytest.mark.slow,
    pytest.mark.skipif(not tb.have_arduino_cli(), reason="no arduino-cli"),
]


@pytest.mark.parametrize("profile,example", CASES, ids=lambda v: v)
def test_example_builds(profile, example):
    if not tb.have_core(PROFILE_CORE[profile]):
        pytest.skip(f"the {PROFILE_CORE[profile]} core is not installed")
    build = tb.compile_profile(EXAMPLES / example, profile)
    assert build["flash"] is not None, f"{example} @ {profile}: no size was reported"
    print(f"  {profile:<10} {example:<12} flash={build['flash']:>7} ram={build['ram']}")


def test_manual_sketch_builds():
    """The hardware bring-up sketch (`tests/manual/m5stack/`) has not rotted.

    It is never run automatically - it only means anything on real hardware.
    **This only builds it.** docs/MANUAL_TEST.ja.md M0b.
    """
    if not tb.have_core(PROFILE_CORE["m5stack"]):
        pytest.skip("the esp32 core is not installed")
    build = tb.compile_profile(tb.REPO / "tests" / "manual" / "m5stack", "m5stack")
    assert build["flash"] is not None
    print(f"  manual     m5stack      flash={build['flash']:>7} ram={build['ram']}")


def test_hardware_spi_still_fails_on_ch32():
    """**Records** that HardwareSPI does not build on the CH32V003 core.

    Not a TinyGFX defect - the state of the core (EXTERNAL_REQUESTS.ja.md E2).
    When it starts building, add it to CASES and delete this test.
    """
    if not tb.have_core(PROFILE_CORE["ch32v003"]):
        pytest.skip("the CH32 core is not installed")
    try:
        tb.compile_profile(EXAMPLES / "HardwareSPI", "ch32v003")
    except tb.BuildError as exc:
        assert "SPI.h" in str(exc), f"failed for an unexpected reason: {exc}"
        return
    pytest.skip("SPI works on the CH32 core now. Add it to CASES and delete this test")


# The text drop macros (src/TinyGFX/FontCell.h and src/TinyGFX/Gfx.h).
# All default to on. The off path is never built unless a sketch asks for it,
# so without compiling it here it rots quietly.
FONT_MACROS = ["TINYGFX_FONT_BG", "TINYGFX_FONT_SCALE", "TINYGFX_FONT_CHAIN",
               "TINYGFX_FONT_UTF8"]


@pytest.mark.parametrize("off", [[m] for m in FONT_MACROS] + [FONT_MACROS],
                         ids=lambda v: "+".join(m.split("_")[-1].lower() for m in v))
def test_font_macros_build(off):
    """Build with each drop macro off, then with all of them off.

    Prints what each one saves while it is at it. **Measure on the reference
    board (CH32V003)**: optimisations that help on AVR have turned out to hurt
    on RISC-V (docs/OPTIMIZE.ja.md J).
    """
    if not tb.have_core(tb.CH32V003):
        pytest.skip("the CH32V003 core is not installed")
    base = tb.compile_construct("t")
    got = tb.compile_construct("t", defines={m: 0 for m in off})
    saved = base["flash"] - got["flash"]
    print(f"  {'+'.join(off):<58} -{saved} B")
    assert saved >= 0, f"turning {off} off grew the build by {-saved} B"


# The two spellings of the colour constants (docs/DECISIONS.ja.md D30). The
# sketch is nothing but static_asserts, so **building at all is the check**.
# -D recreates the case where another library defined TFT_RED first.
@pytest.mark.parametrize("foreign", [False, True], ids=["alone", "foreign_tft_red"])
def test_color_macros(foreign):
    if not tb.have_core(tb.CH32V003):
        pytest.skip("the CH32V003 core is not installed")
    defines = {"TFT_RED": "0x1234", "TGFX_FOREIGN_RED": "0x1234"} if foreign else None
    base = tb.compile_construct("base")
    got = tb.compile_construct("color", defines=defines)
    # They are macros, so not one byte may be added
    assert got["flash"] == base["flash"], (
        f"the colour constants alone added {got['flash'] - base['flash']} B")


def test_text_wrap_macro():
    """`TINYGFX_TEXT_WRAP` defaults to 0 and must cost **not one byte** there.

    Setting it to 1 buys a price tag: wrapping has to know how wide a character
    is before drawing it, which is a second entry point into the font decoder
    (`advance`). See docs/DECISIONS.ja.md D33. The number is always printed.
    """
    if not tb.have_core(tb.CH32V003):
        pytest.skip("the CH32V003 core is not installed")
    off = tb.compile_construct("p1")
    on = tb.compile_construct("p1", defines={"TINYGFX_TEXT_WRAP": 1})
    cost = on["flash"] - off["flash"]
    print(f"  TINYGFX_TEXT_WRAP=1                                        +{cost} B")
    assert cost > 0, "setting it to 1 added nothing; the wrapping path is not being built"
    assert cost <= 260, f"wrapping costs {cost} B, far from the 164 B in the docs"
