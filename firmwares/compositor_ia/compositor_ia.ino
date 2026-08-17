// ==============================================================================================================================================
// PERCUSYNTH - COMPOSITOR IA v2 (voz -> Whisper -> GPT -> JSON de cancion -> sintesis) - GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo Sandoval - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
//
// Fusion de dos firmwares del proyecto:
//   · asistente_ia ............ cadena de voz (mic INMP441 -> Whisper -> GPT por WiFi)
//   · cancion_aleatoria_leds .. motor de audio (voces estereo + percusion sintetizada + 6 LEDs)
// GPT responde un JSON COMPACTO con los parametros de una cancion y el PercuSynth la toca.
//
// v2 — para que los ESTILOS suenen realmente distintos (como tools/generador_estilos):
//   · BAJO con patrones idiomaticos: walking (blues/jazz), contratiempo (techno),
//     octavas (synthwave), riff (rock/grunge), fundamental-quinta (ambiente).
//   · COMPING por golpes ademas del pad sostenido: golpes en 2 y 4 (blues), stabs
//     sincopados (techno), power chords en corcheas (grunge).
//   · 5 GRAMATICAS de melodia: frases generativas (la de cancion_aleatoria), ARPEGIO
//     en semicorcheas, HOOK repetido con mutacion, RIFF transpuesto al acorde, LIRICA
//     de notas largas — cada estilo canta distinto, no siempre igual.
//   · FORMA con SECCIONES: intro -> verso -> coro/drop... cada seccion define que capas
//     suenan, su intensidad y su modo de melodia. La progresion se reinicia por seccion.
//   · SWING real en el grid (pasos pares largos / impares cortos).
// ==============================================================================================================================================
// HARDWARE
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3 (PercuSynth). Se recomienda modulo CON PSRAM.
//
// - DAC PCM5102 por I2S  ->  I2S_NUM_0 (SALIDA, 44.1 kHz / 16 bit / estereo):
//       I2S LCK / LRCK ... GPIO 39
//       I2S DIN / DATA ... GPIO 40
//       I2S BCK / BCLK ... GPIO 41
//
// - Microfono INMP441 por I2S  ->  I2S_NUM_1 (ENTRADA, 16 kHz mono):
//       WS  (LRCL) ....... GPIO 11
//       SCK (BCLK) ....... GPIO 12
//       SD  (DOUT) ....... GPIO 13    (L/R a GND, VDD a 3.3V)
//
// - BTN1 (GPIO 44) ..... MANTENER = grabar pedido de voz (max 5 s)
// - BTN2 (GPIO 42) ..... PLAY / STOP de la ultima cancion
// - POT1 (ADC 1) ....... volumen PADS + BAJO
// - POT2 (ADC 2) ....... volumen MELODIA
// - POT3 (ADC 8) ....... volumen PERCUSION
// - 6 LEDs SMD WS2812 on-board (GPIO 46) + LED RGB del modulo (GPIO 48)
// ==============================================================================================================================================
// ARDUINO IDE SETTINGS
// ==============================================================================================================================================
// - Placa:           ESP32S3 Dev Module
// - Flash Mode:      DIO            (IMPORTANTE en este hardware para que el I2S funcione bien)
// - PSRAM:           OPI PSRAM      (habilitar: el buffer de grabacion son ~160 KB)
// - USB CDC On Boot: Enabled       (para pegar JSON por el Monitor Serie)
// - Upload/Monitor:  115200 baud
// ==============================================================================================================================================
// LIBRERIAS REQUERIDAS
// ==============================================================================================================================================
// - WiFi.h / WiFiClientSecure.h / HTTPClient.h   (core ESP32 Arduino)
// - driver/i2s_std.h                             (core ESP32 Arduino, nuevo driver I2S)
// - FastLED
// ==============================================================================================================================================
// FUNCIONAMIENTO (LEDs de estado)
// ==============================================================================================================================================
//   Tocando       -> visualizador (VU del pad + punto de melodia + pulso del bombo)
//   LED ROJO      -> GRABANDO (BTN1 presionado)
//   LED AMBAR     -> PROCESANDO (Whisper / GPT)
//   FLASH BLANCO  -> cancion nueva recibida y sonando
//   LED MAGENTA   -> error (sin WiFi / JSON invalido) — vuelve a lo anterior
//   Sin WiFi tambien funciona: pega un JSON (una linea) en el Monitor Serie y Enter.
// ==============================================================================================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <math.h>

// ==================== CONFIGURACION USUARIO ====================

// --- Credenciales (WiFi + OpenAI) ---
// No viven en este archivo. Copia secretos.example.h a secretos.h (misma carpeta del
// sketch) y escribe ahi tus claves. secretos.h esta en .gitignore: nunca se sube al repo.
#if !__has_include("secretos.h")
  #error "Falta secretos.h: copia secretos.example.h a secretos.h y pon tus claves."
#endif
#include "secretos.h"

// ==================== PINES ====================

#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41

#define MIC_WS    11
#define MIC_SCK   12
#define MIC_SD    13

#define BTN_RECORD 44
#define BTN_PLAY   42
#define POT_PAD     1
#define POT_MEL     2
#define POT_PERC    8

#define LED_PIN        46
#define NUM_LEDS        6
#define LED_BRIGHT     150
#define ONBOARD_PIN    48
#define ONBOARD_BRIGHT 45
const unsigned long LED_REFRESH_MS = 22;

// ==================== AUDIO ====================

#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
#define MIC_RATE        16000
#define RECORD_SECONDS  5
#define MAX_SAMPLES     (MIC_RATE * RECORD_SECONDS)
#define MIC_GAIN        6

// ==================== TIPOS / ESTADO GENERAL ====================

struct BtnState { uint8_t pin; bool last; unsigned long lastPress; };
struct BiqState { float x1, x2, y1, y2; };
struct BiqCoef  { float b0, b1, b2, a1, a2; };

const unsigned long DEBOUNCE_MS = 220;
BtnState bPlay = {BTN_PLAY, HIGH, 0};

CRGB leds[NUM_LEDS];
CRGB onboard[1];

enum UiState { UI_NORMAL, UI_REC, UI_PROC };
UiState uiState = UI_NORMAL;
bool wifiOK = false;

float g_energy = 0.0f, g_melFlash = 0.0f, g_chordFlash = 0.0f, g_startFlash = 0.0f;
float g_kickFlash = 0.0f, g_filtLfoVal = 0.0f;
int   g_melPos = 0, g_curDeg = 0;
uint8_t g_keyHue = 140;

// ==================== POLIFONIA ====================

#define NUM_VOICES   32
#define KIND_PAD  0
#define KIND_MEL  1
#define KIND_BASS 2
#define LYR_CORE 0
#define LYR_SUB  1

struct Voice {
  bool     active;
  uint8_t  kind, layer;
  float    freq, phase, env;
  uint8_t  stage;
  float    gain, lGain, rGain;
  float    atkInc, decCoef;
  uint8_t  wave;
  float    lp, lpA;
  uint8_t  afterAtk;
  float    freqTarget, glideCoef, vibRamp, vibPhase;
  uint32_t age;
};
Voice voices[NUM_VOICES];
uint32_t voiceCounter = 0;

const float BASE_FREQ = 130.81f;   // C3
#define VOCAL_LO    0
#define VOCAL_HI   27
#define SOFT_FROM  20

#define SEMI_OFFSET 72
#define SEMI_LUT_N  145
float semiLUT[SEMI_LUT_N];
inline float semiToFreq(int semi) {
  int idx = semi + SEMI_OFFSET;
  if (idx < 0) idx = 0; else if (idx >= SEMI_LUT_N) idx = SEMI_LUT_N - 1;
  return BASE_FREQ * semiLUT[idx];
}

float sineLUT[256];
inline float oscSine(float phase) {
  float f = phase * 256.0f;
  int i0 = (int)f; float frac = f - (float)i0;
  i0 &= 255; int i1 = (i0 + 1) & 255;
  return sineLUT[i0] + (sineLUT[i1] - sineLUT[i0]) * frac;
}

uint32_t rng = 0xC0FFEE11u;
inline uint32_t rndU()  { rng = rng * 1664525u + 1013904223u; return rng; }
inline float    rndF()  { return (float)(rndU() >> 8) * (1.0f / 16777216.0f); }
inline int      rndI(int n) { return (n <= 0) ? 0 : (int)((rndU() >> 9) % (uint32_t)n); }

// ==================== ARMONIA ====================

#define NUM_MODES 7
const int8_t MODES[NUM_MODES][7] = {
  { 0, 2, 4, 5, 7, 9, 11 },   // 0 Jonico (mayor)
  { 0, 2, 3, 5, 7, 9, 10 },   // 1 Dorico
  { 0, 1, 3, 5, 7, 8, 10 },   // 2 Frigio
  { 0, 2, 4, 6, 7, 9, 11 },   // 3 Lidio
  { 0, 2, 4, 5, 7, 9, 10 },   // 4 Mixolidio
  { 0, 2, 3, 5, 7, 8, 10 },   // 5 Eolico (menor natural)
  { 0, 2, 3, 5, 7, 8, 11 },   // 6 Menor armonica
};

#define N_SCALE 35
int scaleSemis[N_SCALE];
int8_t scaleInt[7];
int keyRoot = 0;
inline int degSemi(int d) { int oct = d / 7; int dd = d % 7; return scaleInt[dd] + 12 * oct; }

#define PROG_MAX 16
int  progDeg[PROG_MAX];
int  progBars[PROG_MAX];    // compases por acorde (beats/4 del JSON)
int  progLen  = 4;
int  progIdx  = 0;
int  curChordTones[3];

// ==================== CLOCK (grid unificado de 16 pasos por compas) ====================

int      songBPM = 80;
uint32_t beatSamples = 33075;
uint32_t gridSamples = 8268;
float    swingAmt = 0.5f;
int      gridStep = 0;
int32_t  gridToNext = 0;
bool     barArmed = false;      // la 1a vez que gridStep vuelve a 0 NO avanza compas
int      chordBarsLeft = 1;
int      percBar = 0;

// ==================== FORMA (secciones) ====================

#define SEC_MAX 10
#define LAY_BAT  1
#define LAY_BAJO 2
#define LAY_COMP 4
#define LAY_MEL  8
struct Section { uint8_t layers; uint8_t bars; float inten; int8_t melmode; };
Section secs[SEC_MAX];
int     secCount = 0, secIdx = 0, secBar = 0;
uint8_t layMask = 0xFF;
float   secInten = 0.8f;
int8_t  melModeSec = -1;        // override de gramatica por seccion (-1 = global)

// ==================== BAJO ====================

int     bassMode = 1;           // 0 sin · 1 fund-quinta · 2 walking · 3 contratiempo · 4 octavas · 5 riff
float   bassLevel = 0.8f;
uint8_t bassWave = 1;
int     bassVoiceIdx = -1;

// ==================== COMPING ====================

int compMode = 0;               // 0 pad sostenido · 1 golpes 2y4 · 2 stabs · 3 power corcheas

// ==================== MELODIA ====================

int   melMode = 0;              // gramatica global: 0 frases · 1 arpegio · 2 hook · 3 riff · 4 lirica
int   melIdx = 16, melLo = 7, melHi = 16;
int32_t melSamplesToNext = 0;
uint8_t melWave = 3;
int   phraseLeft = 0;
float melGain = 0.85f;
float melRelCoef = 0.9997f;
int   melPrevIdx = -999, melRepeat = 0;
#define MEL_NOTE_MAX_SEC 1.6f

#define NSTYLE 5
const int STYLE_DUR[NSTYLE][6] = {
  { 12,  8,  8, 12,  8,  6 },
  {  8,  6, 12,  6,  8,  8 },
  {  6,  8,  6,  4,  6, 12 },
  { 12,  8, 12,  8,  6, 12 },
  {  6,  4,  6,  8,  4,  6 },
};
const float STYLE_REST[NSTYLE] = { 0.12f, 0.12f, 0.12f, 0.26f, 0.10f };

uint8_t stylePool[4] = {1,1,2,0}; int stylePoolN = 4;
uint8_t envPool[3]   = {0,1,1};   int envPoolN   = 3;
float   subChance = 0.2f;
float   lpMin = 0.3f, lpMax = 0.7f;

#define STEP_N 12
const int STEP_SET[STEP_N] = { -5, -4, -3, -2, -1, -1, 1, 1, 2, 3, 4, 5 };

uint8_t  phraseStyle = 1, phraseEnv = 1;
int8_t   phraseOct   = 0;
bool     phraseSub   = false;
float    phraseLpA   = 0.7f;

int   melVoiceIdx    = -1;
float legatoChance   = 0.30f;
bool  lastWasPassing = false;
float melVibDepth = 0.010f, melVibR0 = 4.0f, melVibR1 = 6.5f;

// HOOK / RIFF de la cancion (motivo de 1 compas — se genera en cada Play, muta cada 4 compases)
uint16_t hookMask = 0;
int8_t   hookDeg[16];           // grado de escala por paso, relativo a la tonica
int      arpCount = 0;
float    melDensity = 0.7f;     // densidad del arpegio (de meldens)

// ==================== PERCUSION SINTETIZADA ====================

#define PERC_VOICES 6
#define PK_KICK  0
#define PK_SNARE 1
#define PK_HATC  2
#define PK_HATO  3
struct PercVoice {
  bool  active;
  float phase, freq, freqEnd, sweepCoef;
  float env, decCoef;
  float noiseMix;
  float lp, lpA;
  bool  hipass;
  float gain, lGain, rGain;
};
PercVoice pvoices[PERC_VOICES];

uint32_t noiseRng = 0x9E3779B9u;
inline float noiseSample() {
  noiseRng = noiseRng * 1664525u + 1013904223u;
  return (float)(int32_t)noiseRng * (1.0f / 2147483648.0f);
}

uint16_t patKick = 0, patSnare = 0, patHatC = 0, patHatO = 0;
bool    percOn = false;
float   percLevel = 0.5f, ghostProb = 0.0f;
float   g_percVol = 0.7f;

float kickF0 = 95.0f, kickF1 = 42.0f, kickDec = 0.22f;
float snFreq = 190.0f, snDec = 0.14f, snNoise = 0.8f;
float hatDecC = 0.045f, hatDecO = 0.20f, hatTone = 0.55f;

// ==================== TIMBRE / SINTESIS ====================

uint8_t padWave = 3;
bool    subOsc  = true;
float   detuneCents = 12.0f;
float   panWidth    = 0.54f;
float   toneCoef    = 0.55f;
float   toneL = 0.0f, toneR = 0.0f;
float   padLevel = 0.55f, melLevel = 1.2f;

uint8_t  padVoicing = 0;
float    padLpA = 0.5f;
float    padGateDepth = 0.0f;
uint32_t padGateSamples = 0, padGateAcc = 0;
float    g_padGate = 1.0f;

float cutoffBase = 700.0f, qBase = 1.4f;
float filtLfoPhase = 0.0f, filtLfoRate = 0.12f, filtLfoDepth = 2200.0f;
BiqCoef coefL = {0,0,0,0,0}, coefR = {0,0,0,0,0};
BiqState bqL = {0,0,0,0}, bqR = {0,0,0,0};

float g_padVol = 0.7f, g_melVol = 0.7f;
float padAtkInc = 0.0008f, padReleaseCoef = 0.99995f;

bool playing = false;
bool haveSpec = false;

static i2s_chan_handle_t tx_chan = NULL;
static i2s_chan_handle_t rx_chan = NULL;
int16_t* audioBuffer = nullptr;

// ==============================================================================================================================================
// PROMPT DE COMPOSITOR (en FLASH)
// ==============================================================================================================================================
const char COMPOSER_PROMPT[] PROGMEM = R"(Eres el compositor del PercuSynth. El usuario pide una cancion en lenguaje natural. Respondes UNICAMENTE un JSON valido en UNA sola linea, sin markdown ni texto extra, con estas claves:
estilo: texto corto. raiz: 0-11 (0=C,2=D,4=E,5=F,7=G,9=A,11=B). modo: 0 jonico, 1 dorico, 2 frigio, 3 lidio, 4 mixolidio, 5 eolico(menor), 6 menor armonica. bpm: 46-132. swing: 0.50 recto a 0.62 shuffle.
prog: grados 0-6 separados por coma, 4-12 acordes, SIEMPRE empieza en 0. Ej blues 12: "0,0,0,0,3,3,0,0,4,3,0,4". beats: negras por acorde, mismo largo ("4,4,...").
voicing: 0 triada, 1 +octava, 2 con septima, 3 abierto SIN tercera (=power chord). padwave: 0 seno,1 sierra,2 cuadrada,3 triangular. atk: 0.02-1.5 seg. rel: 0.4-4. cutoff: 300-2500. q: 0.8-3. lfor: 0.03-0.6 Hz. lfod: 500-4000. det: 5-30 cents. tone: 0.3-0.9. padlvl: 0.3-0.7.
gate: pad ritmico 0 sin,1 blanca,2 negra,3 corchea. gatedepth: 0-0.9.
comp: acompanamiento 0 pad sostenido, 1 golpes en 2 y 4 (blues/soul), 2 stabs sincopados (techno/house), 3 power chords en corcheas (rock/grunge, usar con voicing 3).
bajo: 0 sin, 1 fundamental-quinta (lento/ambiente), 2 WALKING (blues/jazz), 3 contratiempo (techno rolling), 4 octavas en corcheas (synthwave/disco), 5 riff con quinta y septima (rock/grunge/funk). bajowave: 0 seno,1 sierra,3 triangular. bajolvl: 0.4-1.0.
kick,snare,hatc,hato: strings de 16 caracteres 0/1 (semicorcheas). Ej four-on-floor "1000100010001000", backbeat "0000100000001000", hats contratiempo "0010001000100010". ghost: 0-0.2. plvl: 0-0.8 (0=sin percusion). kickdec: 0.1-0.5. sndec: 0.05-0.2. snoise: 0.3-0.9.
melmodo: GRAMATICA de la melodia — 0 frases cantadas generativas (blues/baladas), 1 ARPEGIO en semicorcheas (synthwave/ambiente), 2 HOOK corto repetido con mutacion (techno/electronica), 3 RIFF grave transpuesto al acorde (rock/grunge), 4 LIRICA notas largas con vibrato (coros/epico).
melwave: 0 seno,1 sierra,3 triangular (NUNCA 2). meldens: 0 escasa a 4 movida. melgain: 0.6-1.0. env: 0 pluck,1 sostenida,2 mixta. sub: 0/1.
forma: SECCIONES separadas por coma, cada una "capas/compases/intensidad/gramatica". Capas = letras: b bateria, j bajo, c acordes, m melodia, t todas. Gramatica opcional por seccion: q frases, a arpegio, h hook, r riff, l lirica, s solo denso. Ej: "c/4/0.3, bjm/8/0.5/r, t/8/0.9/l, bjm/8/0.5/r, t/8/1.0/l, c/2/0.4". La progresion se REINICIA en cada seccion. 24-56 compases en total.
CRITERIO POR ESTILO (respeta la teoria):
blues: modo 4 o 5, voicing 2, swing 0.58-0.62, prog de 12, bajo 2, comp 1, melmodo 0, forma "jb/4/0.4, t/12/0.6, t/12/0.75, t/12/0.9/s, t/12/0.6, jcm/1/0.4".
techno: modo 2 o 5, bpm 122-135, kick 4 al piso, hats contratiempo, bajo 3, comp 2, melmodo 2, gate 2-3, atk<0.1, forma "b/8/0.5, bj/8/0.6, t/16/0.85, cm/8/0.5/h, t/16/1.0, bj/8/0.5".
synthwave: modo 5, bpm 84-108, prog "0,5,2,6", padwave 1, det 15+, bajo 4, comp 0, melmodo 1, forma "m/4/0.4/a, bjm/8/0.55/a, t/8/0.85/l, t/8/0.6/a, t/8/0.95/l, cm/4/0.4/a".
grunge/rock: modo 5, voicing 3, comp 3, bajo 5, melmodo 3, tone 0.7+, forma "c/4/0.5, bjm/8/0.45/r, t/8/0.95/l, bjm/8/0.5/r, t/8/1.0/l, cb/2/0.6".
espacial/ambiente: plvl 0-0.2, atk 0.6+, rel 3+, bpm<62, bajo 1, comp 0, melmodo 1 o 4, meldens 0-1, gate 0, cutoff<800, lfor<0.1.
triste: modos 2/5/6, bpm bajo, cutoff bajo. alegre: modos 0/3/4. epico: modo 6, q alto, lfod 3000+.
Responde solo el JSON.)";

// Cancion por defecto al encender (espacial suave) — tambien documenta el formato
const char DEFAULT_SPEC[] PROGMEM =
  "{\"estilo\":\"espacial\",\"raiz\":2,\"modo\":1,\"bpm\":56,\"swing\":0.5,"
  "\"prog\":\"0,3,6,0,5,6\",\"beats\":\"4,4,4,4,4,8\",\"voicing\":2,\"padwave\":1,"
  "\"atk\":0.9,\"rel\":3.5,\"cutoff\":600,\"q\":1.2,\"lfor\":0.05,\"lfod\":1400,"
  "\"det\":16,\"tone\":0.45,\"padlvl\":0.55,\"gate\":0,\"gatedepth\":0,"
  "\"comp\":0,\"bajo\":1,\"bajowave\":0,\"bajolvl\":0.7,"
  "\"kick\":\"1000000010000000\",\"snare\":\"0000000000000000\","
  "\"hatc\":\"0010000000100000\",\"hato\":\"0000000000000000\",\"ghost\":0.03,\"plvl\":0.18,"
  "\"kickdec\":0.35,\"sndec\":0.1,\"snoise\":0.6,"
  "\"melmodo\":1,\"melwave\":3,\"meldens\":1,\"melgain\":0.75,\"env\":2,\"sub\":1,"
  "\"forma\":\"c/4/0.3, cm/8/0.4/a, t/8/0.55/l, jcm/8/0.45/a, t/8/0.6/l, cm/4/0.3/a\"}";

// ==============================================================================================================================================
// PARSER DEL JSON COMPACTO
// ==============================================================================================================================================

float jNum(const String &j, const char *key, float def) {
  String k = "\"" + String(key) + "\"";
  int p = j.indexOf(k);
  if (p < 0) return def;
  p = j.indexOf(':', p + k.length());
  if (p < 0) return def;
  p++;
  while (p < (int)j.length() && (j[p] == ' ' || j[p] == '"')) p++;
  int e = p;
  while (e < (int)j.length() && (isdigit(j[e]) || j[e] == '-' || j[e] == '.' || j[e] == '+')) e++;
  if (e == p) return def;
  return j.substring(p, e).toFloat();
}

void jStr(const String &j, const char *key, char *out, int maxN, const char *def) {
  strncpy(out, def, maxN - 1); out[maxN - 1] = 0;
  String k = "\"" + String(key) + "\"";
  int p = j.indexOf(k);
  if (p < 0) return;
  p = j.indexOf(':', p + k.length()); if (p < 0) return;
  p = j.indexOf('"', p);              if (p < 0) return;
  int e = j.indexOf('"', p + 1);      if (e < 0) return;
  int n = min(e - p - 1, maxN - 1);
  j.substring(p + 1, p + 1 + n).toCharArray(out, n + 1);
}

uint16_t jPat(const String &j, const char *key) {
  char buf[20];
  jStr(j, key, buf, sizeof(buf), "");
  uint16_t m = 0;
  for (int s = 0; s < 16 && buf[s]; s++)
    if (buf[s] == '1' || buf[s] == 'x' || buf[s] == 'X') m |= ((uint16_t)1 << s);
  return m;
}

int jArr(const String &j, const char *key, int *out, int maxN) {
  char buf[80];
  jStr(j, key, buf, sizeof(buf), "");
  int n = 0; char *p = buf;
  while (*p && n < maxN) {
    out[n++] = atoi(p);
    while (*p && *p != ',') p++;
    if (*p == ',') p++;
  }
  return n;
}

// "forma":"c/4/0.3, bjm/8/0.5/r, t/8/0.9/l" -> secs[]
int parseForma(const String &j) {
  char buf[220];
  jStr(j, "forma", buf, sizeof(buf), "");
  secCount = 0;
  char *p = buf;
  while (*p && secCount < SEC_MAX) {
    while (*p == ' ' || *p == ',') p++;
    if (!*p) break;
    uint8_t lay = 0;
    while (*p && *p != '/' && *p != ',') {
      if      (*p == 'b') lay |= LAY_BAT;
      else if (*p == 'j') lay |= LAY_BAJO;
      else if (*p == 'c') lay |= LAY_COMP;
      else if (*p == 'm') lay |= LAY_MEL;
      else if (*p == 't') lay  = 0xF;
      p++;
    }
    int bars = 4; float inten = 0.8f; int8_t mm = -1;
    if (*p == '/') { p++; bars = atoi(p); while (*p && *p != '/' && *p != ',') p++; }
    if (*p == '/') { p++; inten = atof(p); while (*p && *p != '/' && *p != ',') p++; }
    if (*p == '/') {
      p++;
      char c = *p;
      mm = (c=='q')?0 : (c=='a')?1 : (c=='h')?2 : (c=='r')?3 : (c=='l')?4 : (c=='s')?5 : -1;
      while (*p && *p != ',') p++;
    }
    if (bars < 1) bars = 1; if (bars > 32) bars = 32;
    if (inten < 0.1f) inten = 0.1f; if (inten > 1.0f) inten = 1.0f;
    secs[secCount].layers  = lay ? lay : 0xF;
    secs[secCount].bars    = (uint8_t)bars;
    secs[secCount].inten   = inten;
    secs[secCount].melmode = mm;
    secCount++;
  }
  return secCount;
}

// ==============================================================================================================================================
// HELPERS DE ENTRADA / VOCES
// ==============================================================================================================================================

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
  float p = pan * panWidth;
  if (p >  1.0f) p =  1.0f; if (p < -1.0f) p = -1.0f;
  float phr = (p + 1.0f) * 0.125f;
  v.rGain = oscSine(phr);
  v.lGain = oscSine(phr + 0.25f);
  return idx;
}

void releaseAllPad() {
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind == KIND_PAD && voices[i].stage < 2)
      voices[i].stage = 2;
}

void fadeMelody() {
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind == KIND_MEL) {
      voices[i].stage = 3;
      voices[i].decCoef = melRelCoef;
    }
}

void capPad(int keep) {
  float fastFade = expf(-6.5f / (0.006f * SAMPLE_RATE));
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
    voices[vic].stage = 3;
    voices[vic].decCoef = fastFade;
    cnt--;
  }
}

// ─── Acordes: tonos + voicing ──────────────────────────────
void chordTonesOf(int deg, int *tones, int *nt) {
  int r  = keyRoot + degSemi(deg);
  int t3 = keyRoot + degSemi(deg + 2);
  int t5 = keyRoot + degSemi(deg + 4);
  int t7 = keyRoot + degSemi(deg + 6);
  int n = 0;
  if (padVoicing == 3) { tones[n++] = r; tones[n++] = t5; tones[n++] = r + 12; }
  else {
    tones[n++] = r; tones[n++] = t3; tones[n++] = t5;
    if (padVoicing == 1) tones[n++] = r + 12;
    if (padVoicing == 2) tones[n++] = t7;
  }
  *nt = n;
  curChordTones[0] = ((r  % 12) + 12) % 12;
  curChordTones[1] = ((t3 % 12) + 12) % 12;
  curChordTones[2] = ((t5 % 12) + 12) % 12;
}

// Pad SOSTENIDO (comp 0): dispara el acorde completo con voces dobladas
void triggerPadChord(int deg) {
  releaseAllPad();
  capPad(8);
  int tones[4], nt;
  chordTonesOf(deg, tones, &nt);
  for (int n = 0; n < nt; n++) {
    float pan = (n & 1) ? 0.55f : -0.55f;
    spawnVoice(tones[n], 0.55f, pan - 0.22f, KIND_PAD, LYR_CORE, padAtkInc, 0.0f, padWave, padLpA, 1);
    spawnVoice(tones[n], 0.55f, pan + 0.22f, KIND_PAD, LYR_CORE, padAtkInc, 0.0f, padWave, padLpA, 1);
  }
  if (subOsc) spawnVoice(tones[0] - 12, 0.50f, 0.0f, KIND_PAD, LYR_SUB, padAtkInc, 0.0f, padWave, padLpA, 1);
}

// Golpe CORTO de acorde (comp 1/2/3): una voz por tono, envolvente que decae → stabs/power chords
void padHit(float durSec, float gain) {
  capPad(10);
  int tones[4], nt;
  chordTonesOf(progDeg[progIdx], tones, &nt);
  float atkInc  = 1.0f / (0.004f * SAMPLE_RATE);
  float decCoef = expf(-6.5f / (durSec * SAMPLE_RATE));
  for (int n = 0; n < nt; n++) {
    float pan = ((n & 1) ? 0.4f : -0.4f);
    spawnVoice(tones[n], gain, pan, KIND_PAD, LYR_CORE, atkInc, decCoef, padWave, padLpA, 3);
  }
}

// ==============================================================================================================================================
// FORMA / PROGRESION (todo avanza por COMPAS — siempre en la grilla)
// ==============================================================================================================================================

void resolveHungNote();

void applyChord() {
  int deg = progDeg[progIdx];
  g_curDeg = deg;
  if ((layMask & LAY_COMP) && compMode == 0) {
    triggerPadChord(deg);              // sostenido: dispara y actualiza tonos
  } else {
    int tones[4], nt;
    chordTonesOf(deg, tones, &nt);     // solo actualiza los tonos (los golpes usan el grid)
  }
  g_chordFlash = 1.0f;
  resolveHungNote();
}

void applySection() {
  const Section &s = secs[secIdx];
  layMask    = s.layers;
  secInten   = s.inten;
  melModeSec = s.melmode;
  if (!(layMask & LAY_COMP)) releaseAllPad();
  if (!(layMask & LAY_MEL))  fadeMelody();
}

inline int curMelMode() { return (melModeSec >= 0) ? (int)melModeSec : melMode; }

// Un compas nuevo empieza: avanza seccion y/o acorde (la progresion se REINICIA por seccion)
void barTick() {
  bool secChanged = false;
  if (secCount > 0) {
    secBar++;
    if (secBar >= secs[secIdx].bars) {
      secIdx = (secIdx + 1) % secCount;
      secBar = 0;
      applySection();
      secChanged = true;
    }
  }
  if (secChanged) {
    progIdx = 0;
    chordBarsLeft = progBars[0];
    applyChord();
  } else {
    if (--chordBarsLeft <= 0) {
      progIdx = (progIdx + 1) % progLen;
      chordBarsLeft = progBars[progIdx];
      applyChord();
    }
  }
  // El hook/riff MUTA levemente cada 4 compases (que respire sin perder identidad)
  int mm = curMelMode();
  if ((mm == 2 || mm == 3) && (percBar & 3) == 3 && rndF() < 0.5f) {
    int tries = 0;
    while (tries++ < 8) {
      int s = rndI(16);
      if (hookMask & (1u << s)) {
        hookDeg[s] += (rndF() < 0.5f) ? -1 : 1;
        if (hookDeg[s] < 0) hookDeg[s] = 0; if (hookDeg[s] > 9) hookDeg[s] = 9;
        break;
      }
    }
  }
}

// ==============================================================================================================================================
// MELODIA — gramatica 0: frases generativas (motor de cancion_aleatoria)
// ==============================================================================================================================================

inline void clampMelWindow() {
  if (melIdx < melLo) melIdx = melLo + (melLo - melIdx);
  if (melIdx > melHi) melIdx = melHi - (melIdx - melHi);
  if (melIdx < melLo) melIdx = melLo;
  if (melIdx > melHi) melIdx = melHi;
}

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

void resolveHungNote() {
  if (melVoiceIdx >= 0 && voices[melVoiceIdx].active &&
      voices[melVoiceIdx].kind == KIND_MEL && voices[melVoiceIdx].stage == 1) {
    int pc = ((scaleSemis[melIdx] % 12) + 12) % 12;
    if (pc != curChordTones[0] && pc != curChordTones[1] && pc != curChordTones[2]) {
      snapMelToChord();
      int semi = scaleSemis[melIdx] + phraseOct;
      while (semi > VOCAL_HI) semi -= 12;
      while (semi < VOCAL_LO) semi += 12;
      voices[melVoiceIdx].freqTarget = semiToFreq(semi);
      voices[melVoiceIdx].glideCoef  = 1.0f / (0.09f * SAMPLE_RATE);
    }
  }
}

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

void newPhrase() {
  phraseLeft  = 3 + rndI(7);
  phraseStyle = stylePool[rndI(stylePoolN)];
  phraseEnv   = envPool[rndI(envPoolN)];
  static const int8_t OCTS[4] = { 0, 0, 0, -12 };
  phraseOct   = OCTS[rndI(4)];
  phraseSub   = (rndF() < subChance);
  float lp = lpMin + rndF() * (lpMax - lpMin);
  phraseLpA = (melWave == 1) ? (0.10f + lp * 0.35f) : (0.22f + lp * 0.55f);
  if (phraseLpA > 0.85f) phraseLpA = 0.85f;
  legatoChance = 0.40f + rndF() * 0.45f;
  melIdx = (melLo + melHi) / 2 + (rndI(7) - 3);
}

void melodyStep() {
  bool solo = (curMelMode() == 5);
  if (phraseLeft <= 0) newPhrase();
  if (solo) { phraseStyle = (rndF() < 0.6f) ? 4 : 2; }   // solo: siempre movido
  phraseLeft--;

  int units = STYLE_DUR[phraseStyle][rndI(6)];
  uint32_t maxDur = (uint32_t)(SAMPLE_RATE * MEL_NOTE_MAX_SEC);
  while (units > 4 && (uint32_t)units * gridSamples > maxDur) units -= 4;
  uint32_t durSamples = (uint32_t)units * gridSamples;
  melSamplesToNext = (int32_t)durSamples;

  if (!solo && rndF() < STYLE_REST[phraseStyle]) { fadeMelody(); melVoiceIdx = -1; lastWasPassing = false; return; }

  bool longish  = (units >= 6);
  bool shortish = (units <= 3);
  bool chordTone;
  if      (longish || lastWasPassing) chordTone = true;
  else if (shortish)                  chordTone = (rndF() < 0.30f);
  else                                chordTone = (rndF() < 0.70f);
  pickMelodyPitch(chordTone);

  if (melIdx == melPrevIdx) {
    if (++melRepeat >= 2) {
      int tries = 0;
      while (melIdx == melPrevIdx && tries++ < 6) {
        int dir = (melIdx > (melLo + melHi) / 2) ? -1 : 1;
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

  int semi = scaleSemis[melIdx] + phraseOct;
  while (semi > VOCAL_HI) semi -= 12;
  while (semi < VOCAL_LO) semi += 12;
  float freq = semiToFreq(semi);

  bool canLegato = (melVoiceIdx >= 0 && voices[melVoiceIdx].active &&
                    voices[melVoiceIdx].kind == KIND_MEL && voices[melVoiceIdx].env > 0.30f);
  if (canLegato && rndF() < legatoChance) {
    for (int i = 0; i < NUM_VOICES; i++)
      if (i != melVoiceIdx && voices[i].active && voices[i].kind == KIND_MEL && voices[i].stage != 3) {
        voices[i].stage = 3; voices[i].decCoef = melRelCoef;
      }
    Voice &v = voices[melVoiceIdx];
    v.freqTarget = freq;
    float glideSec = 0.03f + rndF() * 0.06f;
    v.glideCoef = 1.0f / (glideSec * SAMPLE_RATE);
    v.stage = 1;
    g_melFlash = 1.0f; g_melPos = (g_melPos + 1) % NUM_LEDS;
    return;
  }

  fadeMelody();

  float durSec = (float)durSamples / SAMPLE_RATE;
  float atkSec, decSec; uint8_t afterAtk;
  if (phraseEnv == 1) {
    atkSec = 0.03f + rndF() * 0.12f;
    decSec = 1.0f;
    afterAtk = 1;
  } else {
    atkSec = 0.010f + rndF() * 0.05f;
    decSec = durSec * (0.75f + rndF() * 0.60f);
    afterAtk = 3;
  }
  if (decSec < 0.15f) decSec = 0.15f;
  if (atkSec > durSec * 0.5f) atkSec = durSec * 0.5f;
  float atkInc  = 1.0f / (atkSec * SAMPLE_RATE);
  float decCoef = expf(-6.5f / (decSec * SAMPLE_RATE));

  melVibDepth = 0.006f + rndF() * 0.012f;
  melVibR0    = 3.5f + rndF() * 1.5f;
  melVibR1    = melVibR0 + 1.5f + rndF() * 2.5f;

  float vel = 0.5f + rndF() * 0.5f;
  if (units >= 8) vel = 0.72f + rndF() * 0.38f;
  float hiSoft = 1.0f;
  if (semi > SOFT_FROM) { hiSoft = 1.0f - (float)(semi - SOFT_FROM) * 0.055f; if (hiSoft < 0.35f) hiSoft = 0.35f; }
  float g   = vel * melGain * hiSoft * (0.7f + 0.3f * secInten);
  float lpN = phraseLpA * (0.6f + 0.4f * vel);
  if (semi > SOFT_FROM) lpN *= 0.8f;
  if (lpN > 0.9f) lpN = 0.9f;
  float pan = (rndF() * 2.0f - 1.0f) * 0.55f;

  melVoiceIdx = spawnVoice(semi, g, pan, KIND_MEL, LYR_CORE, atkInc, decCoef, melWave, lpN, afterAtk);
  if (phraseSub)
    spawnVoice(semi - 12, g * 0.5f, -pan, KIND_MEL, LYR_CORE, atkInc, decCoef, melWave, lpN, afterAtk);

  g_melFlash = 1.0f;
  g_melPos   = (g_melPos + 1) % NUM_LEDS;
}

// ==============================================================================================================================================
// MELODIA — gramaticas por GRID (arpegio / hook / riff / lirica)
// ==============================================================================================================================================

// Nota melodica simple por grid (pluck, sin legato)
void melGridNote(int semi, float durSteps, float vel, bool vibrato) {
  while (semi > VOCAL_HI) semi -= 12;
  while (semi < VOCAL_LO - 5) semi += 12;   // el riff puede bajar un poco del rango vocal
  float durSec = durSteps * (float)gridSamples / SAMPLE_RATE;
  float atkInc  = 1.0f / (0.005f * SAMPLE_RATE);
  float decCoef = expf(-6.5f / (durSec * 1.05f * SAMPLE_RATE));
  float g = vel * melGain * (0.7f + 0.3f * secInten);
  float lpN = 0.35f + 0.35f * ((lpMin + lpMax) * 0.5f);
  if (vibrato) { melVibDepth = 0.010f; melVibR0 = 4.5f; melVibR1 = 6.0f; }
  else         { melVibDepth = 0.002f; }
  float pan = (rndF() * 2.0f - 1.0f) * 0.4f;
  spawnVoice(semi, g, pan, KIND_MEL, LYR_CORE, atkInc, decCoef, melWave, lpN, 3);
  g_melFlash = 1.0f;
  g_melPos   = (g_melPos + 1) % NUM_LEDS;
}

void melGridStep(int step, int mm) {
  switch (mm) {
    case 1: {   // ARPEGIO: semicorcheas subiendo por los tonos del acorde en 2 octavas
      if (rndF() > melDensity) { arpCount++; break; }
      int tones[4], nt;
      chordTonesOf(progDeg[progIdx], tones, &nt);
      int base = tones[0];
      while (base > 12) base -= 12;
      while (base < 2)  base += 12;
      int idx = arpCount % (nt * 2);
      int semi = ((idx < nt) ? tones[idx] - tones[0] : tones[idx - nt] - tones[0] + 12) + base;
      arpCount++;
      melGridNote(semi, 1.1f, ((step & 3) == 0) ? 0.8f : 0.55f, false);
      break;
    }
    case 2: {   // HOOK: motivo de 1 compas anclado a la TONICA (electronica, armonia estatica)
      if (!(hookMask & (1u << step))) break;
      int semi = degSemi(hookDeg[step]) + keyRoot - 12;
      while (semi > 14) semi -= 12;
      while (semi < 0)  semi += 12;
      melGridNote(semi, 1.4f, ((step & 3) == 0) ? 0.85f : 0.6f, false);
      break;
    }
    case 3: {   // RIFF: el mismo motivo pero TRANSPUESTO al acorde y una octava abajo (rock)
      if (!(hookMask & (1u << step))) break;
      int semi = degSemi(hookDeg[step]) + keyRoot + degSemi(progDeg[progIdx]) - 12;
      while (semi > 7)   semi -= 12;
      while (semi < -10) semi += 12;
      melGridNote(semi, 1.5f, ((step & 3) == 0) ? 0.9f : 0.62f, false);
      break;
    }
    case 4: {   // LIRICA: 1-2 notas LARGAS del acorde por compas, con vibrato (coros)
      bool fire = (step == 0) || (step == 8 && rndF() < 0.6f);
      if (!fire) break;
      fadeMelody();
      int tones[4], nt;
      chordTonesOf(progDeg[progIdx], tones, &nt);
      int semi = tones[rndI(nt)] + 12;
      while (semi > VOCAL_HI) semi -= 12;
      while (semi < 10) semi += 12;
      melGridNote(semi, (step == 0) ? 7.0f : 6.0f, 0.85f, true);
      break;
    }
  }
}

// ==============================================================================================================================================
// BAJO — patrones idiomaticos por grid
// ==============================================================================================================================================

void bassNote(int semi, float durSteps, float vel) {
  // mono: la nota anterior se apaga rapido (sin colas que embarren el grave)
  if (bassVoiceIdx >= 0 && voices[bassVoiceIdx].active && voices[bassVoiceIdx].kind == KIND_BASS) {
    voices[bassVoiceIdx].stage = 3;
    voices[bassVoiceIdx].decCoef = expf(-6.5f / (0.025f * SAMPLE_RATE));
  }
  float durSec = durSteps * (float)gridSamples / SAMPLE_RATE;
  float atkInc  = 1.0f / (0.004f * SAMPLE_RATE);
  float decCoef = expf(-6.5f / (durSec * 1.15f * SAMPLE_RATE));
  bassVoiceIdx = spawnVoice(semi, vel * (0.65f + 0.35f * secInten), 0.0f,
                            KIND_BASS, LYR_CORE, atkInc, decCoef, bassWave, 0.30f, 3);
}

inline int bassRootSemi(int deg) {
  int r = keyRoot + degSemi(deg) - 24;          // ~C1
  while (r > -10) r -= 12;
  while (r < -26) r += 12;                      // rango C1..D2
  return r;
}

void bassStep(int step) {
  int deg  = progDeg[progIdx];
  int root = bassRootSemi(deg);
  switch (bassMode) {
    case 1:   // FUNDAMENTAL-QUINTA (lento / ambiente)
      if (step == 0)      bassNote(root, 7.5f, 0.85f);
      else if (step == 8) bassNote(root + 7, 7.5f, 0.65f);
      break;
    case 2: { // WALKING por negras: fundamental → 3ª → 5ª → aproximacion cromatica al proximo
      if ((step & 3) != 0) break;
      int q = step >> 2;
      int t3rel = degSemi(deg + 2) - degSemi(deg);
      int note;
      if      (q == 0) note = root;
      else if (q == 1) note = root + t3rel;
      else if (q == 2) note = root + 7;
      else {
        int nx = bassRootSemi(progDeg[(progIdx + 1) % progLen]);
        note = nx + ((rndF() < 0.5f) ? -1 : 1);
      }
      bassNote(note, 3.6f, (q == 0) ? 0.9f : 0.72f);
      break;
    }
    case 3:   // CONTRATIEMPO (techno rolling: entre los bombos)
      if ((step & 3) == 2) bassNote(root, 1.6f, (step == 2 || step == 10) ? 0.88f : 0.78f);
      else if (step == 15 && rndF() < 0.3f) bassNote(root, 0.9f, 0.55f);
      break;
    case 4:   // OCTAVAS en corcheas (synthwave / disco)
      if ((step & 1) == 0) bassNote(root + ((step & 2) ? 12 : 0), 1.7f, ((step & 3) == 0) ? 0.9f : 0.72f);
      break;
    case 5: { // RIFF en corcheas: fundamental con 5ª/7ª de paso (rock / grunge / funk)
      if ((step & 1) != 0) break;
      int note = root;
      if (step == 12 && rndF() < 0.45f) note = root + 7;
      if (step == 14 && rndF() < 0.40f) note = root + 10;
      bassNote(note, 1.7f, ((step & 3) == 0) ? 0.9f : 0.7f);
      break;
    }
  }
}

// ==============================================================================================================================================
// COMPING por golpes (el pad sostenido es comp 0 y se dispara en applyChord)
// ==============================================================================================================================================

void compStep(int step) {
  float g = 0.5f * (0.6f + 0.4f * secInten);
  switch (compMode) {
    case 1:   // golpes en 2 y 4 (blues/soul) + push ocasional al final del compas
      if (step == 4 || step == 12) padHit(0.32f, g);
      else if (step == 15 && rndF() < 0.25f) padHit(0.15f, g * 0.7f);
      break;
    case 2:   // stabs sincopados (techno/house)
      if (step == 2 || step == 10) padHit(0.16f, g * 1.1f);
      else if (step == 7 && rndF() < 0.4f) padHit(0.14f, g * 0.85f);
      break;
    case 3:   // power chords en corcheas (grunge — usar con voicing 3)
      if ((step & 1) == 0) padHit(0.17f, ((step & 3) == 0 ? 1.15f : 0.85f) * g);
      break;
  }
}

// ==============================================================================================================================================
// PERCUSION
// ==============================================================================================================================================

int percPanStep = 0;   // para alternar el lado de los hats

void triggerPerc(uint8_t type, float vel) {
  int idx = -1;
  for (int i = 0; i < PERC_VOICES; i++) if (!pvoices[i].active) { idx = i; break; }
  if (idx < 0) {
    float lo = 1e30f; idx = 0;
    for (int i = 0; i < PERC_VOICES; i++) {
      float a = pvoices[i].env * pvoices[i].gain;
      if (a < lo) { lo = a; idx = i; }
    }
  }
  PercVoice &v = pvoices[idx];
  v.active = true; v.phase = 0.0f; v.env = 1.0f;
  v.lp = 0.0f; v.hipass = false;
  v.freqEnd = 0.0f; v.sweepCoef = 0.0f;
  float pan = 0.0f, dec = 0.1f;
  switch (type) {
    case PK_KICK:
      v.freq = kickF0; v.freqEnd = kickF1;
      v.sweepCoef = 1.0f / (0.045f * SAMPLE_RATE);
      v.noiseMix = 0.05f; v.lpA = 0.60f;
      dec = kickDec; v.gain = 1.15f * vel; pan = 0.0f;
      g_kickFlash = 1.0f;
      break;
    case PK_SNARE:
      v.freq = snFreq; v.freqEnd = snFreq * 0.6f;
      v.sweepCoef = 1.0f / (0.030f * SAMPLE_RATE);
      v.noiseMix = snNoise; v.lpA = 0.55f;
      dec = snDec; v.gain = 0.75f * vel; pan = 0.12f;
      break;
    case PK_HATC:
      v.freq = 0.0f; v.noiseMix = 1.0f; v.lpA = hatTone; v.hipass = true;
      dec = hatDecC; v.gain = 0.34f * vel;
      pan = (percPanStep & 2) ? 0.35f : -0.35f;
      break;
    default:
      v.freq = 0.0f; v.noiseMix = 1.0f; v.lpA = hatTone * 0.8f; v.hipass = true;
      dec = hatDecO; v.gain = 0.28f * vel; pan = -0.25f;
      break;
  }
  v.decCoef = expf(-6.5f / (dec * SAMPLE_RATE));
  float p = pan; if (p > 1.0f) p = 1.0f; if (p < -1.0f) p = -1.0f;
  float phr = (p + 1.0f) * 0.125f;
  v.rGain = oscSine(phr);
  v.lGain = oscSine(phr + 0.25f);
}

void percStepFire(int step) {
  percPanStep = step;
  uint16_t bit = (uint16_t)1 << step;
  bool  onBeat = (step & 3) == 0;
  float inten  = 0.55f + 0.45f * secInten;
  float acc    = (onBeat ? 1.0f : 0.82f) * inten;
  bool  fill   = ((percBar & 3) == 3) && step >= 12 && secInten > 0.45f;

  if (patKick & bit)  triggerPerc(PK_KICK,  acc * (0.85f + rndF() * 0.15f));
  if (patSnare & bit) triggerPerc(PK_SNARE, acc * (0.80f + rndF() * 0.20f));
  else if (fill && rndF() < 0.35f) triggerPerc(PK_SNARE, 0.35f + rndF() * 0.25f);
  if (patHatO & bit)      triggerPerc(PK_HATO, (0.7f + rndF() * 0.3f) * inten);
  else if (patHatC & bit) triggerPerc(PK_HATC, acc * (0.65f + rndF() * 0.35f));
  else if (ghostProb > 0.0f && rndF() < ghostProb * secInten)
    triggerPerc(PK_HATC, 0.25f + rndF() * 0.20f);
}

// SWING: el paso par dura mas y el impar menos (par+impar = 2 semicorcheas exactas)
inline uint32_t stepDurOf(int firedStep) {
  if (swingAmt <= 0.505f) return gridSamples;
  if ((firedStep & 1) == 0) return (uint32_t)((float)gridSamples * 2.0f * swingAmt);
  else                      return (uint32_t)((float)gridSamples * 2.0f * (1.0f - swingAmt));
}

// ─── El GRID unificado: bateria + bajo + comping + melodia-por-grid, con swing ──
void gridStepFire() {
  int step = gridStep;

  if (step == 0) {
    if (barArmed) barTick();       // acorde/seccion cambian ANTES de sonar el paso 0
    else          barArmed = true;
  }

  if (percOn && (layMask & LAY_BAT)) percStepFire(step);
  if (bassMode > 0 && (layMask & LAY_BAJO)) bassStep(step);
  if ((layMask & LAY_COMP) && compMode > 0) compStep(step);
  int mm = curMelMode();
  if ((layMask & LAY_MEL) && mm >= 1 && mm <= 4) melGridStep(step, mm);

  gridStep = (gridStep + 1) & 15;
  if (gridStep == 0) percBar++;
}

// ==============================================================================================================================================
// APLICAR UN JSON DE CANCION
// ==============================================================================================================================================
bool applySpec(const String &j) {
  if (j.indexOf('{') < 0) return false;

  int tmpProg[PROG_MAX];
  int n = jArr(j, "prog", tmpProg, PROG_MAX);
  if (n < 2) return false;

  keyRoot = ((int)jNum(j, "raiz", 0) % 12 + 12) % 12;
  int mode = (int)jNum(j, "modo", 5);
  if (mode < 0 || mode >= NUM_MODES) mode = 5;
  for (int i = 0; i < 7; i++) scaleInt[i] = MODES[mode][i];
  for (int i = 0; i < N_SCALE; i++) scaleSemis[i] = keyRoot + degSemi(i);

  songBPM = (int)jNum(j, "bpm", 80);
  if (songBPM < 40) songBPM = 40; if (songBPM > 160) songBPM = 160;
  beatSamples = (uint32_t)((uint64_t)SAMPLE_RATE * 60 / songBPM);
  gridSamples = beatSamples / 4;
  swingAmt = jNum(j, "swing", 0.5f);
  if (swingAmt < 0.5f) swingAmt = 0.5f; if (swingAmt > 0.66f) swingAmt = 0.66f;

  progLen = n;
  for (int i = 0; i < progLen; i++) progDeg[i] = ((tmpProg[i] % 7) + 7) % 7;
  int tmpBeats[PROG_MAX];
  int nb = jArr(j, "beats", tmpBeats, PROG_MAX);
  for (int i = 0; i < progLen; i++) {
    int b = (i < nb) ? tmpBeats[i] : 4;
    int bars = (b + 3) / 4;                        // beats -> compases (min 1)
    if (bars < 1) bars = 1; if (bars > 8) bars = 8;
    progBars[i] = bars;
  }

  padVoicing = (int)jNum(j, "voicing", 0) & 3;
  padWave    = (int)jNum(j, "padwave", 3) & 3;
  float atk  = jNum(j, "atk", 0.3f);  if (atk < 0.01f) atk = 0.01f; if (atk > 2.0f) atk = 2.0f;
  float rel  = jNum(j, "rel", 1.5f);  if (rel < 0.2f)  rel = 0.2f;  if (rel > 5.0f) rel = 5.0f;
  padAtkInc      = 1.0f / (atk * SAMPLE_RATE);
  padReleaseCoef = expf(-6.5f / (rel * SAMPLE_RATE));
  cutoffBase   = jNum(j, "cutoff", 800.0f);
  qBase        = jNum(j, "q", 1.4f);
  filtLfoRate  = jNum(j, "lfor", 0.10f);
  filtLfoDepth = jNum(j, "lfod", 1800.0f);
  detuneCents  = jNum(j, "det", 12.0f);
  panWidth     = 0.40f + rndF() * 0.30f;
  float tone   = jNum(j, "tone", 0.55f);
  if (tone < 0.1f) tone = 0.1f; if (tone > 0.95f) tone = 0.95f;
  toneCoef = 0.25f + tone * 0.55f;
  padLpA   = 0.15f + tone * 0.55f;
  lpMin = tone * 0.4f; lpMax = tone * 0.85f + 0.05f;
  subOsc   = jNum(j, "sub", 1) > 0.5f;
  padLevel = jNum(j, "padlvl", 0.55f);
  if (padLevel < 0.1f) padLevel = 0.1f; if (padLevel > 0.9f) padLevel = 0.9f;

  int gate = (int)jNum(j, "gate", 0);
  padGateDepth = jNum(j, "gatedepth", 0.0f);
  if (padGateDepth < 0.0f) padGateDepth = 0.0f; if (padGateDepth > 0.9f) padGateDepth = 0.9f;
  switch (gate) {
    case 1:  padGateSamples = beatSamples * 2; break;
    case 2:  padGateSamples = beatSamples;     break;
    case 3:  padGateSamples = beatSamples / 2; break;
    default: padGateSamples = 0; padGateDepth = 0.0f; break;
  }
  padGateAcc = 0;

  // — Comping / Bajo (NUEVO v2) —
  compMode  = (int)jNum(j, "comp", 0);
  if (compMode < 0 || compMode > 3) compMode = 0;
  bassMode  = (int)jNum(j, "bajo", 1);
  if (bassMode < 0 || bassMode > 5) bassMode = 1;
  int bw = (int)jNum(j, "bajowave", 1);
  bassWave  = (bw == 2) ? 1 : (bw & 3);
  bassLevel = jNum(j, "bajolvl", 0.8f);
  if (bassLevel < 0.0f) bassLevel = 0.0f; if (bassLevel > 1.2f) bassLevel = 1.2f;

  // — Melodia: gramatica + caracter —
  melMode = (int)jNum(j, "melmodo", 0);
  if (melMode < 0 || melMode > 4) melMode = 0;
  int mw = (int)jNum(j, "melwave", 3);
  melWave = (mw == 2) ? 3 : (mw & 3);
  melGain = jNum(j, "melgain", 0.85f);
  int dens = (int)jNum(j, "meldens", 2);
  if (dens < 0) dens = 0; if (dens > 4) dens = 4;
  melDensity = 0.45f + 0.14f * dens;
  switch (dens) {
    case 0: stylePool[0]=3; stylePool[1]=3; stylePool[2]=0; stylePool[3]=3; break;
    case 1: stylePool[0]=0; stylePool[1]=0; stylePool[2]=1; stylePool[3]=3; break;
    case 2: stylePool[0]=1; stylePool[1]=1; stylePool[2]=2; stylePool[3]=0; break;
    case 3: stylePool[0]=2; stylePool[1]=2; stylePool[2]=1; stylePool[3]=4; break;
    default:stylePool[0]=4; stylePool[1]=4; stylePool[2]=2; stylePool[3]=2; break;
  }
  stylePoolN = 4;
  int env = (int)jNum(j, "env", 2);
  switch (env) {
    case 0:  envPool[0]=0; envPool[1]=0; envPool[2]=0; break;
    case 1:  envPool[0]=1; envPool[1]=1; envPool[2]=1; break;
    default: envPool[0]=0; envPool[1]=1; envPool[2]=1; break;
  }
  envPoolN = 3;
  subChance = subOsc ? 0.30f : 0.10f;
  float melRel = 0.20f + rndF() * 0.22f;
  melRelCoef = expf(-6.5f / (melRel * SAMPLE_RATE));
  melLevel = 1.15f + rndF() * 0.30f;

  // — Percusion —
  patKick  = jPat(j, "kick");
  patSnare = jPat(j, "snare");
  patHatC  = jPat(j, "hatc");
  patHatO  = jPat(j, "hato");
  percLevel = jNum(j, "plvl", 0.4f);
  if (percLevel < 0.0f) percLevel = 0.0f; if (percLevel > 0.9f) percLevel = 0.9f;
  percOn = (percLevel > 0.01f) && (patKick | patSnare | patHatC | patHatO);
  ghostProb = jNum(j, "ghost", 0.0f);
  if (ghostProb < 0.0f) ghostProb = 0.0f; if (ghostProb > 0.25f) ghostProb = 0.25f;
  kickDec = jNum(j, "kickdec", 0.22f);
  snDec   = jNum(j, "sndec", 0.12f);
  snNoise = jNum(j, "snoise", 0.75f);
  kickF0  = 70.0f  + rndF() * 45.0f;  kickF1 = 36.0f + rndF() * 12.0f;
  snFreq  = 165.0f + rndF() * 55.0f;
  hatDecC = 0.030f + rndF() * 0.035f; hatDecO = 0.14f + rndF() * 0.14f;
  hatTone = 0.45f  + rndF() * 0.30f;

  // — Forma (secciones) —
  parseForma(j);

  melLo = 7; melHi = 14 + rndI(3);
  if (melHi > N_SCALE - 2) melHi = N_SCALE - 2;

  g_keyHue = (uint8_t)(keyRoot * 21 + 20);
  haveSpec = true;

  char nombre[24];
  jStr(j, "estilo", nombre, sizeof(nombre), "?");
  Serial.printf("[SPEC] estilo=%s raiz=%d modo=%d bpm=%d swing=%.2f prog(%d) bajo=%d comp=%d melmodo=%d secciones=%d\n",
                nombre, keyRoot, mode, songBPM, swingAmt, progLen, bassMode, compMode, melMode, secCount);
  return true;
}

// ─── Arrancar / parar ──────────────────────────────────────
void startSong() {
  if (!haveSpec) return;
  rng ^= (uint32_t)micros() + voiceCounter * 2654435761u;
  rndU(); rndU();

  // Generar el HOOK/RIFF de esta pasada (motivo de 1 compas sobre la escala)
  hookMask = 0;
  int nOn = 3 + rndI(3);                          // 3-5 notas
  for (int k = 0; k < nOn; k++) {
    int s = (k * 16) / nOn + rndI(3);
    s &= 15;
    if ((s & 1) && rndF() < 0.7f) s &= ~1;        // mayormente en corcheas
    hookMask |= (1u << s);
  }
  int cursor = rndI(3);
  for (int s = 0; s < 16; s++)
    if (hookMask & (1u << s)) {
      cursor += rndI(3) - 1;
      if (cursor < 0) cursor = 0; if (cursor > 7) cursor = 7;
      hookDeg[s] = cursor;
    }

  // Estado inicial
  secIdx = 0; secBar = 0;
  if (secCount > 0) applySection();
  else { layMask = 0xF; secInten = 0.8f; melModeSec = -1; }

  progIdx = 0;
  chordBarsLeft = progBars[0];
  gridStep = 0; gridToNext = 0; barArmed = false;
  percBar = 0; arpCount = 0;
  melSamplesToNext = 0;
  melVoiceIdx = -1; lastWasPassing = false;
  melIdx = (melLo + melHi) / 2;
  melPrevIdx = -999; melRepeat = 0;
  phraseLeft = 0;
  padGateAcc = 0; filtLfoPhase = 0.0f;
  g_startFlash = 1.0f;

  applyChord();
  playing = true;
}

void stopSong() {
  releaseAllPad();
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind != KIND_PAD && voices[i].stage != 3) {
      voices[i].stage = 3;
      voices[i].decCoef = expf(-6.5f / (0.12f * SAMPLE_RATE));
    }
  melVoiceIdx = -1; bassVoiceIdx = -1;
  float pf = expf(-6.5f / (0.10f * SAMPLE_RATE));
  for (int i = 0; i < PERC_VOICES; i++)
    if (pvoices[i].active && pvoices[i].decCoef > pf) pvoices[i].decCoef = pf;
  playing = false;
}

// ==============================================================================================================================================
// OSCILADOR / FILTRO
// ==============================================================================================================================================
inline float polyBlep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  else if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

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
  if (!isfinite(out)) { st.x1 = st.x2 = st.y1 = st.y2 = 0.0f; return 0.0f; }
  if (out >  8.0f) out =  8.0f; else if (out < -8.0f) out = -8.0f;
  st.x2 = st.x1; st.x1 = in;
  st.y2 = st.y1; st.y1 = out;
  return out;
}

// ==============================================================================================================================================
// LEDs
// ==============================================================================================================================================
void showUiColor(CRGB c) {
  fill_solid(leds, NUM_LEDS, c);
  onboard[0] = c; onboard[0].nscale8(ONBOARD_BRIGHT);
  FastLED.show();
}

void renderLEDs() {
  unsigned long t = millis();
  static unsigned long lastFrame = 0;
  if (t - lastFrame < LED_REFRESH_MS) return;
  lastFrame = t;

  if (uiState == UI_REC)  { showUiColor(CRGB(120, 0, 0));  return; }
  if (uiState == UI_PROC) { showUiColor(CRGB(90, 55, 0));  return; }

  uint8_t hue = g_keyHue + (uint8_t)(g_curDeg * 9) + (uint8_t)(g_filtLfoVal * 24.0f);

  if (!playing && g_energy < 0.02f) {
    static uint8_t breath = 0; static int8_t bdir = 1;
    breath += bdir * 3;
    if (breath >= 55) bdir = -1;
    if (breath <= 4)  bdir =  1;
    fill_solid(leds, NUM_LEDS, CHSV(hue, 210, breath));
  } else {
    float lvl = g_energy * 1.6f; if (lvl > 1.0f) lvl = 1.0f;
    float litf = lvl * NUM_LEDS;
    for (int i = 0; i < NUM_LEDS; i++) {
      float on = litf - i;
      if (on < 0.0f) on = 0.0f; if (on > 1.0f) on = 1.0f;
      uint8_t v = 22 + (uint8_t)(on * 200.0f);
      leds[i] = CHSV(hue + i * 6, 255, v);
    }
    if (g_melFlash > 0.02f) {
      uint8_t mv = (uint8_t)(g_melFlash * 255.0f);
      leds[g_melPos % NUM_LEDS] += CHSV(hue + 128, 235, mv);
    }
  }
  g_melFlash *= 0.6f;

  if (g_kickFlash > 0.02f) {
    uint8_t kv = (uint8_t)(g_kickFlash * 55.0f);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] += CHSV(hue + 32, 140, kv);
    g_kickFlash *= 0.55f;
  }
  if (g_chordFlash > 0.02f) {
    uint8_t cv = (uint8_t)(g_chordFlash * 90.0f);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] += CHSV(hue + 64, 180, cv);
    g_chordFlash *= 0.7f;
  }
  if (g_startFlash > 0.02f) {
    uint8_t w = (uint8_t)(g_startFlash * 170.0f);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] += CRGB(w, w, w);
    g_startFlash *= 0.62f;
  }

  uint16_t sr = 0, sg = 0, sb = 0;
  for (int i = 0; i < NUM_LEDS; i++) { sr += leds[i].r; sg += leds[i].g; sb += leds[i].b; }
  onboard[0] = CRGB(sr / NUM_LEDS, sg / NUM_LEDS, sb / NUM_LEDS);
  onboard[0].nscale8(ONBOARD_BRIGHT);

  FastLED.show();
}

void flashError() {
  for (int k = 0; k < 3; k++) {
    showUiColor(CRGB(110, 0, 110)); delay(160);
    showUiColor(CRGB::Black);       delay(120);
  }
}

// ==============================================================================================================================================
// I2S: DAC 44.1 kHz + MIC 16 kHz
// ==============================================================================================================================================
void i2s_dac_init() {
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

void i2s_mic_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));
  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(MIC_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                  I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)MIC_SCK,
      .ws   = (gpio_num_t)MIC_WS,
      .dout = I2S_GPIO_UNUSED,
      .din  = (gpio_num_t)MIC_SD,
      .invert_flags = { false, false, false },
    },
  };
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
}

// ==============================================================================================================================================
// VOZ: grabar -> Whisper -> GPT compositor
// ==============================================================================================================================================
int recordAudio() {
  int32_t raw[256];
  size_t bytesRead = 0;
  int n = 0;
  for (int k = 0; k < 10; k++) i2s_channel_read(rx_chan, raw, sizeof(raw), &bytesRead, 50);

  while (digitalRead(BTN_RECORD) == LOW && n < MAX_SAMPLES) {
    if (i2s_channel_read(rx_chan, raw, sizeof(raw), &bytesRead, 200) != ESP_OK) continue;
    int got = bytesRead / sizeof(int32_t);
    for (int i = 0; i < got && n < MAX_SAMPLES; i += 2) {
      int32_t s = raw[i] >> 16;
      s *= MIC_GAIN;
      if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
      audioBuffer[n++] = (int16_t)s;
    }
  }
  return n;
}

String transcribeAudio(int sampleCount) {
  if (sampleCount < 1000) return "";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(60);
  if (!client.connect("api.openai.com", 443)) return "";

  uint8_t wavHeader[44];
  uint32_t dataSize = sampleCount * 2;
  uint32_t fileSize = dataSize + 36;
  uint32_t byteRate = MIC_RATE * 2;
  wavHeader[0]='R'; wavHeader[1]='I'; wavHeader[2]='F'; wavHeader[3]='F';
  wavHeader[4]=fileSize&0xFF; wavHeader[5]=(fileSize>>8)&0xFF;
  wavHeader[6]=(fileSize>>16)&0xFF; wavHeader[7]=(fileSize>>24)&0xFF;
  wavHeader[8]='W'; wavHeader[9]='A'; wavHeader[10]='V'; wavHeader[11]='E';
  wavHeader[12]='f'; wavHeader[13]='m'; wavHeader[14]='t'; wavHeader[15]=' ';
  wavHeader[16]=16; wavHeader[17]=0; wavHeader[18]=0; wavHeader[19]=0;
  wavHeader[20]=1; wavHeader[21]=0;
  wavHeader[22]=1; wavHeader[23]=0;
  wavHeader[24]=MIC_RATE&0xFF; wavHeader[25]=(MIC_RATE>>8)&0xFF;
  wavHeader[26]=(MIC_RATE>>16)&0xFF; wavHeader[27]=(MIC_RATE>>24)&0xFF;
  wavHeader[28]=byteRate&0xFF; wavHeader[29]=(byteRate>>8)&0xFF;
  wavHeader[30]=(byteRate>>16)&0xFF; wavHeader[31]=(byteRate>>24)&0xFF;
  wavHeader[32]=2; wavHeader[33]=0;
  wavHeader[34]=16; wavHeader[35]=0;
  wavHeader[36]='d'; wavHeader[37]='a'; wavHeader[38]='t'; wavHeader[39]='a';
  wavHeader[40]=dataSize&0xFF; wavHeader[41]=(dataSize>>8)&0xFF;
  wavHeader[42]=(dataSize>>16)&0xFF; wavHeader[43]=(dataSize>>24)&0xFF;

  String boundary = "----PercuSynthBoundary";
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";
  String model = "\r\n--" + boundary + "\r\n";
  model += "Content-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1";
  String language = "\r\n--" + boundary + "\r\n";
  language += "Content-Disposition: form-data; name=\"language\"\r\n\r\nes";
  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t totalLen = head.length() + 44 + dataSize + model.length() + language.length() + tail.length();

  client.println("POST /v1/audio/transcriptions HTTP/1.1");
  client.println("Host: api.openai.com");
  client.println("Authorization: Bearer " + String(OPENAI_API_KEY));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(totalLen));
  client.println("Connection: close");
  client.println();

  client.print(head);
  client.write(wavHeader, 44);

  uint8_t* audioBytes = (uint8_t*)audioBuffer;
  size_t sent = 0;
  while (sent < dataSize) {
    size_t toSend = min((size_t)512, (size_t)(dataSize - sent));
    client.write(audioBytes + sent, toSend);
    sent += toSend;
    yield();
  }
  client.print(model);
  client.print(language);
  client.print(tail);

  unsigned long timeout = millis() + 30000;
  while (!client.available() && millis() < timeout) delay(50);

  String response = "";
  bool bodyStarted = false;
  while (client.available() || client.connected()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (!bodyStarted) { if (line == "\r") bodyStarted = true; }
      else response += line;
    } else {
      delay(10);
      if (!client.available()) break;
    }
  }
  client.stop();

  int textStart = response.indexOf("\"text\":\"");
  if (textStart > 0) {
    textStart += 8;
    int textEnd = response.indexOf("\"", textStart);
    return response.substring(textStart, textEnd);
  }
  return "";
}

String askComposer(String pedido) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, "https://api.openai.com/v1/chat/completions");
  http.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000);

  pedido.replace("\\", "\\\\");
  pedido.replace("\"", "\\\"");
  pedido.replace("\n", " ");
  pedido.replace("\r", "");

  String sys = String(FPSTR(COMPOSER_PROMPT));
  sys.replace("\\", "\\\\");
  sys.replace("\"", "\\\"");
  sys.replace("\n", "\\n");
  sys.replace("\r", "");

  String body = "{";
  body += "\"model\":\"gpt-4o-mini\",";
  body += "\"messages\":[";
  body += "{\"role\":\"system\",\"content\":\"" + sys + "\"},";
  body += "{\"role\":\"user\",\"content\":\"" + pedido + "\"}";
  body += "],\"max_tokens\":700,\"temperature\":0.7}";

  int httpCode = http.POST(body);
  String out = "";

  if (httpCode > 0) {
    String response = http.getString();
    int contentStart = response.indexOf("\"content\":");
    if (contentStart > 0) {
      contentStart = response.indexOf("\"", contentStart + 10);
      if (contentStart > 0) {
        contentStart += 1;
        for (int i = contentStart; i < (int)response.length() - 1; i++) {
          if (response.charAt(i) == '"' && response.charAt(i - 1) != '\\') {
            out = response.substring(contentStart, i);
            break;
          }
        }
        out.replace("\\n", " ");
        out.replace("\\r", "");
        out.replace("\\\"", "\"");
        out.replace("\\\\", "\\");
      }
    }
  }
  http.end();

  int a = out.indexOf('{');
  int b = out.lastIndexOf('}');
  if (a >= 0 && b > a) out = out.substring(a, b + 1);
  else out = "";
  return out;
}

void voiceRequest() {
  bool wasPlaying = playing;
  if (playing) stopSong();

  uiState = UI_REC;  showUiColor(CRGB(120, 0, 0));
  int nSamples = recordAudio();
  uiState = UI_PROC; showUiColor(CRGB(90, 55, 0));

  bool ok = false;
  if (nSamples > 1000 && wifiOK) {
    String pedido = transcribeAudio(nSamples);
    Serial.printf("[VOZ] \"%s\"\n", pedido.c_str());
    if (pedido.length() > 0) {
      String json = askComposer(pedido);
      Serial.printf("[GPT] %s\n", json.c_str());
      if (json.length() > 0 && applySpec(json)) {
        startSong();
        ok = true;
      }
    }
  }
  uiState = UI_NORMAL;
  if (!ok) {
    flashError();
    if (wasPlaying) startSong();
  }
}

// ==============================================================================================================================================
// SETUP / LOOP
// ==============================================================================================================================================
String serialBuf = "";

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[BOOT] compositor_ia v2 — manten BTN1 y pide una cancion; o pega un JSON aqui y Enter.");
  esp_log_level_set("*", ESP_LOG_NONE);

  setCpuFrequencyMhz(240);
  pinMode(BTN_RECORD, INPUT_PULLUP);
  pinMode(BTN_PLAY,   INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i < SEMI_LUT_N; i++)
    semiLUT[i] = powf(2.0f, (float)(i - SEMI_OFFSET) / 12.0f);
  for (int i = 0; i < 256; i++)
    sineLUT[i] = sinf(2.0f * (float)M_PI * (float)i / 256.0f);
  for (int i = 0; i < NUM_VOICES; i++) voices[i].active = false;
  for (int i = 0; i < PERC_VOICES; i++) pvoices[i].active = false;

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.addLeds<WS2812, ONBOARD_PIN, GRB>(onboard, 1);
  FastLED.setBrightness(LED_BRIGHT);
  FastLED.clear();
  FastLED.show();

  audioBuffer = (int16_t*)ps_malloc(MAX_SAMPLES * sizeof(int16_t));
  if (!audioBuffer) audioBuffer = (int16_t*)malloc(MAX_SAMPLES * sizeof(int16_t));

  computeFilter(coefL, cutoffBase, qBase);
  computeFilter(coefR, cutoffBase, qBase);
  i2s_dac_init();
  i2s_mic_init();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 24) { delay(500); attempts++; }
  wifiOK = (WiFi.status() == WL_CONNECTED);
  if (wifiOK) { WiFi.setSleep(true); WiFi.setTxPower(WIFI_POWER_19_5dBm); }
  Serial.printf("[WIFI] %s\n", wifiOK ? "conectado" : "SIN RED (modo Serial/BTN2 disponible)");

  applySpec(String(FPSTR(DEFAULT_SPEC)));
}

void loop() {
  // — BTN1 (mantener): pedido por voz —
  if (audioBuffer && digitalRead(BTN_RECORD) == LOW) {
    delay(40);
    if (digitalRead(BTN_RECORD) == LOW) voiceRequest();
  }

  // — BTN2: PLAY / STOP —
  if (buttonPressed(bPlay)) {
    if (playing) stopSong();
    else         startSong();
  }

  // — Monitor Serie: pegar un JSON => cancion nueva —
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuf.indexOf('{') >= 0) {
        if (applySpec(serialBuf)) { startSong(); Serial.println("[OK] cancion cargada"); }
        else                      { Serial.println("[ERR] JSON invalido"); flashError(); }
      }
      serialBuf = "";
    } else if (serialBuf.length() < 4000) {
      serialBuf += c;
    }
  }

  // — POT1/2/3: mezcla —
  static uint8_t potDiv = 0;
  if (++potDiv >= 4) {
    potDiv = 0;
    float vp = readPot(POT_PAD);  g_padVol  = vp * vp;
    float vm = readPot(POT_MEL);  g_melVol  = vm * vm;
    float vq = readPot(POT_PERC); g_percVol = vq * vq;
  }

  // — Gate ritmico del pad —
  if (playing && padGateSamples > 0) {
    padGateAcc += BUFFER_SAMPLES;
    while (padGateAcc >= padGateSamples) padGateAcc -= padGateSamples;
    float ph = (float)padGateAcc / (float)padGateSamples;
    float shape = 0.5f + 0.5f * cosf(2.0f * (float)M_PI * ph);
    g_padGate = (1.0f - padGateDepth) + padGateDepth * shape;
  } else {
    g_padGate = 1.0f;
  }

  // — LFO de filtro —
  filtLfoPhase += filtLfoRate * (float)BUFFER_SAMPLES / SAMPLE_RATE;
  if (filtLfoPhase >= 1.0f) filtLfoPhase -= 1.0f;
  float lfoL = sinf(2.0f * (float)M_PI * filtLfoPhase);
  float phR  = filtLfoPhase + 0.25f; if (phR >= 1.0f) phR -= 1.0f;
  float lfoR = sinf(2.0f * (float)M_PI * phR);
  g_filtLfoVal = lfoL;
  float cutoffL = cutoffBase + (lfoL * 0.5f + 0.5f) * filtLfoDepth;
  float cutoffR = cutoffBase + (lfoR * 0.5f + 0.5f) * filtLfoDepth;
  BiqCoef targetL, targetR, stepL, stepR;
  computeFilter(targetL, cutoffL, qBase);
  computeFilter(targetR, cutoffR, qBase);
  filterDelta(coefL, targetL, stepL);
  filterDelta(coefR, targetR, stepR);

  // — Energia del pad (LEDs) —
  float vsum = 0.0f;
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind == KIND_PAD) vsum += voices[i].env;
  vsum *= 0.16f;
  if (vsum > g_energy) g_energy = vsum;
  else                 g_energy = g_energy * 0.90f + vsum * 0.10f;

  // — Render de audio —
  int16_t buffer[BUFFER_SAMPLES * 2];

  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    // Melodia por FRASES (gramatica 0 / solo): cuenta muestras hasta el proximo evento
    if (playing && (layMask & LAY_MEL)) {
      int mm = curMelMode();
      if (mm == 0 || mm == 5) {
        if (melSamplesToNext <= 0) melodyStep();
        melSamplesToNext--;
      }
    }

    // GRID unificado (bateria + bajo + comping + melodia-por-grid) con SWING
    if (playing) {
      if (gridToNext <= 0) {
        int fired = gridStep;
        gridStepFire();
        gridToNext += (int32_t)stepDurOf(fired);
      }
      gridToNext--;
    }

    float padL = 0.0f, padR = 0.0f, melL = 0.0f, melR = 0.0f, bsL = 0.0f, bsR = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;

      float dt;
      if (v.kind == KIND_MEL) {
        v.freq += (v.freqTarget - v.freq) * v.glideCoef;
        if (v.vibRamp < 1.0f) { v.vibRamp += 0.00008f; if (v.vibRamp > 1.0f) v.vibRamp = 1.0f; }
        float vrate = melVibR0 + (melVibR1 - melVibR0) * v.vibRamp;
        v.vibPhase += vrate / SAMPLE_RATE;
        if (v.vibPhase >= 1.0f) v.vibPhase -= 1.0f;
        float vib = 1.0f + melVibDepth * v.vibRamp * oscSine(v.vibPhase);
        dt = v.freq * vib / SAMPLE_RATE;
      } else {
        dt = v.freq / SAMPLE_RATE;
      }
      v.phase += dt;
      if (v.phase >= 1.0f) v.phase -= 1.0f;

      float wave = osc(v.phase, dt, v.wave);
      v.lp += v.lpA * (wave - v.lp);
      wave = v.lp;

      if (v.stage == 0) {
        v.env += v.atkInc;
        if (v.env >= 1.0f) { v.env = 1.0f; v.stage = v.afterAtk; }
      } else if (v.stage == 2) {
        v.env *= padReleaseCoef;
        if (v.env < 0.0008f) { v.active = false; v.env = 0.0f; continue; }
      } else if (v.stage == 3) {
        v.env *= v.decCoef;
        if (v.env < 0.0008f) { v.active = false; v.env = 0.0f; continue; }
      }

      float a = wave * v.env * v.gain;
      if      (v.kind == KIND_PAD)  { padL += a * v.lGain; padR += a * v.rGain; }
      else if (v.kind == KIND_BASS) { bsL  += a * v.lGain; bsR  += a * v.rGain; }
      else                          { melL += a * v.lGain; melR += a * v.rGain; }
    }

    float pcL = 0.0f, pcR = 0.0f;
    for (int i = 0; i < PERC_VOICES; i++) {
      PercVoice &p = pvoices[i];
      if (!p.active) continue;
      float x;
      if (p.noiseMix >= 1.0f) {
        x = noiseSample();
      } else {
        p.phase += p.freq / SAMPLE_RATE;
        if (p.phase >= 1.0f) p.phase -= 1.0f;
        if (p.sweepCoef > 0.0f) p.freq += (p.freqEnd - p.freq) * p.sweepCoef;
        x = oscSine(p.phase);
        if (p.noiseMix > 0.0f) x = x * (1.0f - p.noiseMix) + noiseSample() * p.noiseMix;
      }
      p.lp += p.lpA * (x - p.lp);
      float y = p.hipass ? (x - p.lp) : p.lp;
      p.env *= p.decCoef;
      if (p.env < 0.0008f) { p.active = false; continue; }
      float a = y * p.env * p.gain;
      pcL += a * p.lGain; pcR += a * p.rGain;
    }

    filterStep(coefL, stepL);
    filterStep(coefR, stepR);

    // Filtro resonante SOLO al pad; bajo, melodia y percusion van SECOS encima
    float padMixL = padL * padLevel * g_padGate * g_padVol + 1.0e-18f;
    float padMixR = padR * padLevel * g_padGate * g_padVol - 1.0e-18f;
    float fL = applyFilter(bqL, coefL, padMixL);
    float fR = applyFilter(bqR, coefR, padMixR);
    toneL += toneCoef * (fL - toneL);
    toneR += toneCoef * (fR - toneR);

    float vL = (toneL + bsL * bassLevel * g_padVol
                + melL * melLevel * g_melVol + pcL * percLevel * g_percVol) * 0.12f;
    float vR = (toneR + bsR * bassLevel * g_padVol
                + melR * melLevel * g_melVol + pcR * percLevel * g_percVol) * 0.12f;

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
