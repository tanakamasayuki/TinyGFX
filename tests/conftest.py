"""Shared hooks for the TinyGFX tests.

Clears `<sketch_dir>/output/` before every test. Artefacts (PPMs and the like)
left over from a previous run would otherwise make a failure look like a pass.

Careful: this rmtree's any directory named `output`, unconditionally. Check
what is in there before copying this file into another repository.
"""

import shutil
from pathlib import Path


def pytest_runtest_setup(item):
    output_dir = Path(item.fspath).parent / "output"
    if output_dir.exists():
        shutil.rmtree(output_dir)
