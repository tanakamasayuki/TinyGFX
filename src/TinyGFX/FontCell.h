// TinyGFX - the CellFont decoder
//
// The format itself is specified outside TinyGFX (LGFXFontToolJs,
// docs/formats/cellfont.ja.md). Only the renderer side lives here; the structs
// and accessors are in TinyGFX/CellFont.h.
//
// Include this before any generated font header. Generated headers do not
// include the renderer's header and stop with an #error when the types are
// missing (spec 12.2).
//
// Best for pixel-grid fonts 16 pixels tall or less. Above that it can still
// win on total size when the glyph count is small (spec 13.4; adding TinyGFX's
// second decoder measures 693 bytes).
//
// Three choices, each expressed by whether a pointer is null:
//   codes  == nullptr  contiguous index / non-null  sparse (head block + binary search)
//   glyphs == nullptr  fixed pitch      / non-null  variable pitch (4 bytes per glyph)
//   next   == nullptr  end of chain     / non-null  try the next font in this format
//
// Falling back to U+FFFD does not happen here (spec 15.2). TinyGFX has an
// outer, cross-format chain (TinyGFXFontRef::next), and falling back in here
// would hide every glyph a later format could have supplied. The fallback
// belongs to TinyGFX::drawChar, at the outermost level.
#pragma once
#include <stdint.h>

#include "CellFont.h"
#include "Font.h"
#include "Gfx.h"

#if CELLFONT_SPEC_VERSION != 1
#error "TinyGFX implements CellFont spec version 1"
#endif

// Everything the decoder can be told to leave out. All default to on, so the
// decoder handles any conforming font; switch off only what your font and your
// sketch demonstrably do not use. Bytes saved, measured on the same glyphs:
//
//                            AVR   CH32V003
//   TINYGFX_FONT_SPARSE      ---   ---        depends on the font, see below
//   TINYGFX_FONT_RECORDS     ---   ---
//   TINYGFX_FONT_BG          130   108
//   TINYGFX_FONT_SCALE        74   116
//   TINYGFX_FONT_CHAIN        34    16
//
// SPARSE and RECORDS must match the font. A generated header says which it is
// on its "Format :" line - "fixed/sparse", "variable/contiguous" and so on.
// Getting these wrong draws the wrong glyphs rather than failing to build,
// because the encoding is data, not something the compiler can see.
//
// BG and SCALE drop a feature of the *sketch*, not of the font: with them off
// the second argument of setTextColor() and any setTextSize() above 1 stop
// having an effect.
#ifndef TINYGFX_FONT_SPARSE
#define TINYGFX_FONT_SPARSE 1  // 0 drops the sparse index (code table, head block)
#endif
#ifndef TINYGFX_FONT_RECORDS
#define TINYGFX_FONT_RECORDS 1  // 0 drops variable pitch (the glyph table)
#endif
#ifndef TINYGFX_FONT_CHAIN
#define TINYGFX_FONT_CHAIN 1  // 0 drops CellFont::next (a font that is one link)
#endif
#ifndef TINYGFX_FONT_BG
#define TINYGFX_FONT_BG 1  // 0 drops the background cell behind each glyph
#endif
#ifndef TINYGFX_FONT_SCALE
#define TINYGFX_FONT_SCALE 1  // 0 drops the setTextSize() multiplier (fixes it at 1)
#endif

/// Walks a glyph's bit stream. Which way is smaller depends on the machine
/// (both measured, same glyphs):
///
///   - AVR has no barrel shifter and no 32-bit registers, so indexing with a
///     bit counter costs a shift by a variable amount on every pixel. Eating
///     the stream a byte at a time is 162 bytes smaller there.
///   - RISC-V shifts in one instruction and works in 32-bit registers, so the
///     counter is free and the extra per-pixel branch would cost 16 bytes on
///     the CH32V003 - the reference board.
struct CellBits {
  // A constructor rather than aggregate initialisation: the default member
  // initialisers below make this a non-aggregate in C++11, which is the
  // standard this library is held to (docs/REQUIREMENTS.ja.md 6.1).
  explicit CellBits(const uint8_t* p) : src(p) {}

  const uint8_t* src;
#if defined(__AVR__)
  uint8_t acc = 0;
  uint8_t left = 0;
  bool next() {
    if (left == 0) { acc = CELLFONT_READ_U8(src++); left = 8; }
    const bool b = (acc & 0x80) != 0;
    acc = (uint8_t)(acc << 1);
    --left;
    return b;
  }
#else
  uint32_t bit = 0;
  bool next() {
    const bool b = ((CELLFONT_READ_U8(&src[bit >> 3]) >> (7 - (bit & 7))) & 1) != 0;
    ++bit;
    return b;
  }
#endif
};

namespace tinygfx_cell {

/// Look one font up. Returns true when found. Spec 7.1.
///
/// Written with subtraction rather than addition so it survives a 16-bit
/// environment: `first + count` and `lo + hi` both wrap when int is 16 bits.
inline bool indexIn(const CellFont* f, uint16_t ch, uint16_t* outIndex) {
  const uint16_t first = CELLFONT_READ_U16(&f->first);
  const uint16_t count = CELLFONT_READ_U16(&f->count);
#if TINYGFX_FONT_SPARSE
  const uint16_t* codes = (const uint16_t*)CELLFONT_READ_PTR(&f->codes);
  if (codes != nullptr) {
    const uint8_t head = CELLFONT_READ_U8(&f->headCount);
    if (head != 0 && ch >= first) {  // head block: contiguous, absent from the code table
      const uint16_t rel = (uint16_t)(ch - first);
      if (rel < head) {
        *outIndex = rel;
        return true;
      }
    }
    // The tail. Codes smaller than `first` can appear here, so an early
    // `ch < first` bail-out would be wrong.
    uint16_t lo = 0;
    uint16_t hi = (uint16_t)(count - head);
    while (lo < hi) {
      const uint16_t mid = (uint16_t)(lo + ((hi - lo) >> 1));
      const uint16_t v = CELLFONT_READ_U16(&codes[mid]);
      if (v == ch) {
        *outIndex = (uint16_t)(head + mid);
        return true;
      }
      if (v < ch) lo = (uint16_t)(mid + 1);
      else hi = mid;
    }
    return false;
  }
#endif
  if (ch < first) return false;  // contiguous index
  const uint16_t rel = (uint16_t)(ch - first);
  if (rel >= count) return false;
  *outIndex = rel;
  return true;
}

/// Walk the chain within this format. Returns the font that has the glyph,
/// or nullptr.
inline const CellFont* findIn(const CellFont* f, uint16_t ch, uint16_t* outIndex) {
#if TINYGFX_FONT_CHAIN
  while (f != nullptr) {
    if (indexIn(f, ch, outIndex)) return f;
    f = (const CellFont*)CELLFONT_READ_PTR(&f->next);
  }
  return nullptr;
#else
  if (f != nullptr && indexIn(f, ch, outIndex)) return f;
  return nullptr;
#endif
}

inline int16_t advance(const void* font, uint16_t ch) {
  uint16_t idx = 0;
  const CellFont* f = findIn((const CellFont*)font, ch, &idx);
  if (f == nullptr) return -1;
#if TINYGFX_FONT_RECORDS
  const CellGlyph* gp = (const CellGlyph*)CELLFONT_READ_PTR(&f->glyphs);
  if (gp != nullptr) return (int16_t)CELLFONT_READ_U8(&gp[idx].xAdvance);
#endif
  return (int16_t)CELLFONT_READ_U8(&f->xAdvance);
}

/// Line advance. Spec 8 requires it to match across the chain, so the first
/// font's value will do.
inline uint8_t lineHeight(const void* font) {
  return CELLFONT_READ_U8(&((const CellFont*)font)->yAdvance);
}

/// Baseline to the top of the box. Spec 4: ascent = -yOffset.
inline int16_t ascent(const void* font) {
  return (int16_t)(-(int8_t)CELLFONT_READ_U8(&((const CellFont*)font)->yOffset));
}

/// Draw one glyph. `y` is the baseline. Returns the advance before the text
/// size multiplier, or -1 when the code is not covered.
inline int16_t draw(TinyGFX& g, const void* font, uint16_t ch, int16_t x, int16_t y) {
  uint16_t idx = 0;
  const CellFont* f = findIn((const CellFont*)font, ch, &idx);
  if (f == nullptr) return -1;

#if TINYGFX_FONT_SCALE
  const uint8_t sz = g.getTextSize();
#else
  const uint8_t sz = 1;
#endif
  uint32_t bmOffset;
  uint8_t gw;
  uint8_t adv;
#if TINYGFX_FONT_RECORDS
  const CellGlyph* gp = (const CellGlyph*)CELLFONT_READ_PTR(&f->glyphs);
  if (gp != nullptr) {  // variable pitch: look it up in the glyph table
    bmOffset = CELLFONT_READ_U16(&gp[idx].offset);
    gw = CELLFONT_READ_U8(&gp[idx].width);
    adv = CELLFONT_READ_U8(&gp[idx].xAdvance);
  } else
#endif
  {  // fixed pitch: no table. Compute in 32 bits (spec 15.2)
    bmOffset = (uint32_t)idx * (uint32_t)CELLFONT_READ_U8(&f->bytesPerGlyph);
    gw = CELLFONT_READ_U8(&f->width);
    adv = CELLFONT_READ_U8(&f->xAdvance);
  }

  g.startWrite();
#if TINYGFX_FONT_BG
  if (g.hasTextBg()) {
    // The line box comes from the chain head's metrics. Using each font's own
    // height would make the rows drift apart.
    const int16_t top = (int16_t)(y - (int16_t)(g.getTextAscent() * sz));
    g.fillRect(x, top, (int16_t)((uint16_t)adv * sz),
               (int16_t)((uint16_t)g.getTextLineHeight() * sz), g.getTextBgColor());
  }
#endif
  const uint8_t gh = CELLFONT_READ_U8(&f->height);
  const uint8_t* bm = (const uint8_t*)CELLFONT_READ_PTR(&f->bitmap);
  if (gw != 0 && gh != 0 && bm != nullptr) {
    const uint16_t fg = g.getTextColor();
    const uint8_t* src = bm + bmOffset;
    const int16_t gx = (int16_t)(x + (int16_t)((int8_t)CELLFONT_READ_U8(&f->xOffset) * sz));
    int16_t py = (int16_t)(y + (int16_t)((int8_t)CELLFONT_READ_U8(&f->yOffset) * sz));
    // Rows are concatenated into one MSB-first bit stream and do not realign
    // to byte boundaries, so one reader walks the whole glyph.
    CellBits bits(src);
    for (uint8_t r = 0; r < gh; ++r) {
      int16_t px = gx;
      uint8_t runStart = 0;
      // The first pixel of the row opens the run, so it is read out here
      // rather than costing a "is this the first column" test on every pixel.
      bool cur = bits.next();
      for (uint8_t c = 1; c < gw; ++c) {
        const bool on = bits.next();
        if (on != cur) {
          const int16_t runW = (int16_t)((uint16_t)(c - runStart) * sz);
          if (cur) g.fillRect(px, py, runW, sz, fg);
          px = (int16_t)(px + runW);
          runStart = c;
          cur = on;
        }
      }
      if (cur) g.fillRect(px, py, (int16_t)((uint16_t)(gw - runStart) * sz), sz, fg);
      py = (int16_t)(py + sz);
    }
  }
  g.endWrite();
  return (int16_t)adv;
}

}  // namespace tinygfx_cell

/// This format's entry point. A TinyGFXFontRef built around a generated font
/// header points here.
static const TinyGFXFontOps tinygfxFontCellOps = {
    &tinygfx_cell::draw,
    &tinygfx_cell::advance,
    &tinygfx_cell::lineHeight,
    &tinygfx_cell::ascent,
};
