// ==============================================================================================================================================
// PERCU-SYNTH — CANCIÓN ALEATORIA (generativa autónoma) + Melodía + 6 LEDs de placa — GC Lab Chile
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
// - 6 LEDs WS2812 SMD internos de la placa |DATA -> 46| (índices 0..5 de la tira)
// - LED RGB direccionable del módulo ESP32-S3 (DevKitC-1) |DATA -> 48| (refleja la tira)
// - 1 Botón con pull-up |BTN1 -> 44|  (PLAY / STOP)
// - 2 Potenciómetros analógicos |POT1 -> ADC1| (VOLUMEN de los PADS)
//                               |POT2 -> ADC2| (VOLUMEN de la MELODÍA)  → mezcla pad/melodía
//   (el resto de botones/pots queda inerte a propósito — este instrumento es autónomo)
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - Flash Mode         : DIO          (¡OPI rompe I2S!)
// - PSRAM              : OPI PSRAM
// - Monitor Serie      : 115200 baud  → al arrancar imprime el MOTIVO del último reset
//                        (PANIC/WDT = código · BROWNOUT = alimentación). Útil para diagnosticar
//                        cuelgues; para el backtrace usar "ESP Exception Decoder" del IDE.
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (incluye driver/i2s_std.h)
// - FastLED (instalar desde el gestor de librerías Arduino) — para los 6 LEDs de placa
//   (NO se usa el MPU6050 / IMU: este firmware es 100 % autónomo)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Máquina de CANCIONES ALEATORIAS. Hermano autónomo de `pads_imu_leds`: reutiliza su
// motor de voces estéreo (pads profundos con detune/paneo + filtro biquad resonante),
// pero elimina TODO el control manual salvo dos cosas:
//
//     · BTN1  → PLAY / STOP
//     · POT1  → VOLUMEN de los PADS
//     · POT2  → VOLUMEN de la MELODÍA   (para mezclar pad ↔ melodía)
//
// Al dar PLAY se sortea una CANCIÓN COMPLETA y coherente, todo aleatorio:
//     · Tonalidad (12) + MODO/escala (jónico, dórico, frigio, lidio, mixolidio, eólico,
//       menor armónica) → de ahí salen los acordes diatónicos y la escala de la melodía,
//       así la melodía SIEMPRE cae dentro de la armonía del pad.
//     · Progresión de acordes (caminata funcional diatónica, cadencia V→I al loopear).
//     · CLOCK: BPM y estado de ánimo (calmo / medio / movido) → densidad y duraciones.
//     · Parámetros de síntesis: formas de onda del pad, detune, ancho estéreo, tono,
//       piso de cutoff, resonancia, LFO de filtro (respiración), ataque/release del pad.
//
// La MELODÍA (reemplaza al arpegio pre-configurado): es una LÍNEA MONOFÓNICA generada
// nota a nota en tiempo real. NO hay patrón fijo. Cada nota sortea:
//     · Altura → caminata melódica dentro de la escala (pasos pequeños + saltos + enganche
//       a notas del acorde en curso) → coherente y cantable, nunca desafinada.
//     · Duración → múltiplos de la cuadrícula rítmica (semicorcheas del BPM) → siempre
//       "en el clock". Puede haber silencios.
//     · Ataque y decay → aleatorios por nota (desde plucks cortos a notas que se abren).
//     · Timbre → forma de onda de la melodía sorteada UNA VEZ al inicio de la canción y
//       FIJA durante toda ella (el brillo sí varía por frase → color tímbrico sin caos).
//     · Paneo → aleatorio (la melodía se pasea en el estéreo).
//
// ESTÉREO: el pad se filtra con dos LFO de filtro DESFASADOS (L/R) → el barrido de filtro
// se mueve distinto en cada canal, creando un estéreo amplio y vivo (además del detune/paneo).
//
// STOP silencia con cola natural. Volver a dar PLAY genera una canción TOTALMENTE nueva.
// ==============================================================================================================================================
// FUNCIONAMIENTO (LEDs — 6 SMD de la placa + LED RGB del módulo)
// ==============================================================================================================================================
//   1. COLOR = tonalidad de la canción; se desplaza con el acorde en curso y con el LFO
//      del filtro (color sweep ↔ filter sweep).
//   2. BARRA (VU) = energía del PAD (cuántos acordes/cola suenan).
//   3. PUNTO que CORRE = avanza una posición por cada NOTA de la melodía (su "pulso").
//   · Flash suave al cambiar de acorde · flash al arrancar una canción nueva.
//   · Sin PLAY → respiración lenta y tenue (modo espera).
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <math.h>
#include <esp_system.h>    // esp_reset_reason() — diagnóstico de cuelgues por el monitor serie

// ─── Tipos ─────────────────────────────────────────────────
struct BtnState { uint8_t pin; bool last; unsigned long lastPress; };
struct BiqState { float x1, x2, y1, y2; };
struct BiqCoef  { float b0, b1, b2, a1, a2; };   // coef. del filtro (uno por canal → estéreo)

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128

// ─── LEDs WS2812 (6 SMD internos de la placa) ──────────────
#define LED_PIN        46
#define NUM_LEDS        6
#define LED_BRIGHT     150    // 0-255 (brillo de la tira de 6 — apunta hacia abajo)
#define LED_TYPE       WS2812
#define COLOR_ORDER    GRB
const unsigned long LED_REFRESH_MS = 22;  // refresco del visualizador (~45 fps)

#define ONBOARD_PIN    48
#define ONBOARD_BRIGHT 45     // 0-255: brillo SOLO del LED frontal (apunta a la cara)

CRGB leds[NUM_LEDS];
CRGB onboard[1];

// Métricas compartidas audio → LEDs (todo en el mismo hilo: sin volatile/locks)
float g_energy   = 0.0f;   // energía del pad suavizada (0..~1)
float g_melFlash = 0.0f;   // intensidad del punto de la melodía (decae en cada render)
int   g_melPos   = 0;      // posición (0..5) del punto; avanza por nota
float g_chordFlash = 0.0f; // flash al cambiar de acorde
float g_startFlash = 0.0f; // flash blanco al arrancar canción nueva
uint8_t g_keyHue = 140;    // color base de la tonalidad
int   g_curDeg   = 0;      // grado del acorde en curso (para el color)
float g_filtLfoVal = 0.0f; // -1..1 del LFO de filtro (color sweep)

// ─── Botón PLAY/STOP + Pots de volumen (mezcla pad/melodía) ─
#define BTN1_PIN   44
#define POT1        1    // ADC1 → volumen de los PADS
#define POT2        2    // ADC2 → volumen de la MELODÍA
const unsigned long DEBOUNCE_MS = 220;
BtnState bPlay = {BTN1_PIN, HIGH, 0};

// ─── Polifonía ─────────────────────────────────────────────
#define NUM_VOICES   32
#define KIND_PAD 0
#define KIND_MEL 1
#define LYR_CORE 0
#define LYR_SUB  1

struct Voice {
  bool     active;
  uint8_t  kind;     // 0 = pad · 1 = melodía
  uint8_t  layer;    // 0 core · 1 sub
  float    freq;
  float    phase;    // 0..1
  float    env;      // amplitud actual 0..1
  uint8_t  stage;    // 0 attack · 1 sustain(pad) · 2 release(pad) · 3 decay(mel)
  float    gain;
  float    lGain, rGain;
  float    atkInc;   // incremento de ataque (por muestra)
  float    decCoef;  // coef. de decay (solo melodía)
  uint8_t  wave;     // forma de onda de ESTA voz
  float    lp;       // estado del LPF de brillo por voz
  float    lpA;      // coef. del LPF (1.0 = bypass · más bajo = más oscuro)
  uint8_t  afterAtk; // stage al terminar el ataque: 1 = sostener · 3 = decaer
  float    freqTarget; // objetivo de frecuencia — portamento/legato (melodía)
  float    glideCoef;  // velocidad de glide hacia freqTarget (1 = instantáneo)
  float    vibRamp;    // onset de vibrato 0→1 (melodía; el vibrato "entra" tras el ataque)
  float    vibPhase;   // fase del vibrato por voz (rate dinámico a lo largo de la nota)
  uint32_t age;
};
Voice voices[NUM_VOICES];
uint32_t voiceCounter = 0;

const float BASE_FREQ = 130.81f;           // C3 (referencia de semitono 0)

// Rango VOCAL humano (semitonos desde C3=0). La melodía se pliega a la octava para no salirse;
// por encima de SOFT_FROM las notas agudas suenan MÁS SUAVES y apagadas (menos molestas).
#define VOCAL_LO    0     // C3
#define VOCAL_HI   27     // ~D#5
#define SOFT_FROM  20     // ~G#4

// ─── Tabla semitono → relación de frecuencia ───────────────
#define SEMI_OFFSET 72
#define SEMI_LUT_N  145
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

// ─── PRNG (LCG) — se re-siembra en cada PLAY ───────────────
uint32_t rng = 0xC0FFEE11u;
inline uint32_t rndU()  { rng = rng * 1664525u + 1013904223u; return rng; }
inline float    rndF()  { return (float)(rndU() >> 8) * (1.0f / 16777216.0f); }  // 0..1
inline int      rndI(int n) { return (n <= 0) ? 0 : (int)((rndU() >> 9) % (uint32_t)n); }

// ==============================================================================================================================================
// MOTOR ARMÓNICO DIATÓNICO
// ==============================================================================================================================================
// 7 modos/escalas (intervalos en semitonos desde la tónica). De la escala derivamos
// TANTO los acordes del pad (tríadas diatónicas) COMO las notas de la melodía → coherencia total.
#define NUM_MODES 7
const int8_t MODES[NUM_MODES][7] = {
  { 0, 2, 4, 5, 7, 9, 11 },   // 0 Jónico (mayor)
  { 0, 2, 3, 5, 7, 9, 10 },   // 1 Dórico
  { 0, 1, 3, 5, 7, 8, 10 },   // 2 Frigio
  { 0, 2, 4, 6, 7, 9, 11 },   // 3 Lidio
  { 0, 2, 4, 5, 7, 9, 10 },   // 4 Mixolidio
  { 0, 2, 3, 5, 7, 8, 10 },   // 5 Eólico (menor natural)
  { 0, 2, 3, 5, 7, 8, 11 },   // 6 Menor armónica
};

// ─── Armonía POR MODO ──────────────────────────────────────
// Un modo solo "suena" a ese modo con SU vocabulario de acordes. Un acorde ajeno (o la
// cadencia V→I tonal) rompe la modalidad. Por eso cada modo define:
//   · degs[]  = grados diatónicos PERMITIDOS (0-indexados sobre la escala del modo).
//               Se excluyen los acordes disminuidos/aumentados y los que rompen el modo.
//   · cadence = grado FINAL de la progresión → resuelve a la tónica al loopear (cadencia modal).
// El constructor de tríadas (degSemi) ya saca la CUALIDAD correcta de cada acorde desde la
// escala del modo (p.ej. ♭II mayor en frigio, II mayor en lidio, ♭VII mayor en mixolidio).
struct ModeHarm { uint8_t nDeg; int8_t degs[6]; int8_t cadence; };
const ModeHarm MODE_HARM[NUM_MODES] = {
  // 0 JÓNICO (mayor):    I  IV V  vi ii      · cadencia V→I
  { 5, {0, 3, 4, 5, 1, 0}, 4 },
  // 1 DÓRICO:            i  IV ♭VII v  ♭III   · cadencia ♭VII→i   (color: IV mayor)
  { 5, {0, 3, 6, 4, 2, 0}, 6 },
  // 2 FRIGIO:            i  ♭II ♭III ♭VI ♭vii · cadencia ♭II→i    (cadencia frigia; color: ♭II mayor)
  { 5, {0, 1, 2, 5, 6, 0}, 1 },
  // 3 LIDIO:             I  II V  vi          · cadencia II→I     (color: II mayor; SIN IV)
  { 4, {0, 1, 4, 5, 0, 0}, 1 },
  // 4 MIXOLIDIO:         I  ♭VII IV v  vi      · cadencia ♭VII→I   (color: ♭VII mayor)
  { 5, {0, 6, 3, 4, 5, 0}, 6 },
  // 5 EÓLICO (menor):    i  ♭VI ♭VII iv ♭III  · cadencia ♭VII→i
  { 5, {0, 5, 6, 3, 2, 0}, 6 },
  // 6 MENOR ARMÓNICA:    i  iv V  ♭VI         · cadencia V→i      (este SÍ usa el V mayor con sensible)
  { 4, {0, 3, 4, 5, 0, 0}, 4 },
};

// Escala extendida a 5 octavas (35 grados) para la caminata melódica
#define N_SCALE 35
int scaleSemis[N_SCALE];        // semitono absoluto (desde C3) de cada grado
int8_t scaleInt[7];             // intervalos del modo elegido esta canción
int keyRoot = 0;                // tónica (0..11) de la canción

inline int degSemi(int d) { int oct = d / 7; int dd = d % 7; return scaleInt[dd] + 12 * oct; }

// ─── Progresión de la canción ──────────────────────────────
#define PROG_MAX 8
int  progDeg[PROG_MAX];     // grado de cada acorde
int  progBeats[PROG_MAX];   // duración en negras de cada acorde
int  progLen  = 4;
int  progIdx  = 0;
int  curChordTones[3];      // semitonos (mod 12) del acorde en curso — para enganchar la melodía

// ─── Clock rítmico ─────────────────────────────────────────
int      songBPM = 80;
uint32_t beatSamples = 33075;   // SR*60/BPM
uint32_t gridSamples = 8268;    // semicorchea = negra/4
uint32_t chordAcc    = 0;
uint32_t chordSamples = 0;

// ─── Melodía (línea monofónica generativa) ─────────────────
int   melIdx = 16;              // grado actual dentro de scaleSemis
int   melLo  = 10, melHi = 26;  // ventana de registro (grados)
int32_t melSamplesToNext = 0;   // muestras hasta el próximo evento
uint8_t melWave = 0;            // timbre de la frase actual
int   phraseLeft = 0;           // notas que faltan para cambiar de frase
float melGain = 0.85f;
float melRelCoef = 0.9997f;     // release SUAVE de la melodía: caída al terminar CADA nota (voz-like, no de golpe)
int   melPrevIdx = -999;        // grado de la nota anterior (para no repetir la misma nota muchas veces)
int   melRepeat  = 0;           // cuántas veces seguidas cayó en el mismo grado
#define MEL_NOTE_MAX_SEC 1.6f   // ninguna nota dura más de esto (recorta redondas en tempos lentos)

// Estilos de ritmo de FRASE (duraciones en semicorcheas). 4=negra 6=negra· 8=blanca 12=blanca·
// NOTAS LARGAS pero acotadas: mínimo = negra (4), máximo = blanca con puntillo (12). Sin redonda (16)
// ni corcheas sueltas (2/3) → notas largas y cantables sin que ninguna se eternice.
#define NSTYLE 5
const int STYLE_DUR[NSTYLE][6] = {
  { 12,  8,  8, 12,  8,  6 },   // 0 LONG    (blanca / blanca con puntillo)
  {  8,  6, 12,  6,  8,  8 },   // 1 FLOW    (negra· / blanca / blanca·)
  {  6,  8,  6,  4,  6, 12 },   // 2 RHYTHM  (negras / negra· / blanca)
  { 12,  8, 12,  8,  6, 12 },   // 3 SPARSE  (largo — blancas y blancas con puntillo)
  {  6,  4,  6,  8,  4,  6 },   // 4 MOVED   (negras y negras con puntillo)
};
const float STYLE_REST[NSTYLE] = { 0.12f, 0.12f, 0.12f, 0.26f, 0.10f };

// Pools por arquetipo (índices de estilo de ritmo / onda / envolvente que puede usar)
const uint8_t ARCHE_STYLES[5][4] = {
  {0,3,0,1}, {0,1,1,2}, {1,2,2,1}, {2,4,2,4}, {2,4,4,2},
};
const uint8_t ARCHE_WAVES[5][4] = {
  {0,3,3,0}, {1,3,1,0}, {1,2,1,2}, {2,1,3,2}, {1,2,2,1},
};
// Ondas de la MELODÍA: solo SUAVES/vocales. NUNCA cuadrada (2) → suena a 555, chillón.
// Triangular (3) y seno (0) son dulces; la sierra (1) solo aparece muy filtrada (ver newPhrase).
const uint8_t MEL_WAVES[4] = { 3, 0, 3, 1 };
// Envolventes: 0 = pluck/legato (decae suave DURANTE la nota) · 1 = sostenida (mantiene nivel).
// Ambas terminan con release SUAVE (nunca de golpe). Sin staccato → todo cantable.
const uint8_t ARCHE_ENVS[5][3] = {
  {1,1,0}, {1,1,0}, {0,1,1}, {0,1,0}, {0,1,0},
};

// Pasos melódicos (grados de escala): movimiento por grado + saltos ocasionales → contorno vivo
#define STEP_N 12
const int STEP_SET[STEP_N] = { -5, -4, -3, -2, -1, -1, 1, 1, 2, 3, 4, 5 };

// ─── Melodía: legato/portamento + vibrato (voz-like) ───────
int   melVoiceIdx    = -1;       // voz de melodía en curso (para LIGAR / hacer glide)
float legatoChance   = 0.30f;    // prob. de ligar a la nota anterior (portamento) — por frase
bool  lastWasPassing = false;    // la nota previa fue de PASO → la siguiente resuelve al acorde
// Vibrato SIEMPRE presente; rate aleatorio y DINÁMICO (acelera a lo largo de la nota vía vibRamp)
float melVibDepth = 0.010f;      // profundidad (ratio de pitch) — por nota
float melVibR0    = 4.0f;        // Hz al empezar la nota (más lento)
float melVibR1    = 6.5f;        // Hz al final de la nota (más rápido) → rate dinámico

// ─── Timbre / síntesis (por canción) ───────────────────────
uint8_t padWave = 3;            // forma de onda del pad
bool    subOsc  = true;
float   detuneCents = 12.0f;
float   panWidth    = 0.54f;
float   toneCoef    = 0.55f;    // one-pole LPF de "tono" (oscuro→brillante)
float   toneL = 0.0f, toneR = 0.0f;
float   padLevel = 0.85f, melLevel = 0.85f;

// ─── Variedad de pad + arquetipo de canción (por canción) ──
uint8_t  padVoicing = 0;        // 0 tríada · 1 tríada+octava · 2 séptima · 3 abierto (5ª)
float    padLpA = 1.0f;         // brillo del pad (LPF por voz)
float    padGateDepth = 0.0f;   // profundidad del "gate" rítmico del pad (0 = sostenido)
uint32_t padGateSamples = 0;    // periodo del gate en muestras (0 = off)
uint32_t padGateAcc = 0;
float    g_padGate = 1.0f;      // multiplicador de amplitud del pad (rítmico)

int      songArche = 0;         // arquetipo: 0 AMBIENT 1 CINEMATIC 2 PULSE 3 PLUCK 4 DRIVE
uint8_t  stylePool[4]   = {1,1,1,1}; int stylePoolN   = 4;  // ritmos de frase permitidos
uint8_t  melWavePool[4] = {0,1,2,3}; int melWavePoolN = 4;  // ondas de melodía permitidas
uint8_t  envPool[3]     = {0,1,2};   int envPoolN     = 3;  // envolventes permitidas
float    subChance = 0.2f;      // prob. de duplicar la melodía una octava abajo (cuerpo)
float    lpMin = 0.4f, lpMax = 0.95f;  // rango de brillo (LPF) de las frases

// Estado de la FRASE actual de la melodía
uint8_t  phraseStyle = 1, phraseEnv = 1;
int8_t   phraseOct   = 0;
bool     phraseSub   = false;
float    phraseLpA   = 0.7f;

// ─── Filtro biquad LPF resonante (LFO autónomo, sin IMU) ───
// Dos juegos de coeficientes (L/R) alimentados por LFO DESFASADO → estéreo amplio y vivo.
float cutoffBase = 700.0f;
float qBase      = 1.4f;
float filtLfoPhase = 0.0f;
float filtLfoRate  = 0.12f;     // Hz
float filtLfoDepth = 2200.0f;
BiqCoef coefL = {0, 0, 0, 0, 0};
BiqCoef coefR = {0, 0, 0, 0, 0};
BiqState bqL = {0, 0, 0, 0};
BiqState bqR = {0, 0, 0, 0};

// ─── Envolvente del pad (por canción) ──────────────────────
float g_padVol    = 0.7f;       // POT1 → volumen de los pads
float g_melVol    = 0.7f;       // POT2 → volumen de la melodía
float padAtkInc   = 0.0008f;
float padReleaseCoef = 0.99995f;

// ─── Estado de reproducción ────────────────────────────────
bool playing = false;

static i2s_chan_handle_t tx_chan;

// ─── Lectura de pot con sobre-muestreo ─────────────────────
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(pin);
  return (float)(sum >> 4) / 4095.0f;
}

bool buttonPressed(BtnState &b) {
  bool now = digitalRead(b.pin);
  unsigned long t = millis();
  bool fired = false;
  if (now == LOW && b.last == HIGH && (t - b.lastPress) > DEBOUNCE_MS) {
    b.lastPress = t; fired = true;
  }
  b.last = now;
  return fired;
}

// ─── Disparar UNA voz (libre o roba la menos audible) ──────
int spawnVoice(int semi, float gain, float pan, uint8_t kind, uint8_t layer,
               float atkInc, float decCoef, uint8_t wave, float lpA, uint8_t afterAtk) {
  int idx = -1;
  for (int i = 0; i < NUM_VOICES; i++) if (!voices[i].active) { idx = i; break; }
  if (idx < 0) {
    float quietest = 1e30f; idx = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
      float a = voices[i].env * voices[i].gain;
      if (a < quietest) { quietest = a; idx = i; }
    }
  }

  // detune (ensemble) SOLO para el pad; la melodía va AFINADA (una voz sola no se desafina)
  float det = 1.0f;
  if (kind == KIND_PAD) {
    float a = (pan * detuneCents) / 1200.0f;
    det = 1.0f + a * (0.6931472f + a * 0.2401597f);
  }

  Voice &v = voices[idx];
  v.active = true; v.kind = kind; v.layer = layer;
  v.freq   = semiToFreq(semi) * det;
  float ph = (float)voiceCounter * 0.61803f; ph -= (float)(int)ph;
  v.phase  = ph;
  v.env    = 0.0f; v.stage = 0;
  v.gain   = gain;
  v.atkInc = atkInc; v.decCoef = decCoef; v.wave = wave;
  v.lp = 0.0f; v.lpA = lpA; v.afterAtk = afterAtk;
  v.freqTarget = v.freq; v.glideCoef = 1.0f; v.vibRamp = 0.0f; v.vibPhase = 0.0f;
  v.age    = voiceCounter++;

  // Paneo equal-power vía tabla de seno
  float p = pan * panWidth;
  if (p >  1.0f) p =  1.0f; if (p < -1.0f) p = -1.0f;
  float phr = (p + 1.0f) * 0.125f;
  v.rGain = oscSine(phr);
  v.lGain = oscSine(phr + 0.25f);
  return idx;
}

// ─── Enviar todas las voces del PAD a release ──────────────
void releaseAllPad() {
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind == KIND_PAD && voices[i].stage < 2)
      voices[i].stage = 2;
}

// ─── Cerrar la nota anterior con caída SUAVE (legato vocal, nunca de golpe) ──
// Al llegar la próxima nota, la actual pasa a un release suave (melRelCoef) → se solapan
// un poco (legato) y ninguna termina abruptamente.
void fadeMelody() {
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind == KIND_MEL) {
      voices[i].stage = 3;
      voices[i].decCoef = melRelCoef;
    }
}

// ─── Limitar colas del PAD ─────────────────────────────────
// Las voces sobrantes NO se cortan en seco (eso hacía "chasquidos" en cada cambio de acorde):
// se mandan a un fundido rápido (~6 ms, stage 3) → desaparecen suave sin discontinuidad.
// Las que ya están en ese fundido (stage 3) no cuentan para el límite (ya van de salida).
void capPad(int keep) {
  float fastFade = expf(-6.5f / (0.006f * SAMPLE_RATE));   // ~6 ms
  int cnt = 0;
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind == KIND_PAD && voices[i].stage != 3) cnt++;
  while (cnt > keep) {
    int vic = -1; float lo = 1e30f;
    for (int i = 0; i < NUM_VOICES; i++)
      if (voices[i].active && voices[i].kind == KIND_PAD && voices[i].stage != 3) {
        float a = voices[i].env * voices[i].gain;
        if (a < lo) { lo = a; vic = i; }
      }
    if (vic < 0) break;
    voices[vic].stage = 3;               // fundido rápido (stage 3 sirve para cualquier voz)
    voices[vic].decCoef = fastFade;
    cnt--;
  }
}

// ─── Disparar un acorde diatónico como pad sostenido ───────
void triggerPadChord(int deg) {
  releaseAllPad();
  capPad(8);
  int r  = keyRoot + degSemi(deg);
  int t3 = keyRoot + degSemi(deg + 2);
  int t5 = keyRoot + degSemi(deg + 4);
  int t7 = keyRoot + degSemi(deg + 6);

  // Voicing (varía el CUERPO del acorde entre canciones)
  int tones[4]; int nt = 0;
  if (padVoicing == 3) {                          // abierto: raíz · quinta · octava (sin 3ª)
    tones[nt++] = r; tones[nt++] = t5; tones[nt++] = r + 12;
  } else {
    tones[nt++] = r; tones[nt++] = t3; tones[nt++] = t5;
    if (padVoicing == 1) tones[nt++] = r + 12;    // + octava
    if (padVoicing == 2) tones[nt++] = t7;        // + séptima diatónica
  }

  // Notas de referencia para enganchar la melodía (raíz/3ª/5ª — siempre diatónicas)
  curChordTones[0] = ((r  % 12) + 12) % 12;
  curChordTones[1] = ((t3 % 12) + 12) % 12;
  curChordTones[2] = ((t5 % 12) + 12) % 12;

  for (int n = 0; n < nt; n++) {
    float pan = (n & 1) ? 0.55f : -0.55f;
    spawnVoice(tones[n], 0.55f, pan - 0.22f, KIND_PAD, LYR_CORE, padAtkInc, 0.0f, padWave, padLpA, 1);
    spawnVoice(tones[n], 0.55f, pan + 0.22f, KIND_PAD, LYR_CORE, padAtkInc, 0.0f, padWave, padLpA, 1);
  }
  if (subOsc) spawnVoice(r - 12, 0.50f, 0.0f, KIND_PAD, LYR_SUB, padAtkInc, 0.0f, padWave, padLpA, 1);
}

// ─── Avanzar la progresión (por tiempo) ────────────────────
void nextChord() {
  progIdx = (progIdx + 1) % progLen;
  chordSamples = (uint32_t)progBeats[progIdx] * beatSamples;
  g_curDeg = progDeg[progIdx];
  triggerPadChord(progDeg[progIdx]);              // actualiza curChordTones
  g_chordFlash = 1.0f;

  // Suspensión→resolución: si una nota SOSTENIDA quedó colgada y ya NO es del acorde nuevo,
  // deslízala (glide) a la nota de acorde más cercana → nunca queda una nota chocando.
  if (melVoiceIdx >= 0 && voices[melVoiceIdx].active &&
      voices[melVoiceIdx].kind == KIND_MEL && voices[melVoiceIdx].stage == 1) {
    int pc = ((scaleSemis[melIdx] % 12) + 12) % 12;
    if (pc != curChordTones[0] && pc != curChordTones[1] && pc != curChordTones[2]) {
      snapMelToChord();
      int semi = scaleSemis[melIdx] + phraseOct;
      while (semi > VOCAL_HI) semi -= 12;
      while (semi < VOCAL_LO) semi += 12;
      voices[melVoiceIdx].freqTarget = semiToFreq(semi);
      voices[melVoiceIdx].glideCoef  = 1.0f / (0.09f * SAMPLE_RATE);   // resuelve con glide suave
    }
  }
}

// ─── Elegir la altura de la próxima nota (dentro de la escala) ──
inline void clampMelWindow() {
  if (melIdx < melLo) melIdx = melLo + (melLo - melIdx);   // reflejar en los bordes
  if (melIdx > melHi) melIdx = melHi - (melIdx - melHi);
  if (melIdx < melLo) melIdx = melLo;
  if (melIdx > melHi) melIdx = melHi;
}

// Mueve melIdx al grado de ACORDE más cercano dentro de la ventana vocal
void snapMelToChord() {
  for (int off = 0; off <= 12; off++)
    for (int s = -1; s <= 1; s += 2) {
      int cand = melIdx + off * s;
      if (cand < melLo || cand > melHi) continue;
      int pc = ((scaleSemis[cand] % 12) + 12) % 12;
      if (pc == curChordTones[0] || pc == curChordTones[1] || pc == curChordTones[2]) { melIdx = cand; return; }
      if (off == 0) break;
    }
}

// chordTone=true  → paso libre (con saltos) + aterriza en NOTA DEL ACORDE → varía con los acordes.
// chordTone=false → paso PEQUEÑO (±1/±2): nota de PASO, vecina de un acorde (se resuelve luego).
void pickMelodyPitch(bool chordTone) {
  if (chordTone) {
    melIdx += STEP_SET[rndI(STEP_N)];
    clampMelWindow();
    snapMelToChord();
  } else {
    int step = (rndF() < 0.7f) ? 1 : 2;
    if (rndF() < 0.5f) step = -step;
    melIdx += step;
    clampMelWindow();
  }
}

// ─── Abrir una FRASE nueva: fija su carácter (ritmo/timbre/registro/brillo) ──
void newPhrase() {
  phraseLeft  = 3 + rndI(7);                      // 3–9 notas por frase
  phraseStyle = stylePool[rndI(stylePoolN)];      // ritmo de la frase (según arquetipo)
  // melWave NO cambia aquí: se fija UNA vez por canción en startSong() (timbre estable).
  phraseEnv   = envPool[rndI(envPoolN)];          // envolvente: 0 pluck/legato · 1 sostenida
  static const int8_t OCTS[4] = { 0, 0, 0, -12 };  // NUNCA sube octava (evita agudos); a veces baja
  phraseOct   = OCTS[rndI(4)];
  phraseSub   = (rndF() < subChance);             // duplicar 8ª abajo (cuerpo) en algunas frases
  // Brillo: la SIERRA se filtra fuerte (que no zumbe); seno/triangular pueden brillar más
  float lp = lpMin + rndF() * (lpMax - lpMin);
  phraseLpA = (melWave == 1) ? (0.10f + lp * 0.35f) : (0.22f + lp * 0.55f);
  if (phraseLpA > 0.85f) phraseLpA = 0.85f;
  // Ligados (portamento): cuánto liga esta frase — más ligado (frases más cantadas/unidas)
  legatoChance = 0.40f + rndF() * 0.45f;          // 0.40–0.85
  melIdx = (melLo + melHi) / 2 + (rndI(7) - 3);   // recentra el registro al abrir frase
}

// ─── Disparar un evento de melodía (nota o silencio) ───────
void melodyStep() {
  if (phraseLeft <= 0) newPhrase();
  phraseLeft--;

  int units = STYLE_DUR[phraseStyle][rndI(6)];    // duración en semicorcheas (según el estilo de frase)
  // Cap absoluto de duración: en tempos lentos una blanca/blanca· puede pasar de ~1.6 s y "eternizarse".
  // Recorto de a pasos de negra (múltiplos de 4) para no salirme de la rejilla rítmica.
  uint32_t maxDur = (uint32_t)(SAMPLE_RATE * MEL_NOTE_MAX_SEC);
  while (units > 4 && (uint32_t)units * gridSamples > maxDur) units -= 4;
  uint32_t durSamples = (uint32_t)units * gridSamples;
  melSamplesToNext = (int32_t)durSamples;

  // Silencio (respiración): cierra la nota actual con release suave y corta el ligado
  if (rndF() < STYLE_REST[phraseStyle]) { fadeMelody(); melVoiceIdx = -1; lastWasPassing = false; return; }

  // COHERENCIA: notas medias/largas = NOTA DEL ACORDE; cortas pueden ser de PASO (y resuelven
  // en la siguiente). Así hay variedad melódica SIN notas que choquen con el acorde.
  bool longish  = (units >= 6);
  bool shortish = (units <= 3);
  bool chordTone;
  if      (longish || lastWasPassing) chordTone = true;
  else if (shortish)                  chordTone = (rndF() < 0.30f);   // cortas: sobre todo de paso
  else                                chordTone = (rndF() < 0.70f);   // negras: sobre todo de acorde
  pickMelodyPitch(chordTone);

  // ANTI-REPETICIÓN: si cae en el mismo grado que la nota anterior, cuéntalo; a la 2ª repetición
  // fuerza un salto (2–4 grados) para que NUNCA se quede martillando la misma nota muchas veces.
  if (melIdx == melPrevIdx) {
    if (++melRepeat >= 2) {
      int tries = 0;
      while (melIdx == melPrevIdx && tries++ < 6) {
        int dir = (melIdx > (melLo + melHi) / 2) ? -1 : 1;   // aléjate del borde
        if (rndF() < 0.35f) dir = -dir;
        melIdx += dir * (2 + rndI(3));
        clampMelWindow();
        if (chordTone) snapMelToChord();
      }
      melRepeat = 0;
    }
  } else {
    melRepeat = 0;
  }
  melPrevIdx = melIdx;
  lastWasPassing = !chordTone;

  // Altura → semitono, PLEGADO al rango vocal humano (nada de agudos molestos)
  int semi = scaleSemis[melIdx] + phraseOct;
  while (semi > VOCAL_HI) semi -= 12;
  while (semi < VOCAL_LO) semi += 12;
  float freq = semiToFreq(semi);

  // ¿LIGADO? → glide (portamento) de la voz actual a la nueva altura, SIN re-atacar (voz-like)
  bool canLegato = (melVoiceIdx >= 0 && voices[melVoiceIdx].active &&
                    voices[melVoiceIdx].kind == KIND_MEL && voices[melVoiceIdx].env > 0.30f);
  if (canLegato && rndF() < legatoChance) {
    // Libera cualquier OTRA voz de melodía (p.ej. la capa sub anterior) para que no quede
    // sonando la altura vieja; solo la voz principal hace glide.
    for (int i = 0; i < NUM_VOICES; i++)
      if (i != melVoiceIdx && voices[i].active && voices[i].kind == KIND_MEL && voices[i].stage != 3) {
        voices[i].stage = 3; voices[i].decCoef = melRelCoef;
      }
    Voice &v = voices[melVoiceIdx];
    v.freqTarget = freq;
    float glideSec = 0.03f + rndF() * 0.06f;       // 30–90 ms de glide
    v.glideCoef = 1.0f / (glideSec * SAMPLE_RATE);
    v.stage = 1;                                   // sostiene (la nota ligada continúa)
    g_melFlash = 1.0f; g_melPos = (g_melPos + 1) % NUM_LEDS;
    return;
  }

  // Nota NUEVA articulada: cierra la anterior con release suave y dispara con ataque
  fadeMelody();

  float durSec = (float)durSamples / SAMPLE_RATE;
  float atkSec, decSec; uint8_t afterAtk;
  if (phraseEnv == 1) {                            // SOSTENIDA — mantiene nivel toda la nota (sustain real)
    atkSec = 0.03f + rndF() * 0.12f;
    decSec = 1.0f;                                 // (no se usa: hold hasta la próxima nota)
    afterAtk = 1;
  } else {                                         // PLUCK/legato — decae gradual a lo largo de la nota
    atkSec = 0.010f + rndF() * 0.05f;
    decSec = durSec * (0.75f + rndF() * 0.60f);
    afterAtk = 3;
  }
  if (decSec < 0.15f) decSec = 0.15f;
  if (atkSec > durSec * 0.5f) atkSec = durSec * 0.5f;
  float atkInc  = 1.0f / (atkSec * SAMPLE_RATE);
  float decCoef = expf(-6.5f / (decSec * SAMPLE_RATE));

  // VIBRATO de ESTA nota: siempre presente, con rate aleatorio y DINÁMICO (empieza lento, acelera)
  melVibDepth = 0.006f + rndF() * 0.012f;         // ~10–31 cents
  melVibR0    = 3.5f + rndF() * 1.5f;             // Hz inicial
  melVibR1    = melVibR0 + 1.5f + rndF() * 2.5f;  // Hz final (más rápido)

  // DINÁMICA: velocidad variada + acento en notas largas; más fuerte → un poco más brillante (voz)
  float vel = 0.5f + rndF() * 0.5f;               // 0.5–1.0
  if (units >= 8) vel = 0.72f + rndF() * 0.38f;   // notas largas → más presencia
  // Agudos MÁS SUAVES y apagados (menos molestos) por encima de SOFT_FROM
  float hiSoft = 1.0f;
  if (semi > SOFT_FROM) { hiSoft = 1.0f - (float)(semi - SOFT_FROM) * 0.055f; if (hiSoft < 0.35f) hiSoft = 0.35f; }
  float g   = vel * melGain * hiSoft;
  float lpN = phraseLpA * (0.6f + 0.4f * vel);
  if (semi > SOFT_FROM) lpN *= 0.8f;              // agudos también más oscuros
  if (lpN > 0.9f) lpN = 0.9f;
  float pan = (rndF() * 2.0f - 1.0f) * 0.55f;

  melVoiceIdx = spawnVoice(semi, g, pan, KIND_MEL, LYR_CORE, atkInc, decCoef, melWave, lpN, afterAtk);
  if (phraseSub)                                   // capa 8ª abajo (cuerpo); no participa del legato
    spawnVoice(semi - 12, g * 0.5f, -pan, KIND_MEL, LYR_CORE, atkInc, decCoef, melWave, lpN, afterAtk);

  g_melFlash = 1.0f;
  g_melPos   = (g_melPos + 1) % NUM_LEDS;
}

// ==============================================================================================================================================
// GENERACIÓN DE UNA CANCIÓN NUEVA
// ==============================================================================================================================================
void startSong() {
  // Semilla fresca: mezcla el estado previo con el reloj real
  rng ^= (uint32_t)micros() + voiceCounter * 2654435761u;
  rndU(); rndU();

  // — Tonalidad + modo (los 7 modos; la progresión se adapta a cada uno) —
  keyRoot = rndI(12);
  int mode = rndI(NUM_MODES);
  for (int i = 0; i < 7; i++) scaleInt[i] = MODES[mode][i];
  for (int i = 0; i < N_SCALE; i++) scaleSemis[i] = keyRoot + degSemi(i);

  // — ARQUETIPO: fija el CARÁCTER global → cada canción suena categóricamente distinta —
  songArche = rndI(5);
  for (int i = 0; i < 4; i++) stylePool[i]   = ARCHE_STYLES[songArche][i];
  for (int i = 0; i < 4; i++) melWavePool[i] = ARCHE_WAVES[songArche][i];
  for (int i = 0; i < 3; i++) envPool[i]     = ARCHE_ENVS[songArche][i];
  stylePoolN = 4; melWavePoolN = 4; envPoolN = 3;

  float padAtk, padRel;
  switch (songArche) {
    case 0:  // AMBIENT — lento, pads sostenidos, notas largas, oscuro
      songBPM = 46 + rndI(22);
      padVoicing = (rndF() < 0.5f) ? 3 : 0;   padLpA = 0.15f + rndF() * 0.25f;
      cutoffBase = 300.0f + rndF() * 600.0f;  qBase = 0.8f + rndF() * 1.4f;
      filtLfoRate = 0.03f + rndF() * 0.10f;   filtLfoDepth = 500.0f + rndF() * 1600.0f;
      lpMin = 0.06f; lpMax = 0.40f;
      detuneCents = 8.0f + rndF() * 18.0f;    panWidth = 0.5f + rndF() * 0.4f;
      padAtk = 0.4f + rndF() * 1.2f;          padRel = 1.5f + rndF() * 3.0f;
      subChance = 0.30f; melGain = 0.70f;     toneCoef = 0.30f + rndF() * 0.35f;
      break;
    case 1:  // CINEMATIC — swells, lush, barrido de filtro amplio
      songBPM = 54 + rndI(26);
      padVoicing = (rndF() < 0.5f) ? 2 : 1;   padLpA = 0.30f + rndF() * 0.40f;
      cutoffBase = 500.0f + rndF() * 1200.0f; qBase = 1.0f + rndF() * 2.0f;
      filtLfoRate = 0.04f + rndF() * 0.12f;   filtLfoDepth = 1500.0f + rndF() * 3000.0f;
      lpMin = 0.20f; lpMax = 0.80f;
      detuneCents = 12.0f + rndF() * 22.0f;   panWidth = 0.5f + rndF() * 0.45f;
      padAtk = 0.5f + rndF() * 1.5f;          padRel = 2.0f + rndF() * 3.0f;
      subChance = 0.40f; melGain = 0.80f;     toneCoef = 0.40f + rndF() * 0.45f;
      break;
    case 2:  // PULSE — pad RÍTMICO (gate), medio-rápido
      songBPM = 82 + rndI(28);
      padVoicing = (rndF() < 0.5f) ? 0 : 2;   padLpA = 0.40f + rndF() * 0.50f;
      cutoffBase = 500.0f + rndF() * 1500.0f; qBase = 1.2f + rndF() * 1.6f;
      filtLfoRate = 0.10f + rndF() * 0.40f;   filtLfoDepth = 1500.0f + rndF() * 2500.0f;
      lpMin = 0.35f; lpMax = 0.80f;
      detuneCents = 6.0f + rndF() * 16.0f;    panWidth = 0.35f + rndF() * 0.4f;
      padAtk = 0.02f + rndF() * 0.15f;        padRel = 0.6f + rndF() * 1.5f;
      subChance = 0.20f; melGain = 0.85f;     toneCoef = 0.50f + rndF() * 0.40f;
      break;
    case 3:  // PLUCK — melodía corta/plucky, pad suave, brillante
      songBPM = 74 + rndI(30);
      padVoicing = (rndF() < 0.5f) ? 0 : 3;   padLpA = 0.50f + rndF() * 0.45f;
      cutoffBase = 800.0f + rndF() * 2400.0f; qBase = 1.0f + rndF() * 1.5f;
      filtLfoRate = 0.08f + rndF() * 0.30f;   filtLfoDepth = 1000.0f + rndF() * 2500.0f;
      lpMin = 0.50f; lpMax = 0.85f;
      detuneCents = 5.0f + rndF() * 14.0f;    panWidth = 0.4f + rndF() * 0.4f;
      padAtk = 0.1f + rndF() * 0.6f;          padRel = 0.8f + rndF() * 2.0f;
      subChance = 0.15f; melGain = 0.85f;     toneCoef = 0.55f + rndF() * 0.40f;
      break;
    default: // 4 DRIVE — rápido, stabs rítmicos, resonante
      songBPM = 100 + rndI(28);
      padVoicing = (rndF() < 0.5f) ? 2 : 0;   padLpA = 0.50f + rndF() * 0.45f;
      cutoffBase = 600.0f + rndF() * 2000.0f; qBase = 1.5f + rndF() * 1.8f;
      filtLfoRate = 0.15f + rndF() * 0.60f;   filtLfoDepth = 2000.0f + rndF() * 3000.0f;
      lpMin = 0.50f; lpMax = 0.85f;
      detuneCents = 6.0f + rndF() * 14.0f;    panWidth = 0.35f + rndF() * 0.45f;
      padAtk = 0.02f + rndF() * 0.10f;        padRel = 0.4f + rndF() * 1.2f;
      subChance = 0.20f; melGain = 0.90f;     toneCoef = 0.50f + rndF() * 0.45f;
      break;
  }

  // — Clock (depende del BPM) —
  beatSamples = (uint32_t)((uint64_t)SAMPLE_RATE * 60 / songBPM);
  gridSamples = beatSamples / 4;                  // semicorchea

  // — Gate rítmico del pad (solo arquetipos rítmicos) —
  if (songArche == 2) {                           // PULSE → blanca o negra
    padGateSamples = (rndF() < 0.5f) ? (beatSamples * 2) : beatSamples;
    padGateDepth   = 0.40f + rndF() * 0.35f;
  } else if (songArche == 4) {                    // DRIVE → negra o corchea
    padGateSamples = (rndF() < 0.5f) ? beatSamples : (beatSamples / 2);
    padGateDepth   = 0.50f + rndF() * 0.40f;
  } else {
    padGateSamples = 0; padGateDepth = 0.0f;      // sostenido
  }
  padGateAcc = 0;

  // — Onda / nivel del pad y la melodía (melodía MÁS presente: es la voz principal) —
  padWave  = melWavePool[rndI(4)];                // el pad toma una onda del arquetipo
  melWave  = MEL_WAVES[rndI(4)];                  // onda VOCAL de la MELODÍA: FIJA toda la canción
  subOsc   = (rndF() < 0.85f);
  padLevel = 0.50f + rndF() * 0.15f;              // pad de fondo (más bajo → no tapa la melodía)
  melLevel = 1.25f + rndF() * 0.35f;              // melodía por delante

  padAtkInc      = 1.0f / (padAtk * SAMPLE_RATE);
  padReleaseCoef = expf(-6.5f / (padRel * SAMPLE_RATE));
  filtLfoPhase   = 0.0f;

  // Release SUAVE de la melodía (caída al terminar cada nota): 200–420 ms → nunca corta en seco,
  // las notas se apagan de a poco y se solapan (legato vocal)
  float melRel = 0.20f + rndF() * 0.22f;
  melRelCoef = expf(-6.5f / (melRel * SAMPLE_RATE));

  // — Progresión MODAL: cada modo usa SU vocabulario de acordes y SU cadencia —
  //   Ancla en la tónica modal, camina por el pool del modo (sin acordes rompe-modo ni
  //   disminuidos), y cierra con la cadencia característica que resuelve a la tónica al loopear.
  const ModeHarm &mh = MODE_HARM[mode];
  progLen = 4 + rndI(5);                            // 4–8 acordes
  int prevDeg = -1;
  for (int i = 0; i < progLen; i++) {
    int d;
    if      (i == 0)             d = 0;             // tónica modal
    else if (i == progLen - 1)   d = mh.cadence;    // cadencia modal
    else { do { d = mh.degs[rndI(mh.nDeg)]; } while (d == prevDeg); }
    progDeg[i]   = d; prevDeg = d;
    progBeats[i] = (rndI(3) == 0) ? 8 : 4;          // 4 u 8 negras
  }

  // — Registro de la melodía: ventana en el RANGO VOCAL (grados 7..~16 → ~C4–D5) —
  //   (el plegado por-nota a [VOCAL_LO, VOCAL_HI] garantiza que no se salga aunque suba de octava)
  melLo  = 7;
  melHi  = 14 + rndI(3);
  if (melHi > N_SCALE - 2) melHi = N_SCALE - 2;
  melIdx = (melLo + melHi) / 2;
  melPrevIdx = -999; melRepeat = 0;               // reinicia el anti-repetición de la melodía
  phraseLeft = 0;                                  // fuerza newPhrase() en la primera nota

  // — Color base según tonalidad —
  g_keyHue = (uint8_t)(keyRoot * 21 + 20);

  // — Reset de contadores y arranque —
  progIdx = 0;
  g_curDeg = progDeg[0];
  chordAcc = 0;
  chordSamples = (uint32_t)progBeats[0] * beatSamples;
  melSamplesToNext = 0;                            // dispara la primera nota enseguida
  melVoiceIdx = -1; lastWasPassing = false;
  g_startFlash = 1.0f;

  triggerPadChord(progDeg[0]);
  playing = true;
}

void stopSong() {
  releaseAllPad();
  for (int i = 0; i < NUM_VOICES; i++)             // melodía → cola rápida
    if (voices[i].active && voices[i].kind == KIND_MEL && voices[i].stage != 3) {
      voices[i].stage = 3;
      voices[i].decCoef = expf(-6.5f / (0.12f * SAMPLE_RATE));
    }
  melVoiceIdx = -1;
  playing = false;
}

// ─── PolyBLEP ──────────────────────────────────────────────
inline float polyBlep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  else if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

// wave: 0 = seno · 1 = sierra · 2 = cuadrada · 3 = triangular
inline float osc(float phase, float dt, uint8_t wave) {
  if (wave == 0) {
    return oscSine(phase);
  } else if (wave == 1) {
    return (2.0f * phase - 1.0f) - polyBlep(phase, dt);
  } else if (wave == 2) {
    float saw1 = (2.0f * phase - 1.0f) - polyBlep(phase, dt);
    float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
    float saw2 = (2.0f * p2 - 1.0f) - polyBlep(p2, dt);
    return (saw1 - saw2) * 0.6f;
  } else {
    return (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
  }
}

// ─── Filtro ────────────────────────────────────────────────
void computeFilter(BiqCoef &co, float cutoff, float Q) {
  if (cutoff < 80.0f)    cutoff = 80.0f;
  if (cutoff > 12000.0f) cutoff = 12000.0f;
  if (Q < 0.5f) Q = 0.5f; if (Q > 18.0f) Q = 18.0f;

  float omega = 2.0f * (float)M_PI * cutoff / SAMPLE_RATE;
  float s = sinf(omega), c = cosf(omega);
  float alpha = s / (2.0f * Q);
  float b0 = (1.0f - c) * 0.5f, b1 = 1.0f - c, b2 = (1.0f - c) * 0.5f;
  float a0 = 1.0f + alpha, a1 = -2.0f * c, a2 = 1.0f - alpha;
  co.b0 = b0 / a0; co.b1 = b1 / a0; co.b2 = b2 / a0;
  co.a1 = a1 / a0; co.a2 = a2 / a0;
}
// Paso de interpolación: cuánto avanzan los coeficientes ACTUALES hacia los OBJETIVO por muestra.
// Recalcular el biquad solo 1 vez por buffer hace que el corte "salte" cada 128 muestras y eso, con
// modulación rápida + resonancia, se oye como chasquidos. Interpolando muestra a muestra el barrido
// es continuo (sin escalón) → sin clicks. (5 sumas/canal por muestra: barato.)
inline void filterStep(BiqCoef &c, const BiqCoef &step) {
  c.b0 += step.b0; c.b1 += step.b1; c.b2 += step.b2; c.a1 += step.a1; c.a2 += step.a2;
}
inline void filterDelta(const BiqCoef &cur, const BiqCoef &target, BiqCoef &step) {
  const float inv = 1.0f / (float)BUFFER_SAMPLES;
  step.b0 = (target.b0 - cur.b0) * inv; step.b1 = (target.b1 - cur.b1) * inv;
  step.b2 = (target.b2 - cur.b2) * inv; step.a1 = (target.a1 - cur.a1) * inv;
  step.a2 = (target.a2 - cur.a2) * inv;
}
inline float applyFilter(BiqState &st, const BiqCoef &co, float in) {
  float out = co.b0 * in + co.b1 * st.x1 + co.b2 * st.x2 - co.a1 * st.y1 - co.a2 * st.y2;
  // ANTI-BLOWUP: si el barrido de filtro es muy dinámico + resonante, un salto grande de cutoff
  // puede hacer que el biquad "dispare" un transitorio enorme o llegue a NaN/Inf → cuelga el audio.
  // Aquí lo acotamos: si el estado deja de ser finito lo reseteamos, y limitamos la magnitud.
  if (!isfinite(out)) { st.x1 = st.x2 = st.y1 = st.y2 = 0.0f; return 0.0f; }
  if (out >  8.0f) out =  8.0f; else if (out < -8.0f) out = -8.0f;
  st.x2 = st.x1; st.x1 = in;
  st.y2 = st.y1; st.y1 = out;
  return out;
}

// ─── Visualizador de 6 LEDs ────────────────────────────────
void renderLEDs() {
  unsigned long t = millis();
  static unsigned long lastFrame = 0;
  if (t - lastFrame < LED_REFRESH_MS) return;
  lastFrame = t;

  // Color = tonalidad + acorde + LFO de filtro
  uint8_t hue = g_keyHue + (uint8_t)(g_curDeg * 9) + (uint8_t)(g_filtLfoVal * 24.0f);

  if (!playing && g_energy < 0.02f) {
    // Modo espera → respiración lenta y tenue
    static uint8_t breath = 0; static int8_t bdir = 1;
    breath += bdir * 3;
    if (breath >= 55) bdir = -1;
    if (breath <= 4)  bdir =  1;
    fill_solid(leds, NUM_LEDS, CHSV(hue, 210, breath));
  } else {
    // Barra VU = energía del pad
    float lvl = g_energy * 1.6f; if (lvl > 1.0f) lvl = 1.0f;
    float litf = lvl * NUM_LEDS;
    for (int i = 0; i < NUM_LEDS; i++) {
      float on = litf - i;
      if (on < 0.0f) on = 0.0f; if (on > 1.0f) on = 1.0f;
      uint8_t v = 22 + (uint8_t)(on * 200.0f);
      leds[i] = CHSV(hue + i * 6, 255, v);
    }
    // Melodía → punto que corre (color de contraste)
    if (g_melFlash > 0.02f) {
      uint8_t mv = (uint8_t)(g_melFlash * 255.0f);
      leds[g_melPos % NUM_LEDS] += CHSV(hue + 128, 235, mv);
    }
  }
  g_melFlash *= 0.6f;

  // Flash al cambiar de acorde (suma en el color complementario suave)
  if (g_chordFlash > 0.02f) {
    uint8_t cv = (uint8_t)(g_chordFlash * 90.0f);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] += CHSV(hue + 64, 180, cv);
    g_chordFlash *= 0.7f;
  }
  // Flash blanco al arrancar canción nueva
  if (g_startFlash > 0.02f) {
    uint8_t w = (uint8_t)(g_startFlash * 170.0f);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] += CRGB(w, w, w);
    g_startFlash *= 0.62f;
  }

  // LED RGB del módulo = promedio de la tira, más tenue
  uint16_t sr = 0, sg = 0, sb = 0;
  for (int i = 0; i < NUM_LEDS; i++) { sr += leds[i].r; sg += leds[i].g; sb += leds[i].b; }
  onboard[0] = CRGB(sr / NUM_LEDS, sg / NUM_LEDS, sb / NUM_LEDS);
  onboard[0].nscale8(ONBOARD_BRIGHT);

  FastLED.show();
}

// ─── Setup I2S ─────────────────────────────────────────────
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  chan_cfg.dma_desc_num  = 10;
  chan_cfg.dma_frame_num = 240;
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

// Motivo del último reset → texto legible (para diagnosticar cuelgues)
const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "POWERON (encendido normal)";
    case ESP_RST_EXT:      return "EXT (reset externo)";
    case ESP_RST_SW:       return "SW (reinicio por software)";
    case ESP_RST_PANIC:    return "PANIC (excepcion/crash de codigo) <-- revisar backtrace";
    case ESP_RST_INT_WDT:  return "INT_WDT (watchdog de interrupciones) <-- codigo";
    case ESP_RST_TASK_WDT: return "TASK_WDT (watchdog de tarea) <-- codigo bloqueo el loop";
    case ESP_RST_WDT:      return "WDT (otro watchdog) <-- codigo";
    case ESP_RST_DEEPSLEEP:return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT (caida de tension) <-- ALIMENTACION, no es el codigo";
    case ESP_RST_SDIO:     return "SDIO";
    default:               return "DESCONOCIDO";
  }
}

void setup() {
  // — Diagnóstico de cuelgues —
  // Imprime por USB-CDC el motivo del ÚLTIMO reset. Tras un cuelgue, al reiniciar aquí sale la causa
  // (PANIC/WDT = código · BROWNOUT = alimentación). El panic-handler de la ESP-IDF además escribe el
  // backtrace por debajo de esto (usar el "ESP Exception Decoder" del IDE). Solo corre una vez → no
  // afecta el audio del loop.
  Serial.begin(115200);
  delay(300);
  esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("\n[BOOT] cancion_aleatoria_leds — motivo del ultimo reset: %d = %s\n",
                (int)rr, resetReasonStr(rr));

  esp_log_level_set("*", ESP_LOG_NONE);

  pinMode(BTN1_PIN, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i < SEMI_LUT_N; i++)
    semiLUT[i] = powf(2.0f, (float)(i - SEMI_OFFSET) / 12.0f);
  for (int i = 0; i < 256; i++)
    sineLUT[i] = sinf(2.0f * (float)M_PI * (float)i / 256.0f);
  for (int i = 0; i < NUM_VOICES; i++) voices[i].active = false;

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, ONBOARD_PIN, COLOR_ORDER>(onboard, 1);
  FastLED.setBrightness(LED_BRIGHT);
  FastLED.clear();
  FastLED.show();

  computeFilter(coefL, cutoffBase, qBase);
  computeFilter(coefR, cutoffBase, qBase);
  i2s_init();
}

void loop() {
  // — BTN1: PLAY / STOP —
  if (buttonPressed(bPlay)) {
    if (playing) stopSong();
    else         startSong();
  }

  // — POT1: volumen de los PADS · POT2: volumen de la MELODÍA (mezcla) —
  static uint8_t potDiv = 0;
  if (++potDiv >= 4) {
    potDiv = 0;
    float vp = readPot(POT1); g_padVol = vp * vp;   // curva cuadrática (más control abajo)
    float vm = readPot(POT2); g_melVol = vm * vm;
  }

  // — Progresión de acordes (avanza por tiempo) —
  if (playing) {
    chordAcc += BUFFER_SAMPLES;
    if (chordAcc >= chordSamples) { chordAcc = 0; nextChord(); }
  }

  // — Gate rítmico del pad (sincronizado al clock; pulso SUAVE y CONTINUO → sin clics) —
  //   Coseno con pico en el beat (ph=0) y valle en el contratiempo; periódico → sin salto.
  if (playing && padGateSamples > 0) {
    padGateAcc += BUFFER_SAMPLES;
    while (padGateAcc >= padGateSamples) padGateAcc -= padGateSamples;
    float ph = (float)padGateAcc / (float)padGateSamples;
    float shape = 0.5f + 0.5f * cosf(2.0f * (float)M_PI * ph);   // 1 en el beat, 0 entre beats
    g_padGate = (1.0f - padGateDepth) + padGateDepth * shape;
  } else {
    g_padGate = 1.0f;
  }

  // — LFO de filtro (autónomo) — DOS fases desfasadas 90° → barrido distinto en L y R (estéreo)
  filtLfoPhase += filtLfoRate * (float)BUFFER_SAMPLES / SAMPLE_RATE;
  if (filtLfoPhase >= 1.0f) filtLfoPhase -= 1.0f;
  float lfoL = sinf(2.0f * (float)M_PI * filtLfoPhase);
  float phR  = filtLfoPhase + 0.25f; if (phR >= 1.0f) phR -= 1.0f;   // +90° en el canal derecho
  float lfoR = sinf(2.0f * (float)M_PI * phR);
  g_filtLfoVal = lfoL;                                               // los LEDs siguen el canal L
  float cutoffL = cutoffBase + (lfoL * 0.5f + 0.5f) * filtLfoDepth;
  float cutoffR = cutoffBase + (lfoR * 0.5f + 0.5f) * filtLfoDepth;
  // Coeficientes OBJETIVO de este buffer; se interpolan muestra a muestra (no saltan → sin chasquidos)
  BiqCoef targetL, targetR, stepL, stepR;
  computeFilter(targetL, cutoffL, qBase);
  computeFilter(targetR, cutoffR, qBase);
  filterDelta(coefL, targetL, stepL);
  filterDelta(coefR, targetR, stepR);

  // — Energía del PAD para los LEDs —
  float vsum = 0.0f;
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind == KIND_PAD) vsum += voices[i].env;
  vsum *= 0.16f;
  if (vsum > g_energy) g_energy = vsum;
  else                 g_energy = g_energy * 0.90f + vsum * 0.10f;

  // — Generar buffer de audio (estéreo) —
  int16_t buffer[BUFFER_SAMPLES * 2];

  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    // Secuenciador de MELODÍA: cuenta muestras hasta el próximo evento
    if (playing) {
      if (melSamplesToNext <= 0) melodyStep();
      melSamplesToNext--;
    }

    float padL = 0.0f, padR = 0.0f, melL = 0.0f, melR = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;

      float dt;
      if (v.kind == KIND_MEL) {                   // melodía: portamento (glide) + vibrato dinámico
        v.freq += (v.freqTarget - v.freq) * v.glideCoef;
        if (v.vibRamp < 1.0f) { v.vibRamp += 0.00008f; if (v.vibRamp > 1.0f) v.vibRamp = 1.0f; }
        float vrate = melVibR0 + (melVibR1 - melVibR0) * v.vibRamp;   // rate DINÁMICO (acelera)
        v.vibPhase += vrate / SAMPLE_RATE;
        if (v.vibPhase >= 1.0f) v.vibPhase -= 1.0f;
        float vib = 1.0f + melVibDepth * v.vibRamp * oscSine(v.vibPhase);  // onset suave del vibrato
        dt = v.freq * vib / SAMPLE_RATE;
      } else {
        dt = v.freq / SAMPLE_RATE;
      }
      v.phase += dt;
      if (v.phase >= 1.0f) v.phase -= 1.0f;

      float wave = osc(v.phase, dt, v.wave);
      v.lp += v.lpA * (wave - v.lp);              // LPF de brillo por voz (timbre por frase)
      wave = v.lp;

      if (v.stage == 0) {                         // attack
        v.env += v.atkInc;
        if (v.env >= 1.0f) { v.env = 1.0f; v.stage = v.afterAtk; }
      } else if (v.stage == 2) {                  // release del pad
        v.env *= padReleaseCoef;
        if (v.env < 0.0008f) { v.active = false; v.env = 0.0f; continue; }
      } else if (v.stage == 3) {                  // decay de la melodía
        v.env *= v.decCoef;
        if (v.env < 0.0008f) { v.active = false; v.env = 0.0f; continue; }
      }                                           // stage 1 = sustain del pad

      float a = wave * v.env * v.gain;
      if (v.kind == KIND_PAD) { padL += a * v.lGain; padR += a * v.rGain; }
      else                    { melL += a * v.lGain; melR += a * v.rGain; }
    }

    // Interpolación de coeficientes del filtro (muestra a muestra) → el barrido de corte es continuo
    filterStep(coefL, stepL);
    filterStep(coefR, stepR);

    // El filtro resonante + "tono" van SOLO al PAD (la melodía va SECA por encima → siempre corta)
    // POT1 = volumen del pad · el barrido de filtro es distinto en L/R → estéreo del pad
    float padMixL = padL * padLevel * g_padGate * g_padVol + 1.0e-18f;   // (+ anti-denormal)
    float padMixR = padR * padLevel * g_padGate * g_padVol - 1.0e-18f;
    float fL = applyFilter(bqL, coefL, padMixL);
    float fR = applyFilter(bqR, coefR, padMixR);
    toneL += toneCoef * (fL - toneL);             // "tono": one-pole LPF (solo oscurece el pad)
    toneR += toneCoef * (fR - toneR);

    // Melodía SECA (solo su LPF de brillo por voz) sumada encima del pad filtrado · POT2 = su volumen
    float vL = (toneL + melL * melLevel * g_melVol) * 0.12f;
    float vR = (toneR + melR * melLevel * g_melVol) * 0.12f;

    if (vL >  3.0f) vL =  3.0f; if (vL < -3.0f) vL = -3.0f;
    if (vR >  3.0f) vR =  3.0f; if (vR < -3.0f) vR = -3.0f;
    float shL = vL * (27.0f + vL * vL) / (27.0f + 9.0f * vL * vL);
    float shR = vR * (27.0f + vR * vR) / (27.0f + 9.0f * vR * vR);

    buffer[n * 2]     = (int16_t)(shL * 30000.0f);
    buffer[n * 2 + 1] = (int16_t)(shR * 30000.0f);
  }

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);

  renderLEDs();
}
