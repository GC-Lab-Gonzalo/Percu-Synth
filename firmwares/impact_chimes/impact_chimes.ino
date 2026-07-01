// ==============================================================================================================================================
// PERCU-SYNTH — Impact Chimes (campanas por golpe en el piso) — GC Lab Chile
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
// Instrumento por IMPACTO: se apoya el PercuSynth en el PISO y, al golpear el piso
// cerca del equipo, la vibración llega al acelerómetro como un pico de aceleración
// → se dispara una nota dentro de la escala activa. Se descarta la gravedad (línea
// base lenta), así solo los GOLPES disparan, no la postura ni el movimiento lento.
// Las notas se eligen con una "caminata melódica" suave (siempre dentro de la
// escala → suena agradable) y las voces resuenan con cola y se solapan, creando una
// textura tipo campanas. Motor: I2S → PCM5102 44.1 kHz / 16-bit estéreo, voces
// polifónicas con oscilador morphing (seno → triángulo → sierra), filtro paso-bajos
// y soft-limiter. Escala por defecto: EÓLICA (menor natural).
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// GOLPE (acelerómetro):
// - Apoya el equipo en el piso y golpea el piso cerca → suena una nota. Golpe más
//   fuerte = nota más fuerte. Golpear seguido genera una secuencia de notas.
//   (Mide el impacto, no la postura: tenerlo quieto/inclinado no suena.)
//
// ESCALA — un botón por escala (queda seleccionada hasta que cambies):
// - BTN1 (44) → EÓLICA (menor natural)   [por defecto]
// - BTN2 (42) → MAYOR (jónica)
// - BTN3 (0)  → DÓRICA
// - BTN4 (45) → PENTATÓNICA MENOR
// - BTN5 (47) → PENTATÓNICA MAYOR
//
// SÍNTESIS — potenciómetros:
// - POT1 (ADC1)  → Ataque   (0.5 ms percusivo → ~150 ms suave)
// - POT2 (ADC2)  → Decay    (cola corta ~0.15 s → muy larga ~5 s, tipo pad/campana)
// - POT3 (ADC8)  → Brillo   (cutoff del filtro paso-bajos)
// - POT4 (ADC10) → Timbre   (morphing de onda: seno → triángulo → sierra)
//
// MODO DE USO:
// 1. Apoya el equipo en el piso y elige una escala con un botón (arranca en Eólica).
// 2. Golpea el piso cerca del equipo: cada golpe dispara una nota.
// 3. Si no dispara o dispara solo, ajusta HIT_HIGH (sensibilidad) abajo.
//    Si un solo golpe suena 2 veces (rebote), sube RETRIG_RATIO o FLOOR_DECAY.
// 4. Sube el Decay (POT2) para colas largas; baja el ataque (POT1) para notas secas.
//
// DIAGNÓSTICO (sin USB):
// - Al encender suena un ACORDE de arranque → confirma que el firmware y el audio
//   funcionan en esa unidad. Si NO lo escuchas, el problema no es el IMU.
// - Si NO se detecta el IMU, suena una nota grave repetida cada ~1.2 s ("latido"
//   de error). Si escuchas eso, revisa el MPU6050 (cableado/soldadura). El firmware
//   autodetecta la dirección 0x68 y 0x69 (por si el pin AD0 quedó en alto).
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <Wire.h>
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

// ─── Botones (INPUT_PULLUP) — uno por escala ───────────────
#define NUM_BTN 5
const uint8_t BTN_PINS[NUM_BTN] = {44, 42, 0, 45, 47};

// ─── Potenciómetros ────────────────────────────────────────
#define POT_ATTACK   1    // ADC1  → ataque
#define POT_DECAY    2    // ADC2  → decay (cola)
#define POT_CUTOFF   8    // ADC8  → brillo (filtro)
#define POT_TIMBRE  10    // ADC10 → morphing de onda

// ─── Detección de golpe (acelerómetro, en g sobre la línea base) ──
const float HIT_HIGH = 0.22f;     // g de "shock" para disparar una nota  (↓ = más sensible)
const float HIT_MAX  = 1.20f;     // g que mapea a velocidad máxima
const unsigned long TRIG_MIN_MS = 50;   // tiempo muerto MÍNIMO entre notas (descarta el rebote inmediato)
// — Umbral dinámico (distingue REBOTE de GOLPE NUEVO por intensidad) —
// Tras un golpe el umbral sube cerca del pico y baja solo con el tiempo. El rebote del MISMO
// impacto es más débil → no supera el umbral (no dispara). Un golpe NUEVO sí es un pico fuerte.
const float RETRIG_RATIO = 0.85f;  // el listón sube al 85% del pico. ↑ = más estricto (menos dobles).
const float FLOOR_DECAY  = 0.970f; // cuánto baja el listón por buffer (~2.9 ms). ↓ = re-dispara antes.

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
};
Voice voices[NUM_VOICES];
uint32_t voiceCounter = 0;

// ─── Escalas (semitonos) — distinta por botón ──────────────
const int SCALE_EOLICA  [7] = {0, 2, 3, 5, 7, 8, 10};   // menor natural
const int SCALE_MAYOR   [7] = {0, 2, 4, 5, 7, 9, 11};   // jónica
const int SCALE_DORICA  [7] = {0, 2, 3, 5, 7, 9, 10};
const int SCALE_PENT_MIN[5] = {0, 3, 5, 7, 10};
const int SCALE_PENT_MAJ[5] = {0, 2, 4, 7, 9};
const int*  SCALES[NUM_BTN]    = {SCALE_EOLICA, SCALE_MAYOR, SCALE_DORICA, SCALE_PENT_MIN, SCALE_PENT_MAJ};
const int   SCALE_LEN[NUM_BTN] = {7, 7, 7, 5, 5};
int   currentScale = 0;                 // 0 = Eólica
const float BASE_FREQ = 220.0f;         // A3 (tónica)
int   walkDeg = 3;                      // grado actual de la caminata melódica

// ─── Parámetros de síntesis (de los pots) ──────────────────
float attackInc = 0.02f;
float decayCoef = 0.99996f;
float g_cutoff  = 3500.0f;
float morph     = 0.30f;

// ─── Acelerómetro / detección de impacto ───────────────────
float accelBaseline = 1.0f;   // línea base lenta (≈ gravedad, 1 g)
float shock = 0.0f;           // desviación instantánea respecto a la base (g)
float trigFloor = 0.0f;       // umbral dinámico: sube tras un golpe y decae con el tiempo
unsigned long lastTrig = 0;

bool    imuOK = false;        // ¿se detectó el MPU6050?
uint8_t imuAddr = 0x68;       // se autodetecta 0x68 / 0x69 en initIMU()
unsigned long lastBeat = 0;   // "latido" de error si no hay IMU

// ─── Filtro biquad LPF (paso-bajos suave) ──────────────────
float f_b0, f_b1, f_b2, f_a1, f_a2;
float f_x1 = 0, f_x2 = 0, f_y1 = 0, f_y2 = 0;

// ─── Tabla de seno ─────────────────────────────────────────
#define LUT_SIZE 512
float sineLUT[LUT_SIZE];

// ─── RNG simple (caminata melódica) ────────────────────────
uint32_t rngState = 0x1234abcd;
inline uint32_t rng() { rngState = rngState * 1664525u + 1013904223u; return rngState; }

bool btnLast[NUM_BTN];
static i2s_chan_handle_t tx_chan;

// ─── Lectura de pot con sobre-muestreo ─────────────────────
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (float)(sum >> 3) / 4095.0f;
}

// ─── PolyBLEP (anti-aliasing del flanco de la sierra) ──────
inline float polyBlep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  else if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

// ─── Oscilador con morphing seno → triángulo → sierra ──────
inline float morphWave(float phase, float dt) {
  float sine = sineLUT[(int)(phase * LUT_SIZE) & (LUT_SIZE - 1)];
  float tri  = (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
  if (morph <= 0.5f) {
    float t = morph * 2.0f;                 // seno → triángulo
    return sine * (1.0f - t) + tri * t;
  } else {
    float saw = (2.0f * phase - 1.0f) - polyBlep(phase, dt);
    float t = (morph - 0.5f) * 2.0f;        // triángulo → sierra
    return tri * (1.0f - t) + saw * t;
  }
}

// ─── Frecuencia de un grado de la escala activa ────────────
float noteFreq(int degree) {
  int len = SCALE_LEN[currentScale];
  int oct = degree / len;
  int idx = degree % len;
  int semis = SCALES[currentScale][idx] + 12 * oct;
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
  voices[idx] = {true, freq, 0.0f, 0.0f, 0, vel, voiceCounter++};
}

// ─── Golpe detectado → caminata melódica + nota ────────────
void onImpact(float sh) {
  int len = SCALE_LEN[currentScale];
  int maxDeg = 2 * len;                       // ~2 octavas
  walkDeg += (int)(rng() % 5) - 2;            // paso -2..+2
  if (walkDeg < 0)      walkDeg = -walkDeg;            // reflejar en los bordes
  if (walkDeg > maxDeg) walkDeg = 2 * maxDeg - walkDeg;
  if (walkDeg < 0)      walkDeg = 0;
  if (walkDeg > maxDeg) walkDeg = maxDeg;

  float vel = (sh - HIT_HIGH) / (HIT_MAX - HIT_HIGH);
  if (vel < 0) vel = 0; if (vel > 1) vel = 1;
  vel = 0.35f + vel * 0.65f;                  // 0.35..1.0

  triggerNote(noteFreq(walkDeg), vel);
}

// ─── IMU: detección de dirección (0x68 / 0x69) ─────────────
bool detectIMU(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x75);                       // WHO_AM_I
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((int)addr, 1, true);
  return (Wire.available() && (Wire.read() == 0x68));
}

// ─── IMU: init (autodetecta dirección) ─────────────────────
void initIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if      (detectIMU(0x68)) { imuAddr = 0x68; imuOK = true; }
  else if (detectIMU(0x69)) { imuAddr = 0x69; imuOK = true; }   // AD0 en alto
  else                       imuOK = false;

  Wire.beginTransmission(imuAddr); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(true); // wake
  Wire.beginTransmission(imuAddr); Wire.write(0x1A); Wire.write(0x00); Wire.endTransmission(true); // DLPF off (rápido)
  Wire.beginTransmission(imuAddr); Wire.write(0x1C); Wire.write(0x10); Wire.endTransmission(true); // ACCEL ±8g
  delay(100);
}

void readAccel() {
  Wire.beginTransmission(imuAddr);
  Wire.write(0x3B);                       // ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom((int)imuAddr, 6, true);
  if (Wire.available() >= 6) {
    int16_t ax = (Wire.read() << 8) | Wire.read();
    int16_t ay = (Wire.read() << 8) | Wire.read();
    int16_t az = (Wire.read() << 8) | Wire.read();
    float x = ax / ACCEL_LSB_PER_G, y = ay / ACCEL_LSB_PER_G, z = az / ACCEL_LSB_PER_G;
    float mag = sqrtf(x * x + y * y + z * z);            // ≈ 1 g en reposo (cualquier orientación)
    accelBaseline += 0.01f * (mag - accelBaseline);      // línea base lenta → quita gravedad/bias
    shock = fabsf(mag - accelBaseline);                  // pico del golpe
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
    case 1: { float dt = 0.15f + val * val * 4.85f;         // 0.15 s – 5 s
              decayCoef = expf(-1.0f / (dt * SAMPLE_RATE)); } break;
    case 2: g_cutoff = 150.0f + val * val * 8850.0f; break; // 150 Hz – 9 kHz
    case 3: morph = val; break;                             // 0..1
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
  esp_log_level_set("*", ESP_LOG_NONE);   // sin logs (no usamos Serial)

  for (int i = 0; i < NUM_BTN; i++) { pinMode(BTN_PINS[i], INPUT_PULLUP); btnLast[i] = HIGH; }
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i < LUT_SIZE; i++) sineLUT[i] = sinf(2.0f * (float)M_PI * i / LUT_SIZE);
  for (int i = 0; i < NUM_VOICES; i++) voices[i] = {false, 0, 0, 0, 0, 0, 0};

  initIMU();
  updateFilter();
  i2s_init();

  // Acorde de arranque: confirma que el FIRMWARE y el AUDIO funcionan en esta unidad
  // (suena aunque no haya golpe ni IMU). Si no escuchas esto, el problema NO es el IMU.
  triggerNote(noteFreq(0), 0.6f);
  triggerNote(noteFreq(2), 0.6f);
  triggerNote(noteFreq(4), 0.6f);
}

// ─── Loop ──────────────────────────────────────────────────
void loop() {
  // — Botones: cada uno selecciona una escala —
  for (int i = 0; i < NUM_BTN; i++) {
    bool now = digitalRead(BTN_PINS[i]);
    if (now == LOW && btnLast[i] == HIGH) {
      currentScale = i;
      int maxDeg = 2 * SCALE_LEN[currentScale];
      if (walkDeg > maxDeg) walkDeg = maxDeg;
    }
    btnLast[i] = now;
  }

  unsigned long t = millis();

  if (imuOK) {
    // — Acelerómetro: detectar golpe con umbral dinámico —
    readAccel();
    trigFloor *= FLOOR_DECAY;                 // el listón baja solo con el tiempo
    float thresh = (trigFloor > HIT_HIGH) ? trigFloor : HIT_HIGH;   // nunca por debajo del mínimo
    if (shock > thresh && (t - lastTrig) > TRIG_MIN_MS) {
      onImpact(shock);
      lastTrig = t;
      trigFloor = shock * RETRIG_RATIO;        // sube el listón cerca del pico: el rebote (más débil) no lo supera
    }
  } else {
    // — Sin IMU: "latido" de error (nota grave cada ~1.2 s) para avisar audiblemente —
    if (t - lastBeat > 1200) { triggerNote(BASE_FREQ * 0.5f, 0.5f); lastBeat = t; }
  }

  // — Pots: 1 por buffer en rotación (ahorra CPU) —
  static const uint8_t POT_PIN[4] = { POT_ATTACK, POT_DECAY, POT_CUTOFF, POT_TIMBRE };
  static uint8_t potScan = 0;
  int pi = potScan; potScan = (potScan + 1) & 3;
  applyPot(pi, readPot(POT_PIN[pi]));

  updateFilter();

  // — Generar buffer de audio —
  int16_t buffer[BUFFER_SAMPLES * 2];
  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    float mix = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;

      float dt = v.freq / SAMPLE_RATE;
      v.phase += dt;
      if (v.phase >= 1.0f) v.phase -= 1.0f;

      float w = morphWave(v.phase, dt);

      if (v.stage == 0) {                 // attack lineal
        v.env += attackInc;
        if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; }
      } else {                            // decay exponencial
        v.env *= decayCoef;
        if (v.env < 0.0006f) { v.active = false; v.env = 0.0f; continue; }
      }

      mix += w * v.env * v.amp;
    }

    mix *= 0.22f;
    float filtered = applyFilter(mix);

    // Soft-clip suave acotado (sin clipping digital)
    float vv = filtered * 1.2f;
    if (vv >  3.0f) vv =  3.0f;
    if (vv < -3.0f) vv = -3.0f;
    float shaped = vv * (27.0f + vv * vv) / (27.0f + 9.0f * vv * vv);

    int16_t out = (int16_t)(shaped * 29000.0f);
    buffer[n * 2]     = out;  // L
    buffer[n * 2 + 1] = out;  // R
  }

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);
}
