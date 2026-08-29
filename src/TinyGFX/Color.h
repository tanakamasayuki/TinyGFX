// TinyGFX - color helpers (RGB565)
#pragma once
#include <stdint.h>

/// RGB888 -> RGB565. constexpr, so it folds at compile time and leaves no code.
constexpr uint16_t tinygfx_color565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (uint16_t)(b >> 3));
}

/// Swap the byte order of an RGB565 buffer, in place.
///
/// TinyGFX has no setSwapBytes(), and this is what stands in its place.
///
/// The byte order on the wire is the Bus implementation's business, and
/// everything TinyGFX itself produces already agrees with it - the converter
/// decides the order when it writes the header, so an image from the tool
/// never needs this. What does need it is a **foreign producer**: a JPEG or PNG
/// decoder, or a camera, handing you RGB565 with the bytes the other way round.
///
///     TJpg_Decoder cb:  tinygfx_swapBytes565(block, w * h);
///                       lcd.pushImage(x, y, w, h, block);
///
/// A runtime mode was measured instead and cost **44 bytes and 4 bytes of RAM
/// on a CH32V003 to every sketch, including every sketch that never swaps
/// anything** - a branch per pixel in the one loop that must not have one.
/// This costs nothing until it is called (docs/DECISIONS.ja.md D29).
inline void tinygfx_swapBytes565(uint16_t* data, uint32_t count) {
  while (count--) {
    const uint16_t c = *data;
    *data++ = (uint16_t)((c >> 8) | (c << 8));
  }
}

// Colour constants, under two spellings.
//
// TFT_* is the spelling TFT_eSPI and LovyanGFX use, so code moving over does
// not have to be rewritten - and it is guarded, so whichever library is
// included first wins and the two can share a sketch.
//
// That guard is exactly why the TINYGFX_* spelling exists too: when another
// library got there first, TFT_RED is *their* TFT_RED. The values agree today
// across every library that uses these names, but "agrees today" is not
// something to build on, and a sketch that needs to be sure has a name that
// cannot be taken. Both are macros, so having both costs nothing.
#define TINYGFX_BLACK     0x0000
#define TINYGFX_NAVY      0x000F
#define TINYGFX_DARKGREEN 0x03E0
#define TINYGFX_MAROON    0x7800
#define TINYGFX_PURPLE    0x780F
#define TINYGFX_OLIVE     0x7BE0
#define TINYGFX_DARKGREY  0x7BEF
#define TINYGFX_BLUE      0x001F
#define TINYGFX_GREEN     0x07E0
#define TINYGFX_CYAN      0x07FF
#define TINYGFX_RED       0xF800
#define TINYGFX_MAGENTA   0xF81F
#define TINYGFX_YELLOW    0xFFE0
#define TINYGFX_WHITE     0xFFFF
#define TINYGFX_ORANGE    0xFDA0
#define TINYGFX_PINK      0xFE19

#ifndef TFT_BLACK
#define TFT_BLACK     0x0000
#endif
#ifndef TFT_NAVY
#define TFT_NAVY      0x000F
#endif
#ifndef TFT_DARKGREEN
#define TFT_DARKGREEN 0x03E0
#endif
#ifndef TFT_MAROON
#define TFT_MAROON    0x7800
#endif
#ifndef TFT_PURPLE
#define TFT_PURPLE    0x780F
#endif
#ifndef TFT_OLIVE
#define TFT_OLIVE     0x7BE0
#endif
#ifndef TFT_DARKGREY
#define TFT_DARKGREY  0x7BEF
#endif
#ifndef TFT_BLUE
#define TFT_BLUE      0x001F
#endif
#ifndef TFT_GREEN
#define TFT_GREEN     0x07E0
#endif
#ifndef TFT_CYAN
#define TFT_CYAN      0x07FF
#endif
#ifndef TFT_RED
#define TFT_RED       0xF800
#endif
#ifndef TFT_MAGENTA
#define TFT_MAGENTA   0xF81F
#endif
#ifndef TFT_YELLOW
#define TFT_YELLOW    0xFFE0
#endif
#ifndef TFT_WHITE
#define TFT_WHITE     0xFFFF
#endif
#ifndef TFT_ORANGE
#define TFT_ORANGE    0xFDA0
#endif
#ifndef TFT_PINK
#define TFT_PINK      0xFE19
#endif
