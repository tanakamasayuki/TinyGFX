// TinyGFX - HardwareSPI
//
// ハードウェア SPI を使う版。バスの宣言が変わるだけで、描画側は何も変わらない。
//
// 注意: SCK / MOSI は指定しない。Arduino Core の SPI に任せる
// （ピンを取らない SPI.begin() のコアがあるため）。
// ピンを指定したい環境では、lcd.begin() より前に自分で SPI.begin(...) を呼び、
// コンストラクタの initSpi に false を渡すこと。
//
// CH32V003 では現状ハードウェア SPI が使えないコアがある。その場合は
// BusSoftSPI（HelloWorld の例）を使う。
#include <TinyGFX.h>
#include <TinyGFX/BusSPI.h>
#include <TinyGFX/PanelST7789.h>

TinyGFXBusSPI bus(/*dc*/3, /*cs*/4, /*freq*/24000000UL);
TinyGFXPanelST7789 panel(bus, 240, 240, /*rst*/2);
TinyGFX lcd(panel);

void setup() {
  lcd.begin();
  lcd.fillScreen(TFT_BLACK);

  // 連続して描くときは startWrite / endWrite で囲むと CS のトグルが減る
  lcd.startWrite();
  for (int16_t i = 0; i < 120; i += 8) {
    lcd.drawRect(i, i, 240 - i * 2, 240 - i * 2, lcd.color565(i, 255 - i, 128));
  }
  lcd.endWrite();
}

void loop() {}
