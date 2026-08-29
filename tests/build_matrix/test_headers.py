"""**すべての公開ヘッダが、すべての対象コアで単体ビルドできること。**

Tier 2。実行しない。**ビルドが通ることだけ**見る。

これが無いと 2 つ抜ける。

- **各ヘッダが単独で成立しているか。** 実際の使われ方は「`<TinyGFX.h>` の後に
  必要なものだけ足す」なので、うっかり別のサブヘッダに依存していると利用者の
  手元でだけ壊れる。`Progmem.h` を切り出したときがまさにその形だった
  （パネルが `tinygfx_rd8` を使うのに `Font.h` 経由でしか手に入らなかった）
- **ホスト以外で通るか。** ホストテストは `lang-ship:host` でしか動かさない。
  新しいヘッダはそこだけ通って満足しがちで、AVR の PROGMEM や
  CH32V003 の 16 ビット int で初めて壊れることがある

examples を回す `test_example_builds` と補い合う関係。あちらは「組み合わせが
動くか」、こちらは「部品が単体で成立するか」。
"""

from pathlib import Path

import pytest

import tinygfx_build as tb

SRC = tb.REPO / "src" / "TinyGFX"

# Arduino のバスを引くヘッダは、そのコアに該当ライブラリが要る。
# CH32V003 のコアには SPI が無い（docs/EXTERNAL_REQUESTS.ja.md E2）。
NEEDS_SPI = {"BusSPI.h"}

# 対象コア。**ホストだけで満足しないこと**がこのテストの主眼。
CORES = [
    ("ch32v003", tb.CH32V003),
    ("uno", "arduino:avr:uno"),
    ("esp32", "esp32:esp32:esp32"),
]

HEADERS = sorted(p.name for p in SRC.glob("*.h"))

pytestmark = [
    pytest.mark.slow,
    pytest.mark.skipif(not tb.have_arduino_cli(), reason="arduino-cli がない"),
]


@pytest.mark.parametrize("core,fqbn", CORES, ids=[c for c, _ in CORES])
def test_every_header_compiles(core, fqbn, tmp_path):
    if not tb.have_core(fqbn):
        pytest.skip(f"{fqbn} のコアが入っていない")

    skipped, built = [], []
    for h in HEADERS:
        if core == "ch32v003" and h in NEEDS_SPI:
            skipped.append(h)
            continue
        sketch = tmp_path / h[:-2]
        sketch.mkdir()
        # `<TinyGFX.h>` + そのヘッダ 1 本だけ。**他のサブヘッダは足さない。**
        #
        # サブヘッダだけを include しても arduino-cli はライブラリを引かない
        # （ライブラリ名と同じ `TinyGFX.h` を見て解決するため。`library.properties`
        # の `includes=TinyGFX.h` がそれ）。だからこれが実際の契約。
        (sketch / f"{sketch.name}.ino").write_text(
            f"#include <TinyGFX.h>\n#include <TinyGFX/{h}>\n"
            "void setup() {}\nvoid loop() {}\n")
        try:
            tb.compile_sketch(sketch, fqbn)
            built.append(h)
        except tb.BuildError as e:
            pytest.fail(
                f"{core}: <TinyGFX.h> の後に <TinyGFX/{h}> を足すと通らない\n  {e}")

    print(f"  {core:<9} {len(built)} 本ビルド"
          + (f"（{', '.join(skipped)} は skip）" if skipped else ""))
