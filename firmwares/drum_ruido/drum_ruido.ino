// ==============================================================================================================================================
// PERCU-SYNTH — DRUM RUIDO: drum machine de timbres ruidosos con salida LIMPIA — GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
//
// Puedes usar, modificar y distribuir este código y hardware, siempre que se mantenga
// la atribución a GC Lab Chile. Se entrega "tal cual", sin garantías de ningún tipo.
// ==============================================================================================================================================
// REPOSITORIO: https://github.com/GC-Lab-Gonzalo/Percu-Synth
// ==============================================================================================================================================
// HARDWARE (usado por este firmware)
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3
// - DAC PCM5102 vía I2S — estéreo 44.1 kHz · 16-bit |LCK -> 39, DIN -> 40, BCK -> 41|
// - 6 LEDs WS2812 SMD internos de la placa |DATA -> 46| (sólo indicadores, no controlan nada)
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - Flash Mode         : DIO          (¡OPI rompe I2S!)
// - PSRAM              : OPI PSRAM
// - Partition Scheme   : Default 4MB with spiffs
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (incluye driver/i2s_std.h)
// - FastLED (gestor de librerías Arduino) — para los 6 LEDs SMD de la placa
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Drum machine 100 % sintetizada (sin samples). Los TIMBRES son ruidosos y sucios a
// propósito — ruido, chapas metálicas, clanks de fierro, ráfagas de vapor — pero la
// CADENA FINAL ES LIMPIA: nada de bit-crush, nada de diezmado de sample rate. La
// suciedad se hace con síntesis y con saturación por bandas, no rompiendo el audio.
//
// Las 7 pistas: BOMBO · CAJA · HATS · CLANK (fierro corto) · METAL (yunque) ·
// RÁFAGA (vapor/aire) · BAJO (sub limpio). Los metales se hacen con parciales
// INARMÓNICOS (1.41, 2.37, 3.14…), que es por qué suenan a chapa y no a nota; los
// barridos rápidos de pitch están descartados a propósito: un barrido corto en el
// rango medio se oye como el "pío" de un pájaro, no como percusión industrial.
//
// Reglas de calidad que sostienen el sonido (no romperlas al editar):
//   · Cada golpe tiene DOS envolventes independientes — la del TONO y la del RUIDO.
//     Es lo que separa un bombo de un "pfff": el ruido de pegada muere en 6 ms
//     mientras el cuerpo grave sigue sonando 400 ms.
//   · El filtro por voz actúa SÓLO sobre la capa de ruido; el tono nunca se filtra,
//     así el fundamental del bombo no se pierde al cerrar nada.
//   · Todas las ondas con esquinas (sierra/cuadrada) llevan PolyBLEP. Una cuadrada
//     cruda a 3 kHz genera aliasing = "frecuencias molestas" que no son de ninguna nota.
//   · El bombo termina en ~52 Hz (no en 36: eso no lo reproduce ningún parlante chico)
//     y lleva saturación suave PROPIA, que le genera armónicos y lo hace audible
//     incluso sin graves reales.
//   · SIDECHAIN: cada bombo agacha 3 dB el resto de la mezcla durante ~120 ms. Es lo
//     que hace que el bombo "se sienta" en vez de competir con los hats.
//   · Resonancia del filtro global limitada a 1.6 y bloqueador de DC a la salida.
//
// Los PATRONES SON ALEATORIOS: cada BTN1 sortea uno nuevo de 32 pasos (2 compases de
// semicorcheas) con el "feel" del timbre activo. 5 TIMBRES: CYBER · DUBSTEP (half-time)
// · GLITCH · INDUSTRIAL · CAOS.
// ==============================================================================================================================================
// FUNCIONAMIENTO (botones — uno = una función, sin combos, todo en el flanco de presión)
// ==============================================================================================================================================
// - BTN1 (44) → PATRÓN: sortea un patrón nuevo (el timbre y el tempo no se tocan).
// - BTN2 (42) → TIMBRE: cicla los 5 kits. Cambia la síntesis al instante; el patrón se mantiene.
// - BTN3 (0)  → TAP TEMPO: marca el pulso (negras). Con 2 toques fija el BPM (60–200).
// - BTN4 (45) → MEDIO TIEMPO (mantener): cada paso del patrón dura el doble, así que el
//               ritmo cae a la mitad de velocidad. El reloj maestro sigue corriendo por
//               debajo → al soltar retoma exactamente donde tocaba, sin desfase.
// - BTN5 (47) → FILL (mantener): redoble que acelera; bombo y bajo siguen debajo.
//
// La máquina siempre suena (no hay play/stop): para silenciarla, el POT1 a cero.
// ==============================================================================================================================================
// FUNCIONAMIENTO (potenciómetros — significado fijo, siempre el mismo)
// ==============================================================================================================================================
// - POT1 (ADC1)  → VOLUMEN master
// - POT2 (ADC2)  → FILTRO: corte del LPF resonante global (200 Hz – abierto del todo).
//                  Curva pensada para que el medio del recorrido deje pasar los hats.
//                  Al cerrarlo sube la resonancia (hasta 3.2) y cada golpe ABRE el corte
//                  → el filtro respira con el ritmo en vez de quedarse quieto.
// - POT3 (ADC8)  → SUCIEDAD: saturación en DOS BANDAS (graves/medios hasta ×7.5, agudos
//                  apenas) + más capa de ruido en cada golpe. Ensucia de verdad sin
//                  bit-crush ni diezmado, que es lo que produce aliasing.
// - POT4 (ADC10) → BEAT REPEAT: OFF · ×2 (8 pasos) · ×4 (4) · ×8 (2) · ×16 (1 paso).
//                  Loopea sobre un reloj maestro que nunca se detiene → al volver a
//                  OFF la máquina retoma en tiempo, nunca desfasada.
// ==============================================================================================================================================
// FUNCIONAMIENTO (LEDs — 6 SMD de la placa, sólo indicadores)
// ==============================================================================================================================================
// - Color base = TIMBRE activo (cian · violeta · verde · naranja · rojo).
// - LED0 bombo · LED1 caja · LED2 hats · LED3 perc/zap · LED4 ruido/bajo · LED5 pulso.
// - BEAT REPEAT activo → N LEDs rojos (1 = ×2 … 4 = ×16) · FILL → estrobo blanco.
// - MEDIO TIEMPO → LED 5 amarillo fijo y el resto a media luz.
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <math.h>

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK      39
#define I2S_DIN      40
#define I2S_BCK      41
#define SAMPLE_RATE  44100
#define BUF_SAMPLES  128          // ≈ 2.9 ms · 4 descriptores DMA ≈ 12 ms de cola → respuesta inmediata

// ─── Pines ─────────────────────────────────────────────────
const uint8_t BTN_PIN[5] = {44, 42, 0, 45, 47};   // BTN1..BTN5 (orden correcto de la placa)
#define POT_VOL     1
#define POT_FILT    2
#define POT_BODY    8
#define POT_REPEAT  10

#define LED_PIN     46
#define NUM_LEDS    6
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define LED_BRIGHT  70

// ─── Secuenciador ──────────────────────────────────────────
#define NUM_STEPS  32             // 2 compases de 4/4 en semicorcheas
#define NUM_TRACKS 7

#define T_KICK   0
#define T_SNARE  1
#define T_HAT    2
#define T_PERC   3
#define T_ZAP    4
#define T_NOISE  5
#define T_BASS   6

#define MAX_VOICES 10

// ==============================================================================================
// TIPOS — van ARRIBA DEL TODO, antes de la primera función.
// El IDE de Arduino inserta los prototipos automáticos JUSTO ANTES de la primera definición de
// función del archivo: cualquier struct declarado más abajo queda por debajo de los prototipos
// que lo usan y el compilador tira "'Biq' does not name a type". (Misma nota que en cyber_kit.)
// ==============================================================================================

// Biquad RBJ (Direct Form I)
struct Biq  { float b0, b1, b2, a1, a2; };
struct BiqZ { float x1, x2, y1, y2; };

// Un TIMBRE completo: redefine la síntesis de las 7 pistas y el "feel" del patrón
struct Kit {
  const char *name;
  uint8_t hue;            // color FastLED del kit
  bool    halfTime;       // caja en el 3 (dubstep) en vez de 2 y 4
  uint8_t gWave;          // onda del blip de percusión: 0 seno · 1 sierra · 2 cuadrada

  // BOMBO: frecuencia final, cuánto arranca por encima, caída del pitch, decay,
  //        nivel del click de pegada, saturación propia (armónicos = se oye en chico)
  float kFreq, kRatio, kDropMs, kDec, kClick, kSat;
  // CAJA: 2 parciales del cuerpo, decay del cuerpo, BP del ruido (fc, Q),
  //       decay del ruido, mezcla cuerpo↔ruido (0 = sólo cuerpo · 1 = sólo ruido)
  float sF1, sF2, sDec, sFc, sQ, sNDec, sMix;
  // HAT: pasa-altos del ruido, Q del BP metálico encima, decay cerrado / abierto
  float hFc, hQ2, hDecC, hDecO;
  // PERC (clank corto de fierro): frecuencia, ratio del 2º parcial INARMÓNICO, decay
  float gF, gR2, gDec;
  // METAL (golpe de fierro / yunque): frecuencia, ratio del 3er parcial, ms del ruido
  //        de impacto, decay. Nada de barridos largos de pitch: eso suena a pájaro.
  float mF, mR3, mNoiseMs, mDec;
  // RUIDO (ráfaga de vapor / aire comprimido): decay, barrido CORTO del BP fc0→fc1, Q BAJO.
  //        Un BP de Q alto barriendo hacia arriba es un silbido de ave, no una ráfaga.
  float nDec, nFc0, nFc1, nQ;
  // BAJO (sub limpio, sin wobble): multiplicador del ataque de pitch, decay, saturación
  float bFreqMul, bDec, bSat;

  float dens[NUM_TRACKS]; // densidad de cada pista al sortear el patrón (0..1)
};

// Voz de un golpe: DOS capas con envolventes independientes (tono y ruido)
struct Voice {
  bool    active;
  uint8_t type;
  float   amp;                  // velocidad del golpe × ganancia de la pista
  float   atk, atkInc;          // rampa de ataque (1.0 = instantáneo; < 1 = swell del riser)
  // Capa TONAL
  float   env, envCoef;
  float   ph, f, fEnd, fCoef;   // barrido exponencial de pitch
  float   ph2, ratio2, amp2;    // 2º parcial
  float   ph3, ratio3, amp3;    // 3er parcial — con ratios INARMÓNICOS (1.41, 2.37…) = metal
  uint8_t wave;                 // 0 seno · 1 sierra · 2 cuadrada (con PolyBLEP)
  float   sat;                  // saturación propia (0 = limpio)
  // Capa de RUIDO (su propia envolvente: es lo que da la pegada)
  float   nEnv, nCoef, nAmt;
  bool    useF1, useF2;
  Biq     bq1; BiqZ bz1;        // pasa-altos o pasa-banda principal
  Biq     bq2; BiqZ bz2;        // resonancia metálica encima (hats)
  float   fFc, fFcEnd, fQ, fT, fTinc;   // barrido del filtro a lo largo del golpe
  // Estéreo
  float   lg, rg;
};

struct Btn { uint8_t pin; bool last; uint32_t tDown; bool longFired; };

// ==============================================================================================
// Utilidades DSP
// ==============================================================================================

// Ruido de audio (LCG propio: no consume el generador que sortea los patrones)
static uint32_t nrng = 0x9E3779B9u;
inline float noiseF() {
  nrng = nrng * 1664525u + 1013904223u;
  return (float)(int32_t)nrng * (1.0f / 2147483648.0f);
}

// Generador de composición (patrones, notas) — separado del ruido de audio
static uint32_t prng = 0x12345677u;
inline uint32_t rnd32() { prng = prng * 1664525u + 1013904223u; return prng; }
inline float    frnd()  { return (float)(rnd32() >> 8) * (1.0f / 16777216.0f); }   // 0..1
inline int      irnd(int n) { return (int)(frnd() * n) % n; }

// Seno por tabla (257 entradas → interpolación lineal sin caso especial en el borde)
static float sineLUT[257];
inline float sineAt(float ph) {
  while (ph >= 1.0f) ph -= 1.0f;
  while (ph <  0.0f) ph += 1.0f;
  float x = ph * 256.0f;
  int   i = (int)x;
  float f = x - (float)i;
  return sineLUT[i] + (sineLUT[i + 1] - sineLUT[i]) * f;
}

// Semitonos → razón de frecuencia (−24 … +36)
#define SEMI_MIN  -24
#define SEMI_N     61
static float semiLUT[SEMI_N];
inline float semiRatio(int s) {
  if (s < SEMI_MIN) s = SEMI_MIN;
  if (s > SEMI_MIN + SEMI_N - 1) s = SEMI_MIN + SEMI_N - 1;
  return semiLUT[s - SEMI_MIN];
}

// PolyBLEP: redondea el salto de sierra/cuadrada. Sin esto, una cuadrada a 3 kHz
// devuelve armónicos por encima de Nyquist que se pliegan hacia abajo como tonos
// inarmónicos — el "chillido" que no pertenece a ninguna nota.
inline float polyBlep(float t, float dt) {
  if (t < dt)             { t /= dt;               return t + t - t * t - 1.0f; }
  else if (t > 1.0f - dt) { t = (t - 1.0f) / dt;   return t * t + t + t + 1.0f; }
  return 0.0f;
}

inline float oscWave(uint8_t wave, float ph, float dt) {
  if (wave == 0) return sineAt(ph);
  if (wave == 1) return (2.0f * ph - 1.0f) - polyBlep(ph, dt);
  float p2 = ph + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
  float a = (2.0f * ph - 1.0f) - polyBlep(ph, dt);
  float b = (2.0f * p2 - 1.0f) - polyBlep(p2, dt);
  return (a - b) * 0.5f;
}

inline float biqProc(const Biq &c, BiqZ &z, float x) {
  float y = c.b0 * x + c.b1 * z.x1 + c.b2 * z.x2 - c.a1 * z.y1 - c.a2 * z.y2;
  z.x2 = z.x1; z.x1 = x;
  z.y2 = z.y1; z.y1 = y;
  return y;
}

void makeLPF(Biq &c, float fc, float Q) {
  if (fc < 40.0f)    fc = 40.0f;
  if (fc > 19000.0f) fc = 19000.0f;
  if (Q  < 0.4f)     Q  = 0.4f;
  float w = 2.0f * (float)M_PI * fc / SAMPLE_RATE;
  float s = sinf(w), co = cosf(w), al = s / (2.0f * Q);
  float a0 = 1.0f + al;
  c.b0 = ((1.0f - co) * 0.5f) / a0;
  c.b1 = (1.0f - co) / a0;
  c.b2 = c.b0;
  c.a1 = (-2.0f * co) / a0;
  c.a2 = (1.0f - al) / a0;
}

void makeBPF(Biq &c, float fc, float Q) {
  if (fc < 60.0f)    fc = 60.0f;
  if (fc > 17000.0f) fc = 17000.0f;
  if (Q  < 0.4f)     Q  = 0.4f;
  float w = 2.0f * (float)M_PI * fc / SAMPLE_RATE;
  float s = sinf(w), co = cosf(w), al = s / (2.0f * Q);
  float a0 = 1.0f + al;
  c.b0 =  al / a0;
  c.b1 =  0.0f;
  c.b2 = -al / a0;
  c.a1 = (-2.0f * co) / a0;
  c.a2 = (1.0f - al) / a0;
}

void makeHPF(Biq &c, float fc, float Q) {
  if (fc < 60.0f)    fc = 60.0f;
  if (fc > 17000.0f) fc = 17000.0f;
  if (Q  < 0.4f)     Q  = 0.4f;
  float w = 2.0f * (float)M_PI * fc / SAMPLE_RATE;
  float s = sinf(w), co = cosf(w), al = s / (2.0f * Q);
  float a0 = 1.0f + al;
  c.b0 =  ((1.0f + co) * 0.5f) / a0;
  c.b1 = -(1.0f + co) / a0;
  c.b2 =  c.b0;
  c.a1 = (-2.0f * co) / a0;
  c.a2 = (1.0f - al) / a0;
}

// Saturación suave (aproximación de tanh, sin llamar a tanhf en el bucle de audio)
inline float softClip(float x) {
  if (x >  3.0f) return  1.0f;
  if (x < -3.0f) return -1.0f;
  float x2 = x * x;
  return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// ==============================================================================================
// TIMBRES (kits) — el struct Kit, con el significado de cada campo, está en el bloque de tipos
// ==============================================================================================
// Nota sobre los DECAYS: son la constante de tiempo tau de una exponencial, así que el
// golpe se oye durante ~3·tau. Un bombo con tau 0.42 suena casi 2 segundos y deja de ser
// un golpe: se vuelve un zumbido grave continuo que tapa el resto. Los valores de abajo
// (bombo 0.13–0.30, bajo 0.14–0.30, hat abierto ≤ 0.17) mantienen todo como PERCUSIÓN.
const Kit KITS[5] = {
  // ── CYBER (cian) — electrónico limpio y punzante ─────────────────────────────
  { "CYBER", HUE_AQUA, false, 2,
    52.0f, 5.0f, 32.0f, 0.20f, 0.55f, 1.35f,
    185.0f, 288.0f, 0.11f, 1900.0f, 0.9f, 0.14f, 0.55f,
    7400.0f, 2.0f, 0.042f, 0.13f,
    880.0f, 1.47f, 0.070f,
    1700.0f, 2.37f, 12.0f, 0.16f,
    0.30f, 380.0f, 1600.0f, 0.75f,
    1.55f, 0.20f, 1.15f,
    { 0.95f, 0.85f, 0.75f, 0.35f, 0.20f, 0.10f, 0.55f } },

  // ── DUBSTEP (violeta) — half-time, bombo grande, caja en el 3, sub gordo ─────
  { "DUBSTEP", HUE_PURPLE, true, 2,
    48.0f, 6.5f, 48.0f, 0.30f, 0.60f, 1.60f,
    172.0f, 258.0f, 0.13f, 1500.0f, 0.8f, 0.16f, 0.62f,
    8000.0f, 2.4f, 0.038f, 0.16f,
    700.0f, 1.62f, 0.085f,
    1200.0f, 2.71f, 15.0f, 0.22f,
    0.40f, 260.0f, 1400.0f, 0.70f,
    1.70f, 0.30f, 1.35f,
    { 0.90f, 1.00f, 0.55f, 0.22f, 0.20f, 0.15f, 0.90f } },

  // ── GLITCH (verde) — IDM roto: todo corto, mucho blip ────────────────────────
  { "GLITCH", HUE_GREEN, false, 1,
    58.0f, 4.2f, 22.0f, 0.13f, 0.70f, 1.25f,
    215.0f, 340.0f, 0.07f, 2400.0f, 1.4f, 0.09f, 0.62f,
    8600.0f, 3.0f, 0.026f, 0.09f,
    1250.0f, 1.93f, 0.045f,
    2100.0f, 3.14f, 8.0f, 0.10f,
    0.22f, 600.0f, 2200.0f, 0.85f,
    1.35f, 0.14f, 1.10f,
    { 0.70f, 0.55f, 0.85f, 0.70f, 0.35f, 0.12f, 0.45f } },

  // ── INDUSTRIAL (naranja) — metálico y macizo, ametralladora ──────────────────
  { "INDUSTRIAL", HUE_ORANGE, false, 2,
    50.0f, 5.8f, 40.0f, 0.19f, 0.85f, 1.70f,
    160.0f, 243.0f, 0.13f, 2700.0f, 1.7f, 0.15f, 0.70f,
    6400.0f, 3.4f, 0.034f, 0.14f,
    520.0f, 1.41f, 0.090f,
    1250.0f, 2.37f, 20.0f, 0.22f,
    0.36f, 200.0f, 1200.0f, 0.65f,
    1.60f, 0.22f, 1.40f,
    { 0.95f, 0.80f, 0.80f, 0.40f, 0.45f, 0.25f, 0.60f } },

  // ── CAOS (rojo) — el más ruidoso, pero sigue siendo audio limpio ─────────────
  { "CAOS", HUE_RED, false, 1,
    55.0f, 7.5f, 60.0f, 0.17f, 0.95f, 1.45f,
    240.0f, 372.0f, 0.10f, 3000.0f, 2.0f, 0.16f, 0.75f,
    5600.0f, 3.8f, 0.030f, 0.17f,
    1550.0f, 2.11f, 0.055f,
    2400.0f, 3.46f, 10.0f, 0.14f,
    0.45f, 300.0f, 2600.0f, 0.80f,
    1.45f, 0.26f, 1.25f,
    { 0.80f, 0.70f, 0.80f, 0.60f, 0.50f, 0.30f, 0.65f } },
};

uint8_t kit = 0;

// Ganancia por pista: el bombo manda, los hats acompañan. Sin esto el bombo compite
// de igual a igual con seis pistas y deja de sentirse. El bajo va contenido (0.62) porque
// vive en la misma banda que el bombo: subirlo emborrona los graves en vez de sumar peso.
const float TRACK_GAIN[NUM_TRACKS] = { 1.00f, 0.80f, 0.62f, 0.52f, 0.42f, 0.34f, 0.62f };

// ==============================================================================================
// Voces
// ==============================================================================================
Voice voices[MAX_VOICES];

Voice &allocVoice() {
  for (int i = 0; i < MAX_VOICES; i++) if (!voices[i].active) return voices[i];
  // Roba la de menor energía, pero NUNCA el bombo (es el ancla del ritmo)
  int best = -1; float lo = 1e9f;
  for (int i = 0; i < MAX_VOICES; i++) {
    if (voices[i].type == T_KICK) continue;
    float e = (voices[i].env + voices[i].nEnv) * voices[i].amp;
    if (e < lo) { lo = e; best = i; }
  }
  return voices[best < 0 ? 0 : best];
}

// ─── Bajo: sub limpio (sin wobble) ─────────────────────────
#define BASS_ROOT_HZ 41.20f   // Mi1
const int8_t BASS_NOTES[8] = { 0, 0, 0, 3, 5, 7, 10, 12 };   // set menor oscuro
int8_t patBassNote[NUM_STEPS];

// ==============================================================================================
// Patrón (bitmask de 32 pasos por pista) + acentos
// ==============================================================================================
uint32_t pat[NUM_TRACKS];
uint32_t patAcc[NUM_TRACKS];

// Peso por posición del paso dentro del compás (16 pasos) para cada pista
float posWeight(uint8_t t, int s, bool halfTime) {
  int b = s & 15;
  switch (t) {
    case T_KICK:
      if (b == 0)                   return 1.00f;
      if (b == 8)                   return halfTime ? 0.20f : 0.80f;
      if ((b & 3) == 0)             return 0.30f;
      if ((b & 1) == 0)             return 0.10f;
      return 0.05f;
    case T_SNARE:
      if (halfTime)                 return (b == 8) ? 1.00f : ((b == 14) ? 0.10f : 0.03f);
      if (b == 4 || b == 12)        return 1.00f;
      if (b == 7 || b == 15)        return 0.12f;
      return 0.04f;
    case T_HAT:
      if ((b & 1) == 1)             return 0.85f;
      if ((b & 3) == 2)             return 0.55f;
      return 0.35f;
    case T_PERC:
      if ((b & 1) == 1)             return 0.70f;
      return 0.40f;
    case T_ZAP:
      if (b >= 12)                  return 0.75f;
      return 0.20f;
    case T_NOISE:
      if (b == 15)                  return 1.00f;   // riser justo antes del "1"
      if (b >= 12)                  return 0.30f;
      return 0.08f;
    case T_BASS:
      if (halfTime)                 return (b == 0 || b == 8) ? 1.00f : ((b & 3) == 0 ? 0.20f : 0.04f);
      if ((b & 3) == 0)             return 0.90f;
      if ((b & 1) == 0)             return 0.30f;
      return 0.14f;
  }
  return 0.0f;
}

void nuevoPatron() {
  const Kit &k = KITS[kit];
  for (int t = 0; t < NUM_TRACKS; t++) {
    pat[t] = 0; patAcc[t] = 0;
    for (int s = 0; s < NUM_STEPS; s++) {
      float p = k.dens[t] * posWeight(t, s, k.halfTime);
      if (frnd() < p) {
        pat[t] |= (1u << s);
        if (frnd() < 0.30f) patAcc[t] |= (1u << s);
      }
    }
  }
  // El bombo del "1" nunca falta: sin ancla, un patrón 100 % aleatorio se vuelve papilla
  pat[T_KICK] |= 1u;
  patAcc[T_KICK] |= 1u;
  // Ni el bombo ni el bajo pueden quedar vacíos en el 2º compás
  if ((pat[T_KICK] >> 16) == 0) pat[T_KICK] |= (1u << 16);

  for (int s = 0; s < NUM_STEPS; s++) patBassNote[s] = BASS_NOTES[irnd(8)];
}

// ==============================================================================================
// Reloj / transporte
// ==============================================================================================
float bpm         = 140.0f;
float stepSamples = 0.0f;      // muestras por semicorchea
float stepAcc     = 0.0f;
int   masterStep  = 0;         // posición LIBRE del reloj (no la altera ni el beat repeat ni el medio tiempo)
int   curPlayStep = 0;         // paso que SUENA

// MEDIO TIEMPO (BTN4 mantenido): cada paso del patrón dura DOS pasos del reloj maestro
bool  halfHeld    = false;
int   halfStep    = 0;
uint8_t halfTick  = 1;

// BEAT REPEAT (POT4): longitud del loop en pasos — 0 = OFF
int   repeatLen   = 0;
int   repeatZone  = 0;         // 0 OFF · 1 ×2 · 2 ×4 · 3 ×8 · 4 ×16
int   repeatAnchor = 0;
int   repeatPos   = 0;

// FILL (BTN5)
bool  fillHeld    = false;
uint32_t fillT0   = 0;
float fillAcc     = 0.0f;
int   fillCount   = 0;

void setBPM(float b) {
  if (b < 60.0f)  b = 60.0f;
  if (b > 200.0f) b = 200.0f;
  bpm = b;
  stepSamples = (60.0f / bpm) * (float)SAMPLE_RATE * 0.25f;   // semicorchea
}

// ==============================================================================================
// Controles (valores suavizados de los pots)
// ==============================================================================================
float pVol = 0.7f, pFilt = 0.85f, pBody = 0.25f;

// ==============================================================================================
// Disparo de voces
// ==============================================================================================
inline float decCoef(float seconds) {
  if (seconds < 0.003f) seconds = 0.003f;
  return expf(-1.0f / (seconds * (float)SAMPLE_RATE));
}

float duckEnv = 0.0f, duckCoef = 0.0f;   // sidechain: el bombo agacha el resto
float filtEnv = 0.0f, filtEnvCoef = 0.0f;  // envolvente de filtro: cada golpe ABRE el corte

void trigVoice(uint8_t t, float vel, float pitchMul) {
  const Kit &k = KITS[kit];
  Voice &v = allocVoice();

  v.active = true; v.type = t;
  v.amp = vel * TRACK_GAIN[t];
  v.atk = 1.0f; v.atkInc = 1.0f;
  v.env = 1.0f; v.nEnv = 1.0f; v.nAmt = 0.0f;
  v.ph = 0.0f; v.ph2 = 0.0f; v.ratio2 = 0.0f; v.amp2 = 0.0f;
  v.ph3 = 0.0f; v.ratio3 = 0.0f; v.amp3 = 0.0f;
  v.f = 100.0f; v.fEnd = 100.0f; v.fCoef = 0.0f;   // las voces sin capa tonal no los usan
  v.wave = 0; v.sat = 0.0f;
  v.useF1 = false; v.useF2 = false;
  v.fT = 0.0f; v.fTinc = 0.0f; v.fQ = 1.0f;
  v.bz1.x1 = v.bz1.x2 = v.bz1.y1 = v.bz1.y2 = 0.0f;
  v.bz2.x1 = v.bz2.x2 = v.bz2.y1 = v.bz2.y2 = 0.0f;
  float pan = 0.5f, dec = 0.2f, nDec = 0.02f;

  switch (t) {
    case T_KICK: {
      // Cuerpo: seno que cae desde kRatio× hasta la fundamental. Terminar en ~50 Hz
      // (y no en 36) es lo que hace que se oiga en parlantes chicos.
      float f1 = k.kFreq * pitchMul;
      v.f = f1 * k.kRatio; v.fEnd = f1;
      v.fCoef = 1.0f - expf(-1.0f / (k.kDropMs * 0.001f * SAMPLE_RATE));
      dec = k.kDec;
      v.sat = k.kSat;                 // armónicos propios = pegada audible sin graves reales
      // Click de pegada: ruido pasa-altos con envolvente PROPIA de 6 ms
      v.nAmt = k.kClick * 0.5f; nDec = 0.006f;
      v.useF1 = true; v.fFc = 2600.0f; v.fFcEnd = 2600.0f; v.fQ = 0.7f;
      makeHPF(v.bq1, v.fFc, v.fQ);
      duckEnv = 1.0f;                 // sidechain
      break;
    }
    case T_SNARE: {
      v.f = k.sF1 * pitchMul; v.fEnd = v.f * 0.82f;
      v.fCoef = 1.0f - expf(-1.0f / (0.035f * SAMPLE_RATE));
      v.ratio2 = k.sF2 / k.sF1; v.amp2 = 0.55f;
      dec = k.sDec;
      v.env = (1.0f - k.sMix) * 1.6f;   // el mix reparte cuerpo ↔ ruido
      v.nAmt = k.sMix * 1.5f; nDec = k.sNDec;
      v.useF1 = true; v.fFc = k.sFc; v.fFcEnd = k.sFc * 0.62f; v.fQ = k.sQ;
      v.fTinc = 1.0f / (nDec * SAMPLE_RATE);
      makeBPF(v.bq1, v.fFc, v.fQ);
      break;
    }
    case T_HAT: {
      // Sólo ruido: pasa-altos + una resonancia encima = metálico sin aliasing
      bool open = (frnd() < 0.22f);
      nDec = open ? k.hDecO : k.hDecC;
      dec = 0.004f; v.env = 0.0f;      // sin capa tonal
      v.nAmt = 1.25f;
      v.useF1 = true; v.fFc = k.hFc; v.fFcEnd = k.hFc; v.fQ = 0.7f;
      makeHPF(v.bq1, v.fFc, v.fQ);
      v.useF2 = true;
      makeBPF(v.bq2, k.hFc * 1.45f, k.hQ2);
      pan = open ? 0.36f : 0.64f;
      break;
    }
    case T_PERC: {
      // CLANK: golpe corto de fierro. Sólo una caída de pitch de ~8 % en 10 ms (impacto);
      // un barrido largo, y más si sube, se oye como el "pío" de un pájaro.
      v.f = k.gF * pitchMul * 1.08f; v.fEnd = k.gF * pitchMul;
      v.fCoef = 1.0f - expf(-1.0f / (0.010f * SAMPLE_RATE));
      dec = k.gDec;
      v.wave = k.gWave;
      v.ratio2 = k.gR2;  v.amp2 = 0.62f;          // parciales INARMÓNICOS = chapa metálica
      v.ratio3 = k.gR2 * 1.71f; v.amp3 = 0.34f;
      v.sat = 1.5f;
      v.nAmt = 0.42f; nDec = 0.010f;              // ruido de impacto
      v.useF1 = true; v.fFc = 2600.0f; v.fFcEnd = 2600.0f; v.fQ = 0.7f;
      makeHPF(v.bq1, v.fFc, v.fQ);
      pan = 0.25f + frnd() * 0.5f;
      break;
    }
    case T_ZAP: {
      // METAL: golpe de fierro / yunque. Tres parciales inarmónicos que no forman
      // ninguna nota (por eso suena a chapa y no a tono) + ruido de impacto.
      v.f = k.mF * pitchMul * 1.05f; v.fEnd = k.mF * pitchMul;
      v.fCoef = 1.0f - expf(-1.0f / (0.012f * SAMPLE_RATE));
      dec = k.mDec;
      v.ratio2 = 1.41f;  v.amp2 = 0.78f;
      v.ratio3 = k.mR3;  v.amp3 = 0.55f;
      v.sat = 1.6f;
      v.nAmt = 0.55f; nDec = k.mNoiseMs * 0.001f;
      v.useF1 = true; v.fFc = 3400.0f; v.fFcEnd = 3400.0f; v.fQ = 0.7f;
      makeHPF(v.bq1, v.fFc, v.fQ);
      pan = frnd() < 0.5f ? 0.22f : 0.78f;
      break;
    }
    case T_NOISE: {
      // RÁFAGA de vapor / aire comprimido. Q bajo y barrido corto: con Q alto y un
      // barrido largo hacia arriba, un pasa-banda de ruido silba igual que un ave.
      nDec = k.nDec; dec = 0.004f; v.env = 0.0f;
      v.nAmt = 1.15f;
      v.useF1 = true;
      v.fFc = k.nFc0; v.fFcEnd = k.nFc1 * pitchMul; v.fQ = k.nQ;
      v.fTinc = 1.0f / (nDec * SAMPLE_RATE);
      makeBPF(v.bq1, v.fFc, v.fQ);
      v.useF2 = true;
      makeHPF(v.bq2, 180.0f, 0.7f);              // le quita el retumbe al chorro de aire
      v.atk = 0.0f; v.atkInc = 1.0f / (nDec * 0.35f * SAMPLE_RATE);
      break;
    }
    case T_BASS: {
      float f1 = BASS_ROOT_HZ * semiRatio(patBassNote[curPlayStep]);
      v.f = f1 * k.bFreqMul; v.fEnd = f1;
      v.fCoef = 1.0f - expf(-1.0f / (0.025f * SAMPLE_RATE));
      dec = k.bDec;
      v.sat = k.bSat;
      v.ratio2 = 2.0f; v.amp2 = 0.16f;   // 2º armónico: hace audible el sub en parlante chico
      break;
    }
  }

  // El POT3 no sólo satura: también sube la capa de RUIDO de cada golpe.
  // Eso es "ruido" de verdad (síntesis), no basura de cuantización en el bus.
  v.nAmt *= (1.0f + pBody * 0.40f);

  // Los golpes con pegada abren el filtro global → el filtro respira con el ritmo
  if (t == T_KICK || t == T_SNARE || t == T_PERC || t == T_ZAP)
    if (vel > filtEnv) filtEnv = vel;

  v.envCoef = decCoef(dec);
  v.nCoef   = decCoef(nDec);
  v.lg = sqrtf(1.0f - pan);
  v.rg = sqrtf(pan);
}

// ==============================================================================================
// Paso del secuenciador
// ==============================================================================================
float ledFlash[NUM_LEDS];

void stepTrigger(int s) {
  curPlayStep = s;
  for (int t = 0; t < NUM_TRACKS; t++) {
    // Durante el FILL el redoble se queda con caja/hats/perc/zap/ruido;
    // bombo y bajo siguen sonando debajo para no perder el piso
    if (fillHeld && t >= T_SNARE && t <= T_NOISE) continue;
    if (!((pat[t] >> s) & 1u)) continue;

    bool  acc = (patAcc[t] >> s) & 1u;
    float vel = acc ? 1.0f : (0.66f + frnd() * 0.22f);
    trigVoice((uint8_t)t, vel, 1.0f);

    if (t == T_KICK)                     ledFlash[0] = 1.0f;
    else if (t == T_SNARE)               ledFlash[1] = 1.0f;
    else if (t == T_HAT)                 ledFlash[2] = 1.0f;
    else if (t == T_PERC || t == T_ZAP)  ledFlash[3] = 1.0f;
    else                                 ledFlash[4] = 1.0f;
  }
}

// ¿Hay algo que suene en este bloque? Si el beat repeat engancha un tramo vacío,
// el break se convierte en silencio — se busca hacia atrás un bloque con contenido.
bool blockHasHits(int anchor, int len) {
  for (int i = 0; i < len; i++) {
    int s = (anchor + i) & (NUM_STEPS - 1);
    for (int t = 0; t < NUM_TRACKS; t++) if ((pat[t] >> s) & 1u) return true;
  }
  return false;
}

void armRepeat(int len) {
  int anchor = masterStep - (masterStep % len);
  for (int tries = 0; tries < 4 && !blockHasHits(anchor, len); tries++)
    anchor = (anchor - len + NUM_STEPS) & (NUM_STEPS - 1);
  repeatAnchor = anchor & (NUM_STEPS - 1);
  repeatPos    = 0;
}

void advanceStep() {
  masterStep = (masterStep + 1) & (NUM_STEPS - 1);
  if ((masterStep & 3) == 0) ledFlash[5] = 1.0f;

  if (repeatLen > 0) {
    stepTrigger((repeatAnchor + (repeatPos % repeatLen)) & (NUM_STEPS - 1));
    repeatPos++;
  } else if (halfHeld) {
    // Cada paso del patrón ocupa dos pasos del maestro: el segundo no dispara nada
    // (la nota anterior sigue sonando). El maestro no se detiene, así que al soltar
    // se retoma exactamente donde tocaba, sin desfase.
    halfTick ^= 1;
    if (halfTick) return;
    stepTrigger(halfStep);
    halfStep = (halfStep + 1) & (NUM_STEPS - 1);
  } else {
    stepTrigger(masterStep);
  }
}

void fillHit() {
  fillCount++;
  float prog = (float)fillCount / 24.0f; if (prog > 1.0f) prog = 1.0f;
  float vel = 0.50f + prog * 0.50f;
  uint8_t t = (fillCount & 1) ? T_SNARE : T_HAT;
  if ((fillCount & 7) == 7) t = T_PERC;
  trigVoice(t, vel, 1.0f + prog * 0.35f);
  ledFlash[1] = 1.0f; ledFlash[3] = 0.8f;
}

// ==============================================================================================
// Cadena global: sidechain → saturación suave → LPF resonante → bloqueador de DC → volumen
// ==============================================================================================
Biq  gCur, gTgt, gStp;
BiqZ gzL, gzR;
float dcX1L = 0.0f, dcY1L = 0.0f, dcX1R = 0.0f, dcY1R = 0.0f;

// Divisor de bandas del saturador: los graves/medios aguantan mucho drive y engordan,
// pero saturar los agudos con la misma fuerza es lo que produce el chirrido de aliasing.
Biq  splitCoef;
BiqZ splitZL, splitZR;

// ==============================================================================================
// LEDs
// ==============================================================================================
CRGB leds[NUM_LEDS];
uint32_t lastShow  = 0;
float    msgTimer  = 0.0f;
uint8_t  msgCount  = 0;

void renderLeds() {
  const Kit &k = KITS[kit];
  FastLED.clear();

  if (msgTimer > 0.0f) {
    for (int i = 0; i < msgCount && i < NUM_LEDS; i++)
      leds[i] = CHSV(k.hue, 220, (uint8_t)(255 * msgTimer));
    msgTimer -= 0.06f;
  } else if (repeatZone > 0) {
    // Beat repeat: N LEDs rojos según la división (×2 … ×16)
    for (int i = 0; i < repeatZone && i < NUM_LEDS; i++) leds[i] = CHSV(HUE_RED, 230, 220);
    leds[5] = CHSV(HUE_RED, 180, (uint8_t)(255.0f * ledFlash[5]));
  } else {
    for (int i = 0; i < 5; i++)
      leds[i] = CHSV(k.hue + i * 6, 230, (uint8_t)(255.0f * ledFlash[i]));
    leds[5] = CHSV(k.hue + 40, 180, (uint8_t)(255.0f * ledFlash[5]));
  }
  if (halfHeld) {                                  // medio tiempo: LED 5 amarillo fijo
    for (int i = 0; i < 5; i++) leds[i].nscale8(120);
    leds[5] = CHSV(HUE_YELLOW, 220, 200);
  }
  if (fillHeld)
    for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB(255, 255, 255).nscale8(180);

  for (int i = 0; i < NUM_LEDS; i++) ledFlash[i] *= 0.72f;
  FastLED.show();
}

// ==============================================================================================
// Botones
// ==============================================================================================
Btn btn[5];
#define DEBOUNCE_MS 25

void initButtons() {
  for (int i = 0; i < 5; i++) {
    btn[i].pin = BTN_PIN[i];
    pinMode(btn[i].pin, INPUT_PULLUP);
    btn[i].last = true; btn[i].tDown = 0; btn[i].longFired = false;
  }
}

uint32_t lastTap = 0;
float    tapAvg  = 0.0f;
uint8_t  tapN    = 0;

// ==============================================================================================
// Audio
// ==============================================================================================
static i2s_chan_handle_t tx_chan;
int16_t audioBuf[BUF_SAMPLES * 2];

void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear    = true;
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = BUF_SAMPLES;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                  I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_BCK,
      .ws   = (gpio_num_t)I2S_LCK,
      .dout = (gpio_num_t)I2S_DIN,
      .din  = I2S_GPIO_UNUSED,
      .invert_flags = { false, false, false },
    },
  };
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
}

float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (float)(sum >> 3) / 4095.0f;
}

// POT4 → zona del beat repeat, con histéresis (el ruido del ADC no debe saltar de división)
void updateRepeat(float v) {
  const float TH[4] = { 0.14f, 0.34f, 0.54f, 0.74f };
  const float HYST  = 0.04f;
  int z = repeatZone;
  while (z < 4 && v > TH[z] + HYST)     z++;
  while (z > 0 && v < TH[z - 1] - HYST) z--;
  if (z == repeatZone) return;

  repeatZone = z;
  const int LEN[5] = { 0, 8, 4, 2, 1 };
  repeatLen = LEN[z];
  if (repeatLen > 0) armRepeat(repeatLen);
}

// ==============================================================================================
// setup
// ==============================================================================================
void setup() {
  for (int i = 0; i <= 256; i++) sineLUT[i] = sinf(2.0f * (float)M_PI * (float)i / 256.0f);
  for (int i = 0; i < SEMI_N; i++) semiLUT[i] = powf(2.0f, (float)(i + SEMI_MIN) / 12.0f);

  initButtons();
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i < MAX_VOICES; i++) voices[i].active = false;
  for (int i = 0; i < NUM_LEDS; i++) ledFlash[i] = 0.0f;

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHT);
  FastLED.clear();
  FastLED.show();

  // Semilla: el ruido del ADC hace que cada encendido sortee un patrón distinto
  uint32_t seed = 0;
  for (int i = 0; i < 16; i++) seed = seed * 31u + (uint32_t)analogRead(POT_REPEAT);
  prng ^= seed | 1u;

  setBPM(140.0f);
  nuevoPatron();

  duckCoef = decCoef(0.120f);            // sidechain: ~120 ms de recuperación
  filtEnvCoef = expf(-(float)BUF_SAMPLES / (0.085f * SAMPLE_RATE));   // por buffer
  makeLPF(splitCoef, 2200.0f, 0.707f);
  makeLPF(gCur, 12000.0f, 1.0f);
  gTgt = gCur;
  gStp.b0 = gStp.b1 = gStp.b2 = gStp.a1 = gStp.a2 = 0.0f;

  i2s_init();
}

// ==============================================================================================
// loop — un buffer de 128 muestras por vuelta (≈ 2.9 ms)
// ==============================================================================================
void loop() {
  uint32_t now = millis();

  // ── BOTONES (flanco de presión: acción inmediata) ────────────────────────
  for (int i = 0; i < 5; i++) {
    bool lvl = digitalRead(btn[i].pin);
    if (lvl == LOW && btn[i].last == HIGH && (now - btn[i].tDown) > DEBOUNCE_MS) {
      btn[i].tDown = now;
      switch (i) {
        case 0:                                   // BTN1 — PATRÓN NUEVO
          nuevoPatron();
          break;
        case 1:                                   // BTN2 — TIMBRE
          kit = (kit + 1) % 5;
          msgCount = kit + 1; msgTimer = 1.0f;
          break;
        case 2: {                                 // BTN3 — TAP TEMPO
          uint32_t dt = now - lastTap;
          if (lastTap == 0 || dt > 2000) {        // primer tap de la serie: reubica el "1"
            tapN = 0; tapAvg = 0.0f;
            masterStep = NUM_STEPS - 1; stepAcc = stepSamples;
          } else if (dt > 150) {
            tapAvg = (tapN == 0) ? (float)dt : (tapAvg * 0.6f + (float)dt * 0.4f);
            tapN++;
            setBPM(60000.0f / tapAvg);
          }
          lastTap = now;
          break;
        }
        case 3:                                   // BTN4 — MEDIO TIEMPO (mantener)
          halfHeld = true; halfStep = masterStep; halfTick = 1;
          break;
        case 4:                                   // BTN5 — FILL
          fillHeld = true; fillT0 = now; fillCount = 0; fillAcc = 0.0f;
          break;
      }
    }
    if (lvl == HIGH && btn[i].last == LOW) {
      if (i == 3) halfHeld = false;               // al soltar retoma el reloj maestro: sin desfase
      if (i == 4) fillHeld = false;
    }
    btn[i].last = lvl;
  }

  // ── POTS (uno por vuelta, suavizado) ─────────────────────────────────────
  static uint8_t potIdx = 0;
  potIdx = (potIdx + 1) & 3;
  switch (potIdx) {
    case 0: { float v = readPot(POT_VOL);  pVol  += (v - pVol)  * 0.25f; break; }
    case 1: { float v = readPot(POT_FILT); pFilt += (v - pFilt) * 0.25f; break; }
    case 2: { float v = readPot(POT_BODY); pBody += (v - pBody) * 0.25f; break; }
    case 3: { updateRepeat(readPot(POT_REPEAT)); break; }
  }

  const Kit &k = KITS[kit];
  (void)k;

  // ── Reloj (libre: la máquina siempre suena; para silenciar está el POT1) ─
  stepAcc += BUF_SAMPLES;
  while (stepAcc >= stepSamples) { stepAcc -= stepSamples; advanceStep(); }

  if (fillHeld) {
    float prog = (float)(now - fillT0) / 900.0f; if (prog > 1.0f) prog = 1.0f;
    float per  = stepSamples * (0.5f - prog * 0.25f);        // fusas → semifusas
    fillAcc += BUF_SAMPLES;
    while (fillAcc >= per) { fillAcc -= per; fillHit(); }
  }

  // ── Filtro global ────────────────────────────────────────────────────────
  // Curva: 200 Hz → 19 kHz con el exponente comprimido, para que a mitad del recorrido
  // el corte ya esté sobre los 3 kHz. Con una exponencial pura, el punto medio caía en
  // 1.4 kHz y los hats (7–12 kHz) simplemente desaparecían.
  // DINÁMICA: cada golpe con pegada abre el corte, y el efecto es más profundo cuanto
  // más cerrado está el filtro (arriba ya está abierto, no habría nada que abrir).
  // No es un LFO que se mueve solo: responde a lo que suena.
  float envOpen = 1.0f + filtEnv * 3.2f * (1.0f - pFilt);
  float cut = 200.0f * powf(95.0f, powf(pFilt, 0.55f)) * envOpen;
  if (cut > 19000.0f) cut = 19000.0f;
  float qg  = 0.90f + (1.0f - pFilt) * 2.30f;                // hasta 3.2: el barrido canta
  if (pFilt > 0.97f) { cut = 19000.0f; qg = 0.71f; }         // tope = abierto del todo
  makeLPF(gTgt, cut, qg);
  float qComp = 1.0f / powf(qg, 0.35f);                      // el pico resonante no debe clipear
  filtEnv *= filtEnvCoef;
  const float inv = 1.0f / (float)BUF_SAMPLES;
  gStp.b0 = (gTgt.b0 - gCur.b0) * inv; gStp.b1 = (gTgt.b1 - gCur.b1) * inv;
  gStp.b2 = (gTgt.b2 - gCur.b2) * inv; gStp.a1 = (gTgt.a1 - gCur.a1) * inv;
  gStp.a2 = (gTgt.a2 - gCur.a2) * inv;

  // ── CUERPO / SUCIEDAD (POT3): saturación EN DOS BANDAS. Sigue sin haber bit-crush
  //    ni diezmado (eso era aliasing puro), pero el drive de graves/medios llega hasta
  //    ×7.5 — distorsión de verdad donde el material la aguanta. Los agudos apenas se
  //    tocan: saturarlos al mismo nivel es lo que produce el chirrido.
  float body    = pBody;
  float driveLo = 0.75f + body * body * 3.6f;                // 0.75 → 4.35
  float driveHi = 0.75f + body * body * 1.0f;                // 0.75 → 1.75
  float dComp   = 1.0f / (1.0f + body * 0.55f);
  float outGain = pVol * pVol * 2.0f;

  // Barrido de los filtros de ruido (1 recálculo por buffer y voz)
  for (int i = 0; i < MAX_VOICES; i++) {
    Voice &v = voices[i];
    if (!v.active || !v.useF1 || v.fTinc <= 0.0f) continue;
    v.fT += v.fTinc * BUF_SAMPLES; if (v.fT > 1.0f) v.fT = 1.0f;
    float fc = v.fFc + (v.fFcEnd - v.fFc) * v.fT;
    if (v.type == T_SNARE || v.type == T_NOISE) makeBPF(v.bq1, fc, v.fQ);
    else                                        makeHPF(v.bq1, fc, v.fQ);
  }

  // ── Render del buffer ────────────────────────────────────────────────────
  for (int n = 0; n < BUF_SAMPLES; n++) {
    float kickL = 0.0f, kickR = 0.0f;      // bus del bombo (no se agacha a sí mismo)
    float restL = 0.0f, restR = 0.0f;

    for (int i = 0; i < MAX_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;

      // ── Capa TONAL ──
      float tone = 0.0f;
      if (v.env > 0.0002f) {
        v.f += (v.fEnd - v.f) * v.fCoef;
        float dt = v.f * (1.0f / SAMPLE_RATE);
        v.ph += dt; if (v.ph >= 1.0f) v.ph -= 1.0f;
        tone = oscWave(v.wave, v.ph, dt);
        if (v.ratio2 > 0.0f) {
          v.ph2 += dt * v.ratio2; if (v.ph2 >= 1.0f) v.ph2 -= 1.0f;
          tone += sineAt(v.ph2) * v.amp2;
        }
        if (v.ratio3 > 0.0f) {
          v.ph3 += dt * v.ratio3; if (v.ph3 >= 1.0f) v.ph3 -= 1.0f;
          tone += sineAt(v.ph3) * v.amp3;
        }
        if (v.sat > 0.0f) tone = softClip(tone * v.sat) * 0.9f;
        tone *= v.env;
      }

      // ── Capa de RUIDO (envolvente propia: la pegada) ──
      float nz = 0.0f;
      if (v.nAmt > 0.0f && v.nEnv > 0.0002f) {
        nz = noiseF();
        if (v.useF1) nz = biqProc(v.bq1, v.bz1, nz);
        if (v.useF2) nz = biqProc(v.bq2, v.bz2, nz);
        nz *= v.nEnv * v.nAmt;
      }

      // Envolventes: ataque (swell del riser) y luego decaimiento
      if (v.atk < 1.0f) { v.atk += v.atkInc; if (v.atk > 1.0f) v.atk = 1.0f; }
      else { v.env *= v.envCoef; v.nEnv *= v.nCoef; }

      float s = (tone + nz) * v.amp * v.atk;
      if (v.type == T_KICK) { kickL += s * v.lg; kickR += s * v.rg; }
      else                  { restL += s * v.lg; restR += s * v.rg; }

      if (v.atk >= 1.0f && v.env < 0.0006f && v.nEnv < 0.0006f) v.active = false;
    }

    // SIDECHAIN: el bombo agacha el resto ~3 dB → el bombo se SIENTE en vez de competir
    duckEnv *= duckCoef;
    float duck = 1.0f - 0.30f * duckEnv;
    float l = kickL + restL * duck;
    float r = kickR + restR * duck;

    // Saturación en dos bandas (POT3)
    float loL = biqProc(splitCoef, splitZL, l);
    float loR = biqProc(splitCoef, splitZR, r);
    l = (softClip(loL * driveLo) + softClip((l - loL) * driveHi) * 0.9f) * dComp;
    r = (softClip(loR * driveLo) + softClip((r - loR) * driveHi) * 0.9f) * dComp;

    // LPF global (coeficientes interpolados muestra a muestra → sin clics al mover el pot)
    gCur.b0 += gStp.b0; gCur.b1 += gStp.b1; gCur.b2 += gStp.b2;
    gCur.a1 += gStp.a1; gCur.a2 += gStp.a2;
    l = biqProc(gCur, gzL, l) * qComp;
    r = biqProc(gCur, gzR, r) * qComp;

    // Bloqueador de DC (un polo a ~20 Hz): quita el offset que dejan los barridos de pitch
    float yl = l - dcX1L + 0.9985f * dcY1L; dcX1L = l; dcY1L = yl; l = yl;
    float yr = r - dcX1R + 0.9985f * dcY1R; dcX1R = r; dcY1R = yr; r = yr;

    // Volumen + limitador suave
    l = softClip(l * outGain);
    r = softClip(r * outGain);

    int32_t li = (int32_t)(l * 30000.0f);
    int32_t ri = (int32_t)(r * 30000.0f);
    if (li >  32767) li =  32767; if (li < -32768) li = -32768;
    if (ri >  32767) ri =  32767; if (ri < -32768) ri = -32768;
    audioBuf[n * 2]     = (int16_t)li;
    audioBuf[n * 2 + 1] = (int16_t)ri;
  }
  gCur = gTgt;

  size_t written;
  i2s_channel_write(tx_chan, audioBuf, sizeof(audioBuf), &written, portMAX_DELAY);

  if (now - lastShow >= 33) { lastShow = now; renderLeds(); }
}
