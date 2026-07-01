// ==============================================================================================================================================
// PERCU-SYNTH — Impact Chimes + LEDs (3 instrumentos · escala mágica C Lydian · show de luces WS2812) — GC Lab Chile
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
// - IMU MPU6050 (se usa el ACELERÓMETRO, ±8g) |SDA -> 21, SCL -> 38, VCC -> 3.3V, GND -> GND|  (dir. 0x68)
// - Tira de LEDs WS2812 (68 LEDs, 5 segmentos) |DATA -> 46|
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
// - FastLED (instalar desde el gestor de librerías Arduino) — para la tira WS2812
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Hermano de `impact_chimes` con LUZ y TIMBRES: se apoya el PercuSynth en el PISO y, al
// golpear el piso cerca, la vibración llega al acelerómetro como un pico → se dispara una
// nota de la escala C LYDIA (mágica, soñadora) y un EFECTO DE LUZ reactivo sobre una tira
// WS2812 de 68 LEDs. Tres TIMBRES seleccionables (campana / marimba / guitarra eléctrica)
// y cinco EFECTOS de luz ciclables. Motor de audio: I2S → PCM5102 44.1 kHz / 16-bit estéreo,
// voces polifónicas, filtro LPF y soft-limiter. Las luces corren por FastLED (RMT, no choca
// con el I2S) y se refrescan throttleadas a ~30 FPS.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// GOLPE (acelerómetro): apoya el equipo en el piso y golpea cerca → suena una nota + luz.
//   Golpe más fuerte = nota más fuerte y luz más brillante.
//
// BOTONES:
// - BTN1 (44) → EFECTO de luz ANTERIOR
// - BTN5 (47) → EFECTO de luz SIGUIENTE   (5 efectos: Onda · Cometa · Pulso · Chispas · Arcoíris)
// - BTN2 (42) → TIMBRE 1: CAMPANA  (el clásico: morph seno→sierra, cola larga)
// - BTN3 (0)  → TIMBRE 2: MARIMBA  (fundamental + 4º armónico, ataque seco, cola corta leñosa)
// - BTN4 (45) → TIMBRE 3: GUITARRA ELÉCTRICA (sierra + overdrive + vibrato, sostenido largo)
//
// POTENCIÓMETROS (igual que impact_chimes, contextual por timbre):
// - POT1 (ADC1)  → Ataque
// - POT2 (ADC2)  → Decay (cola)   [la marimba lo recorta, la guitarra lo alarga]
// - POT3 (ADC8)  → Brillo (cutoff del filtro paso-bajos)
// - POT4 (ADC10) → Timbre: morph (campana) · dureza del mazo (marimba) · drive (guitarra)
//
// DIAGNÓSTICO (sin USB):
// - Al encender: barrido de luz + ACORDE de arranque → confirma firmware/audio/LEDs OK.
// - Si NO se detecta el IMU: "latido" grave cada ~1.2 s + parpadeo rojo de la tira.
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
#define SDA_PIN   21
#define SCL_PIN   38
#define IMU_ADDR  0x68
#define ACCEL_LSB_PER_G  4096.0f    // ±8g → 4096 LSB/g

// ─── Tira de LEDs WS2812 ───────────────────────────────────
#define LED_PIN     46
#define NUM_LEDS    68              // mismos 68 LEDs (5 segmentos) que la referencia
#define LED_BRIGHT  150             // brillo global (0-255). Súbelo si quieres más intensidad.
#define CENTER      (NUM_LEDS / 2)  // 34 — centro de la tira (origen de los efectos)
const unsigned long LED_FRAME_MS = 33;   // ~30 FPS (no robar tiempo a los buffers I2S)
CRGB leds[NUM_LEDS];

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
#define NUM_BTN 5
const uint8_t BTN_PINS[NUM_BTN] = {44, 42, 0, 45, 47};   // BTN1..BTN5

// ─── Potenciómetros ────────────────────────────────────────
#define POT_ATTACK   1    // ADC1  → ataque
#define POT_DECAY    2    // ADC2  → decay (cola)
#define POT_CUTOFF   8    // ADC8  → brillo (filtro)
#define POT_TIMBRE  10    // ADC10 → timbre (morph / dureza / drive)

// ─── Detección de golpe (umbral dinámico anti-doble-disparo) ──
const float HIT_HIGH = 0.22f;     // g mínimo de "shock" para disparar  (↓ = más sensible)
const float HIT_MAX  = 1.20f;     // g que mapea a velocidad máxima
const unsigned long TRIG_MIN_MS = 50;   // tiempo muerto mínimo entre notas
const float RETRIG_RATIO = 0.85f;  // el listón sube al 85% del pico → el rebote (más débil) no redispara
const float FLOOR_DECAY  = 0.970f; // cuánto baja el listón por buffer (↓ = re-dispara antes)

// ─── Instrumentos ──────────────────────────────────────────
#define INST_CAMPANA   0
#define INST_MARIMBA   1
#define INST_GUITARRA  2
uint8_t instrument = INST_CAMPANA;          // timbre activo
const uint8_t instHue[3] = {140, 28, 0};    // color indicador por timbre (cian / naranja / rojo)

// ─── Polifonía ─────────────────────────────────────────────
#define NUM_VOICES 10
struct Voice {
  bool     active;
  float    freq;
  float    phase;
  float    env;
  uint8_t  stage;     // 0 = attack, 1 = decay
  float    amp;       // velocidad (0..1)
  uint32_t age;
  uint8_t  inst;      // instrumento con el que nació esta voz
};
Voice voices[NUM_VOICES];
uint32_t voiceCounter = 0;

// ─── Escala mágica: C LYDIA (mayor con 4ª aumentada) ───────
const int SCALE_LYDIA[7] = {0, 2, 4, 6, 7, 9, 11};   // C D E F# G A B
#define SCALE_LEN 7
const float BASE_FREQ = 130.81f;        // C3 (tónica)
int   walkDeg = 7;                      // grado actual de la caminata melódica

// ─── Parámetros de síntesis (de los pots) ──────────────────
float attackInc    = 0.02f;
float decayTimeSec = 0.8f;     // tiempo de cola (POT2) — los instrumentos lo escalan
float g_cutoff     = 3500.0f;
float morph        = 0.30f;
// Coeficientes de envolvente por instrumento (recalculados cada buffer)
float attackIncInst[3];
float decayCoefInst[3];

// ─── Vibrato global (guitarra) ─────────────────────────────
float vibPhase = 0.0f;
const float vibInc = 5.5f / SAMPLE_RATE;   // ~5.5 Hz

// ─── Acelerómetro / detección de impacto ───────────────────
float accelBaseline = 1.0f;   // línea base lenta (≈ gravedad, 1 g)
float shock = 0.0f;           // desviación instantánea respecto a la base (g)
float trigFloor = 0.0f;       // umbral dinámico: sube tras un golpe y decae con el tiempo
unsigned long lastTrig = 0;

bool    imuOK = false;
uint8_t imuAddr = 0x68;
unsigned long lastBeat = 0;

// ─── Filtro biquad LPF ─────────────────────────────────────
float f_b0, f_b1, f_b2, f_a1, f_a2;
float f_x1 = 0, f_x2 = 0, f_y1 = 0, f_y2 = 0;

// ─── Tabla de seno ─────────────────────────────────────────
#define LUT_SIZE 512
float sineLUT[LUT_SIZE];

// ─── RNG simple ────────────────────────────────────────────
uint32_t rngState = 0x1234abcd;
inline uint32_t rng() { rngState = rngState * 1664525u + 1013904223u; return rngState; }

bool btnLast[NUM_BTN];
static i2s_chan_handle_t tx_chan;

// ─── Estado del motor de luces ─────────────────────────────
#define NUM_EFFECTS 5
int     ledEffect = 0;           // 0=Onda 1=Cometa 2=Pulso 3=Chispas 4=Arcoíris
float   g_energy = 0.0f;         // energía global (0..1) que decae — alimenta Pulso/Arcoíris
uint8_t g_hue = 150;             // color de la última nota
int     g_newSparks = 0;         // chispas pendientes de pintar (efecto Chispas)
float   g_scroll = 0.0f;         // desplazamiento del arcoíris
uint8_t g_fxFlash = 0;           // frames de flash al cambiar de efecto
uint8_t g_instFlash = 0;         // frames de flash al cambiar de instrumento
unsigned long lastLedFrame = 0;

struct LedWave { bool active; float radius; uint8_t hue; uint8_t vel; };
#define NUM_WAVES 8
LedWave lwaves[NUM_WAVES];

// ─── Lectura de pot con sobre-muestreo ─────────────────────
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (float)(sum >> 3) / 4095.0f;
}

// ─── Soft-clip cúbico acotado (saturador / overdrive) ──────
inline float softclip(float x) {
  if (x >  3.0f) x =  3.0f;
  if (x < -3.0f) x = -3.0f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

// ─── PolyBLEP (anti-aliasing del flanco de la sierra) ──────
inline float polyBlep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  else if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

// ─── Oscilador con morphing seno → triángulo → sierra (campana) ──
inline float morphWave(float phase, float dt) {
  float sine = sineLUT[(int)(phase * LUT_SIZE) & (LUT_SIZE - 1)];
  float tri  = (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
  if (morph <= 0.5f) {
    float t = morph * 2.0f;
    return sine * (1.0f - t) + tri * t;
  } else {
    float saw = (2.0f * phase - 1.0f) - polyBlep(phase, dt);
    float t = (morph - 0.5f) * 2.0f;
    return tri * (1.0f - t) + saw * t;
  }
}

// ─── Muestra de un oscilador según el instrumento ──────────
inline float instWave(Voice &v) {
  if (v.inst == INST_CAMPANA) {
    return morphWave(v.phase, v.freq / SAMPLE_RATE);
  } else if (v.inst == INST_MARIMBA) {
    // Fundamental + 4º armónico (típico de la marimba). El armónico es el "golpe del mazo":
    // pesa más al inicio (env²) y se apaga antes que la fundamental → tono leñoso/percusivo.
    float fund = sineLUT[(int)(v.phase * LUT_SIZE) & (LUT_SIZE - 1)];
    float p4 = v.phase * 4.0f; p4 -= (int)p4;
    float h4 = sineLUT[(int)(p4 * LUT_SIZE) & (LUT_SIZE - 1)];
    float h4amt = 0.20f + morph * 0.80f;          // POT4 = dureza del mazo
    return fund + h4amt * h4 * v.env * v.env;
  } else {
    // Guitarra: sierra brillante + overdrive (drive desde POT4).
    float saw = (2.0f * v.phase - 1.0f) - polyBlep(v.phase, v.freq / SAMPLE_RATE);
    float drive = 1.3f + morph * 2.8f;
    return softclip(saw * drive);
  }
}

// ─── Frecuencia de un grado de la escala Lydia ─────────────
float noteFreq(int degree) {
  int oct = degree / SCALE_LEN;
  int idx = degree % SCALE_LEN;
  int semis = SCALE_LYDIA[idx] + 12 * oct;
  return BASE_FREQ * powf(2.0f, semis / 12.0f);
}

// ─── Disparar una nota (voz libre o roba la más vieja) ─────
void triggerNote(float freq, float vel) {
  int idx = -1;
  for (int i = 0; i < NUM_VOICES; i++) if (!voices[i].active) { idx = i; break; }
  if (idx < 0) {
    uint32_t oldest = 0xFFFFFFFF; idx = 0;
    for (int i = 0; i < NUM_VOICES; i++) if (voices[i].age < oldest) { oldest = voices[i].age; idx = i; }
  }
  voices[idx] = {true, freq, 0.0f, 0.0f, 0, vel, voiceCounter++, instrument};
}

// ─── Luces: registrar una nota (color + energía + onda) ────
void ledOnNote(uint8_t hue, float vel) {
  for (int i = 0; i < NUM_WAVES; i++)
    if (!lwaves[i].active) { lwaves[i] = {true, 0.0f, hue, (uint8_t)(vel * 255.0f)}; break; }
  g_hue = hue;
  g_energy += 0.35f + 0.65f * vel; if (g_energy > 1.0f) g_energy = 1.0f;
  g_newSparks += 2 + (int)(vel * 7.0f); if (g_newSparks > 40) g_newSparks = 40;
}

// ─── Golpe detectado → caminata melódica + nota + luz ──────
void onImpact(float sh) {
  int maxDeg = 2 * SCALE_LEN;                  // ~2 octavas
  walkDeg += (int)(rng() % 5) - 2;             // paso -2..+2
  if (walkDeg < 0)      walkDeg = -walkDeg;
  if (walkDeg > maxDeg) walkDeg = 2 * maxDeg - walkDeg;
  if (walkDeg < 0)      walkDeg = 0;
  if (walkDeg > maxDeg) walkDeg = maxDeg;

  float vel = (sh - HIT_HIGH) / (HIT_MAX - HIT_HIGH);
  if (vel < 0) vel = 0; if (vel > 1) vel = 1;
  vel = 0.35f + vel * 0.65f;                   // 0.35..1.0

  triggerNote(noteFreq(walkDeg), vel);
  ledOnNote((uint8_t)(150 + walkDeg * 7), vel);   // paleta mágica cian→violeta→magenta por nota
}

// ─── IMU: detección de dirección (0x68 / 0x69) ─────────────
bool detectIMU(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x75);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((int)addr, 1, true);
  return (Wire.available() && (Wire.read() == 0x68));
}

void initIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  if      (detectIMU(0x68)) { imuAddr = 0x68; imuOK = true; }
  else if (detectIMU(0x69)) { imuAddr = 0x69; imuOK = true; }
  else                       imuOK = false;
  Wire.beginTransmission(imuAddr); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(true); // wake
  Wire.beginTransmission(imuAddr); Wire.write(0x1A); Wire.write(0x00); Wire.endTransmission(true); // DLPF off
  Wire.beginTransmission(imuAddr); Wire.write(0x1C); Wire.write(0x10); Wire.endTransmission(true); // ±8g
  delay(100);
}

void readAccel() {
  Wire.beginTransmission(imuAddr);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((int)imuAddr, 6, true);
  if (Wire.available() >= 6) {
    int16_t ax = (Wire.read() << 8) | Wire.read();
    int16_t ay = (Wire.read() << 8) | Wire.read();
    int16_t az = (Wire.read() << 8) | Wire.read();
    float x = ax / ACCEL_LSB_PER_G, y = ay / ACCEL_LSB_PER_G, z = az / ACCEL_LSB_PER_G;
    float mag = sqrtf(x * x + y * y + z * z);
    accelBaseline += 0.01f * (mag - accelBaseline);
    shock = fabsf(mag - accelBaseline);
  }
}

// ─── Filtro LPF (Q suave fijo) ─────────────────────────────
void updateFilter() {
  float cutoff = g_cutoff; if (cutoff > 12000.0f) cutoff = 12000.0f;
  float Q = 1.2f;
  float omega = 2.0f * (float)M_PI * cutoff / SAMPLE_RATE;
  float s = sinf(omega), c = cosf(omega);
  float alpha = s / (2.0f * Q);
  float b0 = (1.0f - c) * 0.5f, b1 = 1.0f - c, b2 = (1.0f - c) * 0.5f;
  float a0 = 1.0f + alpha, a1 = -2.0f * c, a2 = 1.0f - alpha;
  f_b0 = b0 / a0; f_b1 = b1 / a0; f_b2 = b2 / a0;
  f_a1 = a1 / a0; f_a2 = a2 / a0;
}
inline float applyFilter(float in) {
  float out = f_b0 * in + f_b1 * f_x1 + f_b2 * f_x2 - f_a1 * f_y1 - f_a2 * f_y2;
  f_x2 = f_x1; f_x1 = in; f_y2 = f_y1; f_y1 = out;
  return out;
}

// ─── Aplicar un pot al parámetro de síntesis ───────────────
void applyPot(int i, float val) {
  switch (i) {
    case 0: { float at = 0.0005f + val * 0.15f;             // 0.5 ms – 150 ms
              attackInc = 1.0f / (at * SAMPLE_RATE); } break;
    case 1: decayTimeSec = 0.15f + val * val * 4.85f; break; // 0.15 s – 5 s
    case 2: g_cutoff = 150.0f + val * val * 8850.0f; break;  // 150 Hz – 9 kHz
    case 3: morph = val; break;                              // 0..1
  }
}

// ─── Recalcular envolventes por instrumento (cada buffer) ──
void updateEnvCoefs() {
  attackIncInst[INST_CAMPANA]  = attackInc;
  attackIncInst[INST_MARIMBA]  = 1.0f / (0.0015f * SAMPLE_RATE);   // ~1.5 ms (percusivo, fijo)
  attackIncInst[INST_GUITARRA] = attackInc;
  decayCoefInst[INST_CAMPANA]  = expf(-1.0f / (decayTimeSec * SAMPLE_RATE));
  float tm = decayTimeSec * 0.30f; if (tm < 0.12f) tm = 0.12f;     // marimba: cola corta leñosa
  decayCoefInst[INST_MARIMBA]  = expf(-1.0f / (tm * SAMPLE_RATE));
  float tg = decayTimeSec * 1.10f; if (tg < 0.6f) tg = 0.6f;       // guitarra: sostenido largo
  decayCoefInst[INST_GUITARRA] = expf(-1.0f / (tg * SAMPLE_RATE));
}

// ─── EFECTOS DE LUZ ────────────────────────────────────────
// 0 — ONDA: cada golpe lanza una onda simétrica desde el centro (estilo referencia).
void fxOnda() {
  fadeToBlackBy(leds, NUM_LEDS, 55);
  for (int i = 0; i < NUM_WAVES; i++) {
    if (!lwaves[i].active) continue;
    int r = (int)lwaves[i].radius;
    float prog = (float)r / CENTER;
    if (prog > 1.0f) { lwaves[i].active = false; continue; }
    uint8_t b = (uint8_t)((1.0f - prog) * 255.0f * (0.4f + 0.6f * lwaves[i].vel / 255.0f));
    CRGB c = CHSV(lwaves[i].hue, 255, b);
    int L = CENTER - r, R = CENTER + r;
    if (L >= 0 && L < NUM_LEDS) leds[L] += c;
    if (R >= 0 && R < NUM_LEDS) leds[R] += c;
    lwaves[i].radius += 0.8f + 0.6f * (lwaves[i].vel / 255.0f);
  }
}
// 1 — COMETA: cabezas brillantes que vuelan del centro a los extremos, con estela.
void fxCometa() {
  fadeToBlackBy(leds, NUM_LEDS, 38);
  for (int i = 0; i < NUM_WAVES; i++) {
    if (!lwaves[i].active) continue;
    int r = (int)lwaves[i].radius;
    if (r > CENTER) { lwaves[i].active = false; continue; }
    CRGB c = CHSV(lwaves[i].hue, 255, 255);
    int L = CENTER - r, R = CENTER + r;
    if (L >= 0 && L < NUM_LEDS) leds[L] = c;
    if (R >= 0 && R < NUM_LEDS) leds[R] = c;
    lwaves[i].radius += 1.6f;
  }
}
// 2 — PULSO: toda la tira late con el color de la nota; respiración suave en reposo.
void fxPulso(uint32_t t) {
  uint8_t breath = 18 + (uint8_t)(12.0f * (0.5f + 0.5f * sinf(t * 0.002f)));
  uint8_t v = (uint8_t)(g_energy * 255.0f);
  if (v < breath) v = breath;
  fill_solid(leds, NUM_LEDS, CHSV(g_hue, 210, v));
}
// 3 — CHISPAS: cada golpe rocía polvo mágico en posiciones aleatorias.
void fxChispas() {
  fadeToBlackBy(leds, NUM_LEDS, 32);
  while (g_newSparks > 0) {
    int p = rng() % NUM_LEDS;
    leds[p] = CHSV(g_hue + (int)(rng() % 30) - 15, 200, 255);
    g_newSparks--;
  }
}
// 4 — ARCOÍRIS: arcoíris que se desplaza; su brillo y velocidad pulsan con la energía.
void fxArcoiris() {
  g_scroll += 1.0f + g_energy * 4.0f;
  uint8_t base = (uint8_t)g_scroll;
  uint8_t v = 55 + (uint8_t)(g_energy * 200.0f);
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = CHSV(base + i * 4, 230, v);
}

// ─── Un frame de luces (throttleado a ~30 FPS) ─────────────
void ledFrame(uint32_t t) {
  switch (ledEffect) {
    case 0: fxOnda();        break;
    case 1: fxCometa();      break;
    case 2: fxPulso(t);      break;
    case 3: fxChispas();     break;
    case 4: fxArcoiris();    break;
  }
  // Flashes de confirmación (sobre-escriben unos pocos frames, sin delays)
  if (g_fxFlash > 0)   { g_fxFlash--;   fill_solid(leds, NUM_LEDS, CRGB(50, 50, 70)); }
  if (g_instFlash > 0) { g_instFlash--; fill_solid(leds, NUM_LEDS, CHSV(instHue[instrument], 255, 130)); }

  g_energy *= 0.90f;
  FastLED.show();
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

  for (int i = 0; i < NUM_BTN; i++) { pinMode(BTN_PINS[i], INPUT_PULLUP); btnLast[i] = HIGH; }
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i < LUT_SIZE; i++) sineLUT[i] = sinf(2.0f * (float)M_PI * i / LUT_SIZE);
  for (int i = 0; i < NUM_VOICES; i++) voices[i] = {false, 0, 0, 0, 0, 0, 0, 0};
  for (int i = 0; i < NUM_WAVES; i++) lwaves[i].active = false;

  // LEDs
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHT);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  initIMU();
  updateFilter();
  updateEnvCoefs();
  i2s_init();

  // Barrido de arranque (confirma la tira) + acorde (confirma audio)
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(150 + i * 2, 230, 200);
    FastLED.show();
    delay(6);
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  triggerNote(noteFreq(0), 0.6f);
  triggerNote(noteFreq(2), 0.6f);
  triggerNote(noteFreq(4), 0.6f);
}

// ─── Loop ──────────────────────────────────────────────────
void loop() {
  // — Botones: BTN1/BTN5 = efecto ◀▶ · BTN2/3/4 = instrumento —
  for (int i = 0; i < NUM_BTN; i++) {
    bool now = digitalRead(BTN_PINS[i]);
    if (now == LOW && btnLast[i] == HIGH) {
      if (i == 0)      { ledEffect = (ledEffect + NUM_EFFECTS - 1) % NUM_EFFECTS; g_fxFlash = 4; }
      else if (i == 4) { ledEffect = (ledEffect + 1) % NUM_EFFECTS;               g_fxFlash = 4; }
      else             { instrument = i - 1; g_instFlash = 5; }   // BTN2→0 BTN3→1 BTN4→2
    }
    btnLast[i] = now;
  }

  unsigned long t = millis();

  if (imuOK) {
    // — Acelerómetro: detectar golpe con umbral dinámico —
    readAccel();
    trigFloor *= FLOOR_DECAY;
    float thresh = (trigFloor > HIT_HIGH) ? trigFloor : HIT_HIGH;
    if (shock > thresh && (t - lastTrig) > TRIG_MIN_MS) {
      onImpact(shock);
      lastTrig = t;
      trigFloor = shock * RETRIG_RATIO;
    }
  } else {
    // — Sin IMU: "latido" grave + parpadeo rojo —
    if (t - lastBeat > 1200) {
      triggerNote(BASE_FREQ * 0.5f, 0.5f);
      lastBeat = t;
      fill_solid(leds, NUM_LEDS, CRGB(40, 0, 0));
    }
  }

  // — Pots: 1 por buffer en rotación —
  static const uint8_t POT_PIN[4] = { POT_ATTACK, POT_DECAY, POT_CUTOFF, POT_TIMBRE };
  static uint8_t potScan = 0;
  int pi = potScan; potScan = (potScan + 1) & 3;
  applyPot(pi, readPot(POT_PIN[pi]));

  updateFilter();
  updateEnvCoefs();

  // — Generar buffer de audio —
  int16_t buffer[BUFFER_SAMPLES * 2];
  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    // Vibrato global (lo usa la guitarra)
    vibPhase += vibInc; if (vibPhase >= 1.0f) vibPhase -= 1.0f;
    float vib = sineLUT[(int)(vibPhase * LUT_SIZE) & (LUT_SIZE - 1)];

    float mix = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;

      float dt = v.freq / SAMPLE_RATE;
      if (v.inst == INST_GUITARRA) dt *= (1.0f + 0.005f * vib);   // vibrato
      v.phase += dt;
      if (v.phase >= 1.0f) v.phase -= 1.0f;

      float w = instWave(v);

      if (v.stage == 0) {
        v.env += attackIncInst[v.inst];
        if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; }
      } else {
        v.env *= decayCoefInst[v.inst];
        if (v.env < 0.0006f) { v.active = false; v.env = 0.0f; continue; }
      }

      mix += w * v.env * v.amp;
    }

    mix *= 0.22f;
    float filtered = applyFilter(mix);
    float shaped = softclip(filtered * 1.2f);

    int16_t out = (int16_t)(shaped * 29000.0f);
    buffer[n * 2]     = out;  // L
    buffer[n * 2 + 1] = out;  // R
  }

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);

  // — Luces: refrescar throttleado (~30 FPS) para no robar tiempo al I2S —
  if (t - lastLedFrame >= LED_FRAME_MS) {
    lastLedFrame = t;
    ledFrame(t);
  }
}
