// The SH1106 wiring, checked without hardware.
//
//   TinyGFX -> DriverSH1106 -> the real TinyGFXBusI2C -> Wire
//           -> the host's Wire probe -> a model of an SH1106 -> bitmap
//
// An SH1106 differs from an SSD1306 in exactly two ways, and both are covered.
//
//   1. **132 columns of RAM behind 128 columns of glass.** The left edge of the
//      picture is RAM column 2; get it wrong and the picture shifts two pixels
//   2. **no column/page range commands.** 0x21 / 0x22 are unavailable, so the
//      cursor is placed per page (0xB0|page, then 0x00 / 0x10 for the column)
//      and one page is streamed at a time
//
// **The same picture is drawn on an SSD1306 too, and the decoded results must
// not differ by a bit.** Everything shared (DriverPaged) is the same code, so a
// difference here is a defect in the transfer layer.
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/DriverSH1106.h>
#include <TinyGFX/DriverSSD1306.h>
#include <TinyGFX/Image.h>
#include "images.h"
#include <tgfx_test.h>
#include <TinyGFX/FontCell.h>
#include <tgfx_digits.h>
#include <Wire.h>

static const TinyGFXFontRef digitsFont = {&tgfxDigits, &tinygfxFontCellOps};

static const int W = 128, H = 64, PAGES = H / 8;
static const int RAM_W = 132;  // how many columns an SH1106 really has
static const uint8_t ADDR = 0x3C;

static uint8_t fbA[W * H / 8], fbB[W * H / 8];
TinyGFXBusI2C bus(Wire, ADDR);
TinyGFXDriverSH1106 sh(bus, fbA, W, H);
TinyGFXDriverSSD1306 ssd(bus, fbB, W, H);
TinyGFX shLcd(sh);
TinyGFX ssdLcd(ssd);

// ---- a model of the receiver ---------------------------------------------
// An SH1106 has 132 columns; an SSD1306 uses only 128 of the same array.
static uint8_t model[RAM_W * PAGES];
static uint16_t curCol = 0, curPage = 0;
static uint16_t colStart = 0, colEnd = W - 1;
static uint8_t pendingCmd = 0, argIndex = 0, args[2] = {0, 0};
static bool sh1106Mode = true;
static uint32_t dataBytes = 0;

static void feedCmd(uint8_t c) {
  if (sh1106Mode) {
    // The cursor is placed per page; there is no range to set
    if ((c & 0xF0) == 0xB0)      curPage = (uint16_t)(c & 0x0F);
    else if ((c & 0xF0) == 0x00) curCol = (uint16_t)((curCol & 0xF0) | (c & 0x0F));
    else if ((c & 0xF0) == 0x10) curCol = (uint16_t)((curCol & 0x0F) | ((c & 0x0F) << 4));
    return;
  }
  if (pendingCmd != 0) {
    args[argIndex++] = c;
    if (argIndex == 2) {
      if (pendingCmd == 0x21) { colStart = args[0]; colEnd = args[1]; curCol = colStart; }
      else                    { curPage = args[0]; }
      pendingCmd = 0;
      argIndex = 0;
    }
    return;
  }
  if (c == 0x21 || c == 0x22) { pendingCmd = c; argIndex = 0; }
}

static void putByte(uint8_t b) {
  const uint16_t ramW = sh1106Mode ? (uint16_t)RAM_W : (uint16_t)W;
  if (curPage < (uint16_t)PAGES && curCol < ramW) {
    model[(uint32_t)curPage * RAM_W + curCol] = b;
  }
  if (sh1106Mode) {
    // The column advances within the page and wraps at the end; it never
    // crosses into the next page
    curCol = (uint16_t)((curCol + 1) % ramW);
  } else {
    if (curCol >= colEnd) { curCol = colStart; ++curPage; }
    else { ++curCol; }
  }
}

static uint8_t onWire(uint8_t addr, const uint8_t* d, size_t len, bool stop, void* user) {
  (void)stop; (void)user;
  if (addr != ADDR || len == 0) return 2;
  if (d[0] == 0x00)      { for (size_t i = 1; i < len; ++i) feedCmd(d[i]); }
  else if (d[0] == 0x40) { for (size_t i = 1; i < len; ++i) { putByte(d[i]); ++dataBytes; } }
  return 0;
}

static void scene(TinyGFX& g) {
  g.drawRect(0, 0, W, H, TFT_WHITE);
  g.fillRect(8, 8, 40, 16, TFT_WHITE);
  g.drawCircle(96, 32, 20, TFT_WHITE);
  g.fillTriangle(20, 60, 40, 34, 60, 60, TFT_WHITE);
  g.drawLine(0, 0, W - 1, H - 1, TFT_WHITE);
  g.setFont(&digitsFont);
  g.setTextColor(TFT_WHITE);
  g.drawString("12345", 70, 4);
}

/// Take the visible 128 columns out of the model's RAM.
static void extract(uint8_t* out, int colOffset) {
  for (int p = 0; p < PAGES; ++p) {
    for (int x = 0; x < W; ++x) out[p * W + x] = model[p * RAM_W + colOffset + x];
  }
}

static uint16_t image[W * H];
static void shot(const char* name, const uint8_t* bits) {
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      image[(uint32_t)y * W + x] = (bits[(y >> 3) * W + x] >> (y & 7)) & 1 ? 0xFFFF : 0x0000;
    }
  }
  tgfxShot(name, image, W, H);
}

static uint8_t fromSh[W * H / 8], fromSsd[W * H / 8];

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("sh1106");
  Wire.setWriteHook(onWire, nullptr);
  Wire.begin();

  // --- SH1106 -------------------------------------------------------------
  sh1106Mode = true;
  for (int i = 0; i < RAM_W * PAGES; ++i) model[i] = 0;
  shLcd.begin();
  dataBytes = 0;
  scene(shLcd);
  sh.display();
  tgfxReport("sh_bytes", (long)dataBytes);
  extract(fromSh, 2);  // the default column offset
  shot("sh1106", fromSh);

  // The picture must not be one flat colour: all-white matching all-white
  // would prove nothing
  long lit = 0;
  for (int i = 0; i < W * H / 8; ++i) {
    for (int b = 0; b < 8; ++b) {
      if ((fromSh[i] >> b) & 1) ++lit;
    }
  }
  tgfxReport("sh_lit", lit);

  // Nothing may be written outside the glass (columns 0, 1 and 130, 131)
  long outside = 0;
  for (int p = 0; p < PAGES; ++p) {
    if (model[p * RAM_W + 0]) ++outside;
    if (model[p * RAM_W + 1]) ++outside;
    if (model[p * RAM_W + 130]) ++outside;
    if (model[p * RAM_W + 131]) ++outside;
  }
  tgfxReport("outside_glass", outside);

  // --- the same picture on an SSD1306 ---------------------------------------
  sh1106Mode = false;
  for (int i = 0; i < RAM_W * PAGES; ++i) model[i] = 0;
  ssdLcd.begin();
  dataBytes = 0;
  scene(ssdLcd);
  ssd.display();
  tgfxReport("ssd_bytes", (long)dataBytes);
  extract(fromSsd, 0);
  shot("ssd1306", fromSsd);

  long diff = 0;
  for (int i = 0; i < W * H / 8; ++i) {
    if (fromSh[i] != fromSsd[i]) ++diff;
  }
  tgfxReport("sh_vs_ssd_diff", diff);

  // --- changing the column offset must shift the picture --------------------
  sh1106Mode = true;
  for (int i = 0; i < RAM_W * PAGES; ++i) model[i] = 0;
  sh.setColumnOffset(0);
  sh.clearBuffer();
  scene(shLcd);
  sh.display();
  static uint8_t shifted[W * H / 8];
  extract(shifted, 0);
  long same = 0;
  for (int i = 0; i < W * H / 8; ++i) {
    if (shifted[i] == fromSsd[i]) ++same;
  }
  tgfxReport("offset0_matches_ssd", same == W * H / 8 ? 1 : 0);
  sh.setColumnOffset(2);

  // --- the fast path for vertically packed bitmaps --------------------------
  //
  // **A page-aligned vertical bitmap is this panel's buffer, exactly.**
  // pushVBitmap() blits it. It must not differ from the general drawImage() by
  // a single bit - otherwise it is not a faster path but a different one.
  sh1106Mode = false;   // observed through the SSD1306 model
  {

  }
  {
    for (int i = 0; i < RAM_W * PAGES; ++i) model[i] = 0;
    ssd.clearBuffer();
    const bool fast = ssd.pushVBitmap(0, 0, W, H, splash_vData);
    ssd.display();
    static uint8_t viaFast[W * H / 8];
    extract(viaFast, 0);
    tgfxReport("vblit_taken", fast ? 1 : 0);

    for (int i = 0; i < RAM_W * PAGES; ++i) model[i] = 0;
    ssd.clearBuffer();
    ssdLcd.drawImage(&splash_vRef, 0, 0);
    ssd.display();
    static uint8_t viaGeneric[W * H / 8];
    extract(viaGeneric, 0);

    long d = 0;
    for (int i = 0; i < W * H / 8; ++i) { if (viaFast[i] != viaGeneric[i]) ++d; }
    tgfxReport("vblit_diff", d);

    long lit = 0;
    for (int i = 0; i < W * H / 8; ++i) {
      for (int b = 0; b < 8; ++b) { if ((viaFast[i] >> b) & 1) ++lit; }
    }
    tgfxReport("vblit_lit", lit);

    // An unaligned position is refused: nothing drawn, false returned
    ssd.clearBuffer();
    tgfxReport("vblit_unaligned", ssd.pushVBitmap(0, 3, W, H, splash_vData) ? 1 : 0);
    tgfxReport("vblit_offpanel", ssd.pushVBitmap(0, 0, W, 128, splash_vData) ? 1 : 0);
    ssdLcd.setRotation(1);
    tgfxReport("vblit_rotated", ssd.pushVBitmap(0, 0, W, H, splash_vData) ? 1 : 0);
    ssdLcd.setRotation(0);
  }

  tgfxTestDone();
}
void loop() {}
