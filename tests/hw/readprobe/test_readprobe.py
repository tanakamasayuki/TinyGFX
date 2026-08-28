"""実機で読み出しの効く条件を切り分ける治具。

**通常のテストではない。** 何が効いているのかを 1 回の書き込みで比べるためのもので、
結論が出たら消すか、通る組み合わせだけを残す。
"""

import os
from pathlib import Path

import pytest

PORT = os.environ.get("TEST_SERIAL_PORT_M5STACK") or os.environ.get("TEST_SERIAL_PORT")

pytestmark = [
    pytest.mark.hardware,
    pytest.mark.skipif(not PORT or not Path(PORT).exists(), reason="M5Stack が繋がっていない"),
]


def test_which_strategy_reads(arduino_test):
    try:
        arduino_test.run()
    except AssertionError:
        pass
    for r in arduino_test.results:
        print(f"  {r.name:32} {r.status:8} {r.metrics}")
        for line in r.logs:
            print(f"      {line}")
    assert arduino_test.results, "1 件も走っていない"
