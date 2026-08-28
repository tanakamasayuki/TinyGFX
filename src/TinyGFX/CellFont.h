// CellFont - bitmap font format, version 1
//
// The format itself - the contract between generator and renderer - is
// specified outside TinyGFX:
//   https://github.com/tanakamasayuki/LGFXFontToolJs  docs/formats/cellfont.ja.md
//
// This file carries only what spec 12.1 asks a renderer to provide. The spec
// fixes the macro and type names but deliberately leaves the file name open,
// so this lives under TinyGFX/ rather than occupying the global include
// namespace.
//
// Include this before any generated font header. Generated headers do not
// include the renderer's header themselves; they stop with an #error when
// CELLFONT_SPEC_VERSION is undefined (spec 12.2). In practice TinyGFX.h and
// TinyGFX/FontCell.h both bring it in for you.
//
// Nothing here depends on TinyGFX. Structs and accessors only - it generates
// no code at all.
//
// The whole file is guarded on CELLFONT_SPEC_VERSION so that it can coexist
// with another library implementing the same spec: whichever is included first
// wins, and a generated header reads correctly against either, because the
// spec makes the structs identical.
#ifndef CELLFONT_SPEC_VERSION

/// The spec version this implementation follows. Generated headers check it.
#define CELLFONT_SPEC_VERSION 1

#include <stddef.h>
#include <stdint.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
/// Attribute for font data. Needed on all four of bitmap, glyph table, code
/// table and the font struct. Miss any one of them and the accessors below
/// read a RAM address out of program space, and the text renders as garbage.
#define CELLFONT_PROGMEM PROGMEM
#define CELLFONT_READ_U8(p) pgm_read_byte(p)
#define CELLFONT_READ_U16(p) pgm_read_word(p)
#define CELLFONT_READ_PTR(p) ((const void*)pgm_read_ptr(p))
#else
#define CELLFONT_PROGMEM
#define CELLFONT_READ_U8(p) (*(const uint8_t*)(p))
#define CELLFONT_READ_U16(p) (*(const uint16_t*)(p))
#define CELLFONT_READ_PTR(p) (*(const void* const*)(p))
#endif

/// One glyph record, used by variable-pitch fonts. Exactly 4 bytes (spec 3).
/// Height, xOffset and yOffset are shared by every glyph, so they are not here.
typedef struct CellGlyph {
  uint16_t offset;   ///< byte offset from the start of the bitmap
  uint8_t width;     ///< glyph width in pixels; 0 is legal
  uint8_t xAdvance;  ///< advance in pixels
} CellGlyph;

/// One font. The field order must match spec 3 exactly, because generated
/// headers fill this in with positional initialisers.
typedef struct CellFont {
  const uint8_t* bitmap;        ///< the glyph bit stream
  const CellGlyph* glyphs;      ///< NULL means fixed pitch
  const uint16_t* codes;        ///< NULL means a contiguous index; length is count - headCount
  const struct CellFont* next;  ///< NULL ends the chain
  uint16_t first;               ///< first code of the contiguous index, or of the head block
  uint16_t count;               ///< number of glyphs
  uint8_t width, height;        ///< width is for fixed pitch; height is shared by every glyph
  uint8_t xAdvance, yAdvance;   ///< xAdvance is for fixed pitch; yAdvance is the line advance
  int8_t xOffset, yOffset;      ///< shared by every glyph; yOffset is usually negative
  uint8_t bytesPerGlyph;        ///< fixed pitch only, so nothing divides at run time
  uint8_t headCount;            ///< head block length of a sparse index (>= 1); 0 when contiguous
} CellFont;

#if defined(__cplusplus)
// Spec 3: the size is ABI dependent, so check it against the target ABI.
// CellGlyph in particular must be 4 bytes - the glyph table stride assumes it.
static_assert(sizeof(CellGlyph) == 4, "CellGlyph must be 4 bytes");
#endif

#endif  // CELLFONT_SPEC_VERSION
