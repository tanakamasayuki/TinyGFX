// **One image out of a bundle of eleven.**
//
// The bundle is tests/image_oracle/images.h, brought in with -I so that there
// is one copy and regen.py keeps it current. It holds eleven images and the
// index arrays the converter emits (`images_file_refs` and friends).
//
// Nothing here touches the indexes, and only iconRef is drawn. **Every other
// image, and every decoder those images would need, must drop.**
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
  lcd.drawImage(&iconRef, 0, 0);
}
void loop() {}
