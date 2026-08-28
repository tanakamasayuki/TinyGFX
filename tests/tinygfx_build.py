"""arduino-cli を叩いてビルドサイズとシンボルを取るための共通ヘルパ。

Tier 0（footprint / linkprune）はスケッチを**実行しない**。ビルドして
サイズとシンボル表を見るだけなので pytest-embedded の dut は使わない。
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CONSTRUCTS = REPO / "tests" / "constructs"
FONTS = [REPO / "tests" / "common_libs" / "tgfx_font" / "src",
         REPO / "tests" / "u8g2"]

# 基準機。docs/FOOTPRINT.ja.md §2
#
# 環境変数 TINYGFX_FQBN で別のコアに向けられる。開発中の新コアで測るときに使う:
#   TINYGFX_FQBN=ch32-riscv-ug:ch32v:CH32V003:pnum=CH32V003F4P6 uv run pytest footprint -s
# CI は既定のまま（新コアはまだ Boards Manager から入らない環境がある）。
DEFAULT_FQBN = "ch32-riscv-arduino:ch32riscv:CH32V003_EVT"
CH32V003 = os.environ.get("TINYGFX_FQBN", DEFAULT_FQBN)

# 構成の一覧。docs/FOOTPRINT.ja.md §4
CONSTRUCT_ORDER = ["base", "a", "b", "c", "d", "e", "t", "p1", "p2"]


class BuildError(RuntimeError):
    pass


def have_arduino_cli() -> bool:
    return shutil.which("arduino-cli") is not None


def have_core(fqbn: str) -> bool:
    pkg = fqbn.split(":")[0] + ":" + fqbn.split(":")[1]
    out = subprocess.run(
        ["arduino-cli", "core", "list"], capture_output=True, text=True, check=False
    ).stdout
    return any(line.split()[0] == pkg for line in out.splitlines()[1:] if line.strip())


def compile_construct(name: str, fqbn: str = CH32V003, defines=None) -> dict:
    """1 構成をビルドして {flash, ram, max_flash, max_ram, elf, properties} を返す。"""
    sketch = CONSTRUCTS / name
    if not sketch.is_dir():
        raise BuildError(f"no such construct: {sketch}")
    return compile_sketch(sketch, fqbn, extra_include=FONTS, defines=defines)


def compile_profile(sketch, profile: str) -> dict:
    """sketch.yaml のプロファイルでビルドする（examples 用）。

    プロファイルがあるスケッチは --fqbn だと「そのプラットフォームは
    プロファイルに宣言されていない」と怒られるので、こちらを使う。
    """
    sketch = Path(sketch)
    return _run(["arduino-cli", "compile", "--profile", profile, "--json", str(sketch)],
                sketch.name)


def compile_sketch(sketch, fqbn: str, extra_include=None, defines=None) -> dict:
    """任意のスケッチをビルドする。ライブラリはこのリポジトリを使う。

    `defines` は {"TINYGFX_FONT_BG": 0} の形。`extra_include` と同じ
    extra_flags に載るので、両方渡しても片方が消えない。
    """
    sketch = Path(sketch)
    name = sketch.name
    cmd = [
        "arduino-cli", "compile",
        "--fqbn", fqbn,
        "--library", str(REPO),
    ]
    flags = []
    if extra_include is not None:
        dirs = extra_include if isinstance(extra_include, list) else [extra_include]
        flags += [f"-I{d}" for d in dirs]
    if defines:
        flags += [f"-D{k}={v}" for k, v in defines.items()]
    if flags:
        cmd += ["--build-property", f"compiler.cpp.extra_flags={' '.join(flags)}"]
    cmd += ["--json", str(sketch)]
    return _run(cmd, name)


def _run(cmd, name: str) -> dict:
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
    try:
        data = json.loads(proc.stdout)
    except json.JSONDecodeError:
        data = {}
    if proc.returncode != 0:
        # --json のときコンパイラの出力は compiler_err に入る。
        # build_properties の羅列に埋もれるので、そこだけ取り出す。
        detail = data.get("compiler_err") or proc.stderr or proc.stdout
        lines = [ln for ln in detail.splitlines()
                 if any(k in ln for k in ("error", "Error", "will not fit", "overflowed"))]
        raise BuildError(f"{name}: build failed: " + " / ".join(lines[:3])[:400])
    result = data.get("builder_result", data)
    sizes = {s["name"]: s for s in result.get("executable_sections_size", [])}
    build_path = Path(result["build_path"])
    elf = build_path / f"{name}.ino.elf"
    return {
        "name": name,
        "flash": sizes.get("text", {}).get("size"),
        "max_flash": sizes.get("text", {}).get("max_size"),
        "ram": sizes.get("data", {}).get("size"),
        "max_ram": sizes.get("data", {}).get("max_size"),
        "elf": elf,
        "properties": result.get("build_properties", []),
    }


def _nm_path(properties: list[str]) -> Path | None:
    """build_properties から nm の場所を割り出す。"""
    props = {}
    for line in properties:
        if "=" in line:
            k, v = line.split("=", 1)
            props[k] = v
    compiler_path = props.get("compiler.path")
    size_cmd = props.get("compiler.size.cmd", "")
    if not compiler_path or not size_cmd:
        return None
    nm = Path(compiler_path) / size_cmd.replace("size", "nm")
    return nm if nm.exists() else None


def symbols(build: dict) -> set[str]:
    """ELF のシンボル名の集合。見つからなければ空集合。"""
    nm = _nm_path(build["properties"])
    if nm is None or not build["elf"].exists():
        return set()
    out = subprocess.run(
        [str(nm), "--defined-only", str(build["elf"])],
        capture_output=True, text=True, check=False,
    ).stdout
    names = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            names.add(parts[-1])
    return names


def contains(names: set[str], needle: str) -> bool:
    """マングル名の部分一致。`drawCircle` は `_ZN7TinyGFX10drawCircleEssst` に当たる。"""
    return any(needle in n for n in names)
