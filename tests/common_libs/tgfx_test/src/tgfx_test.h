// TinyGFX テストスケッチの共通部品。ライブラリ本体には含まれない。
//
// 生成物は output/ に書く:
//   output/<name>.ppm    描いた結果（P6。RGB565 を展開するだけで済むので PNG 不要）
//   output/report.txt    key=value を 1 行ずつ
//
// 値をシリアルではなくファイルに出しているのは、dut.expect の取りこぼしで
// テストが不安定になるのを避けるため。シリアルには進行状況だけ流す。
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

/// key=value を report.txt に 1 行書く。シリアルにも出す（ログ用）。
inline void tgfxReport(const char* key, long value) {
  FILE* f = tgfxReportFile();
  if (f != nullptr) fprintf(f, "%s=%ld\n", key, value);
  Serial.print(key);
  Serial.print('=');
  Serial.println(value);
}

/// key=value を接頭辞つきで書く（scene ごとの値など）。
inline void tgfxReport2(const char* prefix, const char* key, long value) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s_%s", prefix, key);
  tgfxReport(buf, value);
}

/// RGB565 のバッファを PPM (P6) で書き出す。
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

/// output/<name>.ppm へ書き、進行をシリアルに出す。
inline void tgfxShot(const char* name, const uint16_t* px, int w, int h) {
  char path[80];
  snprintf(path, sizeof(path), "output/%s.ppm", name);
  tgfxWritePpm(path, px, w, h);
  Serial.print("SCENE ");
  Serial.println(name);
}
