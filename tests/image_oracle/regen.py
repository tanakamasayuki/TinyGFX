#!/usr/bin/env python3
"""Runs GfxImageToolJs over `sources/` and writes what the oracle compares.

    python3 regen.py                # rewrite generated/ and expected/
    python3 regen.py --check        # fail if either is out of date
    python3 regen.py --tool <path>  # a checkout other than the default

**One invocation produces both halves of the oracle**: the header TinyGFX
compiles, and the picture it is measured against.

    sources/*.png  ->  generated/images.h    the code under test
                   ->  expected/*.png        the pixels after conversion

The expected image is **the tool's own `--preview` output**, not something
recomputed here. That is the point: an encoder checked against a decoder that
shares its assumptions agrees with itself. Dithering and colour reduction have
no second opinion available at all - nothing on this side can say which 16
colours the quantiser should have kept.

`--check` exists because the two folders are committed. If the tool changes
what it emits, the committed pair goes stale and **the test would keep passing
against yesterday's answer**. `test_image_oracle.py` runs `--check` where the
tool is installed and skips where it is not; the comparison itself needs only
what is committed, so it runs anywhere.
"""

import argparse
import filecmp
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).parent
SOURCES = HERE / "sources"
GENERATED = HERE / "generated"
EXPECTED = HERE / "expected"

DEFAULT_TOOL = Path.home() / "dev" / "GfxImageToolJs" / "bin" / "gfx-image-tool.js"


def run(tool, out_dir, preview_dir):
    """Convert the whole folder in one pass. Returns what the tool reported."""
    cmd = ["node", str(tool), "build", str(SOURCES),
           "--out", str(out_dir), "--preview", str(preview_dir)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"gfx-image-tool failed:\n{r.stdout}{r.stderr}")
    return r.stdout + r.stderr  # the tool reports what it wrote on stderr


def differences(a, b):
    """Every file that is missing, extra or different between two folders."""
    names = {p.name for p in a.iterdir() if p.is_file()} | \
            {p.name for p in b.iterdir() if p.is_file()}
    out = []
    for n in sorted(names):
        pa, pb = a / n, b / n
        if not pa.exists():
            out.append(f"{n}: no longer produced")
        elif not pb.exists():
            out.append(f"{n}: newly produced")
        elif not filecmp.cmp(pa, pb, shallow=False):
            out.append(f"{n}: differs")
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="do not write; exit 1 if committed output is stale")
    ap.add_argument("--tool", default=str(DEFAULT_TOOL),
                    help=f"path to gfx-image-tool.js (default: {DEFAULT_TOOL})")
    args = ap.parse_args()

    if not Path(args.tool).exists():
        raise SystemExit(f"gfx-image-tool.js not found at {args.tool}\n"
                         "Pass --tool <path>, or skip: the committed output "
                         "is what the test compares against.")

    if not args.check:
        GENERATED.mkdir(exist_ok=True)
        EXPECTED.mkdir(exist_ok=True)
        print(run(args.tool, GENERATED, EXPECTED), end="")
        return 0

    with tempfile.TemporaryDirectory() as tmp:
        gen, exp = Path(tmp) / "generated", Path(tmp) / "expected"
        gen.mkdir()
        exp.mkdir()
        run(args.tool, gen, exp)
        stale = [f"generated/{d}" for d in differences(GENERATED, gen)] + \
                [f"expected/{d}" for d in differences(EXPECTED, exp)]

    if stale:
        print("committed output is out of date - run: python3 regen.py",
              file=sys.stderr)
        for s in stale:
            print(f"  {s}", file=sys.stderr)
        return 1
    print("up to date")
    return 0


if __name__ == "__main__":
    sys.exit(main())
