// TinyGFX - small Arduino-first graphics library for SPI displays
//
// エントリポイント。ここには描画コアだけが入る。
// Bus / Panel の実装と拡張機能は個別に include する:
//
//   #include <TinyGFX.h>
//   #include <TinyGFX/BusSPI.h>
//   #include <TinyGFX/PanelST7789.h>
//
// include していないものはリンクもされない（docs/CORE_DESIGN.ja.md §7.4）。
#pragma once

#include "TinyGFX/Gfx.h"
#include "tinygfx_version.h"
