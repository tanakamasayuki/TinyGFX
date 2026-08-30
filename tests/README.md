# Tests

> 日本語: [README.ja.md](README.ja.md)

The TinyGFX test suite. Strategy and case list live in
[../docs/TEST_PLAN.ja.md](../docs/TEST_PLAN.ja.md) (Japanese).

- [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) with the Arduino CLI backend, dependencies managed by `uv`
- **Tier 0 (`footprint/`, `linkprune/`) never runs a sketch.** It builds and inspects
  size and the symbol table, so there is no `dut` fixture and no hardware
- **Tier 1 runs headless on the `lang-ship:host` core** and checks
  the drawn result pixel by pixel. No SDL2, no LovyanGFX, no hardware
- **Tier 2 (`build_matrix/`) only compiles** the examples for several cores

## Running

```sh
uv sync
uv run pytest -v -s          # everything (-s prints the footprint table)
uv run pytest linkprune -v   # one suite
```

**Never build the same sketch concurrently.** arduino-cli derives the build directory from
the sketch path (`~/.cache/arduino/sketches/<hash>`), so two builds of the same sketch - even
under different profiles - overwrite each other's files. `Image does not have a valid ELF
header` and a missing `partitions.csv` are both this. Do not run `arduino-cli compile` by
hand while pytest is running.

Tier 0 needs a CH32 core; the tests skip without it:

```sh
arduino-cli core install ch32-riscv-arduino:ch32riscv
```

## Layout

```text
tests/
  tinygfx_build.py    shared helper: drives arduino-cli, reads sizes and symbols
  tgfx_check.py       reads what a sketch left in output/ (report / image / lit / colour)
  constructs/         measurement sketches: base / a..e / t / p1 / p2
  common_libs/
    tgfx_test/        PPM output and key=value reporting into output/report.txt
    tgfx_font/        stopgap font; not shipped in the library
  footprint/          size regression: increment over base must stay in budget
  linkprune/          unused features must not survive into the final binary
  capture/            BusCapture rebuilds an image from an ST7789 command stream
  window/             ST7789 rotation MADCTL, width/height, origin offset
  ili9342/            ILI9342C MADCTL, colour order (BGR), mirroring
  ili9341/            ILI9341, and the init sequence all three DCS panels send
  primitive/          every primitive plus degenerate cases
  clip/               inside the clip equals no clip, outside is untouched (invariant)
  fill/               exact number of pixels pushed
  tile/               tile height never changes the image (invariant)
  text/               text: return value, scaling, missing glyphs, background, transparency
  fontchain/          CellFont chaining, U+FFFD fallback, baseline alignment
  panels/             the panel catalogue: what a preset actually carries
  utf8/               UTF-8 decoding: code point and bytes consumed, per sequence
  textwrap/           wrapping at the right edge (TINYGFX_TEXT_WRAP=1)
  panels_seqcom/      the `_SeqCom` entry: same driver and size, different value
  image/              pushImage placement, cropping, transparency, byte swapping
  image_fmt/          every encoding of one picture draws the same pixels
  image_oracle/       converter output vs the converter's own expected image
  hostbus/            captures the bytes the real SPI bus emitted and rebuilds the image
  fillchunk/          block-writing must not change a single byte on the wire
  clifont/            a CellFont header **from the real generator** must render
  scene/              produces the golden the hardware test is compared against
  hw/m5stack/         **real hardware (Tier 3).** Runs only when .env is passed
  u8g2/               u8g2-format font decoding
  i2c/                I2C + SSD1306 (monochrome, page transfer, dirty pages)
  monospi/            SPI + SSD1306 / SH1106: transaction and CS etiquette
  softi2c/            the bit-banged I2C waveform, decoded back into bytes
  sh1106/             SH1106 wiring: the same picture an SSD1306 gives
  build_matrix/       examples compile for ch32v003 / uno / esp32 / m5stack (never run)
  manual/m5stack/     **hardware check sketch.** Not run by pytest (only compiled)
```

## What `linkprune/` checks

Dropping unused code is the linker's job (`--gc-sections`). What this suite checks is
whether something that *should* have been dropped is still reachable through a chain
of references.

For example, a sketch that only calls `fillScreen` must not contain `drawCircle`,
font data, or `Print`. A failure means one of the rules R1-R9 in
[../docs/CORE_DESIGN.ja.md](../docs/CORE_DESIGN.ja.md) §7.4 was broken.

Verdicts are computed as a **diff against base** (an empty sketch), so symbols the
core pulls in on its own (such as `_malloc_r`) are not blamed on TinyGFX.
