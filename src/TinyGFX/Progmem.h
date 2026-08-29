// TinyGFX - reading constants that live in program memory
//
// AVR keeps flash and RAM in separate address spaces, so anything in PROGMEM
// can only be read through pgm_read_*. On every other architecture these
// expand to a plain dereference and cost not a single instruction.
//
// Anything the sketch declares as constant graphics data - fonts, 1bpp
// bitmaps, images - belongs in PROGMEM and is read through here. Leaving the
// attribute off produces garbage on AVR, because pgm_read_* would be reading a
// RAM address out of program space (docs/DECISIONS.ja.md D19).
//
// This is its own header because both fonts and panels need it, and a panel
// has no business including the font machinery to get at three inline
// functions.
//
// CellFont has its own spec-mandated spelling of the same thing,
// CELLFONT_READ_*, which lives in CellFont.h and does not depend on TinyGFX.
#pragma once
#include <stddef.h>
#include <stdint.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
#define TINYGFX_FONT_PROGMEM PROGMEM
inline uint8_t tinygfx_rd8(const void* p) { return pgm_read_byte(p); }
inline uint16_t tinygfx_rd16(const void* p) { return pgm_read_word(p); }
inline const void* tinygfx_rdptr(const void* p) { return (const void*)pgm_read_ptr(p); }
#else
#define TINYGFX_FONT_PROGMEM
inline uint8_t tinygfx_rd8(const void* p) { return *(const uint8_t*)p; }
inline uint16_t tinygfx_rd16(const void* p) { return *(const uint16_t*)p; }
inline const void* tinygfx_rdptr(const void* p) { return *(const void* const*)p; }
#endif
