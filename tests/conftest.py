"""TinyGFX テストの共通フック。

各テストの前に `<sketch_dir>/output/` を消す。生成物（PPM など）が
前回の実行から残っていて、失敗を成功に見せかけないようにするため。

注意: `output` という名前のディレクトリを無条件に rmtree する。
他リポジトリへコピーするときは中身を確認すること。
"""

import shutil
from pathlib import Path


def pytest_runtest_setup(item):
    output_dir = Path(item.fspath).parent / "output"
    if output_dir.exists():
        shutil.rmtree(output_dir)
