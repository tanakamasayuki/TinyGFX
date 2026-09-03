# 利用者のルール — テストの書き方

内部の記録。日本語のみ。読者は**テストを書く人**。概念は
[CONCEPTS.ja.md](CONCEPTS.ja.md) が正。

## 1. 全体の形 —— 経路は 2 本だけ

```cpp
#include <Wire.h>
#include <emu/RegFile.h>                // カタログの模型

static EmuRegFileState axpState;        // 1. 状態は自分で確保
EmuRegFile axp(axpState);

void setup() {
  axp.set(0x03, 0x4A);                  // 2. 仕込み（走行前・直接・型付き）
  emuAttachI2c(0, 0x34, axp);           // 3. 登録（契約の統一 API）

  Wire.begin();                         // 4. あとは普通のフレームワーク API
  Wire.beginTransmission(0x34);         //    アプリはデバイスの存在を知らない
  Wire.write(0x03);
  Wire.endTransmission(false);
  Wire.requestFrom(0x34, 1);            //    ← 裏で axp が答える
  uint8_t id = Wire.read();             //    0x4A

  // 5. 検分（直接・const・イベントに写らない）
  //    assert(axp.get(0x03) == 0x4A);
}
```

```
アプリ:  Wire.requestFrom(0x34)
           → ① フレームワーク実装 → ② 中間層 → ③ 契約（記録 + 表引き）→ デバイス
テスト:  axp.get() / emuAttachI2c()  ── 手元のハンドルで直接。①②③を通らない
```

## 2. 登録（attach / detach）

```c
bool emu_attach_i2c (EmuCtx*, uint8_t bus, uint16_t addr, const EmuI2cDeviceOps*, void* dev);
bool emu_attach_spi (EmuCtx*, uint8_t bus, EmuLine cs, const EmuSpiRoles*, const EmuSpiDeviceOps*, void* dev);
bool emu_attach_uart(EmuCtx*, uint8_t port,               const EmuUartDeviceOps*, void* dev);
bool emu_attach_pins(EmuCtx*, const EmuLine*, uint8_t n,  const EmuPinDeviceOps*, void* dev);
bool emu_detach     (EmuCtx*, void* dev);
```

- **統一 API。** 適合するどの環境でも同じ行が動く
- バスは宣言不要（番号は鍵）。**上限は 2 段**（CONCEPTS §5-11）: IF 上の上限は
  型幅（バス番号 uint8_t = 255 まで、アドレスは 10 ビット I2C が入る幅）。
  **実際の上限は環境と ctx が規定**する —— 表の容量は ctx 初期化時に渡し、
  超過した attach は失敗が返る
- 番号の写像はフレームワーク中間層の規約（Arduino: `Wire`=0、`Wire1`=1）
- **I2C の attach にピンは出ない。** 線が出るのは SPI の CS だけ（バス内の宛先が
  線しか無いため）
- **attach は束縛表でもある。** デバイスの役割番号（0 始まり・全環境共通）と
  世界の線（この環境の番号）の対応はここで渡す —— `attach_pins` の線配列は
  **添字 = デバイスの役割番号**、SPI の `EmuSpiRoles` は DC などの役割 → 線。
  「デバイスの PWM1 はこの GPIO」もこの形
- 線番号を直接使う台本（`emu_at_line`）は**この環境・この板に束縛される**
  （実物のピン番号と同じ）。可搬にしたい台本は物理チャネル（`emu_at_phys`）を使う
- 同じバスに複数デバイス可。一意なのは宛先。**占有済みへの attach は失敗が返る**
  （黙って置き換えない）。同じ実体を複数宛先に付けるのは可
- テスト間の掃除は `emu_detach`

## 3. 観測（B 系統）

```c
EmuListenerId emu_listen  (EmuCtx*, uint32_t domainMask,
                           void (*fn)(const EmuEvent*, void*), void* user);
void          emu_unlisten(EmuCtx*, EmuListenerId);
size_t emu_event_write(const EmuEvent*, char* out, size_t cap);   // 直列化
bool   emu_event_read (const char* line, EmuEvent* out);
```

- 何個でも重ねられる（ログ + golden + カウンタ同時）
- **読み取り専用**。応答は変えられない
- 直列化はトレース = キャプチャ = golden の 1 形式

## 4. 刺激（台本）

```c
void emu_at_line(EmuCtx*, uint64_t t_us, EmuLine, uint8_t level);       // ボタン
void emu_at_rx  (EmuCtx*, uint64_t t_us, uint8_t port, const uint8_t*, size_t);  // 受信
void emu_at_phys(EmuCtx*, uint64_t t_us, void* dev, uint8_t channel, int32_t v); // 物理量
```

- 時刻順に発火し、**必ずイベントに写る**
- **発火させるのは時計であって、アプリではない。** アプリは台本の存在を知らない。
  仮想時計はアプリが待った瞬間に「次の予定」まで跳ぶので（CONCEPTS §4.6）、
  発火はアプリの実行と決定的に織り合わさる —— アプリが関与するのは
  「待つことで時間を進める」ことだけ
- 時計は既定で仮想: `delay` だらけのアプリも一瞬で回る

## 5. 検分と仕込みの規則

| 時期 | 口 | 規則 |
| --- | --- | --- |
| **走行前** | ハンドル直接（型付き） | 自由。プリセット・初期レジスタ・画像 preload |
| **走行中の変化** | **物理面 + 台本** | 裏からレジスタを書かない。「温度が変わった」は世界の変化 → 刺激 |
| **検分** | ハンドル直接（const） | いつでも可。状態も golden も壊さない |

遠隔（別基板でデバイスが動く構成）ではハンドルが届かないが、それは
スレイブポートの都合で、仕込みチャネルはポートが持つ。

## 6. skip の作法

環境は能力を宣言している（`emu_caps`）。テストは要る能力を先に確かめ、
足りなければ skip する（arduino-cli 不在 skip と同じ流儀）。
