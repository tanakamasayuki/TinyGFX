// **The same bundle, indexed with a value the compiler cannot know.**
//
// This is the counterpart to img_one: it exists to show that img_one's result
// is the linker doing its job, not the check looking at nothing. Here the
// index is a runtime value, so **every image in the bundle has to stay**.
//
// It is also the sharp edge worth knowing about. docs/IMAGE_FORMAT.ja.md
#include <TinyGFX.h>
#include <TinyGFX/BusSoftSPI.h>
#include <TinyGFX/DriverST7789.h>
#include <TinyGFX/Image.h>
#include <images.h>

TinyGFXBusSoftSPI bus(5, 6, 3, 4);
TinyGFXDriverST7789 panel(bus, 240, 240, 2);
TinyGFX lcd(panel);

void setup() {
  lcd.begin();
  lcd.drawImage(images_file_refs[millis() % images_file_count], 0, 0);
}
void loop() {}
