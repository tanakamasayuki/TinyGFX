# 環境の実装ルール — 汎用拡張点と中間層

内部の記録。日本語のみ。2026-09-03 改訂。概念は [CONCEPTS.ja.md](CONCEPTS.ja.md) が正。

環境は 2 枚で作る。**①は Protocol を知らない。**

| 枚 | 持つもの | 実物 |
| --- | --- | --- |
| ① フレームワーク実装の**汎用拡張点** | ライフサイクル・時計口・タップ・注入の機構 | **host-arduino-core 1.7.0（E22）** |
| ② フレームワーク**中間層**（共有ライブラリ） | フレームワークの意味論 ⇄ 契約の翻訳。tick の運転 | **未実装（段階 2）** |

## 1. ① の実物 —— host-arduino-core 1.7.0 の拡張点

これが「①に必要な汎用拡張点」の参照実装。別のフレームワーク実装（ESP-IDF の
linux ターゲット等）にも同じ性質を求める。

| 拡張点 | 中身 |
| --- | --- |
| ライフサイクル | `kPreSetup` / `kPostSetup`（各 1 回）、`kPreLoop` / `kPostLoop`（毎周）。1 本のフック + phase 引数。**`kPostLoop` は `runtimePoll()` の前**（外部入力は次の周に属す） |
| 時計口 | `ClockNowHook`（µs）+ `ClockWaitHook`（1 スライス待つ）。`millis`/`micros`/`delay`/`delayMicroseconds`/`yield` と `Stream` のタイムアウトが全部通る。**delay のループ本体・`runtimePoll`・停止チェックはコアに残る**（上書き側が忘れられない）。`clockRealNowMicros()` で実時計も読める |
| タップ | 既存の観測フック続投（GPIO・SPI・Wire・アナログ。単スロット・Arduino 語彙・応答兼用のまま —— 分離と多重化は②以上の仕事） |
| 注入 | `setPinValue` / `setAnalogValue` / `Serial1.pushRx` など |
| デバイス UART | `Serial1`/`Serial2`（オンメモリ両方向キュー、`readTx`/`pushRx`。`HardwareSerial` 非互換 = 既知の制限）。コンソールの `Serial` は別物 |
| 境界 | 実時間のまま残るタイムアウトは README「Timeouts that stay on real time」に全一覧 |

**ハングの条件は「期限の時計と待ちの時計の不一致」**。`steady_clock` の grep は
必要条件ですらない（`condition_variable::wait_for` は写らない）—— 環境適合の
検査観点として持つ。

## 2. ② 中間層の義務（段階 2 で実装）

| 義務 | 使う① |
| --- | --- |
| tick の運転: kPreLoop で ①進行役 ②保留 ISR、kPostLoop で歩進。wait スライスでも同じ（= 待ちの中でも世界が動く） | ライフサイクル + 時計口 |
| 仮想時計（既定）。実時間・倍率は opt-in | 時計口 |
| タップの語彙翻訳（Arduino → ハードウェアの言葉）とイベント発行・応答委譲 | タップ |
| 注入の反映（線・受信）と ISR の保留管理・発火 | 注入 + `attachInterrupt` の追跡 |
| `EmuLine` の番号の意味の宣言（Arduino 中間層 = ピン番号、`Wire`=バス 0 等） | — |
| weak 糖衣（setup 相当・進行役の関数）とビジーウェイト対策 opt-in | — |

実証: `tests/runtime/accept_emu_tick`（1.7.0 同梱）が、この義務の最小形を
スケッチ内ミニ中間層として演じて通っている。

## 3. 適合条件

| # | 義務 | 系統 |
| --- | --- | --- |
| E1 | 完全なタップ 1 本（見える操作すべてをイベントに） | B |
| E2 | 応答の委譲（自前の値を作らない） | A |
| E3 | 注入の受け口（線・受信・割り込みの帰結） | A |
| E4 | 時計の提供（now + 待ちの委譲） | S |

部分実装は正当（E1 だけ = 解析専用 = プローブ）。実装範囲と実際の上限
（線数・表の容量）は能力として申告し、超過は黙らず失敗を返す。
