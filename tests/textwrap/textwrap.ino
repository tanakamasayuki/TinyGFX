// Wrapping at the right edge (Adafruit_GFX's setTextWrap).
//
// **Not on by default.** Wrapping correctly means knowing whether a character
// fits **before** drawing it - one that does not fit belongs on the next line,
// not drawn clipped against the edge. That is a second entry point into the
// font decoder (`advance`, on top of `draw`), and the linker cannot drop it
// once `write()` refers to it. Measured at **164 B on a CH32V003**, paid by
// every sketch that prints.
//
// So it appears only when `TINYGFX_TEXT_WRAP` is 1, and this exercises that side.
#define TINYGFX_TEXT_WRAP 1

#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/PanelST7789.h>
#include <TinyGFX/Print.h>
#include <TinyGFX/FontCell.h>
#include <tgfx_test.h>
#include <tgfx_utf8.h>

// Digits advance by 4; '°' and '℃' by 8. **Characters of different widths are
// the point** - a wrap decision made against a fixed width fails here.
static const TinyGFXFontRef font = {&tgfxUtf8, &tinygfxFontCellOps};

static const int W = 32, H = 48;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXPanelST7789 panel(bus, W, H);
TinyGFXPrint lcd(panel);

static void reset() { bus.fill(0); bus.resetCounters(); }

/// How many rows carry any ink. Counted directly rather than divided out of
/// the line height.
static long litRows() {
  long n = 0;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (gram[y * W + x] != 0) { ++n; break; }
    }
  }
  return n;
}

/// The rightmost column with ink. With wrapping on it never reaches the width.
static long rightmostLit() {
  for (int x = W - 1; x >= 0; --x) {
    for (int y = 0; y < H; ++y) {
      if (gram[y * W + x] != 0) return x;
    }
  }
  return -1;
}

static void run(const char* name, bool wrap, int16_t cx, int16_t cy, const char* s) {
  reset();
  lcd.setTextWrap(wrap);
  lcd.setCursor(cx, cy);
  lcd.print(s);
  tgfxReport2(name, "x", (long)lcd.getCursorX());
  tgfxReport2(name, "y", (long)lcd.getCursorY());
  tgfxReport2(name, "rows", litRows());
  tgfxReport2(name, "right", rightmostLit());
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("textwrap");

  lcd.begin();
  lcd.setFont(&font);
  lcd.setTextColor(0xFFFF);
  tgfxReport("width", (long)lcd.width());
  tgfxReport("line", (long)lcd.fontHeight());
  tgfxReport("adv_digit", (long)lcd.textWidth("0"));
  tgfxReport("adv_wide", (long)lcd.textWidth("\xE2\x84\x83"));

  // Off by default. Twelve characters overrun the width (32 / advance 4 = 8)
  run("off", false, 0, 0, "012345678901");

  // On. Eight characters to a line, the remaining four on the next
  run("on", true, 0, 0, "012345678901");

  // A wrapped line restarts at **the x of setCursor** - where '\n' goes too.
  // Starting at x=4 fits 7 characters to a line.
  run("indent", true, 4, 0, "012345678901");

  // A character of a different width. At x=28 the '℃', which advances 8, does
  // not fit and must wrap. Seven digits (x=28), then '℃'.
  run("wide", true, 0, 0, "0123456\xE2\x84\x83");

  // '\n' is still '\n' with wrapping on
  run("newline", true, 4, 0, "01\n2");

  tgfxTestDone();
}
void loop() {}
