// ==============================================================================================================================================
// PERCU-SYNTH — Pads Profundos Polifónicos + Arpegio (IMU) — GC Lab Chile
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
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Hermano "ambient" del trance_imu: MISMO motor de audio (I2S 44.1 kHz / 16-bit,
// osciladores PolyBLEP anti-aliasing, filtro biquad resonante barrido por el IMU,
// soft-limiter de seguridad) pero SIN secuenciador ni patrones. En su lugar es una
// máquina de PADS PROFUNDOS: cada botón dispara un ACORDE sostenido (drone) con
// ataque y release lentos sobre un pool de voces polifónico, repartidas en el campo
// ESTÉREO con detune (ensemble) → texturas anchas y cinematográficas.
//
// Encima del pad hay una capa opcional de ARPEGIO que recorre las notas del acorde
// que esté sonando (queda amarrado a la armonía) y pasa por el mismo filtro.
//
// Acordes por banco (BTN3 del Panel B cicla los 5 bancos). BTN1+BTN3 a la vez = 6º acorde:
//   Banco 0: C  G  Am F  Dm   (+ Em)
//   Banco 1: C  Am E7 F  G    (+ Dm)
//   Banco 2: C  E  F  Fm Em   (+ G)
//   Banco 3: Dm Bb A  C  F    (+ Gm)
//   Banco 4: E  Em B  Bm C    (+ D)
// Son acordes ABSOLUTOS (raíz + intervalos reales). Tocar el acorde activo otra vez
// lo APAGA (release con cola). Cambiar de acorde hace cross-fade.
//
// Sin LEDs y sin Serial a propósito: todo el presupuesto de CPU va al audio.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// TRES PANELES DE CONTROL (combos para cambiar de panel; el mismo combo te devuelve a A):
//   · BTN1 + BTN5 a la vez → Panel B (arpegio / armonía)
//   · BTN2 + BTN4 a la vez → Panel C (timbre / síntesis)
//
// ── PANEL A (interpretación) ──────────────────────────────────────────────────
// - BTN1..BTN5 → los 5 acordes del banco activo (LATCH: el mismo botón lo apaga)
// - BTN1 + BTN3 a la vez → 6º acorde del banco
// - BTN3 del Panel B cambia de banco SIN reiniciar el pad (aplica al próximo acorde)
// - POT1 (ADC1)  → Ataque    (5 ms percusivo → ~4 s de fundido lento, pad clásico)
// - POT2 (ADC2)  → Volumen del PAD (solo acordes; el arpegio tiene su propio volumen)
// - POT3 (ADC8)  → Release    (~0.2 s corto → ~8 s de cola enorme)
// - POT4 (ADC10) → Movimiento (LFO lento que respira: barre el filtro + tremolo)
//
// ── PANEL B (arpegio / armonía) — BTN1+BTN5 ───────────────────────────────────
// El arpegio recorre las notas del acorde activo. POT1 lo enciende (volumen > 0).
// - BTN1 → Transponer −1 semitono   ·  BTN5 → Transponer +1 semitono
// - BTN2 → Octava global −1          ·  BTN4 → Octava global +1
// - BTN3 → Cambiar BANCO de acordes (0 → 1 → 2)
// - POT1 (ADC1)  → Volumen del arpegio (en 0 = arpegio APAGADO)
// - POT2 (ADC2)  → Velocidad del arpegio (lento → rápido)
// - POT3 (ADC8)  → Rango de octavas del arpegio (1 → 3)
// - POT4 (ADC10) → Gate (largo de cada nota: staccato → casi ligado)
//
// ── PANEL C (timbre / síntesis) — BTN2+BTN4 ───────────────────────────────────
// Todos los botones actúan EN VIVO (no reinician el pad ni su ataque):
// - BTN1 → Tipo de arpegio ◀ anterior
// - BTN2 → Forma de onda  (Seno → Sierra → Cuadrada → Triangular)
// - BTN3 → Sub-osc (−12, cuerpo de bajo) on/off
// - BTN4 → Modo AUTO on/off (cama armónica generativa, ver abajo)
// - BTN5 → Tipo de arpegio ▶ siguiente
//     Tipos de arpegio: UP · DOWN · UP-DOWN · DOWN-UP · RANDOM · CHORD (bloque)
//
// MODO AUTO (BTN4 del Panel C): toca solo. Con una SEMILLA FIJA genera (siempre la
// misma) una progresión diatónica funcional en una tonalidad mayor — empieza en I y
// termina en V (cadencia V→I al loopear), 4–8 acordes — y la reproduce en loop. Cada
// acorde dura un nº fijo de negras elegido al azar en 4/4 (4, 8, 16, 32, 64 o 128;
// mín. 1 compás, tempo 80 BPM). El ARPEGIO pasa a tocar notas ALEATORIAS dentro de la
// escala mayor de la tonalidad (sube su volumen con POT1 del Panel B). Mientras AUTO
// está activo, los botones de acorde manuales se ignoran.
// - POT1 (ADC1)  → Detune/ensemble (duplica voces desafinadas → coro ancho audible)
// - POT2 (ADC2)  → Tono (oscuro → brillante: filtro suave, NO satura)
// - POT3 (ADC8)  → Piso de cutoff del filtro (carácter aunque no muevas el IMU)
// - POT4 (ADC10) → Resonancia base (Q) del filtro (el IMU suma encima)
//
// CONGELADO DE CONTROLES: al cambiar de panel los pots NO se auto-actualizan; quedan
// congelados. Cada pot retoma el control solo cuando se MUEVE (≥ 2 %). El acorde, la
// armonía/arp (Panel B) y el timbre (Panel C) PERSISTEN al volver al Panel A.
//
// - IMU (MPU6050): X → cutoff del LPF · Y → resonancia (Q). Activo en los 3 paneles.
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <Wire.h>
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
// Generoso: el detune duplica voces, el acorde nuevo entra mientras el anterior
// se va con su cola (cross-fade) y encima suma el arpegio → muchas voces a la vez.
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
// Evita llamar powf() al disparar acordes (un acorde dispara muchas voces de golpe;
// el pico de powf provocaba glitches). Rango -72..+72 semitonos.
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
// Genera con semilla FIJA una progresión diatónica funcional (siempre la misma) +
// duraciones aleatorias-pero-fijas en 4/4, y la reproduce en loop. El arpegio toca
// notas aleatorias dentro de la escala mayor de la tonalidad.
bool  autoMode = false;
#define AUTO_BPM   80
const uint32_t beatSamples = (uint32_t)SAMPLE_RATE * 60 / AUTO_BPM;  // 1 negra (4/4)
const uint32_t autoSeed = 0x1234ABCDu;     // semilla FIJA → la cama es siempre la misma
int   autoKeyRoot = 0;                     // tonalidad (mayor), semitonos desde C
int   autoLen     = 4;                     // nº de acordes de la progresión (4–8)
int   autoRoot[8];                         // raíz (semitonos desde C3, sin transpose/octava)
bool  autoMin[8];                          // ¿acorde menor?
int   autoBeats[8];                        // duración en negras (4·2^k: 4,8,16,32,64,128)
int   autoIdx = 0;
uint32_t autoSampleCount = 0, autoChordSamples = 0;
const int scaleMajor[7] = {0, 2, 4, 5, 7, 9, 11};  // escala mayor (para el arpegio aleatorio)

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
// (struct BiqState está definido arriba del todo, junto a los demás tipos)
float f_b0, f_b1, f_b2, f_a1, f_a2;
BiqState bqL = {0, 0, 0, 0};
BiqState bqR = {0, 0, 0, 0};

// ─── Estado de botones ─────────────────────────────────────
// (struct BtnState está definido arriba del todo, junto a los demás tipos)
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
// semi = semitonos desde C3 · gain = peso · pan = -1(L)..+1(R) · kind 0=pad 1=arp · layer = capa del pad.
void spawnVoice(int semi, float gain, float pan, uint8_t kind, uint8_t layer) {
  int idx = -1;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (!voices[i].active) { idx = i; break; }
  }
  if (idx < 0) {
    // Ninguna libre → robar la voz MENOS AUDIBLE (env·gain mínimo), no la más vieja.
    // Así se roban primero las colas casi apagadas → sin "clicks" de robo.
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

  // Tríada/acorde base. Cada nota se duplica en 2 voces desafinadas (pan opuesto)
  // → el detune del Panel C se vuelve un coro/ensemble claramente audible.
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
  // Modo AUTO → nota ALEATORIA dentro de la escala mayor de la tonalidad
  if (autoMode) {
    arpRng = arpRng * 1664525u + 1013904223u;
    int deg = (int)((arpRng >> 9) % 7);
    int oct = (int)((arpRng >> 16) % (uint32_t)arpRange);
    int base = autoKeyRoot + transpose + globalOctaveSemi;
    int semi = base + scaleMajor[deg] + 12 * oct + 12;   // +12: una octava sobre el pad
    float pan = (arpStep & 1) ? 0.5f : -0.5f;
    capArp(1);
    spawnVoice(semi, arpVol, pan, 1, 0);
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

// ─── Generar (con semilla FIJA) la cama armónica y arrancar el modo AUTO ──
void startAuto() {
  uint32_t s = autoSeed;
  static const int8_t degOff[6] = { 0, 2, 4, 5, 7, 9 };          // I ii iii IV V vi (mayor)
  static const bool   degMin[6] = { false, true, true, false, false, true };
  static const int8_t NEXT[6][4] = {                             // movimientos funcionales coherentes
    {3,4,5,1}, {4,3,4,3}, {5,3,5,3}, {4,0,1,4}, {0,5,0,5}, {3,1,4,3}
  };
  s = s * 1664525u + 1013904223u; autoKeyRoot = (int)((s >> 16) % 12);
  s = s * 1664525u + 1013904223u; autoLen = 4 + (int)((s >> 16) % 5);   // 4–8 acordes
  int deg = 0;                                                   // empieza en I (tónica = "casa")
  for (int i = 0; i < autoLen; i++) {
    if      (i == autoLen - 1) deg = 4;                          // termina en V → cadencia V-I al loopear
    else if (i > 0) { s = s * 1664525u + 1013904223u; deg = NEXT[deg][(s >> 16) % 4]; }
    autoRoot[i] = autoKeyRoot + degOff[deg];
    autoMin[i]  = degMin[deg];
    s = s * 1664525u + 1013904223u; uint32_t r = (s >> 16) % 100;
    int k = (r < 40) ? 0 : (r < 65) ? 1 : (r < 82) ? 2 : (r < 93) ? 3 : (r < 98) ? 4 : 5;
    autoBeats[i] = 4 << k;                                       // 4,8,16,32,64,128 negras (pesado a corto)
  }
  autoIdx = 0; autoSampleCount = 0;
  autoChordSamples = (uint32_t)((uint64_t)autoBeats[0] * beatSamples);
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
                // -6.5 ≈ caída a -60 dB en arpGate segundos → el control ES el
                // largo real de la nota (antes era la constante de tiempo → tail 7× más larga).
                arpDecCoef = expf(-6.5f / (arpGate * SAMPLE_RATE)); } break;
    }
  } else {
    // PANEL C — timbre / síntesis
    switch (i) {
      case 0: detuneCents = val * 40.0f;                      // POT1 → detune 0–40 cents
              panWidth    = 0.30f + val * 0.60f;     break;   //        + ancho estéreo
      case 1: toneCoef    = 0.015f + val * 0.985f;   break;   // POT2 → Tono oscuro→brillante
      case 2: cutoffBase  = 200.0f + val * 4800.0f;  break;   // POT3 → piso cutoff
      case 3: qBase       = 0.7f + val * 5.3f;       break;   // POT4 → resonancia base
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
    if (autoSampleCount >= autoChordSamples) {
      autoSampleCount = 0;
      autoIdx = (autoIdx + 1) % autoLen;
      autoChordSamples = (uint32_t)((uint64_t)autoBeats[autoIdx] * beatSamples);
      triggerAutoChord(autoRoot[autoIdx], autoMin[autoIdx]);
    }
  }

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

    // Dos buses independientes: PAD y ARPEGIO. El volumen del Panel A (g_volume)
    // se aplica SOLO al pad; el arpegio lleva su propio volumen (arpVol, Panel B).
    float padL = 0.0f, padR = 0.0f, arpL = 0.0f, arpR = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;

      float dt = v.freq / SAMPLE_RATE;
      v.phase += dt;
      if (v.phase >= 1.0f) v.phase -= 1.0f;

      float wave = osc(v.phase, dt);

      // Envolvente: pad = AHR (attack→sustain→release) · arp = pluck (attack→decay)
      if (v.stage == 0) {                         // attack
        v.env += (v.kind == 1) ? arpAtkInc : attackInc;
        if (v.env >= 1.0f) { v.env = 1.0f; v.stage = (v.kind == 1) ? 3 : 1; }
      } else if (v.stage == 2) {                  // release del pad
        v.env *= releaseCoef;
        if (v.env < 0.0008f) { v.active = false; v.env = 0.0f; continue; }
      } else if (v.stage == 3) {                  // decay del arpegio
        v.env *= arpDecCoef;
        if (v.env < 0.0008f) { v.active = false; v.env = 0.0f; continue; }
      }                                           // stage 1 = sustain del pad (mantiene)

      float a = wave * v.env * v.gain;
      if (v.kind == 0) { padL += a * v.lGain; padR += a * v.rGain; }  // pad
      else             { arpL += a * v.lGain; arpR += a * v.rGain; }  // arpegio
    }

    // Volumen del Panel A solo al pad; el arpegio ya trae su arpVol baked-in.
    float mixL = (padL * g_volume + arpL) * 0.13f;
    float mixR = (padR * g_volume + arpR) * 0.13f;
    // Anti-denormal: DC ínfimo (inaudible) para que los filtros nunca caigan a
    // números denormales (que en el FPU del ESP32 son lentísimos → glitches).
    mixL += 1.0e-18f;
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
}
