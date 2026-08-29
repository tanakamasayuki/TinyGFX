"""A jig for working out which conditions make read-back work on hardware.

**Not an ordinary test.** It exists to compare what matters in a single flash.
Once the answer is known, delete it or keep only the combination that works.
"""

import os
from pathlib import Path

import pytest

PORT = os.environ.get("TEST_SERIAL_PORT_M5STACK") or os.environ.get("TEST_SERIAL_PORT")

pytestmark = [
    pytest.mark.hardware,
    pytest.mark.skipif(not PORT or not Path(PORT).exists(), reason="no M5Stack attached"),
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
    assert arduino_test.results, "nothing ran at all"
