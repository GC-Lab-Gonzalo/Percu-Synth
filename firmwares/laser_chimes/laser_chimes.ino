// ==============================================================================================================================================
// PERCU-SYNTH — Laser Chimes (campanas al cortar el haz de un láser) — GC Lab Chile
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
// - LDR + resistencia de 220 Ω en el SENSOR EXTERNO A (EXT1) |ADC -> GPIO 3|
// - Láser (verde) apuntando directo al LDR — la fuente de luz NO se conecta a la placa
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
//
// CABLEADO DEL LDR (divisor de tensión, cualquiera de las dos formas sirve):
//   3.3V ── LDR ──┬── EXT1 (GPIO 3)        con láser = lectura ALTA
//                 220 Ω
//                 GND
//   ó bien:
//   3.3V ── 220 Ω ──┬── EXT1 (GPIO 3)      con láser = lectura BAJA
//                   LDR
//                   GND
//   El firmware NO asume la polaridad: se define escribiendo las dos lecturas reales en
//   LDR_LASER y LDR_TAPADO (ver bloque "AJUSTE DEL LDR" más abajo).
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
// - Ninguna librería externa (no usa FastLED ni el IMU)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Hermano de `impact_chimes` con el SENSOR DE ENTRADA CAMBIADO: donde aquel escuchaba
// golpes en el piso con el acelerómetro, este vigila un HAZ DE LÁSER apuntado a un LDR.
// Cada vez que algo cruza el haz (una mano, una baqueta, un bailarín) la luz que llega al
// LDR cae → se dispara una nota dentro de la escala activa. Las notas se eligen con una
// "caminata melódica" suave (siempre dentro de la escala → suena agradable) y las voces
// resuenan con cola y se solapan, creando una textura tipo campanas.
//
// El motor de sonido es el mismo: I2S → PCM5102 44.1 kHz / 16-bit estéreo, voces
// polifónicas con oscilador morphing (seno → triángulo → sierra), filtro paso-bajos y
// soft-limiter. Escala por defecto: EÓLICA (menor natural).
//
// El LDR se muestrea a 1 kHz en una tarea propia clavada en el core 0, así el corte del
// haz se detecta con ~1 ms de resolución sin robarle tiempo al audio (core 1). La cola de
// DMA es corta (4×128 ≈ 12 ms) para que la nota se oiga en el acto.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// CORTE DEL HAZ (LDR):
// - Apunta el láser al LDR y cruza el haz → suena una nota. Corte más profundo (tapar el
//   haz entero) = nota más fuerte; rozarlo apenas = nota más suave.
// - La nota se dispara en el FLANCO de corte (al interrumpir), no al restablecer el haz.
//   Para volver a disparar el haz tiene que restablecerse (histéresis) y pasar TRIG_MIN_MS.
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
// PUESTA A PUNTO (lo único que hay que ajustar):
// 1. Flashea con MOSTRAR_ESTADO en 1 y abre el Monitor Serie a 115200.
// 2. Con el láser dando en el LDR, mira `raw` → ese número va en LDR_LASER.
// 3. Tapa el haz con la mano, mira `raw` → ese número va en LDR_TAPADO.
//    (La línea del monitor también trae `min`/`max` vistos desde el arranque: cruza el haz
//     unas cuantas veces y los dos extremos quedan a la vista sin tener que congelar nada.)
// 4. Escribe los dos valores abajo, vuelve a flashear. Listo.
//    Si dispara solo → separa más LDR_LASER de LDR_TAPADO o sube UMBRAL_CORTE.
//    Si un corte suena dos veces → sube TRIG_MIN_MS o baja UMBRAL_REARME.
//
// DIAGNÓSTICO (sin USB):
// - Al encender suena un ACORDE de arranque → confirma que el firmware y el audio
//   funcionan en esa unidad. Si NO lo escuchas, el problema no es el sensor.
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <math.h>

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128

// ─── Botones (INPUT_PULLUP) — uno por escala ───────────────
#define NUM_BTN 5
const uint8_t BTN_PINS[NUM_BTN] = {44, 42, 0, 45, 47};

// ─── Potenciómetros ────────────────────────────────────────
#define POT_ATTACK   1    // ADC1  → ataque
#define POT_DECAY    2    // ADC2  → decay (cola)
#define POT_CUTOFF   8    // ADC8  → brillo (filtro)
#define POT_TIMBRE  10    // ADC10 → morphing de onda

// ==============================================================================================
// AJUSTE DEL LDR  ← ESTO ES LO QUE SE TOCA
// ==============================================================================================
#define LDR_PIN   3       // Sensor externo A (EXT1) — ADC1_CH2, libre de conflictos

// Las DOS lecturas analógicas que definen el rango (0..4095, ADC de 12 bits).
// Se miden con MOSTRAR_ESTADO = 1 y el Monitor Serie abierto (ver "PUESTA A PUNTO").
// NO importa cuál es mayor: si tu divisor está al revés (láser = lectura baja), basta con
// poner LDR_LASER menor que LDR_TAPADO y todo lo demás sigue funcionando igual.
int LDR_LASER  = 4095;    // lectura CON el láser dando de lleno en el LDR
int LDR_TAPADO = 3200;     // lectura con el haz INTERRUMPIDO (luz ambiente sola)

// Umbrales, expresados como fracción del recorrido entre LDR_TAPADO (0.0) y LDR_LASER (1.0).
// Van así, y no en cuentas del ADC, para que cambiar los dos números de arriba no obligue a
// recalcular nada más.
const float UMBRAL_CORTE  = 0.55f;   // por DEBAJO de esto → haz cortado → dispara la nota
const float UMBRAL_REARME = 0.75f;   // hay que volver por ENCIMA para poder disparar de nuevo
                                     // (la separación entre ambos es la histéresis: sin ella el
                                     //  ruido del ADC en el borde dispara una ráfaga de notas)

const unsigned long TRIG_MIN_MS = 60;   // tiempo muerto mínimo entre notas (ms)
const float LDR_SUAVIZADO = 0.60f;      // filtro de la lectura: 1.0 = crudo, 0.2 = muy suave
                                        // (bajarlo quita ruido pero también quita "pegada")

// Volumen de la nota según lo PROFUNDO del corte: tapar el haz entero suena fuerte, rozarlo
// suena suave. Ponlo en false si prefieres que todos los cortes suenen igual de fuerte.
const bool  VEL_POR_PROFUNDIDAD = true;
const float VEL_MINIMA = 0.40f;         // volumen del corte más leve (0..1)

// Diagnóstico por Serial (115200). Ponlo en 0 cuando ya esté calibrado.
#define MOSTRAR_ESTADO 1
const unsigned long ESTADO_MS = 250;    // cada cuánto imprime la línea de estado
// ==============================================================================================

// La DevKitC-1 tiene DOS conectores USB y "USB CDC On Boot" decide cuál es `Serial`:
// imprimimos por los dos puertos para no perseguir un silencio que solo es el cable equivocado.
#if ARDUINO_USB_CDC_ON_BOOT
  #define LOG(...)    do { Serial.print(__VA_ARGS__);   Serial0.print(__VA_ARGS__);   } while (0)
  #define LOGLN(...)  do { Serial.println(__VA_ARGS__); Serial0.println(__VA_ARGS__); } while (0)
#else
  #define LOG(...)    Serial.print(__VA_ARGS__)
  #define LOGLN(...)  Serial.println(__VA_ARGS__)
#endif

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
volatile int currentScale = 0;          // 0 = Eólica
const float BASE_FREQ = 220.0f;         // A3 (tónica)
int   walkDeg = 3;                      // grado actual de la caminata melódica

// ─── Parámetros de síntesis (de los pots) ──────────────────
volatile float attackInc = 0.02f;
volatile float decayCoef = 0.99996f;
volatile float g_cutoff  = 3500.0f;
volatile float morph     = 0.30f;

// ─── Estado del sensor (lo escribe la tarea del core 0) ────
volatile bool  hazCortado  = false;   // estado actual del haz
volatile bool  hitPending  = false;   // hay un corte esperando a sonar
volatile float hitVel      = 1.0f;    // velocidad (volumen) de ese corte
volatile int   ldrRaw      = 0;       // última lectura cruda (para el diagnóstico)
volatile float ldrLuz      = 1.0f;    // lectura normalizada 0..1 (0 = tapado, 1 = láser)
volatile int   ldrMin      = 4095;    // extremos vistos desde el arranque: sirven para
volatile int   ldrMax      = 0;       //   leer de una vez LDR_TAPADO y LDR_LASER

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
  float m = morph;
  float sine = sineLUT[(int)(phase * LUT_SIZE) & (LUT_SIZE - 1)];
  float tri  = (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
  if (m <= 0.5f) {
    float t = m * 2.0f;                     // seno → triángulo
    return sine * (1.0f - t) + tri * t;
  } else {
    float saw = (2.0f * phase - 1.0f) - polyBlep(phase, dt);
    float t = (m - 0.5f) * 2.0f;            // triángulo → sierra
    return tri * (1.0f - t) + saw * t;
  }
}

// ─── Frecuencia de un grado de la escala activa ────────────
float noteFreq(int degree) {
  int sc  = currentScale;
  int len = SCALE_LEN[sc];
  int oct = degree / len;
  int idx = degree % len;
  int semis = SCALES[sc][idx] + 12 * oct;
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

// ─── Haz cortado → caminata melódica + nota ────────────────
void onCorte(float vel) {
  int len = SCALE_LEN[currentScale];
  int maxDeg = 2 * len;                       // ~2 octavas
  walkDeg += (int)(rng() % 5) - 2;            // paso -2..+2
  if (walkDeg < 0)      walkDeg = -walkDeg;            // reflejar en los bordes
  if (walkDeg > maxDeg) walkDeg = 2 * maxDeg - walkDeg;
  if (walkDeg < 0)      walkDeg = 0;
  if (walkDeg > maxDeg) walkDeg = maxDeg;

  triggerNote(noteFreq(walkDeg), vel);

#if MOSTRAR_ESTADO
  LOG(">> CORTE  raw="); LOG(ldrRaw);
  LOG("  luz=");         LOG(ldrLuz, 2);
  LOG("  vel=");         LOG(vel, 2);
  LOG("  grado=");       LOGLN(walkDeg);
#endif
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

// ==============================================================================================
// TAREA DEL SENSOR (core 0) — LDR a 1 kHz + pots
// ==============================================================================================
// Todo lo analógico vive acá: el LDR necesita muestreo rápido y regular, y leer el ADC desde
// los dos cores a la vez es pedir problemas. El audio (core 1) solo consume `hitPending`.
void sensorTask(void *arg) {
  float luzFilt = 1.0f;
  bool  armado  = true;             // ¿el haz está entero y listo para el próximo corte?
  unsigned long lastTrig = 0;
  unsigned long lastPrint = 0;
  uint8_t potScan = 0;
  uint8_t potDiv  = 0;

  for (;;) {
    // — LDR: lectura cruda → normalizada 0..1 —
    int raw = analogRead(LDR_PIN);
    ldrRaw = raw;
    if (raw < ldrMin) ldrMin = raw;
    if (raw > ldrMax) ldrMax = raw;

    // Mapeo lineal entre los dos límites de arriba. Al ser una recta entre dos puntos, la
    // POLARIDAD del divisor no importa: si LDR_LASER < LDR_TAPADO el mapeo se invierte solo.
    float span = (float)(LDR_LASER - LDR_TAPADO);
    if (fabsf(span) < 1.0f) span = 1.0f;                  // protección contra dividir por cero
    float luz = ((float)raw - (float)LDR_TAPADO) / span;
    if (luz < 0.0f) luz = 0.0f;
    if (luz > 1.0f) luz = 1.0f;

    luzFilt += LDR_SUAVIZADO * (luz - luzFilt);
    ldrLuz = luzFilt;

    unsigned long t = millis();

    // — Detección del corte con histéresis —
    if (armado && luzFilt < UMBRAL_CORTE && (t - lastTrig) > TRIG_MIN_MS) {
      // Velocidad por PROFUNDIDAD del corte: cuánto cayó la luz por debajo del umbral.
      // Tapar el haz entero (luz ≈ 0) = máximo; rozarlo = apenas por debajo = suave.
      float vel = 1.0f;
      if (VEL_POR_PROFUNDIDAD) {
        float prof = (UMBRAL_CORTE - luzFilt) / UMBRAL_CORTE;   // 0..1
        if (prof < 0.0f) prof = 0.0f;
        if (prof > 1.0f) prof = 1.0f;
        vel = VEL_MINIMA + prof * (1.0f - VEL_MINIMA);
      }
      hitVel     = vel;
      hitPending = true;
      hazCortado = true;
      armado     = false;
      lastTrig   = t;
    }
    // El haz tiene que restablecerse por encima de UMBRAL_REARME para volver a armar.
    if (!armado && luzFilt > UMBRAL_REARME) {
      armado     = true;
      hazCortado = false;
    }

    // — Pots: uno cada 8 ms en rotación (no necesitan más) —
    if (++potDiv >= 8) {
      potDiv = 0;
      static const uint8_t POT_PIN[4] = { POT_ATTACK, POT_DECAY, POT_CUTOFF, POT_TIMBRE };
      applyPot(potScan, readPot(POT_PIN[potScan]));
      potScan = (potScan + 1) & 3;
    }

#if MOSTRAR_ESTADO
    if (t - lastPrint >= ESTADO_MS) {
      lastPrint = t;
      LOG("LDR raw="); LOG(raw);
      LOG("  luz=");   LOG(luzFilt, 2);
      LOG(hazCortado ? "  [HAZ CORTADO]" : "  [HAZ OK]     ");
      LOG("   min=");  LOG(ldrMin);
      LOG(" max=");    LOG(ldrMax);
      LOG("   (LDR_TAPADO="); LOG(LDR_TAPADO);
      LOG(" LDR_LASER=");     LOG(LDR_LASER);
      LOGLN(")");
    }
#endif

    vTaskDelay(1);                  // 1 ms → el haz se vigila a ~1 kHz
  }
}

// ─── Setup I2S ─────────────────────────────────────────────
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear    = true;
  chan_cfg.dma_desc_num  = 4;                 // cola corta (4×128 ≈ 12 ms): la nota se oye
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;    // al instante de cortar el haz
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

#if MOSTRAR_ESTADO
  Serial.begin(115200);
  #if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);     // OBLIGATORIO con audio: una escritura CDC bloquea hasta que
                                  // el PC lea, y eso mata el DMA del I2S (se oye como crujidos)
    Serial0.begin(115200);        // el otro conector USB de la DevKitC-1
  #endif
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) delay(10);   // con CDC el PC abre el puerto tarde
  LOGLN("");
  LOGLN("=== PERCU-SYNTH · LASER CHIMES ===");
  LOGLN("Apunta el laser al LDR y cruza el haz.");
  LOGLN("Anota 'raw' con laser (-> LDR_LASER) y tapado (-> LDR_TAPADO).");
#endif

  for (int i = 0; i < NUM_BTN; i++) { pinMode(BTN_PINS[i], INPUT_PULLUP); btnLast[i] = HIGH; }
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);     // rango completo 0–3.3 V (el divisor del LDR lo recorre)

  for (int i = 0; i < LUT_SIZE; i++) sineLUT[i] = sinf(2.0f * (float)M_PI * i / LUT_SIZE);
  for (int i = 0; i < NUM_VOICES; i++) voices[i] = {false, 0, 0, 0, 0, 0, 0};

  updateFilter();
  i2s_init();

  // Acorde de arranque: confirma que el FIRMWARE y el AUDIO funcionan en esta unidad
  // (suena aunque el láser esté apagado). Si no escuchas esto, el problema NO es el sensor.
  triggerNote(noteFreq(0), 0.6f);
  triggerNote(noteFreq(2), 0.6f);
  triggerNote(noteFreq(4), 0.6f);

  // El sensor va en el core 0: el audio se queda solo con el core 1 (loopTask de Arduino).
  xTaskCreatePinnedToCore(sensorTask, "sensorLDR", 4096, NULL, 5, NULL, 0);
}

// ─── Loop (core 1) — solo botones y audio ──────────────────
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

  // — Corte de haz pendiente (lo marcó la tarea del sensor) → nota —
  if (hitPending) {
    hitPending = false;
    onCorte(hitVel);
  }

  updateFilter();

  // — Generar buffer de audio —
  int16_t buffer[BUFFER_SAMPLES * 2];
  float aInc = attackInc, dCoef = decayCoef;      // se leen una vez por buffer
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
        v.env += aInc;
        if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; }
      } else {                            // decay exponencial
        v.env *= dCoef;
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
