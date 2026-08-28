// TinyGFX - a small Arduino-first graphics library for SPI and I2C displays
//
// This is the entry point, and it carries the drawing core only. Bus and panel
// implementations, and every extra, are included separately:
//
//   #include <TinyGFX.h>
//   #include <TinyGFX/BusSPI.h>
//   #include <TinyGFX/PanelST7789.h>
//
// Whatever you do not include is not linked in either (docs/CORE_DESIGN.ja.md 7.4).
//
// The CellFont *types* are the one exception: they come in by default. A
// generated font header does not include the renderer's header, and stops with
// an #error if the types are missing (CellFont spec 12.2). Without this you
// would hit an ordering trap - "including the font before TinyGFX.h breaks the
// build". CellFont.h is nothing but structs and macros, so it costs zero bytes
// of code and zero bytes of data (measured across every construct).
// The decoder itself (TinyGFX/FontCell.h) is still opt-in.
#pragma once

#include "TinyGFX/CellFont.h"
#include "TinyGFX/Gfx.h"
#include "tinygfx_version.h"
