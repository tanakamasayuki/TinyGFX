// The two spellings of the colour constants (DECISIONS.ja.md D30).
//
// **Every check here happens at compile time.** Nothing runs.
// It also builds with `TFT_RED` already defined, to show that
//   - whoever defined it first keeps it (TinyGFX does not overwrite)
//   - `TINYGFX_RED` is TinyGFX's value regardless
// build_matrix compiles it both ways.
#include <TinyGFX.h>

static_assert(TINYGFX_BLACK == 0x0000, "TINYGFX_BLACK");
static_assert(TINYGFX_WHITE == 0xFFFF, "TINYGFX_WHITE");
static_assert(TINYGFX_RED == 0xF800, "TINYGFX_RED cannot be taken by another library");
static_assert(TINYGFX_DARKGREY == 0x7BEF, "TINYGFX_DARKGREY");

#if defined(TGFX_FOREIGN_RED)
static_assert(TFT_RED == TGFX_FOREIGN_RED, "a pre-existing TFT_RED was overwritten");
static_assert(TFT_BLACK == TINYGFX_BLACK, "one name being taken lost the others too");
#else
static_assert(TFT_RED == TINYGFX_RED, "the two spellings disagree");
static_assert(TFT_PINK == TINYGFX_PINK, "the two spellings disagree");
#endif

void setup() {}
void loop() {}
