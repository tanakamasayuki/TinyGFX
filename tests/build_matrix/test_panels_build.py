"""**Every panel in the catalogue must compile, on every target core.**

Tier 2. Nothing is run; only that it builds.

A panel header is generated data (tools/gen_panels.py), and the catalogue is
meant to grow to dozens of entries. **Nothing else would notice a broken one**:
the host tests drive one panel, and an entry nobody includes is never compiled.

Also checks the catalogue is not stale - that the checked-in headers are what
the generator produces now.
"""

import subprocess

import pytest

import tinygfx_build as tb

PANELS = sorted(p.name for p in (tb.REPO / "src" / "TinyGFX" / "panels").glob("*.h"))

# Which bus each driver family is exercised with. A paged panel needs a
# framebuffer; a colour one does not.
PAGED = ("SSD1306", "SH1106")

CORES = [
    ("ch32v003", tb.CH32V003),
    ("uno", "arduino:avr:uno"),
]

pytestmark = [
    pytest.mark.slow,
    pytest.mark.skipif(not tb.have_arduino_cli(), reason="no arduino-cli"),
]


def test_catalogue_is_not_stale():
    """The checked-in headers must match what the generator writes today.

    Editing a panel header by hand is silently undone by the next generator
    run, so the two must not be allowed to drift.
    """
    r = subprocess.run(["python3", str(tb.REPO / "tools" / "gen_panels.py"), "--check"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    print("  " + r.stdout.strip())


@pytest.mark.parametrize("core,fqbn", CORES, ids=[c for c, _ in CORES])
def test_every_panel_compiles(core, fqbn, tmp_path):
    if not tb.have_core(fqbn):
        pytest.skip(f"the {fqbn} core is not installed")
    assert PANELS, "no panels found; has the catalogue moved?"

    for h in PANELS:
        cls = "TinyGFXPanel" + h[:-2]
        paged = h.startswith(PAGED)
        sketch = tmp_path / h[:-2]
        sketch.mkdir()
        if paged:
            body = (f"#include <TinyGFX/BusI2C.h>\n"
                    f"#include <TinyGFX/panels/{h}>\n"
                    f"TinyGFXBusI2C bus(Wire);\n"
                    f"static uint8_t fb[{cls}::kBufferBytes];\n"
                    f"{cls} panel(bus, fb);\n")
        else:
            body = (f"#include <TinyGFX/BusSoftSPI.h>\n"
                    f"#include <TinyGFX/panels/{h}>\n"
                    f"TinyGFXBusSoftSPI bus(5, 6, 3, 4);\n"
                    f"{cls} panel(bus, 2);\n")
        (sketch / f"{sketch.name}.ino").write_text(
            "#include <TinyGFX.h>\n" + body
            + "TinyGFX lcd(panel);\n"
            "void setup() { lcd.begin(); lcd.fillScreen(TFT_BLACK); }\n"
            "void loop() {}\n")
        try:
            tb.compile_sketch(sketch, fqbn)
        except tb.BuildError as e:
            pytest.fail(f"{core}: panels/{h} does not build\n  {e}")

    print(f"  {core:<9} {len(PANELS)} panels built")
