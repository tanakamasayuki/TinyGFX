// UTF-8 decoding.
//
// The strings a sketch writes are UTF-8 whether or not anyone chose that: the
// Arduino IDE saves source that way. So the interesting cases here are not
// "does CJK work" but "does an ordinary string with a degree sign in it come
// out as one glyph", and "does a malformed byte cost one character or the rest
// of the line".
//
// nextCode is tested directly rather than through pixels. It is a pure
// function over bytes, and the exact code point and the exact number of bytes
// consumed are what everything else rests on.
#include <TinyGFX.h>
#include <TinyGFX/BusCapture.h>
#include <TinyGFX/DriverST7789.h>
#include <TinyGFX/Print.h>
#include <TinyGFX/FontCell.h>
#include <tgfx_test.h>
#include <tgfx_utf8.h>

// Coverage: U+0030-U+0039, U+0041, U+00B0, U+2103
// One byte, two bytes and three bytes in one font, which is the whole point.
static const TinyGFXFontRef utf8Font = {&tgfxUtf8, &tinygfxFontCellOps};

static const int W = 96, H = 32;
static uint16_t gram[W * H];
TinyGFXBusCapture bus(gram, W, H);
TinyGFXDriverST7789 panel(bus, W, H);
TinyGFXPrint lcd(panel);

static const uint16_t FG = 0xFFFF;
static void reset() { bus.fill(0); bus.resetCounters(); }

/// The rightmost lit column, or -1. Where the pen ended up, checked against
/// the picture rather than against the return value.
static int16_t lastLitColumn() {
  for (int16_t x = (int16_t)(W - 1); x >= 0; --x) {
    for (int16_t y = 0; y < H; ++y) {
      if (gram[(int32_t)y * W + x] != 0x0000) return x;
    }
  }
  return -1;
}

/// Decode one character and report both halves of the answer: the code point,
/// and how far the pointer moved. A decoder that returns the right code point
/// but eats the wrong number of bytes wrecks everything after it, so the
/// second number matters as much as the first.
static void probe(const char* name, const char* s) {
  const char* p = s;
  const uint16_t cp = TinyGFX::nextCode(p);
  tgfxReport2(name, "cp", (long)cp);
  tgfxReport2(name, "used", (long)(p - s));
}

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("utf8");

  lcd.begin();
  lcd.setFont(&utf8Font);
  lcd.setTextColor(FG);

  // ---- the decoder, byte sequence by byte sequence ------------------------
  probe("ascii", "A");                        // 41
  probe("two", "\xC2\xB0");                   // U+00B0 DEGREE SIGN
  probe("three", "\xE2\x84\x83");             // U+2103 DEGREE CELSIUS
  probe("four", "\xF0\x9F\x98\x80");          // U+1F600, past U+FFFF
  probe("stray", "\x80""A");                  // continuation byte with no lead
  probe("ff", "\xFF""A");                     // never appears in UTF-8
  probe("cut_end", "\xC2");                   // lead, then the terminator
  probe("cut_mid", "\xE2\x84""A");            // lead, one continuation, then 'A'
  probe("overlong", "\xE0\x80\x80");          // overlong NUL: decoded, not rejected

  // A sequence cut short must leave the pointer on the byte that broke it,
  // so the character that byte starts is still drawn.
  {
    const char* s = "\xC2""A";
    const char* p = s;
    const uint16_t bad = TinyGFX::nextCode(p);
    const uint16_t next = TinyGFX::nextCode(p);
    tgfxReport("recover_bad", (long)bad);
    tgfxReport("recover_next", (long)next);
    tgfxReport("recover_used", (long)(p - s));
  }

  // ---- strings ------------------------------------------------------------
  // Three characters, six bytes. A byte-per-glyph reading gives six advances.
  tgfxReport("w_mixed", (long)lcd.textWidth("0\xC2\xB0\xE2\x84\x83"));
  tgfxReport("w_c0", (long)lcd.textWidth("0"));
  tgfxReport("w_deg", (long)lcd.textWidth("\xC2\xB0"));
  tgfxReport("w_cel", (long)lcd.textWidth("\xE2\x84\x83"));

  reset();
  tgfxReport("draw_mixed", (long)lcd.drawString("0\xC2\xB0\xE2\x84\x83", 0, 0));
  tgfxReport("mixed_last_col", (long)lastLitColumn());
  tgfxShot("mixed", gram, W, H);

  // An astral character is not in the font and has no U+FFFD to fall back to,
  // so it draws nothing. What it must still do is consume all four bytes:
  // "0<emoji>1" has to land its '1' exactly where "01" does.
  reset();
  lcd.drawString("01", 0, 0);
  const int16_t plainEnd = lastLitColumn();
  reset();
  lcd.drawString("0\xF0\x9F\x98\x80" "1", 0, 0);
  tgfxReport("astral_end", (long)lastLitColumn());
  tgfxReport("plain_end", (long)plainEnd);

  // ---- Print --------------------------------------------------------------
  // Print delivers one byte at a time, so it holds a half-built character
  // between calls. Same string, same picture.
  reset();
  lcd.setCursor(0, 0);
  lcd.print("0\xC2\xB0\xE2\x84\x83");
  tgfxReport("print_last_col", (long)lastLitColumn());
  tgfxShot("print", gram, W, H);

  // Four bytes through Print: held, discarded, and the '1' lands where it
  // would have anyway.
  reset();
  lcd.setCursor(0, 0);
  lcd.print("0\xF0\x9F\x98\x80" "1");
  tgfxReport("print_astral_end", (long)lastLitColumn());

  // A newline arriving mid-sequence must still be a newline.
  reset();
  lcd.setCursor(0, 0);
  lcd.print("\xE2\x84\n0");
  tgfxReport("print_cut_nl_x", (long)lcd.getCursorX());
  tgfxReport("print_cut_nl_y", (long)lcd.getCursorY());
  tgfxReport("line_height", (long)lcd.fontHeight());

  tgfxTestDone();
}
void loop() {}
