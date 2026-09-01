// The `_SeqCom` entry: **same driver, same size, different value.**
//
// A 128x64 SSD1306 is sold with its COM lines wired both ways round, so the
// catalogue carries two entries at that size. They are the case where the size
// cannot tell them apart - which is exactly what a derivation from the height
// would get wrong, and why the height is not used for this.
//
// Only one panel per driver fits in a sketch, so this is a sketch of its own.
// tests/panels/ drives a 128x32; this one drives the 128x64 that is *not* the
// default, and the two together show the value really comes from the header.
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/panels/SSD1306_128x64_SeqCom.h>

// **One panel, so the shorthand is here.** Including a panel also names it, so
// that the class name is never written twice. The two-panel case in
// tests/panels/ asserts the other half: there the name is taken away.
#ifndef TINYGFX_PANEL
#error "TINYGFX_PANEL is missing from a sketch with exactly one panel."
#endif
#include <tgfx_test.h>

static const uint8_t ADDR = 0x3C;
static uint8_t muxArg = 0xFF, comArg = 0xFF, pendingCmd = 0;

static uint8_t onWire(uint8_t addr, const uint8_t* d, size_t len, bool stop, void* user) {
  (void)stop; (void)user;
  if (addr != ADDR || len == 0) return 2;
  if (d[0] != 0x00) return 0;                 // 0x00 = command stream
  for (size_t i = 1; i < len; ++i) {
    if (pendingCmd == 0xA8) { muxArg = d[i]; pendingCmd = 0; continue; }
    if (pendingCmd == 0xDA) { comArg = d[i]; pendingCmd = 0; continue; }
    if (d[i] == 0xA8 || d[i] == 0xDA) pendingCmd = d[i];
  }
  return 0;
}

static uint8_t fb[TinyGFXPanelSSD1306_128x64_SeqCom::kBufferBytes];
TinyGFXBusI2C bus(Wire, ADDR);
TinyGFXPanelSSD1306_128x64_SeqCom panel(bus, fb);
TinyGFX lcd(panel);

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("panels_seqcom");
  Wire.begin();
  Wire.setWriteHook(onWire, nullptr);

  lcd.begin();
  tgfxReport("width", (long)lcd.width());
  tgfxReport("height", (long)lcd.height());
  tgfxReport("buffer_bytes", (long)TinyGFXPanelSSD1306_128x64_SeqCom::kBufferBytes);
  tgfxReport("mux", (long)muxArg);       // derived from the height
  tgfxReport("com_pins", (long)comArg);  // ★ carried by this entry, not the size

  tgfxTestDone();
}
void loop() {}
