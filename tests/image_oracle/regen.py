#!/usr/bin/env python3
"""Runs GfxImageToolJs over `sources/` and writes what the oracle compares.

    python3 regen.py                # rewrite generated/ and expected/
    python3 regen.py --check        # fail if either is out of date
    python3 regen.py --tool <path>  # a checkout other than the default

**One invocation produces both halves of the oracle**: the header TinyGFX
compiles, and the picture it is measured against.

    sources/*.png  ->  generated/images.h    the code under test
                   ->  expected/*.png        the pixels after conversion

Where they go is in `sources/.imagesconfig`, not here, so **the same conversion
happens whether it is this script or a person running the tool by hand.**

The expected image is **the tool's own `--preview` output**, not something
recomputed here. That is the point: an encoder checked against a decoder that
shares its assumptions agrees with itself. Dithering and colour reduction have
no second opinion available at all - nothing on this side can say which 16
colours the quantiser should have kept.

`--check` is the tool's own, passed straight through. It exists because the two
folders are committed: if the tool changes what it emits, the committed pair
goes stale and **the test would keep passing against yesterday's answer**.
`test_image_oracle.py` runs it where the tool is installed and skips where it
is not; the comparison itself needs only what is committed, so it runs anywhere.
"""

import argparse
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).parent
SOURCES = HERE / "sources"

DEFAULT_TOOL = Path.home() / "dev" / "GfxImageToolJs" / "bin" / "gfx-image-tool.js"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="do not write; fail if committed output is stale")
    ap.add_argument("--tool", default=str(DEFAULT_TOOL),
                    help=f"path to gfx-image-tool.js (default: {DEFAULT_TOOL})")
    args = ap.parse_args()

    if not Path(args.tool).exists():
        raise SystemExit(f"gfx-image-tool.js not found at {args.tool}\n"
                         "Pass --tool <path>, or skip: the committed output "
                         "is what the test compares against.")

    cmd = ["node", args.tool, "build", str(SOURCES)]
    if args.check:
        cmd.append("--check")
    r = subprocess.run(cmd, capture_output=True, text=True)
    # The tool reports what it wrote, and what was stale, on stderr
    sys.stdout.write(r.stdout)
    sys.stderr.write(r.stderr)
    if r.returncode != 0 and args.check:
        print("run: python3 regen.py", file=sys.stderr)
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
