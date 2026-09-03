# 環境の実装ルール — フレームワーク × 実行形態のバインディング

内部の記録。日本語のみ。読者は**環境（host-arduino-core のような実装）を
契約に繋ぐ人**。概念は [CONCEPTS.ja.md](CONCEPTS.ja.md) が正。

## 1. 適合条件 —— これを満たせば「対応環境」

| # | 義務 | 系統 |
| --- | --- | --- |
| E1 | **完全なタップ 1 本**: 自分に見える操作すべてを、契約の関数（§3）に流す。写らない操作を作らない | B |
| E2 | **応答の委譲**: 応答が要る箇所（I2C の状態・読みデータ、SPI の MISO、UART の受信）で自前の値を作らず、契約の戻り値を使う | A |
| E3 | **刺激の受け口**: 注入（線のレベル・受信データ）を自分の世界に反映する | A |
| E4 | **時計の提供**: `millis`/`delay` 相当を時計 IF 経由にする | S |

**部分実装は正当。** E1 だけなら「解析専用環境」（実機タップがそれ）。
実装した範囲を `emu_bind()` の能力宣言で申告し、テスト側の skip 判断に使わせる。

**実際の上限も環境が規定してよい**（観測できる線の数・バス数・リスナー数。
CONCEPTS §5-11）。義務は 2 つ —— 能力宣言に含めて見えるようにすること、
超過は黙らず失敗を返すこと。

## 2. 層の役割 —— ①は薄く、②に意味論を集める

環境は 2 枚で作る（[CONCEPTS.ja.md](CONCEPTS.ja.md) §4.9）。

| 枚 | 持つもの | 持たないもの |
| --- | --- | --- |
| ① フレームワーク実装 | **タップの場所**と機構（ピンを変える・ISR を走らせる・受信バッファに積む） | 意味論の判断 |
| ② フレームワーク中間層 | **フレームワークの意味論**（Arduino なら: mode 値の解釈、Wire の作法、CS がただの GPIO であること、`Wire`=バス 0 の写像） | 契約より下の知識 |

**EmuLine の番号の意味は環境（中間層）が宣言する。** 型（幅）は契約が固定して
いるので型は必ず通る。Arduino 中間層はピン番号、ESP-IDF 中間層は gpio_num を
そのまま写すのが自然。宣言は `emu_bind` の情報に含める。

②は**両面を持つ翻訳器**で、同じフレームワークの別実装から使い回せる共有
ライブラリにする。上面はフレームワークの言葉のまま
（`onPinMode(pin, mode)`・`onWireTransaction(...) → status`）、下面が契約。

**①と②の間、②と③（契約）の間に層を足さない。**

## 3. 環境が呼ぶ関数（契約 v0 案）

接頭辞 `emu_` は仮。第 1 引数は必ず世界 `EmuCtx*`。**記録・振り分け・応答・
刺激の発火はぜんぶこの中**で起きるので、環境側に判断は残らない。

```c
// GPIO（線層）
void    emu_gpio_mode  (EmuCtx*, EmuLine, uint8_t dir, uint8_t pull);
void    emu_gpio_output(EmuCtx*, EmuLine, uint8_t level);   // CS 追跡もここ
uint8_t emu_gpio_input (EmuCtx*, EmuLine);                  // 結果込みで記録

// 解釈層（ハードウェア周辺のタップ。線イベントは捏造しない）
uint8_t emu_i2c_txn   (EmuCtx*, uint8_t bus, uint16_t addr,
                       const uint8_t* w, size_t wn,
                       uint8_t* r, size_t rn, bool stop);   // 状態コードを返す
void    emu_spi_config(EmuCtx*, uint8_t bus, uint32_t hz, uint8_t mode);
uint8_t emu_spi_xfer  (EmuCtx*, uint8_t bus, uint8_t out);  // CS は線の状態から
void    emu_uart_tx   (EmuCtx*, uint8_t port, const uint8_t*, size_t);
size_t  emu_uart_rx   (EmuCtx*, uint8_t port, uint8_t*, size_t);
void    emu_pwm       (EmuCtx*, EmuLine, uint32_t hz, uint32_t duty, uint8_t bits);
void    emu_dac       (EmuCtx*, EmuLine, uint16_t value);   // アナログ出力（電圧値）
uint16_t emu_adc      (EmuCtx*, EmuLine);                   // アナログ入力。物理面から解決

// 時間と割り込み（安全点の運転）
void emu_wait       (EmuCtx*, uint64_t dur_us);   // DES: 予定を発火しつつ進める
void emu_int_attach (EmuCtx*, EmuLine, uint8_t edge, uint32_t token);
void emu_int_detach (EmuCtx*, EmuLine);
bool emu_int_pending(EmuCtx*, uint32_t* token);   // 安全点で引いて ISR を走らせる

// 自己申告
void emu_bind(EmuCtx*, const EmuEnvInfo*);        // 能力・時計モード・名前
```

## 4. 割り込みの運転 —— 同期・決定的な環境の宣言

契約は ISR を呼ばない。**環境が安全点で `emu_int_pending` を引いて走らせる。**
host のような同期環境の宣言はこう置く:

- 発火は **API 呼び出しの切れ目**と**時間が進む点**。API 呼び出しはアトミック
- 計算だけのループには割り込めない（実機との意図的な差）
- `noInterrupts()` 中は保留、解除点で発火
- ハンドラ中の API 呼び出しも普通にイベントになる（文脈タグ = 割り込み中）
- 最初は**ネスト無し・FIFO**

## 5. 時計

- 既定は**仮想**（`delay` は実に眠らず `emu_wait` に渡す。速くなる）
- 実時間モードはソケット等が要るときの opt-in
- 再生環境は記録の時刻を差し出す

## 6. host-arduino-core への適用（Arduino ポート第 1 号）

| いまあるもの | どうなる |
| --- | --- |
| 1 スロットのフック群（`setWriteHook` 等） | **廃止**（残すなら新しい口の上の薄い別名） |
| 各 API の実装 | ②の上面関数を呼ぶだけに（判断を持たない） |
| `millis()` = steady_clock、`delay` = 実スリープ | 時計 IF 経由・既定仮想へ |
| `setPinValue` 等の注入 | E3 の受け口として整理。**イベントに写る** |
