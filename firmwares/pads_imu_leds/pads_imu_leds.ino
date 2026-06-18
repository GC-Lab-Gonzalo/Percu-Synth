// ==============================================================================================================================================
// PERCU-SYNTH — Pads Profundos Polifónicos + Arpegio (IMU) + 6 LEDs de placa — GC Lab Chile
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
// - FastLED (instalar desde el gestor de librerías Arduino) — para los 6 LEDs de placa
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Igual que `pads_imu` (máquina de PADS PROFUNDOS: cada botón latchea un acorde
// sostenido estéreo, con capa de arpegio, filtro biquad barrido por el IMU), pero
// AHORA con un visualizador reactivo sobre los 6 LEDs WS2812 internos de la placa
// (los que `test_leds` dejaba apagados a propósito).
//
// El render de LEDs corre en el MISMO loop() que el audio (sin concurrencia): el
// único cuidado es throttlear FastLED.show() para no robarle tiempo a los buffers
// de I2S. FastLED en el ESP32-S3 usa el periférico RMT (no el I2S), así que NO
// choca con la salida de audio. Como son sólo 6 LEDs, cada refresco transmite en
// ~180 µs por hardware → impacto despreciable.
// ==============================================================================================================================================
// FUNCIONAMIENTO (LEDs)
// ==============================================================================================================================================
// Los 6 LEDs de la placa muestran 3 cosas a la vez:
//   1. PALETA = PANEL ACTIVO (también sirve de indicador de panel):
//        · Panel A (interpretación)  → cian/azul (marca GC Lab)
//        · Panel B (arpegio/armonía) → violeta
//        · Panel C (timbre/síntesis) → naranja
//      Al cambiar de panel, un flash blanco breve confirma el cambio.
//   2. BARRA (VU): cuántos LEDs encienden = energía del PAD (suma de envolventes de
//      los acordes). Más voces/cola = barra más llena. Sin acorde → respiración suave.
//   3. ARPEGIO: un PUNTO que CORRE por los 6 LEDs al ritmo del arpegio (avanza una
//      posición por nota), en color de contraste → ves su velocidad y movimiento.
//   El filtro del IMU desplaza el tono (filter sweep ↔ color sweep).
//
// El resto de controles (botones / pots / paneles / IMU) es IDÉNTICO a pads_imu.
// ==============================================================================================================================================
// FUNCIONAMIENTO (controles — igual que pads_imu)
// ==============================================================================================================================================
// TRES PANELES DE CONTROL (combos para cambiar de panel; el mismo combo te devuelve a A):
//   · BTN1 + BTN5 a la vez → Panel B (arpegio / armonía)
//   · BTN2 + BTN4 a la vez → Panel C (timbre / síntesis)
//
// ── PANEL A (interpretación) ──────────────────────────────────────────────────
// - BTN1..BTN5 → los 5 acordes del banco activo (LATCH) · BTN1+BTN3 = 6º acorde
//     5 bancos (BTN3 del Panel B cicla, sin reiniciar el pad):
//       0:C·G·Am·F·Dm(+Em)  1:C·Am·E7·F·G(+Dm)  2:C·E·F·Fm·Em(+G)
//       3:Dm·Bb·A·C·F(+Gm)  4:E·Em·B·Bm·C(+D)
// - POT1 → Ataque · POT2 → Volumen del PAD (no del arpegio) · POT3 → Release · POT4 → Movimiento
//
// ── PANEL B (arpegio / armonía) — BTN1+BTN5 ───────────────────────────────────
// - BTN1/BTN5 → transponer ∓1 · BTN2/BTN4 → octava global ∓1 · BTN3 → cambiar banco
// - POT1 → Volumen del arpegio (0 = apagado) · POT2 → Velocidad · POT3 → Rango octavas · POT4 → Gate
//
// ── PANEL C (timbre / síntesis) — BTN2+BTN4 ───────────────────────────────────
// (todo EN VIVO, sin reiniciar el pad) BTN1 → arpegio ◀ · BTN2 → forma de onda
// (Seno→Sierra→Cuadrada→Triangular) · BTN3 → sub-osc · BTN4 → modo AUTO · BTN5 → arpegio ▶
//   Tipos de arpegio: UP · DOWN · UP-DOWN · DOWN-UP · RANDOM · CHORD
//   Modo AUTO (BTN4): cada toggle genera una CANCIÓN NUEVA (tonalidad mayor/menor +
//   progresión aleatorias, loop en 4/4 con acordes de igual duración) + arpegio
//   aleatorio dentro del acorde que suena. Sube POT1 del Panel B.
// - POT1 → detune/ancho · POT2 → tono · POT3 → piso de cutoff · POT4 → resonancia base
//
// CONGELADO DE CONTROLES: al cambiar de panel los pots quedan congelados y sólo retoman
// el control cuando se MUEVEN (≥ 2 %). El acorde, el arpegio y el timbre PERSISTEN.
//
// - IMU (MPU6050): aceleración X → cutoff del LPF · aceleración Y → resonancia (Q).
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <Wire.h>
#include <FastLED.h>
#include <math.h>

// ─── Tipos (definidos arriba del todo para que el IDE de Arduino genere bien los
//     prototipos de funciones que los reciben por referencia) ───
struct BtnState { uint8_t pin; bool last; unsigned long lastPress; };
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
const unsigned long IMU_READ_INTERVAL = 10;   // ms entre lecturas del IMU
const float IMU_FILTER_ALPHA = 0.1f;          // suavizado de la lectura (0-1)

// ─── LEDs WS2812 (6 SMD internos de la placa) ──────────────
#define LED_PIN        46
#define NUM_LEDS        6     // SÓLO los 6 LEDs internos de la placa
#define LED_BRIGHT     250    // 0-255
#define LED_TYPE       WS2812
#define COLOR_ORDER    GRB
const unsigned long LED_REFRESH_MS = 22;  // refresco del visualizador (~45 fps)

// LED RGB direccionable del módulo ESP32-S3 (DevKitC-1 → GPIO48). Si tu placa lo
// tiene en otro pin (algunas usan 38/48), cambia ONBOARD_PIN.
#define ONBOARD_PIN    48

CRGB leds[NUM_LEDS];
CRGB onboard[1];             // el LED RGB de la placa (refleja la tira de 6)

// Métricas compartidas audio → LEDs (todo en el mismo hilo: sin volatile/locks)
float g_energy   = 0.0f;   // energía del pad suavizada (0..~1)
float g_arpFlash = 0.0f;   // intensidad del punto del arpegio (decae en cada render)
int   g_arpPos   = 0;      // posición (0..5) del punto del arpegio; avanza por nota
float flashLevel = 0.0f;   // flash blanco de cambio de panel (decae en cada render)

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
#define BTN1_PIN   44
#define BTN2_PIN   42
#define BTN3_PIN    0
#define BTN4_PIN   45
#define BTN5_PIN   47
const unsigned long DEBOUNCE_MS = 200;

// ─── Potenciómetros ────────────────────────────────────────
#define POT1   1    // ADC1
#define POT2   2    // ADC2
#define POT3   8    // ADC8
#define POT4  10    // ADC10

// ─── Polifonía ─────────────────────────────────────────────
#define NUM_VOICES   32

struct Voice {
  bool     active;
  uint8_t  kind;     // 0 = pad (AHR sostenido) · 1 = arpegio (pluck que decae solo)
  uint8_t  layer;    // capa del pad: 0 core (tríada) · 1 sub · 2 quinta (para toggles en vivo)
  float    freq;     // Hz
  float    phase;    // 0..1
  float    env;      // amplitud actual 0..1
  uint8_t  stage;    // 0 attack · 1 sustain(pad) · 2 release(pad) · 3 decay(arp)
  float    gain;     // peso fijo de la voz
  float    lGain;    // ganancia canal L (paneo equal-power)
  float    rGain;    // ganancia canal R
  uint32_t age;      // para robo de voz (la más vieja se reemplaza)
};
#define LYR_CORE  0
#define LYR_SUB   1
#define LYR_FIFTH 2

Voice voices[NUM_VOICES];
uint32_t voiceCounter = 0;

const float BASE_FREQ = 130.81f;           // C3 (referencia de semitono 0)

// ─── Tabla semitono → relación de frecuencia ───────────────
#define SEMI_OFFSET 72
#define SEMI_LUT_N  145
float semiLUT[SEMI_LUT_N];

inline float semiToFreq(int semi) {
  int idx = semi + SEMI_OFFSET;
  if (idx < 0) idx = 0; else if (idx >= SEMI_LUT_N) idx = SEMI_LUT_N - 1;
  return BASE_FREQ * semiLUT[idx];
}

// ─── Tabla de seno (oscilador seno barato, sin sinf() por muestra) ──
float sineLUT[256];
inline float oscSine(float phase) {
  float f = phase * 256.0f;
  int i0 = (int)f; float frac = f - (float)i0;
  i0 &= 255; int i1 = (i0 + 1) & 255;
  return sineLUT[i0] + (sineLUT[i1] - sineLUT[i0]) * frac;   // interpolación lineal
}

// ─── Acordes (raíz en semitonos desde C3 + lista de intervalos) ──
struct Chord { const char* name; int8_t root; int8_t iv[4]; uint8_t n; };

// 5 bancos × 6 acordes. Índices 0..4 = BTN1..BTN5; índice 5 = combo BTN1+BTN3.
#define NUM_BANKS 5
const Chord BANKS[NUM_BANKS][6] = {
  // Banco 0 — C  G  Am F  Dm   (+ BTN1+3: Em)
  { {"DoM", 0,{0,4,7,0},3}, {"SolM",7,{0,4,7,0},3}, {"Lam", 9,{0,3,7,0},3}, {"FaM", 5,{0,4,7,0},3}, {"Rem", 2,{0,3,7,0},3}, {"Mim", 4,{0,3,7,0},3} },
  // Banco 1 — C  Am E7 F  G    (+ BTN1+3: Dm)
  { {"DoM", 0,{0,4,7,0},3}, {"Lam", 9,{0,3,7,0},3}, {"Mi7", 4,{0,4,7,10},4},{"FaM", 5,{0,4,7,0},3}, {"SolM",7,{0,4,7,0},3}, {"Rem", 2,{0,3,7,0},3} },
  // Banco 2 — C  E  F  Fm Em   (+ BTN1+3: G)
  { {"DoM", 0,{0,4,7,0},3}, {"MiM", 4,{0,4,7,0},3}, {"FaM", 5,{0,4,7,0},3}, {"Fam", 5,{0,3,7,0},3}, {"Mim", 4,{0,3,7,0},3}, {"SolM",7,{0,4,7,0},3} },
  // Banco 3 — Dm Bb A  C  F    (+ BTN1+3: Gm)
  { {"Rem", 2,{0,3,7,0},3}, {"SibM",10,{0,4,7,0},3},{"LaM", 9,{0,4,7,0},3}, {"DoM", 0,{0,4,7,0},3}, {"FaM", 5,{0,4,7,0},3}, {"Solm",7,{0,3,7,0},3} },
  // Banco 4 — E  Em B  Bm C    (+ BTN1+3: D)
  { {"MiM", 4,{0,4,7,0},3}, {"Mim", 4,{0,3,7,0},3}, {"SiM",11,{0,4,7,0},3}, {"Sim",11,{0,3,7,0},3}, {"DoM", 0,{0,4,7,0},3}, {"ReM", 2,{0,4,7,0},3} },
};
int currentBank = 0;
int activeChord = -1;                       // -1 = nada sonando (latch)
int soundingRoot = 0;                       // raíz (semitonos) del acorde que SUENA ahora

// ─── Paneles de control ────────────────────────────────────
#define PANEL_A 0
#define PANEL_B 1
#define PANEL_C 2
int panel = PANEL_A;

// Paleta por panel (hue FastLED 0-255): A=cian/azul · B=violeta · C=naranja
inline uint8_t panelHue() {
  return (panel == PANEL_A) ? 140 : (panel == PANEL_B ? 192 : 24);
}

// ─── Armonía (Panel B) — PERSISTENTE ───────────────────────
int transpose        = 0;     // transposición fina en semitonos
int globalOctaveSemi = 0;     // octava global (arranca en C3)

// ─── Arpegio (Panel B) — PERSISTENTE ───────────────────────
#define ARP_MAX  8            // tope de voces simultáneas del arpegio (protege CPU/pad)
float arpVol      = 0.0f;     // POT1 → volumen (0 = apagado)
float arpRate     = 6.0f;     // POT2 → notas por segundo
int   arpRange    = 2;        // POT3 → rango de octavas (1–3)
float arpGate     = 0.12f;    // POT4 → largo de cada nota (s)
// Tipo de arpegio (BTN1/BTN5 del Panel C lo cambian ◀ / ▶)
#define ARP_UP      0
#define ARP_DOWN    1
#define ARP_UPDOWN  2
#define ARP_DOWNUP  3
#define ARP_RANDOM  4
#define ARP_CHORD   5
#define ARP_NTYPES  6
int   arpType     = ARP_UP;
int   arpNotes[4];            // semitonos del acorde activo (para arpegiar) — siempre 4 notas
int   arpCount    = 0;
int   arpStep     = 0;
uint32_t arpRng   = 0x12345678;  // estado del PRNG para el modo RANDOM
uint32_t arpSampleCount   = 0;
uint32_t arpSamplesPerStep = 7350;          // SR / arpRate (recalculado)
float arpAtkInc   = 0.02f;                  // ataque rápido del pluck del arp
float arpDecCoef  = 0.99877f;               // decay del pluck (≈ arpGate 0.12 s con -6.5)

// ─── Timbre (Panel C) — PERSISTENTE ────────────────────────
int   waveType    = 3;        // 0 = seno · 1 = sierra · 2 = cuadrada · 3 = triangular (arranca aquí)
bool  subOsc      = true;     // capa una octava abajo (cuerpo) — ON: pad profundo
float detuneCents = 12.0f;    // desafinación ensemble (POT1) — voces duplicadas
float panWidth    = 0.54f;    // ancho estéreo (deriva de detuneCents)
float toneCoef    = 0.55f;    // Tono (POT2): one-pole LPF, oscuro→brillante (no satura)
float toneL = 0.0f, toneR = 0.0f;
float cutoffBase  = 400.0f;   // piso de cutoff (POT3)
float qBase       = 1.0f;     // resonancia base (POT4)

// ─── Modo AUTO (BTN4 Panel C): cama armónica generativa con semilla ─────────
bool  autoMode = false;
#define AUTO_BPM   80
const uint32_t beatSamples = (uint32_t)SAMPLE_RATE * 60 / AUTO_BPM;  // 1 negra (4/4)
uint32_t autoSeed = 0x1234ABCDu;           // semilla; evoluciona en cada toggle → canción distinta
int   autoKeyRoot = 0;
int   autoLen     = 4;
int   autoRoot[8];
bool  autoMin[8];
int   autoBeats = 16;     // duración COMÚN de cada acorde en negras (igual para todos → loop tipo canción)
int   autoIdx = 0;
uint32_t autoSampleCount = 0, autoChordSamples = 0;

// ─── Movimiento (LFO lento, Panel A POT4) ──────────────────
float lfoPhase = 0.0f;
const float LFO_RATE = 0.15f; // Hz
float lfoAmt   = 0.0f;        // profundidad 0–1
float lfoCutMod = 0.0f;
float g_trem   = 1.0f;

// ─── Pots congelados al cambiar de panel ───────────────────
float       potAnchor[4] = {0, 0, 0, 0};
bool        potLive[4]   = {true, true, true, true};
uint8_t     potMoveCnt[4] = {0, 0, 0, 0};   // lecturas seguidas movido (anti-ruido)
bool        panelChanged = false;
const float POT_MOVE_THR = 0.04f;   // 4 %: hay que MOVER el pot a propósito (inmune al ruido del ADC)

// ─── Envolvente del pad (globales) ─────────────────────────
float g_volume    = 0.5f;
float attackInc   = 0.0008f;
float releaseCoef = 0.99995f;

// ─── IMU ───────────────────────────────────────────────────
float imu_x = 0.0f, imu_y = 0.0f;
float filtered_x = 0.0f, filtered_y = 0.0f;
unsigned long lastIMURead = 0;

// ─── Filtro biquad LPF resonante (estéreo: 2 estados) ──────
float f_b0, f_b1, f_b2, f_a1, f_a2;
BiqState bqL = {0, 0, 0, 0};
BiqState bqR = {0, 0, 0, 0};

// ─── Estado de botones ─────────────────────────────────────
BtnState bBtn3 = {BTN3_PIN, HIGH, 0};
bool b1Level = HIGH, b2Level = HIGH, b4Level = HIGH, b5Level = HIGH;
bool combo15 = false;
bool combo24 = false;
bool combo13 = false;   // BTN1+BTN3 → 6º acorde del banco (solo Panel A)

static i2s_chan_handle_t tx_chan;

// ─── Lectura de pot con sobre-muestreo ─────────────────────
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(pin);   // 16× oversampling (anti-ruido)
  return (float)(sum >> 4) / 4095.0f;
}

bool buttonPressed(BtnState &b) {
  bool now = digitalRead(b.pin);
  unsigned long t = millis();
  bool fired = false;
  if (now == LOW && b.last == HIGH && (t - b.lastPress) > DEBOUNCE_MS) {
    b.lastPress = t;
    fired = true;
  }
  b.last = now;
  return fired;
}

// ─── Disparar UNA voz (libre o roba la menos audible) ──────
// semi = semitonos desde C3 · gain = peso · pan = -1(L)..+1(R) · kind 0=pad 1=arp.
void spawnVoice(int semi, float gain, float pan, uint8_t kind, uint8_t layer) {
  int idx = -1;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (!voices[i].active) { idx = i; break; }
  }
  if (idx < 0) {
    float quietest = 1e30f;
    idx = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
      float a = voices[i].env * voices[i].gain;
      if (a < quietest) { quietest = a; idx = i; }
    }
  }

  // detune = 2^a con a pequeño → aprox. polinómica barata (evita exp2f por voz)
  float a = (pan * detuneCents) / 1200.0f;
  float det = 1.0f + a * (0.6931472f + a * 0.2401597f);

  Voice &v = voices[idx];
  v.active = true;
  v.kind   = kind;
  v.layer  = layer;
  v.freq   = semiToFreq(semi) * det;
  float ph = (float)voiceCounter * 0.61803f; ph -= (float)(int)ph;   // fase decorrelacionada
  v.phase  = ph;
  v.env    = 0.0f;
  v.stage  = 0;
  v.gain   = gain;
  v.age    = voiceCounter++;

  // Paneo equal-power vía la tabla de seno (evita cosf/sinf por voz):
  // rGain = sin(th), lGain = cos(th) = sin(th+90°). th=(p+1)·90°/2 → fase (p+1)/8.
  float p = pan * panWidth;
  if (p >  1.0f) p =  1.0f;
  if (p < -1.0f) p = -1.0f;
  float phr = (p + 1.0f) * 0.125f;
  v.rGain = oscSine(phr);
  v.lGain = oscSine(phr + 0.25f);
}

// ─── Mandar las voces del PAD a release (el arp decae solo) ──
void releaseAll() {
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind == 0 && voices[i].stage < 2)
      voices[i].stage = 2;
}

// ─── Capas sub / quinta: agregar o quitar EN VIVO (sin re-disparar el pad) ──
// Usan soundingRoot = la raíz del acorde que SUENA (no recalcula desde el banco,
// así cambiar de banco no desincroniza las capas con lo que está sonando).
void spawnLayerSub()   { if (activeChord >= 0) spawnVoice(soundingRoot - 12, 0.85f,  0.0f,  0, LYR_SUB); }
void releaseLayer(uint8_t layer) {
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind == 0 && voices[i].layer == layer && voices[i].stage < 2)
      voices[i].stage = 2;                       // a release (sale con cola, no de golpe)
}

// ─── Limitar voces del PAD: mata las MENOS audibles si se acumulan colas ──
// (con release muy largo las colas se apilaban y saturaban CPU → crasheo).
void capPad(int keep) {
  while (true) {
    int cnt = 0, vic = -1; float lo = 1e30f;
    for (int i = 0; i < NUM_VOICES; i++)
      if (voices[i].active && voices[i].kind == 0) {
        cnt++;
        float a = voices[i].env * voices[i].gain;
        if (a < lo) { lo = a; vic = i; }
      }
    if (cnt <= keep || vic < 0) break;
    voices[vic].active = false;
  }
}

// ─── Armar y disparar un acorde como pad sostenido ─────────
void triggerChord(int idx) {
  releaseAll();
  capPad(8);                                     // acota las colas viejas antes de sumar el acorde nuevo
  const Chord &ch = BANKS[currentBank][idx];
  int root = ch.root + transpose + globalOctaveSemi;
  soundingRoot = root;                           // recordar la raíz que ahora suena

  // Notas del acorde para el arpegio. Si es tríada (3 notas) se agrega la OCTAVA
  // de la raíz → SIEMPRE 4 notas (para arpegios en 4/4).
  arpCount = ch.n;
  for (int k = 0; k < ch.n; k++) arpNotes[k] = root + ch.iv[k];
  if (arpCount < 4) { arpNotes[arpCount] = root + 12; arpCount++; }
  arpStep = 0;

  const float pans[4] = { 0.0f, -0.7f, 0.7f, -0.35f };
  for (int n = 0; n < ch.n; n++) {
    int semi = root + ch.iv[n];
    spawnVoice(semi, 0.9f, pans[n] - 0.25f, 0, LYR_CORE);
    spawnVoice(semi, 0.9f, pans[n] + 0.25f, 0, LYR_CORE);
  }

  // Capa de timbre (Panel C) — etiquetada para poder togglearla en vivo
  if (subOsc) spawnVoice(root - 12, 0.85f, 0.0f, 0, LYR_SUB);   // cuerpo de bajo
}

// ─── Latch de acorde: tócalo para encender, otra vez para apagar ──
void playChord(int i) {
  if (autoMode) return;                 // en modo AUTO los acordes los maneja la cama
  if (activeChord == i) { releaseAll(); activeChord = -1; arpCount = 0; }
  else                  { triggerChord(i); activeChord = i; }
}

void retriggerActive() {
  if (activeChord >= 0) triggerChord(activeChord);
}

// ─── Liberar voces de arpegio viejas hasta que quepan 'toSpawn' nuevas ──
void capArp(int toSpawn) {
  while (true) {
    int active = 0, oldest = -1; uint32_t oa = 0xFFFFFFFF;
    for (int i = 0; i < NUM_VOICES; i++)
      if (voices[i].active && voices[i].kind == 1) {
        active++;
        if (voices[i].age < oa) { oa = voices[i].age; oldest = i; }
      }
    if (active + toSpawn <= ARP_MAX || oldest < 0) break;
    voices[oldest].active = false;
  }
}

// ─── Disparar UN paso del arpegio según el tipo activo ─────
// UP · DOWN · UP-DOWN · DOWN-UP · RANDOM · CHORD (acorde en bloque).
void arpTrigger() {
  // Modo AUTO → nota ALEATORIA del ACORDE que está sonando (sus 4 notas)
  if (autoMode) {
    if (arpCount > 0) {
      arpRng = arpRng * 1664525u + 1013904223u;
      int note = (int)((arpRng >> 9) % (uint32_t)arpCount);
      int oct  = (int)((arpRng >> 16) % (uint32_t)arpRange);
      int semi = arpNotes[note] + 12 * oct + 12;   // +12: una octava sobre el pad
      float pan = (arpStep & 1) ? 0.5f : -0.5f;
      capArp(1);
      spawnVoice(semi, arpVol, pan, 1, 0);
      g_arpFlash = 1.0f; g_arpPos = (g_arpPos + 1) % NUM_LEDS;   // punto que corre
    }
    arpStep++;
    return;
  }

  int total = arpCount * arpRange;
  if (total < 1) total = 1;

  if (arpType == ARP_CHORD) {
    int oct = arpStep % arpRange;               // bloque: todo el acorde en la octava del paso
    capArp(arpCount);
    for (int k = 0; k < arpCount; k++) {
      int semi  = arpNotes[k] + 12 * oct + 12;
      float pan = (k & 1) ? 0.4f : -0.4f;
      spawnVoice(semi, arpVol, pan, 1, 0);
    }
  } else {
    int p, idx = arpStep % total;
    if      (arpType == ARP_UP)     p = idx;
    else if (arpType == ARP_DOWN)   p = total - 1 - idx;
    else if (arpType == ARP_UPDOWN){ int period = 2 * total;   // extremos repetidos → ciclo múltiplo de 4 (4/4)
                                     int j = arpStep % period; p = (j < total) ? j : (period - 1 - j); }
    else if (arpType == ARP_DOWNUP){ int period = 2 * total;
                                     int j = arpStep % period; int pu = (j < total) ? j : (period - 1 - j);
                                     p = total - 1 - pu; }
    else if (arpType == ARP_RANDOM){ arpRng = arpRng * 1664525u + 1013904223u;   // LCG
                                     p = (int)((arpRng >> 8) % (uint32_t)total); }
    else                            p = idx;
    int note  = p % arpCount;
    int oct   = p / arpCount;
    int semi  = arpNotes[note] + 12 * oct + 12;     // +12: una octava sobre el pad
    float pan = (arpStep & 1) ? 0.5f : -0.5f;       // rebota en el estéreo
    capArp(1);
    spawnVoice(semi, arpVol, pan, 1, 0);
  }

  // Aviso para los LEDs: cada paso del arp enciende y avanza el punto que corre.
  g_arpFlash = 1.0f;
  g_arpPos   = (g_arpPos + 1) % NUM_LEDS;

  arpStep++;
  if (arpStep >= 1000000) arpStep = 0;          // guarda contra overflow (realineación imperceptible)
}

// ─── Disparar un acorde arbitrario (raíz + cualidad) como pad — modo AUTO ──
void triggerAutoChord(int rootBase, bool minor) {
  releaseAll();
  capPad(8);
  int root = rootBase + transpose + globalOctaveSemi;
  soundingRoot = root;
  int third = minor ? 3 : 4;
  arpNotes[0] = root; arpNotes[1] = root + third; arpNotes[2] = root + 7; arpNotes[3] = root + 12;
  arpCount = 4; arpStep = 0;
  const float pans[3] = { 0.0f, -0.7f, 0.7f };
  const int   iv[3]   = { 0, third, 7 };
  for (int n = 0; n < 3; n++) {
    int semi = root + iv[n];
    spawnVoice(semi, 0.9f, pans[n] - 0.25f, 0, LYR_CORE);
    spawnVoice(semi, 0.9f, pans[n] + 0.25f, 0, LYR_CORE);
  }
  if (subOsc) spawnVoice(root - 12, 0.85f, 0.0f, 0, LYR_SUB);
}

// ─── Generar una CANCIÓN NUEVA (tonalidad + modo + progresión aleatorios) ──
// Semilla fresca en cada toggle (mezcla micros + contador de voces) → cada vez que
// enciendes AUTO sale otra tonalidad (12), otro modo (mayor/menor) y otra progresión.
void startAuto() {
  autoSeed = autoSeed * 1664525u + 1013904223u + (uint32_t)micros() + voiceCounter;
  uint32_t s = autoSeed;

  static const int8_t majOff[6] = { 0, 2, 4, 5, 7, 9 };          // I  ii iii IV V  vi
  static const bool   majMin[6] = { false, true, true, false, false, true };
  static const int8_t minOff[6] = { 0, 3, 5, 7, 8, 10 };         // i  III iv V  VI VII
  static const bool   minMin[6] = { true, false, true, false, false, false };
  static const int8_t NEXTmaj[6][4] = {
    {3,4,5,1}, {4,3,4,3}, {5,3,5,3}, {4,0,1,4}, {0,5,0,5}, {3,1,4,3}
  };
  static const int8_t NEXTmin[6][4] = {
    {2,3,4,1}, {4,2,5,4}, {3,0,5,3}, {0,4,0,4}, {2,1,3,2}, {1,0,1,0}
  };

  s = s * 1664525u + 1013904223u; bool minorKey = ((s >> 17) & 1);
  s = s * 1664525u + 1013904223u; autoKeyRoot = (int)((s >> 16) % 12);
  s = s * 1664525u + 1013904223u; autoLen = 4 + (int)((s >> 16) % 5);   // 4–8 acordes
  // Duración COMÚN para TODOS los acordes (loop parejo tipo canción): 8 o 16 negras.
  s = s * 1664525u + 1013904223u; autoBeats = (((s >> 16) & 1) ? 16 : 8);

  int deg = 0;                                                   // empieza en la tónica (i/I)
  int endDeg = minorKey ? 3 : 4;                                 // termina en V → cadencia V→i
  for (int i = 0; i < autoLen; i++) {
    if      (i == autoLen - 1) deg = endDeg;
    else if (i > 0) { s = s * 1664525u + 1013904223u;
                      deg = minorKey ? NEXTmin[deg][(s >> 16) % 4] : NEXTmaj[deg][(s >> 16) % 4]; }
    autoRoot[i] = autoKeyRoot + (minorKey ? minOff[deg] : majOff[deg]);
    autoMin[i]  = minorKey ? minMin[deg] : majMin[deg];
  }
  autoSeed = s;                                                  // decorrelaciona para el próximo toggle
  autoIdx = 0; autoSampleCount = 0;
  autoChordSamples = (uint32_t)((uint64_t)autoBeats * beatSamples);   // igual para todos
  triggerAutoChord(autoRoot[0], autoMin[0]);
}

// ─── PolyBLEP ──────────────────────────────────────────────
inline float polyBlep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  else if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

// waveType: 0 = seno · 1 = sierra · 2 = cuadrada · 3 = triangular
inline float osc(float phase, float dt) {
  if (waveType == 0) {                        // SENO (tabla)
    return oscSine(phase);
  } else if (waveType == 1) {                 // SIERRA (anti-aliasing)
    return (2.0f * phase - 1.0f) - polyBlep(phase, dt);
  } else if (waveType == 2) {                 // CUADRADA = diferencia de 2 sierras
    float saw1 = (2.0f * phase - 1.0f) - polyBlep(phase, dt);
    float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
    float saw2 = (2.0f * p2 - 1.0f) - polyBlep(p2, dt);
    return (saw1 - saw2) * 0.6f;
  } else {                                     // TRIANGULAR
    return (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
  }
}

// ─── IMU ───────────────────────────────────────────────────
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

void updateFilter() {
  float xa = fabsf(filtered_x); if (xa > 1.0f) xa = 1.0f;
  float ya = fabsf(filtered_y); if (ya > 1.0f) ya = 1.0f;

  float cutoff = cutoffBase + xa * 7800.0f + lfoCutMod;
  if (cutoff < 80.0f)    cutoff = 80.0f;
  if (cutoff > 12000.0f) cutoff = 12000.0f;
  float Q = qBase + ya * 15.0f;
  if (Q > 18.0f) Q = 18.0f;

  float omega = 2.0f * (float)M_PI * cutoff / SAMPLE_RATE;
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

// ─── Visualizador de 6 LEDs (throttle por LED_REFRESH_MS) ──
// Corre en el mismo loop() que el audio. FastLED usa RMT (no I2S) → no choca.
void renderLEDs() {
  unsigned long t = millis();
  static unsigned long lastFrame = 0;
  if (t - lastFrame < LED_REFRESH_MS) return;
  lastFrame = t;

  // Flash de cambio de panel (confirma el cambio con un golpe de luz)
  static int lastPanel = PANEL_A;
  if (panel != lastPanel) { lastPanel = panel; flashLevel = 1.0f; }

  // Tono base = panel; el filtro del IMU lo desplaza (filter sweep ↔ color sweep)
  float cut = fabsf(filtered_x); if (cut > 1.0f) cut = 1.0f;
  uint8_t hue = panelHue() + (uint8_t)(cut * 40.0f);

  if (activeChord < 0) {
    // Sin acorde → respiración lenta en el color del panel
    static uint8_t breath = 0; static int8_t bdir = 1;
    breath += bdir * 3;
    if (breath >= 60) bdir = -1;
    if (breath <= 4)  bdir =  1;
    fill_solid(leds, NUM_LEDS, CHSV(hue, 220, breath));
  } else {
    // Pad sonando → barra VU = energía del pad
    float lvl = g_energy * 1.6f; if (lvl > 1.0f) lvl = 1.0f;
    float litf = lvl * NUM_LEDS;
    for (int i = 0; i < NUM_LEDS; i++) {
      float on = litf - i;                       // cuánto le toca a este LED (0..1)
      if (on < 0.0f) on = 0.0f; if (on > 1.0f) on = 1.0f;
      uint8_t v = 25 + (uint8_t)(on * 200.0f);   // piso tenue para que la barra "viva"
      leds[i] = CHSV(hue + i * 6, 255, v);       // leve gradiente a lo largo de la barra
    }
  }

  // Arpegio → punto que corre: LED brillante en g_arpPos, color de contraste (hue+128)
  if (arpVol > 0.02f && activeChord >= 0 && g_arpFlash > 0.02f) {
    uint8_t av = (uint8_t)(g_arpFlash * 255.0f);
    leds[g_arpPos % NUM_LEDS] += CHSV(hue + 128, 255, av);
  }
  g_arpFlash *= 0.6f;                            // decae entre notas

  // Destello blanco de cambio de panel: suma blanco a todos y decae
  if (flashLevel > 0.02f) {
    uint8_t w = (uint8_t)(flashLevel * 150.0f);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] += CRGB(w, w, w);
    flashLevel *= 0.55f;                          // decae rápido (render ~22 ms)
  }

  // LED RGB del módulo = promedio de la tira (×1.5 para que se vea), mismo color/energía
  uint16_t sr = 0, sg = 0, sb = 0;
  for (int i = 0; i < NUM_LEDS; i++) { sr += leds[i].r; sg += leds[i].g; sb += leds[i].b; }
  int or_ = (sr / NUM_LEDS) * 3 / 2, og_ = (sg / NUM_LEDS) * 3 / 2, ob_ = (sb / NUM_LEDS) * 3 / 2;
  onboard[0] = CRGB(or_ > 255 ? 255 : or_, og_ > 255 ? 255 : og_, ob_ > 255 ? 255 : ob_);

  FastLED.show();
}

// ─── Setup I2S ─────────────────────────────────────────────
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  chan_cfg.dma_desc_num   = 10;    // más descriptores DMA → más colchón ante picos de CPU
  chan_cfg.dma_frame_num  = 240;   // (≈ 54 ms de buffer total; latencia imperceptible en un pad)
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

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);
  pinMode(BTN5_PIN, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i < SEMI_LUT_N; i++)
    semiLUT[i] = powf(2.0f, (float)(i - SEMI_OFFSET) / 12.0f);

  for (int i = 0; i < 256; i++)
    sineLUT[i] = sinf(2.0f * (float)M_PI * (float)i / 256.0f);

  for (int i = 0; i < NUM_VOICES; i++)
    voices[i] = {false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  // LEDs de placa (6 SMD) + LED RGB del módulo
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

// ─── Aplicar un pot al parámetro del panel ACTUAL ──────────
void applyPot(int i, float val) {
  if (panel == PANEL_A) {
    // PANEL A — ataque / volumen / release / movimiento
    switch (i) {
      case 0: { float at = 0.005f + val * 4.0f;
                attackInc = 1.0f / (at * SAMPLE_RATE); } break;
      case 1: g_volume = val * val; break;        // volumen SOLO del pad (no del arpegio)
      case 2: { float rt = 0.2f + val * val * 8.0f;            // 0.2 – 8.2 s de release REAL
                // -6.5 ≈ caída a -60 dB en rt segundos (antes -1 = constante de tiempo → cola
                // ~7× más larga → con release alto las colas saturaban la CPU y crasheaba).
                releaseCoef = expf(-6.5f / (rt * SAMPLE_RATE)); } break;
      case 3: lfoAmt = val; break;
    }
  } else if (panel == PANEL_B) {
    // PANEL B — capa de ARPEGIO (todo en vivo)
    switch (i) {
      case 0: arpVol  = val;                         break;  // POT1 → volumen (0 = off)
      case 1: { arpRate = 2.0f + val * 14.0f;                // POT2 → 2–16 notas/s
                arpSamplesPerStep = (uint32_t)(SAMPLE_RATE / arpRate); } break;
      case 2: arpRange = 1 + (int)(val * 2.0f + 0.5f); break; // POT3 → 1–3 octavas
      case 3: { arpGate = 0.03f + val * 0.45f;               // POT4 → 30 ms – 480 ms
                arpDecCoef = expf(-6.5f / (arpGate * SAMPLE_RATE)); } break;
    }
  } else {
    // PANEL C — timbre / síntesis
    switch (i) {
      case 0: detuneCents = val * 40.0f;
              panWidth    = 0.30f + val * 0.60f;     break;
      case 1: toneCoef    = 0.015f + val * 0.985f;   break;
      case 2: cutoffBase  = 200.0f + val * 4800.0f;  break;
      case 3: qBase       = 0.7f + val * 5.3f;       break;
    }
  }
}

void loop() {
  // — Combos: BTN1+BTN5 → Panel B · BTN2+BTN4 → Panel C · BTN1+BTN3 → 6º acorde —
  bool b1 = digitalRead(BTN1_PIN);
  bool b2 = digitalRead(BTN2_PIN);
  bool b3 = digitalRead(BTN3_PIN);
  bool b4 = digitalRead(BTN4_PIN);
  bool b5 = digitalRead(BTN5_PIN);

  if (b1 == LOW && b5 == LOW && !combo15) {
    panel = (panel == PANEL_B) ? PANEL_A : PANEL_B;
    combo15 = true; panelChanged = true;
  }
  if (b2 == LOW && b4 == LOW && !combo24) {
    panel = (panel == PANEL_C) ? PANEL_A : PANEL_C;
    combo24 = true; panelChanged = true;
  }
  // BTN1+BTN3 (solo Panel A) → 6º acorde del banco (índice 5)
  if (panel == PANEL_A && b1 == LOW && b3 == LOW && !combo13) {
    playChord(5); combo13 = true;
  }

  // BTN1 ↑
  if (b1 == HIGH && b1Level == LOW && !combo15 && !combo13) {
    if      (panel == PANEL_A) playChord(0);
    else if (panel == PANEL_B) { transpose--; if (transpose < -12) transpose = -12;
                                 retriggerActive(); }
    else  arpType = (arpType + ARP_NTYPES - 1) % ARP_NTYPES;   // Panel C → tipo de arpegio ◀
  }
  // BTN5 ↑
  if (b5 == HIGH && b5Level == LOW && !combo15) {
    if      (panel == PANEL_A) playChord(4);
    else if (panel == PANEL_B) { transpose++; if (transpose > 12) transpose = 12;
                                 retriggerActive(); }
    else  arpType = (arpType + 1) % ARP_NTYPES;                // Panel C → tipo de arpegio ▶
  }
  // BTN2 ↑
  if (b2 == HIGH && b2Level == LOW && !combo24) {
    if      (panel == PANEL_A) playChord(1);
    else if (panel == PANEL_B) { globalOctaveSemi -= 12;
                                 if (globalOctaveSemi < -36) globalOctaveSemi = -36;
                                 retriggerActive(); }
    else  waveType = (waveType + 1) % 4;        // Panel C → forma de onda (en vivo, sin re-disparar)
  }
  // BTN4 ↑
  if (b4 == HIGH && b4Level == LOW && !combo24) {
    if      (panel == PANEL_A) playChord(3);
    else if (panel == PANEL_B) { globalOctaveSemi += 12;
                                 if (globalOctaveSemi > 12) globalOctaveSemi = 12;
                                 retriggerActive(); }
    else { autoMode = !autoMode;                // Panel C → modo AUTO (cama armónica generativa)
           if (autoMode) startAuto();
           else { releaseAll(); activeChord = -1; arpCount = 0; } }
  }

  if (b1 == HIGH && b5 == HIGH) combo15 = false;
  if (b2 == HIGH && b4 == HIGH) combo24 = false;
  if (b1 == HIGH && b3 == HIGH) combo13 = false;
  b1Level = b1; b2Level = b2; b4Level = b4; b5Level = b5;

  // — BTN3 (fuera de combos): acorde Lam / BANCO / sub-osc según panel —
  if (buttonPressed(bBtn3) && !combo13) {
    if      (panel == PANEL_A) playChord(2);
    else if (panel == PANEL_B) currentBank = (currentBank + 1) % NUM_BANKS;  // banco: NO re-dispara
    else { subOsc = !subOsc;                  // Panel C → sub-osc EN VIVO (sin re-disparar el pad)
           if (subOsc) spawnLayerSub(); else releaseLayer(LYR_SUB); }
  }

  // — Movimiento: LFO lento —
  lfoPhase += LFO_RATE * (float)BUFFER_SAMPLES / SAMPLE_RATE;
  if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
  float lfoVal = sinf(2.0f * (float)M_PI * lfoPhase);
  lfoCutMod = lfoVal * lfoAmt * 3000.0f;
  g_trem    = 1.0f - lfoAmt * 0.30f * (0.5f - 0.5f * lfoVal);

  // — IMU + filtro —
  readIMU();
  updateFilter();

  // — Pots: 1 por buffer en rotación, congelados al cambiar de panel —
  static const uint8_t POT_PIN[4] = { POT1, POT2, POT3, POT4 };
  static uint8_t potScan = 0;

  if (panelChanged) {
    // Captura las 4 anclas DE GOLPE (lectura limpia) y congela todo. Ningún
    // parámetro del panel nuevo cambia hasta que MUEVAS su pot.
    for (int i = 0; i < 4; i++) {
      potLive[i] = false; potMoveCnt[i] = 0; potAnchor[i] = readPot(POT_PIN[i]);
    }
    panelChanged = false;
  }

  int pi = potScan; potScan = (potScan + 1) & 3;
  float pv = readPot(POT_PIN[pi]);
  if (!potLive[pi]) {
    // Despierta sólo si el movimiento supera el umbral en 3 lecturas SEGUIDAS
    // (un pico de ruido no dura 3 lecturas → no despierta el pot solo).
    if (fabsf(pv - potAnchor[pi]) > POT_MOVE_THR) { if (++potMoveCnt[pi] >= 3) potLive[pi] = true; }
    else potMoveCnt[pi] = 0;
  }
  if (potLive[pi]) applyPot(pi, pv);

  // — Modo AUTO: avanza la progresión por tiempo (cada acorde dura sus negras) —
  if (autoMode) {
    autoSampleCount += BUFFER_SAMPLES;
    if (autoSampleCount >= autoChordSamples) {     // misma duración para cada acorde
      autoSampleCount = 0;
      autoIdx = (autoIdx + 1) % autoLen;
      triggerAutoChord(autoRoot[autoIdx], autoMin[autoIdx]);
    }
  }

  // — Energía del PAD para los LEDs: suma de envolventes de las voces del pad
  //   (kind 0), independiente del volumen, con seguidor de envolvente. —
  float vsum = 0.0f;
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind == 0) vsum += voices[i].env;
  vsum *= 0.16f;                                    // ~6 voces de pad a tope ≈ 1.0
  if (vsum > g_energy) g_energy = vsum;             // ataque instantáneo
  else                 g_energy = g_energy * 0.90f + vsum * 0.10f;  // release suave

  // — Generar buffer de audio (estéreo) —
  int16_t buffer[BUFFER_SAMPLES * 2];

  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    // — Arpegiador: dispara un paso cada arpSamplesPerStep (en AUTO suena siempre) —
    if (arpVol > 0.02f && (autoMode || (activeChord >= 0 && arpCount > 0))) {
      if (arpSampleCount >= arpSamplesPerStep) { arpSampleCount = 0; arpTrigger(); }
      arpSampleCount++;
    } else {
      arpSampleCount = 0;
    }

    // Dos buses independientes: PAD y ARPEGIO.
    float padL = 0.0f, padR = 0.0f, arpL = 0.0f, arpR = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;

      float dt = v.freq / SAMPLE_RATE;
      v.phase += dt;
      if (v.phase >= 1.0f) v.phase -= 1.0f;

      float wave = osc(v.phase, dt);

      if (v.stage == 0) {                         // attack
        v.env += (v.kind == 1) ? arpAtkInc : attackInc;
        if (v.env >= 1.0f) { v.env = 1.0f; v.stage = (v.kind == 1) ? 3 : 1; }
      } else if (v.stage == 2) {                  // release del pad
        v.env *= releaseCoef;
        if (v.env < 0.0008f) { v.active = false; v.env = 0.0f; continue; }
      } else if (v.stage == 3) {                  // decay del arpegio
        v.env *= arpDecCoef;
        if (v.env < 0.0008f) { v.active = false; v.env = 0.0f; continue; }
      }                                           // stage 1 = sustain del pad

      float a = wave * v.env * v.gain;
      if (v.kind == 0) { padL += a * v.lGain; padR += a * v.rGain; }  // pad
      else             { arpL += a * v.lGain; arpR += a * v.rGain; }  // arpegio
    }

    // Volumen del Panel A solo al pad; el arpegio ya trae su arpVol baked-in.
    float mixL = (padL * g_volume + arpL) * 0.13f;
    float mixR = (padR * g_volume + arpR) * 0.13f;
    mixL += 1.0e-18f;                             // anti-denormal
    mixR -= 1.0e-18f;
    float fL = applyFilter(bqL, mixL);
    float fR = applyFilter(bqR, mixR);

    // Tono (POT2 Panel C): one-pole LPF. Solo oscurece → nunca sube nivel ni satura.
    toneL += toneCoef * (fL - toneL);
    toneR += toneCoef * (fR - toneR);

    float gd = g_trem;                            // salida limpia (volumen ya por-bus)
    float vL = toneL * gd;
    float vR = toneR * gd;

    // Soft-clip de SEGURIDAD (tanh racional): solo actúa en picos extremos.
    if (vL >  3.0f) vL =  3.0f; if (vL < -3.0f) vL = -3.0f;
    if (vR >  3.0f) vR =  3.0f; if (vR < -3.0f) vR = -3.0f;
    float shL = vL * (27.0f + vL * vL) / (27.0f + 9.0f * vL * vL);
    float shR = vR * (27.0f + vR * vR) / (27.0f + 9.0f * vR * vR);

    buffer[n * 2]     = (int16_t)(shL * 30000.0f);
    buffer[n * 2 + 1] = (int16_t)(shR * 30000.0f);
  }

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);

  // — Visualizador de LEDs (throttled; tras volcar el buffer al DMA) —
  renderLEDs();
}
