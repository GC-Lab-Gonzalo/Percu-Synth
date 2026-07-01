// ==============================================================================================================================================
// PERCU-SYNTH — Secuenciador de Trance Polifónico (IMU) + 6 LEDs de placa — GC Lab Chile
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
// - 5 Botones con pull-up |BTN1 -> 44 (Play), BTN2 -> 42 (Escala), BTN3 -> 0 (Patrón), BTN4 -> 45 (Largo), BTN5 -> 47 (Octava)|
// - 4 Potenciómetros analógicos |POT1 -> ADC1 (Ataque), POT2 -> ADC2 (Volumen), POT3 -> ADC8 (Tempo), POT4 -> ADC10 (Decay)|
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
// Igual que `trance_imu` (motor de audio I2S → PCM5102 a 44.1 kHz / 16-bit estéreo,
// polifónico de 16 voces, osciladores anti-aliasing PolyBLEP, filtro biquad
// resonante controlado por el IMU y soft-limiter), pero AHORA con un visualizador
// reactivo sobre los 6 LEDs WS2812 internos de la placa (los que `test_leds` dejaba
// apagados a propósito).
//
// El render de LEDs corre en el MISMO loop() que el audio, así que NO hay
// concurrencia: el único cuidado es throttlear FastLED.show() para no robarle
// tiempo a los buffers de I2S. FastLED en el ESP32-S3 usa el periférico RMT (no el
// I2S), por lo que NO choca con la salida de audio. Como son sólo 6 LEDs, cada
// refresco transmite en ~180 µs por hardware → impacto despreciable.
// ==============================================================================================================================================
// FUNCIONAMIENTO (LEDs)
// ==============================================================================================================================================
// Los 6 LEDs de la placa muestran 4 cosas a la vez:
//   1. PALETA = PANEL ACTIVO (también sirve de indicador de panel):
//        · Panel A (normal)         → cian/azul (marca GC Lab)
//        · Panel B (notas/tonalidad)→ violeta
//        · Panel C (timbre/síntesis)→ naranja
//   2. NIVEL (barra/VU): cuántos LEDs encienden = energía polifónica (suma de las
//      envolventes de las voces activas). Más acorde/cola = barra más llena.
//   3. COLOR fino: el filtro del IMU desplaza el tono (filter sweep ↔ color sweep).
//   4. BEAT: cada paso con nota da un destello; el downbeat (pasos 0/4/8/12) pega
//      más fuerte. Detenido (Stop) → respiración suave en el color del panel.
//   Al cambiar de panel, un flash breve confirma el cambio.
//
// El resto de controles (botones / pots / paneles / IMU) es IDÉNTICO a trance_imu.
// ==============================================================================================================================================
// FUNCIONAMIENTO (controles — igual que trance_imu)
// ==============================================================================================================================================
// TRES PANELES DE CONTROL (combos de cambio de panel; el mismo combo te devuelve a A):
//   · BTN1 + BTN5 a la vez → Panel B (notas / tonalidad)
//   · BTN2 + BTN4 a la vez → Panel C (timbre / síntesis)
//
// ── PANEL A (normal) ──────────────────────────────────────────────────────────
// - BTN1 (44) → Play / Stop  (al detener, las voces se apagan con su cola, sin clicks)
// - BTN2 (42) → Cambiar escala  (Mayor · Menor · Armónica · Árabe · Lidia · Frigia)
// - BTN3 (0)  → Cambiar patrón  (16 patrones)
// - BTN4 (45) → Cambiar largo de secuencia  (4 → 8 → 16 pasos)
// - BTN5 (47) → Octava  (-1 → 0 → +1 → +2 respecto a C3)
// - POT1 (ADC1)  → Ataque · POT2 (ADC2) → Volumen · POT3 (ADC8) → Tempo · POT4 (ADC10) → Decay
//
// ── PANEL B (notas / tonalidad) — BTN1+BTN5 ───────────────────────────────────
// - BTN1 → F# (+7) · BTN2 → C# (+2) · BTN3 → B/tónica (0) · BTN4 → D# (+4) · BTN5 → G# (+9)
// - POT1..POT4 → grado de escala (0–7) de cada una de las 4 voces del acorde (persiste)
//
// ── PANEL C (timbre / síntesis) — BTN2+BTN4 ───────────────────────────────────
// - BTN1 → forma de onda · BTN2 → octava arriba · BTN3 → unison/poly · BTN4 → quinta · BTN5 → sub-osc
// - POT1 → detune · POT2 → drive · POT3 → piso de cutoff · POT4 → resonancia base
//
// CONGELADO DE CONTROLES: al cambiar de panel los pots quedan congelados y sólo retoman
// el control cuando se MUEVEN (≥ 2 %). La nota/tonalidad y el timbre PERSISTEN.
//
// - IMU (MPU6050): aceleración X → cutoff del LPF · aceleración Y → resonancia (Q).
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <Wire.h>
#include <FastLED.h>
#include <math.h>

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
#define LED_BRIGHT     250     // 0-255 — bajo para no sobrecargar la fuente
#define LED_TYPE       WS2812
#define COLOR_ORDER    GRB
const unsigned long LED_REFRESH_MS = 22;  // refresco del visualizador (~45 fps)

CRGB leds[NUM_LEDS];

// Métricas compartidas audio → LEDs (todo en el mismo hilo: sin volatile/locks)
float g_energy   = 0.0f;   // energía polifónica suavizada (0..~1)
bool  g_stepFlash = false;  // un paso disparó nota desde el último render
bool  g_downbeat  = false;  // ese paso fue fuerte (múltiplo de 4)
float flashLevel  = 0.0f;   // intensidad del destello de beat (decae en cada render)

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
#define BTN_PLAY     44
#define BTN_SCALE    42
#define BTN_PATTERN   0
#define BTN_LENGTH   45
#define BTN_OCTAVE   47
const unsigned long DEBOUNCE_MS = 200;

// Estado de botón con anti-rebote por flanco (declarado arriba para que el
// auto-prototipo de Arduino vea el tipo antes de buttonPressed()/panelHue()).
struct BtnState { uint8_t pin; bool last; unsigned long lastPress; };
BtnState bPattern = {BTN_PATTERN, HIGH, 0};   // BTN3 (único que no participa en combos)

// ─── Potenciómetros ────────────────────────────────────────
#define POT_ATTACK   1    // ADC1  → ataque
#define POT_VOLUME   2    // ADC2  → volumen master
#define POT_TEMPO    8    // ADC8  → tempo (BPM)
#define POT_DECAY   10    // ADC10 → decay/release

// ─── Polifonía ─────────────────────────────────────────────
#define NUM_VOICES   16

struct Voice {
  bool     active;
  float    freq;     // Hz
  float    phase;    // 0..1
  float    env;      // amplitud actual 0..1
  uint8_t  stage;    // 0 = attack, 1 = decay
  uint32_t age;      // para robo de voz (la más vieja se reemplaza)
};

Voice voices[NUM_VOICES];
uint32_t voiceCounter = 0;   // sello de tiempo incremental para el robo de voz

// ─── Estado del secuenciador ───────────────────────────────
bool  isPlaying      = false;
int   currentStep    = 0;
int   sequenceLength = 16;
int   currentPattern = 0;
int   currentScale   = 0;
int   octaveIndex    = 1;                  // índice dentro de OCTAVE_SEMITONES
const int OCTAVE_SEMITONES[4] = {-12, 0, 12, 24};
const float BASE_FREQ = 130.81f;           // C3

// ─── Paneles de control ────────────────────────────────────
// A = normal · B (BTN1+BTN5) = notas/tonalidad · C (BTN2+BTN4) = timbre/síntesis.
#define PANEL_A 0
#define PANEL_B 1
#define PANEL_C 2
int   panel          = PANEL_A;
int   rootSemi       = 0;                  // transposición de la tónica en semitonos (0 = tónica)
// Voicing del acorde (grado de escala de cada voz). PERSISTENTE.
int   chordOffset[4] = {0, 2, 4, 7};

// Cada botón (BTN1..BTN5) fija una transposición de tónica en semitonos.
const int BTN_NOTE[5] = {7, 2, 0, 4, 9};

// Paleta por panel (hue FastLED 0-255): A=cian/azul · B=violeta · C=naranja
inline uint8_t panelHue() {
  return (panel == PANEL_A) ? 140 : (panel == PANEL_B ? 192 : 24);
}

// Cambio de panel por COMBOS: BTN1+BTN5 → Panel B · BTN2+BTN4 → Panel C
bool b1Level = HIGH, b2Level = HIGH, b4Level = HIGH, b5Level = HIGH;
bool combo15 = false;   // BTN1+BTN5 en curso (consumido hasta soltar ambos)
bool combo24 = false;   // BTN2+BTN4 en curso

// ─── Panel C (timbre / síntesis) ───────────────────────────
int   waveType    = 0;        // 0 = sierra · 1 = cuadrada · 2 = triangular
bool  unison      = false;    // true = todas las voces sobre la fundamental
bool  subOsc      = false;    // capa una octava abajo
bool  octUp       = false;    // capa una octava arriba
bool  fifthLayer  = false;    // capa de quinta justa (+7)
float detuneCents = 6.0f;     // desafinación entre voces (POT1)
float driveAmt    = 1.3f;     // saturación de salida (POT2)
float cutoffBase  = 200.0f;   // piso de cutoff del filtro en Hz (POT3)
float qBase       = 0.7f;     // resonancia base (Q) del filtro (POT4)

// Pots: congelados al cambiar de panel; sólo retoman control cuando se MUEVEN.
float       potAnchor[4] = {0, 0, 0, 0};
bool        potLive[4]   = {true, true, true, true};
bool        panelChanged = false;
const float POT_MOVE_THR = 0.02f;          // 2 % del recorrido para considerar "movido"

uint32_t sampleInStep   = 0;               // muestras transcurridas en el paso actual
uint32_t samplesPerStep = 11025;           // recalculado cada buffer según el tempo

// ─── Parámetros de envolvente (globales, recalculados por buffer) ───
float g_volume    = 0.5f;
float attackInc   = 0.01f;                 // incremento de envolvente por muestra (attack)
float decayCoef   = 0.9999f;               // multiplicador por muestra (decay exponencial)

// ─── IMU ───────────────────────────────────────────────────
float imu_x = 0.0f, imu_y = 0.0f;
float filtered_x = 0.0f, filtered_y = 0.0f;
unsigned long lastIMURead = 0;

// ─── Filtro biquad paso-bajo resonante (controlado por IMU) ──
float f_b0, f_b1, f_b2, f_a1, f_a2;        // coeficientes normalizados
float f_x1 = 0, f_x2 = 0, f_y1 = 0, f_y2 = 0;

// ─── Escalas musicales (semitonos) ─────────────────────────
const int scales[][8] = {
  {0, 2, 4, 5, 7, 9, 11, 12},  // Mayor
  {0, 2, 3, 5, 7, 8, 10, 12},  // Menor natural
  {0, 2, 3, 5, 7, 8, 11, 12},  // Menor armónica
  {0, 1, 3, 4, 6, 8, 10, 12},  // Árabe
  {0, 2, 4, 6, 7, 9, 11, 12},  // Lidia
  {0, 1, 4, 5, 7, 8, 11, 12}   // Frigia
};
const int numScales = 6;

// ─── Patrones de secuencia (0 = silencio, 1-7 = grado de la escala) ──
const int patterns[][16] = {
  {1, 0, 3, 0, 5, 0, 3, 0, 1, 0, 3, 0, 5, 0, 7, 0}, // Trance básico
  {1, 0, 0, 0, 5, 0, 0, 0, 3, 0, 0, 0, 7, 0, 0, 0}, // Espaciado
  {1, 3, 5, 7, 5, 3, 1, 0, 1, 3, 5, 7, 5, 3, 1, 0}, // Ascendente
  {7, 5, 3, 1, 3, 5, 7, 0, 7, 5, 3, 1, 3, 5, 7, 0}, // Descendente
  {1, 1, 5, 5, 3, 3, 7, 0, 1, 1, 5, 5, 3, 3, 7, 0}, // Repetitivo
  {1, 0, 5, 3, 0, 7, 1, 0, 5, 0, 3, 7, 0, 1, 5, 0}, // Sincopado
  {1, 3, 0, 5, 7, 0, 3, 1, 5, 7, 0, 1, 3, 0, 5, 7}, // Complejo
  {1, 0, 0, 5, 0, 0, 1, 5, 0, 3, 0, 7, 0, 1, 0, 0}, // Breakbeat
  {7, 7, 5, 5, 3, 3, 1, 1, 7, 7, 5, 5, 3, 3, 1, 1}, // Dobles
  {1, 5, 1, 7, 1, 3, 1, 5, 1, 7, 1, 3, 1, 5, 1, 0}, // Bajo constante
  {0, 1, 0, 3, 0, 5, 0, 7, 0, 5, 0, 3, 0, 1, 0, 0}, // Off-beat
  {1, 3, 5, 1, 7, 5, 3, 7, 1, 3, 5, 1, 7, 5, 3, 0}, // Arpegio
  {5, 0, 5, 0, 1, 0, 1, 0, 3, 0, 3, 0, 7, 0, 7, 0}, // Alternado
  {1, 7, 1, 5, 1, 3, 1, 7, 5, 7, 5, 3, 5, 1, 5, 0}, // Saltos
  {3, 3, 3, 5, 5, 5, 1, 0, 7, 7, 7, 5, 5, 5, 3, 0}, // Grupos
  {1, 0, 7, 0, 5, 0, 3, 0, 5, 0, 7, 0, 1, 0, 0, 0}  // Clásico
};
const int numPatterns = 16;

static i2s_chan_handle_t tx_chan;

// ─── Lectura de pot con sobre-muestreo (anti-ruido) ────────
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (float)(sum >> 3) / 4095.0f;
}

// ─── Detección de flanco de bajada con anti-rebote ─────────
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

// ─── Semitono de un grado de la escala (con envoltura por octava) ──
int scaleSemitone(int degree) {
  int oct = 0;
  while (degree >= 7) { degree -= 7; oct += 12; }   // 7 grados únicos por octava
  while (degree < 0)  { degree += 7; oct -= 12; }
  return scales[currentScale][degree] + oct;
}

// ─── Disparar una voz (busca libre o roba la más vieja) ────
void triggerVoice(float freq) {
  int idx = -1;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (!voices[i].active) { idx = i; break; }
  }
  if (idx < 0) {  // ninguna libre → robar la más vieja
    uint32_t oldest = 0xFFFFFFFF;
    idx = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
      if (voices[i].age < oldest) { oldest = voices[i].age; idx = i; }
    }
  }
  voices[idx].active = true;
  voices[idx].freq   = freq;
  voices[idx].phase  = 0.0f;
  voices[idx].env    = 0.0f;
  voices[idx].stage  = 0;            // attack
  voices[idx].age    = voiceCounter++;
}

// ─── Frecuencia de un grado de escala (con transposición global) ──
inline float degreeFreq(int degree, int octSemi) {
  int semi = scaleSemitone(degree) + octSemi;
  return BASE_FREQ * powf(2.0f, semi / 12.0f);
}

// ─── Disparar el acorde del paso actual ────────────────────
void triggerStep() {
  int noteValue = patterns[currentPattern][currentStep];
  if (noteValue == 0) return;                       // silencio
  int base    = noteValue - 1;                      // grado 0..6
  int octSemi = OCTAVE_SEMITONES[octaveIndex] + rootSemi;

  // Detune simétrico por voz (en cents → multiplicador de frecuencia)
  static const float det[4] = {-1.5f, -0.5f, 0.5f, 1.5f};

  for (int c = 0; c < 4; c++) {
    int degree = unison ? base : (base + chordOffset[c]);   // unison: todas en la fundamental
    float f = degreeFreq(degree, octSemi);
    f *= powf(2.0f, (det[c] * detuneCents) / 1200.0f);      // desafinación
    triggerVoice(f);
  }

  if (subOsc)     triggerVoice(degreeFreq(base, octSemi - 12)); // capa octava abajo
  if (octUp)      triggerVoice(degreeFreq(base, octSemi + 12)); // capa octava arriba
  if (fifthLayer) {                                              // capa de quinta justa (+7 semitonos)
    int semi = scaleSemitone(base) + octSemi + 7;
    triggerVoice(BASE_FREQ * powf(2.0f, semi / 12.0f));
  }

  // Aviso para los LEDs: este paso disparó nota (downbeat = pasos 0/4/8/12).
  g_stepFlash = true;
  g_downbeat  = (currentStep % 4 == 0);
}

// ─── PolyBLEP: corrección anti-aliasing en las discontinuidades ──
inline float polyBlep(float t, float dt) {
  if (t < dt) {
    t /= dt;
    return t + t - t * t - 1.0f;
  } else if (t > 1.0f - dt) {
    t = (t - 1.0f) / dt;
    return t * t + t + t + 1.0f;
  }
  return 0.0f;
}

// ─── Oscilador: forma de onda según waveType (sierra/cuadrada/triangular) ──
inline float osc(float phase, float dt) {
  if (waveType == 0) {                       // SIERRA (anti-aliasing)
    return (2.0f * phase - 1.0f) - polyBlep(phase, dt);
  } else if (waveType == 1) {                // CUADRADA 50 % = diferencia de 2 sierras
    float saw1 = (2.0f * phase - 1.0f) - polyBlep(phase, dt);
    float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
    float saw2 = (2.0f * p2 - 1.0f) - polyBlep(p2, dt);
    return (saw1 - saw2) * 0.6f;             // 0.6: iguala el nivel a la sierra
  } else {                                    // TRIANGULAR (armónicos suaves)
    return (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
  }
}

// ─── IMU: inicializar MPU6050 ──────────────────────────────
void initIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B); Wire.write(0);     // PWR_MGMT_1 = 0 → despertar
  Wire.endTransmission(true);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x1C); Wire.write(0x00);  // ACCEL_CONFIG → ±2g
  Wire.endTransmission(true);
  delay(100);
}

// ─── IMU: leer aceleración (throttle a IMU_READ_INTERVAL) ──
void readIMU() {
  unsigned long t = millis();
  if (t - lastIMURead < IMU_READ_INTERVAL) return;
  lastIMURead = t;

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x3B);                       // ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, 6, true);
  if (Wire.available() >= 6) {
    int16_t rx = (Wire.read() << 8) | Wire.read();
    int16_t ry = (Wire.read() << 8) | Wire.read();
    (void)((Wire.read() << 8) | Wire.read());   // Z descartado
    imu_x = rx / 16384.0f;
    imu_y = ry / 16384.0f;
    filtered_x = filtered_x * (1.0f - IMU_FILTER_ALPHA) + imu_x * IMU_FILTER_ALPHA;
    filtered_y = filtered_y * (1.0f - IMU_FILTER_ALPHA) + imu_y * IMU_FILTER_ALPHA;
  }
}

// ─── Recalcular coeficientes del biquad LPF desde el IMU ───
void updateFilter() {
  float xa = fabsf(filtered_x); if (xa > 1.0f) xa = 1.0f;
  float ya = fabsf(filtered_y); if (ya > 1.0f) ya = 1.0f;

  float cutoff = cutoffBase + xa * 7800.0f;
  if (cutoff > 12000.0f) cutoff = 12000.0f; // margen de estabilidad del biquad
  float Q      = qBase + ya * 15.0f;
  if (Q > 18.0f) Q = 18.0f;                  // tope para que no se vuelva inestable

  float omega = 2.0f * (float)M_PI * cutoff / SAMPLE_RATE;
  float s = sinf(omega), c = cosf(omega);
  float alpha = s / (2.0f * Q);

  float b0 = (1.0f - c) * 0.5f;
  float b1 =  1.0f - c;
  float b2 = (1.0f - c) * 0.5f;
  float a0 =  1.0f + alpha;
  float a1 = -2.0f * c;
  float a2 =  1.0f - alpha;

  f_b0 = b0 / a0;
  f_b1 = b1 / a0;
  f_b2 = b2 / a0;
  f_a1 = a1 / a0;
  f_a2 = a2 / a0;
}

inline float applyFilter(float in) {
  float out = f_b0 * in + f_b1 * f_x1 + f_b2 * f_x2 - f_a1 * f_y1 - f_a2 * f_y2;
  f_x2 = f_x1; f_x1 = in;
  f_y2 = f_y1; f_y1 = out;
  return out;
}

// ─── Visualizador de 6 LEDs (throttle por LED_REFRESH_MS) ──
// Corre en el mismo loop() que el audio. FastLED usa RMT (no I2S) → no choca.
void renderLEDs() {
  unsigned long t = millis();
  static unsigned long lastFrame = 0;
  if (t - lastFrame < LED_REFRESH_MS) return;
  lastFrame = t;

  // Consumir el aviso de beat del audio
  if (g_stepFlash) {
    if (flashLevel < (g_downbeat ? 1.0f : 0.55f))
      flashLevel = g_downbeat ? 1.0f : 0.55f;
    g_stepFlash = false;
  }

  // Flash de cambio de panel (confirma el cambio con un golpe de luz)
  static int lastPanel = PANEL_A;
  if (panel != lastPanel) { lastPanel = panel; flashLevel = 1.0f; }

  // Tono base = panel; el filtro del IMU lo desplaza (filter sweep ↔ color sweep)
  float cut = fabsf(filtered_x); if (cut > 1.0f) cut = 1.0f;
  uint8_t hue = panelHue() + (uint8_t)(cut * 40.0f);

  if (!isPlaying) {
    // Detenido → respiración lenta en el color del panel
    static uint8_t breath = 0; static int8_t bdir = 1;
    breath += bdir * 3;
    if (breath >= 60) bdir = -1;
    if (breath <= 4)  bdir =  1;
    fill_solid(leds, NUM_LEDS, CHSV(hue, 220, breath));
  } else {
    // Reproduciendo → barra VU = energía polifónica
    float lvl = g_energy * 1.6f; if (lvl > 1.0f) lvl = 1.0f;
    float litf = lvl * NUM_LEDS;
    for (int i = 0; i < NUM_LEDS; i++) {
      float on = litf - i;                       // cuánto le toca a este LED (0..1)
      if (on < 0.0f) on = 0.0f; if (on > 1.0f) on = 1.0f;
      uint8_t v = 25 + (uint8_t)(on * 200.0f);   // piso tenue para que la barra "viva"
      leds[i] = CHSV(hue + i * 6, 255, v);       // leve gradiente a lo largo de la barra
    }
  }

  // Destello de beat: suma blanco a todos y decae
  if (flashLevel > 0.02f) {
    uint8_t w = (uint8_t)(flashLevel * 130.0f);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] += CRGB(w, w, w);
    flashLevel *= 0.55f;                          // decae rápido (render ~22 ms)
  }

  FastLED.show();
}

// ─── Setup I2S ─────────────────────────────────────────────
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
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

// ─── Setup ─────────────────────────────────────────────────
void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);   // sin logs (no usamos Serial)

  pinMode(BTN_PLAY,    INPUT_PULLUP);
  pinMode(BTN_SCALE,   INPUT_PULLUP);
  pinMode(BTN_PATTERN, INPUT_PULLUP);
  pinMode(BTN_LENGTH,  INPUT_PULLUP);
  pinMode(BTN_OCTAVE,  INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i < NUM_VOICES; i++) voices[i] = {false, 0, 0, 0, 0, 0};

  // LEDs de placa
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
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
    switch (i) {
      case 0: { float at = 0.0005f + val * 0.1f;          // 0.5 ms – 100.5 ms
                attackInc = 1.0f / (at * SAMPLE_RATE); } break;
      case 1: g_volume = val * val; break;                // curva cuadrática
      case 2: { int bpm = 40 + (int)(val * 260.0f);       // 40 – 300 BPM
                samplesPerStep = (uint32_t)((uint64_t)SAMPLE_RATE * 60 / (bpm * 4)); } break;
      case 3: { float dt = 0.05f + val * val * 2.5f;      // 50 ms – 2.55 s
                decayCoef = expf(-1.0f / (dt * SAMPLE_RATE)); } break;
    }
  } else if (panel == PANEL_B) {
    chordOffset[i] = (int)(val * 7.0f + 0.5f);
  } else {
    switch (i) {
      case 0: detuneCents = val * 25.0f;          break;  // POT1 → detune 0–25 cents
      case 1: driveAmt    = 0.6f + val * 2.4f;    break;  // POT2 → drive 0.6–3.0
      case 2: cutoffBase  = 200.0f + val * 4800.0f; break;// POT3 → piso cutoff 200–5000 Hz
      case 3: qBase       = 0.7f + val * 5.3f;    break;  // POT4 → resonancia base 0.7–6.0
    }
  }
}

// ─── Loop ──────────────────────────────────────────────────
void loop() {
  // — Combos de cambio de panel: BTN1+BTN5 → Panel B · BTN2+BTN4 → Panel C —
  bool b1 = digitalRead(BTN_PLAY);     // LOW = presionado
  bool b2 = digitalRead(BTN_SCALE);
  bool b4 = digitalRead(BTN_LENGTH);
  bool b5 = digitalRead(BTN_OCTAVE);

  if (b1 == LOW && b5 == LOW && !combo15) {          // BTN1 + BTN5 → Panel B (o vuelta a A)
    panel = (panel == PANEL_B) ? PANEL_A : PANEL_B;
    combo15 = true; panelChanged = true;
  }
  if (b2 == LOW && b4 == LOW && !combo24) {          // BTN2 + BTN4 → Panel C (o vuelta a A)
    panel = (panel == PANEL_C) ? PANEL_A : PANEL_C;
    combo24 = true; panelChanged = true;
  }

  // BTN1 ↑
  if (b1 == HIGH && b1Level == LOW && !combo15) {
    if      (panel == PANEL_A) { isPlaying = !isPlaying;            // Play / Stop
                                 if (isPlaying) { currentStep = 0; sampleInStep = 0; } }
    else if (panel == PANEL_B) rootSemi = BTN_NOTE[0];             // nota F# (+7)
    else                       waveType  = (waveType + 1) % 3;      // Panel C → forma de onda
  }
  // BTN5 ↑
  if (b5 == HIGH && b5Level == LOW && !combo15) {
    if      (panel == PANEL_A) octaveIndex = (octaveIndex + 1) % 4; // octava
    else if (panel == PANEL_B) rootSemi = BTN_NOTE[4];             // nota G# (+9)
    else                       subOsc = !subOsc;                    // Panel C → sub-osc (-1 oct)
  }
  // BTN2 ↑
  if (b2 == HIGH && b2Level == LOW && !combo24) {
    if      (panel == PANEL_A) currentScale = (currentScale + 1) % numScales; // escala
    else if (panel == PANEL_B) rootSemi = BTN_NOTE[1];                        // nota C# (+2)
    else                       octUp = !octUp;                                 // Panel C → octava arriba
  }
  // BTN4 ↑
  if (b4 == HIGH && b4Level == LOW && !combo24) {
    if (panel == PANEL_A) {                                         // largo de secuencia
      sequenceLength = (sequenceLength == 4) ? 8 : ((sequenceLength == 8) ? 16 : 4);
      if (currentStep >= sequenceLength) currentStep = 0;
    } else if (panel == PANEL_B) rootSemi = BTN_NOTE[3];           // nota D# (+4)
    else                         fifthLayer = !fifthLayer;         // Panel C → capa de quinta
  }

  if (b1 == HIGH && b5 == HIGH) combo15 = false;     // reset de cada combo al soltar su par
  if (b2 == HIGH && b4 == HIGH) combo24 = false;
  b1Level = b1; b2Level = b2; b4Level = b4; b5Level = b5;

  // — BTN3 (no participa en combos): patrón / nota Do / unison según panel —
  if (buttonPressed(bPattern)) {
    if      (panel == PANEL_A) currentPattern = (currentPattern + 1) % numPatterns; // patrón
    else if (panel == PANEL_B) rootSemi = BTN_NOTE[2];                               // nota B (tónica)
    else                       unison = !unison;                                     // Panel C → unison/poly
  }

  // — IMU + filtro —
  readIMU();
  updateFilter();

  // — Pots: 1 por buffer en ROTACIÓN. Congelados al cambiar de panel —
  static const uint8_t POT_PIN[4] = { POT_ATTACK, POT_VOLUME, POT_TEMPO, POT_DECAY };
  static uint8_t potScan = 0;

  if (panelChanged) {
    for (int i = 0; i < 4; i++) { potLive[i] = false; potAnchor[i] = -1.0f; } // -1 = re-anclar
    panelChanged = false;
  }

  int pi = potScan; potScan = (potScan + 1) & 3;   // siguiente pot
  float pv = readPot(POT_PIN[pi]);
  if (potAnchor[pi] < 0.0f) {
    potAnchor[pi] = pv;                              // captura el ancla (no aplica todavía)
  } else {
    if (!potLive[pi] && fabsf(pv - potAnchor[pi]) > POT_MOVE_THR)
      potLive[pi] = true;                            // se movió → toma el control
    if (potLive[pi]) applyPot(pi, pv);
  }

  // — Energía polifónica para los LEDs: suma de envolventes activas (1 lectura
  //   por buffer; independiente del volumen master) con seguidor de envolvente. —
  float vsum = 0.0f;
  for (int i = 0; i < NUM_VOICES; i++) if (voices[i].active) vsum += voices[i].env;
  vsum *= 0.25f;                                     // ~4 voces a tope ≈ 1.0
  if (vsum > g_energy) g_energy = vsum;              // ataque instantáneo
  else                 g_energy = g_energy * 0.90f + vsum * 0.10f;  // release suave

  // — Generar buffer de audio —
  int16_t buffer[BUFFER_SAMPLES * 2];

  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    // Secuenciador exacto a nivel de muestra
    if (isPlaying) {
      if (sampleInStep >= samplesPerStep) {
        sampleInStep = 0;
        currentStep = (currentStep + 1) % sequenceLength;
        triggerStep();
      }
      sampleInStep++;
    }

    // Mezcla de voces activas
    float mix = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;

      float dt = v.freq / SAMPLE_RATE;
      v.phase += dt;
      if (v.phase >= 1.0f) v.phase -= 1.0f;

      float wave = osc(v.phase, dt);

      if (v.stage == 0) {            // attack lineal
        v.env += attackInc;
        if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; }
      } else {                       // decay exponencial
        v.env *= decayCoef;
        if (v.env < 0.0008f) { v.active = false; v.env = 0.0f; continue; }
      }

      mix += wave * v.env;
    }

    // Escalado de cabecera + filtro IMU
    mix *= 0.18f;
    float filtered = applyFilter(mix);

    float v = filtered * g_volume * driveAmt;

    // Soft-clip tipo tanh (aproximación racional), ACOTADO a ±1 sin recorte duro
    if (v >  3.0f) v =  3.0f;
    if (v < -3.0f) v = -3.0f;
    float shaped = v * (27.0f + v * v) / (27.0f + 9.0f * v * v);

    int16_t outS = (int16_t)(shaped * 30000.0f);
    buffer[n * 2]     = outS;  // L
    buffer[n * 2 + 1] = outS;  // R
  }

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);

  // — Visualizador de LEDs (throttled; tras volcar el buffer al DMA) —
  renderLEDs();
}
