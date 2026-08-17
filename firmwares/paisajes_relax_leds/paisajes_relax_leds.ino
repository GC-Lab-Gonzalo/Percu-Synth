// ==============================================================================================================================================
// PERCU-SYNTH — Paisajes Relax + LEDs (10 sonidos relajantes combinables · 2 anillos WS2812 de 30 LEDs) — GC Lab Chile
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
// - Tira de LEDs WS2812 de 39 LEDs |DATA -> 46| montada como DOS ANILLOS:
//     · ANILLO A (LEDs 0-18 de la tira, 19 LEDs)  → alumbra HACIA AFUERA  (efectos y eventos)
//     · ANILLO B (LEDs 19-38 de la tira, 20 LEDs) → alumbra HACIA ADENTRO (resplandor ambiente)
//   (los primeros 6 LEDs de la cadena son SMD internos del PCB → indicadores de estado)
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// - NO usa IMU ni Serial (todo el CPU para el audio y las luces)
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
// - FastLED (instalar desde el gestor de librerías Arduino) — para la tira WS2812
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Máquina de PAISAJES SONOROS RELAJANTES: 10 capas de sonido sintetizadas en tiempo real
// (sin samples) que se pueden ENCENDER Y COMBINAR libremente. Cada capa activa pinta su
// propio efecto de luz sobre dos anillos de 30 LEDs (uno hacia afuera, otro hacia adentro),
// mezclándose de forma aditiva igual que el sonido. Todo suena DIRECTO Y NATURAL, sin
// efectos (sin delay/reverb). Motor de audio: I2S → PCM5102 44.1 kHz / 16-bit estéreo con
// soft-limiter. Las luces corren por FastLED (RMT, no choca con el I2S) a ~30 FPS.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// BOTONES — cada botón alterna (toggle) una capa. Pulsación CORTA = capa principal,
// pulsación LARGA (>0.6 s) = capa alternativa. Todas se pueden combinar:
// - BTN1 (44) corta → VIENTO       · larga → FUEGO (fogata crepitando)
// - BTN2 (42) corta → MAR (olas)   · larga → LLUVIA
// - BTN3 (0)  corta → CAMPANITAS   · larga → CUENCO tibetano (dron armónico)
// - BTN4 (45) corta → GOTAS de agua · larga → ARROYO (agua corriendo)
// - BTN5 (47) corta → GRILLOS      · larga → PÁJAROS (trinos)
// Al alternar una capa, los anillos hacen un flash con el color de la capa
// (brillante = encendida, tenue = apagada).
//
// LEDs INTERNOS DE LA PLACA (los 6 SMD) = INDICADORES DE ESTADO:
// - LED 1..5 = BTN1..BTN5: apagado = nada activo · color de la capa principal o de la
//   alternativa según cuál esté encendida · si están AMBAS, alterna entre los dos colores.
// - LED 6 = respiración tenue cian cuando hay al menos una capa sonando.
//
// POTENCIÓMETROS (globales, afectan a todas las capas activas):
// - POT1 (ADC1)  → Volumen maestro
// - POT2 (ADC2)  → Densidad / actividad (ritmo de campanitas, gotas, lluvia, ráfagas…)
// - POT3 (ADC8)  → Color tonal (oscuro ↔ brillante — filtro paso-bajos global)
// - POT4 (ADC10) → Balance fondo ↔ eventos (sube/baja campanitas, gotas, grillos y pájaros
//                  sobre los fondos continuos: viento, mar, fuego, lluvia, cuenco, arroyo)
//
// LUCES — cada capa aporta su efecto (mezcla aditiva):
// - VIENTO: banda azul-blanca que fluye por el anillo A al ritmo de las ráfagas
// - MAR: la ola llena el anillo A en turquesa y "rompe" en espuma blanca; B respira azul
// - CAMPANITAS: cada nota = destello dorado en el anillo A
// - GOTAS: cada gota = onda expansiva en A + reflejo tenue simultáneo en B
// - GRILLOS: noche azul muy tenue en B + chispitas verdes al cantar
// - FUEGO: fuego vivo (flicker cálido) en B + chispas amarillas en A al crepitar
// - LLUVIA: gotitas azules parpadeando en ambos anillos
// - CUENCO: respiración lenta violeta-ámbar en ambos anillos
// - ARROYO: corriente verde-agua fluyendo por el anillo A
// - PÁJAROS: destellos celestes saltarines en A con cada trino
//
// AL ENCENDER: barrido de luz por los dos anillos y arranca EN SILENCIO — el usuario
// decide qué sonidos activar. Sin IMU y sin Serial.
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <math.h>

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
const float DT_BUF = (float)BUFFER_SAMPLES / SAMPLE_RATE;   // ≈ 2.9 ms por buffer

// ─── Tira de LEDs WS2812: 6 internos + 2 anillos de 30 ─────
#define LED_PIN       46
#define START_LED      6                 // LEDs 0-5 son SMD internos del PCB → siempre apagados
#define RING_A_LEN    19                           // anillo EXTERIOR (hacia afuera)
#define RING_B_LEN    20                           // anillo INTERIOR (hacia adentro)
#define EXT_LEN       (RING_A_LEN + RING_B_LEN)    // 39 LEDs externos
#define NUM_LEDS      (START_LED + EXT_LEN)        // 45
#define RING_A_START  START_LED
#define RING_B_START  (START_LED + RING_A_LEN)
inline uint8_t ringN(uint8_t ring) { return ring ? RING_B_LEN : RING_A_LEN; }   // largo del anillo
#define LED_BRIGHT    140
const unsigned long LED_FRAME_MS = 33;   // ~30 FPS (no robar tiempo a los buffers I2S)
CRGB leds[NUM_LEDS];

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
#define NUM_BTN 5
const uint8_t BTN_PINS[NUM_BTN] = {44, 42, 0, 45, 47};   // BTN1..BTN5
const unsigned long LONG_PRESS_MS = 600;
bool          btnLast[NUM_BTN];
unsigned long btnPressedAt[NUM_BTN];
bool          btnLongFired[NUM_BTN];

// ─── Potenciómetros ────────────────────────────────────────
#define POT_VOLUMEN   1    // ADC1  → volumen maestro
#define POT_DENSIDAD  2    // ADC2  → densidad / actividad
#define POT_COLOR     8    // ADC8  → color tonal (LPF global)
#define POT_BALANCE  10    // ADC10 → balance fondo ↔ eventos

// ─── Capas de sonido ───────────────────────────────────────
#define L_VIENTO      0   // BTN1 corta
#define L_MAR         1   // BTN2 corta
#define L_CAMPANITAS  2   // BTN3 corta
#define L_GOTAS       3   // BTN4 corta
#define L_GRILLOS     4   // BTN5 corta
#define L_FUEGO       5   // BTN1 larga
#define L_LLUVIA      6   // BTN2 larga
#define L_CUENCO      7   // BTN3 larga
#define L_ARROYO      8   // BTN4 larga
#define L_PAJAROS     9   // BTN5 larga
#define NUM_LAYERS   10

bool  layerOn[NUM_LAYERS];
float layerEnv[NUM_LAYERS];              // envolvente 0..1 (fundido suave al encender/apagar)
// Color distintivo de cada capa (para el flash, sus efectos y los LEDs de estado)
const uint8_t layerHue[NUM_LAYERS] = {150, 132, 40, 158, 96, 20, 170, 196, 110, 125};

// ─── Parámetros globales (de los pots) ─────────────────────
float masterVol = 0.5f;    // POT1
float density   = 1.0f;    // POT2 → 0.15 .. 3.0
float tiltCut   = 6000.0f; // POT3 → cutoff LPF global
float eventGain = 1.0f;    // POT4 → 0.25 .. 2.0 (eventos sobre el fondo)

// ─── RNG simple ────────────────────────────────────────────
uint32_t rngState = 0xC0FFEE21;
inline uint32_t rng()    { rngState = rngState * 1664525u + 1013904223u; return rngState; }
inline float    rnd01()  { return (float)(rng() >> 8) * (1.0f / 16777216.0f); }   // 0..1
inline float    frand()  { return (float)(int32_t)rng() * 4.6566129e-10f; }       // -1..1 (ruido blanco)

// ─── Tabla de seno ─────────────────────────────────────────
#define LUT_SIZE 512
float sineLUT[LUT_SIZE];
inline float sinLUT(float ph) {          // ph en ciclos (cualquier valor ≥ 0)
  return sineLUT[(int)(ph * LUT_SIZE) & (LUT_SIZE - 1)];
}

// ─── Soft-clip cúbico acotado ──────────────────────────────
inline float softclip(float x) {
  if (x >  3.0f) x =  3.0f;
  if (x < -3.0f) x = -3.0f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

inline float expCoef(float sec) { return expf(-1.0f / (sec * SAMPLE_RATE)); }

// ─── Biquad paso-banda (skirt constante, pico 0 dB) ────────
struct BiquadBP {
  float b0 = 0, b2 = 0, a1 = 0, a2 = 0;
  float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  void set(float fc, float Q) {
    if (fc < 40.0f)    fc = 40.0f;
    if (fc > 16000.0f) fc = 16000.0f;
    float w = 2.0f * (float)M_PI * fc / SAMPLE_RATE;
    float s = sinf(w), c = cosf(w);
    float alpha = s / (2.0f * Q);
    float a0 = 1.0f + alpha;
    b0 =  alpha / a0;
    b2 = -alpha / a0;
    a1 = -2.0f * c / a0;
    a2 = (1.0f - alpha) / a0;
  }
  inline float process(float in) {
    float out = b0 * in + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = in; y2 = y1; y1 = out;
    return out;
  }
};

// ══════════════════════════════════════════════════════════
//  ESTADO DE SÍNTESIS POR CAPA
// ══════════════════════════════════════════════════════════

// — VIENTO: ruido por paso-banda errante + silbido resonante, con ráfagas —
BiquadBP windBP, windWhistle;
float gust = 0.3f, gustTarget = 0.3f, gustTimer = 1.0f;

// — MAR: dos olas lentas superpuestas sobre ruido filtrado —
struct SeaWave { float phase; float period; };
SeaWave seaW[2] = { {0.0f, 11.0f}, {0.45f, 14.0f} };
float g_seaEnv = 0.0f;          // envolvente de la ola dominante (también la usan las luces)
float seaLP = 0.0f, kSea = 0.1f;

// — CAMPANITAS: voces de 3 parciales inarmónicos, pentatónica de Do —
#define NUM_CHIMES 10
struct ChimeV {
  bool  active;
  float f;                       // frecuencia fundamental
  float p0, p1, p2;              // fases de los 3 parciales
  float e0, e1, e2;              // envolventes
  float d0, d1, d2;              // coefs de decaimiento
  float amp;
  uint32_t age;
};
ChimeV chimes[NUM_CHIMES];
uint32_t chimeCounter = 0;
const float CHIME_RATIOS[3] = {1.0f, 2.76f, 5.40f};   // parciales tipo campana
const int   PENTA[5] = {0, 2, 4, 7, 9};               // Do mayor pentatónica
const float CHIME_BASE = 523.25f;                     // C5
float chimeTimer = 1.0f;

// — GOTAS: plink con barrido de pitch ascendente → eco —
#define NUM_DROPS 6
struct DropV { bool active; float phase, freq, env; uint8_t stage; };
DropV drops[NUM_DROPS];
const float DROP_RISE = 1.000225f;    // ≈ +1 octava en 70 ms
float dropTimer = 2.0f;
float dropDecay = 0.99962f;           // ~60 ms — la gota es breve, fugaz

// — GRILLOS: 2 grillos con portadora aguda y sílabas moduladas —
struct Cricket { float carrier, phase, sylPh, gate, gateS, t; bool singing; };
Cricket crickets[2] = { {4050.0f, 0, 0, 0, 0, 0.5f, false}, {4420.0f, 0.3f, 0, 0, 0, 1.6f, false} };

// — FUEGO: rumor grave (ruido browniano) + crepitar aleatorio —
#define NUM_CRACKLES 6
struct Crackle { bool active; float env, decay, amp; };
Crackle crackles[NUM_CRACKLES];
float brown = 0.0f, flare = 0.8f, flareTarget = 0.8f;
float fireLP = 0.0f;                  // paso-bajos final → fogata suave y lejana

// — LLUVIA: POT2 = intensidad real. Al mínimo: gotitas sueltas y dispersas ("recién
//   empieza a llover"), casi sin colchón. Al máximo: aguacero continuo de sibilancia. —
float rainLP = 0.0f, rainBody = 0.0f;
float rainVar = 0.92f, rainVarT = 0.92f;  // variación MUY leve de intensidad
#define NUM_RAINTICKS 6
struct RainTick { bool active; float lp, k, env, decay; };   // gotita = tic corto y brillante
RainTick rainTicks[NUM_RAINTICKS];

// — CUENCO tibetano: dron GRAVE de parciales batientes (fundamental + quinta + octava) —
float bowlRoot = 65.41f;
float bowlPh[4] = {0, 0, 0, 0};
const float BOWL_RATIOS[4] = {1.0f, 1.003f, 1.5f, 2.005f};   // armónicos cálidos, nada agudo
const float BOWL_AMPS[4]   = {0.55f, 0.55f, 0.25f, 0.12f};
float bowlLfoPh = 0.0f;

// — ARROYO: dos paso-banda burbujeantes sobre ruido —
BiquadBP streamBP1, streamBP2;
float streamJit1 = 0.0f, streamJit2 = 0.0f, bubble = 0.8f;

// — PÁJAROS: 2 aves con frases cortas de notas DISTINTAS (volumen/duración/pausas variables,
//   notas bien separadas — nunca repeticiones parejas que suenen a eco) —
struct Bird {
  uint8_t state;                 // 0 = silencio, 1 = nota, 2 = pausa entre notas
  float   t;                     // tiempo restante del estado (s)
  int     notesLeft;
  float   f0, f1, noteDur, notePos;
  float   phase, env, fBase;
  float   ampl;                  // volumen propio de CADA nota (varía nota a nota)
};
Bird birds[2];

// — Filtro tonal global (un polo por canal) —
float tiltL = 0.0f, tiltR = 0.0f, kTilt = 0.5f;

static i2s_chan_handle_t tx_chan;

// ══════════════════════════════════════════════════════════
//  MOTOR DE LUCES
// ══════════════════════════════════════════════════════════

#define FX_SPARK  0
#define FX_RIPPLE 1
#define NUM_FX 14
struct LedFx {
  bool    active;
  uint8_t mode;
  uint8_t ring;        // 0 = A (exterior) · 1 = B (interior)
  float   pos;         // 0..30
  float   radius, speed;
  uint8_t hue, sat;
  float   level, decay;
  int16_t delayF;      // frames de espera antes de nacer (para el eco de las gotas)
};
LedFx fxPool[NUM_FX];

uint8_t heatB[RING_B_LEN];              // mapa de calor del fuego (anillo B)
float   windFlowPh = 0.0f;              // fase de la banda fluida del viento
uint8_t g_flashFrames = 0;              // flash de confirmación al alternar capa
uint8_t g_flashHue = 0;
bool    g_flashOn = true;
unsigned long lastLedFrame = 0;

inline void addPix(uint8_t ring, float pos, const CRGB &c) {
  // pinta con interpolación fraccional entre 2 LEDs contiguos del anillo (con wrap)
  int base = (ring == 0) ? RING_A_START : RING_B_START;
  int n    = ringN(ring);
  while (pos < 0)          pos += n;
  while (pos >= (float)n)  pos -= n;
  int   i0 = (int)pos;
  int   i1 = (i0 + 1) % n;
  float fr = pos - i0;
  leds[base + i0] += CRGB((uint8_t)(c.r * (1.0f - fr)), (uint8_t)(c.g * (1.0f - fr)), (uint8_t)(c.b * (1.0f - fr)));
  leds[base + i1] += CRGB((uint8_t)(c.r * fr), (uint8_t)(c.g * fr), (uint8_t)(c.b * fr));
}

void spawnFx(uint8_t mode, uint8_t ring, float pos, uint8_t hue, uint8_t sat,
             float level, float decay, float speed, int16_t delayF) {
  for (int i = 0; i < NUM_FX; i++) {
    if (fxPool[i].active) continue;
    fxPool[i] = {true, mode, ring, pos, 0.0f, speed, hue, sat, level, decay, delayF};
    return;
  }
}

// ══════════════════════════════════════════════════════════
//  DISPARADORES DE EVENTOS (audio + luz)
// ══════════════════════════════════════════════════════════

void triggerChime() {
  int idx = -1;
  for (int i = 0; i < NUM_CHIMES; i++) if (!chimes[i].active) { idx = i; break; }
  if (idx < 0) {
    uint32_t oldest = 0xFFFFFFFF; idx = 0;
    for (int i = 0; i < NUM_CHIMES; i++) if (chimes[i].age < oldest) { oldest = chimes[i].age; idx = i; }
  }
  int deg = rng() % 10;                                  // 2 octavas de pentatónica
  int oct = deg / 5, st = PENTA[deg % 5] + 12 * oct;
  float f = CHIME_BASE * powf(2.0f, st / 12.0f);
  f *= 1.0f + (rnd01() - 0.5f) * 0.007f;                 // desafinación leve (±6 cents)
  float T = 2.2f + rnd01() * 1.8f;                       // cola de la fundamental
  ChimeV &c = chimes[idx];
  c.active = true; c.f = f; c.age = chimeCounter++;
  c.p0 = c.p1 = c.p2 = 0.0f;
  c.e0 = c.e1 = c.e2 = 1.0f;
  c.d0 = expCoef(T); c.d1 = expCoef(T * 0.45f); c.d2 = expCoef(T * 0.22f);
  c.amp = 0.35f + rnd01() * 0.65f;
  spawnFx(FX_SPARK, 0, rnd01() * RING_A_LEN, 40 + (rng() % 16) - 8, 210,
          0.35f + 0.6f * c.amp, 0.90f, 0, 0);
}

void triggerDrop() {
  // UNA sola gota a la vez: si todavía suena la anterior, esta se salta
  // (gotas superpuestas o muy seguidas se perciben como un eco)
  for (int i = 0; i < NUM_DROPS; i++) if (drops[i].active) return;
  drops[0] = {true, 0.0f, 280.0f + rnd01() * 260.0f, 0.0f, 0};
  float pos = rnd01() * RING_A_LEN;
  spawnFx(FX_RIPPLE, 0, pos, layerHue[L_GOTAS], 220, 0.9f, 0, 0.55f, 0);
  spawnFx(FX_RIPPLE, 1, pos * (float)RING_B_LEN / RING_A_LEN,               // misma posición angular
          layerHue[L_GOTAS], 200, 0.30f, 0, 0.55f, 0);                      // reflejo interior
}

void triggerCrackle() {
  for (int i = 0; i < NUM_CRACKLES; i++) {
    if (crackles[i].active) continue;
    crackles[i] = {true, 1.0f, expCoef(0.004f + rnd01() * 0.008f), 0.4f + rnd01() * 0.6f};
    if (rnd01() < 0.5f)
      spawnFx(FX_SPARK, 0, rnd01() * RING_A_LEN, 30, 255, 0.5f + rnd01() * 0.3f, 0.78f, 0, 0);
    return;
  }
}

void triggerRainTick() {
  for (int i = 0; i < NUM_RAINTICKS; i++) {
    if (rainTicks[i].active) continue;
    // brillante y MUY corto (2–6 ms) → se percibe como gotita, no como golpe
    rainTicks[i] = {true, 0.0f, 0.30f + rnd01() * 0.35f, 1.0f, expCoef(0.002f + rnd01() * 0.004f)};
    return;
  }
}

void birdStartSong(int i) {
  Bird &b = birds[i];
  b.state = 1;
  b.notesLeft = 1 + (rng() % 3);                 // frase corta: 1–3 notas
  b.fBase = 1900.0f + rnd01() * 1500.0f;
  b.f0 = b.fBase;
  b.f1 = b.fBase * (1.25f + rnd01() * 0.35f);
  b.noteDur = 0.06f + rnd01() * 0.10f;
  b.notePos = 0.0f;
  b.ampl = 0.35f + rnd01() * 0.65f;
  b.env = 0.0f;
}

// ══════════════════════════════════════════════════════════
//  CONTROL POR BUFFER (envolventes, ráfagas, olas, agendas)
// ══════════════════════════════════════════════════════════

void updateControl() {
  // — Envolventes de capa (fundido suave, el cuenco más lento) —
  for (int i = 0; i < NUM_LAYERS; i++) {
    float target = layerOn[i] ? 1.0f : 0.0f;
    float k = (i == L_CUENCO) ? 0.0016f : 0.0035f;   // τ ≈ 1.8 s / 0.8 s
    layerEnv[i] += (target - layerEnv[i]) * k;
  }

  // — VIENTO: ráfagas (paseo aleatorio lento) + filtros errantes —
  if (layerEnv[L_VIENTO] > 0.002f) {
    gustTimer -= DT_BUF;
    if (gustTimer <= 0.0f) {
      if (rnd01() < 0.35f) {                     // momento de CALMA (brisa casi nula, más largo)
        gustTarget = 0.02f + rnd01() * 0.08f;
        gustTimer  = 3.0f + rnd01() * 5.0f;
      } else {
        gustTarget = 0.15f + powf(rnd01(), 1.6f) * 0.85f;
        gustTimer  = 2.0f + rnd01() * 4.0f;
      }
    }
    gust += (gustTarget - gust) * 0.004f;
    windBP.set(230.0f + 620.0f * gust, 1.1f);
    windWhistle.set(1100.0f + 1600.0f * gust, 7.0f);
  }

  // — MAR: avanzar las dos olas y derivar la envolvente dominante —
  if (layerEnv[L_MAR] > 0.002f) {
    float envMax = 0.0f;
    for (int i = 0; i < 2; i++) {
      seaW[i].phase += DT_BUF / seaW[i].period;
      if (seaW[i].phase >= 1.0f) { seaW[i].phase -= 1.0f; seaW[i].period = 8.0f + rnd01() * 8.0f; }
      float e = powf(0.5f - 0.5f * cosf(2.0f * (float)M_PI * seaW[i].phase), 1.8f);
      if (i == 1) e *= 0.8f;
      if (e > envMax) envMax = e;
    }
    g_seaEnv = envMax;
    float fc = 250.0f + 2500.0f * powf(envMax, 1.5f);
    kSea = 1.0f - expf(-2.0f * (float)M_PI * fc / SAMPLE_RATE);
  }

  // — CAMPANITAS: agenda con racimos; el viento activo las hace sonar más —
  if (layerEnv[L_CAMPANITAS] > 0.01f) {
    chimeTimer -= DT_BUF;
    if (chimeTimer <= 0.0f) {
      triggerChime();
      if (rnd01() < 0.40f) chimeTimer = 0.09f + rnd01() * 0.26f;          // racimo
      else                 chimeTimer = (0.8f + rnd01() * 5.2f) / density;
      if (layerOn[L_VIENTO] && gust > 0.6f) chimeTimer *= 0.35f;          // ráfaga → tintineo
    }
  }

  // — GOTAS: agenda zen —
  if (layerEnv[L_GOTAS] > 0.01f) {
    dropTimer -= DT_BUF;
    if (dropTimer <= 0.0f) {
      triggerDrop();
      dropTimer = (2.5f + rnd01() * 6.5f) / density;   // gotas espaciadas, zen
    }
  }

  // — GRILLOS: máquina de estados canto / silencio por grillo —
  for (int i = 0; i < 2; i++) {
    Cricket &cr = crickets[i];
    if (layerEnv[L_GRILLOS] > 0.01f) {
      cr.t -= DT_BUF;
      if (cr.t <= 0.0f) {
        cr.singing = !cr.singing;
        cr.t = cr.singing ? (0.35f + rnd01() * 0.5f) : (0.5f + rnd01() * 2.5f / density);
        if (cr.singing) cr.sylPh = 0.0f;
      }
      if (cr.singing) {
        cr.sylPh += 19.0f * DT_BUF;                       // ~19 sílabas/s
        float s = sinf(2.0f * (float)M_PI * cr.sylPh);
        cr.gate = (s > 0.0f) ? s * s * s : 0.0f;
        if (cr.gate > 0.8f && rnd01() < 0.10f)
          spawnFx(FX_SPARK, 1, rnd01() * RING_B_LEN, layerHue[L_GRILLOS], 235, 0.28f, 0.72f, 0, 0);
      } else cr.gate = 0.0f;
    } else cr.gate = 0.0f;
  }

  // — FUEGO: llamarada (paseo aleatorio) —
  if (layerEnv[L_FUEGO] > 0.002f) {
    if (rnd01() < 0.01f) flareTarget = 0.55f + rnd01() * 0.55f;
    flare += (flareTarget - flare) * 0.006f;
  }

  // — LLUVIA: variación de intensidad casi imperceptible (lluvia pareja, calma) —
  if (layerEnv[L_LLUVIA] > 0.002f) {
    if (rnd01() < 0.004f) rainVarT = 0.85f + rnd01() * 0.15f;
    rainVar += (rainVarT - rainVar) * 0.002f;
  }

  // — CUENCO: LFO de respiración —
  bowlLfoPh += 0.08f * DT_BUF;
  if (bowlLfoPh >= 1.0f) bowlLfoPh -= 1.0f;

  // — ARROYO: jitter de burbujeo en los paso-banda —
  if (layerEnv[L_ARROYO] > 0.002f) {
    streamJit1 += (frand() * 0.15f - streamJit1) * 0.03f;
    streamJit2 += (frand() * 0.15f - streamJit2) * 0.05f;
    streamBP1.set(900.0f  * (1.0f + streamJit1), 1.8f);
    streamBP2.set(2300.0f * (1.0f + streamJit2), 1.8f);
    bubble += (0.85f + 0.15f * frand() - bubble) * 0.06f;
  }

  // — PÁJAROS: máquina de estados por ave —
  for (int i = 0; i < 2; i++) {
    Bird &b = birds[i];
    if (layerEnv[L_PAJAROS] <= 0.01f) { b.state = 0; b.t = 1.0f + rnd01() * 3.0f; b.env = 0; continue; }
    if (b.state == 0) {                                   // silencio entre cantos
      b.t -= DT_BUF;
      if (b.t <= 0.0f) birdStartSong(i);
    } else if (b.state == 1) {                            // nota (barrido f0→f1)
      b.notePos += DT_BUF / b.noteDur;
      if (b.notePos >= 1.0f) {
        b.notesLeft--;
        if (b.notesLeft <= 0) { b.state = 0; b.t = (2.0f + rnd01() * 6.0f) / density; }
        else                  { b.state = 2; b.t = 0.10f + rnd01() * 0.30f; }   // pausa irregular
      }
    } else {                                              // pausa entre notas
      b.t -= DT_BUF;
      if (b.t <= 0.0f) {
        b.state = 1; b.notePos = 0.0f;
        float up = (rnd01() < 0.5f) ? 1.0f : -1.0f;
        b.f0 = b.fBase * (1.0f + rnd01() * 0.15f);
        b.f1 = b.f0 * (1.0f + up * (0.2f + rnd01() * 0.25f));
        b.noteDur = 0.06f + rnd01() * 0.10f;              // cada nota distinta en duración…
        b.ampl = 0.35f + rnd01() * 0.65f;                 // …y en volumen (no parece eco)
        spawnFx(FX_SPARK, 0, rnd01() * RING_A_LEN, layerHue[L_PAJAROS], 180, 0.45f, 0.80f, 0, 0);
      }
    }
  }

  // — Filtro tonal (del pot) —
  kTilt = 1.0f - expf(-2.0f * (float)M_PI * tiltCut / SAMPLE_RATE);
}

// ─── Aplicar un pot al parámetro global ────────────────────
void applyPot(int i, float val) {
  switch (i) {
    case 0: masterVol = val * val * 1.2f; break;
    case 1: density   = 0.15f + powf(val, 1.5f) * 2.85f; break;
    case 2: tiltCut   = 600.0f + val * val * 11400.0f; break;
    case 3: eventGain = 0.25f + val * 1.75f; break;   // eventos: de casi nada a protagonistas
  }
}

float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (float)(sum >> 3) / 4095.0f;
}

// ══════════════════════════════════════════════════════════
//  RENDER DE AUDIO (por muestra)
// ══════════════════════════════════════════════════════════

void renderAudio(int16_t *buffer) {
  float eV  = layerEnv[L_VIENTO],  eM  = layerEnv[L_MAR],   eCh = layerEnv[L_CAMPANITAS];
  float eG  = layerEnv[L_GOTAS],   eGr = layerEnv[L_GRILLOS], eF = layerEnv[L_FUEGO];
  float eLl = layerEnv[L_LLUVIA],  eCu = layerEnv[L_CUENCO], eA = layerEnv[L_ARROYO];
  float ePj = layerEnv[L_PAJAROS];

  float pCrack = 0.00018f * density * (0.6f + flare);

  // Intensidad de la lluvia 0..1 (POT2): 0 = recién empieza · 1 = aguacero
  float rainInt = (density - 0.15f) * (1.0f / 2.85f);
  if (rainInt < 0.0f) rainInt = 0.0f;
  if (rainInt > 1.0f) rainInt = 1.0f;
  float pTick = (2.0f + 22.0f * rainInt) / SAMPLE_RATE;   // gotitas: ~2/s → ~24/s

  float bowlBreath = 0.85f + 0.15f * sinLUT(bowlLfoPh);

  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    float white = frand();
    float dry = 0.0f;    // fondos continuos (viento, mar, fuego, lluvia, cuenco, arroyo)
    float ev  = 0.0f;    // eventos (campanitas, gotas, grillos, pájaros) — POT4 los balancea

    // — VIENTO —
    if (eV > 0.002f) {
      float w = windBP.process(white) * (0.08f + 0.92f * gust);
      w += windWhistle.process(white) * 0.15f * gust * gust;
      dry += w * 0.9f * eV;
    }

    // — MAR —
    if (eM > 0.002f) {
      seaLP += kSea * (white - seaLP);
      float foam = (white - seaLP) * 0.5f * g_seaEnv * g_seaEnv * g_seaEnv * g_seaEnv;
      dry += (seaLP * (0.25f + 0.75f * g_seaEnv) * 1.4f + foam) * 0.55f * eM;
    }

    // — CAMPANITAS —
    if (eCh > 0.002f) {
      float cs = 0.0f;
      for (int i = 0; i < NUM_CHIMES; i++) {
        ChimeV &c = chimes[i];
        if (!c.active) continue;
        float dt = c.f / SAMPLE_RATE;
        c.p0 += dt;                    if (c.p0 >= 1.0f) c.p0 -= 1.0f;
        c.p1 += dt * CHIME_RATIOS[1];  if (c.p1 >= 1.0f) c.p1 -= 1.0f;
        c.p2 += dt * CHIME_RATIOS[2];  if (c.p2 >= 1.0f) c.p2 -= 1.0f;
        c.e0 *= c.d0; c.e1 *= c.d1; c.e2 *= c.d2;
        if (c.e0 < 0.0008f) { c.active = false; continue; }
        cs += (sinLUT(c.p0) * c.e0 + 0.50f * sinLUT(c.p1) * c.e1 + 0.22f * sinLUT(c.p2) * c.e2) * c.amp;
      }
      ev += cs * 0.22f * eCh;
    }

    // — GOTAS (plink breve que sube de tono, directo y natural) —
    if (eG > 0.002f) {
      float gs = 0.0f;
      for (int i = 0; i < NUM_DROPS; i++) {
        DropV &d = drops[i];
        if (!d.active) continue;
        d.freq *= DROP_RISE;
        d.phase += d.freq / SAMPLE_RATE;
        if (d.phase >= 1.0f) d.phase -= 1.0f;
        if (d.stage == 0) { d.env += 0.02f; if (d.env >= 1.0f) { d.env = 1.0f; d.stage = 1; } }
        else              { d.env *= dropDecay; if (d.env < 0.001f) { d.active = false; continue; } }
        gs += sinLUT(d.phase) * d.env;
      }
      ev += gs * 0.42f * eG;
    }

    // — GRILLOS (muy sutiles) —
    if (eGr > 0.002f) {
      float crs = 0.0f;
      for (int i = 0; i < 2; i++) {
        Cricket &cr = crickets[i];
        cr.gateS += (cr.gate - cr.gateS) * 0.002f;
        if (cr.gateS < 0.001f) continue;
        cr.phase += cr.carrier / SAMPLE_RATE;
        if (cr.phase >= 1.0f) cr.phase -= 1.0f;
        crs += sinLUT(cr.phase) * cr.gateS;
      }
      ev += crs * 0.055f * eGr;
    }

    // — FUEGO —
    if (eF > 0.002f) {
      brown += 0.015f * white;
      brown *= 0.9985f;
      if (brown >  1.0f) brown =  1.0f;
      if (brown < -1.0f) brown = -1.0f;
      float fs = brown * 0.40f * flare;                    // rumor grave contenido (no "viento")
      if (rnd01() < pCrack) triggerCrackle();
      for (int i = 0; i < NUM_CRACKLES; i++) {
        Crackle &k = crackles[i];
        if (!k.active) continue;
        k.env *= k.decay;
        if (k.env < 0.01f) { k.active = false; continue; }
        fs += white * k.env * k.env * k.amp * 0.5f;
      }
      fireLP += 0.10f * (fs - fireLP);                     // fogata suave, como a unos metros
      dry += fireLP * 0.9f * eF;
    }

    // — LLUVIA: colchón que crece con la intensidad + gotitas sueltas (dominan al empezar) —
    if (eLl > 0.002f) {
      rainLP   += 0.22f * (white - rainLP);
      rainBody += 0.09f * (white - rainBody);
      float hiss = white - rainLP;                          // sibilancia aguda (el "shhh")
      float bed = powf(rainInt, 1.5f);                      // al mínimo casi no hay colchón
      float rs = (hiss * (0.015f + 0.26f * bed) + rainBody * 0.15f * bed) * rainVar;
      if (rnd01() < pTick) triggerRainTick();
      for (int i = 0; i < NUM_RAINTICKS; i++) {
        RainTick &rt = rainTicks[i];
        if (!rt.active) continue;
        rt.lp += rt.k * (white - rt.lp);
        rt.env *= rt.decay;
        if (rt.env < 0.02f) { rt.active = false; continue; }
        rs += rt.lp * rt.env * 0.24f;
      }
      dry += rs * eLl;
    }

    // — CUENCO —
    if (eCu > 0.002f) {
      float bs = 0.0f;
      for (int i = 0; i < 4; i++) {
        bowlPh[i] += bowlRoot * BOWL_RATIOS[i] / SAMPLE_RATE;
        if (bowlPh[i] >= 1.0f) bowlPh[i] -= 1.0f;
        bs += sinLUT(bowlPh[i]) * BOWL_AMPS[i];
      }
      dry += bs * 0.36f * bowlBreath * eCu * eCu;          // env² = ataque aún más suave
    }

    // — ARROYO —
    if (eA > 0.002f) {
      float ss = streamBP1.process(white) * 0.45f + streamBP2.process(white) * 0.28f;
      dry += ss * bubble * 0.28f * eA;                     // vertiente calma, no torrente
    }

    // — PÁJAROS —
    if (ePj > 0.002f) {
      float ps = 0.0f;
      for (int i = 0; i < 2; i++) {
        Bird &b = birds[i];
        float f;
        if (b.state == 1) {
          f = b.f0 + (b.f1 - b.f0) * b.notePos;
          b.env += (1.0f - b.env) * 0.008f;
        } else {
          f = b.f1;
          b.env *= 0.9985f;              // la nota muere en ~30 ms → notas bien separadas
        }
        if (b.env > 0.001f) {
          b.phase += f / SAMPLE_RATE;
          if (b.phase >= 1.0f) b.phase -= 1.0f;
          ps += sinLUT(b.phase) * b.env * b.ampl;
        }
      }
      ev += ps * 0.13f * ePj;
    }

    // — Mezcla directa (sin efectos): fondo + eventos balanceados por POT4 —
    float m = dry + ev * eventGain;

    // — Filtro tonal global + limitador + volumen —
    tiltL += kTilt * (m - tiltL);
    tiltR += kTilt * (m - tiltR);
    float outL = softclip(tiltL * 1.1f) * masterVol;
    float outR = softclip(tiltR * 1.1f) * masterVol;

    buffer[n * 2]     = (int16_t)(outL * 28000.0f);
    buffer[n * 2 + 1] = (int16_t)(outR * 28000.0f);
  }
}

// ══════════════════════════════════════════════════════════
//  FRAME DE LUCES (~30 FPS)
// ══════════════════════════════════════════════════════════

void ledFrame(uint32_t t) {
  fadeToBlackBy(leds + START_LED, EXT_LEN, 70);

  // — VIENTO: banda fluida azul-blanca en A + halo tenue en B —
  if (layerEnv[L_VIENTO] > 0.02f) {
    float e = layerEnv[L_VIENTO];
    windFlowPh += 0.06f + 0.30f * gust;
    for (int i = 0; i < RING_A_LEN; i++) {
      float s = 0.5f + 0.5f * sinf(i * (12.566f / RING_A_LEN) + windFlowPh);   // 2 ondas por vuelta
      uint8_t b = (uint8_t)(s * s * s * (12.0f + 75.0f * gust) * e);
      leds[RING_A_START + i] += CHSV(layerHue[L_VIENTO], 70, b);
    }
    uint8_t bB = (uint8_t)((6.0f + 18.0f * gust) * e);
    for (int i = 0; i < RING_B_LEN; i++) leds[RING_B_START + i] += CHSV(layerHue[L_VIENTO], 60, bB);
  }

  // — MAR: la ola llena el anillo A; B respira azul profundo en contrafase —
  if (layerEnv[L_MAR] > 0.02f) {
    float e = layerEnv[L_MAR];
    int filled = (int)(g_seaEnv * RING_A_LEN);
    for (int i = 0; i < filled && i < RING_A_LEN; i++)
      leds[RING_A_START + i] += CHSV(layerHue[L_MAR], 200, (uint8_t)((25.0f + 95.0f * g_seaEnv) * e));
    if (g_seaEnv > 0.80f && filled > 0 && filled <= RING_A_LEN)
      leds[RING_A_START + filled - 1] += CHSV(layerHue[L_MAR], 30, (uint8_t)(160.0f * e));   // espuma
    uint8_t bB = (uint8_t)((12.0f + 35.0f * (1.0f - g_seaEnv)) * e);
    for (int i = 0; i < RING_B_LEN; i++) leds[RING_B_START + i] += CHSV(160, 255, bB);
  }

  // — FUEGO: flicker de brasas en B + resplandor cálido leve en A —
  if (layerEnv[L_FUEGO] > 0.02f) {
    float e = layerEnv[L_FUEGO];
    for (int i = 0; i < RING_B_LEN; i++) {
      int h = heatB[i] + (int)(rng() % 41) - 20;
      int hMax = (int)(255.0f * (0.4f + 0.6f * flare));
      if (h < 25)   h = 25;
      if (h > hMax) h = hMax;
      heatB[i] = (uint8_t)h;
      CRGB c = HeatColor(heatB[i]);
      c.nscale8((uint8_t)(160.0f * e));
      leds[RING_B_START + i] += c;
    }
    for (int i = 0; i < RING_A_LEN; i++)
      leds[RING_A_START + i] += CHSV(layerHue[L_FUEGO], 240, (uint8_t)(10.0f * flare * e));
  }

  // — LLUVIA: velo azul-gris + gotitas al azar, ambos según la intensidad (POT2) —
  if (layerEnv[L_LLUVIA] > 0.02f) {
    float e = layerEnv[L_LLUVIA];
    float rInt = (density - 0.15f) * (1.0f / 2.85f);
    if (rInt < 0.0f) rInt = 0.0f;
    if (rInt > 1.0f) rInt = 1.0f;
    uint8_t veil = (uint8_t)((2.0f + 9.0f * rInt) * e);
    for (int i = 0; i < EXT_LEN; i++) leds[START_LED + i] += CHSV(layerHue[L_LLUVIA], 120, veil);
    int nDrops = (int)(e * (1.0f + 4.0f * rInt));
    for (int i = 0; i < nDrops; i++)
      leds[START_LED + (rng() % EXT_LEN)] += CHSV(layerHue[L_LLUVIA], 90, (uint8_t)(120.0f * e));
  }

  // — CUENCO: respiración violeta-ámbar lenta en ambos anillos —
  if (layerEnv[L_CUENCO] > 0.02f) {
    float e = layerEnv[L_CUENCO];
    float breath = 0.5f + 0.5f * sinLUT(bowlLfoPh);
    uint8_t hue = (uint8_t)(196.0f - 25.0f * breath);
    uint8_t b = (uint8_t)((10.0f + 45.0f * breath) * e);
    for (int i = 0; i < EXT_LEN; i++) leds[START_LED + i] += CHSV(hue, 180, b);
  }

  // — ARROYO: corriente verde-agua (ruido perlin desplazándose) en A —
  if (layerEnv[L_ARROYO] > 0.02f) {
    float e = layerEnv[L_ARROYO];
    for (int i = 0; i < RING_A_LEN; i++) {
      uint8_t nz = inoise8(i * 35, t / 6);
      leds[RING_A_START + i] += CHSV(100 + (nz >> 3), 210, (uint8_t)(nz * 0.45f * e));
    }
    for (int i = 0; i < RING_B_LEN; i++) {
      uint8_t nz = inoise8(i * 35, t / 6);
      leds[RING_B_START + i] += CHSV(110, 200, (uint8_t)(nz * 0.10f * e));
    }
  }

  // — GRILLOS: noche azul muy tenue en B —
  if (layerEnv[L_GRILLOS] > 0.02f) {
    uint8_t b = (uint8_t)(9.0f * layerEnv[L_GRILLOS]);
    for (int i = 0; i < RING_B_LEN; i++) leds[RING_B_START + i] += CHSV(160, 255, b);
  }

  // — Eventos: chispas y ondas expansivas (campanitas, gotas+eco, fuego, pájaros, grillos) —
  for (int i = 0; i < NUM_FX; i++) {
    LedFx &f = fxPool[i];
    if (!f.active) continue;
    if (f.delayF > 0) { f.delayF--; continue; }
    if (f.mode == FX_SPARK) {
      CRGB c = CHSV(f.hue, f.sat, (uint8_t)(f.level * 255.0f));
      addPix(f.ring, f.pos, c);
      CRGB h = c; h.nscale8(70);
      addPix(f.ring, f.pos - 1.0f, h);
      addPix(f.ring, f.pos + 1.0f, h);
      f.level *= f.decay;
      if (f.level < 0.02f) f.active = false;
    } else {                                       // FX_RIPPLE: dos frentes desde el origen
      float half = 0.5f * ringN(f.ring);           // media vuelta del anillo
      float lv = f.level * (1.0f - f.radius / half);
      if (lv > 0.0f) {
        CRGB c = CHSV(f.hue, f.sat, (uint8_t)(lv * 255.0f));
        addPix(f.ring, f.pos - f.radius, c);
        addPix(f.ring, f.pos + f.radius, c);
      }
      f.radius += f.speed;
      if (f.radius >= half) f.active = false;
    }
  }

  // — Flash de confirmación al alternar una capa —
  if (g_flashFrames > 0) {
    g_flashFrames--;
    uint8_t v = g_flashOn ? 150 : 35;
    fill_solid(leds + START_LED, EXT_LEN, CHSV(g_flashHue, 200, v));
  }

  // — LEDs internos de la placa = INDICADORES DE ESTADO —
  // LED 0..4 = BTN1..BTN5: color de la capa activa (principal o alternativa);
  // si están ambas encendidas, alterna entre los dos colores cada 0.4 s.
  for (int i = 0; i < 5; i++) {
    bool p = layerOn[i], a = layerOn[i + 5];
    if (p && a)      leds[i] = CHSV(layerHue[((t / 400) & 1) ? i : i + 5], 230, 90);
    else if (p)      leds[i] = CHSV(layerHue[i],     230, 90);
    else if (a)      leds[i] = CHSV(layerHue[i + 5], 230, 90);
    else             leds[i] = CRGB::Black;
  }
  // LED 5: respiración tenue cian si hay al menos una capa sonando
  bool anyOn = false;
  for (int i = 0; i < NUM_LAYERS; i++) if (layerOn[i]) { anyOn = true; break; }
  leds[5] = anyOn ? CRGB(CHSV(140, 80, 22 + (uint8_t)(16.0f * (0.5f + 0.5f * sinf(t * 0.003f)))))
                  : CRGB::Black;
  FastLED.show();
}

// ══════════════════════════════════════════════════════════
//  BOTONES: corta = capa principal · larga = capa alternativa
// ══════════════════════════════════════════════════════════

void toggleLayer(int l) {
  layerOn[l] = !layerOn[l];
  if (l == L_CUENCO && layerOn[l]) {
    const float roots[4] = {65.41f, 73.42f, 82.41f, 98.0f};   // C2 · D2 · E2 · G2 (grave)
    bowlRoot = roots[rng() % 4];
  }
  g_flashHue = layerHue[l];
  g_flashOn = layerOn[l];
  g_flashFrames = 4;
}

void scanButtons(unsigned long t) {
  for (int i = 0; i < NUM_BTN; i++) {
    bool now = digitalRead(BTN_PINS[i]);
    if (now == LOW && btnLast[i] == HIGH) {            // presionado
      btnPressedAt[i] = t;
      btnLongFired[i] = false;
    }
    if (now == LOW && !btnLongFired[i] && (t - btnPressedAt[i]) >= LONG_PRESS_MS) {
      toggleLayer(i + 5);                              // pulsación larga → capa alternativa
      btnLongFired[i] = true;
    }
    if (now == HIGH && btnLast[i] == LOW) {            // soltado
      if (!btnLongFired[i] && (t - btnPressedAt[i]) < LONG_PRESS_MS)
        toggleLayer(i);                                // pulsación corta → capa principal
    }
    btnLast[i] = now;
  }
}

// ─── Setup I2S ─────────────────────────────────────────────
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));
  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
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

// ─── Setup ─────────────────────────────────────────────────
void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);

  for (int i = 0; i < NUM_BTN; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
    btnLast[i] = HIGH; btnPressedAt[i] = 0; btnLongFired[i] = false;
  }
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i < LUT_SIZE; i++) sineLUT[i] = sinf(2.0f * (float)M_PI * i / LUT_SIZE);
  for (int i = 0; i < NUM_LAYERS; i++)    { layerOn[i] = false; layerEnv[i] = 0.0f; }
  for (int i = 0; i < NUM_CHIMES; i++)    chimes[i].active = false;
  for (int i = 0; i < NUM_DROPS; i++)     drops[i].active = false;
  for (int i = 0; i < NUM_CRACKLES; i++)  crackles[i].active = false;
  for (int i = 0; i < NUM_RAINTICKS; i++) rainTicks[i].active = false;
  for (int i = 0; i < NUM_FX; i++)        fxPool[i].active = false;
  for (int i = 0; i < RING_B_LEN; i++)    heatB[i] = 40;
  for (int i = 0; i < 2; i++)             { birds[i].state = 0; birds[i].t = 1.0f + i; birds[i].env = 0.0f; birds[i].phase = 0.0f; birds[i].ampl = 0.5f; }

  windBP.set(400.0f, 1.1f);
  windWhistle.set(1500.0f, 7.0f);
  streamBP1.set(900.0f, 1.8f);
  streamBP2.set(2300.0f, 1.8f);

  // LEDs
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHT);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  i2s_init();

  // Barrido de arranque por los dos anillos (confirma tira + segmentos)
  for (int i = 0; i < RING_B_LEN; i++) {
    if (i < RING_A_LEN) leds[RING_A_START + i] = CHSV(132, 200, 180);
    leds[RING_B_START + i] = CHSV(40, 210, 140);
    FastLED.show();
    delay(10);
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // Arranca EN SILENCIO: el usuario decide qué sonidos activar con los botones.
}

// ─── Loop ──────────────────────────────────────────────────
void loop() {
  unsigned long t = millis();

  scanButtons(t);

  // — Pots: 1 por buffer en rotación —
  static const uint8_t POT_PIN[4] = { POT_VOLUMEN, POT_DENSIDAD, POT_COLOR, POT_BALANCE };
  static uint8_t potScan = 0;
  int pi = potScan; potScan = (potScan + 1) & 3;
  applyPot(pi, readPot(POT_PIN[pi]));

  updateControl();

  int16_t buffer[BUFFER_SAMPLES * 2];
  renderAudio(buffer);

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);

  // — Luces: refrescar throttleado (~30 FPS) para no robar tiempo al I2S —
  if (t - lastLedFrame >= LED_FRAME_MS) {
    lastLedFrame = t;
    ledFrame(t);
  }
}
