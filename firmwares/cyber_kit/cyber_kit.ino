// ==============================================================================================================================================
// PERCU-SYNTH — CYBER KIT: secuenciador de texturas, FX rítmicos y leads cyber (sin samples) — GC Lab Chile
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
// - IMU MPU6050 (acelerómetro I2C) |SDA -> 21, SCL -> 38, VCC -> 3.3V, GND -> GND|  (dirección 0x68)
// - 6 LEDs WS2812 SMD internos de la placa |DATA -> 46| (índices 0..5 de la tira)
// - LED RGB direccionable del módulo ESP32-S3 (DevKitC-1) |DATA -> 48| (refleja la tira)
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - Flash Mode         : DIO          (¡OPI rompe I2S!)
// - PSRAM              : OPI PSRAM
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (incluye driver/i2s_std.h)
// - Wire.h (I2C, incluida en el core) — para el MPU6050
// - FastLED (gestor de librerías Arduino) — para los 6 LEDs de placa
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// CYBER KIT no es una drum machine: es un SECUENCIADOR DE TEXTURAS, FX RÍTMICOS
// y MELODÍAS/LEADS CYBER, 100% sintetizado en tiempo real (sin samples):
// leads neón, campanas FM, hoovers, risers, nubes granulares, drones oscuros,
// zaps, downlifters, glitch bursts, growls, reeses, subs…
//
// - RESPUESTA INSTANTÁNEA: los botones disparan en el flanco de presión (sin
//   ventana de espera) y la cola DMA es corta (~12 ms). Si un combo de dos
//   botones se forma dentro de 50 ms, la acción de cada botón se DESHACE
//   (los golpes se apagan con un fade de 4 ms, inaudible) → tocar es inmediato
//   y los combos siguen siendo confiables.
// - 4 BANCOS de 5 sonidos: LEADS · TEXTURAS · FX · BAJOS.
// - Los sonidos afinados viven DENTRO de una ESCALA elegible (10 escalas:
//   modos griegos + flamencas oscuras) con nota fundamental variable.
// - MACROS DE SÍNTESIS que transforman los sonidos de forma NOTABLE (Panel C):
//   attack (un golpe se convierte en swell), decay (de click a cola larga),
//   textura (FM + ruido blanco) y pitch global (±1 octava).
// - 2 MODOS DE TOCAR (BTN1+BTN3): PERC (cada botón = un sonido; los afinados
//   caminan por el patrón de notas) y SEQ (secuenciador 16 pasos × 5 slots
//   con TRANSPORTE COMPLETO: play/stop, reversa, beat repeat, velocidad, caos).
// - 2 MODOS DE FILTRO global (BTN1+BTN5 >1 s): POTS (cutoff + resonancia) o
//   IMU (aceleración X → cutoff · Y → resonancia).
// - Cadena global: mixer texturas↔leads → DRIVE → LPF resonante con WOBBLE
//   (LFO → cutoff) → volumen master → soft-clip.
//
// Motor de paneles/pots congelados de pads_imu_leds: al cambiar de panel los
// pots quedan CONGELADOS y sólo retoman el control al moverlos (≥ 4 % en 3
// lecturas) → TODOS los valores persisten al cambiar de panel.
// ==============================================================================================================================================
// FUNCIONAMIENTO (botones)
// ==============================================================================================================================================
// COMBOS (dos botones casi a la vez, dentro de 50 ms):
//   · BTN1 + BTN3           → cambia MODO DE TOCAR (PERC ↔ SEQ)
//   · BTN2 + BTN4           → siguiente BANCO de sonidos (1..4)
//   · BTN3 + BTN5           → entra/sale del PANEL D (escala / secuencia) +
//   · BTN4 + BTN5           → cicla los paneles A → B → C → A
//   · BTN1 + BTN5 (>1 seg)  → cambia MODO DE FILTRO (POTS ↔ IMU)
// +56222509344
// MODO PERC : BTN1..BTN5 disparan los 5 sonidos del banco activo (al instante).
//             Los sonidos afinados avanzan por el patrón de notas en cada golpe.
// MODO SEQ  : transporte de performance (el secuenciador toca el patrón):
//   · BTN1 → PLAY / STOP (play reinicia en el paso 0)
//   · BTN2 → REVERSA (invierte la dirección del secuenciador)
//   · BTN3 → BEAT REPEAT (mantener: loopea 2 pasos; al soltar sigue en tiempo)
//   · BTN4 → VELOCIDAD (cicla ×1 → ×2 → ×½)
//   · BTN5 → CAOS (mantener: dispara golpes/notas aleatorias de la escala)
// ==============================================================================================================================================
// FUNCIONAMIENTO (4 paneles de potenciómetros)
// ==============================================================================================================================================
// ── PANEL A — MEZCLA (cian) — panel inicial ───────────────────────────────────
// - POT1 → Volumen master · POT2 → Mixer texturas ↔ leads (balance de buses)
// - POT3 → Drive (saturación) · POT4 → Tempo (60–180 BPM)
//
// ── PANEL B — FILTRO / LFO (violeta) ──────────────────────────────────────────
// - POT1 → Cutoff (modo POTS) · POT2 → Resonancia (modo POTS)
// - POT3 → Velocidad del LFO (0.25–16 Hz) · POT4 → WOBBLE (LFO → cutoff)
//   En modo filtro IMU, POT1/POT2 no actúan: manda el acelerómetro.
//
// ── PANEL C — SÍNTESIS (naranja) — transforma los sonidos de forma NOTABLE ────
// - POT1 → Attack (0.5 ms – 0.8 s: de golpe a swell/reverse)
// - POT2 → Decay (×0.1 – ×8 del decay natural: de click a cola larga)
// - POT3 → TEXTURA (FM/armónicos + capa de ruido blanco)
// - POT4 → PITCH global (−12 … +12 semitonos, afecta a todos los sonidos)
//
// ── PANEL D — ESCALA / SECUENCIA (verde) — BTN3+BTN5 ──────────────────────────
// - POT1 → Nota fundamental (Do..Si) · POT2 → Escala (10): Jónico · Dórico ·
//   Frigio · Lidio · Mixolidio · Eólico · Locrio · Frigio dominante (flamenca) ·
//   Doble armónica (gitana oscura) · Menor armónica
// - POT3 → Patrón rítmico (8) · POT4 → Patrón de notas (8)
// ==============================================================================================================================================
// FUNCIONAMIENTO (LEDs — 6 SMD de placa)
// ==============================================================================================================================================
// - PALETA = PANEL ACTIVO: A cian · B violeta · C naranja · D verde (+ flash
//   blanco al cambiar). El filtro/wobble desplaza el tono del color.
// - LEDs 0-4 = flash por slot/sonido al disparar · LED 5 = pulso del beat (SEQ)
//   o VU de energía (PERC). SEQ detenido → respiración tenue.
// - CAOS → chispas blancas · BEAT REPEAT → LED 5 rojo.
// - MENSAJES: banco → N LEDs amarillos · SEQ verde / PERC azul · filtro IMU
//   violeta / POTS blanco · velocidad → 1 (×½) / 2 (×1) / 4 (×2) LEDs celestes.
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <Wire.h>
#include <FastLED.h>
#include <math.h>

// ─── Tipos (arriba del todo para que el IDE genere bien los prototipos) ───
struct SoundDef {
  const char* name;
  uint8_t wave;        // 0 seno · 1 sierra · 2 cuadrada
  bool    tuned;       // true → f0/f1 son MULTIPLICADORES de la nota de la escala · false → Hz absolutos
  int8_t  fixedDeg;    // -1 = sigue el patrón de notas · ≥0 = grado fijo de la escala
  int8_t  octOff;      // octavas de desplazamiento (sólo tuned)
  float   f0, f1;      // frecuencia inicio → fin del sweep (Hz o multiplicador)
  float   dropMs;      // duración del sweep de pitch / del barrido del ruido (ms) — 0 = sin sweep
  float   fmRatio, fmAmt;   // operador FM — la macro TEXTURA lo escala y añade FM a todos
  float   det2;        // 2º oscilador desafinado (ratio, p.ej. 1.012) — 0 = off (leads/reese)
  float   noiseMix;    // mezcla oscilador↔ruido propio (0..1; 1 = sólo ruido)
  float   noiseFc, noiseFcEnd, noiseQ;  // BP del ruido; FcEnd > 0 = barrido del BP a lo largo de dropMs
  float   atkMs;       // attack MÍNIMO del sonido (ms) — swells/risers/reverses
  float   decayS;      // decay natural (s) — el POT2 del Panel C lo escala ×0.1–×8
  float   plfoHz, plfoAmt;  // LFO cuadrado de pitch (sirena/trino/beam) — 0 = off
  float   gateHz;      // compuerta AM (stutter/granular) — 0 = off
  float   rndPitch;    // aleatoriedad de pitch por ciclo de compuerta (nube granular)
  float   gain, pan;   // mezcla y paneo fijo (-1..+1)
  bool    leadBus;     // true → bus LEADS · false → bus TEXTURAS/FX (mixer POT2 Panel A)
};

struct Voice {
  bool     active;
  uint8_t  wave;
  bool     lead;
  float    fStart, fEnd, t, dropInc;
  float    phase1, phase2, phase3;
  float    fmRatio, fmAmt, det2;
  float    noiseMix;
  bool     useNBQ;
  float    nFc0, nFcEnd, nQ;
  float    nb0, nb1, nb2, na1, na2;   // coefs BP del ruido
  float    nx1, nx2, ny1, ny2;        // estado BP del ruido
  uint32_t nseed, nCnt;
  float    plfoInc, plfoAmt, plfoPhase;
  float    gateInc, gatePhase, rndPitch, rndMul;
  float    env, atkInc, decCoef;
  uint8_t  stage;                     // 0 attack · 1 decay · 2 kill rápido (combo deshecho)
  float    gain, lG, rG;
  uint32_t age;
};

struct BiqState { float x1, x2, y1, y2; };

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128

// ─── IMU MPU6050 (I2C) ─────────────────────────────────────
#define SDA_PIN     21
#define SCL_PIN     38
#define IMU_ADDR    0x68
const unsigned long IMU_READ_INTERVAL = 10;
const float IMU_FILTER_ALPHA = 0.1f;

// ─── LEDs WS2812 (6 SMD internos + LED del módulo) ─────────
#define LED_PIN        46
#define NUM_LEDS        6
#define LED_BRIGHT     250
#define LED_TYPE       WS2812
#define COLOR_ORDER    GRB
const unsigned long LED_REFRESH_MS = 22;
#define ONBOARD_PIN    48
#define ONBOARD_BRIGHT 45

CRGB leds[NUM_LEDS];
CRGB onboard[1];

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
const uint8_t BTN_PIN[5] = {44, 42, 0, 45, 47};
const uint32_t PRESS_LOCKOUT_MS = 20;   // anti-rebote por flanco (dispara AL INSTANTE)
const uint32_t COMBO_MS         = 50;   // dos botones dentro de esta ventana = combo (se deshacen sus acciones)
const uint32_t HOLD_FILTER_MS   = 1000; // BTN1+BTN5 sostenido → cambia modo de filtro

// ─── Potenciómetros ────────────────────────────────────────
const uint8_t POT_PIN[4] = { 1, 2, 8, 10 };   // ADC1, ADC2, ADC8, ADC10

// ─── Polifonía ─────────────────────────────────────────────
#define NUM_VOICES 14
Voice voices[NUM_VOICES];
uint32_t voiceCounter = 0;

const float BASE_FREQ = 130.81f;      // C3 (semitono 0)

// ─── Tabla semitono → relación de frecuencia ───────────────
#define SEMI_OFFSET 60
#define SEMI_LUT_N  121
float semiLUT[SEMI_LUT_N];
inline float semiToFreq(int semi) {
  int idx = semi + SEMI_OFFSET;
  if (idx < 0) idx = 0; else if (idx >= SEMI_LUT_N) idx = SEMI_LUT_N - 1;
  return BASE_FREQ * semiLUT[idx];
}

// ─── Tabla de seno ─────────────────────────────────────────
float sineLUT[256];
inline float oscSine(float phase) {
  float f = phase * 256.0f;
  int i0 = (int)f; float frac = f - (float)i0;
  i0 &= 255; int i1 = (i0 + 1) & 255;
  return sineLUT[i0] + (sineLUT[i1] - sineLUT[i0]) * frac;
}

// ─── Ruido blanco LCG por voz ──────────────────────────────
inline float lcgNoise(uint32_t &s) {
  s = s * 1664525u + 1013904223u;
  return (float)(int32_t)s * (1.0f / 2147483648.0f);
}

// ─── ESCALAS (10) — grados en semitonos desde la fundamental ──
#define NUM_SCALES 10
const int8_t SCALES[NUM_SCALES][7] = {
  {0, 2, 4, 5, 7, 9, 11},   // 0 Jónico (mayor)
  {0, 2, 3, 5, 7, 9, 10},   // 1 Dórico
  {0, 1, 3, 5, 7, 8, 10},   // 2 Frigio
  {0, 2, 4, 6, 7, 9, 11},   // 3 Lidio
  {0, 2, 4, 5, 7, 9, 10},   // 4 Mixolidio
  {0, 2, 3, 5, 7, 8, 10},   // 5 Eólico (menor natural)
  {0, 1, 3, 5, 6, 8, 10},   // 6 Locrio
  {0, 1, 4, 5, 7, 8, 10},   // 7 Frigio dominante (flamenca / española)
  {0, 1, 4, 5, 7, 8, 11},   // 8 Doble armónica (gitana oscura)
  {0, 2, 3, 5, 7, 8, 11},   // 9 Menor armónica
};
const char* SCALE_NAMES[NUM_SCALES] = {
  "Jonico", "Dorico", "Frigio", "Lidio", "Mixolidio",
  "Eolico", "Locrio", "FrigioDom", "DobleArm", "MenorArm"
};

// ─── PATRONES RÍTMICOS (8) — 16 pasos × 5 slots (bit 0 = paso 0) ──
// Slots: 0 ancla (denso grave) · 1 acentos · 2 rápido/ticks · 3 medio · 4 escaso
#define NUM_RHYTHMS 8
const uint16_t RHYTHMS[NUM_RHYTHMS][5] = {
  //  SLOT1(ancla)          SLOT2(acentos)        SLOT3(ticks)          SLOT4(medio)          SLOT5(escaso)
  { 0b0001000100010001, 0b0001000000010000, 0b0100010001000100, 0b0100100101001001, 0b1000000010000000 }, // 0 TECHNO
  { 0b0000000010000001, 0b0000000100000000, 0b1100010011000100, 0b0100000101000001, 0b0001000000000000 }, // 1 DUBSTEP (halftime)
  { 0b0000010000000001, 0b1001000000010000, 0b0101010101010101, 0b0000010010000001, 0b0010000000100000 }, // 2 BREAKBEAT
  { 0b0000100000001001, 0b0000000100000000, 0b1111111111111111, 0b0000100100000001, 0b0100000000000000 }, // 3 TRAP
  { 0b0000000101000001, 0b0001000000010000, 0b0100010001000100, 0b0101010101010101, 0b1000100010001000 }, // 4 ELECTRO
  { 0b0000010000000001, 0b0001000000010000, 0b0101010101110101, 0b0000010000100001, 0b0100000001000000 }, // 5 DNB
  { 0b0001001001001001, 0b0100000010000100, 0b0010100100110010, 0b0001000001000001, 0b0000001000000000 }, // 6 TRIBAL
  { 0b0000000100000001, 0b0001000000000000, 0b0100000001000000, 0b0000100000000001, 0b1000000000000000 }, // 7 MINIMAL
};
const char* RHYTHM_NAMES[NUM_RHYTHMS] = {
  "TECHNO", "DUBSTEP", "BREAKBEAT", "TRAP", "ELECTRO", "DNB", "TRIBAL", "MINIMAL"
};

// ─── PATRONES DE NOTAS (8) — 16 grados de la escala (0 = fundamental, 7 = octava) ──
#define NUM_NOTEPATS 8
const int8_t NOTE_PATTERNS[NUM_NOTEPATS][16] = {
  { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },   // 0 PEDAL
  { 0,7,0,7, 0,7,0,7, 0,7,0,7, 0,7,0,7 },   // 1 OCTAVAS
  { 0,0,4,0, 0,4,0,4, 0,0,4,0, 7,4,0,4 },   // 2 QUINTAS
  { 0,1,2,3, 4,5,6,7, 7,6,5,4, 3,2,1,0 },   // 3 SUBE-BAJA
  { 0,0,1,0, 0,3,1,0, 0,0,1,0, 4,3,1,0 },   // 4 RIFF OSCURO (aprovecha la 2ª menor frigia)
  { 0,4,7,4, 2,5,9,5, 0,4,7,4, 1,5,8,5 },   // 5 SALTOS
  { 0,0,0,7, 0,0,0,6, 0,0,0,5, 0,0,0,4 },   // 6 PEDAL CAE
  { 0,3,6,1, 7,2,5,0, 4,1,8,3, 6,2,7,5 },   // 7 CAOS
};
const char* NOTEPAT_NAMES[NUM_NOTEPATS] = {
  "PEDAL", "OCTAVAS", "QUINTAS", "SUBE-BAJA", "RIFF OSCURO", "SALTOS", "PEDAL CAE", "CAOS"
};

// ─── BANCOS DE SONIDOS (4 × 5) — texturas / FX / leads, cero samples ──
#define NUM_BANKS 4
const SoundDef SOUND_BANKS[NUM_BANKS][5] = {
  { // ── BANCO 0 — LEADS (melodías cyber, todas afinadas) ─────
    //  name        wav tuned fDg oct  f0     f1    drop  fmR   fmA   det2    nMix  nFc   nFcE  nQ    atk  decay  plHz  plA    gate  rnd  gain   pan    lead
    { "NeonLead",   1, true, -1,  0,  1.0f,  1.0f,   0,  0,    0,    1.012f, 0,    0,    0,    0,     0,  0.35f,  0,    0,     0,    0,   0.80f, -0.10f, true },
    { "CampanaFM",  0, true, -1,  1,  1.0f,  1.0f,   0,  3.5f, 1.8f, 0,      0,    0,    0,    0,     0,  0.55f,  0,    0,     0,    0,   0.70f,  0.25f, true },
    { "AcidSq",     2, true, -1,  0,  1.0f,  1.0f,   0,  0,    0,    0,      0,    0,    0,    0,     0,  0.30f,  5.5f, 0.035f,0,    0,   0.75f, -0.25f, true },
    { "Hoover",     1, true, -1,  0,  0.5f,  1.0f, 120,  1.0f, 0.5f, 1.020f, 0,    0,    0,    0,     0,  0.65f,  0,    0,     0,    0,   0.80f,  0.10f, true },
    { "Chip",       2, true, -1,  2,  1.0f,  1.0f,   0,  0,    0,    0,      0,    0,    0,    0,     0,  0.10f, 24.0f, 0.33f, 0,    0,   0.55f,  0.35f, true },
  },
  { // ── BANCO 1 — TEXTURAS (paisajes rítmicos) ────────────────
    { "Riser",      0, false,-1,  0,  200,   200,  1500, 0,    0,    0,      1.0f, 300,  6500, 2.5f, 250,  1.80f,  0,    0,     0,    0,   0.80f,  0.00f, false },
    { "Nube",       0, true, -1,  1,  1.0f,  1.0f,   0,  0,    0,    0,      0.25f,3000, 0,    1.0f, 120,  1.30f,  0,    0,    14.0f, 0.8f,0.70f, -0.20f, false },
    { "DronOscuro", 1, true,  0, -1,  1.0f,  1.0f,   0,  0.5f, 0.8f, 1.006f, 0,    0,    0,    0,    400,  2.60f,  0,    0,     0,    0,   0.75f,  0.00f, false },
    { "Brillo",     0, true, -1,  2,  1.0f,  1.0f,   0,  2.76f,2.2f, 0,      0,    0,    0,    0,    150,  1.80f,  0,    0,     0,    0,   0.55f,  0.30f, false },
    { "Polvo",      0, false,-1,  0,  100,   100,    0,  0,    0,    0,      1.0f, 5500, 0,    0.8f,  0,   1.60f,  0,    0,     7.0f, 0,   0.60f, -0.30f, false },
  },
  { // ── BANCO 2 — FX RÍTMICOS ─────────────────────────────────
    { "Zap",        2, true, -1,  1,  6.0f,  0.5f,  30,  0,    0,    0,      0,    0,    0,    0,     0,  0.15f,  0,    0,     0,    0,   0.70f,  0.30f, false },
    { "Reversa",    1, true, -1,  0,  1.0f,  1.0f,   0,  0,    0,    1.015f, 0,    0,    0,    0,    350,  0.10f,  0,    0,     0,    0,   0.80f, -0.10f, false },
    { "Down",       1, false,-1,  0, 1400,    90,   900, 0,    0,    0,      0.3f, 2200, 0,    1.0f,  0,   1.00f,  0,    0,     0,    0,   0.75f,  0.00f, false },
    { "Beam",       2, true, -1,  1,  1.0f,  1.0f,   0,  0,    0,    0,      0,    0,    0,    0,     0,  0.50f,  9.0f, 0.22f, 0,    0,   0.65f, -0.30f, false },
    { "GlitchBurst",0, false,-1,  0,  200,   200,   260, 0,    0,    0,      1.0f, 3800, 700,  2.0f,  0,   0.30f,  0,    0,    33.0f, 0,   0.85f,  0.20f, false },
  },
  { // ── BANCO 3 — BAJOS CYBER (todos afinados) ────────────────
    { "Growl",      0, true, -1, -1,  1.0f,  1.0f,   0,  2.01f,2.6f, 0,      0,    0,    0,    0,     0,  0.50f,  0,    0,     0,    0,   0.95f,  0.00f, true },
    { "Reese",      1, true, -1, -1,  1.0f,  1.0f,   0,  0,    0,    1.018f, 0,    0,    0,    0,     0,  0.90f,  0,    0,     0,    0,   0.85f,  0.00f, true },
    { "SubPunch",   0, true, -1, -2,  2.0f,  1.0f,  25,  0,    0,    0,      0,    0,    0,    0,     0,  0.35f,  0,    0,     0,    0,   1.00f,  0.00f, true },
    { "Talk",       0, true, -1, -1,  1.0f,  1.0f,   0,  0.5f, 2.2f, 0,      0,    0,    0,    0,     0,  0.60f,  6.0f, 0.10f, 0,    0,   0.85f,  0.10f, true },
    { "Stab",       1, true, -1,  0,  1.0f,  1.0f,   0,  1.0f, 0.6f, 1.010f, 0,    0,    0,    0,     0,  0.18f,  0,    0,     0,    0,   0.80f, -0.15f, true },
  },
};
const char* BANK_NAMES[NUM_BANKS] = { "LEADS", "TEXTURAS", "FX", "BAJOS" };

// ─── Estado musical (Panel D) — PERSISTENTE ────────────────
int rootSemi  = 0;    // fundamental: 0 = Do .. 11 = Si
int scaleIdx  = 7;    // arranca en Frigio dominante (carácter flamenco/cyber oscuro)
int rhythmPat = 1;    // arranca en DUBSTEP
int notePat   = 4;    // arranca en RIFF OSCURO

inline int degToSemi(int deg) {
  int oct = deg / 7, idx = deg % 7;
  if (idx < 0) { idx += 7; oct--; }
  return oct * 12 + SCALES[scaleIdx][idx];
}

// ─── Modos ─────────────────────────────────────────────────
bool seqMode   = false;   // false = PERC (botones disparan) · true = SEQ (transporte)
bool filterIMU = false;   // false = filtro por POTS · true = filtro por IMU
int  bank      = 0;

// ─── Secuenciador / transporte (modo SEQ) ──────────────────
bool  seqPlaying  = false;
int   seqDir      = 1;      // +1 normal · -1 REVERSA
int   masterStep  = 15;     // posición "real" (sigue avanzando durante el beat repeat)
float stepCounter = 0.0f;
float stepSamples = (60.0f / 128.0f / 4.0f) * SAMPLE_RATE;
float bpm         = 128.0f;
int   speedIdx    = 0;      // 0 = ×1 · 1 = ×2 · 2 = ×½
const float SPEED_MUL[3] = { 1.0f, 2.0f, 0.5f };
bool  repeatHeld  = false;  // BTN3 sostenido → beat repeat (loop de 2 pasos)
int   repStart    = 0;
int   repIdx      = 0;
bool  chaosHeld   = false;  // BTN5 sostenido → caos (golpes aleatorios en escala)
uint32_t chaosRng = 0xC0FFEEu;

// ─── Contadores melódicos del modo PERC ────────────────────
uint8_t percCtr[5] = { 0, 0, 0, 0, 0 };

// ─── Paneles de control (pots) ─────────────────────────────
#define PANEL_A 0   // MEZCLA
#define PANEL_B 1   // FILTRO / LFO
#define PANEL_C 2   // SÍNTESIS
#define PANEL_D 3   // ESCALA / SECUENCIA
int panel = PANEL_A;

inline uint8_t panelHue() {
  switch (panel) {
    case PANEL_A: return 140;
    case PANEL_B: return 192;
    case PANEL_C: return 24;
    default:      return 96;
  }
}

// ─── Parámetros globales (todos PERSISTEN al cambiar de panel) ──
// Panel A — mezcla
float masterVol  = 0.55f;
float texLvl     = 0.92f;     // bus TEXTURAS/FX (equal-power con leads)
float leadLvl    = 0.92f;     // bus LEADS
float drive      = 0.20f;
// Panel B — filtro / LFO
float cutoffBase = 11000.0f;
float qBase      = 1.0f;
float lfoRate    = 4.0f;
float lfoAmt     = 0.0f;
// Panel C — síntesis (macros NOTABLES)
float globalAtkS = 0.001f;    // attack global (s) — cada sonido puede tener un mínimo mayor
float decayMult  = 1.0f;      // ×0.1 – ×8
float texMacro   = 0.30f;     // FM/armónicos + ruido blanco
float gNoiseMix  = 0.30f * 0.35f;
float pitchRatio = 1.0f;      // ±12 semitonos globales

// ─── LFO wobble ────────────────────────────────────────────
float lfoPhase = 0.0f;
float lfoVal   = 0.0f;

// ─── Pots congelados al cambiar de panel (motor de pads_imu) ──
float   potAnchor[4]  = {0, 0, 0, 0};
bool    potLive[4]    = {true, true, true, true};
uint8_t potMoveCnt[4] = {0, 0, 0, 0};
bool    panelChanged  = false;
const float POT_MOVE_THR = 0.04f;

// ─── IMU ───────────────────────────────────────────────────
float imu_x = 0.0f, imu_y = 0.0f;
float filtered_x = 0.0f, filtered_y = 0.0f;
unsigned long lastIMURead = 0;

// ─── Filtro global biquad LPF resonante (estéreo) ──────────
float f_b0, f_b1, f_b2, f_a1, f_a2;
BiqState bqL = {0, 0, 0, 0};
BiqState bqR = {0, 0, 0, 0};

// ─── Botones: disparo instantáneo + combos con "deshacer" ──
bool     rawLast[5]  = {false, false, false, false, false};
uint32_t lastEdge[5] = {0, 0, 0, 0, 0};
bool     lvl[5]      = {false, false, false, false, false};
uint32_t downAt[5]   = {0, 0, 0, 0, 0};
// última voz disparada por cada botón (para matarla si el botón era parte de un combo)
int      lastVoiceIdx[5] = {-1, -1, -1, -1, -1};
uint32_t lastVoiceAge[5] = {0, 0, 0, 0, 0};
// latches de combos (no refire mientras se mantienen)
bool c13 = false, c24 = false, c35 = false, c45 = false, c15 = false;
uint32_t c15At = 0; bool c15Fired = false;

// ─── Métricas para los LEDs (mismo hilo, sin locks) ────────
float g_energy      = 0.0f;
float g_hitFlash[5] = {0, 0, 0, 0, 0};
float g_beatFlash   = 0.0f;
float flashLevel    = 0.0f;
#define MSG_NONE   0
#define MSG_BANK   1
#define MSG_SEQ    2
#define MSG_PERC   3
#define MSG_FIMU   4
#define MSG_FPOTS  5
#define MSG_SPEED  6
uint8_t  msgType  = MSG_NONE;
uint32_t msgUntil = 0;

static i2s_chan_handle_t tx_chan;

// ─── Lectura de pot con sobre-muestreo ─────────────────────
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (float)(sum >> 3) / 4095.0f;
}

// ─── Coeficientes biquad paso-banda (ruido de cada voz) ────
void biquadBP(float fc, float Q, float &b0, float &b1, float &b2, float &a1, float &a2) {
  float w0    = 2.0f * (float)M_PI * fc / SAMPLE_RATE;
  float alpha = sinf(w0) / (2.0f * Q);
  float cosw0 = cosf(w0);
  float norm  = 1.0f + alpha;
  b0 =  alpha / norm;
  b1 =  0.0f;
  b2 = -alpha / norm;
  a1 = (-2.0f * cosw0) / norm;
  a2 = (1.0f - alpha)  / norm;
}

// ─── Disparar un sonido del banco activo — devuelve el índice de voz ──
// step ≥ 0 → viene del secuenciador (grado del patrón en ese paso).
// step  < 0 → modo PERC (avanza el contador melódico de la pista).
// degOverride > -100 → grado forzado (modo CAOS).
int triggerSound(int trk, int step, int degOverride) {
  const SoundDef &d = SOUND_BANKS[bank][trk];

  int idx = -1;
  for (int i = 0; i < NUM_VOICES; i++)
    if (!voices[i].active) { idx = i; break; }
  if (idx < 0) {
    float lo = 1e30f; idx = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
      float a = voices[i].env * voices[i].gain;
      if (a < lo) { lo = a; idx = i; }
    }
  }

  Voice &v = voices[idx];
  v.active = true;
  v.wave   = d.wave;
  v.lead   = d.leadBus;

  // ── Afinación dentro de la escala + PITCH global (macro Panel C) ──
  if (d.tuned) {
    int deg;
    if (degOverride > -100)   deg = degOverride;
    else if (d.fixedDeg >= 0) deg = d.fixedDeg;
    else if (step >= 0) {     deg = NOTE_PATTERNS[notePat][step];
                              if (trk == 4) deg += 7; }          // el slot 5 canta una octava arriba
    else {                    deg = NOTE_PATTERNS[notePat][percCtr[trk] & 15];
                              percCtr[trk]++; }                  // PERC: cada golpe camina el patrón
    float base = semiToFreq(rootSemi + degToSemi(deg) + d.octOff * 12);
    v.fStart = base * d.f0;
    v.fEnd   = base * d.f1;
  } else {
    v.fStart = d.f0;
    v.fEnd   = d.f1;
  }
  v.fStart *= pitchRatio;                 // pitch global ±12 semitonos (afecta TODO)
  v.fEnd   *= pitchRatio;

  if (d.dropMs > 0) { v.t = 0.0f; v.dropInc = 1.0f / (d.dropMs * 0.001f * SAMPLE_RATE); }
  else              { v.t = 1.0f; v.dropInc = 0.0f; }

  v.phase1 = 0.0f; v.phase2 = 0.0f; v.phase3 = 0.0f;
  v.det2   = d.det2;

  // Macro TEXTURA: escala el FM del preset y añade FM base a todos los sonidos
  float fmEff = d.fmAmt * (0.25f + texMacro * 2.75f) + texMacro * 1.1f;
  v.fmRatio = (d.fmRatio > 0.0f) ? d.fmRatio : 2.0f;
  v.fmAmt   = fmEff;

  v.noiseMix = d.noiseMix;
  v.nFc0 = d.noiseFc; v.nFcEnd = d.noiseFcEnd; v.nQ = d.noiseQ;
  if (d.noiseFc > 0.0f) {
    v.useNBQ = true;
    biquadBP(d.noiseFc, d.noiseQ, v.nb0, v.nb1, v.nb2, v.na1, v.na2);
    v.nx1 = v.nx2 = v.ny1 = v.ny2 = 0.0f;
  } else v.useNBQ = false;
  v.nseed = esp_random(); v.nCnt = 0;

  v.plfoAmt = d.plfoAmt; v.plfoInc = d.plfoHz / SAMPLE_RATE; v.plfoPhase = 0.0f;
  v.gateInc = d.gateHz / SAMPLE_RATE; v.gatePhase = 0.0f;
  v.rndPitch = d.rndPitch; v.rndMul = 1.0f;

  // Envolvente: attack = máx(global, mínimo del sonido) · decay natural × macro global
  float atkS = globalAtkS;
  float minAtk = d.atkMs * 0.001f;
  if (minAtk > atkS) atkS = minAtk;
  v.env = 0.0f; v.stage = 0;
  v.atkInc  = 1.0f / (atkS * SAMPLE_RATE);
  v.decCoef = expf(-6.5f / (d.decayS * decayMult * SAMPLE_RATE));
  v.gain = d.gain;
  v.age  = voiceCounter++;

  float p = d.pan;
  if (p >  1.0f) p =  1.0f;
  if (p < -1.0f) p = -1.0f;
  float phr = (p + 1.0f) * 0.125f;
  v.rG = oscSine(phr);
  v.lG = oscSine(phr + 0.25f);

  g_hitFlash[trk] = 1.0f;
  return idx;
}

// ─── Matar una voz con fade de ~4 ms (deshacer un golpe de combo) ──
void fastKill(int idx, uint32_t expectAge) {
  if (idx < 0 || idx >= NUM_VOICES) return;
  Voice &v = voices[idx];
  if (!v.active || v.age != expectAge) return;   // la voz ya fue robada/reusada
  v.stage = 2;
}

// ─── Render de UNA muestra de una voz ──────────────────────
inline float renderVoice(Voice &v) {
  if (v.t < 1.0f) { v.t += v.dropInc; if (v.t > 1.0f) v.t = 1.0f; }
  float f = v.fStart + (v.fEnd - v.fStart) * v.t;

  // LFO cuadrado de pitch (sirena / beam / trino)
  if (v.plfoInc > 0.0f && v.plfoAmt > 0.0f) {
    v.plfoPhase += v.plfoInc;
    if (v.plfoPhase >= 1.0f) v.plfoPhase -= 1.0f;
    f *= (v.plfoPhase < 0.5f) ? (1.0f + v.plfoAmt) : (1.0f - v.plfoAmt);
  }
  f *= v.rndMul;                          // pitch aleatorio por grano (nube granular)

  float dt = f / SAMPLE_RATE;
  if (dt > 0.45f) dt = 0.45f;
  v.phase1 += dt; v.phase1 -= (int)v.phase1;

  // FM — escalada por la envolvente (el sonido se limpia al decaer)
  float mod = 0.0f;
  if (v.fmAmt > 0.0f) {
    v.phase2 += dt * v.fmRatio; v.phase2 -= (int)v.phase2;
    mod = oscSine(v.phase2) * v.fmAmt * 0.12f * v.env;
  }
  float ph = v.phase1 + mod + 1.0f; ph -= (int)ph;

  float osc;
  if      (v.wave == 0) osc = oscSine(ph);
  else if (v.wave == 1) osc = 2.0f * ph - 1.0f;
  else                  osc = (ph < 0.5f) ? 0.8f : -0.8f;

  // 2º oscilador desafinado (leads gordos / reese)
  if (v.det2 > 0.0f) {
    v.phase3 += dt * v.det2; v.phase3 -= (int)v.phase3;
    float o2;
    if      (v.wave == 0) o2 = oscSine(v.phase3);
    else if (v.wave == 1) o2 = 2.0f * v.phase3 - 1.0f;
    else                  o2 = (v.phase3 < 0.5f) ? 0.8f : -0.8f;
    osc = (osc + o2) * 0.6f;
  }

  // ruido propio (BP opcional, con barrido si nFcEnd > 0) + capa de ruido global
  float n  = lcgNoise(v.nseed);
  float nf = n;
  if (v.useNBQ) {
    v.nCnt++;
    if (v.nFcEnd > 0.0f && (v.nCnt & 127) == 0) {   // barrido del BP (riser / glitch caída)
      float fc = v.nFc0 + (v.nFcEnd - v.nFc0) * v.t;
      biquadBP(fc, v.nQ, v.nb0, v.nb1, v.nb2, v.na1, v.na2);
    }
    nf = v.nb0 * n + v.nb1 * v.nx1 + v.nb2 * v.nx2 - v.na1 * v.ny1 - v.na2 * v.ny2;
    v.nx2 = v.nx1; v.nx1 = n;
    v.ny2 = v.ny1; v.ny1 = nf;
  }
  float s = osc * (1.0f - v.noiseMix) + nf * v.noiseMix + n * gNoiseMix * 0.6f;

  // compuerta AM (stutter / granular) — al reiniciar el ciclo sortea el pitch del grano
  if (v.gateInc > 0.0f) {
    v.gatePhase += v.gateInc;
    if (v.gatePhase >= 1.0f) {
      v.gatePhase -= 1.0f;
      if (v.rndPitch > 0.0f) {
        float r = lcgNoise(v.nseed);
        float m = 1.0f + r * v.rndPitch;
        if (m < 0.3f) m = 0.3f;
        v.rndMul = m;
      }
    }
    if (v.gatePhase >= 0.5f) s *= 0.06f;
  }

  // envolvente
  if (v.stage == 0) {
    v.env += v.atkInc;
    if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; }
  } else if (v.stage == 1) {
    v.env *= v.decCoef;
    if (v.env < 0.0012f) { v.active = false; v.env = 0.0f; return 0.0f; }
  } else {                                 // kill rápido (~4 ms): golpe deshecho por combo
    v.env *= 0.964f;
    if (v.env < 0.0012f) { v.active = false; v.env = 0.0f; return 0.0f; }
  }

  return s * v.env * v.gain;
}

// ─── Disparar un paso del secuenciador ─────────────────────
void fireStep(int step) {
  for (int t = 0; t < 5; t++)
    if (RHYTHMS[rhythmPat][t] & (1u << step)) triggerSound(t, step, -128);
  if ((step & 3) == 0) g_beatFlash = 1.0f;
}

// ─── CAOS (BTN5 sostenido): golpe aleatorio en escala por paso ──
void fireChaos() {
  chaosRng = chaosRng * 1664525u + 1013904223u;
  int slot = (int)((chaosRng >> 8)  % 5u);
  int deg  = (int)((chaosRng >> 16) % 10u);
  triggerSound(slot, -1, deg);
}

// ─── Filtro global ─────────────────────────────────────────
void updateFilter() {
  float cut, Q;
  if (filterIMU) {
    float xa = fabsf(filtered_x); if (xa > 1.0f) xa = 1.0f;
    float ya = fabsf(filtered_y); if (ya > 1.0f) ya = 1.0f;
    cut = 120.0f + xa * 9500.0f;
    Q   = 0.8f  + ya * 14.0f;
  } else {
    cut = cutoffBase;
    Q   = qBase;
  }
  cut += lfoVal * lfoAmt * cut * 0.92f;
  if (cut < 60.0f)    cut = 60.0f;
  if (cut > 15000.0f) cut = 15000.0f;
  if (Q   > 16.0f)    Q   = 16.0f;
  if (Q   < 0.5f)     Q   = 0.5f;

  float omega = 2.0f * (float)M_PI * cut / SAMPLE_RATE;
  float s = sinf(omega), c = cosf(omega);
  float alpha = s / (2.0f * Q);

  float b0 = (1.0f - c) * 0.5f;
  float b1 =  1.0f - c;
  float b2 = (1.0f - c) * 0.5f;
  float a0 =  1.0f + alpha;
  float a1 = -2.0f * c;
  float a2 =  1.0f - alpha;

  f_b0 = b0 / a0; f_b1 = b1 / a0; f_b2 = b2 / a0;
  f_a1 = a1 / a0; f_a2 = a2 / a0;
}

inline float applyFilter(BiqState &st, float in) {
  float out = f_b0 * in + f_b1 * st.x1 + f_b2 * st.x2 - f_a1 * st.y1 - f_a2 * st.y2;
  st.x2 = st.x1; st.x1 = in;
  st.y2 = st.y1; st.y1 = out;
  return out;
}

// ─── IMU (misma secuencia probada de pads_imu / test_imu) ──
void initIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B); Wire.write(0);
  Wire.endTransmission(true);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x1C); Wire.write(0x00);  // ±2g
  Wire.endTransmission(true);
  delay(100);
}

void readIMU() {
  unsigned long t = millis();
  if (t - lastIMURead < IMU_READ_INTERVAL) return;
  lastIMURead = t;

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, 6, true);
  if (Wire.available() >= 6) {
    int16_t rx = (Wire.read() << 8) | Wire.read();
    int16_t ry = (Wire.read() << 8) | Wire.read();
    (void)((Wire.read() << 8) | Wire.read());
    imu_x = rx / 16384.0f;
    imu_y = ry / 16384.0f;
    filtered_x = filtered_x * (1.0f - IMU_FILTER_ALPHA) + imu_x * IMU_FILTER_ALPHA;
    filtered_y = filtered_y * (1.0f - IMU_FILTER_ALPHA) + imu_y * IMU_FILTER_ALPHA;
  }
}

// ─── Aplicar un pot al parámetro del panel ACTUAL ──────────
void applyPot(int i, float val) {
  if (panel == PANEL_A) {
    // MEZCLA — master / mixer texturas↔leads / drive / tempo
    switch (i) {
      case 0: masterVol = val * val;                    break;
      case 1: { float th = val * 1.5708f;               // balance equal-power
                texLvl  = cosf(th) * 1.3f;
                leadLvl = sinf(th) * 1.3f; }            break;
      case 2: drive = val;                              break;
      case 3: { bpm = 60.0f + val * 120.0f;
                stepSamples = (60.0f / bpm / 4.0f) * SAMPLE_RATE; } break;
    }
  } else if (panel == PANEL_B) {
    // FILTRO / LFO
    switch (i) {
      case 0: cutoffBase = 60.0f * expf(val * 5.3f);    break;   // 60 Hz – ~12 kHz
      case 1: qBase      = 0.7f + val * 11.0f;          break;
      case 2: lfoRate    = 0.25f + val * 15.75f;        break;
      case 3: lfoAmt     = val;                         break;
    }
  } else if (panel == PANEL_C) {
    // SÍNTESIS — macros que transforman los sonidos de forma NOTABLE
    switch (i) {
      case 0: globalAtkS = 0.0005f + val * val * 0.8f;  break;   // 0.5 ms – 0.8 s (golpe → swell)
      case 1: decayMult  = 0.1f * expf(val * 4.382f);   break;   // ×0.1 – ×8 exponencial
      case 2: texMacro   = val;
              gNoiseMix  = val * 0.35f;                 break;   // FM/armónicos + ruido blanco
      case 3: { float semi = val * 24.0f - 12.0f;                // pitch global ±12 semitonos
                pitchRatio = powf(2.0f, semi / 12.0f); } break;
    }
  } else {
    // ESCALA / SECUENCIA
    switch (i) {
      case 0: rootSemi  = (int)(val * 11.99f);                  break;
      case 1: scaleIdx  = (int)(val * (NUM_SCALES   - 0.01f));  break;
      case 2: rhythmPat = (int)(val * (NUM_RHYTHMS  - 0.01f));  break;
      case 3: notePat   = (int)(val * (NUM_NOTEPATS - 0.01f));  break;
    }
  }
}

// ─── Mensajes de LEDs ──────────────────────────────────────
void showMsg(uint8_t type) { msgType = type; msgUntil = millis() + 600; }

// ─── Acciones de combos ────────────────────────────────────
void togglePlayMode() {
  seqMode = !seqMode;
  if (seqMode) {
    seqPlaying = true;
    masterStep = (0 - seqDir) & 15;       // el primer tick cae en el paso 0
    stepCounter = stepSamples;            // dispara de inmediato
  } else {
    seqPlaying = false;
    repeatHeld = false; chaosHeld = false;
  }
  showMsg(seqMode ? MSG_SEQ : MSG_PERC);
}

void nextBank()         { bank = (bank + 1) % NUM_BANKS; showMsg(MSG_BANK); }
void toggleFilterMode() { filterIMU = !filterIMU; showMsg(filterIMU ? MSG_FIMU : MSG_FPOTS); }

// ─── Acción INSTANTÁNEA al presionar un botón ──────────────
void pressAction(int i) {
  if (!seqMode) {
    // PERC: dispara al instante (si resulta ser combo, se deshace con fastKill)
    lastVoiceIdx[i] = triggerSound(i, -1, -128);
    lastVoiceAge[i] = voices[lastVoiceIdx[i]].age;
    return;
  }
  // SEQ: transporte de performance
  switch (i) {
    case 0:                                        // PLAY / STOP
      seqPlaying = !seqPlaying;
      if (seqPlaying) { masterStep = (0 - seqDir) & 15; stepCounter = stepSamples; }
      break;
    case 1:                                        // REVERSA
      seqDir = -seqDir;
      break;
    case 2:                                        // BEAT REPEAT (mantener)
      repeatHeld = true;
      repStart = masterStep; repIdx = 0;
      break;
    case 3:                                        // VELOCIDAD ×1 → ×2 → ×½
      speedIdx = (speedIdx + 1) % 3;
      showMsg(MSG_SPEED);
      break;
    case 4:                                        // CAOS (mantener)
      chaosHeld = true;
      break;
  }
}

// ─── Deshacer la acción de un botón (era parte de un combo) ──
void undoAction(int i) {
  if (!seqMode) { fastKill(lastVoiceIdx[i], lastVoiceAge[i]); return; }
  switch (i) {
    case 0: seqPlaying = !seqPlaying;               break;
    case 1: seqDir = -seqDir;                       break;
    case 2: repeatHeld = false;                     break;
    case 3: speedIdx = (speedIdx + 2) % 3;          break;
    case 4: chaosHeld = false;                      break;
  }
}

// ─── Al soltar un botón (efectos de mantener) ──────────────
void releaseAction(int i) {
  if (!seqMode) return;
  if (i == 2) repeatHeld = false;                   // BEAT REPEAT se suelta
  if (i == 4) chaosHeld  = false;                   // CAOS se suelta
}

// ─── Lectura de botones: flanco inmediato con lockout ──────
bool pressEdge[5]   = {false, false, false, false, false};
bool releaseEdge[5] = {false, false, false, false, false};

void readButtons() {
  uint32_t now = millis();
  for (int i = 0; i < 5; i++) {
    bool r = (digitalRead(BTN_PIN[i]) == LOW);
    if (r != rawLast[i] && (now - lastEdge[i]) >= PRESS_LOCKOUT_MS) {
      lastEdge[i] = now;
      rawLast[i] = r;
      if (r)  { pressEdge[i] = true;  downAt[i] = now; }
      else      releaseEdge[i] = true;
      lvl[i] = r;
    }
  }
}

inline uint32_t absDiff(uint32_t a, uint32_t b) { return (a > b) ? (a - b) : (b - a); }

// combo válido: ambos presionados Y sus presiones cayeron dentro de COMBO_MS
inline bool comboNow(int i, int j) {
  return lvl[i] && lvl[j] && absDiff(downAt[i], downAt[j]) <= COMBO_MS;
}

// ─── Visualizador de 6 LEDs (throttled; FastLED usa RMT, no choca con I2S) ──
void renderLEDs() {
  unsigned long t = millis();
  static unsigned long lastFrame = 0;
  if (t - lastFrame < LED_REFRESH_MS) return;
  lastFrame = t;

  static int lastPanel = PANEL_A;
  if (panel != lastPanel) { lastPanel = panel; flashLevel = 1.0f; }

  if (msgType != MSG_NONE && t < msgUntil) {
    FastLED.clear();
    switch (msgType) {
      case MSG_BANK:
        for (int i = 0; i <= bank; i++) leds[i] = CHSV(60, 255, 220);
        break;
      case MSG_SEQ:   fill_solid(leds, NUM_LEDS, CHSV(96,  255, 200)); break;
      case MSG_PERC:  fill_solid(leds, NUM_LEDS, CHSV(160, 255, 200)); break;
      case MSG_FIMU:  fill_solid(leds, NUM_LEDS, CHSV(192, 255, 200)); break;
      case MSG_FPOTS: fill_solid(leds, NUM_LEDS, CHSV(0,   0,   170)); break;
      case MSG_SPEED: {                                  // ×½ = 1 LED · ×1 = 2 · ×2 = 4
        int n = (speedIdx == 2) ? 1 : (speedIdx == 0 ? 2 : 4);
        for (int i = 0; i < n; i++) leds[i] = CHSV(128, 200, 220);
        break;
      }
    }
  } else {
    msgType = MSG_NONE;

    float sweep = filterIMU ? fabsf(filtered_x) : (lfoAmt * (0.5f + 0.5f * lfoVal));
    if (sweep > 1.0f) sweep = 1.0f;
    uint8_t hue = panelHue() + (uint8_t)(sweep * 40.0f);

    static const uint8_t TRACK_HUE[5] = { 0, 32, 64, 160, 200 };

    for (int i = 0; i < 5; i++) {
      uint8_t base = 14;
      uint8_t v = base + (uint8_t)(g_hitFlash[i] * 225.0f);
      uint8_t h = (g_hitFlash[i] > 0.05f) ? TRACK_HUE[i] : hue;
      leds[i] = CHSV(h, 255, v);
      g_hitFlash[i] *= 0.72f;
    }

    if (seqMode) {
      if (seqPlaying) {
        // LED 5 = pulso del beat; rojo si el BEAT REPEAT está activo
        uint8_t h5 = repeatHeld ? 0 : hue;
        leds[5] = CHSV(h5, 230, 20 + (uint8_t)(g_beatFlash * 230.0f));
        g_beatFlash *= 0.80f;
      } else {
        // detenido → respiración tenue
        static uint8_t breath = 0; static int8_t bdir = 1;
        breath += bdir * 3;
        if (breath >= 70) bdir = -1;
        if (breath <= 6)  bdir =  1;
        leds[5] = CHSV(hue, 220, breath);
      }
      // CAOS → chispas blancas sobre toda la tira
      if (chaosHeld) {
        chaosRng = chaosRng * 1664525u + 1013904223u;
        leds[(chaosRng >> 10) % NUM_LEDS] += CRGB(120, 120, 120);
      }
    } else {
      float e = g_energy * 1.5f; if (e > 1.0f) e = 1.0f;
      leds[5] = CHSV(hue, 255, 15 + (uint8_t)(e * 235.0f));
    }
  }

  if (flashLevel > 0.02f) {
    uint8_t w = (uint8_t)(flashLevel * 150.0f);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] += CRGB(w, w, w);
    flashLevel *= 0.55f;
  }

  uint16_t sr = 0, sg = 0, sb = 0;
  for (int i = 0; i < NUM_LEDS; i++) { sr += leds[i].r; sg += leds[i].g; sb += leds[i].b; }
  onboard[0] = CRGB(sr / NUM_LEDS, sg / NUM_LEDS, sb / NUM_LEDS);
  onboard[0].nscale8(ONBOARD_BRIGHT);

  FastLED.show();
}

// ─── Setup I2S — cola DMA CORTA para respuesta percutiva (~12 ms) ──
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = 128;
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

void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);

  for (int i = 0; i < 5; i++) pinMode(BTN_PIN[i], INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i < SEMI_LUT_N; i++)
    semiLUT[i] = powf(2.0f, (float)(i - SEMI_OFFSET) / 12.0f);

  for (int i = 0; i < 256; i++)
    sineLUT[i] = sinf(2.0f * (float)M_PI * (float)i / 256.0f);

  memset(voices, 0, sizeof(voices));

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, ONBOARD_PIN, COLOR_ORDER>(onboard, 1);
  FastLED.setBrightness(LED_BRIGHT);
  FastLED.clear();
  FastLED.show();

  initIMU();
  delay(50);
  readIMU();
  updateFilter();

  i2s_init();
}

void loop() {
  uint32_t now = millis();

  // ── Botones: acción INSTANTÁNEA en el flanco ──
  readButtons();
  for (int i = 0; i < 5; i++) {
    if (pressEdge[i])   { pressEdge[i]   = false; pressAction(i);   }
    if (releaseEdge[i]) { releaseEdge[i] = false; releaseAction(i); }
  }

  // ── Combos: si dos botones cayeron dentro de 50 ms, DESHACER sus acciones
  //    y ejecutar el combo (latches como pads_imu: no refire mientras se mantienen) ──
  if (comboNow(0, 2) && !c13) { c13 = true; undoAction(0); undoAction(2); togglePlayMode(); }
  if (comboNow(1, 3) && !c24) { c24 = true; undoAction(1); undoAction(3); nextBank(); }
  if (comboNow(2, 4) && !c35) { c35 = true; undoAction(2); undoAction(4);
                                panel = (panel == PANEL_D) ? PANEL_A : PANEL_D; panelChanged = true; }
  if (comboNow(3, 4) && !c45) { c45 = true; undoAction(3); undoAction(4);
                                panel = (panel == PANEL_A) ? PANEL_B :
                                        (panel == PANEL_B) ? PANEL_C : PANEL_A; panelChanged = true; }
  if (comboNow(0, 4) && !c15) { c15 = true; c15At = now; c15Fired = false;
                                undoAction(0); undoAction(4); }
  if (c15 && lvl[0] && lvl[4] && !c15Fired && (now - c15At) > HOLD_FILTER_MS) {
    toggleFilterMode(); c15Fired = true;
  }

  if (!lvl[0] && !lvl[2]) c13 = false;
  if (!lvl[1] && !lvl[3]) c24 = false;
  if (!lvl[2] && !lvl[4]) c35 = false;
  if (!lvl[3] && !lvl[4]) c45 = false;
  if (!lvl[0] && !lvl[4]) c15 = false;

  // ── LFO wobble (por buffer) ──
  lfoPhase += lfoRate * (float)BUFFER_SAMPLES / SAMPLE_RATE;
  if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
  lfoVal = oscSine(lfoPhase);

  // ── IMU + filtro global ──
  readIMU();
  updateFilter();

  // ── Pots: 1 por buffer en rotación, congelados al cambiar de panel ──
  static uint8_t potScan = 0;
  if (panelChanged) {
    for (int i = 0; i < 4; i++) {
      potLive[i] = false; potMoveCnt[i] = 0; potAnchor[i] = readPot(POT_PIN[i]);
    }
    panelChanged = false;
  }
  int pi = potScan; potScan = (potScan + 1) & 3;
  float pv = readPot(POT_PIN[pi]);
  if (!potLive[pi]) {
    if (fabsf(pv - potAnchor[pi]) > POT_MOVE_THR) { if (++potMoveCnt[pi] >= 3) potLive[pi] = true; }
    else potMoveCnt[pi] = 0;
  }
  if (potLive[pi]) applyPot(pi, pv);

  // ── Energía para los LEDs ──
  float vsum = 0.0f;
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active) vsum += voices[i].env;
  vsum *= 0.25f;
  if (vsum > g_energy) g_energy = vsum;
  else                 g_energy = g_energy * 0.90f + vsum * 0.10f;

  // ── Generar buffer de audio (estéreo) ──
  int16_t buffer[BUFFER_SAMPLES * 2];

  float driveG    = 1.0f + drive * 6.0f;
  float drivePost = 1.0f / (1.0f + drive * 1.1f);
  float speedMul  = SPEED_MUL[speedIdx];

  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    // secuenciador con transporte (sólo en modo SEQ y en PLAY)
    if (seqMode && seqPlaying) {
      stepCounter += speedMul;
      if (stepCounter >= stepSamples) {
        stepCounter -= stepSamples;
        masterStep = (masterStep + seqDir) & 15;          // la posición REAL sigue avanzando
        int play;
        if (repeatHeld) {                                  // BEAT REPEAT: loop de 2 pasos
          play = (repStart + (repIdx & 1) * seqDir) & 15;
          repIdx++;
        } else play = masterStep;
        fireStep(play);
        if (chaosHeld) fireChaos();
      }
    }

    // dos buses: TEXTURAS/FX y LEADS (mixer del Panel A)
    float tL = 0.0f, tR = 0.0f, lL = 0.0f, lR = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;
      float smp = renderVoice(v);
      if (v.lead) { lL += smp * v.lG; lR += smp * v.rG; }
      else        { tL += smp * v.lG; tR += smp * v.rG; }
    }

    float mL = (tL * texLvl + lL * leadLvl) * 0.30f;
    float mR = (tR * texLvl + lR * leadLvl) * 0.30f;

    // DRIVE: saturación pre-filtro (tanh racional) con compensación de nivel
    float xL = mL * driveG, xR = mR * driveG;
    if (xL >  3.0f) xL =  3.0f; if (xL < -3.0f) xL = -3.0f;
    if (xR >  3.0f) xR =  3.0f; if (xR < -3.0f) xR = -3.0f;
    mL = (xL * (27.0f + xL * xL) / (27.0f + 9.0f * xL * xL)) * drivePost;
    mR = (xR * (27.0f + xR * xR) / (27.0f + 9.0f * xR * xR)) * drivePost;

    // FILTRO global resonante (POTS o IMU) con WOBBLE
    mL += 1.0e-18f;
    mR -= 1.0e-18f;
    float fL = applyFilter(bqL, mL);
    float fR = applyFilter(bqR, mR);

    float vL = fL * masterVol;
    float vR = fR * masterVol;

    // Soft-clip de SEGURIDAD
    if (vL >  3.0f) vL =  3.0f; if (vL < -3.0f) vL = -3.0f;
    if (vR >  3.0f) vR =  3.0f; if (vR < -3.0f) vR = -3.0f;
    float shL = vL * (27.0f + vL * vL) / (27.0f + 9.0f * vL * vL);
    float shR = vR * (27.0f + vR * vR) / (27.0f + 9.0f * vR * vR);

    buffer[n * 2]     = (int16_t)(shL * 28000.0f);
    buffer[n * 2 + 1] = (int16_t)(shR * 28000.0f);
  }

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);

  // ── LEDs (tras volcar el buffer al DMA) ──
  renderLEDs();
}
