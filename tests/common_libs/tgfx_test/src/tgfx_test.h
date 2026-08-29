// Shared parts for the TinyGFX test sketches. Not part of the library itself.
//
// Everything produced goes into output/:
//   output/<name>.ppm    what was drawn (P6; RGB565 only needs expanding, so
//                        there is no reason to reach for PNG)
//   output/report.txt    one key=value a line
//
// Values go to a file rather than the serial line so that a dropped
// dut.expect cannot make a test flaky. Serial carries progress only.
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

inline FILE*& tgfxReportFile() {
  static FILE* f = nullptr;
  return f;
}

inline void tgfxTestBegin(const char* name) {
  mkdir("output", 0755);
  tgfxReportFile() = fopen("output/report.txt", "w");
  Serial.print("TEST start ");
  Serial.println(name);
}

inline void tgfxTestDone() {
  FILE*& f = tgfxReportFile();
  if (f != nullptr) {
    fclose(f);
    f = nullptr;
  }
  Serial.println("TEST done");
}

/// Write one key=value line into report.txt, and echo it to serial for the log.
inline void tgfxReport(const char* key, long value) {
  FILE* f = tgfxReportFile();
  if (f != nullptr) fprintf(f, "%s=%ld\n", key, value);
  Serial.print(key);
  Serial.print('=');
  Serial.println(value);
}

/// The same, with a prefix - one value per scene, say.
inline void tgfxReport2(const char* prefix, const char* key, long value) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s_%s", prefix, key);
  tgfxReport(buf, value);
}

/// Write an RGB565 buffer out as a PPM (P6).
inline bool tgfxWritePpm(const char* path, const uint16_t* px, int w, int h) {
  FILE* f = fopen(path, "wb");
  if (f == nullptr) return false;
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (long i = 0, n = (long)w * h; i < n; ++i) {
    const uint16_t c = px[i];
    const uint8_t rgb[3] = {
        (uint8_t)((c >> 8) & 0xF8),
        (uint8_t)((c >> 3) & 0xFC),
        (uint8_t)((c << 3) & 0xF8),
    };
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  return true;
}

/// Write output/<name>.ppm and report the progress on serial.
inline void tgfxShot(const char* name, const uint16_t* px, int w, int h) {
  char path[80];
  snprintf(path, sizeof(path), "output/%s.ppm", name);
  tgfxWritePpm(path, px, w, h);
  Serial.print("SCENE ");
  Serial.println(name);
}
