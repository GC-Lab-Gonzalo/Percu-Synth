// ==============================================================================================================================================
// PERCUSYNTH - TRANCE EN PISTAS (con peso) (4 pistas SINTETIZADAS → filtros por pista → DAC + datos para las visuales) - GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo Sandoval - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
// REPOSITORIO: https://github.com/GC-Lab-Gonzalo/Percu-Synth
// ==============================================================================================================================================
// HARDWARE
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3 (PercuSynth). NO necesita el módulo microSD: todo el sonido se sintetiza.
// - DAC PCM5102 vía I2S — estéreo 44.1 kHz · 16-bit |LCK -> 39, DIN -> 40, BCK -> 41|
// - IMU MPU6050 (I2C) |SDA -> 21, SCL -> 38|  (dirección 0x68 o 0x69; opcional)
// - 5 Botones pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// - 4 Piezos |ADC 4, 5, 6, 7|   (opcionales: golpes en vivo que sólo van a las visuales)
// - 6 LEDs SMD WS2812 de la placa en GPIO 46, usados como indicadores
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - Flash Mode         : DIO          (¡OPI rompe I2S!)
// - PSRAM              : OPI PSRAM    (sin PSRAM también suena; sólo se pierden los «repeat» del POT3)
// - Monitor            : 115200 baud  (ver «SALIDA SERIAL» más abajo: lo lee Resonancia)
// También se instala sin Arduino IDE, desde Resonancia (Entrada → Instalar firmware), con firmware.bin.
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (driver/i2s_std.h y Wire.h vienen incluidas)
// - FastLED
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// El hermano sintetizado de mezclador_pistas: la misma idea (4 pistas que suenan juntas, cada una con
// volumen, filtro, resonancia y efecto rítmico, y cada una moviendo su capa en Resonancia), pero acá
// la música la hace la placa, sin tarjeta. Trance con peso, en la línea de Chemical Brothers:
//   pista 0  BATERÍA  las voces de drum_poder (kit NEXO): bombo de tres bandas, caja afinada con bordonera
//                     y crack, clap de tres palmadas, hats y plato de ruido pasa-banda, cada uno en su banda
//   pista 1  BAJO     el de bajo_8_pasos: 2 sierras + cuadrada una octava abajo, saturación suave y filtro con envolvente
//   pista 2  RIFF     un riff corto que se repite y sigue el acorde (sólo notas del acorde) + eco ping-pong
//   pista 3  ACORDES  pad supersaw que crece por secciones y se abre en la ruptura, stabs rave y subida de ruido
// El bombo «bombea» los acordes, el riff y el bajo (sidechain).
// Cada tema es un viaje de 32 compases (A · A' · ruptura sin bombo · drop) en menor natural, con dos
// progresiones consonantes. Hay 6 temas; BTN1 mantenido 2,5 s pasa al siguiente.
//
// Cadena por pista:  síntesis → EFECTO RÍTMICO → FILTRO (pasa-bajos ↔ pasa-altos) → VOLUMEN → paneo
// Master:            suma → bloqueador DC → limitador con lookahead (techo 0.89) → pasa-bajos 13 kHz → DAC
// Acá sí va el techo de 13 kHz (a diferencia de mezclador_pistas): la síntesis genera alias arriba.
// ==============================================================================================================================================
// FUNCIONAMIENTO  (los mismos controles que mezclador_pistas)
// ==============================================================================================================================================
// Todo funciona por BANCOS: un botón elige el banco y las 4 perillas pasan a controlar ese banco.
// Al cambiar de banco, una perilla NO salta al valor nuevo: toma el control recién cuando la mueves.
//
// - BTN1  toque     -> BANCO GENERAL: POT1 volumen BAJO · POT2 RIFF · POT3 BATERÍA · POT4 ACORDES
//         mantener  -> PLAY / STOP (medio segundo). Mantenido 2,5 s -> SIGUIENTE TEMA, desde el inicio y en stop
// - BTN2 -> banco BAJO · BTN3 -> banco RIFF · BTN4 -> banco BATERÍA · BTN5 -> banco ACORDES. En cada uno:
//         POT1  FILTRO: al centro abierto · a la izquierda pasa-bajos (hasta 110 Hz) · a la derecha pasa-altos (hasta 5 kHz)
//         POT2  RESONANCIA del filtro (atada al corte: nunca un pico resonante arriba)
//         POT3  EFECTO RÍTMICO al pulso: apagado · gate 1/8 · gate 1/16 · repeat 1/2 · repeat 1/4 · repeat 1/8 · repeat 1/16
//         POT4  INTENSIDAD del efecto
// - IMU   -> no toca el sonido: inclinar la placa gira y tiñe las visuales
// - PIEZOS -> no suenan: cada golpe va a las visuales
//
// - LEDs:  0  verde = sonando · ámbar = stop · parpadea en blanco con el banco GENERAL elegido
//          1–4  bajo, riff, batería, acordes (el orden de BTN2..BTN5): su color con el brillo de su nivel;
//               parpadea en blanco la del banco elegido
//          5  pulso: chartreuse en el «uno» de cada compás, blanco en los otros tiempos.
//             MAGENTA parpadeando = la CPU no alcanza (un bloque de audio pasó el 80 % de su tiempo)
//
// SALIDA SERIAL: idéntica a mezclador_pistas (R,… de 43 campos, 60 por segundo), así Resonancia no
// distingue una placa de la otra. La línea I suma un octavo campo con los nombres de las pistas:
//   I,titulo,artista,bpm,duracion_ms,tema,total,batería|bajo|riff|acordes
// Los golpes no se detectan: salen del secuenciador (exactos). bajo = cada grupo de notas,
// riff = cada corchea, acordes = cada cambio de acorde y cada stab.
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <math.h>
#include <string.h>
#pragma GCC optimize ("O2")

// ─── Tipos (arriba del todo para que el IDE genere bien los prototipos) ───
struct SVF { float ic1, ic2, a1, a2, a3, k, comp; int modo; };   // filtro de cada pista (TPT), igual que mezclador_pistas
struct Filtro { float ic1, ic2, a1, a2, a3, k; };                 // filtro de cada voz (TPT): pasa-bajos y pasa-banda
struct Biquad { float b0, b1, b2, a1, a2, z1, z2; };
struct Acorde { int8_t raiz; char tipo; };   // raíz en semitonos sobre la tónica · 'm' menor, 'M' mayor
struct Tema {
  const char *nombre;
  float bpm;
  int   tonica;                  // nota MIDI de la tónica (menor natural; sólo importa la clase de altura)
  Acorde a[4], b[4];             // progresión A (compases 1–16) y B (ruptura y drop, 17–32): 2 compases por acorde
  const char *bombo, *caja, *clap, *hat, *abierto, *bajo, *stabs;   // 16 caracteres = un compás de semicorcheas
  int8_t riffA, riffB;           // riffs de RIFFS[]: A en la primera mitad, B en la ruptura y el drop
};
struct BQ { float b0, b1, b2, a1, a2, z1, z2; };
struct VozBombo { float f, ph, env, eC, knock, kC, click, cC, amp; bool viva; BQ bp, hp; };
struct VozCaja  { float f, ph, ph2, env, eC, ruido, rC, crack, crC, amp; bool viva; BQ bp, cr, banda; };
struct VozArp { float f1, f2, inc1, inc2, inv1, inv2, amp, fenv, base; bool atacando, viva; Filtro s; };
struct VozPad { float f[3], inc[3], inv[3], amp; bool encendida, viva; };

// ==============================================================================================================================================
// CONFIGURACIÓN
// ==============================================================================================================================================
#define NUM_PISTAS        4
const char* const NOMBRES_PISTAS = "batería|bajo|riff|acordes";

const float  TECHO          = 0.89f;    // −1 dBFS: techo del limitador
const float  MASTER         = 0.80f;
const float  CURVA_VOLUMEN  = 1.8f;     // pot^1.8, como un fader real
const float  FILTRO_LP_MIN  = 110.0f;
const float  FILTRO_HP_MAX  = 5000.0f;
const float  HAAS_MS        = 11.0f;    // el pad se abre en estéreo con un retardo corto en el canal derecho
const float  ECO_FEEDBACK   = 0.38f;    // eco del riff
const float  ECO_MEZCLA     = 0.30f;
const unsigned long PLAY_MS  = 500;     // BTN1 mantenido medio segundo = play / stop
const unsigned long TEMA_MS  = 2500;    // BTN1 mantenido 2,5 s = siguiente tema
const float  TOMA_PERILLA   = 0.04f;

// ─── Temas ───────────────────────────────────────────────────
// Trance con peso, en la línea de Chemical Brothers («Hey Boy Hey Girl»): bombo y bajo pesados, un riff
// corto que se repite y SIEMPRE calza con el acorde, stabs de acordes rave y builds hacia el drop.
// La armonía es menor natural y consonante: i, iv, v, VI y VII. Nada de notas «de color» que choquen
// con el acorde (se rechazaron), y el riff sólo toca notas del acorde que está sonando.
// Cada tema es un viaje de 32 compases:
//   1–8   A        batería, bajo y el riff A (sin pad: la entrada)
//   9–16  A'       entra el pad filtrado, los hats abiertos y, en la segunda mitad, los stabs
//   17–24 RUPTURA  sin bombo ni bajo: el pad se abre de a poco (el build), el riff B a media densidad,
//                  y en los compases 23–24 un redoble y una subida de ruido
//   25–32 DROP     todo: pad bombeando con el bombo, stabs y el riff B
// Batería: X acento · x normal · . fantasma · - nada. Bajo: x tónica · o octava · 5 quinta.
// Riffs: índices a las notas del acorde (0–2, y 3–5 una octava arriba); −1 = silencio.
const int8_t RIFFS[6][16] = {
  { 3,-1, 3, 0, -1, 3,-1, 2,  3,-1, 3, 0, -1, 4,-1, 2 },     // 0 la octava que martilla, sincopada
  { 0, 0,-1, 2, -1, 0,-1, 1,  0, 0,-1, 2, -1, 3,-1, 1 },     // 1 grave y repetido
  { 3,-1,-1, 3, -1,-1, 3,-1,  4,-1,-1, 4, -1,-1, 5,-1 },     // 2 3-3-2: el gancho rave
  { 0,-1, 1,-1,  2,-1, 1,-1,  0,-1, 1,-1,  2,-1, 3,-1 },     // 3 corcheas que suben y bajan
  { 5, 4, 3,-1,  4, 3, 2,-1,  3, 2, 1,-1,  2, 1, 0,-1 },     // 4 cascada que baja
  { 0, 3, 0, 3,  1, 4, 1, 4,  2, 5, 2, 5,  1, 4, 1, 4 },     // 5 el «gate» del trance
};
const Tema TEMAS[] = {
  // La menor: i · i · VI · VII  →  VI · VII · i · i
  { "Fuego", 132, 57, { {0,'m'}, {0,'m'}, {8,'M'}, {10,'M'} }, { {8,'M'}, {10,'M'}, {0,'m'}, {0,'m'} },
    "X---x---X---x---", "----X-------X---", "----x-------x---", "x.X.x.X.x.X.x.X.", "--x---x---x---x-", "-xxx-xxx-xxx-xxo", "x--x--x---x--x--", 0, 2 },
  // Mi menor: i · VII · VI · VII  →  i · VI · iv · v
  { "Pulso", 136, 52, { {0,'m'}, {10,'M'}, {8,'M'}, {10,'M'} }, { {0,'m'}, {8,'M'}, {5,'m'}, {7,'m'} },
    "X---x---X---x-x-", "----X-------X---", "----x-------x--x", ".xX..xX..xX..xX.", "--x---x---x---x-", "--x---x---x---x-", "--x--x--x--x--x-", 5, 1 },
  // Re menor (big beat, más lento): i · iv · i · iv  →  VI · iv · VII · i
  { "Motor", 128, 50, { {0,'m'}, {5,'m'}, {0,'m'}, {5,'m'} }, { {8,'M'}, {5,'m'}, {10,'M'}, {0,'m'} },
    "X---x---X---x---", "----X--.----X---", "----x-------x---", "x-X-x-X-x-X-x-X-", "------x-------x-", "-x-x--x--x-x--xo", "x-----x---x-----", 3, 4 },
  // Fa# menor: i · VI · iv · v  →  i · VI · VII · VII
  { "Trueno", 138, 54, { {0,'m'}, {8,'M'}, {5,'m'}, {7,'m'} }, { {0,'m'}, {8,'M'}, {10,'M'}, {10,'M'} },
    "X---x---X---x---", "----X-------X---", "----x-------x---", "x.X.x.X.x.X.x.X.", "--x---x---x---x-", "-xxx-xxx-xxx-xxx", "--x---x---x---x-", 2, 0 },
  // Si menor: i · i · iv · iv  →  VI · VII · iv · v
  { "Aurora", 140, 47, { {0,'m'}, {0,'m'}, {5,'m'}, {5,'m'} }, { {8,'M'}, {10,'M'}, {5,'m'}, {7,'m'} },
    "X---x---X---x---", "----X-------X---", "----x-------x---", ".xX..xX..xX..xX.", "--x---x---x---x-", "-xxx-xxx-xxx-xx5", "x--x--x---x--x--", 1, 5 },
  // Sol menor: i · v · VI · iv  →  iv · v · i · i
  { "Vértigo", 134, 55, { {0,'m'}, {7,'m'}, {8,'M'}, {5,'m'} }, { {5,'m'}, {7,'m'}, {0,'m'}, {0,'m'} },
    "X---x---X---x-x-", "----X-------X---", "----x--x----x---", "x-X-x-X-x-X-x-X-", "------x-------x-", "-xx--xx--xx--xx-", "---x--x----x--x-", 4, 3 },
};
const int NUM_TEMAS = sizeof(TEMAS) / sizeof(TEMAS[0]);

// ─── Pines ───────────────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SDA_PIN   21
#define SCL_PIN   38
#define LED_PIN   46
#define NUM_LEDS   6
#define LED_BRILLO 70
const uint8_t BTN_PIN[5]   = { 44, 42, 0, 45, 47 };
const uint8_t POT_PIN[4]   = { 1, 2, 8, 10 };   // POT1..POT4, el pinout fijo de PROMPT_PARA_LA_IA.md
// Pistas internas: 0 batería · 1 bajo · 2 riff · 3 acordes (el mismo orden que mezclador_pistas)
const int GENERAL_PISTA[4] = { 1, 2, 0, 3 };   // banco general: POT1 bajo · POT2 riff · POT3 batería · POT4 acordes
const int BANCO_PISTA[4]   = { 1, 2, 0, 3 };   // BTN2 bajo · BTN3 riff · BTN4 batería · BTN5 acordes
#define HIST           32768                   // historia por pista para el repeat: 0,74 s (un pulso hasta 81 BPM)
#define ECO_MAX        16384                   // eco: hasta 0,37 s (corchea con punto desde 122 BPM)
const uint8_t PIEZO_PIN[4] = { 4, 5, 6, 7 };
const int     PIEZO_UMBRAL = 500;

// ─── Audio ───────────────────────────────────────────────────
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
const float SR     = (float)SAMPLE_RATE;
const float PI_F   = 3.14159265f;
#define PASOS_LOOP      512                    // 32 compases de 16 semicorcheas: A · A' · ruptura · drop

// ==============================================================================================================================================
// ESTADO COMPARTIDO ENTRE NÚCLEOS (cada variable tiene UN solo escritor)
//   control → audio : vol[], filtro[], reso[], efecto[], intensidad[], reproducir, pedidoTema
//   audio   → control: nivelPista[], golpes, posición
// ==============================================================================================================================================
volatile bool     reproducir = false;
volatile bool     pedidoTema = false, infoNueva = true;
volatile int      temaActual = 0;
volatile float    vol[NUM_PISTAS]    = { 0.8f, 0.8f, 0.8f, 0.8f };
volatile float    filtro[NUM_PISTAS] = { 0, 0, 0, 0 };
volatile float    reso[NUM_PISTAS]   = { 0, 0, 0, 0 };
volatile int      efecto[NUM_PISTAS] = { 0, 0, 0, 0 };
volatile float    intensidad[NUM_PISTAS] = { .7f, .7f, .7f, .7f };
volatile int      banco = 0;
volatile uint32_t muestraLoop = 0;               // posición dentro del loop, en muestras
volatile uint32_t largoLoop = 1;                 // largo del loop en muestras
volatile float    bpm = 138.0f;
volatile float    nivelPista[NUM_PISTAS] = { 0, 0, 0, 0 };
volatile uint32_t golpesPista[NUM_PISTAS] = { 0, 0, 0, 0 };
volatile uint32_t golpesKick = 0, golpesCaja = 0, golpesHat = 0;
int16_t *hist = nullptr;                         // [NUM_PISTAS][HIST], PSRAM si hay
int16_t *ecoL = nullptr, *ecoR = nullptr;        // eco ping-pong del arpegio

// ==============================================================================================================================================
// UTILIDADES DSP
// ==============================================================================================================================================
// Medido en la placa: la primera versión calculaba todo muestra a muestra, saltando entre ~15 voces en
// cada muestra, con divisiones y exp2f adentro. Costaba ~7.400 ciclos por muestra de los 5.442 que hay a
// 240 MHz: el DMA se quedaba sin datos, metía silencio (los «clipeos») y el tempo se arrastraba.
// Por eso el motor va por BLOQUES: cada voz se calcula de corrido para todo el bloque con su estado en
// variables locales (registros), no hay divisiones ni libm por muestra (los coeficientes de los filtros
// se recalculan cada 16 muestras con aproximaciones), y todo lo que corre 44 100 veces por segundo vive
// en IRAM (la PSRAM comparte la caché con el código en flash).
#define TABLA 1024
float SENO[TABLA + 1];
static inline float seno(float fase) {           // fase 0..1
  float x = fase * TABLA; int i = (int)x; float f = x - i;
  return SENO[i] + (SENO[i + 1] - SENO[i]) * f;
}
uint32_t semilla = 22222;
static inline float ruido(uint32_t &s) { s = s * 1664525u + 1013904223u; return (int32_t)s * (1.0f / 2147483648.0f); }
static inline float tanRapido(float x) { float x2 = x * x; return x * (15.0f - x2) / (15.0f - 6.0f * x2); }   // Padé, error < 0.2 % hasta 13 kHz
static inline float exp2Rapido(float x) {        // 2^x para x ≥ 0, error < 0.2 %
  int e = (int)x; float f = x - e;
  float p = 1.0f + f * (0.6951786f + f * (0.2261487f + f * 0.0782451f));
  union { float f; int32_t i; } u; u.f = p; u.i += e << 23; return u.f;
}
static inline float decaimiento(float tau) { return expf(-1.0f / (tau * SR)); }
static inline float suave(float x) {             // saturación cúbica (sin división), para voces con UN parcial fuerte
  if (x > 1.5f) x = 1.5f; else if (x < -1.5f) x = -1.5f;
  return x - x * x * x * (4.0f / 27.0f);
}
static inline float sierra(float &f, float inc, float inv) {   // diente de sierra con PolyBLEP; inv = 1/inc (sin dividir)
  float y = 2.0f * f - 1.0f;
  if (f < inc) { float t = f * inv; y -= t + t - t * t - 1.0f; }
  else if (f > 1.0f - inc) { float t = (f - 1.0f) * inv; y -= t * t + t + t + 1.0f; }
  f += inc; if (f >= 1.0f) f -= 1.0f;
  return y;
}
static inline void coefFiltro(Filtro &s, float fc, float q) {
  if (fc > 12500.0f) fc = 12500.0f; if (fc < 20.0f) fc = 20.0f;
  float g = tanRapido(PI_F * fc / SR); s.k = 1.0f / q;
  s.a1 = 1.0f / (1.0f + g * (g + s.k)); s.a2 = g * s.a1; s.a3 = g * s.a2;
}
static inline void pasoFiltro(Filtro &s, float x, float &lp, float &bp) {
  float v3 = x - s.ic2, v1 = s.a1 * s.ic1 + s.a2 * v3, v2 = s.ic2 + s.a2 * s.ic1 + s.a3 * v3;
  s.ic1 = 2.0f * v1 - s.ic1; s.ic2 = 2.0f * v2 - s.ic2;
  lp = v2; bp = v1 * s.k;                        // pasa-banda con ganancia 1 en el centro
}
static inline float hz(float midi) { return 440.0f * exp2f((midi - 69.0f) / 12.0f); }

// ==============================================================================================================================================
// SÍNTESIS — estado de las voces (todo lo toca sólo la tarea de audio)
// ==============================================================================================================================================
// ─── BATERÍA: las recetas de drum_poder (kit NEXO, el híbrido 808/909) ───
// Tres envolventes por golpe donde hace falta: el mazo tiene que morir en 14 ms mientras el cuerpo
// grave sigue 300 ms. Arriba de 1 kHz todo es RUIDO pasa-banda (no tiene nota) y cada pista lleva su
// pasa-altos de banda, que es lo que deja el grave entero al bombo: la mezcla fija de drum_poder.
static inline float pasoBQ(BQ &q, float x) { float y = q.b0 * x + q.z1; q.z1 = q.b1 * x - q.a1 * y + q.z2; q.z2 = q.b2 * x - q.a2 * y; return y; }
void bqBPF(BQ &c, float fc, float Q) {
  float w = 2.0f * PI_F * fc / SR, s = sinf(w), co = cosf(w), al = s / (2.0f * Q), a0 = 1.0f + al;
  c.b0 = al / a0; c.b1 = 0.0f; c.b2 = -al / a0; c.a1 = -2.0f * co / a0; c.a2 = (1.0f - al) / a0;
}
void bqHPF(BQ &c, float fc, float Q) {
  float w = 2.0f * PI_F * fc / SR, s = sinf(w), co = cosf(w), al = s / (2.0f * Q), a0 = 1.0f + al;
  c.b0 = (1.0f + co) * 0.5f / a0; c.b1 = -(1.0f + co) / a0; c.b2 = c.b0; c.a1 = -2.0f * co / a0; c.a2 = (1.0f - al) / a0;
}
static inline float softClip(float x) {          // tanh suave de drum_poder (Padé)
  if (x > 3.0f) return 1.0f; if (x < -3.0f) return -1.0f;
  float x2 = x * x; return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
// BOMBO — tres bandas: fundamental que aterriza en 46 Hz (el peso), 2.º armónico (el pecho) con
// saturación propia (tiene UNA parcial fuerte), mazo de RUIDO pasa-banda 14 ms y click de 5 ms.
const float NIVEL_BATERIA = 0.70f;
const float K_FREQ = 46.0f, K_RATIO = 4.8f, K_DROP = 0.030f, K_DEC = 0.30f, K_SAT = 1.70f;
const float K_KNOCK = 0.22f, K_KNOCK_F = 1200.0f, K_CLICK = 0.28f;
// CAJA — dos parciales de cuerpo que caen 15 %, bordonera de ruido, crack de 3,4 kHz. Sin saturación.
const float S_F1 = 195.0f, S_F2 = 305.0f, S_DEC = 0.055f, S_FC = 2300.0f, S_Q = 1.5f, S_NDEC = 0.085f, S_MIX = 0.70f, S_CRACK = 0.32f;
VozBombo bombos[2]; int sigBombo = 0;             // dos instancias: la que sonaba se apaga en 3 ms mientras entra la nueva
VozCaja  cajas[2];  int sigCaja = 0;
struct { float env, eC, cola, colaC, amp; int t; BQ bp, bpCola, banda; } clap;   // 3 palmadas (0 · 6,5 · 13 ms) + cola
struct { float env, eC, amp; BQ bp, banda; } hat;                                // cerrado y abierto comparten parche (el choke)
struct { float env, eC, atk, amp; BQ bp, banda; } plato;
float fCoefBombo, fCoefCaja, dRapido3;

// ─── BAJO: el de bajo_8_pasos ───
// Dos sierras PolyBLEP (una +9 cents) + una cuadrada una octava abajo → saturación suave → SVF
// pasa-bajos con envolvente de filtro (3,2 octavas, cierra antes que el volumen) → VCA.
const float BAJO_CORTE = 140.0f, BAJO_Q = 2.2f, BAJO_DEC = 0.14f, BAJO_SUB = 0.32f, BAJO_DRIVE = 1.25f;
struct { float f1, f2, fs, inc1, inc2, incS, inv1, inv2, invS, env, envF; bool atk, atkF; Filtro s; } bajo;
float dBajo, dBajoF, bajoComp;

// ─── RIFF: pluck de dos sierras con filtro por nota; SIEMPRE notas del acorde ───
#define VOCES_RIFF 6
VozArp riff[VOCES_RIFF]; int sigRiff = 0;
float dRiffAmp, dRiffFenv;

// ─── ACORDES: pad supersaw (2 sierras ±9 cents por nota) + stabs rave + subida de ruido antes del drop ───
#define VOCES_PAD 6
VozPad pad[VOCES_PAD];
Filtro filtroPad;
float padNivel = 0, padNivelObj = 0, padCorte = 1000, dPadAtk, dPadRel;
struct { float f[3], inc[3], inv[3], amp, fenv; bool viva; Filtro s; } stab;
struct { float prog, paso; Filtro s; bool activa; } subida;
float dStabAmp, dStabFenv;

float scObj = 0, sc = 0;                         // sidechain del bombo
float dRapido, dSc;
int   ecoN = 10000, ecoIdx = 0; float ecoLp = 0;
uint32_t relojCoef = 0;                          // reparte el recálculo de coeficientes entre muestras

// Secuenciador
int      paso = -1;
uint32_t sigPaso = 0;
float    sps = 4793.0f;                          // muestras por semicorchea
int      notasPad[3], tonosRiff[6], notaBajo = 33;
int      acordeAct = -1;

void prepararSintesis() {
  for (int i = 0; i <= TABLA; i++) SENO[i] = sinf(2.0f * PI_F * i / TABLA);
  fCoefBombo = 1.0f - expf(-1.0f / (K_DROP * SR)); fCoefCaja = 1.0f - expf(-1.0f / (0.035f * SR));
  dRapido3 = decaimiento(0.003f);
  bajoComp = 1.0f / powf(BAJO_Q, 0.30f); dBajo = decaimiento(BAJO_DEC); dBajoF = decaimiento(BAJO_DEC * 0.55f);
  dRiffAmp = decaimiento(0.20f); dRiffFenv = decaimiento(0.10f);
  dPadAtk = 1.0f - decaimiento(0.12f); dPadRel = decaimiento(0.40f);
  dStabAmp = decaimiento(0.17f); dStabFenv = decaimiento(0.07f);
  dRapido = decaimiento(0.0015f); dSc = decaimiento(0.14f);
  for (int i = 0; i < 2; i++) {
    bqBPF(bombos[i].bp, K_KNOCK_F, 0.8f); bqHPF(bombos[i].hp, 3000.0f, 0.7f);
    bqBPF(cajas[i].bp, S_FC, S_Q); bqBPF(cajas[i].cr, 3400.0f, 0.9f); bqHPF(cajas[i].banda, 150.0f, 0.707f);
  }
  bqBPF(clap.bp, 1200.0f, 1.6f); bqBPF(clap.bpCola, 1140.0f, 1.28f); bqHPF(clap.banda, 900.0f, 0.707f);
  bqBPF(hat.bp, 8000.0f, 0.55f); bqHPF(hat.banda, 2500.0f, 0.707f);
  bqBPF(plato.bp, 4700.0f, 0.45f); bqHPF(plato.banda, 1500.0f, 0.707f);
}

// Lleva una nota a la octava [desde, desde+12)
int enOctava(int n, int desde) { int pc = ((n - desde) % 12 + 12) % 12; return desde + pc; }
int compararInt(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

void elegirAcorde(const Tema &t, const Acorde &c) {
  int r = t.tonica + c.raiz;
  int n[3] = { r, r + (c.tipo == 'M' ? 4 : 3), r + 7 };
  for (int i = 0; i < 3; i++) notasPad[i] = enOctava(n[i], t.tonica - 2);       // pad: alrededor de la tónica
  qsort(notasPad, 3, sizeof(int), compararInt);
  int a[3]; for (int i = 0; i < 3; i++) a[i] = enOctava(n[i], t.tonica + 7);    // riff: arriba del pad
  qsort(a, 3, sizeof(int), compararInt);
  for (int i = 0; i < 3; i++) { tonosRiff[i] = a[i]; tonosRiff[i + 3] = a[i] + 12; }
  notaBajo = enOctava(r, 33);                                                    // bajo: La1 (55 Hz) a Sol#2
}

// ─── disparos ────────────────────────────────────────────────
static inline float vel(char c) { return c == 'X' ? 1.0f : c == 'x' ? 0.75f : 0.35f; }   // X acento · x normal · . fantasma
void golpeBombo(float v) {
  VozBombo &a = bombos[sigBombo]; if (a.viva) { a.eC = a.kC = a.cC = dRapido3; }   // el parche anterior se calla en 3 ms
  sigBombo ^= 1; VozBombo &b = bombos[sigBombo];
  b.f = K_FREQ * K_RATIO; b.ph = 0.0f; b.env = 1.0f; b.knock = 1.0f; b.click = 1.0f; b.amp = v; b.viva = true;
  b.eC = decaimiento(K_DEC); b.kC = decaimiento(0.014f); b.cC = decaimiento(0.005f);
  scObj = 1.0f; golpesKick++;
}
void golpeCaja(float v, float decMul) {
  VozCaja &a = cajas[sigCaja]; if (a.viva) { a.eC = a.rC = a.crC = dRapido3; }
  sigCaja ^= 1; VozCaja &b = cajas[sigCaja];
  b.f = S_F1; b.ph = 0.0f; b.ph2 = 0.0f; b.env = 1.0f; b.ruido = 1.0f; b.crack = 1.0f; b.amp = 0.85f * v; b.viva = true;
  b.eC = decaimiento(S_DEC * decMul); b.rC = decaimiento(S_NDEC * decMul); b.crC = decaimiento(0.008f);
  golpesCaja++;
}
void golpeClap(float v) { clap.env = 1.0f; clap.cola = 1.0f; clap.t = 0; clap.amp = 0.34f * v; clap.eC = decaimiento(0.006f); clap.colaC = decaimiento(0.060f); }
void golpeHat(bool abierto, float v) { hat.env = 1.0f; hat.eC = decaimiento(abierto ? 0.075f : 0.018f); hat.amp = 0.28f * v; golpesHat++; }
void golpePlato() { plato.env = 1.0f; plato.atk = 0.0f; plato.eC = decaimiento(0.22f); plato.amp = 0.26f; }
void golpeBajo(int nota) {
  float f = hz(nota);
  bajo.inc1 = f / SR; bajo.inc2 = bajo.inc1 * 1.00521f; bajo.incS = bajo.inc1 * 0.5f;   // +9 cents · una octava abajo
  bajo.inv1 = 1.0f / bajo.inc1; bajo.inv2 = 1.0f / bajo.inc2; bajo.invS = 1.0f / bajo.incS;
  bajo.atk = true; bajo.atkF = true;              // sube desde donde estaba: sin clic, sin reiniciar la fase
}
void golpeRiff(int nota) {
  VozArp &v = riff[sigRiff]; sigRiff = (sigRiff + 1) % VOCES_RIFF;
  float f = hz(nota);
  v.inc1 = f * 0.994f / SR; v.inc2 = f * 1.006f / SR; v.inv1 = 1.0f / v.inc1; v.inv2 = 1.0f / v.inc2;
  v.base = f * 1.6f; v.atacando = true; v.fenv = 1.0f; v.viva = true;
  coefFiltro(v.s, v.base * exp2Rapido(3.0f), 1.4f);
}
void cambiarPad() {
  for (int i = 0; i < VOCES_PAD; i++) pad[i].encendida = false;          // las que sonaban se sueltan con su cola
  for (int k = 0; k < 3; k++) {                                          // las 3 notas nuevas toman las voces más calladas
    int m = 0; float menor = 9;
    for (int i = 0; i < VOCES_PAD; i++) if (!pad[i].encendida && pad[i].amp < menor) { menor = pad[i].amp; m = i; }
    VozPad &v = pad[m]; float f = hz(notasPad[k]);
    const float desafino[2] = { 0.9948f, 1.0052f };                    // 2 sierras ±9 cents: ancho de supersaw a 2/3 del costo
    for (int j = 0; j < 2; j++) { v.inc[j] = f * desafino[j] / SR; v.inv[j] = 1.0f / v.inc[j]; }
    if (v.amp < 1e-4f) for (int j = 0; j < 2; j++) v.f[j] = (ruido(semilla) + 1.0f) * 0.5f;   // fases sueltas: la «supersaw» no parte en fase
    v.encendida = true; v.viva = true;
  }
}
void golpeStab() {                               // el acorde del momento, una octava arriba del pad
  for (int k = 0; k < 3; k++) { float f = hz(notasPad[k] + 12) * (k == 1 ? 1.003f : 1.0f); stab.inc[k] = f / SR; stab.inv[k] = 1.0f / stab.inc[k]; }
  stab.amp = 1.0f; stab.fenv = 1.0f; stab.viva = true; golpesPista[3]++;
}

// Un paso del secuenciador (semicorchea)
void avanzarPaso() {
  const Tema &t = TEMAS[temaActual];
  paso++;
  if (paso >= PASOS_LOOP) { paso = 0; muestraLoop = muestraLoop - largoLoop; }
  int s = paso & 15, compas = paso >> 4;
  int seccion = compas >> 3;                                              // 0 A · 1 A' · 2 ruptura · 3 drop
  bool ruptura = seccion == 2, conTodo = seccion != 0;
  int acorde = (compas >> 1) & 3, clave = (seccion >= 2) * 4 + acorde;
  if (clave != acordeAct) { acordeAct = clave; elegirAcorde(t, seccion >= 2 ? t.b[acorde] : t.a[acorde]); }
  if (s == 0 && (compas & 1) == 0) { cambiarPad(); golpesPista[3]++; }
  if (s == 0) {                                                           // el pad según la sección
    float pr = (compas & 7) / 8.0f;
    if (seccion == 0)      { padNivelObj = 0.0f;  padCorte = 900.0f; }
    else if (seccion == 1) { padNivelObj = 0.55f; padCorte = 1000.0f + 700.0f * pr; }
    else if (seccion == 2) { padNivelObj = 1.0f;  padCorte = 800.0f * exp2f(2.6f * pr); }   // se abre toda la ruptura: el build
    else                   { padNivelObj = 0.85f; padCorte = 3600.0f; }
  }

  // batería (callada en la ruptura, salvo el redoble final)
  bool golpe = false;
  if (!ruptura) {
    if (t.bombo[s] != '-') { golpeBombo(vel(t.bombo[s])); golpe = true; }
    if (t.caja[s] != '-')  { golpeCaja(vel(t.caja[s]), 1.0f); golpe = true; }
    if (t.clap[s] != '-')  golpeClap(vel(t.clap[s]));
    if (conTodo && t.abierto[s] == 'x') golpeHat(true, 0.8f);
    else if (t.hat[s] != '-') golpeHat(false, vel(t.hat[s]));
  }
  if (compas == 22 || compas == 23) {                                     // redoble que crece hasta el drop (golpes cortos)
    int k = (compas - 22) * 16 + s;
    bool toca = compas == 23 || (s < 8 ? (s & 3) == 0 : (s & 1) == 0);
    if (toca) { golpeCaja(0.30f + 0.70f * k / 31.0f, 0.40f); golpe = true; }
  }
  if (s == 0 && (compas == 0 || compas == 16 || compas == 24)) golpePlato();
  if (golpe) golpesPista[0]++;

  // bajo (callado en la ruptura): un golpe por grupo de notas
  char b = t.bajo[s], antes = t.bajo[(s + 15) & 15];
  if (!ruptura && b != '-') {
    golpeBajo(b == 'o' ? notaBajo + 12 : b == '5' ? notaBajo + 7 : notaBajo);
    if (antes == '-') golpesPista[1]++;
  }
  // riff: A en la primera mitad, B en la ruptura y el drop; en la ruptura sólo las corcheas
  int a = RIFFS[seccion <= 1 ? t.riffA : t.riffB][s];
  if (ruptura && (s & 1)) a = -1;
  if (a >= 0) { golpeRiff(tonosRiff[a]); if ((s & 1) == 0) golpesPista[2]++; }
  // stabs rave: en el drop y en la segunda mitad de A'
  if ((seccion == 3 || (seccion == 1 && compas >= 12)) && t.stabs[s] == 'x') golpeStab();
  if (compas == 22 && s == 0) { subida.activa = true; subida.prog = 0; subida.paso = 1.0f / (32.0f * sps); }
  if (compas == 24 && s == 0) subida.activa = false;
  sigPaso = (uint32_t)((paso + 1) * sps);
}

void cargarTema(int i) {
  temaActual = i;
  const Tema &t = TEMAS[i];
  bpm = t.bpm; sps = SR * 60.0f / t.bpm / 4.0f;
  largoLoop = (uint32_t)(PASOS_LOOP * sps);
  paso = -1; sigPaso = 0; muestraLoop = 0; acordeAct = -1;
  memset(bombos, 0, sizeof(bombos)); memset(cajas, 0, sizeof(cajas)); memset(&clap, 0, sizeof(clap));
  memset(&hat, 0, sizeof(hat)); memset(&plato, 0, sizeof(plato)); memset(&bajo, 0, sizeof(bajo));
  memset(riff, 0, sizeof(riff)); memset(pad, 0, sizeof(pad)); memset(&stab, 0, sizeof(stab)); memset(&subida, 0, sizeof(subida));
  padNivel = 0; padNivelObj = 0;
  prepararSintesis();
  ecoN = (int)(3.0f * sps); if (ecoN > ECO_MAX - 1) ecoN = ECO_MAX - 1;   // corchea con punto, fija por tema
  if (ecoL) { memset(ecoL, 0, ECO_MAX * 2); memset(ecoR, 0, ECO_MAX * 2); }
  if (hist) memset(hist, 0, NUM_PISTAS * HIST * 2);
  infoNueva = true;
}

// ─── render por bloques: cada voz de corrido, estado en locales ───
float sintBuf[NUM_PISTAS][BUFFER_SAMPLES];
float scBuf[BUFFER_SAMPLES];

IRAM_ATTR void renderSidechain(float *o, int m) {  // baja en ~2 ms al llegar el bombo y vuelve en ~140 ms
  float obj = scObj, s = sc;
  for (int i = 0; i < m; i++) { obj *= dSc; s += (obj - s) * 0.012f; o[i] = s; }
  scObj = obj; sc = s;
}

IRAM_ATTR void renderBateria(float *o, int m) {
  for (int i = 0; i < m; i++) o[i] = 0.0f;
  uint32_t sem = semilla;
  for (int k = 0; k < 2; k++) {                  // bombo
    VozBombo &v = bombos[k]; if (!v.viva) continue;
    float f = v.f, ph = v.ph, env = v.env, kn = v.knock, cl = v.click; const float eC = v.eC, kC = v.kC, cC = v.cC, amp = v.amp;
    for (int i = 0; i < m; i++) {
      f += (K_FREQ - f) * fCoefBombo;
      ph += f * (1.0f / SR); if (ph >= 1.0f) ph -= 1.0f;
      float p2 = ph + ph; if (p2 >= 1.0f) p2 -= 1.0f;
      float y = softClip((seno(ph) + 0.10f * seno(p2)) * K_SAT) * 0.9f * env;
      if (kn > 1e-3f) y += pasoBQ(v.bp, ruido(sem)) * kn * (K_KNOCK * 1.6f);   // capas de ruido: sólo mientras se oyen
      if (cl > 1e-3f) y += pasoBQ(v.hp, ruido(sem)) * cl * (K_CLICK * 0.45f);
      o[i] += y * amp;
      env *= eC; kn *= kC; cl *= cC;
    }
    v.f = f; v.ph = ph; v.env = env; v.knock = kn; v.click = cl;
    if (env < 3e-4f && kn < 1e-3f) v.viva = false;   // −70 dB: el escalón ya es inaudible (y la voz no sigue ocupando CPU 2 s más)
  }
  for (int k = 0; k < 2; k++) {                  // caja
    VozCaja &v = cajas[k]; if (!v.viva) continue;
    float f = v.f, ph = v.ph, ph2 = v.ph2, env = v.env, rz = v.ruido, cr = v.crack; const float eC = v.eC, rC = v.rC, crC = v.crC, amp = v.amp;
    const float r2 = S_F2 / S_F1, tono = (1.0f - S_MIX) * 1.7f, nAmt = S_MIX * 1.5f, crAmt = S_CRACK * 0.9f;
    for (int i = 0; i < m; i++) {
      f += (S_F1 * 0.85f - f) * fCoefCaja;
      float dt = f * (1.0f / SR);
      ph += dt; if (ph >= 1.0f) ph -= 1.0f; ph2 += dt * r2; if (ph2 >= 1.0f) ph2 -= 1.0f;
      float y = (seno(ph) + 0.60f * seno(ph2)) * env * tono;
      if (rz > 1e-3f) y += pasoBQ(v.bp, ruido(sem)) * rz * nAmt;
      if (cr > 1e-3f) y += pasoBQ(v.cr, ruido(sem)) * cr * crAmt;
      o[i] += pasoBQ(v.banda, y) * amp;           // la banda de la caja: nada abajo de 150 Hz
      env *= eC; rz *= rC; cr *= crC;
    }
    v.f = f; v.ph = ph; v.ph2 = ph2; v.env = env; v.ruido = rz; v.crack = cr;
    if (env < 3e-4f && rz < 1e-3f) v.viva = false;
  }
  if (clap.env > 1e-3f || clap.cola > 1e-3f || clap.t < 600) {   // 3 palmadas a 0 · 6,5 · 13 ms + cola: suena a varias manos
    float env = clap.env, cola = clap.cola; int t = clap.t; const float eC = clap.eC, colaC = clap.colaC, amp = clap.amp;
    for (int i = 0; i < m; i++) {
      t++; if (t == 287 || t == 573) env = 1.0f;
      float y = pasoBQ(clap.bp, ruido(sem)) * env * 1.20f + pasoBQ(clap.bpCola, ruido(sem)) * cola * 0.60f;
      o[i] += pasoBQ(clap.banda, y) * amp;
      env *= eC; cola *= colaC;
    }
    clap.env = env; clap.cola = cola; clap.t = t;
  }
  if (hat.env > 1e-3f) {
    float env = hat.env; const float eC = hat.eC, amp = hat.amp * 1.02f;
    for (int i = 0; i < m; i++) { o[i] += pasoBQ(hat.banda, pasoBQ(hat.bp, ruido(sem)) * env) * amp; env *= eC; }
    hat.env = env;
  }
  if (plato.env > 1e-3f) {
    float env = plato.env, atk = plato.atk; const float eC = plato.eC, amp = plato.amp * 1.35f, ai = 1.0f / (0.004f * SR);
    for (int i = 0; i < m; i++) {
      if (atk < 1.0f) { atk += ai; if (atk > 1.0f) atk = 1.0f; } else env *= eC;   // abre en 4 ms
      o[i] += pasoBQ(plato.banda, pasoBQ(plato.bp, ruido(sem)) * env * atk) * amp;
    }
    plato.env = env; plato.atk = atk;
  }
  semilla = sem;
  for (int i = 0; i < m; i++) o[i] *= NIVEL_BATERIA;   // deja aire al limitador: si lo aplasta, el golpe se pierde
}

static inline float cuadrada(float &f, float inc, float inv) {   // cuadrada PolyBLEP = sierra − sierra desfasada
  float p2 = f + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
  float a = 2.0f * f - 1.0f, b = 2.0f * p2 - 1.0f;
  if (f < inc) { float t = f * inv; a -= t + t - t * t - 1.0f; } else if (f > 1.0f - inc) { float t = (f - 1.0f) * inv; a -= t * t + t + t + 1.0f; }
  if (p2 < inc) { float t = p2 * inv; b -= t + t - t * t - 1.0f; } else if (p2 > 1.0f - inc) { float t = (p2 - 1.0f) * inv; b -= t * t + t + t + 1.0f; }
  f += inc; if (f >= 1.0f) f -= 1.0f;
  return (a - b) * 0.5f;
}

IRAM_ATTR void renderBajo(float *o, const float *scb, int m) {
  if (!bajo.atk && bajo.env < 1e-5f) { for (int i = 0; i < m; i++) o[i] = 0.0f; return; }
  float f1 = bajo.f1, f2 = bajo.f2, fs = bajo.fs, env = bajo.env, envF = bajo.envF; bool atk = bajo.atk, atkF = bajo.atkF;
  const float i1 = bajo.inc1, i2 = bajo.inc2, iS = bajo.incS, v1 = bajo.inv1, v2 = bajo.inv2, vS = bajo.invS;
  const float comp = bajoComp;
  Filtro s = bajo.s; uint32_t c = relojCoef + 2;
  for (int i = 0; i < m; i++) {
    if (atk) { env += 1.0f / 88.0f; if (env >= 1.0f) { env = 1.0f; atk = false; } } else env *= env < 1e-3f ? dRapido : dBajo;
    if (atkF) { envF += 1.0f / 66.0f; if (envF >= 1.0f) { envF = 1.0f; atkF = false; } } else envF *= dBajoF;
    if (((c + i) & 15) == 0) coefFiltro(s, BAJO_CORTE * exp2Rapido(3.2f * envF), BAJO_Q);
    float mez = (sierra(f1, i1, v1) + sierra(f2, i2, v2)) * 0.5f + cuadrada(fs, iS, vS) * BAJO_SUB;
    float lp, bp; pasoFiltro(s, softClip(mez * BAJO_DRIVE) * 0.9f, lp, bp);
    o[i] = 0.62f * lp * comp * env * (1.0f - 0.30f * scb[i]);
  }
  bajo.f1 = f1; bajo.f2 = f2; bajo.fs = fs; bajo.env = env; bajo.envF = envF; bajo.atk = atk; bajo.atkF = atkF; bajo.s = s;
}

IRAM_ATTR void renderRiff(float *o, const float *scb, int m) {
  for (int i = 0; i < m; i++) o[i] = 0.0f;
  for (int k = 0; k < VOCES_RIFF; k++) {
    VozArp &v = riff[k];
    if (!v.viva) continue;
    float f1 = v.f1, f2 = v.f2, i1 = v.inc1, i2 = v.inc2, w1 = v.inv1, w2 = v.inv2;
    float amp = v.amp, fenv = v.fenv, base = v.base; bool atk = v.atacando; Filtro s = v.s;
    uint32_t c = relojCoef + 3 * k;
    for (int i = 0; i < m; i++) {
      if (atk) { amp += 1.0f / 88.0f; if (amp >= 1.0f) { amp = 1.0f; atk = false; } }
      else {
        amp *= amp < 1e-3f ? dRapido : dRiffAmp;     // bajo −60 dB: cola de 1,5 ms y recién ahí se libera
        if (amp < 8e-5f) { v.viva = false; break; }
      }
      fenv *= dRiffFenv;
      if (((c + i) & 15) == 0) coefFiltro(s, base * exp2Rapido(3.0f * fenv), 1.4f);
      float x = 0.5f * (sierra(f1, i1, w1) + sierra(f2, i2, w2)), lp, bp;
      pasoFiltro(s, x, lp, bp);
      o[i] += lp * amp;
    }
    v.f1 = f1; v.f2 = f2; v.amp = amp; v.fenv = fenv; v.atacando = atk; v.s = s;
  }
  for (int i = 0; i < m; i++) o[i] *= 0.42f * (1.0f - 0.25f * scb[i]);
}

IRAM_ATTR void renderAcordes(float *o, const float *scb, int m) {
  for (int i = 0; i < m; i++) o[i] = 0.0f;
  // pad: su nivel y su corte los fija la sección (en la ruptura se abre de a poco: el build)
  coefFiltro(filtroPad, padCorte, 0.85f);
  for (int k = 0; k < VOCES_PAD; k++) {
    VozPad &v = pad[k];
    if (!v.viva) continue;
    float a0 = v.f[0], a1 = v.f[1];
    const float i0 = v.inc[0], i1 = v.inc[1], w0 = v.inv[0], w1 = v.inv[1];
    float amp = v.amp; const bool enc = v.encendida;
    for (int i = 0; i < m; i++) {
      if (enc) amp += (1.0f - amp) * dPadAtk;
      else { amp *= amp < 1e-3f ? dRapido : dPadRel; if (amp < 8e-5f) { v.viva = false; break; } }
      o[i] += (sierra(a0, i0, w0) + sierra(a1, i1, w1)) * amp;
    }
    v.f[0] = a0; v.f[1] = a1; v.amp = amp;
  }
  { Filtro s = filtroPad; float nv = padNivel; const float obj = padNivelObj;
    for (int i = 0; i < m; i++) {
      nv += (obj - nv) * 0.0002f;                 // la sección entra y sale en ~110 ms
      float lp, bp; pasoFiltro(s, o[i], lp, bp); o[i] = 0.105f * nv * lp * (1.0f - 0.60f * scb[i]);
    }
    filtroPad = s; padNivel = nv; }
  if (stab.viva) {                               // stab rave: el acorde, corto, con el filtro que se cierra
    float a0 = stab.f[0], a1 = stab.f[1], a2 = stab.f[2], amp = stab.amp, fenv = stab.fenv; Filtro s = stab.s;
    const float i0 = stab.inc[0], i1 = stab.inc[1], i2 = stab.inc[2], w0 = stab.inv[0], w1 = stab.inv[1], w2 = stab.inv[2];
    uint32_t c = relojCoef + 9;
    for (int i = 0; i < m; i++) {
      amp *= amp < 1e-3f ? dRapido : dStabAmp;
      if (amp < 8e-5f) { stab.viva = false; break; }
      fenv *= dStabFenv;
      if (((c + i) & 15) == 0) coefFiltro(s, 700.0f * exp2Rapido(2.5f * fenv), 1.1f);
      float lp, bp; pasoFiltro(s, sierra(a0, i0, w0) + sierra(a1, i1, w1) + sierra(a2, i2, w2), lp, bp);
      o[i] += 0.13f * lp * amp * (1.0f - 0.35f * scb[i]);
    }
    stab.f[0] = a0; stab.f[1] = a1; stab.f[2] = a2; stab.amp = amp; stab.fenv = fenv; stab.s = s;
  }
  if (subida.activa) {                           // ruido que sube de 300 Hz a 6 kHz en los dos compases antes del drop
    uint32_t sem = semilla; float pr = subida.prog; const float dp = subida.paso; Filtro s = subida.s;
    uint32_t c = relojCoef + 13;
    for (int i = 0; i < m; i++) {
      pr += dp; if (pr > 1.0f) pr = 1.0f;
      if (((c + i) & 15) == 0) coefFiltro(s, 300.0f * exp2Rapido(4.3f * pr), 1.2f);
      float lp, bp; pasoFiltro(s, ruido(sem), lp, bp);
      o[i] += 0.14f * pr * pr * bp;
    }
    subida.prog = pr; subida.s = s; semilla = sem;
  }
}

// Llena sintBuf con las 4 pistas de un bloque, partiendo el bloque en cada semicorchea (golpes exactos a la muestra)
IRAM_ATTR void sintetizarBloque() {
  int n = 0; uint32_t pos = muestraLoop;
  while (n < BUFFER_SAMPLES) {
    if (pos >= sigPaso) { muestraLoop = pos; avanzarPaso(); pos = muestraLoop; }
    uint32_t quedan = sigPaso - pos;
    int m = BUFFER_SAMPLES - n; if (quedan < (uint32_t)m) m = (int)quedan;
    renderSidechain(scBuf + n, m);
    renderBateria(sintBuf[0] + n, m);
    renderBajo(sintBuf[1] + n, scBuf + n, m);
    renderRiff(sintBuf[2] + n, scBuf + n, m);
    renderAcordes(sintBuf[3] + n, scBuf + n, m);
    relojCoef += m; pos += m; n += m;
  }
  muestraLoop = pos;
}

// ==============================================================================================================================================
// CADENA POR PISTA (la de mezclador_pistas, también por bloques)
// ==============================================================================================================================================
SVF      svf[NUM_PISTAS];
float    volSuave[NUM_PISTAS] = { 0, 0, 0, 0 };
float    fundido = 0.0f;
float    nivelSeg[NUM_PISTAS] = { 0, 0, 0, 0 };
#define HAAS_MAX 1024
float    haas[HAAS_MAX]; int haasIdx = 0; int haasN = 485;
#define LIM_LOOK 64
float limDlyL[LIM_LOOK], limDlyR[LIM_LOOK];
int   limDlyIdx = 0;
float limEnv = 0.0f, limRel = 0.0f, limGain = 1.0f;
float dcX1L = 0, dcY1L = 0, dcX1R = 0, dcY1R = 0;
Biquad techoL, techoR;                           // pasa-bajos de 13 kHz del master

void calcularFiltro(int p, float f, float r) {
  float fc, q;
  if (f <= 0.0f) { svf[p].modo = 0; fc = 18000.0f * powf(FILTRO_LP_MIN / 18000.0f, -f); }
  else           { svf[p].modo = 1; fc = 20.0f    * powf(FILTRO_HP_MAX / 20.0f,     f); }
  float tope = 1.15f - fc / 9000.0f; tope = tope < 0.15f ? 0.15f : tope > 1.0f ? 1.0f : tope;
  q = 0.707f + 0.5f * fabsf(f) + r * 7.0f * tope;
  float g = tanf(PI_F * fc / SR), k = 1.0f / q;
  svf[p].k = k; svf[p].a1 = 1.0f / (1.0f + g * (g + k)); svf[p].a2 = g * svf[p].a1; svf[p].a3 = g * svf[p].a2;
  svf[p].comp = 1.0f / (1.0f + 0.22f * (q - 0.707f));
}
static inline float techoSuave(float x) {
  float a = fabsf(x); if (a <= 0.95f) return x;
  float y = 0.95f + 0.05f * tanhf((a - 0.95f) / 0.05f); return x < 0 ? -y : y;
}
static inline float pasoBiquad(Biquad &q, float x) {
  float y = q.b0 * x + q.z1; q.z1 = q.b1 * x - q.a1 * y + q.z2; q.z2 = q.b2 * x - q.a2 * y; return y;
}
void prepararDSP() {
  for (int p = 0; p < NUM_PISTAS; p++) { memset(&svf[p], 0, sizeof(SVF)); calcularFiltro(p, 0.0f, 0.0f); nivelSeg[p] = 0; }
  memset(haas, 0, sizeof(haas)); haasN = (int)(HAAS_MS * 0.001f * SR);
  for (int i = 0; i < LIM_LOOK; i++) { limDlyL[i] = 0; limDlyR[i] = 0; }
  limRel = expf(-1.0f / (SR * 0.08f));
  // Butterworth de 2.º orden en 13 kHz (RBJ), DESPUÉS del limitador
  float w = 2.0f * PI_F * 13000.0f / SR, cw = cosf(w), al = sinf(w) / (2.0f * 0.7071f), a0 = 1.0f + al;
  Biquad q = { (1 - cw) / 2 / a0, (1 - cw) / a0, (1 - cw) / 2 / a0, -2 * cw / a0, (1 - al) / a0, 0, 0 };
  techoL = q; techoR = q;
}

const float DIV_EFECTO[7] = { 0, 0.5f, 0.25f, 0.5f, 0.25f, 0.125f, 0.0625f };
int   efectoAct[NUM_PISTAS] = { 0, 0, 0, 0 };
float mezclaEf[NUM_PISTAS]  = { 0, 0, 0, 0 };
float fSuave[NUM_PISTAS] = { 0, 0, 0, 0 }, rSuave[NUM_PISTAS] = { 0, 0, 0, 0 };
uint32_t muestraGlobal = 0;
float fundBuf[BUFFER_SAMPLES], mezL[BUFFER_SAMPLES], mezR[BUFFER_SAMPLES];

static inline int16_t a16(float x) { int32_t v = (int32_t)(x * 32767.0f); return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

IRAM_ATTR void procesarBloque(int16_t *salida, bool corre) {
  for (int p = 0; p < NUM_PISTAS; p++) {
    fSuave[p] += (filtro[p] - fSuave[p]) * 0.15f;
    rSuave[p] += (reso[p]   - rSuave[p]) * 0.15f;
    calcularFiltro(p, fSuave[p], rSuave[p]);
  }
  const float Lb = 4.0f * sps;                   // un pulso, en muestras
  const float pulso0 = corre ? fmodf((float)muestraLoop, Lb) : 0.0f;
  if (corre) sintetizarBloque(); else memset(sintBuf, 0, sizeof(sintBuf));
  { const float obj = reproducir ? 1.0f : 0.0f; float f = fundido;   // play y stop sin clic (~8 ms)
    for (int i = 0; i < BUFFER_SAMPLES; i++) { f += (obj - f) * 0.003f; fundBuf[i] = f; } fundido = f; }
  memset(mezL, 0, sizeof(mezL)); memset(mezR, 0, sizeof(mezR));
  const uint32_t g0 = muestraGlobal;

  for (int p = 0; p < NUM_PISTAS; p++) {
    float *x = sintBuf[p];
    if (hist) { int16_t *h = hist + p * HIST; for (int i = 0; i < BUFFER_SAMPLES; i++) h[(g0 + i) & (HIST - 1)] = a16(x[i]); }

    // efecto rítmico (gate / repeat), amarrado al pulso
    int pedido = corre ? efecto[p] : 0;
    if (!hist && pedido >= 3) pedido = 0;        // sin PSRAM no hay repeat
    int ef = efectoAct[p]; float mez = mezclaEf[p];
    if (ef || pedido || mez > 0.0005f) {
      const float inten = intensidad[p];
      float L = ef ? Lb * DIV_EFECTO[ef] : Lb, enP = pulso0, fase = fmodf(pulso0, L);
      const int16_t *h = hist ? hist + p * HIST : nullptr;
      for (int i = 0; i < BUFFER_SAMPLES; i++) {
        float objMezcla = (pedido == ef && pedido) ? inten : 0.0f;
        mez += (objMezcla - mez) * 0.0025f;
        if (pedido != ef && mez < 0.002f) { ef = pedido; if (ef) { L = Lb * DIV_EFECTO[ef]; fase = fmodf(enP, L); } }
        if (ef && mez > 0.0005f) {
          if (ef <= 2) {                         // gate: abierto en la primera mitad del trozo, rampas de 3 ms
            float media = 0.5f * L, env;
            if (fase < media) env = fase < 132.0f ? fase * (1.0f / 132.0f) : (media - fase < 132.0f ? (media - fase) * (1.0f / 132.0f) : 1.0f);
            else env = 0.0f;
            x[i] *= 1.0f - mez * (1.0f - env);
          } else {                               // repeat: el comienzo del trozo, sacado de la historia
            uint32_t atras = (uint32_t)(enP - fase);
            float rep = h[(g0 + i - atras) & (HIST - 1)] * (1.0f / 32768.0f);
            float e = fase < 66.0f ? fase * (1.0f / 66.0f) : 1.0f;
            if (L - fase < 66.0f) e = (L - fase) * (1.0f / 66.0f);
            x[i] = x[i] * (1.0f - mez) + rep * e * mez;
          }
        }
        enP += 1.0f; if (enP >= Lb) enP -= Lb;
        fase += 1.0f; if (fase >= L) fase -= L;
      }
    }
    efectoAct[p] = ef; mezclaEf[p] = mez;

    // filtro → volumen → nivel
    SVF s = svf[p]; float vs = volSuave[p], niv = nivelSeg[p];
    const float ov = powf(vol[p], CURVA_VOLUMEN);
    for (int i = 0; i < BUFFER_SAMPLES; i++) {
      float v3 = x[i] - s.ic2, v1 = s.a1 * s.ic1 + s.a2 * v3, v2 = s.ic2 + s.a2 * s.ic1 + s.a3 * v3;
      s.ic1 = 2.0f * v1 - s.ic1; s.ic2 = 2.0f * v2 - s.ic2;
      float y = (s.modo ? (x[i] - s.k * v1 - v2) : v2) * s.comp;
      vs += (ov - vs) * 0.004f;
      y *= vs * fundBuf[i];
      float a = fabsf(y); niv = a > niv ? a : niv * 0.99985f;
      x[i] = y;
    }
    svf[p] = s; volSuave[p] = vs; nivelSeg[p] = niv;

    // a la mezcla
    if (p == 2) {                                // riff + eco ping-pong a corchea con punto
      float lpE = ecoLp; int w = ecoIdx;
      for (int i = 0; i < BUFFER_SAMPLES; i++) {
        int rd = w - ecoN; if (rd < 0) rd += ECO_MAX;
        float dl = ecoL[rd] * (1.0f / 32768.0f), dr = ecoR[rd] * (1.0f / 32768.0f);
        lpE += (dr - lpE) * 0.40f;               // el eco se oscurece en cada vuelta (~3,5 kHz)
        ecoL[w] = a16(x[i] + ECO_FEEDBACK * lpE); ecoR[w] = a16(ECO_FEEDBACK * dl);
        w = (w + 1) & (ECO_MAX - 1);
        mezL[i] += x[i] + ECO_MEZCLA * dl; mezR[i] += x[i] + ECO_MEZCLA * dr;
      }
      ecoLp = lpE; ecoIdx = w;
    } else if (p == 3) {                         // acordes abiertos con un retardo de Haas
      int w = haasIdx;
      for (int i = 0; i < BUFFER_SAMPLES; i++) {
        float d = haas[(w - haasN + HAAS_MAX) & (HAAS_MAX - 1)];
        haas[w] = x[i]; w = (w + 1) & (HAAS_MAX - 1);
        mezL[i] += x[i]; mezR[i] += 0.85f * d + 0.15f * x[i];
      }
      haasIdx = w;
    } else for (int i = 0; i < BUFFER_SAMPLES; i++) { mezL[i] += x[i]; mezR[i] += x[i]; }
  }

  // master: bloqueador DC → limitador con lookahead → 13 kHz → techo suave
  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    float l = mezL[n] * MASTER, r = mezR[n] * MASTER;
    float yl = l - dcX1L + 0.9985f * dcY1L; dcX1L = l; dcY1L = yl; l = yl;
    float yr = r - dcX1R + 0.9985f * dcY1R; dcX1R = r; dcY1R = yr; r = yr;
    float pk = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
    if (pk > limEnv) limEnv = pk; else limEnv = pk + (limEnv - pk) * limRel;
    float limObj = (limEnv > TECHO) ? (TECHO / limEnv) : 1.0f;
    limGain += (limObj - limGain) * 0.020f;
    float dl = limDlyL[limDlyIdx], dr = limDlyR[limDlyIdx];
    limDlyL[limDlyIdx] = l; limDlyR[limDlyIdx] = r;
    limDlyIdx++; if (limDlyIdx >= LIM_LOOK) limDlyIdx = 0;
    l = techoSuave(pasoBiquad(techoL, dl * limGain)); r = techoSuave(pasoBiquad(techoR, dr * limGain));
    int32_t li = (int32_t)(l * 32000.0f), ri = (int32_t)(r * 32000.0f);
    salida[2 * n]     = (int16_t)(li > 32767 ? 32767 : li < -32768 ? -32768 : li);
    salida[2 * n + 1] = (int16_t)(ri > 32767 ? 32767 : ri < -32768 ? -32768 : ri);
  }
  muestraGlobal = g0 + BUFFER_SAMPLES;
  for (int p = 0; p < NUM_PISTAS; p++) nivelPista[p] = nivelSeg[p];
}

// ==============================================================================================================================================
// AUDIO (core 1)
// ==============================================================================================================================================
i2s_chan_handle_t tx_chan;

void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear    = true;
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;
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

// Carga de CPU: si un bloque tarda más del 80 % de su tiempo (2,9 ms), el LED 5 parpadea en magenta.
// Así, si algún día se oye un corte, se sabe si es falta de CPU sin conectar nada.
volatile unsigned long cargaAlta = 0;
void audioTask(void *) {
  static int16_t salida[BUFFER_SAMPLES * 2];
  const unsigned long PRESUPUESTO_US = (unsigned long)(0.8f * BUFFER_SAMPLES * 1e6f / SAMPLE_RATE);
  size_t n;
  for (;;) {
    bool corre = reproducir || fundido > 0.0005f;
    if (pedidoTema && !corre) { cargarTema((temaActual + 1) % NUM_TEMAS); pedidoTema = false; }
    unsigned long t0 = micros();
    procesarBloque(salida, corre);
    if (micros() - t0 > PRESUPUESTO_US) cargaAlta = millis();
    i2s_channel_write(tx_chan, salida, sizeof(salida), &n, portMAX_DELAY);
  }
}

// ==============================================================================================================================================
// CONTROL (core 0, 1 kHz): botones, pots, IMU, piezos, LEDs y la línea para las visuales
// ==============================================================================================================================================
CRGB leds[NUM_LEDS];
uint8_t imuAddr = 0x68;
float ax = 0, ay = 0;
bool  btnAbajo[5] = { false, false, false, false, false };
unsigned long btnDesde[5] = { 0, 0, 0, 0, 0 }, btnCambio[5] = { 0, 0, 0, 0, 0 };
bool  yaPlay = false, yaTema = false;
float potFis[4] = { 0, 0, 0, 0 };
float potRef[4] = { 0, 0, 0, 0 };
bool  tomado[4] = { true, true, true, true };
uint32_t golpesPiezo = 0; float velPiezo = 0;
int   piezoPico[4] = { 0, 0, 0, 0 }; unsigned long piezoDesde[4] = { 0, 0, 0, 0 }, piezoUltimo[4] = { 0, 0, 0, 0 };
const CRGB COLOR_PISTA[NUM_PISTAS] = { CRGB(255, 70, 20), CRGB(160, 40, 255), CRGB(0, 200, 255), CRGB(212, 255, 58) };

void iniciarIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Wire.beginTransmission(0x68); if (Wire.endTransmission() != 0) imuAddr = 0x69;
  Wire.beginTransmission(imuAddr); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(true);
  Wire.beginTransmission(imuAddr); Wire.write(0x1C); Wire.write(0x00); Wire.endTransmission(true);
}
bool imuVivo = true;
void leerIMU() {
  static unsigned long reintento = 0;
  if (!imuVivo && millis() - reintento < 3000) return;
  Wire.beginTransmission(imuAddr); Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom((int)imuAddr, 14, true);
  if (Wire.available() < 14) { imuVivo = false; reintento = millis(); iniciarIMU(); return; }
  imuVivo = true;
  int16_t rx = (Wire.read() << 8) | Wire.read();
  int16_t ry = (Wire.read() << 8) | Wire.read();
  for (int i = 0; i < 10; i++) Wire.read();
  ax += (rx / 16384.0f - ax) * 0.2f;
  ay += (ry / 16384.0f - ay) * 0.2f;
}

float leerPot(uint8_t pin) { uint32_t s = 0; for (int i = 0; i < 4; i++) s += analogRead(pin); return (s >> 2) / 4095.0f; }

void elegirBanco(int b) {
  banco = b;
  for (int i = 0; i < 4; i++) { potRef[i] = potFis[i]; tomado[i] = false; }
}

void pasoBotones(unsigned long t) {
  for (int b = 0; b < 5; b++) {
    bool abajo = digitalRead(BTN_PIN[b]) == LOW;
    if (abajo == btnAbajo[b] || t - btnCambio[b] < 25) continue;
    btnCambio[b] = t; btnAbajo[b] = abajo;
    if (abajo) {
      btnDesde[b] = t;
      if (b == 0) { yaPlay = false; yaTema = false; }
      if (b != banco) elegirBanco(b);
    }
  }
  if (btnAbajo[0] && !yaPlay && t - btnDesde[0] > PLAY_MS) { reproducir = !reproducir; yaPlay = true; }
  if (btnAbajo[0] && !yaTema && t - btnDesde[0] > TEMA_MS) { reproducir = false; yaTema = true; pedidoTema = true; }
}

void aplicarPerilla(int i) {
  if (!tomado[i]) { if (fabsf(potFis[i] - potRef[i]) > TOMA_PERILLA) tomado[i] = true; else return; }
  float v = potFis[i];
  if (banco == 0) { vol[GENERAL_PISTA[i]] = v; return; }
  int p = BANCO_PISTA[banco - 1];
  if (i == 0) {
    float f = v * 2.0f - 1.0f;
    f = fabsf(f) < 0.06f ? 0.0f : (f > 0 ? (f - 0.06f) / 0.94f : (f + 0.06f) / 0.94f);
    filtro[p] = f;
  } else if (i == 1) reso[p] = v;
  else if (i == 2) {
    float z = v * 6.999f; int act = efecto[p];
    if (z < act - 0.15f || z > act + 1.15f) efecto[p] = (int)z;
  } else intensidad[p] = v;
}

void pasoPiezos(unsigned long t) {
  for (int i = 0; i < 4; i++) {
    int v = analogRead(PIEZO_PIN[i]);
    if (piezoDesde[i]) {
      if (v > piezoPico[i]) piezoPico[i] = v;
      if (t - piezoDesde[i] >= 12) {
        velPiezo = (piezoPico[i] - PIEZO_UMBRAL) / (float)(4095 - PIEZO_UMBRAL);
        if (velPiezo < 0.05f) velPiezo = 0.05f;
        golpesPiezo++; piezoUltimo[i] = t; piezoDesde[i] = 0;
      }
    } else if (v > PIEZO_UMBRAL && t - piezoUltimo[i] > 60) { piezoDesde[i] = t; piezoPico[i] = v; }
  }
}

float pulsoFase(uint32_t pos, uint32_t &compas) {
  float x = (float)pos / (SR * 60.0f / bpm);
  compas = (uint32_t)x;
  return x - compas;
}

void renderLEDs(unsigned long t) {
  leds[0] = reproducir ? CRGB(0, 150, 40) : CRGB(150, 100, 0);
  bool parpadeo = (t % 400) < 200;
  if (banco == 0 && parpadeo) leds[0] = CRGB(200, 200, 200);
  for (int k = 0; k < 4; k++) {
    int p = BANCO_PISTA[k];
    if (banco == k + 1 && parpadeo) { leds[1 + k] = CRGB(220, 220, 220); continue; }
    float n = nivelPista[p] * 2.5f; if (n > 1) n = 1;
    CRGB c = COLOR_PISTA[p]; c.nscale8((uint8_t)(25 + 230 * n)); leds[1 + k] = c;
  }
  uint32_t compas; float fase = pulsoFase(muestraLoop, compas);
  float brillo = reproducir ? expf(-fase * 7.0f) : 0.0f;
  CRGB c5 = (compas % 4 == 0) ? CRGB(212, 255, 58) : CRGB(255, 255, 255); c5.nscale8((uint8_t)(brillo * 255)); leds[5] = c5;
  if (cargaAlta && t - cargaAlta < 600 && parpadeo) leds[5] = CRGB(255, 0, 200);
  FastLED.show();
}

void enviarLinea(unsigned long t) {
  static unsigned long ultInfo = 0;
  char b[360];
  if (t - ultInfo > 5000 || infoNueva) {
    ultInfo = t; infoNueva = false;
    snprintf(b, sizeof(b), "I,%s,PercuSynth Trance,%.2f,%lu,%d,%d,%s\n", TEMAS[temaActual].nombre, bpm,
             (unsigned long)(largoLoop / (SAMPLE_RATE / 1000)), temaActual + 1, NUM_TEMAS, NOMBRES_PISTAS);
    Serial.print(b);
  }
  uint32_t pos = muestraLoop, compas;
  float fase = pulsoFase(pos, compas);
  int vv[4], ff[4], nn[4];
  for (int p = 0; p < NUM_PISTAS; p++) {
    vv[p] = (int)(powf(vol[p], CURVA_VOLUMEN) * 1000);
    ff[p] = (int)(filtro[p] * 1000);
    float n = nivelPista[p] * 2.5f; nn[p] = (int)((n > 1 ? 1 : n) * 1000);
  }
  int btn = 0; for (int i = 0; i < 5; i++) if (btnAbajo[i]) btn |= 1 << i;
  int tom = 0; for (int i = 0; i < 4; i++) if (tomado[i]) tom |= 1 << i;
  snprintf(b, sizeof(b), "R,%lu,%d,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
    (unsigned long)(pos / (SAMPLE_RATE / 1000)), (int)(fase * 1000), (unsigned long)compas, reproducir ? 1 : 0,
    vv[0], vv[1], vv[2], vv[3], ff[0], ff[1], ff[2], ff[3], nn[0], nn[1], nn[2], nn[3],
    (unsigned long)golpesPista[0], (unsigned long)golpesPista[1], (unsigned long)golpesPista[2], (unsigned long)golpesPista[3],
    (unsigned long)golpesKick, (unsigned long)golpesCaja, (unsigned long)golpesHat,
    (unsigned long)golpesPiezo, (int)(velPiezo * 1000), (int)(ax * 1000), (int)(ay * 1000), btn,
    (int)banco, (int)(reso[0] * 1000), (int)(reso[1] * 1000), (int)(reso[2] * 1000), (int)(reso[3] * 1000),
    efecto[0], efecto[1], efecto[2], efecto[3],
    (int)(intensidad[0] * 1000), (int)(intensidad[1] * 1000), (int)(intensidad[2] * 1000), (int)(intensidad[3] * 1000), tom);
  Serial.print(b);                               // con setTxTimeoutMs(0): si nadie escucha, se descarta sin bloquear
}

void controlTask(void *) {
  unsigned long ultLinea = 0, ultLED = 0, ultIMU = 0;
  uint8_t scan = 0;
  for (;;) {
    unsigned long t = millis();
    pasoBotones(t);
    pasoPiezos(t);
    potFis[scan] = leerPot(POT_PIN[scan]); aplicarPerilla(scan); scan = (scan + 1) & 3;
    if (t - ultIMU >= 5)   { ultIMU = t; leerIMU(); }
    if (t - ultLED >= 20)  { ultLED = t; renderLEDs(t); }
    if (t - ultLinea >= 16){ ultLinea = t; enviarLinea(t); }
    vTaskDelay(1);
  }
}

// ==============================================================================================================================================
// SETUP
// ==============================================================================================================================================
void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // OBLIGATORIO con audio: un print por USB CDC bloquea hasta que el PC lea
  esp_log_level_set("i2c.master", ESP_LOG_NONE);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRILLO);
  FastLED.clear(); FastLED.show();

  for (int b = 0; b < 5; b++) pinMode(BTN_PIN[b], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  for (int i = 0; i < 4; i++) { potFis[i] = leerPot(POT_PIN[i]); vol[GENERAL_PISTA[i]] = potFis[i]; }

  // PSRAM si la hay; el eco cabe también en la RAM interna, la historia del repeat no
  // el eco va en RAM interna (se lee y escribe cada muestra); la historia del repeat, en PSRAM si la hay
  ecoL = (int16_t *)heap_caps_calloc(ECO_MAX, 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  ecoR = (int16_t *)heap_caps_calloc(ECO_MAX, 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!ecoL) ecoL = (int16_t *)heap_caps_calloc(ECO_MAX, 2, MALLOC_CAP_SPIRAM);
  if (!ecoR) ecoR = (int16_t *)heap_caps_calloc(ECO_MAX, 2, MALLOC_CAP_SPIRAM);
  hist = (int16_t *)heap_caps_calloc(NUM_PISTAS * HIST, 2, MALLOC_CAP_SPIRAM);
  if (!hist) Serial.println("Sin PSRAM: el trance suena igual, pero los «repeat» del POT3 quedan apagados");

  prepararDSP();
  cargarTema(0);
  iniciarIMU();
  i2s_init();
  reproducir = true;          // arranca sonando
  Serial.printf("Trance en pistas · %d temas · «%s» a %.0f BPM\n", NUM_TEMAS, TEMAS[0].nombre, TEMAS[0].bpm);
  xTaskCreatePinnedToCore(audioTask,   "audio",   8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask, "control", 6144, NULL,  3, NULL, 0);
}

void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }
