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
//
// **CellFont の型だけは既定で連れてくる。** 生成されたフォントヘッダは
// 描画器のヘッダを include せず、型が無ければ #error で止まる（仕様 §12.2）ので、
// これが無いと「TinyGFX.h より前にフォントを include したら死ぬ」順序の罠になる。
// 中身は構造体とマクロだけで**コードもデータも 1 バイトも生まない**（実測で確認）。
// デコーダ（TinyGFX/FontCell.h）のほうは今までどおり個別に include する。
#pragma once

#include "TinyGFX/CellFont.h"
#include "TinyGFX/Gfx.h"
#include "tinygfx_version.h"
