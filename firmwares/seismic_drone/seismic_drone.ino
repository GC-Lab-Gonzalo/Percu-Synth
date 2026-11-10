// ==============================================================================================================================================
// PERCU-SYNTH — Seismic Drone (drones épicos por vibración de la tierra) — GC Lab Chile
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
// - IMU MPU6050 (ACELERÓMETRO en ±2g = MÁXIMA SENSIBILIDAD) |SDA -> 21, SCL -> 38, VCC -> 3.3V, GND -> GND|  (dir. 0x68)
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
// Hermano de "impact_chimes", pero AMBIENTAL y GRAVE. Se apoya el PercuSynth en el
// PISO y el acelerómetro —configurado en ±2g, su rango MÁS SENSIBLE— escucha la
// VIBRACIÓN DE LA TIERRA: pasos, golpes lejanos, retumbar, manos sobre el suelo.
// Esa vibración no toca "campanas": ALIMENTA un DRON. Hay un dron de base siempre
// presente (tónica + quinta) que respira con la tierra —cuando el suelo vibra, el
// dron crece y el filtro se abre; en silencio, queda oscuro y tenue—. Los golpes
// más marcados, además, siembran notas-pad de la escala que florecen lento y se
// solapan en una textura espacial y épica.
//
// MOTOR: I2S → PCM5102 44.1 kHz / 16-bit ESTÉREO. Cada voz = 3 osciladores DIENTE
// DE SIERRA (muchos armónicos) ligeramente DESAFINADOS entre sí + un SUB seno una
// octava abajo (peso grave). El desafine reparte los osciladores entre L y R →
// ANCHO ESTÉREO espacial. Todo pasa por un FILTRO PASO-BAJOS RESONANTE (lows
// profundos + un pico de agudos que silba como un coro/formante = lo "épico").
// Base muy grave (A1 = 55 Hz). Sin LEDs; por Serial sólo avisa si encontró el IMU.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// VIBRACIÓN (acelerómetro, ±2g):
// - Apoya el equipo en el piso. El temblor del suelo HACE RESPIRAR al dron
//   (volumen + brillo del filtro). Mientras más vibra la tierra, más épico y abierto.
// - Un golpe marcado SIEMBRA una nota-pad de la escala (camina suave por la escala),
//   con ataque LENTO (~550 ms, entra con swell, nunca al choque) y cola larga → se
//   solapan creando el ambiente.
//
// ESCALA — un botón por escala ÉPICA (de la naturaleza y el universo):
// - BTN1 (44) → NEBULOSA   (eólica / menor natural)   [por defecto] — oscura, vasta
// - BTN2 (42) → GALAXIA    (lidia, #4)                — flotante, celeste, expansiva
// - BTN3 (0)  → ECLIPSE    (menor armónica)           — dramática, tensa
// - BTN4 (45) → OCÉANO     (dórica)                   — terrenal, heroica
// - BTN5 (47) → COSMOS     (frigia)                   — antigua, profunda, ritual
//
// TEXTURA — potenciómetros (NO hay ataque: el pad entra lento solo, ~550 ms; sí controlas la cola):
// - POT1 (ADC1)  → DESAFINE / ANCHO   (unísono fino → super-saw ancho y espacial)
// - POT2 (ADC2)  → COLA del dron       (pad ~1 s → dron casi infinito ~30 s)
// - POT3 (ADC8)  → BRILLO (cutoff)     (retumbar oscuro → sierra abierta y brillante)
// - POT4 (ADC10) → RESONANCIA          (suave → pico resonante que silba = épico)
//
// MODO DE USO:
// 1. Apoya el equipo en el piso y elige una escala (arranca en NEBULOSA).
// 2. El dron ya está sonando bajito: PISA o GOLPEA el suelo cerca y escúchalo respirar/crecer.
// 3. Sube POT2 (cola) y abre POT3/POT4 para texturas más épicas y resonantes.
// 4. Si reacciona poco o de más a la vibración, ajusta VIB_FULL / HIT_HIGH abajo.
//
// DIAGNÓSTICO (sin USB):
// - Al encender el DRON de base ya suena (tónica+quinta tenue) → confirma firmware + audio.
//   Si NO escuchas nada, el problema no es el IMU.
// - Si NO se detecta el IMU, el dron PULSA fuerte cada ~1.2 s ("latido" de error).
//   Revisa el MPU6050 (cableado/soldadura). Autodetecta 0x68 y 0x69, acepta MPU6050
//   clones (WHO_AM_I 0x70/0x72/0x98) y lo sigue buscando cada segundo: el latido se
//   calla solo cuando aparece. Por Serial (115200) dice "IMU OK en 0x68 (WHO_AM_I =
//   0x..)" o "IMU NO responde". En la V2.0 el IMU y la OLED comparten conector.
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
#define ACCEL_LSB_PER_G  16384.0f   // ±2g → 16384 LSB/g  (¡rango MÁS sensible!)

// ─── Botones (INPUT_PULLUP) — uno por escala ───────────────
#define NUM_BTN 5
const uint8_t BTN_PINS[NUM_BTN] = {44, 42, 0, 45, 47};

// ─── Potenciómetros ────────────────────────────────────────
#define POT_DETUNE   1    // ADC1  → desafine / ancho estéreo
#define POT_RELEASE  2    // ADC2  → cola del dron
#define POT_CUTOFF   8    // ADC8  → brillo (cutoff del filtro)
#define POT_RESO    10    // ADC10 → resonancia (Q del filtro)

// ─── Sensibilidad a la vibración de la tierra (g sobre la línea base) ──
const float VIB_FULL = 0.06f;     // g de vibración que abre el dron al máximo (↓ = más sensible)
const float HIT_HIGH = 0.05f;     // g de "shock" para SEMBRAR una nota-pad   (↓ = más sensible)
const float HIT_LOW  = 0.02f;     // g para "rearmar" la siembra (histéresis)
const float HIT_MAX  = 0.60f;     // g que mapea a velocidad máxima de la nota
const unsigned long TRIG_MIN_MS = 120;  // separación mínima entre notas sembradas

// ─── Polifonía ─────────────────────────────────────────────
#define NUM_VOICES 8
struct Voice {
  bool     active;
  bool     drone;     // true = voz de dron de base (no muere; env sigue a la vibración)
  float    freq;
  float    ph0, ph1, ph2;   // 3 osciladores sierra desafinados
  float    phSub;           // sub seno (-1 octava)
  float    env;
  uint8_t  stage;     // 0 = attack (florece), 1 = decay (cola)
  float    amp;       // velocidad (0..1)
  uint32_t age;
};
Voice voices[NUM_VOICES];
uint32_t voiceCounter = 0;

// ─── Escalas ÉPICAS (semitonos) — naturaleza y universo ────
const int SCALE_NEBULOSA[7] = {0, 2, 3, 5, 7, 8, 10};   // eólica (menor natural)
const int SCALE_GALAXIA [7] = {0, 2, 4, 6, 7, 9, 11};   // lidia (#4 flotante)
const int SCALE_ECLIPSE [7] = {0, 2, 3, 5, 7, 8, 11};   // menor armónica
const int SCALE_OCEANO  [7] = {0, 2, 3, 5, 7, 9, 10};   // dórica
const int SCALE_COSMOS  [7] = {0, 1, 3, 5, 7, 8, 10};   // frigia
const int*  SCALES[NUM_BTN]    = {SCALE_NEBULOSA, SCALE_GALAXIA, SCALE_ECLIPSE, SCALE_OCEANO, SCALE_COSMOS};
const int   SCALE_LEN[NUM_BTN] = {7, 7, 7, 7, 7};
int   currentScale = 0;                 // 0 = Nebulosa
const float BASE_FREQ = 55.0f;          // A1 (tónica) — MUY grave
int   walkDeg = 4;                      // grado actual de la caminata melódica

// ─── Parámetros de síntesis (de los pots) ──────────────────
const float ATTACK_INC = 1.0f / (0.55f * SAMPLE_RATE);  // ataque FIJO ~550 ms (pad entra lento, swell)
float decayCoef  = 0.999975f;           // cola (POT2) — arranca ~1 s, sube hasta ~30 s
float detuneLo   = 0.995f;              // factor del osc grave (POT1)
float detuneHi   = 1.005f;              // factor del osc agudo (POT1)
float g_cutoff   = 700.0f;              // brillo base (POT3)
float g_Q        = 1.4f;                // resonancia (POT4)

// ─── Acelerómetro / vibración de la tierra ─────────────────
float accelBaseline = 1.0f;   // línea base lenta (≈ gravedad, 1 g)
float shock = 0.0f;           // desviación instantánea respecto a la base (g)
float energy = 0.0f;          // vibración suavizada (0..1) → respiración del dron
bool  hitArmed = true;
unsigned long lastTrig = 0;

bool    imuOK = false;        // ¿se detectó el MPU6050?
uint8_t imuAddr = 0x68;       // se autodetecta 0x68 / 0x69 en initIMU()
unsigned long lastBeat = 0;   // "latido" de error si no hay IMU

// ─── Filtro biquad LPF resonante (ESTÉREO: estado por canal) ──
float f_b0, f_b1, f_b2, f_a1, f_a2;
float f_x1[2] = {0, 0}, f_x2[2] = {0, 0}, f_y1[2] = {0, 0}, f_y2[2] = {0, 0};

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

// ─── Oscilador sierra band-limited ─────────────────────────
inline float sawBlep(float phase, float dt) {
  return (2.0f * phase - 1.0f) - polyBlep(phase, dt);
}
inline float sineAt(float phase) {
  return sineLUT[(int)(phase * LUT_SIZE) & (LUT_SIZE - 1)];
}

// ─── Frecuencia de un grado de la escala activa ────────────
float noteFreq(int degree) {
  int len = SCALE_LEN[currentScale];
  int oct = degree / len;
  int idx = degree % len;
  int semis = SCALES[currentScale][idx] + 12 * oct;
  return BASE_FREQ * powf(2.0f, semis / 12.0f);
}

// ─── Disparar una voz (libre o roba la nota-pad más vieja; nunca roba drones) ──
void triggerNote(float freq, float vel, bool isDrone) {
  int idx = -1;
  for (int i = 0; i < NUM_VOICES; i++) if (!voices[i].active) { idx = i; break; }
  if (idx < 0) {
    uint32_t oldest = 0xFFFFFFFF; idx = 0;
    for (int i = 0; i < NUM_VOICES; i++)
      if (!voices[i].drone && voices[i].age < oldest) { oldest = voices[i].age; idx = i; }
  }
  voices[idx] = {true, isDrone, freq, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, vel, voiceCounter++};
}

// ─── Golpe marcado → caminata melódica + nota-pad ──────────
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
  vel = 0.30f + vel * 0.55f;                  // 0.30..0.85 (los pads no tapan al dron)

  triggerNote(noteFreq(walkDeg), vel, false);
}

// ─── IMU: despertar, detectar y leer ───────────────────────
// El IMU se da por VIVO si entrega los 6 bytes del acelerómetro, NO por su WHO_AM_I:
// muchos MPU6050 clones contestan 0x70 / 0x72 / 0x98 en vez de 0x68 y funcionan igual
// (la versión anterior exigía 0x68, daba "sin IMU" y quedaba en el latido de error sin
// reaccionar a nada). Tampoco se mira el retorno de endTransmission(false): en el core
// del ESP32 ese repeated-start puede dar != 0 con el IMU presente. Y nunca se decide con
// una sola lectura en frío: setup() reintenta y loop() lo vuelve a buscar cada
// IMU_REINTENTO_MS mientras no esté (sin delay, para no cortar el audio).
const unsigned long IMU_REINTENTO_MS = 1000;
unsigned long lastImuProbe = 0;
uint8_t imuFallos = 0;             // lecturas fallidas seguidas (si se suelta el IMU)
bool    imuBaseNueva = true;       // la primera lectura fija la línea base (sin golpe falso)

void imuConfigurar(uint8_t addr) {
  Wire.beginTransmission(addr); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(true); // despertar
  Wire.beginTransmission(addr); Wire.write(0x1A); Wire.write(0x00); Wire.endTransmission(true); // DLPF off (rápido)
  Wire.beginTransmission(addr); Wire.write(0x1C); Wire.write(0x00); Wire.endTransmission(true); // ACCEL ±2g (MÁS sensible)
}

// Lee los 6 bytes del acelerómetro. Devuelve false si el IMU no contestó.
bool imuLeer(uint8_t addr, int16_t &ax, int16_t &ay, int16_t &az) {
  Wire.beginTransmission(addr);
  Wire.write(0x3B);                            // ACCEL_XOUT_H
  Wire.endTransmission(false);                 // retorno ignorado a propósito (ver arriba)
  Wire.requestFrom((int)addr, 6, true);
  if (Wire.available() < 6) { while (Wire.available()) Wire.read(); return false; }
  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();
  return true;
}

uint8_t imuWhoAmI(uint8_t addr) {             // solo informativo (Serial)
  Wire.beginTransmission(addr); Wire.write(0x75); Wire.endTransmission(false);
  Wire.requestFrom((int)addr, 1, true);
  return Wire.available() ? Wire.read() : 0xFF;
}

void imuAviso() {
  char msg[96];
  if (imuOK) snprintf(msg, sizeof msg, "IMU OK en 0x%02X (WHO_AM_I = 0x%02X)", imuAddr, imuWhoAmI(imuAddr));
  else       snprintf(msg, sizeof msg, "IMU NO responde (0x68/0x69): latido de error y se sigue buscando");
  Serial.println(msg);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial0.println(msg);        // la DevKitC-1 tiene dos conectores USB: se imprime en los dos
#endif
}

// Despierta el MPU en sus dos direcciones posibles y se queda con la que entrega datos.
bool imuBuscar() {
  const uint8_t dirs[2] = {0x68, 0x69};        // 0x69 = AD0 en alto
  for (int k = 0; k < 2; k++) {
    imuConfigurar(dirs[k]);
    int16_t ax, ay, az;
    if (imuLeer(dirs[k], ax, ay, az)) {
      imuAddr = dirs[k]; imuOK = true; imuFallos = 0; imuBaseNueva = true;
      return true;
    }
  }
  imuOK = false;
  return false;
}

void initIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Wire.setTimeOut(5);                          // ms: un bus colgado no frena el audio mucho rato
  for (int intento = 0; intento < 10 && !imuBuscar(); intento++) delay(20);
  delay(50);                                   // deja asentar el acelerómetro recién despierto
  imuAviso();
}

void readAccel() {
  int16_t ax, ay, az;
  if (!imuLeer(imuAddr, ax, ay, az)) {
    if (++imuFallos >= 50) { imuOK = false; lastImuProbe = millis(); imuAviso(); }  // se soltó
    return;
  }
  imuFallos = 0;
  float x = ax / ACCEL_LSB_PER_G, y = ay / ACCEL_LSB_PER_G, z = az / ACCEL_LSB_PER_G;
  float mag = sqrtf(x * x + y * y + z * z);            // ≈ 1 g en reposo (cualquier orientación)
  if (imuBaseNueva) { accelBaseline = mag; imuBaseNueva = false; }
  accelBaseline += 0.01f * (mag - accelBaseline);      // línea base lenta → quita gravedad/bias
  shock = fabsf(mag - accelBaseline);                  // vibración instantánea

  // Energía del dron: sube rápido con la vibración, baja lento (respiración)
  float target = shock / VIB_FULL;
  if (target > 1.0f) target = 1.0f;
  if (target > energy) energy += (target - energy) * 0.25f;   // ataque rápido
  else                 energy += (target - energy) * 0.02f;   // release lento
}

// ─── Filtro LPF resonante — cutoff modulado por la vibración ──
void updateFilter() {
  // La tierra abre el filtro: en silencio queda oscuro; con vibración brilla.
  float cutoff = g_cutoff + energy * 5000.0f;
  if (cutoff > 12000.0f) cutoff = 12000.0f;
  if (cutoff < 60.0f)    cutoff = 60.0f;
  float omega = 2.0f * (float)M_PI * cutoff / SAMPLE_RATE;
  float s = sinf(omega), c = cosf(omega);
  float alpha = s / (2.0f * g_Q);
  float b0 = (1.0f - c) * 0.5f, b1 = 1.0f - c, b2 = (1.0f - c) * 0.5f;
  float a0 = 1.0f + alpha, a1 = -2.0f * c, a2 = 1.0f - alpha;
  f_b0 = b0 / a0; f_b1 = b1 / a0; f_b2 = b2 / a0;
  f_a1 = a1 / a0; f_a2 = a2 / a0;
}
inline float applyFilter(int ch, float in) {
  float out = f_b0 * in + f_b1 * f_x1[ch] + f_b2 * f_x2[ch] - f_a1 * f_y1[ch] - f_a2 * f_y2[ch];
  f_x2[ch] = f_x1[ch]; f_x1[ch] = in; f_y2[ch] = f_y1[ch]; f_y1[ch] = out;
  return out;
}

// ─── Aplicar un pot al parámetro de textura ────────────────
void applyPot(int i, float val) {
  switch (i) {
    case 0: { float d = 0.001f + val * 0.020f;              // ±0.1% .. ±2% de desafine
              detuneLo = 1.0f - d; detuneHi = 1.0f + d; } break;
    case 1: { float dt = 1.0f + val * val * 29.0f;          // 1 s – 30 s de cola (muy larga, épica)
              decayCoef = expf(-1.0f / (dt * SAMPLE_RATE)); } break;
    case 2: g_cutoff = 120.0f + val * val * 5000.0f; break; // 120 Hz – 5.1 kHz (base, la vibración suma)
    case 3: g_Q = 0.7f + val * val * 8.0f; break;           // Q 0.7 (suave) – 8.7 (silba)
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
  Serial.begin(115200);        // sólo dice si encontró el IMU (al encender y si cambia)
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);    // sin PC conectado, un print por USB CDC nunca bloquea el audio
  Serial0.begin(115200);
#endif

  for (int i = 0; i < NUM_BTN; i++) { pinMode(BTN_PINS[i], INPUT_PULLUP); btnLast[i] = HIGH; }
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i < LUT_SIZE; i++) sineLUT[i] = sinf(2.0f * (float)M_PI * i / LUT_SIZE);
  for (int i = 0; i < NUM_VOICES; i++) voices[i] = {false, false, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  initIMU();
  updateFilter();
  i2s_init();

  // DRON de base: tónica + quinta SIEMPRE presentes (respiran con la tierra).
  // Que suene al encender ya confirma que el FIRMWARE y el AUDIO funcionan.
  triggerNote(BASE_FREQ,         0.9f, true);   // raíz (A1)
  triggerNote(BASE_FREQ * 1.5f,  0.7f, true);   // quinta justa
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
    // — Acelerómetro: respira el dron (energy) + siembra notas en golpes marcados —
    readAccel();
    if (hitArmed && shock > HIT_HIGH && (t - lastTrig) > TRIG_MIN_MS) {
      onImpact(shock);
      lastTrig = t;
      hitArmed = false;
    }
    if (shock < HIT_LOW) hitArmed = true;
  } else {
    // — Sin IMU: se sigue buscando (no se decide nunca con una sola lectura) —
    if (t - lastImuProbe >= IMU_REINTENTO_MS) {
      lastImuProbe = t;
      if (imuBuscar()) imuAviso();
    }
    // — y mientras no aparece: "latido" de error (pulso de energía cada ~1.2 s) —
    if (!imuOK && t - lastBeat > 1200) { energy = 1.0f; lastBeat = t; }
    energy *= 0.97f;
  }

  // — Pots: 1 por buffer en rotación (ahorra CPU) —
  static const uint8_t POT_PIN[4] = { POT_DETUNE, POT_RELEASE, POT_CUTOFF, POT_RESO };
  static uint8_t potScan = 0;
  int pi = potScan; potScan = (potScan + 1) & 3;
  applyPot(pi, readPot(POT_PIN[pi]));

  updateFilter();

  // env objetivo del dron según la vibración (un piso tenue siempre presente)
  float droneTarget = 0.06f + energy * 0.55f;

  // — Generar buffer de audio (estéreo) —
  int16_t buffer[BUFFER_SAMPLES * 2];
  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    float mixL = 0.0f, mixR = 0.0f;

    for (int i = 0; i < NUM_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;

      // Incrementos de fase de los 3 saws desafinados + sub seno
      float base = v.freq / SAMPLE_RATE;
      float d0 = base * detuneLo;
      float d1 = base;
      float d2 = base * detuneHi;
      v.ph0 += d0; if (v.ph0 >= 1.0f) v.ph0 -= 1.0f;
      v.ph1 += d1; if (v.ph1 >= 1.0f) v.ph1 -= 1.0f;
      v.ph2 += d2; if (v.ph2 >= 1.0f) v.ph2 -= 1.0f;
      v.phSub += base * 0.5f; if (v.phSub >= 1.0f) v.phSub -= 1.0f;

      float s0  = sawBlep(v.ph0, d0);
      float s1  = sawBlep(v.ph1, d1);
      float s2  = sawBlep(v.ph2, d2);
      float sub = sineAt(v.phSub);

      // Reparto estéreo: osc grave→L, agudo→R, centro y sub al medio → ANCHO espacial
      float L = s0 * 0.6f + s1 * 0.45f + s2 * 0.2f + sub * 0.5f;
      float R = s2 * 0.6f + s1 * 0.45f + s0 * 0.2f + sub * 0.5f;

      // Envolvente
      if (v.drone) {
        v.env += (droneTarget - v.env) * 0.0006f;   // sigue lento a la tierra (respira)
      } else if (v.stage == 0) {                     // ataque (florece)
        v.env += ATTACK_INC;
        if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; }
      } else {                                       // cola exponencial
        v.env *= decayCoef;
        if (v.env < 0.0006f) { v.active = false; v.env = 0.0f; continue; }
      }

      float e = v.env * v.amp;
      mixL += L * e;
      mixR += R * e;
    }

    mixL *= 0.16f;
    mixR *= 0.16f;
    float fL = applyFilter(0, mixL);
    float fR = applyFilter(1, mixR);

    // Soft-clip suave acotado (sin clipping digital) — protege del pico resonante
    float vL = fL * 1.2f; if (vL >  3.0f) vL = 3.0f; if (vL < -3.0f) vL = -3.0f;
    float vR = fR * 1.2f; if (vR >  3.0f) vR = 3.0f; if (vR < -3.0f) vR = -3.0f;
    float shL = vL * (27.0f + vL * vL) / (27.0f + 9.0f * vL * vL);
    float shR = vR * (27.0f + vR * vR) / (27.0f + 9.0f * vR * vR);

    buffer[n * 2]     = (int16_t)(shL * 29000.0f);  // L
    buffer[n * 2 + 1] = (int16_t)(shR * 29000.0f);  // R
  }

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);
}
