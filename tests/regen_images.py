#!/usr/bin/env python3
"""Regenerates every committed image header from its sources.

    python3 regen_images.py                 # all projects
    python3 regen_images.py image_fmt       # just one
    python3 regen_images.py --check         # fail if any is out of date
    python3 regen_images.py --tool <cmd>    # a checkout instead of the release

**Nothing is installed.** The released package is fetched and cached by npx,
pinned in the command itself: `npx --yes gfx-image-tool@1.0.0`.

Each project is the converter's own layout - sources and `.imagesconfig` in
`images/`, the bundle beside it - so **the same conversion happens whether it
is this script or a person running the tool by hand.**

    image_oracle/   the tool's output measured against the tool's own pixels
    image_fmt/      one picture per encoding, to run each decoder
    sh1106/         the splash the page-addressed panel pushes

`images/.gfx-image-tool/` is the tool's disposable cache and is git-ignored;
deleting it changes no output.

## Why the version is pinned

A different version may encode differently, and then `--check` fails with
`images.h  mismatch` - **which looks exactly like someone having changed a
source image**. The version travels in the npx spec, so the default path cannot
drift. `--tool` can point anywhere, so **when a check fails, the version is read
back and reported first** if it is not the pinned one. Upgrading is deliberate:
change TOOL_SPEC, run this, and commit what moved.

Exit 3 means the tool could not be obtained at all - no npx, or no network on a
cold cache. The tests skip on it rather than failing: **what they compare is
committed**, so the comparisons themselves run anywhere.
"""

import argparse
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

# Every folder holding an `images/` project. Adding one here is all it takes.
PROJECTS = ["image_oracle", "image_fmt", "sh1106"]

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


def build(cmd, project, check):
    extra = ["--check"] if check else []
    return subprocess.run(cmd + ["build", str(HERE / project), *extra],
                          capture_output=True, text=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("projects", nargs="*", default=None,
                    help=f"which to do (default: {' '.join(PROJECTS)})")
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
              "Skipping: the committed output is what the tests compare "
              "against.", file=sys.stderr)
        return UNAVAILABLE

    worst = 0
    for project in (args.projects or PROJECTS):
        r = build(cmd, project, args.check)
        # The tool reports what it wrote, and what was stale, on stderr
        head = f"[{project}] "
        for line in (r.stdout + r.stderr).splitlines():
            print(head + line, file=sys.stderr)
        worst = worst or r.returncode

    if worst and args.check:
        # **Say "wrong version" rather than let it read as a changed image.**
        # Only reachable through --tool; the npx spec pins the default.
        if got != TOOL_VERSION:
            print(f"\ngfx-image-tool {got} produced this, and the committed "
                  f"output was made by {TOOL_VERSION}.\n"
                  "  A different version may encode differently, and the "
                  "mismatch above would read as a changed source image.\n"
                  "  Drop --tool to use the pinned release, or raise "
                  "TOOL_SPEC and commit what moved.", file=sys.stderr)
        else:
            print("run: python3 tests/regen_images.py", file=sys.stderr)
    return worst


if __name__ == "__main__":
    sys.exit(main())
