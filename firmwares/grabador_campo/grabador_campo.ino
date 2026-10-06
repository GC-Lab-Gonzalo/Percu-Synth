// ==============================================================================================================================================
// PERCUSYNTH - GRABADOR DE CAMPO (micrófono INMP441 → filtro, EQ y efectos en vivo → microSD) - GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo Sandoval - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
// ==============================================================================================================================================
// HARDWARE
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3 (PercuSynth, DevKitC-1 N16R8: hace falta la PSRAM).
//
// - DAC PCM5102 por I2S  ->  I2S_NUM_0 (SALIDA) a 44100 Hz, 16 bit, estéreo  = MONITOR con audífonos
//       I2S LCK 39 · DIN 40 · BCK 41
//
// - Micrófono INMP441 por I2S  ->  I2S_NUM_1 (ENTRADA) a 44100 Hz, 24 bit (la MISMA tasa que el DAC):
//       WS 11 · SCK 12 · SD 13 · L/R a GND · VDD a 3.3 V (NUNCA 5 V)
//
// - Módulo microSD por SPI (el de 6 pines con regulador, o uno de 3.3 V):
//       SCK ...... GPIO 14
//       MOSI ..... GPIO 15
//       MISO ..... GPIO 16
//       CS ....... GPIO 17
//       VCC ...... 5 V si el módulo trae regulador (el azul común), 3.3 V si no · GND a GND
//   Tarjeta formateada en FAT32 (hasta 32 GB tal cual; una de 64 GB hay que formatearla FAT32 a mano).
//
// - Botones (INPUT_PULLUP, presionado = LOW): 44, 42, 0, 45, 47
// - Potenciómetros: ADC 1, 2, 8, 10
// - Indicadores: 6 LEDs SMD WS2812 on-board (data GPIO 46)
//
// ¡AUDÍFONOS para monitorear! Con parlante el micrófono se escucha a sí mismo y se acopla.
// ==============================================================================================================================================
// ARDUINO IDE SETTINGS
// ==============================================================================================================================================
// - Placa:           ESP32S3 Dev Module
// - Flash Mode:      DIO            (IMPORTANTE en este hardware para que el I2S funcione bien)
// - PSRAM:           OPI PSRAM      (OBLIGATORIA: el colchón de grabación de 8 s vive ahí)
// - USB CDC On Boot: Enabled
// - Monitor:         115200 baud    (imprime la tarjeta, el nombre de cada toma y los errores)
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - driver/i2s_std.h, SPI.h, SD.h   (core ESP32 Arduino)
// - FastLED                         (indicadores WS2812)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Una grabadora de campo con el sonido en las manos: el micrófono pasa por ganancia, filtro,
// ecualizador y un efecto, todo manejado con las perillas en tiempo real, y lo que suena en los
// audífonos es exactamente lo que se graba. Pensado para filmar la placa en terreno (un cerro,
// un río, un bosque) y que en el video se vea mover una perilla y cambiar el ambiente.
//
// Cada toma graba DOS archivos que empiezan en la misma muestra:
//   CAMPO_0001.WAV        el sonido PROCESADO, estéreo, 24 bit / 44.1 kHz (lo que oyes)
//   CAMPO_0001_CRUDO.WAV  el micrófono, mono, 24 bit / 44.1 kHz, sin ganancia ni efectos:
//                         sólo pasa por el techo de agudos de 15.5 kHz (FILTRAR_CRUDO). Es la
//                         calidad real del micrófono, y te deja reprocesar la toma después.
// En el editor se ponen uno encima del otro y calzan muestra a muestra. Para sincronizar con
// el video del teléfono: un aplauso frente al micrófono al empezar.
//
// Cadena del procesado:
//   mic → pasa-altos 30 Hz (retumbo de viento) → GANANCIA → [CONGELAR] → FILTRO → EQ →
//   EFECTO → bloqueador DC → TECHO 15.5 kHz → limitador con lookahead (−1 dBFS) → WAV + DAC
// El techo es un Butterworth de 8º orden: plano hasta 14 kHz y −37 dB en 18 kHz. Sobre 16 kHz
// el INMP441 no capta nada útil y sí entrega una joroba de ruido propio, que se oía como un
// soplido fino.
//
// La grabación va a la tarjeta desde otra tarea (core 0), con un colchón de 8 s en PSRAM: una
// tarjeta se toma a veces 100–300 ms en escribir y eso no puede cortar el audio (core 1).
// Las cabeceras de los WAV se actualizan cada 5 s: si se acaba la batería a mitad de una toma,
// el archivo se abre igual (se pierden a lo más los últimos segundos).
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// - BTN1 (GPIO44) -> GRABAR / PARAR. LED 0 rojo parpadeando = grabando.
// - BTN2 (GPIO42) -> EFECTO siguiente: NATURAL → ECO → CATEDRAL → CHORUS → FLANGER → PHASER → NATURAL
//                    (el sonido directo nunca se corta al cambiar: sólo entra y sale el efecto)
// - BTN3 (GPIO0)  -> CONGELAR on/off: toma los últimos 3 s del ambiente y los sostiene como una
//                    nube continua (granos al azar de esos 3 s). El filtro, el EQ y el efecto
//                    siguen actuando sobre la nube: congela y después mueve el filtro.
// - BTN4, BTN5    -> libres
//
// - POT1 (ADC1)   -> GANANCIA del micrófono, +6 … +60 dB. Un ambiente callado necesita mucha.
// - POT2 (ADC2)   -> FILTRO. Al centro = abierto. A la izquierda cierra un pasa-bajos
//                    (20 kHz → 120 Hz: el sonido se va lejos, detrás de una pared); a la
//                    derecha sube un pasa-altos (20 Hz → 5 kHz: sólo queda el aire y los pájaros).
// - POT3 (ADC8)   -> COLOR (EQ de inclinación). Al centro = plano. Izquierda = cálido y oscuro
//                    (graves +8 dB, agudos −10 dB); derecha = brillante (agudos +10, graves −8).
// - POT4 (ADC10)  -> CANTIDAD del efecto (en NATURAL no hace nada). ECO: nivel y repeticiones;
//                    CATEDRAL: mezcla y tamaño; CHORUS: mezcla; FLANGER: mezcla y realimentación
//                    (de suave a metálico); PHASER: profundidad de las muescas y realimentación.
//                    La velocidad de cada modulación es fija (constantes CHORUS_HZ, FLANGER_HZ, PHASER_HZ).
//
// - LEDs:
//     LED 0  verde tenue = lista · ROJO parpadeando = grabando · naranjo = se perdió audio
//            (la tarjeta no alcanzó: usa una clase 10 / A1) · amarillo rápido = cerrando el
//            archivo, no apagues · MAGENTA parpadeando = sin tarjeta o error de tarjeta
//     LED 1  color del efecto: blanco NATURAL · celeste ECO · azul CATEDRAL · verde CHORUS ·
//            violeta FLANGER · ámbar PHASER
//     LED 2  cian = congelado
//     LED 3–5 nivel de entrada (después de la ganancia): verde > −40 dBFS · amarillo > −18 ·
//            rojo > −3 (baja el POT1: el limitador está trabajando)
// ==============================================================================================================================================

#ifndef SIMULADOR
#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>
#endif
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// ─── Tipos (arriba del todo para que el IDE de Arduino genere bien los prototipos) ───
struct Biquad { float b0, b1, b2, a1, a2, z1, z2; };

// ==============================================================================================================================================
// CONFIGURACIÓN
// ==============================================================================================================================================
#define MOSTRAR_ESTADO    1          // 1 = imprime tarjeta / tomas / errores por Serial USB (sólo en eventos). 0 = cero Serial
#define BITS_PROCESADO   24          // 24 o 16. Si el LED 0 se pone naranjo con una tarjeta lenta, 16 baja un tercio el caudal
#define GRABAR_CRUDO      1          // 1 = graba también el micrófono crudo (mono, 24 bit)

const float GANANCIA_MIN_DB  = 6.0f;
const float GANANCIA_MAX_DB  = 60.0f;   // un bosque callado llega al micro a −70/−80 dBFS
const float CORTE_VIENTO_HZ  = 30.0f;   // pasa-altos fijo del procesado: retumbo de viento y de manipulación
const float TECHO            = 0.89f;   // −1 dBFS: techo del limitador
const int   ECO_MS           = 420;     // tiempo del eco ping-pong (fijo: moverlo con un pot desafina las repeticiones)
const float CHORUS_HZ        = 0.35f;   // velocidad de las tres voces del chorus (cada una un poco distinta)
const float FLANGER_HZ       = 0.12f;   // un barrido cada ~8 s: lento, como un avión que pasa
const float PHASER_HZ        = 0.20f;   // un barrido cada 5 s
const float TECHO_HZ         = 15500.0f;   // techo de agudos (Butterworth de 8º orden): plano hasta 14 kHz, −37 dB en 18 kHz
#define FILTRAR_CRUDO     1          // 1 = el crudo también pasa por el techo de agudos (sólo eso: ni ganancia ni nada más)
const unsigned long CABECERA_CADA_MS = 5000;   // cada cuánto se reescriben los tamaños de los WAV

// ─── Pines ───────────────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define MIC_WS    11
#define MIC_SCK   12
#define MIC_SD    13

#define SD_SCK    14
#define SD_MOSI   15
#define SD_MISO   16
#define SD_CS     17
const uint32_t SD_FREQ_HZ = 20000000;   // 20 MHz: lo aguanta cualquier módulo con buffer 74LVC125

#define BTN1_PIN  44
#define BTN2_PIN  42
#define BTN3_PIN   0
#define BTN4_PIN  45
#define BTN5_PIN  47
const unsigned long DEBOUNCE_MS = 150;

const uint8_t POT_PIN[4] = { 1, 2, 8, 10 };

#define LED_PIN      46
#define NUM_LEDS      6
#define LED_BRIGHT   90

// ─── Audio ───────────────────────────────────────────────────
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
const float SR     = (float)SAMPLE_RATE;
const float INV_SR = 1.0f / (float)SAMPLE_RATE;
const float PI_F   = 3.14159265f;

// ─── Colchón de grabación (PSRAM) ────────────────────────────
#define RING_SEGUNDOS        8
#define RING_BLOQUES         ((RING_SEGUNDOS * SAMPLE_RATE) / BUFFER_SAMPLES)   // 2756 bloques
#define BYTES_PROC           (BITS_PROCESADO / 8)
#define BYTES_BLOQUE_PROC    (BUFFER_SAMPLES * 2 * BYTES_PROC)                  // estéreo
#define BYTES_BLOQUE_CRUDO   (BUFFER_SAMPLES * 3)                               // mono, 24 bit
#define BLOQUES_POR_ESCRITURA 32                                                // ~24 KB por escritura
const uint32_t LIMITE_DATOS = 3900000000u;   // FAT32 no pasa de 4 GB por archivo: ahí se corta sola (~4 h)

#ifdef SIMULADOR
  #define RESERVAR_PSRAM(n)   malloc(n)
  #define RESERVAR_INTERNA(n) malloc(n)
#else
  #define RESERVAR_PSRAM(n)   heap_caps_malloc((n), MALLOC_CAP_SPIRAM)
  #define RESERVAR_INTERNA(n) heap_caps_malloc((n), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#endif

#if MOSTRAR_ESTADO
  #ifdef SIMULADOR
    #define LOG(...) printf(__VA_ARGS__)
  #else
    #define LOG(...) Serial.printf(__VA_ARGS__)
  #endif
#else
  #define LOG(...) do {} while (0)
#endif

// ==============================================================================================================================================
// EFECTOS
// ==============================================================================================================================================
enum { EF_NATURAL, EF_ECO, EF_CATEDRAL, EF_CHORUS, EF_FLANGER, EF_PHASER, NUM_EFECTOS };
const char* const NOMBRE_EFECTO[NUM_EFECTOS] = { "NATURAL", "ECO", "CATEDRAL", "CHORUS", "FLANGER", "PHASER" };

// Compensación del nivel del efecto (se ajustó en simulación: con POT4 al máximo, el efecto
// suena parejo con el directo y ninguno se come al otro).
const float NIVEL_ECO      = 0.85f;
const float NIVEL_CATEDRAL = 1.00f;
const float NIVEL_CHORUS   = 0.70f;
const float NIVEL_FLANGER  = 0.70f;

// ==============================================================================================================================================
// PEDIDOS ENTRE NÚCLEOS
// ==============================================================================================================================================
// Escrituras de 32 bits alineadas, atómicas en el S3: no hace falta mutex.
// control → audio   : efectoPedido, congelarPedido, potV[]
// control → grabador: pedidoConmutar
// grabador → audio  : audioEmpuja (el audio llena el colchón sólo mientras esto está en true)
// audio → grabador  : bloquesEsc (lo escribe SÓLO el audio) · grabador → audio: bloquesLeidos
volatile int      efectoPedido   = EF_NATURAL;
volatile bool     congelarPedido = false;
volatile float    potV[4]        = { 0.3f, 0.5f, 0.5f, 0.5f };
volatile float    nivelEntrada   = 0.0f;
volatile float    nivelSalida    = 0.0f;

volatile bool     pedidoConmutar = false;
volatile bool     audioEmpuja    = false;
volatile uint32_t bloquesEsc     = 0;
volatile uint32_t bloquesLeidos  = 0;
volatile uint32_t bloquesPerdidos = 0;
volatile uint32_t contadorBloquesAudio = 0;

enum { GR_REPOSO, GR_GRABANDO, GR_PARANDO, GR_CERRANDO };
volatile int  estadoGrab = GR_REPOSO;
volatile bool sdLista    = false;
volatile bool sdError    = false;

uint8_t *ringProc  = nullptr;
uint8_t *ringCrudo = nullptr;

// ==============================================================================================================================================
// UTILIDADES DSP
// ==============================================================================================================================================
#define TABLA_N 1024
float tablaSeno[TABLA_N + 1];

// seno de t vueltas (t en ciclos): tabla + interpolación, sin libm por muestra
inline float senoT(float t) {
  t -= floorf(t);
  float f = t * (float)TABLA_N;
  int i = (int)f;
  if (i >= TABLA_N) { i = 0; f = 0.0f; }
  float fr = f - (float)i;
  return tablaSeno[i] + (tablaSeno[i + 1] - tablaSeno[i]) * fr;
}

inline float softClip(float x) {
  if (x >  3.0f) x =  3.0f;
  if (x < -3.0f) x = -3.0f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

inline float coefLP1(float f) { return 1.0f - expf(-2.0f * PI_F * f * INV_SR); }

inline float bq(Biquad &q, float x) {
  float y = q.b0 * x + q.z1;
  q.z1 = q.b1 * x - q.a1 * y + q.z2;
  q.z2 = q.b2 * x - q.a2 * y;
  return y;
}

// tipo: 0 = pasa-bajos, 1 = pasa-altos. Sólo cambia coeficientes: el estado sigue (sin clic).
void bqDisenar(Biquad &q, int tipo, float f, float Q) {
  if (f > 0.45f * SR) f = 0.45f * SR;
  if (f < 10.0f) f = 10.0f;
  float w = 2.0f * PI_F * f * INV_SR;
  float cw = cosf(w), sw = sinf(w);
  float alfa = sw / (2.0f * Q);
  float a0 = 1.0f + alfa;
  float b0, b1, b2;
  if (tipo == 0) { b0 = (1.0f - cw) * 0.5f; b1 = 1.0f - cw;    b2 = b0; }
  else           { b0 = (1.0f + cw) * 0.5f; b1 = -(1.0f + cw); b2 = b0; }
  q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0;
  q.a1 = (-2.0f * cw) / a0;
  q.a2 = (1.0f - alfa) / a0;
}

// tipo: 4 = low-shelf, 5 = high-shelf; gananciaDb en dB
void bqDisenarShelf(Biquad &q, int tipo, float f, float gananciaDb) {
  float A = powf(10.0f, gananciaDb / 40.0f);
  float w = 2.0f * PI_F * f * INV_SR;
  float cw = cosf(w), sw = sinf(w);
  float alfa = sw / (2.0f * 0.707f);
  float k = 2.0f * sqrtf(A) * alfa;
  float b0, b1, b2, a0, a1, a2;
  if (tipo == 4) {
    b0 = A * ((A + 1) - (A - 1) * cw + k); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - k);
    a0 = (A + 1) + (A - 1) * cw + k;       a1 = -2 * ((A - 1) + (A + 1) * cw);    a2 = (A + 1) + (A - 1) * cw - k;
  } else {
    b0 = A * ((A + 1) + (A - 1) * cw + k); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - k);
    a0 = (A + 1) - (A - 1) * cw + k;       a1 = 2 * ((A - 1) - (A + 1) * cw);      a2 = (A + 1) - (A - 1) * cw - k;
  }
  q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0; q.a1 = a1 / a0; q.a2 = a2 / a0;
}

inline float leerLinea(const float *buf, int mask, int w, float d) {
  float rp = (float)w - d;
  int i0 = (int)floorf(rp);
  float fr = rp - (float)i0;
  float a = buf[i0 & mask], b = buf[(i0 + 1) & mask];
  return a + (b - a) * fr;
}

uint32_t semillaAzar = 20260929u;
inline uint32_t azar() {
  semillaAzar = semillaAzar * 1664525u + 1013904223u;
  return semillaAzar >> 8;
}

// ==============================================================================================================================================
// ESTADO DE AUDIO (sólo lo toca la tarea de audio)
// ==============================================================================================================================================
int   efectoActual = EF_NATURAL;
float gCambio      = 1.0f;                                  // fundido del EFECTO al cambiarlo (el directo sigue)
const float PASO_CAMBIO = 1.0f / (0.020f * SAMPLE_RATE);    // 20 ms
float pS[4]        = { 0.3f, 0.5f, 0.5f, 0.5f };            // pots suavizados
const float K_POT  = 1.0f - expf(-(float)BUFFER_SAMPLES / (0.03f * SAMPLE_RATE));   // 30 ms
const float K_SUAVE = 1.0f - expf(-1.0f / (0.030f * SAMPLE_RATE));                 // 30 ms por muestra

// Entrada y cadena
Biquad hpViento, hpFiltro, lpFiltro, eqGraves, eqAgudos;
float gAct = 2.0f, gObj = 2.0f;             // ganancia (interpolada a lo largo del bloque)
float secoObj = 1.0f, secoS = 1.0f;         // nivel del directo (baja un poco con la reverb a tope)
float wetObj  = 0.0f, wetS  = 0.0f;         // nivel del efecto

// Salida
float dcX1L = 0, dcY1L = 0, dcX1R = 0, dcY1R = 0;
#define LIM_LOOK 64
float limL[LIM_LOOK], limR[LIM_LOOK];
int   limIdx = 0;
float envLim = 0.0f, gLim = 1.0f;
const float LIM_REL_ENV = expf(-1.0f / (0.080f * SAMPLE_RATE));
const float LIM_ATQ     = 1.0f - expf(-1.0f / (0.00025f * SAMPLE_RATE));
const float LIM_SUELTA  = 1.0f - expf(-1.0f / (0.060f * SAMPLE_RATE));

// TECHO DE AGUDOS: sobre ~16 kHz el INMP441 no capta nada útil y sí entrega una joroba de ruido
// propio (medida en las tomas: +4/+7 dB entre 16 y 20 kHz), que se oye como un soplido fino.
// 4 secciones = Butterworth de 8º orden (Q de cada par de polos: 1 / (2·sen((2k−1)·π/16))).
#define TECHO_SECC 4
const float TECHO_Q[TECHO_SECC] = { 0.5098f, 0.6013f, 0.9000f, 2.5629f };
Biquad techoL[TECHO_SECC], techoR[TECHO_SECC], techoC[TECHO_SECC];
inline float techo(Biquad *s, float x) { for (int k = 0; k < TECHO_SECC; k++) x = bq(s[k], x); return x; }

// CONGELAR: anillo de ~3 s del ambiente + 6 granos Hann desfasados que leen posiciones al azar
#define FRZ_N      131072
#define FRZ_MASK   (FRZ_N - 1)
#define GRANOS     6
const int   GRANO_L   = (int)(0.35f * SAMPLE_RATE);
const float INV_GRANO = 1.0f / (float)GRANO_L;
const float NORM_NUBE = 0.667f;                 // 1/√(6·0.375): granos incorrelados suman en potencia
float   *frz = nullptr;
uint32_t frzW = 0;
bool     congelado = false;
float    gF = 0.0f;                             // 0 = ambiente vivo · 1 = nube
const float PASO_F = 1.0f / (0.250f * SAMPLE_RATE);   // fundido de 250 ms
uint32_t grIni[GRANOS];
int      grT[GRANOS];

// ECO ping-pong (PSRAM)
#define ECO_N     32768
#define ECO_MASK  (ECO_N - 1)
const int ECO_D = (ECO_MS * SAMPLE_RATE) / 1000;
float *ecoL = nullptr, *ecoR = nullptr;
int   ecoW = 0, ecoValido = 0;
float ecoFb = 0.0f, ecoLp = 0.0f;
const float ECO_LP = 1.0f - expf(-2.0f * 3.14159265f * 3500.0f / SAMPLE_RATE);   // repeticiones oscuras

// CATEDRAL (Schroeder/Freeverb: 4 peines + 2 pasa-todo por canal, RAM interna)
#define REV_PEINES 4
#define REV_AP     2
const int REV_LARGO_PEINE[REV_PEINES] = { 1674, 1782, 1916, 2034 };
const int REV_LARGO_AP[REV_AP]        = { 556, 441 };
const int REV_ESTEREO = 35;
float *revMem = nullptr;
int    revTotal = 0;
float *revPeine[2][REV_PEINES], *revAp[2][REV_AP];
int    revLen[2][REV_PEINES], revApLen[2][REV_AP];
int    revIdx[2][REV_PEINES], revApIdx[2][REV_AP];
float  revGuard[2][REV_PEINES];
float  revFb = 0.85f, revDamp = 0.35f;

// CHORUS: tres copias con retardo que se mece (13/17/23 ms ± 2.2 ms): izquierda, derecha y centro
#define CH_N    2048
#define CH_MASK (CH_N - 1)
const float CH_BASE[3] = { 13.0f, 17.0f, 23.0f };            // ms
const float CH_VEL[3]  = { 1.00f, 1.31f, 0.77f };            // × CHORUS_HZ: nunca se alinean
const float CH_PROF_MS = 2.2f;
float *chBuf = nullptr;
int    chW = 0;
float  chFase[3] = { 0.0f, 0.33f, 0.67f };

// FLANGER: peine con realimentación, izquierda y derecha en cuadratura (el barrido gira en estéreo)
#define FL_N    1024
#define FL_MASK (FL_N - 1)
float *flL = nullptr, *flR = nullptr;
int    flW = 0;
float  flFase = 0.0f, flFb = 0.0f;
const float FL_MIN = 0.0008f * SAMPLE_RATE;                  // 0.8 ms …
const float FL_MAX = 0.0070f * SAMPLE_RATE;                  // … 7 ms

// PHASER: 6 pasa-todo de 1er orden por canal barridos entre 200 Hz y 3 kHz. El coeficiente se
// calcula al borde del bloque y se interpola por muestra (tanf por muestra sería caro en el S3).
#define PH_ETAPAS 6
float phZ[2][PH_ETAPAS];
float phA[2] = { 0.0f, 0.0f }, phDA[2] = { 0.0f, 0.0f };
float phFase = 0.0f, phFb = 0.0f, phUlt[2] = { 0.0f, 0.0f };

// ==============================================================================================================================================
// INICIALIZACIÓN
// ==============================================================================================================================================
bool reservarMemoria() {
  ringProc = (uint8_t *)RESERVAR_PSRAM((size_t)RING_BLOQUES * BYTES_BLOQUE_PROC);
#if GRABAR_CRUDO
  ringCrudo = (uint8_t *)RESERVAR_PSRAM((size_t)RING_BLOQUES * BYTES_BLOQUE_CRUDO);
#endif
  frz  = (float *)RESERVAR_PSRAM(FRZ_N * sizeof(float));
  ecoL = (float *)RESERVAR_PSRAM(ECO_N * sizeof(float));
  ecoR = (float *)RESERVAR_PSRAM(ECO_N * sizeof(float));

  revTotal = 0;
  for (int c = 0; c < 2; c++) {
    for (int k = 0; k < REV_PEINES; k++) revTotal += REV_LARGO_PEINE[k] + c * REV_ESTEREO;
    for (int k = 0; k < REV_AP; k++)     revTotal += REV_LARGO_AP[k] + c * REV_ESTEREO;
  }
  revMem = (float *)RESERVAR_INTERNA(revTotal * sizeof(float));
  chBuf  = (float *)RESERVAR_INTERNA(CH_N * sizeof(float));
  flL    = (float *)RESERVAR_INTERNA(FL_N * sizeof(float));
  flR    = (float *)RESERVAR_INTERNA(FL_N * sizeof(float));

  if (!ringProc || (GRABAR_CRUDO && !ringCrudo) || !frz || !ecoL || !ecoR || !revMem || !chBuf || !flL || !flR) return false;
  memset(frz, 0, FRZ_N * sizeof(float));
  memset(ecoL, 0, ECO_N * sizeof(float));
  memset(ecoR, 0, ECO_N * sizeof(float));
  return true;
}

void prepararDSP() {
  for (int i = 0; i <= TABLA_N; i++) tablaSeno[i] = sinf(2.0f * PI_F * (float)i / (float)TABLA_N);

  Biquad *todos[] = { &hpViento, &hpFiltro, &lpFiltro, &eqGraves, &eqAgudos };
  for (Biquad *b : todos) memset(b, 0, sizeof(Biquad));
  bqDisenar(hpViento, 1, CORTE_VIENTO_HZ, 0.707f);
  bqDisenar(hpFiltro, 1, 20.0f, 0.707f);
  bqDisenar(lpFiltro, 0, 20000.0f, 0.707f);
  bqDisenarShelf(eqGraves, 4, 250.0f, 0.0f);
  bqDisenarShelf(eqAgudos, 5, 4000.0f, 0.0f);
  for (int k = 0; k < TECHO_SECC; k++) {
    Biquad *t[3] = { &techoL[k], &techoR[k], &techoC[k] };
    for (Biquad *b : t) { memset(b, 0, sizeof(Biquad)); bqDisenar(*b, 0, TECHO_HZ, TECHO_Q[k]); }
  }

  // reverb: repartir el bloque
  float *p = revMem;
  for (int c = 0; c < 2; c++) {
    int extra = c * REV_ESTEREO;
    for (int k = 0; k < REV_PEINES; k++) { revPeine[c][k] = p; revLen[c][k] = REV_LARGO_PEINE[k] + extra; p += revLen[c][k]; }
    for (int k = 0; k < REV_AP; k++)     { revAp[c][k] = p;    revApLen[c][k] = REV_LARGO_AP[k] + extra;  p += revApLen[c][k]; }
  }
}

// Limpia el estado del efecto que va a empezar (se llama con su fundido en 0).
void resetEfecto(int e) {
  switch (e) {
    case EF_ECO:
      // Sin memset de 256 KB de PSRAM en la tarea de audio: las lecturas dan cero hasta que la
      // línea tenga ECO_D muestras nuevas.
      ecoValido = 0; ecoLp = 0.0f;
      break;
    case EF_CATEDRAL:
      memset(revMem, 0, revTotal * sizeof(float));
      for (int c = 0; c < 2; c++) {
        for (int k = 0; k < REV_PEINES; k++) { revIdx[c][k] = 0; revGuard[c][k] = 0.0f; }
        for (int k = 0; k < REV_AP; k++) revApIdx[c][k] = 0;
      }
      break;
    case EF_CHORUS:
      memset(chBuf, 0, CH_N * sizeof(float));
      chW = 0;
      break;
    case EF_FLANGER:
      memset(flL, 0, FL_N * sizeof(float));
      memset(flR, 0, FL_N * sizeof(float));
      flW = 0;
      break;
    case EF_PHASER:
      memset(phZ, 0, sizeof(phZ));
      phUlt[0] = phUlt[1] = 0.0f;
      break;
    default: break;
  }
}

// ==============================================================================================================================================
// PARÁMETROS POR BLOQUE (a partir de los pots suavizados)
// ==============================================================================================================================================
void parametrosBloque() {
  // GANANCIA: lineal en dB a lo largo del recorrido
  float gDb = GANANCIA_MIN_DB + (GANANCIA_MAX_DB - GANANCIA_MIN_DB) * pS[0];
  gObj = powf(10.0f, gDb / 20.0f);

  // FILTRO: centro abierto; izquierda pasa-bajos, derecha pasa-altos. La resonancia crece
  // con el recorrido pero moderada (nunca un pico que pite).
  float p = pS[1];
  float fLP = 20000.0f, qLP = 0.707f, fHP = 20.0f, qHP = 0.707f;
  if (p < 0.46f) {
    float u = (0.46f - p) / 0.46f;
    fLP = 18000.0f * powf(120.0f / 18000.0f, u);
    qLP = 0.707f + 0.6f * u;
  } else if (p > 0.54f) {
    float u = (p - 0.54f) / 0.46f;
    fHP = 20.0f * powf(5000.0f / 20.0f, u);
    qHP = 0.707f + 0.5f * u;
  }
  bqDisenar(lpFiltro, 0, fLP, qLP);
  bqDisenar(hpFiltro, 1, fHP, qHP);

  // COLOR: inclinación con zona muerta al centro
  float c = (pS[2] - 0.5f) * 2.0f;
  if (fabsf(c) < 0.06f) c = 0.0f;
  else c = (c > 0.0f ? c - 0.06f : c + 0.06f) / 0.94f;
  bqDisenarShelf(eqGraves, 4, 250.0f, -8.0f * c);
  bqDisenarShelf(eqAgudos, 5, 4000.0f, 10.0f * c);

  // EFECTO: POT4 = cantidad
  float q = pS[3];
  switch (efectoActual) {
    case EF_NATURAL:  wetObj = 0.0f; secoObj = 1.0f; break;
    case EF_ECO:      ecoFb = 0.25f + 0.50f * q; wetObj = q * NIVEL_ECO; secoObj = 1.0f; break;
    case EF_CATEDRAL: revFb = 0.82f + 0.15f * q; revDamp = 0.35f; wetObj = q * NIVEL_CATEDRAL; secoObj = 1.0f - 0.45f * q; break;
    case EF_CHORUS:   wetObj = q * NIVEL_CHORUS;  secoObj = 1.0f - 0.30f * q; break;
    case EF_FLANGER:  flFb = 0.70f * q; wetObj = q * NIVEL_FLANGER; secoObj = 1.0f - 0.35f * q; break;
    case EF_PHASER:
      // 0.5·(directo + pasa-todo) = muescas completas con POT4 al máximo, y los picos quedan en 0 dB
      wetObj = 0.5f * q; secoObj = 1.0f - 0.5f * q; phFb = 0.45f * q;
      break;
  }

  // El barrido del phaser corre siempre (así no salta al elegirlo): coeficiente al final del
  // bloque, y por muestra se interpola desde el actual
  phFase += PHASER_HZ * (float)BUFFER_SAMPLES * INV_SR;
  if (phFase >= 1.0f) phFase -= 1.0f;
  for (int c = 0; c < 2; c++) {
    float lfo = 0.5f + 0.5f * senoT(phFase + 0.25f * (float)c);        // derecha en cuadratura
    float f = 200.0f * powf(15.0f, lfo);                               // 200 Hz – 3 kHz, exponencial
    float t = tanf(PI_F * f * INV_SR);
    float aFin = (t - 1.0f) / (t + 1.0f);
    phDA[c] = (aFin - phA[c]) / (float)BUFFER_SAMPLES;
  }
}

// ==============================================================================================================================================
// CONGELAR — nube de granos sobre los últimos ~3 s
// ==============================================================================================================================================
// El anillo va de frzW (lo más viejo) a frzW-1 (lo más nuevo). Un grano no puede cruzar
// ese borde (ahí se juntan el final y el principio: un salto).
inline uint32_t posGrano() { return (frzW + azar() % (uint32_t)(FRZ_N - GRANO_L)) & FRZ_MASK; }

void iniciarGranos() {
  for (int k = 0; k < GRANOS; k++) {
    grT[k]   = (k * GRANO_L) / GRANOS;     // desfasados: la suma de las ventanas es constante
    grIni[k] = posGrano();
  }
}

inline float leerNube() {
  float acc = 0.0f;
  for (int k = 0; k < GRANOS; k++) {
    float s = senoT(0.5f * (float)grT[k] * INV_GRANO);       // Hann = sen²(π·t/L)
    acc += frz[(grIni[k] + (uint32_t)grT[k]) & FRZ_MASK] * s * s;
    if (++grT[k] >= GRANO_L) { grT[k] = 0; grIni[k] = posGrano(); }
  }
  return acc * NORM_NUBE;
}

// ==============================================================================================================================================
// EFECTOS — una muestra (mono) → parte húmeda estéreo (el directo se suma aparte)
// ==============================================================================================================================================
// Reverb de peines (usa revFb / revDamp). `in` ya viene normalizada por la realimentación.
inline void procReverb(float in, float &oL, float &oR) {
  float sal[2];
  for (int c = 0; c < 2; c++) {
    float acc = 0.0f;
    for (int k = 0; k < REV_PEINES; k++) {
      float *buf = revPeine[c][k];
      int   &i   = revIdx[c][k];
      float o = buf[i];
      float &g = revGuard[c][k];
      g = o * (1.0f - revDamp) + g * revDamp;
      buf[i] = in + g * revFb + 1.0e-18f;
      if (++i >= revLen[c][k]) i = 0;
      acc += o;
    }
    for (int k = 0; k < REV_AP; k++) {
      float *buf = revAp[c][k];
      int   &i   = revApIdx[c][k];
      float bo = buf[i];
      buf[i] = acc + bo * 0.5f;
      acc = bo - acc;
      if (++i >= revApLen[c][k]) i = 0;
    }
    sal[c] = acc;
  }
  oL = sal[0]; oR = sal[1];
}

inline void procesarEfecto(float x, float &L, float &R) {
  switch (efectoActual) {
    case EF_ECO: {
      float yL = 0.0f, yR = 0.0f;
      if (ecoValido >= ECO_D) {
        yL = ecoL[(ecoW - ECO_D) & ECO_MASK];
        yR = ecoR[(ecoW - ECO_D) & ECO_MASK];
      } else ecoValido++;
      ecoLp += (yR - ecoLp) * ECO_LP;
      ecoL[ecoW] = x + softClip(ecoLp * ecoFb) + 1.0e-18f;   // saturación suave DENTRO del lazo
      ecoR[ecoW] = yL;                                       // ping-pong: izquierda → derecha → izquierda
      ecoW = (ecoW + 1) & ECO_MASK;
      L = yL; R = yR;
      break;
    }
    case EF_CATEDRAL: {
      procReverb(x * (1.0f - revFb) * 2.0f, L, R);
      break;
    }
    case EF_CHORUS: {
      chBuf[chW] = x;
      float v[3];
      for (int k = 0; k < 3; k++) {
        chFase[k] += CHORUS_HZ * CH_VEL[k] * INV_SR;
        if (chFase[k] >= 1.0f) chFase[k] -= 1.0f;
        float d = (CH_BASE[k] + CH_PROF_MS * senoT(chFase[k])) * 0.001f * SR;
        v[k] = leerLinea(chBuf, CH_MASK, chW, d);
      }
      chW = (chW + 1) & CH_MASK;
      L = v[0] + 0.6f * v[2];
      R = v[1] + 0.6f * v[2];
      break;
    }
    case EF_FLANGER: {
      flFase += FLANGER_HZ * INV_SR;
      if (flFase >= 1.0f) flFase -= 1.0f;
      float dL = FL_MIN + (FL_MAX - FL_MIN) * (0.5f + 0.5f * senoT(flFase));
      float dR = FL_MIN + (FL_MAX - FL_MIN) * (0.5f + 0.5f * senoT(flFase + 0.25f));
      float yL = leerLinea(flL, FL_MASK, flW, dL);
      float yR = leerLinea(flR, FL_MASK, flW, dR);
      flL[flW] = x + softClip(yL * flFb) + 1.0e-18f;     // saturación suave dentro del lazo: nunca se desboca
      flR[flW] = x + softClip(yR * flFb) + 1.0e-18f;
      flW = (flW + 1) & FL_MASK;
      float comp = 1.0f - 0.45f * flFb;                  // la realimentación sube los picos del peine
      L = yL * comp; R = yR * comp;
      break;
    }
    case EF_PHASER: {
      float sal[2];
      for (int c = 0; c < 2; c++) {
        float a = phA[c];
        float y = x + phUlt[c] * phFb;
        for (int k = 0; k < PH_ETAPAS; k++) {            // pasa-todo de 1er orden: o = a·y + z ; z = y − a·o
          float o = a * y + phZ[c][k];
          phZ[c][k] = y - a * o;
          y = o;
        }
        phUlt[c] = y;
        sal[c] = y;
      }
      L = sal[0]; R = sal[1];
      break;
    }
    default:
      L = R = 0.0f;
  }
}

// ==============================================================================================================================================
// BLOQUE DE AUDIO: 128 muestras del micro (estéreo 32 bit, canal izquierdo) → DAC + colchón de grabación
// ==============================================================================================================================================
inline void escribir24(uint8_t *d, int32_t v) {
  d[0] = (uint8_t)(v);
  d[1] = (uint8_t)(v >> 8);
  d[2] = (uint8_t)(v >> 16);
}

void procesarBloque(const int32_t *mic, int16_t *salida) {
  // ── Borde del buffer: pedidos de la tarea de control ──
  int pedido = efectoPedido;
  if (pedido != efectoActual && gCambio <= 0.0f) {   // el efecto ya se fue: se cambia y se limpia
    efectoActual = pedido;
    resetEfecto(efectoActual);
  }
  float objetivoCambio = (pedido == efectoActual) ? 1.0f : 0.0f;

  for (int i = 0; i < 4; i++) pS[i] += (potV[i] - pS[i]) * K_POT;
  float gIni = gAct;
  parametrosBloque();
  float dG = (gObj - gIni) / (float)BUFFER_SAMPLES;

  bool pedCong = congelarPedido;
  if (pedCong && !congelado) { congelado = true; iniciarGranos(); }
  float objF = pedCong ? 1.0f : 0.0f;

  // ── ¿Hay dónde grabar este bloque? (un bloque nunca cruza el final del anillo) ──
  uint8_t *dstP = nullptr, *dstC = nullptr;
  if (audioEmpuja) {
    uint32_t esc = bloquesEsc;
    if (esc - bloquesLeidos >= (uint32_t)RING_BLOQUES) {
      bloquesPerdidos = bloquesPerdidos + 1;             // la tarjeta no alcanzó: se pierde este bloque
    } else {
      uint32_t pos = esc % (uint32_t)RING_BLOQUES;
      dstP = ringProc + (size_t)pos * BYTES_BLOQUE_PROC;
#if GRABAR_CRUDO
      dstC = ringCrudo + (size_t)pos * BYTES_BLOQUE_CRUDO;
#endif
    }
  }

  float picoIn = 0.0f, picoOut = 0.0f;
  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    int32_t crudo = mic[n * 2];
    float x = (float)crudo * (1.0f / 2147483648.0f);
    gAct += dG;
    x = bq(hpViento, x) * gAct;
    float ax = fabsf(x);
    if (ax > picoIn) picoIn = ax;

    // ── Congelar ──
    if (!congelado) { frz[frzW] = x; frzW = (frzW + 1) & FRZ_MASK; }
    if (congelado) {
      float nube = leerNube();
      if (gF < objF)      { gF += PASO_F; if (gF > 1.0f) gF = 1.0f; }
      else if (gF > objF) { gF -= PASO_F; if (gF < 0.0f) gF = 0.0f; }
      // fundido de igual potencia (ambiente y nube son incorrelados)
      x = x * senoT(0.25f - 0.25f * gF) + nube * senoT(0.25f * gF);
      if (!pedCong && gF <= 0.0f) congelado = false;       // vuelve a escuchar (y a llenar el anillo)
    }

    // ── Filtro y color ──
    x = bq(hpFiltro, x);
    x = bq(lpFiltro, x);
    x = bq(eqGraves, x);
    x = bq(eqAgudos, x);

    // ── Efecto (sólo la parte húmeda: el directo nunca se corta) ──
    float wL = 0.0f, wR = 0.0f;
    phA[0] += phDA[0]; phA[1] += phDA[1];
    if (efectoActual != EF_NATURAL) procesarEfecto(x, wL, wR);
    if (gCambio < objetivoCambio)      { gCambio += PASO_CAMBIO; if (gCambio > 1.0f) gCambio = 1.0f; }
    else if (gCambio > objetivoCambio) { gCambio -= PASO_CAMBIO; if (gCambio < 0.0f) gCambio = 0.0f; }
    wetS  += (wetObj  - wetS)  * K_SUAVE;
    secoS += (secoObj - secoS) * K_SUAVE;
    float gw = wetS * gCambio;
    float L = x * secoS + wL * gw;
    float R = x * secoS + wR * gw;

    // ── Bloqueador de continua ──
    float dl = L - dcX1L + 0.9995f * dcY1L; dcX1L = L; dcY1L = dl;
    float dr = R - dcX1R + 0.9995f * dcY1R; dcX1R = R; dcY1R = dr;

    // ── Techo de agudos ANTES del limitador: un Butterworth de 8º orden rebota un poco en los
    //    picos (medido: con el techo después, el limitador ya no los atajaba y llegaban a tope) ──
    dl = techo(techoL, dl);
    dr = techo(techoR, dr);

    // ── Limitador con lookahead de 64 muestras (ganancia suavizada, sin escalones) ──
    float pk = fabsf(dl) > fabsf(dr) ? fabsf(dl) : fabsf(dr);
    envLim = pk > envLim ? pk : envLim * LIM_REL_ENV;
    float gObjLim = envLim > TECHO ? TECHO / envLim : 1.0f;
    gLim += (gObjLim - gLim) * (gObjLim < gLim ? LIM_ATQ : LIM_SUELTA);
    float oL = limL[limIdx] * gLim, oR = limR[limIdx] * gLim;
    limL[limIdx] = dl; limR[limIdx] = dr;
    limIdx = (limIdx + 1) & (LIM_LOOK - 1);
    if (oL >  1.0f) oL =  1.0f; else if (oL < -1.0f) oL = -1.0f;
    if (oR >  1.0f) oR =  1.0f; else if (oR < -1.0f) oR = -1.0f;

    float aL = fabsf(oL), aR = fabsf(oR);
    if (aL > picoOut) picoOut = aL;
    if (aR > picoOut) picoOut = aR;

    // ── Monitor (DAC, 16 bit) ──
    salida[n * 2]     = (int16_t)lrintf(oL * 32767.0f);
    salida[n * 2 + 1] = (int16_t)lrintf(oR * 32767.0f);

    // ── Grabación ──
    if (dstP) {
#if BITS_PROCESADO == 24
      escribir24(dstP + n * 6,     (int32_t)lrintf(oL * 8388607.0f));
      escribir24(dstP + n * 6 + 3, (int32_t)lrintf(oR * 8388607.0f));
#else
      int16_t *d16 = (int16_t *)(dstP + n * 4);
      d16[0] = salida[n * 2];
      d16[1] = salida[n * 2 + 1];
#endif
    }
    if (dstC) {
#if FILTRAR_CRUDO
      float c = techo(techoC, (float)(crudo >> 8));      // en unidades de 24 bit: sin pérdida de resolución
      if (c > 8388607.0f) c = 8388607.0f; else if (c < -8388608.0f) c = -8388608.0f;
      escribir24(dstC + n * 3, (int32_t)lrintf(c));
#else
      escribir24(dstC + n * 3, crudo >> 8);              // el INMP441 entrega 24 bits arriba del slot de 32
#endif
    }
  }

  if (dstP) {
    __sync_synchronize();                                // los datos antes que el contador
    bloquesEsc = bloquesEsc + 1;
  }
  nivelEntrada = picoIn;
  nivelSalida  = picoOut;
  contadorBloquesAudio = contadorBloquesAudio + 1;
}

// ==============================================================================================================================================
// GRABADORA (tarea del core 0): vacía el colchón a la tarjeta
// ==============================================================================================================================================
// Lo de la tarjeta en sí va más abajo (SD en la placa, archivos del PC en el simulador).
bool   archivoAbrir(int k, const char *nombre);
size_t archivoEscribir(int k, const uint8_t *p, size_t n);
bool   archivoSeek(int k, uint32_t pos);
void   archivoFlush(int k);
void   archivoCerrar(int k);
unsigned long relojMs();

const int NUM_ARCHIVOS = GRABAR_CRUDO ? 2 : 1;
const int CANALES[2]   = { 2, 1 };
const int BITS[2]      = { BITS_PROCESADO, 24 };

int      siguienteToma = 1;
uint32_t bytesDatos[2] = { 0, 0 };
unsigned long ultimaCabecera = 0;
uint32_t marcaParada = 0;
uint32_t perdidosAlAbrir = 0;

void cabeceraWav(uint8_t *h, int canales, int bits, uint32_t datos) {
  uint32_t blockAlign = (uint32_t)canales * (uint32_t)(bits / 8);
  uint32_t byteRate   = (uint32_t)SAMPLE_RATE * blockAlign;
  uint32_t v;
  memcpy(h, "RIFF", 4);  v = 36 + datos;  memcpy(h + 4, &v, 4);
  memcpy(h + 8, "WAVEfmt ", 8);
  v = 16;                memcpy(h + 16, &v, 4);
  uint16_t s = 1;        memcpy(h + 20, &s, 2);        // PCM
  s = (uint16_t)canales; memcpy(h + 22, &s, 2);
  v = SAMPLE_RATE;       memcpy(h + 24, &v, 4);
  memcpy(h + 28, &byteRate, 4);
  s = (uint16_t)blockAlign; memcpy(h + 32, &s, 2);
  s = (uint16_t)bits;    memcpy(h + 34, &s, 2);
  memcpy(h + 36, "data", 4);
  memcpy(h + 40, &datos, 4);
}

// Reescribe los tamaños y vuelve al final. Con esto una toma cortada por la batería se abre igual.
bool actualizarCabeceras() {
  uint8_t h[44];
  bool ok = true;
  for (int k = 0; k < NUM_ARCHIVOS; k++) {
    cabeceraWav(h, CANALES[k], BITS[k], bytesDatos[k]);
    ok &= archivoSeek(k, 0);
    ok &= archivoEscribir(k, h, 44) == 44;
    ok &= archivoSeek(k, 44 + bytesDatos[k]);
    archivoFlush(k);
  }
  return ok;
}

bool abrirToma() {
  char nombre[2][40];
  snprintf(nombre[0], sizeof(nombre[0]), "/CAMPO_%04d.WAV", siguienteToma);
  snprintf(nombre[1], sizeof(nombre[1]), "/CAMPO_%04d_CRUDO.WAV", siguienteToma);
  uint8_t h[44];
  for (int k = 0; k < NUM_ARCHIVOS; k++) {
    bytesDatos[k] = 0;
    if (!archivoAbrir(k, nombre[k])) {
      LOG("ERROR: no se pudo crear %s\n", nombre[k]);
      for (int j = 0; j < k; j++) archivoCerrar(j);
      return false;
    }
    cabeceraWav(h, CANALES[k], BITS[k], 0);
    if (archivoEscribir(k, h, 44) != 44) {
      LOG("ERROR: no se pudo escribir en %s\n", nombre[k]);
      for (int j = 0; j <= k; j++) archivoCerrar(j);
      return false;
    }
  }
  LOG("GRABANDO toma %04d  (%s%s)\n", siguienteToma, nombre[0], GRABAR_CRUDO ? " + _CRUDO" : "");
  siguienteToma++;
  return true;
}

void cerrarToma() {
  bool ok = actualizarCabeceras();
  for (int k = 0; k < NUM_ARCHIVOS; k++) archivoCerrar(k);
  uint32_t perdidos = bloquesPerdidos - perdidosAlAbrir;
  float seg = (float)bytesDatos[0] / (float)(SAMPLE_RATE * CANALES[0] * (BITS[0] / 8));
  LOG("Toma cerrada: %.1f s%s", seg, ok ? "" : "  (ERROR al escribir la cabecera)");
  if (perdidos) LOG("  · OJO: se perdieron %u bloques (%.2f s): la tarjeta no alcanzó", perdidos, perdidos * (float)BUFFER_SAMPLES / SR);
  LOG("\n");
}

// Vacía hasta BLOQUES_POR_ESCRITURA bloques. Devuelve cuántos escribió (−1 = error de tarjeta).
int vaciarColchon() {
  uint32_t disp = bloquesEsc - bloquesLeidos;
  if (disp == 0) return 0;
  uint32_t pos = bloquesLeidos % (uint32_t)RING_BLOQUES;
  uint32_t n = disp;
  if (n > BLOQUES_POR_ESCRITURA) n = BLOQUES_POR_ESCRITURA;
  if (n > (uint32_t)RING_BLOQUES - pos) n = (uint32_t)RING_BLOQUES - pos;   // no cruzar el final del anillo
  __sync_synchronize();
  size_t b0 = (size_t)n * BYTES_BLOQUE_PROC;
  if (archivoEscribir(0, ringProc + (size_t)pos * BYTES_BLOQUE_PROC, b0) != b0) return -1;
  bytesDatos[0] += b0;
#if GRABAR_CRUDO
  size_t b1 = (size_t)n * BYTES_BLOQUE_CRUDO;
  if (archivoEscribir(1, ringCrudo + (size_t)pos * BYTES_BLOQUE_CRUDO, b1) != b1) return -1;
  bytesDatos[1] += b1;
#endif
  bloquesLeidos = bloquesLeidos + n;
  return (int)n;
}

// Un paso de la grabadora. Devuelve true si hizo trabajo (para no dormir mientras haya cola).
bool pasoGrabadora() {
  if (pedidoConmutar) {
    pedidoConmutar = false;
    if (estadoGrab == GR_REPOSO) {
      if (sdLista && abrirToma()) {
        bloquesLeidos   = bloquesEsc;          // el audio no está empujando: se parte de cero
        perdidosAlAbrir = bloquesPerdidos;
        ultimaCabecera  = relojMs();
        sdError = false;
        estadoGrab  = GR_GRABANDO;
        audioEmpuja = true;
      } else {
        sdError = true;
      }
    } else if (estadoGrab == GR_GRABANDO) {
      audioEmpuja = false;
      marcaParada = contadorBloquesAudio;
      estadoGrab  = GR_PARANDO;
    }
  }

  if (estadoGrab == GR_REPOSO) return false;

  int n = vaciarColchon();
  if (n < 0) {                                 // tarjeta llena o sacada: se cierra lo que haya
    LOG("ERROR: la tarjeta dejó de aceptar datos (¿llena o se soltó?)\n");
    audioEmpuja = false;
    cerrarToma();
    sdError = true;
    estadoGrab = GR_REPOSO;
    return false;
  }

  if (estadoGrab == GR_GRABANDO) {
    if (bytesDatos[0] > LIMITE_DATOS || (GRABAR_CRUDO && bytesDatos[1] > LIMITE_DATOS)) {
      LOG("La toma llegó al máximo de FAT32 (4 GB): se cierra sola\n");
      audioEmpuja = false;
      marcaParada = contadorBloquesAudio;
      estadoGrab  = GR_PARANDO;
    } else if (relojMs() - ultimaCabecera >= CABECERA_CADA_MS) {
      ultimaCabecera = relojMs();
      actualizarCabeceras();
    }
  }
  // Dos bloques de audio después de bajar audioEmpuja, ya nadie está escribiendo en el colchón
  if (estadoGrab == GR_PARANDO && contadorBloquesAudio - marcaParada >= 2) estadoGrab = GR_CERRANDO;
  if (estadoGrab == GR_CERRANDO && bloquesEsc == bloquesLeidos) {
    cerrarToma();
    estadoGrab = GR_REPOSO;
  }
  return n > 0;
}

// ==============================================================================================================================================
// HARDWARE (todo lo de abajo no entra en el simulador de PC)
// ==============================================================================================================================================
#ifndef SIMULADOR

i2s_chan_handle_t tx_chan, rx_chan;
CRGB leds[NUM_LEDS];
SPIClass spiSD(FSPI);
File archivos[2];

unsigned long relojMs() { return millis(); }
bool   archivoAbrir(int k, const char *nombre)             { archivos[k] = SD.open(nombre, FILE_WRITE); return (bool)archivos[k]; }
size_t archivoEscribir(int k, const uint8_t *p, size_t n) { return archivos[k].write(p, n); }
bool   archivoSeek(int k, uint32_t pos)                    { return archivos[k].seek(pos); }
void   archivoFlush(int k)                                 { archivos[k].flush(); }
void   archivoCerrar(int k)                                { archivos[k].close(); }

bool iniciarSD() {
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD, SD_FREQ_HZ)) {
    LOG("Sin tarjeta microSD (o el módulo no responde): revisa SCK %d · MOSI %d · MISO %d · CS %d\n", SD_SCK, SD_MOSI, SD_MISO, SD_CS);
    return false;
  }
  if (SD.cardType() == CARD_NONE) { LOG("Sin tarjeta microSD\n"); return false; }
  // La próxima toma continúa la numeración de lo que ya hay en la tarjeta
  File raiz = SD.open("/");
  for (File f = raiz.openNextFile(); f; f = raiz.openNextFile()) {
    int num;
    if (sscanf(f.name(), "CAMPO_%d", &num) == 1 && num >= siguienteToma) siguienteToma = num + 1;
    f.close();
  }
  raiz.close();
  uint64_t libre = SD.totalBytes() - SD.usedBytes();
  float bps = (float)SAMPLE_RATE * (2 * BYTES_PROC + (GRABAR_CRUDO ? 3 : 0));
  LOG("microSD lista: %.1f GB libres (~%.1f h de grabación). Próxima toma: %04d\n",
      libre / 1.0e9, libre / bps / 3600.0f, siguienteToma);
  return true;
}

float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) sum += analogRead(pin);
  return (float)(sum >> 2) / 4095.0f;
}

void i2s_dac_init() {
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

// El micro corre a la MISMA tasa y con la misma fuente de reloj que el DAC: no derivan.
void i2s_mic_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num  = 8;                 // ~23 ms de colchón de entrada: ni una muestra se pierde
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));
  // El INMP441 entrega 24 bits en un slot de 32: estéreo de 32 bits y nos quedamos con el IZQUIERDO
  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
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

// ─── Tarea de audio (core 1): leer el micro marca el ritmo ──
void audioTask(void *) {
  static int32_t micBuf[BUFFER_SAMPLES * 2];
  static int16_t salBuf[BUFFER_SAMPLES * 2];
  size_t n;
  for (int k = 0; k < 16; k++)
    if (i2s_channel_read(rx_chan, micBuf, sizeof(micBuf), &n, 0) != ESP_OK) break;
  memset(salBuf, 0, sizeof(salBuf));
  for (int k = 0; k < 2; k++) i2s_channel_write(tx_chan, salBuf, sizeof(salBuf), &n, portMAX_DELAY);

  for (;;) {
    if (i2s_channel_read(rx_chan, micBuf, sizeof(micBuf), &n, portMAX_DELAY) != ESP_OK) continue;
    if (n < sizeof(micBuf)) memset((uint8_t *)micBuf + n, 0, sizeof(micBuf) - n);
    procesarBloque(micBuf, salBuf);
    i2s_channel_write(tx_chan, salBuf, sizeof(salBuf), &n, portMAX_DELAY);
  }
}

// ─── Tarea grabadora (core 0, debajo de la de control) ──
void grabadoraTask(void *) {
  for (;;) {
    bool trabajo = pasoGrabadora();
    vTaskDelay(trabajo ? 1 : 5);               // siempre cede: el watchdog y los botones no esperan a la tarjeta
  }
}

// ─── Tarea de control (core 0, 1 kHz): botones, pots, LEDs ──
const uint8_t BTN_PIN[5] = { BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN, BTN5_PIN };
bool btnNivel[5] = { HIGH, HIGH, HIGH, HIGH, HIGH };
unsigned long btnTiempo[5] = { 0, 0, 0, 0, 0 };
float flash = 0.0f, vu = 0.0f;
unsigned long rojoHasta = 0, naranjoHasta = 0;
uint32_t perdidosVistos = 0;

const CRGB COLOR_EFECTO[NUM_EFECTOS] = { CRGB(150, 150, 140), CRGB(40, 150, 255), CRGB(30, 50, 255),
                                         CRGB(0, 200, 90), CRGB(160, 40, 255), CRGB(230, 140, 0) };

void renderLEDs() {
  unsigned long t = millis();

  // LED 0: estado de la grabación
  uint32_t perd = bloquesPerdidos;
  if (perd != perdidosVistos) { perdidosVistos = perd; naranjoHasta = t + 1500; }
  int est = estadoGrab;
  if (est == GR_GRABANDO) {
    if (t < naranjoHasta) leds[0] = CRGB(255, 90, 0);
    else leds[0] = (t % 1000) < 700 ? CRGB(255, 0, 0) : CRGB(40, 0, 0);
  } else if (est == GR_PARANDO || est == GR_CERRANDO) {
    leds[0] = (t % 200) < 100 ? CRGB(255, 180, 0) : CRGB::Black;
  } else if (!sdLista || sdError) {
    leds[0] = (t % 600) < 300 ? CRGB(200, 0, 160) : CRGB::Black;
  } else {
    leds[0] = CRGB(0, 40, 0);
  }

  // LED 1: efecto · LED 2: congelado
  leds[1] = COLOR_EFECTO[efectoPedido];
  leds[2] = congelarPedido ? CRGB(0, 200, 200) : CRGB::Black;

  // LEDs 3–5: nivel de entrada
  float pk = nivelEntrada;
  float db = (pk > 1.0e-6f) ? 20.0f * log10f(pk) : -120.0f;
  float nivel = (db + 60.0f) / 60.0f;
  if (nivel < 0.0f) nivel = 0.0f;
  vu = nivel > vu ? nivel : vu * 0.9f;
  if (db > -3.0f) rojoHasta = t + 500;
  float vuDb = vu * 60.0f - 60.0f;
  leds[3] = vuDb > -40.0f ? CRGB(0, 160, 0)   : CRGB::Black;
  leds[4] = vuDb > -18.0f ? CRGB(170, 140, 0) : CRGB::Black;
  leds[5] = (t < rojoHasta) ? CRGB(255, 0, 0) : CRGB::Black;

  if (flash > 0.02f) {
    uint8_t w = (uint8_t)(flash * 120.0f);
    for (int i = 1; i < NUM_LEDS; i++) leds[i] += CRGB(w, w, w);
    flash *= 0.75f;
  }
  FastLED.show();
}

bool flancoPresion(int b, unsigned long tms) {
  bool nivel = digitalRead(BTN_PIN[b]);
  bool disparo = (nivel == LOW && btnNivel[b] == HIGH && (tms - btnTiempo[b]) > DEBOUNCE_MS);
  if (disparo) btnTiempo[b] = tms;
  btnNivel[b] = nivel;
  return disparo;
}

void pasoControl() {
  unsigned long tms = millis();

  if (flancoPresion(0, tms)) pedidoConmutar = true;                       // BTN1 grabar / parar
  if (flancoPresion(1, tms)) {                                            // BTN2 efecto
    int e = (efectoPedido + 1) % NUM_EFECTOS;
    efectoPedido = e;
    flash = 1.0f;
    LOG("Efecto: %s\n", NOMBRE_EFECTO[e]);
  }
  if (flancoPresion(2, tms)) {                                            // BTN3 congelar
    congelarPedido = !congelarPedido;
    LOG("Congelar: %s\n", congelarPedido ? "SÍ" : "no");
  }

  // Pots: uno por pasada, en rotación (cada analogRead cuesta decenas de µs)
  static uint8_t scan = 0;
  potV[scan] = readPot(POT_PIN[scan]);
  scan = (scan + 1) & 3;

  static unsigned long ultLED = 0;
  if (tms - ultLED >= 20) { ultLED = tms; renderLEDs(); }
}

void controlTask(void *) { for (;;) { pasoControl(); vTaskDelay(1); } }

void setup() {
#if MOSTRAR_ESTADO
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // OBLIGATORIO con audio: un print por USB CDC bloquea hasta que el PC lea
#endif
  for (int b = 0; b < 5; b++) pinMode(BTN_PIN[b], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  for (int i = 0; i < 4; i++) potV[i] = pS[i] = readPot(POT_PIN[i]);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHT);
  FastLED.clear();
  FastLED.show();

  if (!reservarMemoria()) {
    // Sin PSRAM (¿"PSRAM: OPI PSRAM" en el IDE?): LEDs rojos parpadeando y nada más
    for (;;) {
      fill_solid(leds, NUM_LEDS, (millis() / 300) % 2 ? CRGB(120, 0, 0) : CRGB::Black);
      FastLED.show();
      delay(50);
    }
  }
  prepararDSP();

#if MOSTRAR_ESTADO
  delay(1500);                 // tiempo para abrir el monitor serie y ver el estado de la tarjeta
  LOG("\nGRABADOR DE CAMPO — BTN1 grabar · BTN2 efecto · BTN3 congelar · usa audífonos\n");
#endif
  sdLista = iniciarSD();

  i2s_dac_init();
  i2s_mic_init();

  xTaskCreatePinnedToCore(audioTask,     "audio",     8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask,   "control",   4096, NULL,  3, NULL, 0);
  xTaskCreatePinnedToCore(grabadoraTask, "grabadora", 6144, NULL,  2, NULL, 0);
}

void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }

#endif  // SIMULADOR
