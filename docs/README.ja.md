# ドキュメント案内

> **この下の文書はすべて日本語のみ。** 開発中の内部記録であって、
> 利用者が読むものではないため（[DECISIONS.ja.md](DECISIONS.ja.md) D23）。
> 利用者向けの文書は英語と日本語の両方を用意している —
> [../README.md](../README.md) / [../README.ja.md](../README.ja.md)。

TinyGFX の設計文書。**たたき台だが、フットプリントの数字は実測に置き換わっている。** どの文書も確定していない項目には「**暫定**」と書いてある。

**言語方針は 3 段。正本は日本語版。** 兄弟プロジェクト（PaperCanvas / BarcodeKit）と同じ。

| 区分 | 言語 | 対象 |
| --- | --- | --- |
| 使う人が読むもの | 日英 | `../README.ja.md`、`GUIDE.ja.md`、`API.ja.md`、`../examples/README.ja.md`、`../tests/README.ja.md` |
| 内部の記録・作業メモ | 日本語のみ | [REQUIREMENTS.ja.md](REQUIREMENTS.ja.md)、[CORE_DESIGN.ja.md](CORE_DESIGN.ja.md)、[DECISIONS.ja.md](DECISIONS.ja.md)、[FOOTPRINT.ja.md](FOOTPRINT.ja.md)、[OPTIMIZE.ja.md](OPTIMIZE.ja.md)、[FONT_FORMAT.ja.md](FONT_FORMAT.ja.md)、[TEST_PLAN.ja.md](TEST_PLAN.ja.md)、[DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md)、[EXTERNAL_REQUESTS.ja.md](EXTERNAL_REQUESTS.ja.md)、[MANUAL_TEST.ja.md](MANUAL_TEST.ja.md) |

利用者向け文書（README / GUIDE / API）は **API が固まってから**書く。今は内部文書だけ。

## 読む順

| やりたいこと | 読む文書 |
| --- | --- |
| **何を作るライブラリで、どこまでが責務なのか知る** | **[REQUIREMENTS.ja.md](REQUIREMENTS.ja.md)** |
| **API の形と内部構造を知る** | **[CORE_DESIGN.ja.md](CORE_DESIGN.ja.md)** |
| **なぜそう設計したのかを知る／論点を潰す** | **[DECISIONS.ja.md](DECISIONS.ja.md)** |
| フラッシュ・RAM の予算と実測値を見る | [FOOTPRINT.ja.md](FOOTPRINT.ja.md) |
| これから削る余地と、その実測値を見る | [OPTIMIZE.ja.md](OPTIMIZE.ja.md) |
| **フォントまわりの実測を知る**（形式そのものは外部仕様） | **[FONT_FORMAT.ja.md](FONT_FORMAT.ja.md)** |
| テストの方針とケース一覧を知る | [TEST_PLAN.ja.md](TEST_PLAN.ja.md) |
| 現在地と残作業、実装の順序を知る | [DEVELOPMENT_PLAN.ja.md](DEVELOPMENT_PLAN.ja.md) |
| **外部（コア・ツール）に何を頼む必要があるか知る** | **[EXTERNAL_REQUESTS.ja.md](EXTERNAL_REQUESTS.ja.md)** |
| **実機で何を確かめるか知る** | **[MANUAL_TEST.ja.md](MANUAL_TEST.ja.md)** |

## 出発点

`memo.ja.md`（コンセプト叩き台）。内容はこの docs へ移したので **2026-08-28 に削除した。**

memo から変えた点は [DECISIONS.ja.md](DECISIONS.ja.md) §3 にまとめてある。
