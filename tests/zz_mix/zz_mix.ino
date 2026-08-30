// Different drivers must still build together.
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/panels/SSD1306_128x64.h>
#include <TinyGFX/panels/ST7789_240x240.h>
TinyGFXBusI2C i2c(Wire, 0x3C);
TinyGFXBusSoftSPI spi(5, 6, 3, 4);
static uint8_t fb[TinyGFXPanelSSD1306_128x64::kBufferBytes];
TinyGFXPanelSSD1306_128x64 oled(i2c, fb);
TinyGFXPanelST7789_240x240 tft(spi, 2);
void setup() { oled.init(); tft.init(); }
void loop() {}
