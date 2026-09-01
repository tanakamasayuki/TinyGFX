// The panel catalogue: does a panel header carry what its module needs?
//
// A panel is a preset - the product you bought - and what it carries is only
// what cannot be derived. **Everything else the driver works out**, so the
// interesting checks are the two that a preset really does decide:
//
//   1. the COM pin layout, which is a fact about how the glass is wired
//   2. the column offset, when the glass does not sit centred in RAM
//
// Only one panel per driver can exist in a sketch (the header refuses to be the
// second - docs/GLOSSARY.md 3), so this drives **the 128x32**: it is the one
// whose COM pin layout differs from the datasheet reset, which makes it the
// case that proves the mechanism carries anything at all.
#include <TinyGFX.h>
#include <TinyGFX/BusI2C.h>
#include <TinyGFX/panels/SSD1306_128x32.h>
#include <TinyGFX/panels/ST7789_240x240.h>   // a different driver: must coexist

// **Two panels, so the shorthand has to be gone.** A single-panel sketch gets
// `TINYGFX_PANEL` as the name of the one it drives, which is what keeps the
// class name from being written twice. Here there is no such thing as "the"
// panel, and the second header to arrive takes the name away rather than
// quietly meaning one of the two.
//
// This is the assertion for that, and it costs nothing to make.
#ifdef TINYGFX_PANEL
#error "TINYGFX_PANEL survived two panel headers. It would then silently name whichever was included first."
#endif
#include <TinyGFX/BusCapture.h>
#include <tgfx_test.h>

static const uint8_t ADDR = 0x3C;

// The two init bytes worth watching, picked out of the Wire traffic.
static uint8_t muxArg = 0xFF, comArg = 0xFF, pendingCmd = 0;

static uint8_t onWire(uint8_t addr, const uint8_t* data, size_t len, bool stop, void* user) {
  (void)stop; (void)user;
  if (addr != ADDR || len == 0) return 2;
  if (data[0] != 0x00) return 0;             // 0x00 = command stream
  for (size_t i = 1; i < len; ++i) {
    if (pendingCmd == 0xA8) { muxArg = data[i]; pendingCmd = 0; continue; }
    if (pendingCmd == 0xDA) { comArg = data[i]; pendingCmd = 0; continue; }
    if (data[i] == 0xA8 || data[i] == 0xDA) pendingCmd = data[i];
  }
  return 0;
}

static uint8_t fb[TinyGFXPanelSSD1306_128x32::kBufferBytes];
TinyGFXBusI2C i2c(Wire, ADDR);
TinyGFXPanelSSD1306_128x32 oled(i2c, fb);

// A colour panel of a different driver, on a capture bus.
static uint16_t gram[8 * 8];
TinyGFXBusCapture cap(gram, 8, 8);
TinyGFXPanelST7789_240x240 tft(cap);

void setup() {
  Serial.begin(115200);
  tgfxTestBegin("panels");
  Wire.begin();
  Wire.setWriteHook(onWire, nullptr);

  // --- the buffer size comes from the panel, not from the sketch ------------
  tgfxReport("buffer_bytes", (long)TinyGFXPanelSSD1306_128x32::kBufferBytes);
  tgfxReport("panel_w", (long)TinyGFXPanelSSD1306_128x32::kWidth);
  tgfxReport("panel_h", (long)TinyGFXPanelSSD1306_128x32::kHeight);

  // --- what the preset actually carries ------------------------------------
  TinyGFX g(oled);
  g.begin();
  tgfxReport("mux", (long)muxArg);       // derived: height - 1
  tgfxReport("com_pins", (long)comArg);  // ★ carried by the preset
  tgfxReport("col0", (long)oled.columnOffset());  // derived: centred in RAM
  tgfxReport("width", (long)g.width());
  tgfxReport("height", (long)g.height());

  // --- a panel of a different driver coexists ------------------------------
  //
  // The include guard is per driver, so a TFT next to an OLED builds. That this
  // sketch compiles at all is most of the check; the rest is that the colour
  // panel came up with its GRAM size set, which a bare driver would not have.
  TinyGFX t(tft);
  t.begin();
  tgfxReport("tft_w", (long)t.width());
  tgfxReport("tft_h", (long)t.height());
  // Rotation 2 on a 240x240 ST7789 sits 80 rows down its 240x320 memory.
  // **A bare driver that was never told the GRAM size puts it at 0** - which is
  // what every example in this repository used to do.
  t.setRotation(2);
  t.setAddrWindow(0, 0, 1, 1);
  tgfxReport("tft_rot2_ys", (long)cap.windowYs());

  tgfxTestDone();
}
void loop() {}
