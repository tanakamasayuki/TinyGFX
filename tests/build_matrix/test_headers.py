"""**Every public header must compile on its own, on every target core.**

Tier 2. Nothing is run; only that it **builds**.

Without this, two things go unchecked.

- **Whether each header stands on its own.** The way these are actually used is
  "`<TinyGFX.h>`, then add what you need", so a header that quietly depends on
  another sub-header breaks only on the user's machine. Splitting out
  `Progmem.h` was exactly that shape: panels used `tinygfx_rd8` but could only
  reach it through `Font.h`.
- **Whether it builds anywhere but the host.** The host tests only ever run on
  `lang-ship:host`. A new header passes there and feels finished, then breaks
  on AVR's PROGMEM or the CH32V003's 16-bit int.

The counterpart of `test_example_builds`, which runs the examples: that one
asks whether the combinations work, this one whether the pieces stand alone.
"""

from pathlib import Path

import pytest

import tinygfx_build as tb

SRC = tb.REPO / "src" / "TinyGFX"

# A header that pulls in an Arduino bus needs that library on the core.
# The CH32V003 core has no SPI (docs/EXTERNAL_REQUESTS.ja.md E2).
NEEDS_SPI = {"BusSPI.h"}

# The target cores. **Not settling for the host alone** is the whole point.
CORES = [
    ("ch32v003", tb.CH32V003),
    ("uno", "arduino:avr:uno"),
    ("esp32", "esp32:esp32:esp32"),
]

HEADERS = sorted(p.name for p in SRC.glob("*.h"))

pytestmark = [
    pytest.mark.slow,
    pytest.mark.skipif(not tb.have_arduino_cli(), reason="no arduino-cli"),
]


@pytest.mark.parametrize("core,fqbn", CORES, ids=[c for c, _ in CORES])
def test_every_header_compiles(core, fqbn, tmp_path):
    if not tb.have_core(fqbn):
        pytest.skip(f"the {fqbn} core is not installed")

    skipped, built = [], []
    for h in HEADERS:
        if core == "ch32v003" and h in NEEDS_SPI:
            skipped.append(h)
            continue
        sketch = tmp_path / h[:-2]
        sketch.mkdir()
        # `<TinyGFX.h>` plus that one header. **No other sub-header.**
        #
        # Including only a sub-header does not make arduino-cli find the
        # library: it resolves through `TinyGFX.h`, the header named after the
        # library (that is what `includes=TinyGFX.h` in `library.properties`
        # is). So this pair is the real contract.
        (sketch / f"{sketch.name}.ino").write_text(
            f"#include <TinyGFX.h>\n#include <TinyGFX/{h}>\n"
            "void setup() {}\nvoid loop() {}\n")
        try:
            tb.compile_sketch(sketch, fqbn)
            built.append(h)
        except tb.BuildError as e:
            pytest.fail(
                f"{core}: <TinyGFX.h> followed by <TinyGFX/{h}> does not build\n  {e}")

    print(f"  {core:<9} {len(built)} headers built"
          + (f" (skipped {', '.join(skipped)})" if skipped else ""))
