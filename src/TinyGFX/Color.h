// TinyGFX - color helpers (RGB565)
#pragma once
#include <stdint.h>

/// RGB888 -> RGB565. constexpr: 定数畳み込みされ、コードは残らない。
constexpr uint16_t tinygfx_color565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (uint16_t)(b >> 3));
}

// 既存ライブラリと同居できるよう、定義済みなら尊重する。マクロなのでコストは 0。
#ifndef TFT_BLACK
#define TFT_BLACK   0x0000
#define TFT_NAVY    0x000F
#define TFT_DARKGREEN 0x03E0
#define TFT_MAROON  0x7800
#define TFT_PURPLE  0x780F
#define TFT_OLIVE   0x7BE0
#define TFT_DARKGREY 0x7BEF
#define TFT_BLUE    0x001F
#define TFT_GREEN   0x07E0
#define TFT_CYAN    0x07FF
#define TFT_RED     0xF800
#define TFT_MAGENTA 0xF81F
#define TFT_YELLOW  0xFFE0
#define TFT_WHITE   0xFFFF
#define TFT_ORANGE  0xFDA0
#define TFT_PINK    0xFE19
#endif
