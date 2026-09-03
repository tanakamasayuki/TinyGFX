# デバイスの実装ルール — 模型の作り方

内部の記録。日本語のみ。読者は**デバイスの模型を書く人と、profile から模型を
生成する生成器**。概念は [CONCEPTS.ja.md](CONCEPTS.ja.md) が正。

## 1. デバイスから見える世界 —— 3 つしかない

**デバイスは環境もイベントも割り込みも知らない。** 見えるのは:

1. **バス面から来る呼び出し**（下の ops を実装する）
2. **`EmuDeviceEnv` の 3 つの道具**（attach 時に渡される。世界への唯一の出口）
3. **自分の状態**（呼び出し側が確保して渡したメモリ）

```c
typedef struct {
  void     (*drive_line)(EmuCtx*, EmuLine, uint8_t level);   // INT を引く 等
  void     (*uart_send) (EmuCtx*, uint8_t port, const uint8_t*, size_t);
  uint64_t (*now_us)    (EmuCtx*);                           // busy の表現用
  EmuCtx*  ctx;
} EmuDeviceEnv;
```

`drive_line` は**線のレベルを変えるだけ**。割り込みが起きるかは関知しない
（[CONCEPTS.ja.md](CONCEPTS.ja.md) §4.7 —— 割り込みは経路ではなく帰結）。

## 2. 実装する ops（4 形のうち自分のもの）

```c
// I2C: トランザクション単位。0 = ACK、2 = 不在
typedef struct {
  uint8_t (*write)(void* dev, const uint8_t*, size_t, bool stop);
  size_t  (*read) (void* dev, uint8_t*, size_t, bool stop);
  void    (*reset)(void* dev);               // RESET 線・電源投入相当（任意）
} EmuI2cDeviceOps;

// SPI: バイト単位・全二重。役割線（DC 等）はレベルで通知される
typedef struct {
  void    (*select)(void* dev, bool asserted);
  uint8_t (*transfer)(void* dev, uint8_t out);         // MISO（無応答は 0xFF）
  void    (*role)(void* dev, uint8_t roleId, uint8_t level);   // 任意
  void    (*reset)(void* dev);
} EmuSpiDeviceOps;

// UART: アプリの送信を受ける。返信は EmuDeviceEnv.uart_send
typedef struct {
  void (*tx)(void* dev, const uint8_t*, size_t);
} EmuUartDeviceOps;

// レベル型: 線の変化を受ける
typedef struct {
  void (*line)(void* dev, EmuLine, uint8_t level);
} EmuPinDeviceOps;
```

## 3. 中身の分割 —— 枠組みと解釈器を分ける

**ops を実装するのは枠組み（薄い皮）で、本体は解釈器**
（[CONCEPTS.ja.md](CONCEPTS.ja.md) §4.5）。SSD1306 なら I2C 枠組み
（制御バイト 0x00/0x40）と SPI 枠組み（DC 役割）が**同じ解釈器**を呼ぶ。

| 解釈器の型 | 書くもの | 例 |
| --- | --- | --- |
| **register-map 型** | **表だけ**（番地・リセット値・副作用の小さな callback）。データ柱の profile から生成できる形 | EEPROM・PMIC・QMP6988 |
| **command 型** | 状態機械（ここが本体） | SSD1306・SHT30 |

## 4. 物理面 —— 「走行中の変化」の正しい入り口

測る量・動く量は名前つきチャネルとして宣言する。台本（`emu_at_phys`）の宛先に
なり、変化は刺激としてイベントに写る。

```c
uint8_t emu_phys_channel(EmuCtx*, void* dev, const char* name,
                         void (*set)(void* dev, int32_t value));
```

温度・照度・押下（入）だけでなく、動作量（出: モータ速度）も物理面。
出は読み出し（§5）として公開する。

## 5. 検分と仕込みの口 —— 公開はデバイス作者が決める

利用者は**ハンドルを直接**使う（環境は中継しない）。作法だけ守る:

| 口 | 規則 |
| --- | --- |
| 検分（get・RAM 読み・表示窓） | **状態を変えない・イベントも出さない**（const）。見ただけで golden が壊れない |
| 仕込み（set・preload） | 走行前は自由。**走行中の変化は物理面 + 台本で**（裏からレジスタを書かせない） |

register-map 型は `get(reg)` / `set(reg, value)` が自動で揃う。command 型は
型付きの口を自分で切る（表示なら RAM 全体と表示窓の 2 口 —— SH1106 の 132 列
RAM に 128 列の窓、のようにオフセットのずれが絵で見える形に）。

## 6. 新しいデバイスを作るチェックリスト

| # | 準備するもの | 量 |
| --- | --- | --- |
| 1 | 状態の構造体 | 定義だけ（確保は利用者） |
| 2 | 解釈器 | register-map 型なら表、command 型なら状態機械 |
| 3 | 枠組み | 既製から選ぶ。新しい線上の作法のときだけ自作 |
| 4 | IO 面の宣言 | 持っている役割（INT / RESET / DC…） |
| 5 | 物理面（あれば） | 名前つきチャネル |
| 6 | 検分・仕込みの型付き口 | §5 の作法で |
| 7 | 適合検査 | Protocol 同梱のデバイス適合キット + scenario か golden を 1 つ |

## 7. 禁止事項（規約の適用）

- `<Arduino.h>` もどのフレームワークのヘッダも含めない。動的確保・例外・RTTI・
  I/O をしない（MCU に載る前提 —— U16）
- `EmuDeviceEnv` 以外の道で世界に触れない
- デバイス種の列挙に自分を足す必要があったら、それは契約側の設計ミス（報告する）
