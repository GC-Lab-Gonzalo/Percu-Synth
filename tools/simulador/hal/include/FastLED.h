// =============================================================================
// FastLED.h de la placa virtual — la parte de FastLED que usan los sketches del
// PercuSynth (y la que suele escribir una IA): CRGB, CHSV, lib8tion, paletas,
// ruido, beatsin, EVERY_N_* y el controlador WS2812 en el GPIO 46.
//
// La conversión HSV→RGB "rainbow", sin8/sin16, random8/16 y scale8 están
// portadas de FastLED 3.9 para que los colores salgan igual que en la placa.
// show() manda los colores al SimBus en el orden de bytes declarado y los
// reinterpreta como los lee una WS2812B real (GRB): si el sketch declara RGB,
// rojo y verde salen cambiados, igual que en el hardware.
// =============================================================================
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "Arduino.h"

#define FASTLED_VERSION 3009013
#define FASTLED_SIM 1
#define FL_PROGMEM
#define FASTLED_FORCE_SOFTWARE_SPI
#define FASTLED_USING_NAMESPACE
#define FASTLED_NAMESPACE_BEGIN
#define FASTLED_NAMESPACE_END
#define NUM_LEDS_UNUSED

typedef uint8_t  fract8;
typedef uint16_t fract16;
typedef uint16_t accum88;
typedef int8_t   sfract7;

// ---------------------------------------------------------------- lib8tion
static inline uint8_t scale8(uint8_t i, fract8 scale) { return (uint8_t)(((uint16_t)i * (1 + (uint16_t)scale)) >> 8); }
static inline uint8_t scale8_LEAVING_R1_DIRTY(uint8_t i, fract8 scale) { return scale8(i, scale); }
static inline uint8_t scale8_video(uint8_t i, fract8 scale) { return (uint8_t)((((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0)); }
static inline uint8_t scale8_video_LEAVING_R1_DIRTY(uint8_t i, fract8 scale) { return scale8_video(i, scale); }
static inline void cleanup_R1() {}
static inline uint16_t scale16(uint16_t i, fract16 scale) { return (uint16_t)(((uint32_t)i * (1 + (uint32_t)scale)) >> 16); }
static inline uint16_t scale16by8(uint16_t i, fract8 scale) { return (uint16_t)((i * (1 + (uint32_t)scale)) >> 8); }
static inline void nscale8x3(uint8_t &r, uint8_t &g, uint8_t &b, fract8 s) { r = scale8(r, s); g = scale8(g, s); b = scale8(b, s); }
static inline void nscale8x3_video(uint8_t &r, uint8_t &g, uint8_t &b, fract8 s) { r = scale8_video(r, s); g = scale8_video(g, s); b = scale8_video(b, s); }
static inline uint8_t qadd8(uint8_t i, uint8_t j) { unsigned t = i + j; return t > 255 ? 255 : (uint8_t)t; }
static inline int8_t  qadd7(int8_t i, int8_t j) { int t = i + j; return t > 127 ? 127 : (t < -128 ? -128 : (int8_t)t); }
static inline uint8_t qsub8(uint8_t i, uint8_t j) { int t = i - j; return t < 0 ? 0 : (uint8_t)t; }
static inline uint8_t qmul8(uint8_t i, uint8_t j) { unsigned p = (unsigned)i * j; return p > 255 ? 255 : (uint8_t)p; }
static inline uint8_t add8(uint8_t i, uint8_t j) { return (uint8_t)(i + j); }
static inline uint8_t sub8(uint8_t i, uint8_t j) { return (uint8_t)(i - j); }
static inline uint8_t mul8(uint8_t i, uint8_t j) { return (uint8_t)(i * j); }
static inline uint8_t avg8(uint8_t i, uint8_t j) { return (uint8_t)((i + j) >> 1); }
static inline uint16_t avg16(uint16_t i, uint16_t j) { return (uint16_t)(((uint32_t)i + j) >> 1); }
static inline uint8_t abs8(int8_t i) { return (uint8_t)(i < 0 ? -i : i); }
static inline uint8_t addmod8(uint8_t a, uint8_t b, uint8_t m) { a += b; while (a >= m) a -= m; return a; }
static inline uint8_t submod8(uint8_t a, uint8_t b, uint8_t m) { a -= b; while (a >= m) a -= m; return a; }
static inline uint8_t mod8(uint8_t a, uint8_t m) { while (a >= m) a -= m; return a; }
static inline uint8_t lerp8by8(uint8_t a, uint8_t b, fract8 f) {
  if (b > a) return a + scale8(b - a, f); return a - scale8(a - b, f); }
static inline uint16_t lerp16by16(uint16_t a, uint16_t b, fract16 f) {
  if (b > a) return a + scale16(b - a, f); return a - scale16(a - b, f); }
static inline uint8_t map8(uint8_t in, uint8_t lo, uint8_t hi) { return lo + scale8(in, hi - lo); }
static inline uint8_t dim8_raw(uint8_t x) { return scale8(x, x); }
static inline uint8_t dim8_video(uint8_t x) { return scale8_video(x, x); }
static inline uint8_t dim8_lin(uint8_t x) { if (x & 0x80) return scale8(x, x); x += 1; x /= 2; return x; }
static inline uint8_t brighten8_raw(uint8_t x) { uint8_t ix = 255 - x; return 255 - scale8(ix, ix); }
static inline uint8_t brighten8_video(uint8_t x) { uint8_t ix = 255 - x; return 255 - scale8_video(ix, ix); }
static inline uint8_t brighten8_lin(uint8_t x) { uint8_t ix = 255 - x; return 255 - dim8_lin(ix); }
static inline uint8_t ease8InOutQuad(uint8_t i) {
  uint8_t j = i; if (j & 0x80) j = 255 - j; uint8_t jj = scale8(j, j); uint8_t jj2 = jj << 1; if (i & 0x80) jj2 = 255 - jj2; return jj2; }
static inline uint8_t ease8InOutCubic(fract8 i) {
  uint8_t ii = scale8_LEAVING_R1_DIRTY(i, i); uint8_t iii = scale8_LEAVING_R1_DIRTY(ii, i);
  uint16_t r1 = (3 * (uint16_t)ii) - (2 * (uint16_t)iii); uint8_t result = r1; if (r1 & 0x100) result = 255; return result; }
static inline uint8_t ease8InOutApprox(fract8 i) {
  if (i < 64) i /= 2; else if (i > (255 - 64)) { i = 255 - i; i /= 2; i = 255 - i; } else { i -= 64; i += (i / 2); i += 32; } return i; }
static inline uint8_t triwave8(uint8_t in) { if (in & 0x80) in = 255 - in; return in << 1; }
static inline uint8_t quadwave8(uint8_t in) { return ease8InOutQuad(triwave8(in)); }
static inline uint8_t cubicwave8(uint8_t in) { return ease8InOutCubic(triwave8(in)); }
static inline uint8_t squarewave8(uint8_t in, uint8_t pulsewidth = 128) { return in < pulsewidth ? 255 : 0; }
static inline uint16_t sqrt16(uint16_t x) { return (uint16_t)sqrtf((float)x); }

static inline uint8_t sin8(uint8_t theta) {
  static const uint8_t b_m16_interleave[] = { 0, 49, 49, 41, 90, 27, 117, 10 };
  uint8_t offset = theta;
  if (theta & 0x40) offset = (uint8_t)255 - offset;
  offset &= 0x3F;
  uint8_t secoffset = offset & 0x0F;
  if (theta & 0x40) ++secoffset;
  uint8_t section = offset >> 4;
  const uint8_t *p = b_m16_interleave + section * 2;
  uint8_t b = p[0], m16 = p[1];
  uint8_t mx = (m16 * secoffset) >> 4;
  int8_t y = mx + b;
  if (theta & 0x80) y = -y;
  y += 128;
  return (uint8_t)y;
}
static inline uint8_t cos8(uint8_t theta) { return sin8(theta + 64); }
static inline int16_t sin16(uint16_t theta) {
  static const uint16_t base[] = { 0, 6393, 12539, 18204, 23170, 26956, 29621, 31197 };
  static const uint8_t slope[] = { 49, 48, 44, 38, 31, 23, 14, 4 };
  uint16_t offset = (theta & 0x3FFF) >> 3;
  if (theta & 0x4000) offset = 2047 - offset;
  uint8_t section = offset / 256;
  uint16_t b = base[section];
  uint8_t m = slope[section];
  uint8_t secoffset8 = (uint8_t)(offset) / 2;
  uint16_t mx = m * secoffset8;
  int16_t y = mx + b;
  if (theta & 0x8000) y = -y;
  return y;
}
static inline int16_t cos16(uint16_t theta) { return sin16(theta + 16384); }

// generador pseudoaleatorio de FastLED (el mismo LCG)
extern uint16_t rand16seed;
static inline uint8_t random8() { rand16seed = (rand16seed * 2053) + 13849; return (uint8_t)(((uint8_t)(rand16seed & 0xFF)) + ((uint8_t)(rand16seed >> 8))); }
static inline uint8_t random8(uint8_t lim) { uint8_t r = random8(); r = (r * lim) >> 8; return r; }
static inline uint8_t random8(uint8_t mn, uint8_t lim) { uint8_t delta = lim - mn; return random8(delta) + mn; }
static inline uint16_t random16() { rand16seed = (rand16seed * 2053) + 13849; return rand16seed; }
static inline uint16_t random16(uint16_t lim) { uint16_t r = random16(); uint32_t p = (uint32_t)lim * (uint32_t)r; return p >> 16; }
static inline uint16_t random16(uint16_t mn, uint16_t lim) { uint16_t delta = lim - mn; return random16(delta) + mn; }
static inline void random16_set_seed(uint16_t s) { rand16seed = s; }
static inline uint16_t random16_get_seed() { return rand16seed; }
static inline void random16_add_entropy(uint16_t e) { rand16seed += e; }

// tiempo y ondas sincronizadas
static inline uint32_t GET_MILLIS() { return millis(); }
static inline uint16_t beat88(accum88 bpm88, uint32_t timebase = 0) { return (uint16_t)(((millis() - timebase) * bpm88 * 280) >> 16); }
static inline uint16_t beat16(accum88 bpm, uint32_t timebase = 0) { if (bpm < 256) bpm <<= 8; return beat88(bpm, timebase); }
static inline uint8_t beat8(accum88 bpm, uint32_t timebase = 0) { return beat16(bpm, timebase) >> 8; }
static inline uint16_t beatsin88(accum88 bpm88, uint16_t lo = 0, uint16_t hi = 65535, uint32_t timebase = 0, uint16_t phase = 0) {
  uint16_t beat = beat88(bpm88, timebase); uint16_t beatsin = (sin16(beat + phase) + 32768);
  uint16_t rangewidth = hi - lo; uint16_t scaledbeat = scale16(beatsin, rangewidth); return lo + scaledbeat; }
static inline uint16_t beatsin16(accum88 bpm, uint16_t lo = 0, uint16_t hi = 65535, uint32_t timebase = 0, uint16_t phase = 0) {
  uint16_t beat = beat16(bpm, timebase); uint16_t beatsin = (sin16(beat + phase) + 32768);
  uint16_t rangewidth = hi - lo; uint16_t scaledbeat = scale16(beatsin, rangewidth); return lo + scaledbeat; }
static inline uint8_t beatsin8(accum88 bpm, uint8_t lo = 0, uint8_t hi = 255, uint32_t timebase = 0, uint8_t phase = 0) {
  uint8_t beat = beat8(bpm, timebase); uint8_t beatsin = sin8(beat + phase);
  uint8_t rangewidth = hi - lo; uint8_t scaledbeat = scale8(beatsin, rangewidth); return lo + scaledbeat; }
static inline uint16_t seconds16() { return (uint16_t)(millis() / 1000); }
static inline uint16_t minutes16() { return (uint16_t)(millis() / 60000); }

class CEveryNMillis {
public:
  uint32_t prev, period;
  CEveryNMillis(uint32_t p) : prev(millis()), period(p) {}
  bool ready() { uint32_t now = millis(); if (now - prev >= period) { prev = now; return true; } return false; }
  void reset() { prev = millis(); }
  void setPeriod(uint32_t p) { period = p; }
  operator bool() { return ready(); }
};
#define FL_CONCAT_(a, b) a##b
#define FL_CONCAT(a, b) FL_CONCAT_(a, b)
#define EVERY_N_MILLISECONDS(N) static CEveryNMillis FL_CONCAT(_every_, __LINE__)(N); if (FL_CONCAT(_every_, __LINE__))
#define EVERY_N_MILLIS(N) EVERY_N_MILLISECONDS(N)
#define EVERY_N_SECONDS(N) EVERY_N_MILLISECONDS((N) * 1000UL)
#define EVERY_N_MINUTES(N) EVERY_N_MILLISECONDS((N) * 60000UL)
#define EVERY_N_MILLISECONDS_I(NAME, N) static CEveryNMillis NAME(N); if (NAME)

// ---------------------------------------------------------------- colores
struct CRGB;
struct CHSV {
  union { struct { union { uint8_t hue; uint8_t h; }; union { uint8_t sat; uint8_t s; }; union { uint8_t val; uint8_t v; }; }; uint8_t raw[3]; };
  CHSV() : hue(0), sat(0), val(0) {}
  CHSV(uint8_t h_, uint8_t s_, uint8_t v_) : hue(h_), sat(s_), val(v_) {}
  uint8_t &operator[](uint8_t x) { return raw[x]; }
  CHSV &setHSV(uint8_t h_, uint8_t s_, uint8_t v_) { hue = h_; sat = s_; val = v_; return *this; }
};
typedef enum { HUE_RED = 0, HUE_ORANGE = 32, HUE_YELLOW = 64, HUE_GREEN = 96, HUE_AQUA = 128, HUE_BLUE = 160, HUE_PURPLE = 192, HUE_PINK = 224 } HSVHue;

void hsv2rgb_rainbow(const CHSV &hsv, CRGB &rgb);
void hsv2rgb_spectrum(const CHSV &hsv, CRGB &rgb);
void hsv2rgb_raw(const CHSV &hsv, CRGB &rgb);
void hsv2rgb_rainbow(const CHSV *phsv, CRGB *prgb, int n);
void hsv2rgb_spectrum(const CHSV *phsv, CRGB *prgb, int n);
CHSV rgb2hsv_approximate(const CRGB &rgb);

struct CRGB {
  union {
    struct { union { uint8_t r; uint8_t red; }; union { uint8_t g; uint8_t green; }; union { uint8_t b; uint8_t blue; }; };
    uint8_t raw[3];
  };
  CRGB() : r(0), g(0), b(0) {}
  constexpr CRGB(uint8_t ir, uint8_t ig, uint8_t ib) : r(ir), g(ig), b(ib) {}
  constexpr CRGB(uint32_t c) : r((c >> 16) & 0xFF), g((c >> 8) & 0xFF), b(c & 0xFF) {}
  CRGB(const CHSV &h) { hsv2rgb_rainbow(h, *this); }
  CRGB(const CRGB &) = default;
  CRGB &operator=(const CRGB &) = default;
  CRGB &operator=(const CHSV &h) { hsv2rgb_rainbow(h, *this); return *this; }
  CRGB &operator=(uint32_t c) { r = (c >> 16) & 0xFF; g = (c >> 8) & 0xFF; b = c & 0xFF; return *this; }
  uint8_t &operator[](uint8_t x) { return raw[x]; }
  const uint8_t &operator[](uint8_t x) const { return raw[x]; }
  CRGB &setRGB(uint8_t nr, uint8_t ng, uint8_t nb) { r = nr; g = ng; b = nb; return *this; }
  CRGB &setHSV(uint8_t h, uint8_t s, uint8_t v) { hsv2rgb_rainbow(CHSV(h, s, v), *this); return *this; }
  CRGB &setHue(uint8_t h) { hsv2rgb_rainbow(CHSV(h, 255, 255), *this); return *this; }
  CRGB &setColorCode(uint32_t c) { return *this = c; }
  CRGB &operator+=(const CRGB &o) { r = qadd8(r, o.r); g = qadd8(g, o.g); b = qadd8(b, o.b); return *this; }
  CRGB &addToRGB(uint8_t d) { r = qadd8(r, d); g = qadd8(g, d); b = qadd8(b, d); return *this; }
  CRGB &operator-=(const CRGB &o) { r = qsub8(r, o.r); g = qsub8(g, o.g); b = qsub8(b, o.b); return *this; }
  CRGB &subtractFromRGB(uint8_t d) { r = qsub8(r, d); g = qsub8(g, d); b = qsub8(b, d); return *this; }
  CRGB &operator++() { return addToRGB(1); }
  CRGB operator++(int) { CRGB t = *this; addToRGB(1); return t; }
  CRGB &operator--() { return subtractFromRGB(1); }
  CRGB operator--(int) { CRGB t = *this; subtractFromRGB(1); return t; }
  CRGB &operator/=(uint8_t d) { r /= d; g /= d; b /= d; return *this; }
  CRGB &operator>>=(uint8_t d) { r >>= d; g >>= d; b >>= d; return *this; }
  CRGB &operator*=(uint8_t d) { r = qmul8(r, d); g = qmul8(g, d); b = qmul8(b, d); return *this; }
  CRGB &nscale8_video(uint8_t s) { nscale8x3_video(r, g, b, s); return *this; }
  CRGB &operator%=(uint8_t s) { return nscale8_video(s); }
  CRGB &fadeLightBy(uint8_t f) { return nscale8_video(255 - f); }
  CRGB &nscale8(uint8_t s) { nscale8x3(r, g, b, s); return *this; }
  CRGB &nscale8(const CRGB &s) { r = ::scale8(r, s.r); g = ::scale8(g, s.g); b = ::scale8(b, s.b); return *this; }
  CRGB scale8(uint8_t s) const { CRGB o = *this; nscale8x3(o.r, o.g, o.b, s); return o; }
  CRGB scale8(const CRGB &s) const { CRGB o = *this; o.nscale8(s); return o; }
  CRGB &fadeToBlackBy(uint8_t f) { return nscale8(255 - f); }
  CRGB &operator|=(const CRGB &o) { if (o.r > r) r = o.r; if (o.g > g) g = o.g; if (o.b > b) b = o.b; return *this; }
  CRGB &operator|=(uint8_t d) { if (d > r) r = d; if (d > g) g = d; if (d > b) b = d; return *this; }
  CRGB &operator&=(const CRGB &o) { if (o.r < r) r = o.r; if (o.g < g) g = o.g; if (o.b < b) b = o.b; return *this; }
  CRGB &operator&=(uint8_t d) { if (d < r) r = d; if (d < g) g = d; if (d < b) b = d; return *this; }
  explicit operator bool() const { return r || g || b; }
  explicit operator uint32_t() const { return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b; }
  CRGB operator-() const { return CRGB(255 - r, 255 - g, 255 - b); }
  uint8_t getLuminance() const { return ::scale8(r, 54) + ::scale8(g, 183) + ::scale8(b, 18); }
  uint8_t getAverageLight() const { return ::scale8(r, 85) + ::scale8(g, 85) + ::scale8(b, 85); }
  CRGB &maximizeBrightness(uint8_t limit = 255) {
    uint8_t m = r; if (g > m) m = g; if (b > m) m = b; if (!m) return *this;
    uint16_t f = ((uint16_t)limit * 256) / m; r = (r * f) / 256; g = (g * f) / 256; b = (b * f) / 256; return *this; }
  CRGB lerp8(const CRGB &o, fract8 f) const { return CRGB(lerp8by8(r, o.r, f), lerp8by8(g, o.g, f), lerp8by8(b, o.b, f)); }
  CRGB lerp16(const CRGB &o, fract16 f) const { return lerp8(o, f >> 8); }
  static CRGB blend(const CRGB &a, const CRGB &b, fract8 f);
  static CRGB computeAdjustment(uint8_t, const CRGB &c, const CRGB &) { return c; }

  typedef enum {
    AliceBlue = 0xF0F8FF, Amethyst = 0x9966CC, AntiqueWhite = 0xFAEBD7, Aqua = 0x00FFFF, Aquamarine = 0x7FFFD4,
    Azure = 0xF0FFFF, Beige = 0xF5F5DC, Bisque = 0xFFE4C4, Black = 0x000000, BlanchedAlmond = 0xFFEBCD,
    Blue = 0x0000FF, BlueViolet = 0x8A2BE2, Brown = 0xA52A2A, BurlyWood = 0xDEB887, CadetBlue = 0x5F9EA0,
    Chartreuse = 0x7FFF00, Chocolate = 0xD2691E, Coral = 0xFF7F50, CornflowerBlue = 0x6495ED, Cornsilk = 0xFFF8DC,
    Crimson = 0xDC143C, Cyan = 0x00FFFF, DarkBlue = 0x00008B, DarkCyan = 0x008B8B, DarkGoldenrod = 0xB8860B,
    DarkGray = 0xA9A9A9, DarkGrey = 0xA9A9A9, DarkGreen = 0x006400, DarkKhaki = 0xBDB76B, DarkMagenta = 0x8B008B,
    DarkOliveGreen = 0x556B2F, DarkOrange = 0xFF8C00, DarkOrchid = 0x9932CC, DarkRed = 0x8B0000, DarkSalmon = 0xE9967A,
    DarkSeaGreen = 0x8FBC8F, DarkSlateBlue = 0x483D8B, DarkSlateGray = 0x2F4F4F, DarkSlateGrey = 0x2F4F4F,
    DarkTurquoise = 0x00CED1, DarkViolet = 0x9400D3, DeepPink = 0xFF1493, DeepSkyBlue = 0x00BFFF, DimGray = 0x696969,
    DimGrey = 0x696969, DodgerBlue = 0x1E90FF, FireBrick = 0xB22222, FloralWhite = 0xFFFAF0, ForestGreen = 0x228B22,
    Fuchsia = 0xFF00FF, Gainsboro = 0xDCDCDC, GhostWhite = 0xF8F8FF, Gold = 0xFFD700, Goldenrod = 0xDAA520,
    Gray = 0x808080, Grey = 0x808080, Green = 0x008000, GreenYellow = 0xADFF2F, Honeydew = 0xF0FFF0,
    HotPink = 0xFF69B4, IndianRed = 0xCD5C5C, Indigo = 0x4B0082, Ivory = 0xFFFFF0, Khaki = 0xF0E68C,
    Lavender = 0xE6E6FA, LavenderBlush = 0xFFF0F5, LawnGreen = 0x7CFC00, LemonChiffon = 0xFFFACD, LightBlue = 0xADD8E6,
    LightCoral = 0xF08080, LightCyan = 0xE0FFFF, LightGoldenrodYellow = 0xFAFAD2, LightGreen = 0x90EE90,
    LightGrey = 0xD3D3D3, LightPink = 0xFFB6C1, LightSalmon = 0xFFA07A, LightSeaGreen = 0x20B2AA,
    LightSkyBlue = 0x87CEFA, LightSlateGray = 0x778899, LightSlateGrey = 0x778899, LightSteelBlue = 0xB0C4DE,
    LightYellow = 0xFFFFE0, Lime = 0x00FF00, LimeGreen = 0x32CD32, Linen = 0xFAF0E6, Magenta = 0xFF00FF,
    Maroon = 0x800000, MediumAquamarine = 0x66CDAA, MediumBlue = 0x0000CD, MediumOrchid = 0xBA55D3,
    MediumPurple = 0x9370DB, MediumSeaGreen = 0x3CB371, MediumSlateBlue = 0x7B68EE, MediumSpringGreen = 0x00FA9A,
    MediumTurquoise = 0x48D1CC, MediumVioletRed = 0xC71585, MidnightBlue = 0x191970, MintCream = 0xF5FFFA,
    MistyRose = 0xFFE4E1, Moccasin = 0xFFE4B5, NavajoWhite = 0xFFDEAD, Navy = 0x000080, OldLace = 0xFDF5E6,
    Olive = 0x808000, OliveDrab = 0x6B8E23, Orange = 0xFFA500, OrangeRed = 0xFF4500, Orchid = 0xDA70D6,
    PaleGoldenrod = 0xEEE8AA, PaleGreen = 0x98FB98, PaleTurquoise = 0xAFEEEE, PaleVioletRed = 0xDB7093,
    PapayaWhip = 0xFFEFD5, PeachPuff = 0xFFDAB9, Peru = 0xCD853F, Pink = 0xFFC0CB, Plaid = 0xCC5533,
    Plum = 0xDDA0DD, PowderBlue = 0xB0E0E6, Purple = 0x800080, Red = 0xFF0000, RosyBrown = 0xBC8F8F,
    RoyalBlue = 0x4169E1, SaddleBrown = 0x8B4513, Salmon = 0xFA8072, SandyBrown = 0xF4A460, SeaGreen = 0x2E8B57,
    Seashell = 0xFFF5EE, Sienna = 0xA0522D, Silver = 0xC0C0C0, SkyBlue = 0x87CEEB, SlateBlue = 0x6A5ACD,
    SlateGray = 0x708090, SlateGrey = 0x708090, Snow = 0xFFFAFA, SpringGreen = 0x00FF7F, SteelBlue = 0x4682B4,
    Tan = 0xD2B48C, Teal = 0x008080, Thistle = 0xD8BFD8, Tomato = 0xFF6347, Turquoise = 0x40E0D0,
    Violet = 0xEE82EE, Wheat = 0xF5DEB3, White = 0xFFFFFF, WhiteSmoke = 0xF5F5F5, Yellow = 0xFFFF00,
    YellowGreen = 0x9ACD32, FairyLight = 0xFFE42D, FairyLightNCC = 0xFF9D2A
  } HTMLColorCode;
};

inline bool operator==(const CRGB &a, const CRGB &b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
inline bool operator!=(const CRGB &a, const CRGB &b) { return !(a == b); }
inline CRGB operator+(const CRGB &a, const CRGB &b) { return CRGB(qadd8(a.r, b.r), qadd8(a.g, b.g), qadd8(a.b, b.b)); }
inline CRGB operator-(const CRGB &a, const CRGB &b) { return CRGB(qsub8(a.r, b.r), qsub8(a.g, b.g), qsub8(a.b, b.b)); }
inline CRGB operator*(const CRGB &a, uint8_t d) { return CRGB(qmul8(a.r, d), qmul8(a.g, d), qmul8(a.b, d)); }
inline CRGB operator/(const CRGB &a, uint8_t d) { return CRGB(a.r / d, a.g / d, a.b / d); }
inline CRGB operator&(const CRGB &a, const CRGB &b) { return CRGB(a.r < b.r ? a.r : b.r, a.g < b.g ? a.g : b.g, a.b < b.b ? a.b : b.b); }
inline CRGB operator|(const CRGB &a, const CRGB &b) { return CRGB(a.r > b.r ? a.r : b.r, a.g > b.g ? a.g : b.g, a.b > b.b ? a.b : b.b); }
inline CRGB operator%(const CRGB &a, uint8_t s) { CRGB o = a; o.nscale8_video(s); return o; }

typedef enum { RGB = 0012, RBG = 0021, GRB = 0102, GBR = 0120, BRG = 0201, BGR = 0210 } EOrder;
typedef enum { TypicalSMD5050 = 0xFFB0F0, TypicalLEDStrip = 0xFFB0F0, Typical8mmPixel = 0xFFE08C,
               TypicalPixelString = 0xFFE08C, UncorrectedColor = 0xFFFFFF } LEDColorCorrection;
typedef enum { Candle = 0xFF9329, Tungsten40W = 0xFFC58F, Tungsten100W = 0xFFD6AA, Halogen = 0xFFF1E0,
               CarbonArc = 0xFFFAF4, HighNoonSun = 0xFFFFFB, DirectSunlight = 0xFFFFFF, OvercastSky = 0xC9E2FF,
               ClearBlueSky = 0x409CFF, UncorrectedTemperature = 0xFFFFFF } ColorTemperature;
#define DISABLE_DITHER 0x00
#define BINARY_DITHER 0x01

// ---------------------------------------------------------------- utilidades de arreglos
void fill_solid(CRGB *leds, int n, const CRGB &c);
void fill_solid(CHSV *leds, int n, const CHSV &c);
void fill_rainbow(CRGB *leds, int n, uint8_t initialhue, uint8_t deltahue = 5);
void fill_rainbow_circular(CRGB *leds, int n, uint8_t initialhue, bool reversed = false);
void fill_gradient_RGB(CRGB *leds, uint16_t startpos, CRGB startcolor, uint16_t endpos, CRGB endcolor);
void fill_gradient_RGB(CRGB *leds, uint16_t n, const CRGB &c1, const CRGB &c2);
void fill_gradient_RGB(CRGB *leds, uint16_t n, const CRGB &c1, const CRGB &c2, const CRGB &c3);
void fadeToBlackBy(CRGB *leds, uint16_t n, uint8_t fade);
void fade_video(CRGB *leds, uint16_t n, uint8_t fade);
void fade_raw(CRGB *leds, uint16_t n, uint8_t fade);
void fadeLightBy(CRGB *leds, uint16_t n, uint8_t fade);
void nscale8(CRGB *leds, uint16_t n, uint8_t scale);
void nscale8_video(CRGB *leds, uint16_t n, uint8_t scale);
void fadeUsingColor(CRGB *leds, uint16_t n, const CRGB &colormask);
CRGB blend(const CRGB &p1, const CRGB &p2, fract8 amountOfP2);
CHSV blend(const CHSV &p1, const CHSV &p2, fract8 amountOfP2);
CRGB &nblend(CRGB &existing, const CRGB &overlay, fract8 amountOfOverlay);
void nblend(CRGB *existing, const CRGB *overlay, uint16_t n, fract8 amount);
void blend(const CRGB *src1, const CRGB *src2, CRGB *dest, uint16_t n, fract8 amountOfsrc2);
void blur1d(CRGB *leds, uint16_t n, fract8 blur_amount);
void blur2d(CRGB *leds, uint8_t width, uint8_t height, fract8 blur_amount);
void blurRows(CRGB *leds, uint8_t width, uint8_t height, fract8 blur_amount);
void blurColumns(CRGB *leds, uint8_t width, uint8_t height, fract8 blur_amount);
CRGB HeatColor(uint8_t temperature);

// ---------------------------------------------------------------- paletas
typedef uint32_t TProgmemRGBPalette16[16];
typedef uint32_t TProgmemPalette16[16];
typedef enum { NOBLEND = 0, LINEARBLEND = 1, LINEARBLEND_NOWRAP = 2 } TBlendType;

class CRGBPalette16 {
public:
  CRGB entries[16];
  CRGBPalette16() {}
  CRGBPalette16(const CRGB &c) { for (int i = 0; i < 16; i++) entries[i] = c; }
  CRGBPalette16(const CRGB &c1, const CRGB &c2) { fill_gradient_RGB(entries, 16, c1, c2); }
  CRGBPalette16(const CRGB &c1, const CRGB &c2, const CRGB &c3) { fill_gradient_RGB(entries, 16, c1, c2, c3); }
  CRGBPalette16(const CRGB &c00, const CRGB &c01, const CRGB &c02, const CRGB &c03,
                const CRGB &c04, const CRGB &c05, const CRGB &c06, const CRGB &c07,
                const CRGB &c08, const CRGB &c09, const CRGB &c10, const CRGB &c11,
                const CRGB &c12, const CRGB &c13, const CRGB &c14, const CRGB &c15) {
    const CRGB *a[16] = { &c00, &c01, &c02, &c03, &c04, &c05, &c06, &c07, &c08, &c09, &c10, &c11, &c12, &c13, &c14, &c15 };
    for (int i = 0; i < 16; i++) entries[i] = *a[i]; }
  CRGBPalette16(const TProgmemRGBPalette16 &p) { for (int i = 0; i < 16; i++) entries[i] = CRGB(p[i]); }
  CRGBPalette16 &operator=(const TProgmemRGBPalette16 &p) { for (int i = 0; i < 16; i++) entries[i] = CRGB(p[i]); return *this; }
  CRGB &operator[](uint8_t x) { return entries[x]; }
  const CRGB &operator[](uint8_t x) const { return entries[x]; }
  bool operator==(const CRGBPalette16 &o) const { for (int i = 0; i < 16; i++) if (entries[i] != o.entries[i]) return false; return true; }
  bool operator!=(const CRGBPalette16 &o) const { return !(*this == o); }
};
class CHSVPalette16 {
public:
  CHSV entries[16];
  CHSVPalette16() {}
  CHSV &operator[](uint8_t x) { return entries[x]; }
  const CHSV &operator[](uint8_t x) const { return entries[x]; }
};
CRGB ColorFromPalette(const CRGBPalette16 &pal, uint8_t index, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND);
CHSV ColorFromPalette(const CHSVPalette16 &pal, uint8_t index, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND);
void nblendPaletteTowardPalette(CRGBPalette16 &current, CRGBPalette16 &target, uint8_t maxChanges);
void fill_palette(CRGB *L, uint16_t N, uint8_t startIndex, uint8_t incIndex, const CRGBPalette16 &pal,
                  uint8_t brightness = 255, TBlendType blendType = LINEARBLEND);

extern const TProgmemRGBPalette16 CloudColors_p;
extern const TProgmemRGBPalette16 LavaColors_p;
extern const TProgmemRGBPalette16 OceanColors_p;
extern const TProgmemRGBPalette16 ForestColors_p;
extern const TProgmemRGBPalette16 RainbowColors_p;
extern const TProgmemRGBPalette16 RainbowStripeColors_p;
#define RainbowStripesColors_p RainbowStripeColors_p
extern const TProgmemRGBPalette16 PartyColors_p;
extern const TProgmemRGBPalette16 HeatColors_p;

// ---------------------------------------------------------------- ruido
uint8_t inoise8(uint16_t x);
uint8_t inoise8(uint16_t x, uint16_t y);
uint8_t inoise8(uint16_t x, uint16_t y, uint16_t z);
uint16_t inoise16(uint32_t x);
uint16_t inoise16(uint32_t x, uint32_t y);
uint16_t inoise16(uint32_t x, uint32_t y, uint32_t z);
int8_t inoise8_raw(uint16_t x, uint16_t y, uint16_t z);

// ---------------------------------------------------------------- controladores
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class WS2812B {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class WS2812 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class WS2813 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class WS2811 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class WS2811_400 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class SK6812 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class SK6822 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class WS2815 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class GS1903 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class TM1803 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class TM1804 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class TM1809 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class APA106 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class PL9823 {};
template <uint8_t DATA_PIN, EOrder RGB_ORDER = GRB> class UCS1903 {};
template <uint8_t DATA_PIN> class NEOPIXEL {};

class CLEDController {
public:
  CRGB *data = nullptr;
  int count = 0;
  int pin = -1;
  int order = GRB;
  CRGB correction = CRGB(255, 255, 255);
  CRGB temperature = CRGB(255, 255, 255);
  CLEDController *next = nullptr;
  CLEDController &setCorrection(CRGB c) { correction = c; return *this; }
  CLEDController &setCorrection(LEDColorCorrection c) { correction = CRGB((uint32_t)c); return *this; }
  CLEDController &setTemperature(CRGB c) { temperature = c; return *this; }
  CLEDController &setTemperature(ColorTemperature c) { temperature = CRGB((uint32_t)c); return *this; }
  CLEDController &setDither(uint8_t = BINARY_DITHER) { return *this; }
  CLEDController &setRgbw(...) { return *this; }
  CRGB *leds() { return data; }
  int size() const { return count; }
  CRGB &operator[](int i) { return data[i]; }
  void showLeds(uint8_t brightness = 255);
  void clearLedData() { if (data) memset((void *)data, 0, sizeof(CRGB) * count); }
};

class CFastLED {
  CLEDController *first = nullptr;
  uint8_t brightness = 255;
  uint32_t maxPower_mW = 0;
  uint16_t maxRefresh = 400;
  CLEDController *add(CRGB *data, int n, int pin, int order, int offset);
public:
  template <template <uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
  CLEDController &addLeds(CRGB *data, int n, int offset = 0) { return *add(data, n, DATA_PIN, RGB_ORDER, offset); }
  template <template <uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN>
  CLEDController &addLeds(CRGB *data, int n, int offset = 0) { return *add(data, n, DATA_PIN, RGB, offset); }
  template <template <uint8_t DATA_PIN> class CHIPSET, uint8_t DATA_PIN>
  CLEDController &addLeds(CRGB *data, int n, int offset = 0) { return *add(data, n, DATA_PIN, GRB, offset); }

  void setBrightness(uint8_t b) { brightness = b; }
  uint8_t getBrightness() const { return brightness; }
  void setMaxPowerInVoltsAndMilliamps(uint8_t volts, uint32_t mA) { maxPower_mW = volts * mA; }
  void setMaxPowerInMilliWatts(uint32_t mW) { maxPower_mW = mW; }
  void setMaxRefreshRate(uint16_t r, bool = false) { maxRefresh = r; }
  void setCorrection(const CRGB &c) { for (CLEDController *p = first; p; p = p->next) p->setCorrection(c); }
  void setCorrection(LEDColorCorrection c) { setCorrection(CRGB((uint32_t)c)); }
  void setTemperature(const CRGB &c) { for (CLEDController *p = first; p; p = p->next) p->setTemperature(c); }
  void setTemperature(ColorTemperature c) { setTemperature(CRGB((uint32_t)c)); }
  void setDither(uint8_t = BINARY_DITHER) {}
  void show(uint8_t scale);
  void show() { show(brightness); }
  void clear(bool writeData = false);
  void clearData() { clear(false); }
  void showColor(const CRGB &c, uint8_t scale);
  void showColor(const CRGB &c) { showColor(c, brightness); }
  void delay(unsigned long ms);
  int count() const { int k = 0; for (CLEDController *p = first; p; p = p->next) k++; return k; }
  CLEDController &operator[](int x) { CLEDController *p = first; while (x-- > 0 && p && p->next) p = p->next; return *p; }
  int size() { return first ? first->count : 0; }
  CRGB *leds() { return first ? first->data : nullptr; }
  uint16_t getFPS() { return 0; }
};
extern CFastLED FastLED;
