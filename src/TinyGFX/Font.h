// TinyGFX - the format-independent side of fonts
//
// The core knows nothing about font formats. The font does: it carries, next
// to its data, a pointer to the code that draws it (TinyGFXFontOps).
//
//   sketch ── #include <TinyGFX/FontCell.h> ──> the CellFont decoder
//          └─ #include <TinyGFX/FontU8g2.h> ──> the u8g2 decoder
//
// A decoder you do not include is referenced by nothing and therefore is not
// linked. Adding formats does not grow the footprint - an unused one is zero
// bytes.
//
// There are two levels of chaining, and both are needed because they do
// different jobs:
//
//   TinyGFXFontRef::next  across formats - Latin in CellFont, CJK in u8g2
//   CellFont::next        within one format - split by cell width class so
//                         each part stays fixed pitch
//
// Falling back to U+FFFD happens only at the outermost level, in
// TinyGFX::drawChar. Doing it inside a decoder means a notdef in the first
// format hides every glyph that a later format would have provided (CellFont
// spec 15.2). A decoder only ever reports found or not found.
#pragma once
#include <stddef.h>
#include <stdint.h>

class TinyGFX;

/// The entry point for one format. Everything returns -1 for "not covered".
struct TinyGFXFontOps {
  /// Draw one glyph and return its advance. `y` is the baseline.
  /// Returns -1 when the code is not covered, having drawn nothing.
  int16_t (*draw)(TinyGFX& gfx, const void* font, uint16_t ch, int16_t x, int16_t y);
  /// The advance only, without drawing. -1 when not covered.
  int16_t (*advance)(const void* font, uint16_t ch);
  /// Line advance, before the text size multiplier.
  uint8_t (*lineHeight)(const void* font);
  /// Baseline to the top of the line box, before the text size multiplier.
  /// The core uses this to turn a "line top" into a baseline, and it only ever
  /// asks the first font in the chain.
  int16_t (*ascent)(const void* font);
};

/// What a sketch hands to setFont(): font data paired with its entry point.
struct TinyGFXFontRef {
  const void* data;
  const TinyGFXFontOps* ops;
  /// Where to look next when this font has no such glyph; nullptr ends the
  /// chain. The next font may be in a different format.
  const TinyGFXFontRef* next;
};

// ---------------------------------------------------------------------------
// Reading font data (for formats other than CellFont)
//
// AVR keeps flash and data in separate address spaces, so anything in PROGMEM
// can only be read through pgm_read_*. On every other architecture these
// expand to a plain dereference and cost not a single instruction.
//
// Every last piece of font data belongs in PROGMEM. The decoders always read
// it through tinygfx_rd*, so leaving the attribute off any one of them
// produces garbage on AVR - pgm_read_* would be reading a RAM address out of
// program space. That is the generator's responsibility.
//
// TinyGFXFontRef and TinyGFXFontOps go the other way: plain const (RAM on
// AVR), read by plain dereference. Together they are 6-12 bytes per font, and
// in exchange the dispatch path stays cheap.
//
// CellFont has its own spec-mandated spelling of the same thing,
// CELLFONT_READ_*, which lives in CellFont.h and does not depend on TinyGFX.
// ---------------------------------------------------------------------------
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
