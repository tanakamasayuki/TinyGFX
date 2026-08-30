// **However a picture is encoded, not one pixel may differ.**
//
// The converter (tools/img2h.py) brute-forces the smallest format for each
// picture. **Which one it picked must not be visible from the sketch**, and
// that is what this pins down - the same idea as "three encodings draw the same
// pixels" for CellFont (tests/text/).
//
// Vertical and horizontal packing likewise: the data is laid out completely
// differently, and the picture must be the same.
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/DriverST7789.h>
#include <TinyGFX/Image.h>
#include <tgfx_test.h>
#include "same_raw565.h"
#include "same_rle565.h"
#include "same_rlepal4.h"
#include "mono_h.h"
#include "mono_v.h"
#include "trans_icon.h"
#include "photo64.h"

static const int W = 32, H = 32;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXDriverST7789 panel(bus, W, H);
TinyGFX lcd(panel);

static uint16_t ref[W * H];
static void snap() { for (int i = 0; i < W * H; ++i) ref[i] = gram[i]; }
static long diff() {
  long d = 0;
  for (int i = 0; i < W * H; ++i) { if (gram[i] != ref[i]) ++d; }
  return d;
}
static long lit() {
  long n = 0;
  for (int i = 0; i < W * H; ++i) { if (gram[i] != 0) ++n; }
  return n;
}
static void reset() { bus.fill(0); bus.resetCounters(); }

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("image_fmt");
  lcd.begin();

  // --- one picture, three formats -------------------------------------------
  reset(); lcd.drawImage(&same_raw565Ref, 0, 0);
  snap();
  tgfxReport("raw_lit", lit());
  tgfxReport("raw_pixels", (long)bus.pixelCount());
  tgfxShot("raw565", gram, W, H);

  reset(); lcd.drawImage(&same_rle565Ref, 0, 0);
  tgfxReport("rle565_diff", diff());
  tgfxShot("rle565", gram, W, H);

  reset(); lcd.drawImage(&same_rlepal4Ref, 0, 0);
  tgfxReport("rlepal4_diff", diff());
  tgfxShot("rlepal4", gram, W, H);

  // --- 1bpp packed horizontally and vertically ------------------------------
  reset(); lcd.drawImage(&mono_hRef, 0, 0);
  snap();
  tgfxReport("mono_lit", lit());
  tgfxShot("mono_h", gram, W, H);

  reset(); lcd.drawImage(&mono_vRef, 0, 0);
  tgfxReport("mono_v_diff", diff());
  tgfxShot("mono_v", gram, W, H);

  // --- clipping and off screen ----------------------------------------------
  reset();
  lcd.setClipRect(8, 8, 8, 8);
  lcd.drawImage(&same_rlepal4Ref, 0, 0);
  lcd.clearClipRect();
  long outside = 0;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if ((x < 8 || x >= 16 || y < 8 || y >= 16) && gram[y * W + x] != 0) ++outside;
    }
  }
  tgfxReport("clip_outside", outside);

  reset();
  lcd.drawImage(&same_rlepal4Ref, -40, -40);
  lcd.drawImage(&same_rlepal4Ref, 40, 40);
  tgfxReport("offscreen_pixels", (long)bus.pixelCount());

  // --- raw565 opens one window a row, no more -------------------------------
  //
  // A photograph has runs of one, so a fillRect per run means **a window per
  // pixel** (CASET + RASET + RAMWR is 11 bytes; a pixel is 2). Drawing a 64x64
  // photograph into a 32x32 screen leaves 32 rows visible, so there must be
  // **one window a row: 96 commands**.
  reset();
  lcd.drawImage(&img_photo64Ref, 0, 0);
  tgfxReport("photo_cmds", (long)bus.commandCount());
  tgfxReport("photo_pixels", (long)bus.pixelCount());
  snap();
  tgfxShot("photo", gram, W, H);

  // The same hanging off the top left. **Nothing outside the window may be
  // counted** - this path clips itself, and getting it wrong writes off screen.
  reset();
  lcd.drawImage(&img_photo64Ref, -8, -8);
  tgfxReport("photo_off_cmds", (long)bus.commandCount());
  tgfxReport("photo_off_pixels", (long)bus.pixelCount());

  // Inside the clip must not differ by a pixel from no clip; outside it, not
  // one pixel may be touched.
  reset();
  lcd.setClipRect(8, 8, 16, 16);
  lcd.drawImage(&img_photo64Ref, 0, 0);
  lcd.clearClipRect();
  {
    long inDiff = 0, outLit = 0;
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        const bool inside = (x >= 8 && x < 24 && y >= 8 && y < 24);
        if (inside) { if (gram[y * W + x] != ref[y * W + x]) ++inDiff; }
        else if (gram[y * W + x] != 0) ++outLit;
      }
    }
    tgfxReport("photo_clip_in_diff", inDiff);
    tgfxReport("photo_clip_out_lit", outLit);
    tgfxReport("photo_clip_cmds", (long)bus.commandCount());
    tgfxReport("photo_clip_pixels", (long)bus.pixelCount());
  }

  // Extreme coordinates. **`clipX0 - x` overflows int16_t**: at x = -32768 the
  // difference is 32768. Taking coordinates from outside the screen and
  // clipping them is part of the contract (the same case as "extreme
  // coordinates" in tests/clip), so walk into it here. On overflow c0 goes
  // negative and it **reads in front of the image data.**
  reset();
  lcd.drawImage(&img_photo64Ref, -32768, 0);
  lcd.drawImage(&img_photo64Ref, 32767, 0);
  lcd.drawImage(&img_photo64Ref, 0, -32768);
  lcd.drawImage(&img_photo64Ref, 0, 32767);
  tgfxReport("photo_extreme_pixels", (long)bus.pixelCount());
  tgfxReport("photo_extreme_cmds", (long)bus.commandCount());

  // --- transparency ---------------------------------------------------------
  //
  // The same image drawn with and without transparency. **Only the pixels of
  // the transparent colour leave the background showing; nothing else may
  // differ by a pixel.**
  //
  // Whether transparency is honoured is decided by the ops, not the format (the
  // generated header points at one), so a sketch that only uses opaque images
  // does not link the test for it.
  {
    reset();
    lcd.fillRect(0, 0, W, H, 0xF81F);       // the background: magenta
    lcd.drawImage(&same_rlepal4Ref, 0, 0);  // opaque: paints everything
    long bgLeftOpaque = 0;
    for (int i = 0; i < W * H; ++i) { if (gram[i] == 0xF81F) ++bgLeftOpaque; }
    tgfxReport("opaque_bg_left", bgLeftOpaque);
    snap();

    reset();
    lcd.fillRect(0, 0, W, H, 0xF81F);
    lcd.drawImage(&trans_iconRef, 0, 0);    // transparent: skips the black
    long bgLeftTrans = 0, differ = 0;
    for (int i = 0; i < W * H; ++i) {
      if (gram[i] == 0xF81F) ++bgLeftTrans;
      if (gram[i] != ref[i]) ++differ;
    }
    tgfxReport("trans_bg_left", bgLeftTrans);
    tgfxReport("trans_differ", differ);
    tgfxShot("trans", gram, W, H);
  }

  tgfxTestDone();
}
void loop() {}
