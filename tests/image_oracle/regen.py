#!/usr/bin/env python3
"""Runs gfx-image-tool over this folder and writes what the oracle compares.

    python3 regen.py                # rewrite images.h and expected/
    python3 regen.py --check        # fail if either is out of date
    python3 regen.py --tool <cmd>   # a checkout instead of the released package

**Nothing is installed.** The released package is fetched and cached by npx,
pinned in the command itself: `npx --yes gfx-image-tool@1.0.0`.

**One invocation produces both halves of the oracle**: the header TinyGFX
compiles, and the picture it is measured against.

    images/*.png  ->  images.h         the code under test
                  ->  expected/*.png   the pixels after conversion

**The layout is the tool's own project layout**: sources in `images/`, the
bundle beside it, which is where a sketch includes it from.
`images/.gfx-image-tool/` is the tool's disposable cache and is git-ignored;
deleting it changes no output.

Where they go is in `images/.imagesconfig`, not here, so **the same conversion
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

## Why the version is pinned

A different version may encode differently, and then `--check` fails with
`images.h  mismatch` - **which looks exactly like someone having changed a
source image**. The version travels in the npx spec, so the default path cannot
drift. `--tool` can point anywhere, so **when a check fails, the version is read
back and reported first** if it is not the pinned one. Upgrading is deliberate:
change TOOL_SPEC, run `regen.py`, and commit what moved.

Exit 3 means the tool could not be obtained at all - no npx, or no network on a
cold cache. `test_image_oracle.py` skips on it rather than failing: the
comparison itself needs only what is committed.
"""

import argparse
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).parent
PROJECT = HERE

# **The released package, fetched by npx.** The version is in the spec, so the
# default path cannot pick up a different one.
TOOL_VERSION = "1.0.0"
TOOL_SPEC = f"gfx-image-tool@{TOOL_VERSION}"
NPX = ["npx", "--yes", TOOL_SPEC]

UNAVAILABLE = 3


def tool_cmd(spec):
    """[argv...] for the released package, a command, or a checkout's .js."""
    if spec is None:
        return list(NPX)
    return ["node", spec] if spec.endswith(".js") else [spec]


def probe(cmd):
    """The version it reports, or None if it cannot be run at all."""
    try:
        r = subprocess.run(cmd + ["--version"], capture_output=True, text=True)
    except OSError:
        return None
    return r.stdout.strip() if r.returncode == 0 else None


def run(cmd, extra=()):
    return subprocess.run(cmd + ["build", str(PROJECT), *extra],
                          capture_output=True, text=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="do not write; fail if committed output is stale")
    ap.add_argument("--tool", default=None,
                    help=f"command or .js path (default: npx --yes {TOOL_SPEC})")
    args = ap.parse_args()

    cmd = tool_cmd(args.tool)
    got = probe(cmd)
    if got is None:
        print(f"{' '.join(cmd)} could not be run - no npx, or no network on a "
              "cold npx cache.\n"
              "Skipping: the committed output is what the test compares "
              "against.", file=sys.stderr)
        return UNAVAILABLE

    r = run(cmd, ("--check",) if args.check else ())
    # The tool reports what it wrote, and what was stale, on stderr
    sys.stdout.write(r.stdout)
    sys.stderr.write(r.stderr)
    if r.returncode != 0 and args.check:
        # **Say "wrong version" rather than let it read as a changed image.**
        # Only reachable through --tool; the npx spec pins the default.
        if got != TOOL_VERSION:
            print(f"\ngfx-image-tool {got} produced this, and the committed "
                  f"output was made by {TOOL_VERSION}.\n"
                  "  A different version may encode differently, and the "
                  "mismatch above would read as a changed source image.\n"
                  "  Drop --tool to use the pinned release, or raise "
                  "TOOL_VERSION in regen.py and commit what moved.",
                  file=sys.stderr)
        else:
            print("run: python3 regen.py", file=sys.stderr)
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
