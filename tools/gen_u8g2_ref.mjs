// u8g2 形式のデコーダを検証するための、参照データを作る。
//
//   出力 1: tests/u8g2/<name>.h        u8g2 のバイト列（C ヘッダ）
//   出力 2: tests/u8g2/<name>.ref.txt  LGFXFontToolJs が描いた期待の絵（テキストアート）
//
// TinyGFX 側のデコーダが同じ絵を出せば、実装が正しいと言える。
// 使い方: node tools/gen_u8g2_ref.mjs
import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const OUT = resolve(HERE, "..", "tests", "u8g2");
const TOOL = resolve(HERE, "..", "..", "LGFXFontToolJs", "dist", "lgfx-font-tool.js");

const m = await import(TOOL);

const CASES = [
  { name: "u8g2_ascii", font: "lgfxJapanGothic_8", text: "0123456789ABCabc" },
  { name: "u8g2_cjk", font: "lgfxJapanGothic_8", text: "日本語表示" },
];

function toCArray(bytes, symbol) {
  const lines = [];
  for (let i = 0; i < bytes.length; i += 12) {
    lines.push("    " + [...bytes.slice(i, i + 12)].map((b) => "0x" + b.toString(16).padStart(2, "0").toUpperCase()).join(", ") + ",");
  }
  return `static const uint8_t ${symbol}[${bytes.length}] TINYGFX_FONT_PROGMEM = {\n${lines.join("\n")}\n};\n`;
}

/// 墨の外接矩形で切り出したテキストアート。原点の流儀の違いを無視して比べるため。
function cropArt(bmp) {
  const { width, height } = bmp;
  const at = (x, y) => m.__getPixel ? m.__getPixel(bmp, x, y) : bmp.data[y * ((width + 7) >> 3) + (x >> 3)] >> (7 - (x & 7)) & 1;
  let x0 = width, y0 = height, x1 = -1, y1 = -1;
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      if (at(x, y)) {
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
      }
    }
  }
  if (x1 < 0) return "";
  const rows = [];
  for (let y = y0; y <= y1; y++) {
    let r = "";
    for (let x = x0; x <= x1; x++) r += at(x, y) ? "#" : ".";
    rows.push(r);
  }
  return rows.join("\n");
}

mkdirSync(OUT, { recursive: true });
const summary = [];

for (const c of CASES) {
  const full = await m.loadFont(c.font);
  const font = m.subset(full, c.text);

  const check = m.canEncodeU8g2 ? m.canEncodeU8g2(font) : { ok: true };
  if (check && check.ok === false) {
    console.error(`${c.name}: u8g2 にできない`, check.issues);
    continue;
  }
  const bytes = m.encodeU8g2(font);

  const w = m.textWidth ? m.textWidth(font, c.text) : 200;
  const h = m.fontHeight(font);
  const bmp = m.createBitmap(w + 4, h + 4, 1);
  m.drawString(bmp, font, c.text, 0, 0);

  // The font headers themselves come from the CLI now (E8 landed in
  // lgfx-font-tool 2.2.2, which added --no-wrapper). What is still needed here
  // is the reference art: the picture LGFXFontToolJs draws for the same font
  // and the same string, which tests/u8g2/ compares the decoder against.
  writeFileSync(resolve(OUT, `${c.name}.ref.txt`), cropArt(bmp) + "\n");
  writeFileSync(resolve(OUT, `${c.name}.json`),
    JSON.stringify({ font: c.font, text: c.text, bytes: bytes.length, width: w, height: h }, null, 2) + "\n");
  summary.push(`${c.name}: ${bytes.length} B  text=${JSON.stringify(c.text)}  ${w}x${h}`);
}

console.log(summary.join("\n"));
