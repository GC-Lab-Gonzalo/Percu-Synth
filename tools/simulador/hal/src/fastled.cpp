// FastLED de la placa virtual: conversión de color (portada de FastLED 3.9),
// utilidades de arreglos, paletas, ruido y el controlador que publica en el SimBus.
#include "FastLED.h"
#include <emscripten.h>
#include <time.h>

uint16_t rand16seed = 1337;
CFastLED FastLED;

// ---------------------------------------------------------------- HSV → RGB
void hsv2rgb_rainbow(const CHSV &hsv, CRGB &rgb) {
  uint8_t hue = hsv.hue, sat = hsv.sat, val = hsv.val;
  uint8_t offset = hue & 0x1F;
  uint8_t offset8 = offset << 3;
  uint8_t third = scale8(offset8, (256 / 3));
  uint8_t r, g, b;
  if (!(hue & 0x80)) {
    if (!(hue & 0x40)) {
      if (!(hue & 0x20)) { r = 255 - third; g = third; b = 0; }
      else { r = 171; g = 85 + third; b = 0; }
    } else {
      if (!(hue & 0x20)) { uint8_t twothirds = scale8(offset8, ((256 * 2) / 3)); r = 171 - twothirds; g = 170 + third; b = 0; }
      else { r = 0; g = 255 - third; b = third; }
    }
  } else {
    if (!(hue & 0x40)) {
      if (!(hue & 0x20)) { r = 0; uint8_t twothirds = scale8(offset8, ((256 * 2) / 3)); g = 171 - twothirds; b = 85 + twothirds; }
      else { r = third; g = 0; b = 255 - third; }
    } else {
      if (!(hue & 0x20)) { r = 85 + third; g = 0; b = 171 - third; }
      else { r = 170 + third; g = 0; b = 85 - third; }
    }
  }
  if (sat != 255) {
    if (sat == 0) { r = 255; b = 255; g = 255; }
    else {
      uint8_t desat = 255 - sat;
      desat = scale8_video(desat, desat);
      uint8_t satscale = 255 - desat;
      r = scale8(r, satscale); g = scale8(g, satscale); b = scale8(b, satscale);
      r += desat; g += desat; b += desat;
    }
  }
  if (val != 255) {
    val = scale8_video(val, val);
    if (val == 0) { r = 0; g = 0; b = 0; }
    else { r = scale8(r, val); g = scale8(g, val); b = scale8(b, val); }
  }
  rgb.r = r; rgb.g = g; rgb.b = b;
}
void hsv2rgb_raw(const CHSV &hsv, CRGB &rgb) {
  uint8_t value = hsv.val, saturation = hsv.sat;
  uint8_t invsat = 255 - saturation;
  uint8_t brightness_floor = (value * invsat) / 256;
  uint8_t color_amplitude = value - brightness_floor;
  uint8_t hue = hsv.hue;            // 0..191 (hsv2rgb_spectrum lo escala)
  uint8_t section = hue / 0x40;
  uint8_t offset = hue % 0x40;
  uint8_t rampup = offset;
  uint8_t rampdown = (0x40 - 1) - offset;
  uint8_t rampup_amp_adj = (rampup * color_amplitude) / (256 / 4);
  uint8_t rampdown_amp_adj = (rampdown * color_amplitude) / (256 / 4);
  uint8_t rampup_adj_with_floor = rampup_amp_adj + brightness_floor;
  uint8_t rampdown_adj_with_floor = rampdown_amp_adj + brightness_floor;
  if (section) {
    if (section == 1) { rgb.r = brightness_floor; rgb.g = rampdown_adj_with_floor; rgb.b = rampup_adj_with_floor; }
    else { rgb.r = rampup_adj_with_floor; rgb.g = brightness_floor; rgb.b = rampdown_adj_with_floor; }
  } else { rgb.r = rampdown_adj_with_floor; rgb.g = rampup_adj_with_floor; rgb.b = brightness_floor; }
}
void hsv2rgb_spectrum(const CHSV &hsv, CRGB &rgb) {
  CHSV h2(hsv); h2.hue = scale8(h2.hue, 191); hsv2rgb_raw(h2, rgb);
}
void hsv2rgb_rainbow(const CHSV *phsv, CRGB *prgb, int n) { for (int i = 0; i < n; i++) hsv2rgb_rainbow(phsv[i], prgb[i]); }
void hsv2rgb_spectrum(const CHSV *phsv, CRGB *prgb, int n) { for (int i = 0; i < n; i++) hsv2rgb_spectrum(phsv[i], prgb[i]); }
CHSV rgb2hsv_approximate(const CRGB &c) {
  float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
  float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn;
  float h = 0;
  if (d > 0) { if (mx == r) h = fmodf((g - b) / d, 6.f); else if (mx == g) h = (b - r) / d + 2; else h = (r - g) / d + 4; }
  if (h < 0) h += 6;
  return CHSV((uint8_t)(h / 6.f * 255.f), (uint8_t)(mx > 0 ? d / mx * 255.f : 0), (uint8_t)(mx * 255.f));
}

// ---------------------------------------------------------------- arreglos
void fill_solid(CRGB *leds, int n, const CRGB &c) { for (int i = 0; i < n; i++) leds[i] = c; }
void fill_solid(CHSV *leds, int n, const CHSV &c) { for (int i = 0; i < n; i++) leds[i] = c; }
void fill_rainbow(CRGB *leds, int n, uint8_t hue, uint8_t delta) {
  CHSV h(hue, 240, 255); for (int i = 0; i < n; i++) { leds[i] = h; h.hue += delta; }
}
void fill_rainbow_circular(CRGB *leds, int n, uint8_t hue, bool reversed) {
  if (n == 0) return; const uint16_t hueChange = 65535 / (uint16_t)n; uint16_t hueOffset = 0;
  CHSV h(hue, 240, 255);
  for (int i = 0; i < n; i++) { leds[i] = h; if (reversed) hueOffset -= hueChange; else hueOffset += hueChange; h.hue = (uint8_t)(hueOffset >> 8) + hue; }
}
void fill_gradient_RGB(CRGB *leds, uint16_t startpos, CRGB startcolor, uint16_t endpos, CRGB endcolor) {
  if (endpos < startpos) { uint16_t t = endpos; endpos = startpos; startpos = t; CRGB tc = endcolor; endcolor = startcolor; startcolor = tc; }
  int32_t rdistance87 = (endcolor.r - startcolor.r) << 7, gdistance87 = (endcolor.g - startcolor.g) << 7, bdistance87 = (endcolor.b - startcolor.b) << 7;
  uint16_t pixeldistance = endpos - startpos; int16_t divisor = pixeldistance ? pixeldistance : 1;
  int32_t rdelta87 = rdistance87 / divisor, gdelta87 = gdistance87 / divisor, bdelta87 = bdistance87 / divisor;
  rdelta87 *= 2; gdelta87 *= 2; bdelta87 *= 2;
  int32_t r88 = startcolor.r << 8, g88 = startcolor.g << 8, b88 = startcolor.b << 8;
  for (uint16_t i = startpos; i <= endpos; ++i) { leds[i] = CRGB(r88 >> 8, g88 >> 8, b88 >> 8); r88 += rdelta87; g88 += gdelta87; b88 += bdelta87; }
}
void fill_gradient_RGB(CRGB *leds, uint16_t n, const CRGB &c1, const CRGB &c2) { if (n) fill_gradient_RGB(leds, 0, c1, n - 1, c2); }
void fill_gradient_RGB(CRGB *leds, uint16_t n, const CRGB &c1, const CRGB &c2, const CRGB &c3) {
  uint16_t half = n / 2, last = n - 1; fill_gradient_RGB(leds, 0, c1, half, c2); fill_gradient_RGB(leds, half, c2, last, c3);
}
void nscale8(CRGB *leds, uint16_t n, uint8_t s) { for (uint16_t i = 0; i < n; i++) leds[i].nscale8(s); }
void nscale8_video(CRGB *leds, uint16_t n, uint8_t s) { for (uint16_t i = 0; i < n; i++) leds[i].nscale8_video(s); }
void fadeToBlackBy(CRGB *leds, uint16_t n, uint8_t f) { nscale8(leds, n, 255 - f); }
void fade_raw(CRGB *leds, uint16_t n, uint8_t f) { nscale8(leds, n, 255 - f); }
void fade_video(CRGB *leds, uint16_t n, uint8_t f) { nscale8_video(leds, n, 255 - f); }
void fadeLightBy(CRGB *leds, uint16_t n, uint8_t f) { nscale8_video(leds, n, 255 - f); }
void fadeUsingColor(CRGB *leds, uint16_t n, const CRGB &m) { for (uint16_t i = 0; i < n; i++) leds[i].nscale8(m); }
CRGB blend(const CRGB &p1, const CRGB &p2, fract8 a) {
  CRGB o = p1; nblend(o, p2, a); return o;
}
CRGB CRGB::blend(const CRGB &a, const CRGB &b, fract8 f) { return ::blend(a, b, f); }
CHSV blend(const CHSV &p1, const CHSV &p2, fract8 a) {
  CHSV o = p1; o.hue = p1.hue + scale8((uint8_t)(p2.hue - p1.hue), a);
  o.sat = lerp8by8(p1.sat, p2.sat, a); o.val = lerp8by8(p1.val, p2.val, a); return o;
}
CRGB &nblend(CRGB &e, const CRGB &o, fract8 a) {
  if (a == 0) return e;
  if (a == 255) { e = o; return e; }
  fract8 a_of_e = 255 - a;
  e.r = scale8(e.r, a_of_e) + scale8(o.r, a);
  e.g = scale8(e.g, a_of_e) + scale8(o.g, a);
  e.b = scale8(e.b, a_of_e) + scale8(o.b, a);
  return e;
}
void nblend(CRGB *e, const CRGB *o, uint16_t n, fract8 a) { for (uint16_t i = 0; i < n; i++) nblend(e[i], o[i], a); }
void blend(const CRGB *s1, const CRGB *s2, CRGB *d, uint16_t n, fract8 a) { for (uint16_t i = 0; i < n; i++) d[i] = blend(s1[i], s2[i], a); }
void blur1d(CRGB *leds, uint16_t n, fract8 blur) {
  uint8_t keep = 255 - blur, seep = blur >> 1; CRGB carryover = CRGB(0, 0, 0);
  for (uint16_t i = 0; i < n; ++i) {
    CRGB cur = leds[i]; CRGB part = cur; part.nscale8(seep); cur.nscale8(keep); cur += carryover;
    if (i) leds[i - 1] += part; leds[i] = cur; carryover = part;
  }
}
void blurRows(CRGB *leds, uint8_t w, uint8_t h, fract8 blur) { for (uint8_t row = 0; row < h; row++) blur1d(leds + row * w, w, blur); }
void blurColumns(CRGB *leds, uint8_t w, uint8_t h, fract8 blur) {
  uint8_t keep = 255 - blur, seep = blur >> 1;
  for (uint8_t col = 0; col < w; ++col) {
    CRGB carryover = CRGB(0, 0, 0);
    for (uint8_t i = 0; i < h; ++i) {
      CRGB cur = leds[i * w + col]; CRGB part = cur; part.nscale8(seep); cur.nscale8(keep); cur += carryover;
      if (i) leds[(i - 1) * w + col] += part; leds[i * w + col] = cur; carryover = part;
    }
  }
}
void blur2d(CRGB *leds, uint8_t w, uint8_t h, fract8 blur) { blurRows(leds, w, h, blur); blurColumns(leds, w, h, blur); }
CRGB HeatColor(uint8_t temperature) {
  CRGB heatcolor; uint8_t t192 = scale8_video(temperature, 191); uint8_t heatramp = t192 & 0x3F; heatramp <<= 2;
  if (t192 & 0x80) { heatcolor.r = 255; heatcolor.g = 255; heatcolor.b = heatramp; }
  else if (t192 & 0x40) { heatcolor.r = 255; heatcolor.g = heatramp; heatcolor.b = 0; }
  else { heatcolor.r = heatramp; heatcolor.g = 0; heatcolor.b = 0; }
  return heatcolor;
}

// ---------------------------------------------------------------- paletas
const TProgmemRGBPalette16 CloudColors_p = { CRGB::Blue, CRGB::DarkBlue, CRGB::DarkBlue, CRGB::DarkBlue, CRGB::DarkBlue, CRGB::DarkBlue,
  CRGB::DarkBlue, CRGB::DarkBlue, CRGB::Blue, CRGB::DarkBlue, CRGB::SkyBlue, CRGB::SkyBlue, CRGB::LightBlue, CRGB::White, CRGB::LightBlue, CRGB::SkyBlue };
const TProgmemRGBPalette16 LavaColors_p = { CRGB::Black, CRGB::Maroon, CRGB::Black, CRGB::Maroon, CRGB::DarkRed, CRGB::DarkRed, CRGB::Maroon,
  CRGB::DarkRed, CRGB::DarkRed, CRGB::DarkRed, CRGB::Red, CRGB::Orange, CRGB::White, CRGB::Orange, CRGB::Red, CRGB::DarkRed };
const TProgmemRGBPalette16 OceanColors_p = { CRGB::MidnightBlue, CRGB::DarkBlue, CRGB::MidnightBlue, CRGB::Navy, CRGB::DarkBlue, CRGB::MediumBlue,
  CRGB::SeaGreen, CRGB::Teal, CRGB::CadetBlue, CRGB::Blue, CRGB::DarkCyan, CRGB::CornflowerBlue, CRGB::Aquamarine, CRGB::SeaGreen, CRGB::Aqua, CRGB::LightSkyBlue };
const TProgmemRGBPalette16 ForestColors_p = { CRGB::DarkGreen, CRGB::DarkGreen, CRGB::DarkOliveGreen, CRGB::DarkGreen, CRGB::Green, CRGB::ForestGreen,
  CRGB::OliveDrab, CRGB::Green, CRGB::SeaGreen, CRGB::MediumAquamarine, CRGB::LimeGreen, CRGB::YellowGreen, CRGB::LightGreen, CRGB::LawnGreen,
  CRGB::MediumAquamarine, CRGB::ForestGreen };
const TProgmemRGBPalette16 RainbowColors_p = { 0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00, 0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
  0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5, 0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B };
const TProgmemRGBPalette16 RainbowStripeColors_p = { 0xFF0000, 0x000000, 0xAB5500, 0x000000, 0xABAB00, 0x000000, 0x00FF00, 0x000000,
  0x00AB55, 0x000000, 0x0000FF, 0x000000, 0x5500AB, 0x000000, 0xAB0055, 0x000000 };
const TProgmemRGBPalette16 PartyColors_p = { 0x5500AB, 0x84007C, 0xB5004B, 0xE5001B, 0xE81700, 0xB84700, 0xAB7700, 0xABAB00,
  0xAB5500, 0xDD2200, 0xF2000E, 0xC2003E, 0x8F0071, 0x5F00A1, 0x2F00D0, 0x0007F9 };
const TProgmemRGBPalette16 HeatColors_p = { 0x000000, 0x330000, 0x660000, 0x990000, 0xCC0000, 0xFF0000, 0xFF3300, 0xFF6600,
  0xFF9900, 0xFFCC00, 0xFFFF00, 0xFFFF33, 0xFFFF66, 0xFFFF99, 0xFFFFCC, 0xFFFFFF };

CRGB ColorFromPalette(const CRGBPalette16 &pal, uint8_t index, uint8_t brightness, TBlendType blendType) {
  if (blendType == LINEARBLEND_NOWRAP) index = map8(index, 0, 239);
  uint8_t hi4 = index >> 4, lo4 = index & 0x0F;
  const CRGB &entry = pal.entries[hi4];
  uint8_t red1 = entry.r, green1 = entry.g, blue1 = entry.b;
  uint8_t blend = lo4 && (blendType != NOBLEND);
  if (blend) {
    const CRGB &e2 = hi4 == 15 ? pal.entries[0] : pal.entries[hi4 + 1];
    uint8_t f2 = lo4 << 4, f1 = 255 - f2;
    red1 = scale8(red1, f1) + scale8(e2.r, f2);
    green1 = scale8(green1, f1) + scale8(e2.g, f2);
    blue1 = scale8(blue1, f1) + scale8(e2.b, f2);
  }
  if (brightness != 255) {
    if (brightness) { ++brightness; if (red1) red1 = scale8(red1, brightness); if (green1) green1 = scale8(green1, brightness); if (blue1) blue1 = scale8(blue1, brightness); }
    else { red1 = 0; green1 = 0; blue1 = 0; }
  }
  return CRGB(red1, green1, blue1);
}
CHSV ColorFromPalette(const CHSVPalette16 &pal, uint8_t index, uint8_t brightness, TBlendType blendType) {
  uint8_t hi4 = index >> 4, lo4 = index & 0x0F;
  CHSV e = pal.entries[hi4];
  if (lo4 && blendType != NOBLEND) e = blend(e, hi4 == 15 ? pal.entries[0] : pal.entries[hi4 + 1], lo4 << 4);
  e.val = scale8(e.val, brightness);
  return e;
}
void nblendPaletteTowardPalette(CRGBPalette16 &cur, CRGBPalette16 &tgt, uint8_t maxChanges) {
  uint8_t changes = 0;
  for (int i = 0; i < 16; i++) for (int k = 0; k < 3; k++) {
    if (changes >= maxChanges) return;
    uint8_t &p1 = cur.entries[i].raw[k]; uint8_t p2 = tgt.entries[i].raw[k];
    if (p1 == p2) continue;
    if (p1 < p2) { ++p1; ++changes; } else { --p1; ++changes; if (p1 > p2) { --p1; } }
  }
}
void fill_palette(CRGB *L, uint16_t N, uint8_t startIndex, uint8_t incIndex, const CRGBPalette16 &pal, uint8_t b, TBlendType t) {
  uint8_t idx = startIndex; for (uint16_t i = 0; i < N; ++i) { L[i] = ColorFromPalette(pal, idx, b, t); idx += incIndex; }
}

// ---------------------------------------------------------------- ruido (Perlin clásico, mismo rango que inoise8)
static const uint8_t PERM[256] = {
  151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
  247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
  74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
  65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
  52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
  119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
  218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,
  184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180 };
static inline int P(int i) { return PERM[i & 255]; }
static inline float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static inline float grad(int h, float x, float y, float z) {
  h &= 15; float u = h < 8 ? x : y, v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
  return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}
static float perlin(float x, float y, float z) {
  int X = (int)floorf(x), Y = (int)floorf(y), Z = (int)floorf(z);
  x -= X; y -= Y; z -= Z;
  float u = fade(x), v = fade(y), w = fade(z);
  int A = P(X) + Y, AA = P(A) + Z, AB = P(A + 1) + Z, B = P(X + 1) + Y, BA = P(B) + Z, BB = P(B + 1) + Z;
  auto L = [](float t, float a, float b) { return a + t * (b - a); };
  return L(w, L(v, L(u, grad(P(AA), x, y, z), grad(P(BA), x - 1, y, z)), L(u, grad(P(AB), x, y - 1, z), grad(P(BB), x - 1, y - 1, z))),
              L(v, L(u, grad(P(AA + 1), x, y, z - 1), grad(P(BA + 1), x - 1, y, z - 1)), L(u, grad(P(AB + 1), x, y - 1, z - 1), grad(P(BB + 1), x - 1, y - 1, z - 1))));
}
static uint8_t a8(float n) { int v = (int)(128 + n * 127 * 1.1f); return v < 0 ? 0 : (v > 255 ? 255 : v); }
uint8_t inoise8(uint16_t x, uint16_t y, uint16_t z) { return a8(perlin(x / 256.f, y / 256.f, z / 256.f)); }
uint8_t inoise8(uint16_t x, uint16_t y) { return a8(perlin(x / 256.f, y / 256.f, 0.5f)); }
uint8_t inoise8(uint16_t x) { return a8(perlin(x / 256.f, 0.5f, 0.5f)); }
int8_t inoise8_raw(uint16_t x, uint16_t y, uint16_t z) { return (int8_t)(perlin(x / 256.f, y / 256.f, z / 256.f) * 64); }
static uint16_t a16(float n) { int v = (int)(32768 + n * 32767 * 1.1f); return v < 0 ? 0 : (v > 65535 ? 65535 : v); }
uint16_t inoise16(uint32_t x, uint32_t y, uint32_t z) { return a16(perlin(x / 65536.f, y / 65536.f, z / 65536.f)); }
uint16_t inoise16(uint32_t x, uint32_t y) { return a16(perlin(x / 65536.f, y / 65536.f, 0.5f)); }
uint16_t inoise16(uint32_t x) { return a16(perlin(x / 65536.f, 0.5f, 0.5f)); }

// ---------------------------------------------------------------- controladores → SimBus
CLEDController *CFastLED::add(CRGB *data, int n, int pin, int order, int offset) {
  CLEDController *c = new CLEDController();
  c->data = data + offset; c->count = n; c->pin = pin; c->order = order;
  if (!first) first = c; else { CLEDController *p = first; while (p->next) p = p->next; p->next = c; }
  return c;
}

static double g_ultimo_show = 0;
void CFastLED::show(uint8_t scale) {
  SimBus *bus = sim_bus();
  // límite de potencia (misma fórmula que FastLED: mW por canal a 5 V)
  if (maxPower_mW) {
    uint32_t total = 0, nleds = 0;
    for (CLEDController *p = first; p; p = p->next) {
      for (int i = 0; i < p->count; i++) { const CRGB &c = p->data[i]; total += (c.r * 80u + c.g * 55u + c.b * 75u) >> 8; }
      nleds += p->count;
    }
    total += nleds * 5;
    uint32_t pedido = (total * scale) / 256;
    if (pedido > maxPower_mW && pedido) scale = (uint8_t)((uint32_t)scale * maxPower_mW / pedido);
  }
  // refresco máximo (FastLED limita a 400 Hz por defecto)
  if (maxRefresh) {
    double min_ms = 1000.0 / maxRefresh, t = emscripten_get_now();
    double falta = g_ultimo_show + min_ms - t;
    if (falta > 0) { struct timespec ts = { 0, (long)(falta * 1e6) }; nanosleep(&ts, nullptr); }
  }
  int idx46 = 0, total46 = 0;
  for (CLEDController *p = first; p; p = p->next) {
    CRGB adj(scale8(p->correction.r, p->temperature.r), scale8(p->correction.g, p->temperature.g), scale8(p->correction.b, p->temperature.b));
    for (int i = 0; i < p->count; i++) {
      const CRGB &c = p->data[i];
      uint8_t v[3] = { scale8(scale8(c.r, adj.r), scale), scale8(scale8(c.g, adj.g), scale), scale8(scale8(c.b, adj.b), scale) };
      // bytes en el cable, en el orden declarado...
      uint8_t b0 = v[(p->order >> 6) & 3], b1 = v[(p->order >> 3) & 3], b2 = v[p->order & 3];
      // ...y como los lee una WS2812B real (G, R, B)
      uint8_t R = b1, G = b0, B = b2;
      if (p->pin == 48) {
        if (i == 0) { bus->rgb48[0] = R; bus->rgb48[1] = G; bus->rgb48[2] = B; __atomic_add_fetch(&bus->rgb48_seq, 1, __ATOMIC_RELEASE); }
      } else if (p->pin == 46 && idx46 < SIM_MAX_LEDS) {
        bus->led_rgb[idx46 * 3] = R; bus->led_rgb[idx46 * 3 + 1] = G; bus->led_rgb[idx46 * 3 + 2] = B; idx46++;
      }
    }
    if (p->pin == 46) total46 += p->count;
  }
  bus->led_count = total46 < SIM_MAX_LEDS ? total46 : SIM_MAX_LEDS;
  __atomic_add_fetch(&bus->led_seq, 1, __ATOMIC_RELEASE);
  // mandar N LEDs por el cable toma ~30 µs cada uno; FastLED espera a que termine
  if (total46 > 0) { double us = total46 * 30.0; struct timespec ts = { 0, (long)(us * 1000) }; nanosleep(&ts, nullptr); }
  g_ultimo_show = emscripten_get_now();
}
void CFastLED::clear(bool writeData) {
  for (CLEDController *p = first; p; p = p->next) p->clearLedData();
  if (writeData) show(0);
}
void CFastLED::showColor(const CRGB &c, uint8_t scale) {
  for (CLEDController *p = first; p; p = p->next) for (int i = 0; i < p->count; i++) p->data[i] = c;
  show(scale);
}
void CFastLED::delay(unsigned long ms) {
  unsigned long fin = millis() + ms;
  do { show(); } while (millis() < fin);
}
void CLEDController::showLeds(uint8_t brightness) { (void)brightness; FastLED.show(); }
