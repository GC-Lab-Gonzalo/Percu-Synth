// ==============================================================================================================================================
// PERCU-SYNTH — OSCILADOR DE ESCALAS (dron de 4 osciladores, IMU → filtro) — GC Lab Chile
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
// - PSRAM              : OPI PSRAM    (no es obligatorio: el delay vive en RAM interna)
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (incluye driver/i2s_std.h)
// - Wire.h (I2C, incluida en el core) — para el MPU6050
// - FastLED (gestor de librerías de Arduino) — para los 6 LEDs de placa
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Port del "Oscilador 4 escalas" del PROTO-SYNTH V2 (que usaba Mozzi sobre el DAC de
// 8 bits del ESP32) al hardware del PERCU-SYNTH, SIN Mozzi: síntesis propia a
// 44.1 kHz · 16 bits ESTÉREO por I2S hacia el PCM5102.
//
// La idea original se mantiene tal cual: CUATRO POTENCIÓMETROS = CUATRO OSCILADORES,
// cada uno cuantizado a la escala activa. Un control = una cosa, sin paneles ni
// combos. Lo que cambia es TODO lo que hay debajo:
//
//   · 44.1 kHz / 16 bit estéreo (antes ~16 kHz / 8 bit mono por PWM) → sin ruido de fondo
//   · Sierra con PolyBLEP (anti-aliasing real): sin el "chirrido" de la tabla de Mozzi
//   · Cada oscilador es un STACK DE 3 VOCES DESAFINADAS (unísono/ensemble) repartidas
//     en el estéreo con paneo de potencia constante, más un SUB una octava abajo
//   · 5 formas de onda: sierra · cuadrada · pulso 25 % · triangular · seno
//   · Portamento (glide) de 40 ms entre notas + envolvente por oscilador (sin clics)
//   · Filtro pasa-bajos BIQUAD RESONANTE (RBJ) barrido por el EJE X DEL IMU
//     (antes: LDR sobre el filtro de 8 bits de Mozzi)
//   · Saturación (drive) antes del filtro
//   · INTERMITENCIA (el sonido se corta y vuelve) con TAP TEMPO
//   · DELAY ESTÉREO ping-pong sincronizado al tap (corchea con puntillo). El tiempo
//     cambia por CROSSFADE entre dos cabezas de lectura: nunca cambia de tono
//   · Visualizador sobre los 6 LEDs WS2812 de la placa
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// - POT1..POT4 → nota del oscilador 1..4, cuantizada a la escala activa (4 octavas,
//                28 notas). Al mínimo (< 2 % del recorrido) ese oscilador queda EN
//                SILENCIO, igual que en el original.
//
// - BTN1 → ESCALA siguiente (10 escalas, ciclo):
//            Jónico · Dórico · Frigio · Lidio · Mixolidio · Eólico · Locrio ·
//            Frigio dominante (flamenca) · Doble armónica (gitana) · Menor armónica
// - BTN2 → OCTAVA global (ciclo de 4: −1 · 0 · +1 · +2)
// - BTN3 → INTERMITENCIA ON/OFF (el sonido se corta y enciende al tempo)
// - BTN4 → TAP TEMPO de la intermitencia (marca 2 o más golpes; también reengancha la
//          fase, así que el corte cae donde tú marcas). Sin tocarlo: 120 BPM
// - BTN5 → FORMA DE ONDA siguiente (toque) · TÓNICA +1 semitono (mantener > 0.8 s)
//
// - IMU (MPU6050): el EJE X (fijo) abre y cierra el filtro pasa-bajos resonante.
//   Inclina el equipo y el dron respira.
//
// LEDs de la placa: 0..3 = los 4 osciladores (color = nota, brillo = nivel) ·
//                   4 = escala activa (brillo = octava) · 5 = filtro (IMU) + latido del tempo.
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <Wire.h>
#include <FastLED.h>
#include <math.h>

// ─── Tipos (arriba del todo para que el IDE de Arduino genere bien los prototipos) ───
struct BiqState { float x1, x2, y1, y2; };

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
const float INV_SR = 1.0f / (float)SAMPLE_RATE;

// ─── IMU MPU6050 (I2C) ─────────────────────────────────────
#define SDA_PIN     21
#define SCL_PIN     38
#define IMU_ADDR    0x68
const unsigned long IMU_READ_INTERVAL = 10;   // ms entre lecturas
const float IMU_FILTER_ALPHA = 0.08f;         // suavizado de la lectura (0-1)

// ─── LEDs WS2812 (6 SMD internos de la placa) ──────────────
#define LED_PIN        46
#define NUM_LEDS        6
#define LED_BRIGHT     235
#define LED_TYPE       WS2812
#define COLOR_ORDER    GRB
#define ONBOARD_PIN    48
#define ONBOARD_BRIGHT 45
const unsigned long LED_REFRESH_MS = 22;      // ~45 fps

CRGB leds[NUM_LEDS];
CRGB onboard[1];
float flashLevel = 0.0f;      // destello blanco al cambiar escala / octava / onda
float beatPulse  = 0.0f;      // latido del tempo (LED 5)

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
#define BTN1_PIN   44
#define BTN2_PIN   42
#define BTN3_PIN    0
#define BTN4_PIN   45
#define BTN5_PIN   47
const unsigned long DEBOUNCE_MS  = 180;
const unsigned long LONGPRESS_MS = 800;       // BTN5 mantenido → tónica

// ─── Potenciómetros ────────────────────────────────────────
#define POT1   1    // ADC1
#define POT2   2    // ADC2
#define POT3   8    // ADC8
#define POT4  10    // ADC10

// ==============================================================================================================================================
// ESCALAS — las 10 de siempre, guardadas como INTERVALOS (semitonos), no como
// frecuencias fijas: así la tónica y la octava global transponen todo sin tablas extra.
// ==============================================================================================================================================
#define NUM_SCALES  10
#define SCALE_DEG    7
#define NUM_OCTAVES  4
#define NUM_NOTES   (SCALE_DEG * NUM_OCTAVES)   // 28 notas, como el original

const int8_t SCALES[NUM_SCALES][SCALE_DEG] = {
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

int scaleIdx = 3;             // arranca en Lidio (como el original)
int rootSemi = 0;             // 0 = La (BASE_FREQ ya es A1)

// Octava global (BTN2): ciclo de 4 posiciones
const int8_t OCT_STEPS[4] = { -1, 0, 1, 2 };
int octIdx = 1;               // arranca en 0

// ─── Tabla semitono → relación de frecuencia ───────────────
const float BASE_FREQ = 55.0f;    // A1 = grado 0, octava 0 (igual que el original)
#define SEMI_OFFSET 24
#define SEMI_LUT_N  120
float semiLUT[SEMI_LUT_N];

inline float semiToFreq(int semi) {
  int idx = semi + SEMI_OFFSET;
  if (idx < 0) idx = 0; else if (idx >= SEMI_LUT_N) idx = SEMI_LUT_N - 1;
  return BASE_FREQ * semiLUT[idx];
}

// nota (0..27) → frecuencia según escala + tónica + octava global
float noteToFreq(int note) {
  int oct = note / SCALE_DEG;
  int deg = note % SCALE_DEG;
  int semi = rootSemi + OCT_STEPS[octIdx] * 12 + oct * 12 + SCALES[scaleIdx][deg];
  return semiToFreq(semi);
}

// ─── Tabla de seno ─────────────────────────────────────────
float sineLUT[256];
inline float oscSine(float phase) {
  float f = phase * 256.0f;
  int i0 = (int)f; float frac = f - (float)i0;
  i0 &= 255; int i1 = (i0 + 1) & 255;
  return sineLUT[i0] + (sineLUT[i1] - sineLUT[i0]) * frac;
}

// ==============================================================================================================================================
// OSCILADORES — 4 slots (uno por pot), cada uno con 3 voces de unísono + sub
// ==============================================================================================================================================
#define NUM_SLOTS 4
#define UNISON    3

struct Slot {
  int   note;                 // índice en la escala (0..27) · −1 = silencio
  float freqTarget, freqCur;  // Hz (freqCur persigue a freqTarget = glide)
  float amp, ampTarget;       // envolvente del slot (evita clics al entrar/salir)
  float phase[UNISON];
  float subPhase;
  float lg[UNISON], rg[UNISON];   // paneo de potencia constante por voz de unísono
};
Slot slots[NUM_SLOTS];

// Posición base de cada oscilador en el estéreo (llenan el campo de izq. a der.)
const float SLOT_PAN[NUM_SLOTS] = { -0.55f, -0.18f, 0.18f, 0.55f };
float detRatio[UNISON] = { 1.0f, 1.0f, 1.0f };

// ─── Timbre (fijo salvo la forma de onda, BTN5) ────────────
#define WAVE_SAW   0
#define WAVE_SQR   1
#define WAVE_PULSE 2
#define WAVE_TRI   3
#define WAVE_SINE  4
#define WAVE_N     5
int   waveType = WAVE_SAW;              // el original era sierra

const float DETUNE_CENTS = 14.0f;       // desafinación del ensemble
const float SPREAD_WIDTH = 0.60f;       // ancho estéreo del unísono
const float TONE_COEF    = 0.68f;       // tono (one-pole LPF de salida)
const float DRIVE_AMT    = 1.6f;        // saturación antes del filtro
const float MASTER_VOL   = 0.72f;
const float CUTOFF_BASE  = 380.0f;      // piso del cutoff (el IMU suma sobre esto)
const float FILTER_Q     = 2.2f;        // resonancia
const float GLIDE_MS     = 40.0f;       // portamento entre notas
const bool  SUB_OSC      = true;        // sub una octava abajo (cuerpo)

float toneL = 0.0f, toneR = 0.0f;
float glideCoef = 1.0f;
const float AMP_COEF = 1.0f - expf(-1.0f / (0.025f * SAMPLE_RATE));   // ~25 ms, anti-clic
const float DRIVE_COMP = 1.0f / (0.75f + 0.25f * DRIVE_AMT);

// ==============================================================================================================================================
// TEMPO — tap tempo (BTN4) → intermitencia (BTN3) + tiempo del delay
// ==============================================================================================================================================
bool     gateOn   = false;              // BTN3: intermitencia activada
uint32_t tapMs    = 500;                // 120 BPM por defecto
uint32_t gatePeriod = (uint32_t)(0.5f * SAMPLE_RATE);
uint32_t gateHalf   = (uint32_t)(0.25f * SAMPLE_RATE);
uint32_t gateCount  = 0;
float    gateEnv    = 1.0f;             // 1 = abierto · 0 = cortado
const float GATE_COEF = 1.0f - expf(-1.0f / (0.003f * SAMPLE_RATE));  // rampa ~3 ms (sin clic)

unsigned long lastTap = 0;
uint8_t  tapCount = 0;

// ─── Delay estéreo (sincronizado al tap) ───────────────────
// El tiempo cambia con CROSSFADE ENTRE DOS CABEZAS DE LECTURA, no deslizando una sola:
// una cabeza que se mueve es un cambio de tono (efecto cinta), y al marcar el tap eso
// se oía como si el sonido se desafinara. Con el crossfade el eco salta limpio.
#define DELAY_MAX 20000                 // 453 ms @ 44.1 kHz (80 KB en RAM interna)
int16_t dlL[DELAY_MAX];
int16_t dlR[DELAY_MAX];
int   dlIdx = 0;
float delayA = 8820.0f;                 // cabeza activa (muestras)
float delayB = 8820.0f;                 // cabeza anterior (sólo durante el crossfade)
float xfadeAmt = 0.0f;                  // 1 = suena la anterior · 0 = sólo la activa
const float XFADE_INC = 1.0f / (0.030f * SAMPLE_RATE);   // 30 ms
const float DELAY_MIX = 0.26f;
const float DELAY_FB  = 0.36f;
const float DRY_MIX   = 1.0f - DELAY_MIX * 0.35f;
float dampL = 0.0f, dampR = 0.0f;       // amortiguación en la realimentación
const float DAMP_COEF = 0.42f;

// ─── IMU ───────────────────────────────────────────────────
float imu_x = 0.0f;
float filtered_x = 0.0f;
unsigned long lastIMURead = 0;
bool  imuOk = false;
uint8_t imuFails = 0;
unsigned long lastIMURetry = 0;
const unsigned long IMU_RETRY_MS = 3000;      // sin IMU, reintenta cada 3 s (no cada lectura)

// ─── Filtro biquad LPF resonante (estéreo) ─────────────────
float f_b0, f_b1, f_b2, f_a1, f_a2;
BiqState bqL = {0, 0, 0, 0};
BiqState bqR = {0, 0, 0, 0};
float g_cutNorm = 0.0f;      // apertura del filtro 0..1 (para los LEDs)

// ─── Estado de botones ─────────────────────────────────────
bool b1Level = HIGH, b2Level = HIGH, b3Level = HIGH, b4Level = HIGH, b5Level = HIGH;
unsigned long b1Time = 0, b2Time = 0, b3Time = 0, b4Time = 0, b5Time = 0;
bool b5Long = false;

static i2s_chan_handle_t tx_chan;

// ─── Lectura de pot con sobre-muestreo ─────────────────────
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(pin);
  return (float)(sum >> 4) / 4095.0f;
}

// ==============================================================================================================================================
// SÍNTESIS
// ==============================================================================================================================================

// ─── PolyBLEP: corrige el salto de la sierra → sin aliasing ──
inline float polyBlep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  else if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

// Sierra / cuadrada / pulso 25 % / triangular / seno
inline float osc(float phase, float dt) {
  switch (waveType) {
    case WAVE_SAW:
      return (2.0f * phase - 1.0f) - polyBlep(phase, dt);
    case WAVE_SQR: {
      float s1 = (2.0f * phase - 1.0f) - polyBlep(phase, dt);
      float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
      float s2 = (2.0f * p2 - 1.0f) - polyBlep(p2, dt);
      return (s1 - s2) * 0.62f;
    }
    case WAVE_PULSE: {                        // ancho 25 % (más nasal, tipo "reed")
      // saw(p) − saw(p+0.25) ya sale SIN componente continua (media = 0)
      float s1 = (2.0f * phase - 1.0f) - polyBlep(phase, dt);
      float p2 = phase + 0.25f; if (p2 >= 1.0f) p2 -= 1.0f;
      float s2 = (2.0f * p2 - 1.0f) - polyBlep(p2, dt);
      return (s1 - s2) * 0.55f;
    }
    case WAVE_TRI:
      return (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
    default:
      return oscSine(phase);
  }
}

// ─── Unísono: relaciones de desafinación + paneo (se calcula una vez) ──
void initSpread() {
  for (int k = 0; k < UNISON; k++)
    detRatio[k] = powf(2.0f, (((float)k - 1.0f) * DETUNE_CENTS) / 1200.0f);   // −d · 0 · +d

  for (int s = 0; s < NUM_SLOTS; s++) {
    for (int k = 0; k < UNISON; k++) {
      float p = SLOT_PAN[s] + ((float)k - 1.0f) * SPREAD_WIDTH * 0.45f;
      if (p >  1.0f) p =  1.0f;
      if (p < -1.0f) p = -1.0f;
      // Paneo de potencia constante con la tabla de seno: th = (p+1)·90°/2 → fase (p+1)/8
      float phr = (p + 1.0f) * 0.125f;
      slots[s].rg[k] = oscSine(phr);
      slots[s].lg[k] = oscSine(phr + 0.25f);
    }
  }
}

// ─── Cuantizar la posición del pot a una nota de la escala ──
// Con HISTÉRESIS: el ruido del ADC no hace saltar la nota en los bordes.
int quantizeNote(int slot, float v) {
  if (v < 0.02f) return -1;                              // zona de silencio (como el original)
  float x = (v - 0.02f) / 0.98f;
  float f = x * (float)(NUM_NOTES - 1);                  // posición continua 0..27
  int cand = (int)(f + 0.5f);
  if (cand < 0) cand = 0;
  if (cand > NUM_NOTES - 1) cand = NUM_NOTES - 1;
  int cur = slots[slot].note;
  if (cur >= 0 && cand != cur && fabsf(f - (float)cur) < 0.65f) cand = cur;
  return cand;
}

// ─── Aplicar una nota nueva a un slot ──────────────────────
void setSlotNote(int s, int note) {
  Slot &S = slots[s];
  S.note = note;
  if (note < 0) { S.ampTarget = 0.0f; return; }
  S.freqTarget = noteToFreq(note);
  // Si el slot venía en silencio, no hay que "deslizar" desde la nota anterior.
  if (S.amp < 0.02f) S.freqCur = S.freqTarget;
  S.ampTarget = 1.0f;
}

// ─── Re-afinar todos los slots (cambio de escala / tónica / octava) ──
void retuneAll() {
  for (int s = 0; s < NUM_SLOTS; s++)
    if (slots[s].note >= 0) slots[s].freqTarget = noteToFreq(slots[s].note);
}

// ==============================================================================================================================================
// TEMPO
// ==============================================================================================================================================
// El tempo manda dos cosas: el corte de la intermitencia y el tiempo del delay
// (corchea con puntillo = 3/4 del pulso; si no cabe en la línea, se va dividiendo).
void applyTempo() {
  gatePeriod = (uint32_t)((float)tapMs * 0.001f * SAMPLE_RATE);
  if (gatePeriod < 1000) gatePeriod = 1000;              // tope de seguridad (~23 ms)
  gateHalf = gatePeriod >> 1;

  float d = (float)gatePeriod * 0.75f;
  while (d > (float)(DELAY_MAX - 2)) d *= 0.5f;
  if (d < 1764.0f) d = 1764.0f;                          // mínimo 40 ms

  // Sólo si el cambio es audible (> 32 muestras): la cabeza vieja se va en 30 ms
  // mientras entra la nueva → el tiempo cambia SIN barrido de tono.
  if (fabsf(d - delayA) > 32.0f) {
    delayB   = delayA;
    delayA   = d;
    xfadeAmt = 1.0f;
  }
}

// Cada toque de BTN4: mide el intervalo, promedia y REENGANCHA la fase del corte.
void tapTempo() {
  unsigned long t = millis();
  unsigned long dt = t - lastTap;

  if (lastTap != 0 && dt > 120 && dt < 3000) {
    // Serie en curso → promedio suave (el golpe nuevo pesa el doble)
    tapMs = (tapCount >= 1) ? (uint32_t)((tapMs + 2UL * dt) / 3UL) : (uint32_t)dt;
    tapCount++;
    applyTempo();
  } else {
    tapCount = 0;                                        // primer golpe de una serie nueva
  }
  lastTap = t;

  gateCount = 0;                                         // el corte cae donde tú marcas
  beatPulse = 1.0f;
}

// ==============================================================================================================================================
// IMU — eje X fijo → filtro
// ==============================================================================================================================================
// Nota: el WHO_AM_I es sólo informativo (algunos clones devuelven 0x68/0x70/0x72/0x98).
// Lo que manda es que la lectura de datos funcione; si falla varias veces seguidas se
// re-inicializa sola. warm = true → reintento en caliente, con esperas mínimas para no
// vaciar el DMA de audio mientras suena.
bool initIMU(bool warm) {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);   // despertar (PWR_MGMT_1 = 0)
  bool ack = (Wire.endTransmission(true) == 0);
  delay(warm ? 8 : 60);

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x1C); Wire.write(0x00);   // ACCEL_CONFIG → ±2g (máxima sensibilidad)
  Wire.endTransmission(true);

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x1A); Wire.write(0x03);   // DLPF ~44 Hz → lectura estable
  Wire.endTransmission(true);
  if (!warm) delay(20);
  return ack;
}

void readIMU() {
  unsigned long t = millis();
  if (t - lastIMURead < IMU_READ_INTERVAL) return;
  lastIMURead = t;

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, 2, true);      // sólo ACCEL_XOUT (el eje que usamos)

  if (Wire.available() >= 2) {
    int16_t rx = (Wire.read() << 8) | Wire.read();
    imu_x = rx / 16384.0f;
    filtered_x = filtered_x * (1.0f - IMU_FILTER_ALPHA) + imu_x * IMU_FILTER_ALPHA;
    imuOk = true;
    imuFails = 0;
  } else {
    if (++imuFails > 20) {
      imuFails = 0;
      imuOk = false;
      if (t - lastIMURetry > IMU_RETRY_MS) { lastIMURetry = t; imuOk = initIMU(true); }
    }
  }
}

// ─── Filtro: el eje X del IMU abre/cierra el pasa-bajos ────
void updateFilter() {
  float axis = fabsf(filtered_x);
  if (axis > 1.0f) axis = 1.0f;

  float cutoff = CUTOFF_BASE + axis * 8200.0f;
  if (cutoff > 13000.0f) cutoff = 13000.0f;
  g_cutNorm = (cutoff - CUTOFF_BASE) / (13000.0f - CUTOFF_BASE);

  float omega = 2.0f * (float)M_PI * cutoff / SAMPLE_RATE;
  float s = sinf(omega), c = cosf(omega);
  float alpha = s / (2.0f * FILTER_Q);

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

// ─── Leer la línea de delay en una posición (interpolación lineal) ──
inline float readDelay(const int16_t *line, float dly) {
  float rp = (float)dlIdx - dly;
  while (rp < 0.0f) rp += (float)DELAY_MAX;
  int i0 = (int)rp;
  float fr = rp - (float)i0;
  if (i0 >= DELAY_MAX) i0 -= DELAY_MAX;
  int i1 = i0 + 1; if (i1 >= DELAY_MAX) i1 = 0;
  return (line[i0] + (line[i1] - line[i0]) * fr) * (1.0f / 32768.0f);
}

// ─── Saturación suave (aproximación racional de tanh) ──────
inline float softClip(float x) {
  if (x >  3.0f) x =  3.0f;
  if (x < -3.0f) x = -3.0f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

// ==============================================================================================================================================
// LEDs
// ==============================================================================================================================================
// 0..3 = los 4 osciladores (color = nota, brillo = nivel)
// 4    = escala activa (color = escala, brillo = octava)
// 5    = filtro (IMU) + latido del tempo (fuerte si la intermitencia está activa)
void renderLEDs() {
  unsigned long t = millis();
  static unsigned long lastFrame = 0;
  if (t - lastFrame < LED_REFRESH_MS) return;
  lastFrame = t;

  for (int s = 0; s < NUM_SLOTS; s++) {
    Slot &S = slots[s];
    if (S.note < 0 && S.amp < 0.02f) {
      leds[s] = CHSV(140, 200, 6);                   // apagado casi total = silencio
    } else {
      uint8_t hue = (uint8_t)(10 + S.note * 8);      // recorre el arcoíris con la nota
      uint8_t val = 18 + (uint8_t)(S.amp * 215.0f);
      leds[s] = CHSV(hue, 245, val);
    }
  }

  // LED 4 → escala (color) + octava (brillo)
  const uint8_t OCT_VAL[4] = { 45, 80, 135, 195 };
  leds[4] = CHSV((uint8_t)(scaleIdx * 25), 255, OCT_VAL[octIdx]);

  // LED 5 → apertura del filtro (IMU) + latido del tempo
  leds[5] = CHSV(140 + (uint8_t)(g_cutNorm * 45.0f), 235,
                 30 + (uint8_t)(g_cutNorm * 200.0f));
  if (beatPulse > 0.02f) {
    uint8_t p = (uint8_t)(beatPulse * (gateOn ? 220.0f : 90.0f));
    leds[5] += CRGB(p, p, p);
  }
  beatPulse *= 0.55f;

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

// ==============================================================================================================================================
// SETUP
// ==============================================================================================================================================
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  chan_cfg.dma_desc_num  = 8;
  chan_cfg.dma_frame_num = 240;      // ≈ 43 ms de colchón (es un dron: latencia irrelevante)
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

  for (int s = 0; s < NUM_SLOTS; s++) {
    slots[s].note = -1;
    slots[s].freqTarget = slots[s].freqCur = 110.0f;
    slots[s].amp = slots[s].ampTarget = 0.0f;
    slots[s].subPhase = 0.0f;
    for (int k = 0; k < UNISON; k++)      // fases decorreladas: el unísono nace "ancho"
      slots[s].phase[k] = fmodf(0.61803f * (float)(s * UNISON + k), 1.0f);
  }
  initSpread();
  glideCoef = 1.0f - expf(-1.0f / (GLIDE_MS * 0.001f * SAMPLE_RATE));
  applyTempo();

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, ONBOARD_PIN, COLOR_ORDER>(onboard, 1);
  FastLED.setBrightness(LED_BRIGHT);
  FastLED.clear();
  FastLED.show();

  imuOk = initIMU(false);
  delay(30);
  readIMU();
  updateFilter();

  i2s_init();
}

// ==============================================================================================================================================
// LOOP
// ==============================================================================================================================================
void loop() {
  unsigned long tms = millis();

  // ── Botones: uno por función, sin combos ni paneles ──
  bool b1 = digitalRead(BTN1_PIN);
  bool b2 = digitalRead(BTN2_PIN);
  bool b3 = digitalRead(BTN3_PIN);
  bool b4 = digitalRead(BTN4_PIN);
  bool b5 = digitalRead(BTN5_PIN);

  // BTN1 → escala siguiente (10)
  if (b1 == LOW && b1Level == HIGH && (tms - b1Time) > DEBOUNCE_MS) {
    b1Time = tms;
    scaleIdx = (scaleIdx + 1) % NUM_SCALES;
    retuneAll(); flashLevel = 0.8f;
  }
  // BTN2 → octava global (ciclo de 4)
  if (b2 == LOW && b2Level == HIGH && (tms - b2Time) > DEBOUNCE_MS) {
    b2Time = tms;
    octIdx = (octIdx + 1) & 3;
    retuneAll(); flashLevel = 0.6f;
  }
  // BTN3 → intermitencia ON/OFF
  if (b3 == LOW && b3Level == HIGH && (tms - b3Time) > DEBOUNCE_MS) {
    b3Time = tms;
    gateOn = !gateOn;
    if (gateOn) gateCount = 0;                 // arranca abierto, en el pulso
    flashLevel = 0.5f;
  }
  // BTN4 → tap tempo
  if (b4 == LOW && b4Level == HIGH && (tms - b4Time) > 100) {
    b4Time = tms;
    tapTempo();
  }
  // BTN5 → toque: forma de onda · mantenido > 0.8 s: tónica +1 semitono
  if (b5 == LOW && b5Level == HIGH && (tms - b5Time) > DEBOUNCE_MS) {
    b5Time = tms; b5Long = false;
  }
  if (b5 == LOW && !b5Long && (tms - b5Time) > LONGPRESS_MS) {
    b5Long = true;                             // se dispara al cumplirse el tiempo
    rootSemi = (rootSemi + 1) % 12;
    retuneAll(); flashLevel = 1.0f;
  }
  if (b5 == HIGH && b5Level == LOW && !b5Long && (tms - b5Time) < LONGPRESS_MS) {
    waveType = (waveType + 1) % WAVE_N;
    flashLevel = 0.6f;
  }

  b1Level = b1; b2Level = b2; b3Level = b3; b4Level = b4; b5Level = b5;

  // ── IMU → filtro ──
  readIMU();
  updateFilter();

  // ── Pots: uno por buffer en rotación (los 4 son notas, siempre activos) ──
  static const uint8_t POT_PIN[4] = { POT1, POT2, POT3, POT4 };
  static uint8_t potScan = 0;
  int pi = potScan; potScan = (potScan + 1) & 3;
  int n = quantizeNote(pi, readPot(POT_PIN[pi]));
  if (n != slots[pi].note) setSlotNote(pi, n);

  // ── Buffer de audio (estéreo) ──
  int16_t buffer[BUFFER_SAMPLES * 2];

  for (int i = 0; i < BUFFER_SAMPLES; i++) {
    float mixL = 0.0f, mixR = 0.0f;

    for (int s = 0; s < NUM_SLOTS; s++) {
      Slot &S = slots[s];

      // Glide y envolvente (suavizados por muestra → nada de saltos ni clics)
      S.freqCur += (S.freqTarget - S.freqCur) * glideCoef;
      S.amp     += (S.ampTarget  - S.amp)     * AMP_COEF;
      if (S.amp < 0.0004f) { S.amp = 0.0f; continue; }      // slot mudo: no gasta CPU

      // 3 voces de unísono desafinadas y repartidas en el estéreo
      for (int k = 0; k < UNISON; k++) {
        float dt = S.freqCur * detRatio[k] * INV_SR;
        S.phase[k] += dt;
        if (S.phase[k] >= 1.0f) S.phase[k] -= 1.0f;
        float a = osc(S.phase[k], dt) * S.amp * 0.30f;
        mixL += a * S.lg[k];
        mixR += a * S.rg[k];
      }

      // Sub-oscilador (seno una octava abajo, al centro): cuerpo de bajo
      if (SUB_OSC) {
        S.subPhase += S.freqCur * 0.5f * INV_SR;
        if (S.subPhase >= 1.0f) S.subPhase -= 1.0f;
        float a = oscSine(S.subPhase) * S.amp * 0.40f * 0.7071f;
        mixL += a; mixR += a;
      }
    }

    mixL *= 0.22f; mixR *= 0.22f;
    mixL += 1.0e-18f;                       // anti-denormal
    mixR -= 1.0e-18f;

    // Drive → filtro resonante (IMU) → tono
    float dL = softClip(mixL * DRIVE_AMT) * DRIVE_COMP;
    float dR = softClip(mixR * DRIVE_AMT) * DRIVE_COMP;

    float fL = applyFilter(bqL, dL);
    float fR = applyFilter(bqR, dR);

    toneL += TONE_COEF * (fL - toneL);
    toneR += TONE_COEF * (fR - toneR);

    // ── INTERMITENCIA: el reloj corre SIEMPRE (así el tap sigue vivo aunque
    //    esté apagada); cuando está activa, corta media parte de cada pulso.
    //    Rampa de ~3 ms en cada flanco → se corta seco pero sin chasquido. ──
    gateCount++;
    if (gateCount >= gatePeriod) { gateCount = 0; beatPulse = 1.0f; }
    float gTarget = (!gateOn || gateCount < gateHalf) ? 1.0f : 0.0f;
    gateEnv += (gTarget - gateEnv) * GATE_COEF;

    float gL = toneL * gateEnv;
    float gR = toneR * gateEnv;

    // ── Delay estéreo (después del corte: las colas siguen sonando en los
    //    silencios, que es lo que hace que la intermitencia respire) ──
    // Las dos cabezas están QUIETAS; al cambiar el tempo se hace un crossfade de
    // 30 ms entre ellas. Ninguna se mueve ⇒ el eco no cambia de tono nunca.
    float eL = readDelay(dlL, delayA);
    float eR = readDelay(dlR, delayA);
    if (xfadeAmt > 0.0f) {
      eL += (readDelay(dlL, delayB) - eL) * xfadeAmt;
      eR += (readDelay(dlR, delayB) - eR) * xfadeAmt;
      xfadeAmt -= XFADE_INC;
      if (xfadeAmt < 0.0f) xfadeAmt = 0.0f;
    }

    // Amortiguación: cada repetición sale más oscura (como una cinta)
    dampL += DAMP_COEF * (eL - dampL);
    dampR += DAMP_COEF * (eR - dampR);

    float wL = gL + dampR * DELAY_FB;        // ping-pong: la cola cruza de lado
    float wR = gR + dampL * DELAY_FB;
    if (wL >  1.0f) wL =  1.0f; if (wL < -1.0f) wL = -1.0f;
    if (wR >  1.0f) wR =  1.0f; if (wR < -1.0f) wR = -1.0f;

    dlL[dlIdx] = (int16_t)(wL * 32767.0f);
    dlR[dlIdx] = (int16_t)(wR * 32767.0f);
    dlIdx++; if (dlIdx >= DELAY_MAX) dlIdx = 0;

    float outL = gL * DRY_MIX + eL * DELAY_MIX;
    float outR = gR * DRY_MIX + eR * DELAY_MIX;

    // Limitador de seguridad + escalado a 16 bits (con tope duro: nunca desborda)
    float vL = softClip(outL * MASTER_VOL) * 30000.0f;
    float vR = softClip(outR * MASTER_VOL) * 30000.0f;
    if (vL >  32000.0f) vL =  32000.0f; if (vL < -32000.0f) vL = -32000.0f;
    if (vR >  32000.0f) vR =  32000.0f; if (vR < -32000.0f) vR = -32000.0f;

    buffer[i * 2]     = (int16_t)vL;
    buffer[i * 2 + 1] = (int16_t)vR;
  }

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);

  renderLEDs();
}
