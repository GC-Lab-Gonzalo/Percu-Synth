// ==============================================================================================================================================
// PERCUSYNTH - VOZ FX (procesador de voz en tiempo real con el micrófono INMP441) - GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo Sandoval - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
// ==============================================================================================================================================
// HARDWARE
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3 (PercuSynth).
//
// - DAC PCM5102 por I2S  ->  I2S_NUM_0 (SALIDA / TX) a 44100 Hz, 16 bit, estéreo:
//       I2S LCK / LRCK ... GPIO 39
//       I2S DIN / DATA ... GPIO 40
//       I2S BCK / BCLK ... GPIO 41
//
// - Micrófono INMP441 por I2S  ->  I2S_NUM_1 (ENTRADA / RX) a 44100 Hz (la MISMA tasa que el DAC):
//       WS  (LRCL) ....... GPIO 11
//       SCK (BCLK) ....... GPIO 12
//       SD  (DOUT) ....... GPIO 13
//       L/R .............. GND       (dato en el slot IZQUIERDO)
//       VDD .............. 3.3V      (NO 5V)
//
// - Botones (INPUT_PULLUP, presionado = LOW): 44, 42, 0, 45, 47  (sólo se usa BTN1)
// - Potenciómetros: ADC 1, 2, 8, 10
// - Indicadores: 6 LEDs SMD WS2812 on-board (data GPIO 46)
//
// ¡AUDÍFONOS! Con parlante, el micrófono escucha su propia salida y el eco, la catedral y el
// flanger se acoplan (el pitido de un micrófono frente al parlante). Con audífonos no pasa.
// ==============================================================================================================================================
// ARDUINO IDE SETTINGS
// ==============================================================================================================================================
// - Placa:           ESP32S3 Dev Module
// - Flash Mode:      DIO            (IMPORTANTE en este hardware para que el I2S funcione bien)
// - PSRAM:           OPI PSRAM      (opcional: los buffers van en RAM interna si caben)
// - USB CDC On Boot: Enabled
// - Monitor:         115200 baud    (imprime el efecto elegido y qué hace cada pot)
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - driver/i2s_std.h   (core ESP32 Arduino, driver I2S nuevo)
// - FastLED            (indicadores WS2812)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Hablas o cantas al micrófono y sale por el DAC transformada, sin grabar nada: la voz pasa
// por el efecto muestra a muestra, con ~12 ms de latencia (un buffer de entrada + la cola del
// DAC). Once efectos, BTN1 pasa al siguiente y los pots lo modifican en tiempo real.
//
//   1 VOZ          voz limpia de alta calidad: graves, agudos y compresión (al centro = plana)
//   2 ELECTRO      cadena de voz para música electrónica (compresor 4:1, presencia, doblador
//                  estéreo, delay ping-pong a 1/8 del tempo, reverb plate) + TRÉMOLO
//                  CUANTIZADO al tempo: 1 · 1/2 · 1/4 · 1/8 · 1/16 · 1/32 de compás
//   3 AUTOTUNE     detecta la nota que cantas (YIN) y la lleva a la nota más cercana de la
//                  escala con PSOLA sincronizado al período (sin fase ni chorus). Lento =
//                  corrección natural; rápido = efecto robótico. Encima, una capa de VOCODER
//                  cuyo sinte toca la nota ya corregida (+ la octava de abajo): la "doble voz"
//                  de Instant Crush. ESCALA FIJA EN SI♭ MENOR por ahora (la de esa canción)
//   4 ROBOT        modulador en anillo: la voz multiplicada por un seno (Dalek a 30 Hz,
//                  campana metálica arriba de 300 Hz)
//   5 VOCODER      la voz SINTETIZADA: 12 bandas analizan tu voz y moldean un acorde de
//                  sierras. Hablas y el sinte habla con tus palabras
//   6 PITCH        voz de ardilla o de monstruo: ±12 semitonos, sin cambiar la velocidad
//   7 ARMONIZADOR  tu voz + dos voces desplazadas que forman un acorde (coro de uno)
//   8 ECO          delay de cinta con realimentación oscura
//   9 CATEDRAL     reverb estéreo grande
//  10 RADIO        banda estrecha + saturación: radio, teléfono, megáfono
//  11 FLANGER      barrido de peine estéreo (avión / chorus)
//
// Cadena fija alrededor del efecto:
//   mic → pasa-altos 90 Hz → ganancia → PUERTA de ruido → EFECTO → volumen → bloqueador DC →
//   limitador con lookahead → pasa-bajos 13 kHz → DAC
// La puerta silencia el soplido del micrófono entre frase y frase; las colas del eco y la
// reverb siguen sonando porque la puerta va ANTES del efecto.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// - BTN1 (GPIO44) -> EFECTO SIGUIENTE (1 → 11 → 1). El cambio es un fundido de 5 ms, sin clic.
// - BTN2 (GPIO42) -> PUERTA DE RUIDO on/off. Encendida (al arrancar) = sólo pasa la voz;
//                    apagada = pasa todo, también el sonido ambiente. LED 0 blanco = apagada.
//
// - POT4 (ADC10)  -> VOLUMEN general. Siempre. En cero = silencio.
// - POT1..POT3    -> los parámetros del efecto activo:
//
//   EFECTO        POT1                      POT2                     POT3
//   VOZ           graves ±6 dB              agudos ±6 dB             compresión (0 = nada)
//   ELECTRO       tempo 70–180 BPM          división del trémolo     profundidad del trémolo
//   AUTOTUNE      mezcla autotune/vocoder   octava abajo (vocoder)   velocidad (natural → robot)
//   ROBOT         frecuencia 30 Hz–1 kHz    mezcla limpio/robot      brillo
//   VOCODER       nota (Do2–Do4)            acorde (6)               brillo (0.8–8 kHz)
//   PITCH         semitonos −12..+12        mezcla limpio/desplazado grano (25–85 ms)
//   ARMONIZADOR   acorde (6)                nivel de las voces       ancho estéreo
//   ECO           tiempo 60–720 ms          realimentación           brillo de las repeticiones
//   CATEDRAL      tamaño                    brillo                   mezcla
//   RADIO         centro de la banda        estrechez                saturación
//   FLANGER       velocidad 0.05–5 Hz       profundidad              realimentación
//
//   Acordes del VOCODER: unísono · octavas · quinta · menor · mayor · cuartas
//   Acordes del ARMONIZADOR: menor (+3 +7) · mayor (+4 +7) · quinta y octava abajo ·
//                            octavas (−12 +12) · cuartas (+5 +10) · coro (±12 cents)
//
// - LEDs: LED 0 = color del efecto · LEDs 1..5 = vúmetro de la salida en ese color.
//   VOZ rojo · ELECTRO naranjo · AUTOTUNE amarillo · ROBOT lima · VOCODER verde ·
//   PITCH verde agua · ARMONIZADOR celeste · ECO azul claro · CATEDRAL azul · RADIO violeta ·
//   FLANGER rosado
//   AUTOTUNE: escala fija Si♭ menor (atTono / atEscala en el código)
//   En ELECTRO el LED 0 late con el trémolo.
// ==============================================================================================================================================

#ifndef SIMULADOR
#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <esp_heap_caps.h>
#endif
#include <math.h>
#include <string.h>
#include <stdint.h>

// ─── Tipos (arriba del todo para que el IDE de Arduino genere bien los prototipos) ───
struct Biquad { float b0, b1, b2, a1, a2, z1, z2; };

// ==============================================================================================================================================
// CONFIGURACIÓN
// ==============================================================================================================================================
#define MOSTRAR_ESTADO    0          // 1 = imprime el efecto al cambiarlo (Serial USB, 115200). 0 = cero Serial

const float GANANCIA_MIC   = 8.0f;   // +18 dB: medido pasando el micro directo al DAC, la voz llega a −25/−40 dBFS
const float UMBRAL_PUERTA  = 0.04f;  // nivel (tras la ganancia) que abre la puerta de ruido
const float CORTE_MIC_HZ   = 90.0f;  // pasa-altos de entrada: golpes al micro, respiración, DC
const float MASTER         = 1.0f;   // ganancia con POT4 al máximo (el limitador cuida el pico)
const float TECHO          = 0.89f;  // −1 dBFS: techo del limitador

// ─── Pines ───────────────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define MIC_WS    11
#define MIC_SCK   12
#define MIC_SD    13

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

// ==============================================================================================================================================
// EFECTOS
// ==============================================================================================================================================
enum { EF_VOZ, EF_ELECTRO, EF_AUTOTUNE, EF_ROBOT, EF_VOCODER, EF_PITCH, EF_ARMONIZADOR, EF_ECO, EF_CATEDRAL, EF_RADIO, EF_FLANGER, NUM_EFECTOS };

const char* const NOMBRE_EFECTO[NUM_EFECTOS] = {
  "VOZ", "ELECTRO", "AUTOTUNE", "ROBOT", "VOCODER", "PITCH", "ARMONIZADOR", "ECO", "CATEDRAL", "RADIO", "FLANGER"
};
const char* const AYUDA_EFECTO[NUM_EFECTOS] = {
  "POT1 graves · POT2 agudos · POT3 compresión (centro = plano)",
  "POT1 tempo 70-180 BPM · POT2 división 1 1/2 1/4 1/8 1/16 1/32 · POT3 profundidad trémolo",
  "POT1 mezcla autotune/vocoder · POT2 octava abajo · POT3 velocidad (natural -> robot) · Sib menor",
  "POT1 frecuencia · POT2 mezcla · POT3 brillo",
  "POT1 nota · POT2 acorde · POT3 brillo",
  "POT1 semitonos · POT2 mezcla · POT3 grano",
  "POT1 acorde · POT2 nivel voces · POT3 ancho",
  "POT1 tiempo · POT2 realimentación · POT3 brillo",
  "POT1 tamaño · POT2 brillo · POT3 mezcla",
  "POT1 banda · POT2 estrechez · POT3 saturación",
  "POT1 velocidad · POT2 profundidad · POT3 realimentación"
};
const uint8_t HUE_EFECTO[NUM_EFECTOS] = { 0, 22, 45, 70, 92, 115, 135, 150, 165, 192, 224 };

// Compensación de nivel por efecto (dB): iguala a todos con VOZ, así cambiar de efecto no
// sube ni baja el volumen. Medido en simulación con la misma voz y los pots al centro
// (ELECTRO con el trémolo en 0: el trémolo es modulación, no nivel).
const float TRIM_EFECTO_DB[NUM_EFECTOS] = { 0.0f, -2.9f, 5.4f, 8.6f, -0.1f, 7.6f, 5.8f, 2.7f, 3.2f, 2.0f, 9.0f };
float trimEfecto[NUM_EFECTOS];

// ELECTRO: divisiones del trémolo (golpes por compás de 4/4)
#define NUM_DIVS 6
const int DIVS_TREMOLO[NUM_DIVS] = { 1, 2, 4, 8, 16, 32 };   // 1 · 1/2 · 1/4 · 1/8 · 1/16 · 1/32

// Vocoder: acordes de la portadora (semitonos sobre la nota del POT1)
#define NUM_ACORDES_VOC 6
const float ACORDE_VOC[NUM_ACORDES_VOC][3] = {
  { 0.0f,  0.12f, -0.12f },   // unísono (tres sierras desafinadas ±12 cents)
  { 0.0f, 12.0f, -12.0f },    // octavas
  { 0.0f,  7.0f,  12.0f },    // quinta
  { 0.0f,  3.0f,   7.0f },    // menor
  { 0.0f,  4.0f,   7.0f },    // mayor
  { 0.0f,  5.0f,  10.0f },    // cuartas
};
const float NOTA_BASE_VOC = 65.406f;   // Do2; el POT1 recorre 25 semitonos hasta Do4

// Armonizador: las dos voces que se suman a la tuya
#define NUM_ACORDES_ARM 6
const float ACORDE_ARM[NUM_ACORDES_ARM][2] = {
  {   3.0f,  7.0f },          // menor
  {   4.0f,  7.0f },          // mayor
  { -12.0f,  7.0f },          // quinta + octava abajo
  { -12.0f, 12.0f },          // octavas
  {   5.0f, 10.0f },          // cuartas
  {  -0.12f, 0.12f },         // coro: dos copias desafinadas ±12 cents
};

// ─── Vocoder: bandas ─────────────────────────────────────────
#define VOC_BANDAS   12
const float VOC_F_MIN   = 180.0f;
const float VOC_F_MAX   = 5000.0f;
const float VOC_Q       = 5.0f;
const float VOC_GANANCIA = 14.0f;

// ─── Memoria de líneas de retardo: UN solo bloque compartido ──
// Los efectos no suenan a la vez, así que usan el mismo bloque: se limpia al cambiar.
#define POOL_N      52000           // ~203 KB de floats (lo que pide ELECTRO: reverb + doblador + delay)
#define ECO_MASK    32767           // eco: los primeros 32768 (≈ 0.74 s)
#define DOBLE_N     2048            // ELECTRO: doblador
#define DOBLE_MASK  (DOBLE_N - 1)
#define ELDEL_N     16384           // ELECTRO: delay ping-pong, dos líneas (≈ 0.37 s cada una)
#define ELDEL_MASK  (ELDEL_N - 1)
#define DESP_N      4096            // pitch / armonizador
#define DESP_MASK   (DESP_N - 1)
#define FLAN_N      2048            // flanger: dos líneas (L y R)
#define FLAN_MASK   (FLAN_N - 1)
float* pool = nullptr;

// ==============================================================================================================================================
// PEDIDOS ENTRE NÚCLEOS
// ==============================================================================================================================================
// control → audio : efectoPedido, potV[]      audio → control : nivelSalida
// Escrituras de 32 bits alineadas, atómicas en el S3: no hace falta mutex.
volatile int   efectoPedido = EF_VOZ;
volatile float potV[4]      = { 0.5f, 0.5f, 0.5f, 0.7f };
volatile float nivelSalida  = 0.0f;
volatile bool  puertaActiva = true;       // BTN2: puerta de ruido encendida / apagada
volatile float ledTremolo   = 1.0f;       // ELECTRO: estado del trémolo, para que el LED 0 lata

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

// tipo: 0 = pasa-bajos, 1 = pasa-altos, 2 = pasa-banda (0 dB en el centro)
void bqDisenar(Biquad &q, int tipo, float f, float Q) {
  if (f > 0.45f * SR) f = 0.45f * SR;
  if (f < 10.0f) f = 10.0f;
  float w = 2.0f * PI_F * f * INV_SR;
  float cw = cosf(w), sw = sinf(w);
  float alfa = sw / (2.0f * Q);
  float a0 = 1.0f + alfa;
  float b0, b1, b2;
  if (tipo == 0)      { b0 = (1.0f - cw) * 0.5f; b1 = 1.0f - cw;    b2 = b0; }
  else if (tipo == 1) { b0 = (1.0f + cw) * 0.5f; b1 = -(1.0f + cw); b2 = b0; }
  else                { b0 = alfa;               b1 = 0.0f;         b2 = -alfa; }
  q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0;
  q.a1 = (-2.0f * cw) / a0;
  q.a2 = (1.0f - alfa) / a0;
}

// tipo: 3 = peaking, 4 = low-shelf, 5 = high-shelf; gananciaDb en dB
void bqDisenarG(Biquad &q, int tipo, float f, float Q, float gananciaDb) {
  if (f > 0.45f * SR) f = 0.45f * SR;
  float A = powf(10.0f, gananciaDb / 40.0f);
  float w = 2.0f * PI_F * f * INV_SR;
  float cw = cosf(w), sw = sinf(w);
  float alfa = sw / (2.0f * Q);
  float b0, b1, b2, a0, a1, a2;
  if (tipo == 3) {
    b0 = 1.0f + alfa * A; b1 = -2.0f * cw; b2 = 1.0f - alfa * A;
    a0 = 1.0f + alfa / A; a1 = -2.0f * cw; a2 = 1.0f - alfa / A;
  } else {
    float k = 2.0f * sqrtf(A) * alfa;
    if (tipo == 4) {
      b0 = A * ((A + 1) - (A - 1) * cw + k); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - k);
      a0 = (A + 1) + (A - 1) * cw + k;       a1 = -2 * ((A - 1) + (A + 1) * cw);    a2 = (A + 1) + (A - 1) * cw - k;
    } else {
      b0 = A * ((A + 1) + (A - 1) * cw + k); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - k);
      a0 = (A + 1) - (A - 1) * cw + k;       a1 = 2 * ((A - 1) - (A + 1) * cw);      a2 = (A + 1) - (A - 1) * cw - k;
    }
  }
  q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0; q.a1 = a1 / a0; q.a2 = a2 / a0;
}

// lectura fraccional de una línea de retardo circular (tamaño potencia de 2)
inline float leerLinea(const float *buf, int mask, int w, float d) {
  float rp = (float)w - d;
  int i0 = (int)floorf(rp);
  float fr = rp - (float)i0;
  float a = buf[i0 & mask], b = buf[(i0 + 1) & mask];
  return a + (b - a) * fr;
}

// sierra con PolyBLEP
inline float polyBlep(float t, float dt) {
  if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
  if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

uint32_t semillaRuido = 22222u;
inline float ruidoBlanco() {
  semillaRuido = semillaRuido * 1664525u + 1013904223u;
  return (float)(int32_t)semillaRuido * (1.0f / 2147483648.0f);
}

inline float semis(float s) { return powf(2.0f, s / 12.0f); }

// Zona con HISTÉRESIS: el ruido del ADC no hace saltar la zona en los bordes.
// n valores repartidos de punta a punta del recorrido (el centro cae en (n-1)/2).
int zonaHist(float v, int n, int actual) {
  float f = v * (float)(n - 1);
  int cand = (int)(f + 0.5f);
  if (cand < 0) cand = 0;
  if (cand > n - 1) cand = n - 1;
  if (actual >= 0 && cand != actual && fabsf(f - (float)actual) < 0.65f) cand = actual;
  return cand;
}

// ==============================================================================================================================================
// ESTADO DE AUDIO (sólo lo toca la tarea de audio)
// ==============================================================================================================================================
int   efectoActual = EF_VOZ;
float gCambio      = 1.0f;          // fundido al cambiar de efecto
const float PASO_CAMBIO = 1.0f / (0.005f * SAMPLE_RATE);   // 5 ms
float pS[4]        = { 0.5f, 0.5f, 0.5f, 0.7f };           // pots suavizados
const float K_POT  = 1.0f - expf(-(float)BUFFER_SAMPLES / (0.03f * SAMPLE_RATE));  // 30 ms

// Entrada
Biquad hpMic;
float envPuerta = 0.0f, gPuerta = 0.0f;
bool  puertaAbierta = false;
const float A_ENV_SUBE  = 1.0f - expf(-1.0f / (0.001f * SAMPLE_RATE));
const float A_ENV_BAJA  = 1.0f - expf(-1.0f / (0.060f * SAMPLE_RATE));
const float A_PUERTA_AB = 1.0f - expf(-1.0f / (0.002f * SAMPLE_RATE));
const float A_PUERTA_CI = 1.0f - expf(-1.0f / (0.120f * SAMPLE_RATE));

// Salida
float volS = 0.5f;
float dcX1L = 0, dcY1L = 0, dcX1R = 0, dcY1R = 0;
#define LIM_LOOK 64
float limL[LIM_LOOK], limR[LIM_LOOK];
int   limIdx = 0;
float envLim = 0.0f, gLim = 1.0f;
const float LIM_REL_ENV = expf(-1.0f / (0.080f * SAMPLE_RATE));
const float LIM_ATQ     = 1.0f - expf(-1.0f / (0.00025f * SAMPLE_RATE));   // 11 muestras: converge dentro del lookahead
const float LIM_SUELTA  = 1.0f - expf(-1.0f / (0.060f * SAMPLE_RATE));
Biquad lpSalL, lpSalR;

// COMPRESOR (VOZ y ELECTRO): detector de pico, ganancia calculada cada 8 muestras y suavizada
float compEnv = 0.0f, compG = 1.0f, compGObj = 1.0f;
int   compCont = 0;
float compUmbral = 0.08f, compExp = 0.0f, compMakeup = 1.0f;
const float C_ATQ   = 1.0f - expf(-1.0f / (0.003f * SAMPLE_RATE));
const float C_REL   = 1.0f - expf(-1.0f / (0.120f * SAMPLE_RATE));
const float C_SUAVE = 1.0f - expf(-1.0f / (0.001f * SAMPLE_RATE));

// VOZ
Biquad vozGraves, vozAgudos;

// ELECTRO
Biquad elPres;                                   // presencia +3 dB en 3 kHz
float *elDbl = nullptr, *elDelL = nullptr, *elDelR = nullptr;
int   elW = 0;
float elDblFase[2] = { 0.0f, 0.5f };
float elDelD = 10000.0f, elDelObj = 10000.0f, elDelLp = 0.0f;
int   elBpmIdx = 50, elDivIdx = 2;               // 120 BPM, 1/4
float elBarra = 0.0f, elBarraInc = 0.0f, elTrem = 1.0f, elProf = 0.0f;
const float EL_DEL_LP = 1.0f - expf(-2.0f * 3.14159265f * 4500.0f / SAMPLE_RATE);
const float K_TREM    = 1.0f - expf(-1.0f / (0.0025f * SAMPLE_RATE));   // bordes de 2.5 ms: corta sin clic

// AUTOTUNE — detección YIN a 11 kHz (decimado ×4) + PSOLA sincronizado al período
#define AT_N        4096            // líneas de entrada y de salida (overlap-add)
#define AT_MASK     (AT_N - 1)
#define AT_DEC_N    1024            // señal decimada para detectar la altura
#define AT_DEC_MASK (AT_DEC_N - 1)
#define AT_YIN_W     256            // ventana de YIN (23 ms a 11 kHz)
#define AT_TAU_MIN    14            // 787 Hz
#define AT_TAU_MAX   110            // 100 Hz (más grave = sin voz, pasa sin corregir)
const float AT_LATENCIA = 700.0f;   // retraso de la fuente (≥ 1.5 períodos del más grave)
const int   AT_CENTRO   = 441;      // el grano se centra este tanto adelante de la salida
const float AT_UMBRAL_YIN = 0.15f;
float *atIn = nullptr, *atOut = nullptr, *atDec = nullptr;
int   atW = 0, atDecW = 0, atDecCont = 0, atBloque = 0;
Biquad atLpDec;                                  // antialias de la decimación (1 kHz)
float atT = 200.0f;                              // período de la voz (muestras a 44.1 kHz)
float atSrc = -700.0f;                           // fuente del próximo grano, relativa a "ahora"
float atHasta = 0.0f;                            // muestras hasta el próximo grano
float atRatio = 1.0f, atRatioObj = 1.0f, atK = 0.01f;
int   atTono = 10, atEscala = 2, atNota = -100;   // Si♭ menor (fijo por ahora: Instant Crush)
volatile float atFrecDet = 0.0f;                 // diagnóstico: altura detectada (0 = sin voz)
float atMix = 0.0f, atOct = 0.0f;                // POT1 mezcla con el vocoder · POT2 octava abajo
float atFrecPort = 220.0f;                       // nota del sinte del vocoder (sigue a la corregida)
const float AT_VOC_NIVEL = 0.54f;                // iguala el vocoder con la voz afinada (−5.4 dB)
const uint16_t ESCALAS_AT[4] = {                 // bit i = semitono i sobre la tónica
  0x0FFF,                                        // cromática
  (1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<11),   // mayor
  (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<10),   // menor natural
  (1<<0)|(1<<3)|(1<<5)|(1<<7)|(1<<10),                  // pentatónica menor
};

// ROBOT
float robFase = 0.0f, robInc = 0.0f, robMix = 0.0f, robCoef = 0.0f, robLp = 0.0f;

// VOCODER
Biquad vocA[VOC_BANDAS], vocS[VOC_BANDAS];
float  vocEnv[VOC_BANDAS], vocTilt[VOC_BANDAS];
float  sierraFase[3] = { 0.0f, 0.33f, 0.67f };
float  sierraInc[3]  = { 0.0f, 0.0f, 0.0f };
float  vocFrec = 110.0f;
Biquad vocLp;                                    // brillo (POT3)
int    vocNota = 12, vocAcorde = 0;
const float VOC_ATQ = 1.0f - expf(-1.0f / (0.002f * SAMPLE_RATE));
const float VOC_REL = 1.0f - expf(-1.0f / (0.020f * SAMPLE_RATE));

// PITCH / ARMONIZADOR (desplazador de dos cabezas con fundido)
int   despW = 0;
float despFase[2] = { 0.0f, 0.0f };
float despRatio[2] = { 1.0f, 1.0f };
float despVent = 2200.0f, despMix = 0.0f;
int   pitchSemi = 12, armAcorde = 0;
float armNivel = 0.0f, armAncho = 0.0f;

// ECO
int   ecoW = 0;
float ecoD = 0.3f * SAMPLE_RATE, ecoDObj = 0.3f * SAMPLE_RATE;
float ecoFb = 0.0f, ecoCoef = 0.0f, ecoLp = 0.0f;
const float ECO_CINTA = 1.0f - expf(-1.0f / (0.150f * SAMPLE_RATE));   // la cabeza se desliza (cinta)

// CATEDRAL (Schroeder/Freeverb: 4 peines + 2 pasa-todo por canal)
#define REV_PEINES 4
#define REV_AP     2
const int   REV_LARGO_PEINE[REV_PEINES] = { 1674, 1782, 1916, 2034 };   // ×1.5 de Freeverb
const int   REV_LARGO_AP[REV_AP]        = { 556, 441 };
const int   REV_ESTEREO = 35;
float *revPeine[2][REV_PEINES], *revAp[2][REV_AP];
int    revLen[2][REV_PEINES], revApLen[2][REV_AP];
int    revIdx[2][REV_PEINES], revApIdx[2][REV_AP];
float  revGuard[2][REV_PEINES];
float  revFb = 0.8f, revDamp = 0.3f, revMix = 0.3f;

// RADIO
Biquad radBp;
float radDrive = 1.0f, radComp = 1.0f;

// FLANGER
int   flanW = 0;
float flanFase = 0.0f, flanInc = 0.0f, flanProf = 0.0f, flanFb = 0.0f;
float flanUltL = 0.0f, flanUltR = 0.0f;

// ==============================================================================================================================================
// INICIALIZACIÓN / LIMPIEZA
// ==============================================================================================================================================
void prepararDSP() {
  for (int i = 0; i <= TABLA_N; i++) tablaSeno[i] = sinf(2.0f * PI_F * (float)i / (float)TABLA_N);
  for (int e = 0; e < NUM_EFECTOS; e++) trimEfecto[e] = powf(10.0f, TRIM_EFECTO_DB[e] / 20.0f);

  memset(&hpMic, 0, sizeof(hpMic));
  bqDisenar(hpMic, 1, CORTE_MIC_HZ, 0.707f);
  memset(&lpSalL, 0, sizeof(lpSalL));
  memset(&lpSalR, 0, sizeof(lpSalR));
  bqDisenar(lpSalL, 0, 13000.0f, 0.707f);
  bqDisenar(lpSalR, 0, 13000.0f, 0.707f);

  // Vocoder: bandas repartidas en escala logarítmica. La sierra pierde energía por banda
  // hacia arriba (~1/√f con Q constante), el "tilt" lo compensa para que las consonantes se oigan.
  for (int b = 0; b < VOC_BANDAS; b++) {
    float f = VOC_F_MIN * powf(VOC_F_MAX / VOC_F_MIN, (float)b / (float)(VOC_BANDAS - 1));
    memset(&vocA[b], 0, sizeof(Biquad));
    memset(&vocS[b], 0, sizeof(Biquad));
    bqDisenar(vocA[b], 2, f, VOC_Q);
    bqDisenar(vocS[b], 2, f, VOC_Q);
    vocTilt[b] = powf(f / VOC_F_MIN, 0.25f);   // compensación suave: con raíz de f sobraban agudos
  }
}

// Reparte la reverb (4 peines + 2 pasa-todo por canal) desde `base` y devuelve dónde termina.
float *armarReverb(float *base) {
  float *p = base;
  for (int c = 0; c < 2; c++) {
    int extra = c * REV_ESTEREO;
    for (int k = 0; k < REV_PEINES; k++) {
      revPeine[c][k] = p; revLen[c][k] = REV_LARGO_PEINE[k] + extra;
      revIdx[c][k] = 0; revGuard[c][k] = 0.0f;
      p += revLen[c][k];
    }
    for (int k = 0; k < REV_AP; k++) {
      revAp[c][k] = p; revApLen[c][k] = REV_LARGO_AP[k] + extra;
      revApIdx[c][k] = 0;
      p += revApLen[c][k];
    }
  }
  return p;
}

void resetCompresor() { compEnv = 0.0f; compG = compGObj = 1.0f; compCont = 0; }

// Limpia el estado del efecto que va a empezar a sonar (se llama en silencio, con gCambio = 0).
void resetEfecto(int e) {
  switch (e) {
    case EF_VOZ:
      memset(&vozGraves, 0, sizeof(Biquad));
      memset(&vozAgudos, 0, sizeof(Biquad));
      bqDisenarG(vozGraves, 4, 150.0f, 0.707f, 0.0f);
      bqDisenarG(vozAgudos, 5, 5000.0f, 0.707f, 0.0f);
      resetCompresor();
      break;
    case EF_ELECTRO: {
      memset(&elPres, 0, sizeof(Biquad));
      bqDisenarG(elPres, 3, 3000.0f, 1.0f, 3.0f);
      float *p = armarReverb(pool);
      elDbl  = p; p += DOBLE_N;
      elDelL = p; p += ELDEL_N;
      elDelR = p; p += ELDEL_N;
      memset(pool, 0, (size_t)(p - pool) * sizeof(float));
      elW = 0; elDelLp = 0.0f; elDelD = elDelObj; elTrem = 1.0f; elBarra = 0.0f;
      resetCompresor();
      break;
    }
    case EF_AUTOTUNE: {
      float *p = pool;
      atIn  = p; p += AT_N;
      atOut = p; p += AT_N;
      atDec = p; p += AT_DEC_N;
      memset(pool, 0, (size_t)(p - pool) * sizeof(float));
      memset(&atLpDec, 0, sizeof(Biquad));
      bqDisenar(atLpDec, 0, 1000.0f, 0.707f);
      for (int b = 0; b < VOC_BANDAS; b++) {
        vocEnv[b] = 0.0f;
        vocA[b].z1 = vocA[b].z2 = vocS[b].z1 = vocS[b].z2 = 0.0f;
      }
      memset(&vocLp, 0, sizeof(Biquad));
      bqDisenar(vocLp, 0, 4000.0f, 0.707f);            // oscuro: al vocoder le sobraban agudos
      atW = atDecW = atDecCont = atBloque = 0;
      atT = 200.0f; atSrc = -AT_LATENCIA; atHasta = 0.0f;
      atRatio = atRatioObj = 1.0f; atNota = -100;
      break;
    }
    case EF_ROBOT:
      robLp = 0.0f;
      break;
    case EF_VOCODER:
      for (int b = 0; b < VOC_BANDAS; b++) {
        vocEnv[b] = 0.0f;
        vocA[b].z1 = vocA[b].z2 = vocS[b].z1 = vocS[b].z2 = 0.0f;
      }
      vocLp.z1 = vocLp.z2 = 0.0f;
      break;
    case EF_PITCH:
    case EF_ARMONIZADOR:
      memset(pool, 0, DESP_N * sizeof(float));
      despW = 0; despFase[0] = 0.0f; despFase[1] = 0.37f;
      break;
    case EF_ECO:
      memset(pool, 0, POOL_N * sizeof(float));
      ecoW = 0; ecoLp = 0.0f; ecoD = ecoDObj;
      break;
    case EF_CATEDRAL: {
      float *p = armarReverb(pool);
      memset(pool, 0, (size_t)(p - pool) * sizeof(float));
      break;
    }
    case EF_RADIO:
      radBp.z1 = radBp.z2 = 0.0f;
      break;
    case EF_FLANGER:
      memset(pool, 0, 2 * FLAN_N * sizeof(float));
      flanW = 0; flanUltL = flanUltR = 0.0f;
      break;
  }
}

// ── AUTOTUNE: detecta el período con YIN sobre la señal decimada y fija la corrección ──
void autotuneDetectar() {
  static float x[AT_YIN_W + AT_TAU_MAX];
  static float d[AT_TAU_MAX + 1];
  const int N = AT_YIN_W + AT_TAU_MAX;
  float energia = 0.0f;
  for (int i = 0; i < N; i++) {
    x[i] = atDec[(atDecW - N + i) & AT_DEC_MASK];
    energia += x[i] * x[i];
  }
  bool sonoro = false;
  float tau = 0.0f;
  if (energia / (float)N > 1.0e-5f) {
    d[0] = 1.0f;
    float acum = 0.0f;
    int elegido = -1;
    for (int t = 1; t <= AT_TAU_MAX; t++) {
      float s = 0.0f;
      for (int j = 0; j < AT_YIN_W; j++) { float df = x[j] - x[j + t]; s += df * df; }
      acum += s;
      d[t] = (acum > 0.0f) ? s * (float)t / acum : 1.0f;    // diferencia normalizada (CMND)
    }
    for (int t = AT_TAU_MIN; t < AT_TAU_MAX; t++) {
      if (d[t] < AT_UMBRAL_YIN) {
        while (t + 1 < AT_TAU_MAX && d[t + 1] < d[t]) t++;   // bajar hasta el mínimo local
        elegido = t;
        break;
      }
    }
    if (elegido > 0) {
      float a = d[elegido - 1], b = d[elegido], c = d[elegido + 1];
      float den = a - 2.0f * b + c;
      float off = (fabsf(den) > 1.0e-9f) ? 0.5f * (a - c) / den : 0.0f;
      if (off > 0.5f) off = 0.5f; else if (off < -0.5f) off = -0.5f;
      tau = (float)elegido + off;
      sonoro = true;
    }
  }
  if (!sonoro) { atRatioObj = 1.0f; atFrecDet = 0.0f; return; }

  atT = tau * 4.0f;                                         // período a 44.1 kHz
  float f = SR / atT;
  atFrecDet = f;
  float m = 69.0f + 12.0f * log2f(f / 440.0f);             // nota MIDI con decimales
  uint16_t esc = ESCALAS_AT[atEscala];
  // ¿La nota actual sigue sirviendo? (histéresis: no salta entre dos notas al cantar justo al medio)
  bool actualOk = atNota > -100 && ((esc >> (((atNota - atTono) % 12 + 12) % 12)) & 1);
  if (!actualOk || fabsf(m - (float)atNota) > 0.75f) {
    int mejor = -100; float mejorDist = 99.0f;
    int base = (int)floorf(m + 0.5f);
    for (int c = base - 6; c <= base + 6; c++) {
      if (!((esc >> (((c - atTono) % 12 + 12) % 12)) & 1)) continue;
      float dd = fabsf(m - (float)c);
      if (dd < mejorDist) { mejorDist = dd; mejor = c; }
    }
    atNota = mejor;
  }
  float r = powf(2.0f, ((float)atNota - m) / 12.0f);
  if (r < 0.5f) r = 0.5f; else if (r > 2.0f) r = 2.0f;
  atRatioObj = r;
}

// ==============================================================================================================================================
// PARÁMETROS POR BLOQUE (a partir de los pots suavizados)
// ==============================================================================================================================================
void parametrosBloque() {
  float p1 = pS[0], p2 = pS[1], p3 = pS[2];
  float v = pS[3];
  volS = v * v * MASTER;

  switch (efectoActual) {
    case EF_VOZ: {
      // graves / agudos: ±6 dB con zona muerta al centro (al centro = voz plana)
      float g1 = (p1 - 0.5f) * 2.0f, g2 = (p2 - 0.5f) * 2.0f;
      if (fabsf(g1) < 0.08f) g1 = 0.0f;
      if (fabsf(g2) < 0.08f) g2 = 0.0f;
      bqDisenarG(vozGraves, 4, 150.0f, 0.707f, g1 * 6.0f);
      bqDisenarG(vozAgudos, 5, 5000.0f, 0.707f, g2 * 6.0f);
      float ratio = 1.0f + 3.0f * p3;                    // 1:1 … 4:1
      compUmbral = 0.08f;
      compExp    = 1.0f / ratio - 1.0f;
      compMakeup = powf(0.3f / compUmbral, (1.0f - 1.0f / ratio) * 0.7f);
      break;
    }

    case EF_ELECTRO: {
      elBpmIdx = zonaHist(p1, 111, elBpmIdx);            // 70 … 180 BPM, de a 1
      elDivIdx = zonaHist(p2, NUM_DIVS, elDivIdx);
      elProf   = p3;
      float bpm = 70.0f + (float)elBpmIdx;
      elBarraInc = bpm / (240.0f * SR);                  // un compás de 4/4 = 4 negras
      float t = SR * 30.0f / bpm;                        // corchea (1/8)
      if (t > (float)(ELDEL_N - 8)) t *= 0.5f;           // tempo lento: semicorchea
      elDelObj = t;
      revFb = 0.84f; revDamp = 0.45f;                    // plate corta y oscura
      compUmbral = 0.06f;
      compExp    = 1.0f / 4.0f - 1.0f;                   // 4:1
      compMakeup = 2.3f;
      break;
    }

    case EF_AUTOTUNE: {
      // escala fija (Si♭ menor) por ahora: POT1 y POT2 manejan la capa de vocoder
      atMix = p1;
      atOct = p2;
      // velocidad: 0 → 120 ms (natural, respeta el vibrato) … 1 → instantáneo (robot)
      float q = 1.0f - p3;
      float tauS = 0.120f * q * q + 0.0005f;
      atK = 1.0f - expf(-1.0f / (tauS * SR));
      if (++atBloque >= 4) { atBloque = 0; autotuneDetectar(); }
      if (atNota > -100) {
        float fObj = 440.0f * powf(2.0f, ((float)atNota - 69.0f) / 12.0f);
        atFrecPort += (fObj - atFrecPort) * 0.5f;     // glide de ~5 ms entre notas
      }
      sierraInc[0] = atFrecPort * INV_SR;
      sierraInc[1] = atFrecPort * 0.5f * INV_SR;     // octava abajo
      break;
    }

    case EF_ROBOT:
      robInc  = 30.0f * powf(33.3f, p1) * INV_SR;        // 30 Hz – 1 kHz
      robMix  = p2;
      robCoef = coefLP1(600.0f * powf(20.0f, p3));       // 600 Hz – 12 kHz
      break;

    case EF_VOCODER: {
      vocNota   = zonaHist(p1, 25, vocNota);
      vocAcorde = zonaHist(p2, NUM_ACORDES_VOC, vocAcorde);
      float fObj = NOTA_BASE_VOC * semis((float)vocNota);
      vocFrec += (fObj - vocFrec) * 0.35f;               // glide de ~8 ms entre notas
      for (int k = 0; k < 3; k++)
        sierraInc[k] = vocFrec * semis(ACORDE_VOC[vocAcorde][k]) * INV_SR;
      bqDisenar(vocLp, 0, 800.0f * powf(10.0f, p3), 0.707f);   // brillo 0.8–8 kHz
      break;
    }

    case EF_PITCH:
      pitchSemi = zonaHist(p1, 25, pitchSemi);
      despRatio[0] = semis((float)(pitchSemi - 12));
      despVent = (0.025f + 0.060f * p3) * SR;            // grano 25–85 ms
      despMix  = p2;
      break;

    case EF_ARMONIZADOR:
      armAcorde = zonaHist(p1, NUM_ACORDES_ARM, armAcorde);
      despRatio[0] = semis(ACORDE_ARM[armAcorde][0]);
      despRatio[1] = semis(ACORDE_ARM[armAcorde][1]);
      despVent = 0.050f * SR;
      armNivel = p2;
      armAncho = p3;
      break;

    case EF_ECO:
      ecoDObj = (0.060f + 0.660f * p1) * SR;             // 60–720 ms
      ecoFb   = 0.85f * p2;
      ecoCoef = coefLP1(1200.0f * powf(10.0f, p3));      // 1.2–12 kHz dentro del lazo
      break;

    case EF_CATEDRAL:
      revFb   = 0.72f + 0.26f * p1;
      revDamp = 0.60f - 0.55f * p2;
      revMix  = p3;
      break;

    case EF_RADIO:
      radDrive = 1.0f + 19.0f * p3 * p3;
      radComp  = 1.0f / (1.0f + 0.25f * sqrtf(radDrive - 1.0f));
      bqDisenar(radBp, 2, 300.0f * powf(10.0f, p1), 0.7f + 4.3f * p2);
      break;

    case EF_FLANGER:
      flanInc  = 0.05f * powf(100.0f, p1) * INV_SR;      // 0.05–5 Hz
      flanProf = 20.0f + 420.0f * p2;                    // hasta ~10 ms de barrido
      flanFb   = 0.85f * p3;
      break;
  }
}

// ==============================================================================================================================================
// EFECTOS — una muestra de entrada (mono) → salida estéreo
// ==============================================================================================================================================

// Desplazador de altura: dos cabezas de lectura que recorren la línea a velocidad `ratio`,
// cada una con ventana sen² y desfasadas medio ciclo: cuando una salta, la otra está a pleno.
inline float leerDesplazado(int v) {
  float &fase = despFase[v];
  fase += (1.0f - despRatio[v]) / despVent;
  fase -= floorf(fase);
  float f2 = fase + 0.5f; if (f2 >= 1.0f) f2 -= 1.0f;
  float s = senoT(fase * 0.5f);                         // sen(π·fase)
  float g1 = s * s, g2 = 1.0f - g1;
  float a = leerLinea(pool, DESP_MASK, despW, 2.0f + fase * despVent);
  float b = leerLinea(pool, DESP_MASK, despW, 2.0f + f2 * despVent);
  return a * g1 + b * g2;
}

inline float comprimir(float x) {
  float a = fabsf(x);
  compEnv += (a - compEnv) * (a > compEnv ? C_ATQ : C_REL);
  if (--compCont <= 0) {
    compCont = 8;
    compGObj = compEnv > compUmbral ? powf(compEnv / compUmbral, compExp) : 1.0f;
  }
  compG += (compGObj - compG) * C_SUAVE;
  return x * compG * compMakeup;
}

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

    case EF_VOZ: {
      float y = bq(vozAgudos, bq(vozGraves, x));
      L = R = comprimir(y);
      break;
    }

    case EF_ELECTRO: {
      // voz: presencia → compresor
      float c = comprimir(bq(elPres, x));
      // doblador: dos copias a 12 y 19 ms que se mecen ±0.3 ms, una a cada lado
      elDbl[elW & DOBLE_MASK] = c;
      elDblFase[0] += 0.31f * INV_SR; if (elDblFase[0] >= 1.0f) elDblFase[0] -= 1.0f;
      elDblFase[1] += 0.23f * INV_SR; if (elDblFase[1] >= 1.0f) elDblFase[1] -= 1.0f;
      float dblL = leerLinea(elDbl, DOBLE_MASK, elW, 530.0f + 12.0f * senoT(elDblFase[0]));
      float dblR = leerLinea(elDbl, DOBLE_MASK, elW, 840.0f + 12.0f * senoT(elDblFase[1]));
      // delay ping-pong a 1/8 del tempo, oscuro (4.5 kHz dentro del lazo)
      elDelD += (elDelObj - elDelD) * ECO_CINTA;
      float yL = leerLinea(elDelL, ELDEL_MASK, elW, elDelD);
      float yR = leerLinea(elDelR, ELDEL_MASK, elW, elDelD);
      elDelLp += (yR - elDelLp) * EL_DEL_LP;
      elDelL[elW & ELDEL_MASK] = c * 0.5f + elDelLp * 0.38f + 1.0e-18f;
      elDelR[elW & ELDEL_MASK] = yL;
      elW = (elW + 1) & 65535;
      // reverb plate
      float rL, rR;
      procReverb(c * (1.0f - revFb) * 2.0f, rL, rR);
      L = c + dblL * 0.28f + yL * 0.30f + rL * 0.25f;
      R = c + dblR * 0.28f + yR * 0.30f + rR * 0.25f;
      // TRÉMOLO CUANTIZADO: la fase vive en compases, así cambiar de división no desfasa
      elBarra += elBarraInc; if (elBarra >= 1.0f) elBarra -= 1.0f;
      float t = elBarra * (float)DIVS_TREMOLO[elDivIdx];
      t -= floorf(t);
      float cuadrada = (t < 0.5f) ? 1.0f : 0.0f;
      elTrem += (cuadrada - elTrem) * K_TREM;
      float g = 1.0f - elProf + elProf * elTrem;
      L *= g; R *= g;
      break;
    }

    case EF_AUTOTUNE: {
      // entrada + señal decimada para la detección
      atIn[atW] = x;
      float v = bq(atLpDec, x);
      if (++atDecCont >= 4) { atDecCont = 0; atDec[atDecW] = v; atDecW = (atDecW + 1) & AT_DEC_MASK; }
      atRatio += (atRatioObj - atRatio) * atK;
      // PSOLA: cada T/ratio muestras se suma un grano de 2 períodos con ventana Hann. La fuente
      // avanza de a períodos enteros (repite o salta uno) para quedar cerca de "ahora - latencia":
      // así los granos se enciman EN FASE y no aparece el chorus del desplazador de dos cabezas.
      atSrc -= 1.0f;
      atHasta -= 1.0f;
      if (atHasta <= 0.0f) {
        float T = atT;
        float m = floorf((-AT_LATENCIA - atSrc) / T + 0.5f);
        atSrc += m * T;
        if (atSrc > -AT_LATENCIA + T || atSrc < -AT_LATENCIA - T) atSrc = -AT_LATENCIA;   // re-anclar
        int Ti = (int)T;
        if (Ti > AT_CENTRO) Ti = AT_CENTRO;               // el grano nunca toca lo ya leído
        float invT = 1.0f / T;
        float gnorm = 1.0f / atRatio;                      // Hann con paso T/ratio suma "ratio"
        for (int j = -Ti; j <= Ti; j++) {
          float w = 0.5f + 0.5f * senoT(0.25f + 0.5f * (float)j * invT);   // Hann: ½(1+cos(πj/T))
          float s = leerLinea(atIn, AT_MASK, atW, -(atSrc + (float)j));
          atOut[(atW + AT_CENTRO + j) & AT_MASK] += s * w * gnorm;
        }
        atHasta += T / atRatio;
      }
      float y = atOut[atW];
      atOut[atW] = 0.0f;
      float voc = 0.0f;
      if (atMix > 0.01f) {
        // el análisis va con la entrada RETRASADA lo mismo que el PSOLA: vocoder y voz afinada
        // quedan en el mismo tiempo (si no, las consonantes suenan dobles)
        float xa = leerLinea(atIn, AT_MASK, atW, AT_LATENCIA + (float)AT_CENTRO);
        float c = 0.0f;
        for (int k = 0; k < 2; k++) {
          float &t = sierraFase[k];
          t += sierraInc[k]; if (t >= 1.0f) t -= 1.0f;
          float sierra = 2.0f * t - 1.0f - polyBlep(t, sierraInc[k]);
          c += (k == 0) ? sierra : sierra * atOct;
        }
        c *= 0.5f;
        for (int b = 0; b < VOC_BANDAS; b++) {
          float a = fabsf(bq(vocA[b], xa));
          float &e = vocEnv[b];
          e += (a - e) * (a > e ? VOC_ATQ : VOC_REL);
          voc += bq(vocS[b], c) * e * vocTilt[b];
        }
        voc = bq(vocLp, voc) * VOC_GANANCIA * AT_VOC_NIVEL;
      }
      atW = (atW + 1) & AT_MASK;
      L = R = y * sqrtf(1.0f - atMix) + voc * sqrtf(atMix);   // potencia constante: el medio no baja
      break;
    }

    case EF_ROBOT: {
      robFase += robInc; if (robFase >= 1.0f) robFase -= 1.0f;
      float w = x * senoT(robFase) * 1.4f;
      robLp += (w - robLp) * robCoef;
      L = R = x * (1.0f - robMix) + robLp * robMix;
      break;
    }

    case EF_VOCODER: {
      // portadora: tres sierras (el acorde)
      float c = 0.0f;
      for (int k = 0; k < 3; k++) {
        float &t = sierraFase[k];
        t += sierraInc[k]; if (t >= 1.0f) t -= 1.0f;
        c += 2.0f * t - 1.0f - polyBlep(t, sierraInc[k]);
      }
      c *= 0.33f;
      float y = 0.0f;
      for (int b = 0; b < VOC_BANDAS; b++) {
        float a = fabsf(bq(vocA[b], x));
        float &e = vocEnv[b];
        e += (a - e) * (a > e ? VOC_ATQ : VOC_REL);
        y += bq(vocS[b], c) * e * vocTilt[b];
      }
      L = R = bq(vocLp, y) * VOC_GANANCIA;
      break;
    }

    case EF_PITCH: {
      pool[despW] = x;
      float w = leerDesplazado(0);
      despW = (despW + 1) & DESP_MASK;
      L = R = x * (1.0f - despMix) + w * despMix;
      break;
    }

    case EF_ARMONIZADOR: {
      pool[despW] = x;
      float v1 = leerDesplazado(0) * armNivel;
      float v2 = leerDesplazado(1) * armNivel;
      despW = (despW + 1) & DESP_MASK;
      float a = 0.5f + 0.5f * armAncho, b = 1.0f - a;   // v1 a la izquierda, v2 a la derecha
      L = 0.8f * x + (v1 * a + v2 * b) * 0.9f;
      R = 0.8f * x + (v1 * b + v2 * a) * 0.9f;
      break;
    }

    case EF_ECO: {
      ecoD += (ecoDObj - ecoD) * ECO_CINTA;
      float y = leerLinea(pool, ECO_MASK, ecoW, ecoD);
      ecoLp += (y - ecoLp) * ecoCoef;
      pool[ecoW] = x + softClip(ecoLp * ecoFb) + 1.0e-18f;   // saturación suave DENTRO del lazo
      ecoW = (ecoW + 1) & ECO_MASK;
      L = R = x + ecoLp * 0.8f;
      break;
    }

    case EF_CATEDRAL: {
      // La entrada se normaliza por la realimentación: si no, la reverb florece al alargarla.
      float sL, sR;
      procReverb(x * (1.0f - revFb) * 2.0f, sL, sR);
      L = x * (1.0f - revMix) + sL * revMix;
      R = x * (1.0f - revMix) + sR * revMix;
      break;
    }

    case EF_RADIO: {
      // saturación ANTES del filtro: el filtro saca los armónicos agudos que ella genera
      float s = softClip(x * radDrive) * radComp;
      L = R = bq(radBp, s) * 1.8f;
      break;
    }

    case EF_FLANGER: {
      float *bufL = pool, *bufR = pool + FLAN_N;
      flanFase += flanInc; if (flanFase >= 1.0f) flanFase -= 1.0f;
      float dL = 44.0f + flanProf * (0.5f + 0.5f * senoT(flanFase));
      float dR = 44.0f + flanProf * (0.5f + 0.5f * senoT(flanFase + 0.25f));   // L y R en cuadratura
      float yL = leerLinea(bufL, FLAN_MASK, flanW, dL);
      float yR = leerLinea(bufR, FLAN_MASK, flanW, dR);
      bufL[flanW] = x + yL * flanFb + 1.0e-18f;
      bufR[flanW] = x + yR * flanFb + 1.0e-18f;
      flanW = (flanW + 1) & FLAN_MASK;
      float comp = 0.6f * (1.0f - 0.5f * flanFb);
      L = (x + yL) * comp;
      R = (x + yR) * comp;
      break;
    }

    default:
      L = R = x;
  }
}

// ==============================================================================================================================================
// BLOQUE DE AUDIO: 128 muestras del micro (estéreo 32 bit, canal izquierdo) → 128 al DAC
// ==============================================================================================================================================
void procesarBloque(const int32_t *mic, int16_t *salida) {
  // ── Borde del buffer: pedidos de la tarea de control ──
  int pedido = efectoPedido;
  if (pedido != efectoActual && gCambio <= 0.0f) {   // ya en silencio: se cambia y se limpia
    efectoActual = pedido;
    resetEfecto(efectoActual);
  }
  float objetivoCambio = (pedido == efectoActual) ? 1.0f : 0.0f;

  for (int i = 0; i < 4; i++) pS[i] += (potV[i] - pS[i]) * K_POT;
  parametrosBloque();

  float pico = 0.0f;
  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    // ── Entrada ──
    float x = (float)mic[n * 2] * (1.0f / 2147483648.0f);
    x = bq(hpMic, x) * GANANCIA_MIC;

    // ── Puerta de ruido ──
    float ax = fabsf(x);
    envPuerta += (ax - envPuerta) * (ax > envPuerta ? A_ENV_SUBE : A_ENV_BAJA);
    if (!puertaAbierta && envPuerta > UMBRAL_PUERTA)              puertaAbierta = true;
    else if (puertaAbierta && envPuerta < UMBRAL_PUERTA * 0.5f)   puertaAbierta = false;
    float gObj = (puertaAbierta || !puertaActiva) ? 1.0f : 0.0f;   // sin puerta = pasa todo (ambiente)
    gPuerta += (gObj - gPuerta) * (gObj > gPuerta ? A_PUERTA_AB : A_PUERTA_CI);
    x *= gPuerta;

    // ── Efecto ──
    float L, R;
    procesarEfecto(x, L, R);

    // ── Fundido de cambio de efecto ──
    if (gCambio < objetivoCambio)      { gCambio += PASO_CAMBIO; if (gCambio > 1.0f) gCambio = 1.0f; }
    else if (gCambio > objetivoCambio) { gCambio -= PASO_CAMBIO; if (gCambio < 0.0f) gCambio = 0.0f; }
    float g = gCambio * volS * trimEfecto[efectoActual];
    L *= g; R *= g;

    // ── Bloqueador de continua ──
    float dl = L - dcX1L + 0.9995f * dcY1L; dcX1L = L; dcY1L = dl;
    float dr = R - dcX1R + 0.9995f * dcY1R; dcX1R = R; dcY1R = dr;

    // ── Limitador con lookahead de 64 muestras (ganancia suavizada, sin escalones) ──
    float pk = fabsf(dl) > fabsf(dr) ? fabsf(dl) : fabsf(dr);
    envLim = pk > envLim ? pk : envLim * LIM_REL_ENV;
    float gObjLim = envLim > TECHO ? TECHO / envLim : 1.0f;
    gLim += (gObjLim - gLim) * (gObjLim < gLim ? LIM_ATQ : LIM_SUELTA);
    float oL = limL[limIdx] * gLim, oR = limR[limIdx] * gLim;
    limL[limIdx] = dl; limR[limIdx] = dr;
    limIdx = (limIdx + 1) & (LIM_LOOK - 1);

    // ── Techo de 13 kHz DESPUÉS del limitador (el limitador modula) ──
    oL = bq(lpSalL, oL);
    oR = bq(lpSalR, oR);

    float aL = fabsf(oL), aR = fabsf(oR);
    if (aL > pico) pico = aL;
    if (aR > pico) pico = aR;

    float sl = oL * 32767.0f, sr = oR * 32767.0f;
    if (sl >  32767.0f) sl =  32767.0f; else if (sl < -32767.0f) sl = -32767.0f;
    if (sr >  32767.0f) sr =  32767.0f; else if (sr < -32767.0f) sr = -32767.0f;
    salida[n * 2]     = (int16_t)sl;
    salida[n * 2 + 1] = (int16_t)sr;
  }
  nivelSalida = pico;
  ledTremolo  = (efectoActual == EF_ELECTRO) ? (1.0f - elProf + elProf * elTrem) : 1.0f;
}

// ==============================================================================================================================================
// HARDWARE (todo lo de abajo no entra en el simulador de PC)
// ==============================================================================================================================================
#ifndef SIMULADOR

i2s_chan_handle_t tx_chan, rx_chan;
CRGB leds[NUM_LEDS];

float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) sum += analogRead(pin);
  return (float)(sum >> 2) / 4095.0f;
}

void i2s_dac_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear    = true;
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;     // cola de ~12 ms: poca latencia de voz
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

// El micro corre a la MISMA tasa y con la misma fuente de reloj que el DAC: los dos relojes
// salen del mismo divisor, así que no derivan y no hace falta remuestrear.
void i2s_mic_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));
  // El INMP441 entrega 24 bits en un slot de 32: estéreo de 32 bits (64 BCLK por trama) y
  // nos quedamos con el canal IZQUIERDO (L/R a GND).
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

// ─── Tarea de audio (core 1): leer el micro marca el ritmo; el DAC recibe lo mismo que entra ──
void audioTask(void *) {
  static int32_t micBuf[BUFFER_SAMPLES * 2];
  static int16_t salBuf[BUFFER_SAMPLES * 2];
  size_t n;

  // Vaciar lo que el micro acumuló durante el arranque (si no, esa cola es latencia fija)
  for (int k = 0; k < 16; k++)
    if (i2s_channel_read(rx_chan, micBuf, sizeof(micBuf), &n, 0) != ESP_OK) break;
  // Dos buffers de silencio al DAC: el colchón que absorbe el vaivén de la tarea
  memset(salBuf, 0, sizeof(salBuf));
  for (int k = 0; k < 2; k++) i2s_channel_write(tx_chan, salBuf, sizeof(salBuf), &n, portMAX_DELAY);

  for (;;) {
    if (i2s_channel_read(rx_chan, micBuf, sizeof(micBuf), &n, portMAX_DELAY) != ESP_OK) continue;
    if (n < sizeof(micBuf)) memset((uint8_t *)micBuf + n, 0, sizeof(micBuf) - n);
    procesarBloque(micBuf, salBuf);
    i2s_channel_write(tx_chan, salBuf, sizeof(salBuf), &n, portMAX_DELAY);
  }
}

// ─── Tarea de control (core 0, 1 kHz): botón, pots, LEDs ──
const uint8_t BTN_PIN[5] = { BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN, BTN5_PIN };
bool btnNivel = HIGH, btn2Nivel = HIGH;
unsigned long btnTiempo = 0, btn2Tiempo = 0;
float flash = 0.0f, vu = 0.0f;

void anunciarEfecto(int e) {
#if MOSTRAR_ESTADO
  Serial.printf("[%d/%d] %s  ->  %s · POT4 volumen\n", e + 1, NUM_EFECTOS, NOMBRE_EFECTO[e], AYUDA_EFECTO[e]);
#endif
}

void renderLEDs() {
  int e = efectoPedido;
  float pk = nivelSalida;
  float db = (pk > 1.0e-5f) ? 20.0f * log10f(pk) : -100.0f;
  float nivel = (db + 48.0f) / 48.0f;                  // −48 dB … 0 dB → 0 … 1
  if (nivel < 0.0f) nivel = 0.0f;
  if (nivel > 1.0f) nivel = 1.0f;
  vu = nivel > vu ? nivel : vu * 0.85f;

  leds[0] = puertaActiva ? CHSV(HUE_EFECTO[e], 255, 200) : CRGB(150, 150, 150);   // blanco = sin puerta
  if (e == EF_ELECTRO) leds[0].nscale8_video((uint8_t)(40 + 215 * ledTremolo));    // late con el trémolo
  float barra = vu * 5.0f;
  for (int i = 0; i < 5; i++) {
    float f = barra - (float)i;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    leds[1 + i] = CHSV(HUE_EFECTO[e], 230, (uint8_t)(f * 180.0f));
  }
  if (flash > 0.02f) {
    uint8_t w = (uint8_t)(flash * 160.0f);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] += CRGB(w, w, w);
    flash *= 0.75f;
  }
  FastLED.show();
}

void pasoControl() {
  unsigned long tms = millis();

  // BTN1: efecto siguiente, en el flanco de presión
  bool nivel = digitalRead(BTN1_PIN);
  if (nivel == LOW && btnNivel == HIGH && (tms - btnTiempo) > DEBOUNCE_MS) {
    btnTiempo = tms;
    int e = (efectoPedido + 1) % NUM_EFECTOS;
    efectoPedido = e;
    flash = 1.0f;
    anunciarEfecto(e);
  }
  btnNivel = nivel;

  // BTN2: puerta de ruido encendida / apagada (apagada = se oye también el ambiente)
  bool nivel2 = digitalRead(BTN2_PIN);
  if (nivel2 == LOW && btn2Nivel == HIGH && (tms - btn2Tiempo) > DEBOUNCE_MS) {
    btn2Tiempo = tms;
    puertaActiva = !puertaActiva;
    flash = 0.6f;
#if MOSTRAR_ESTADO
    Serial.printf("Puerta de ruido: %s\n", puertaActiva ? "ENCENDIDA (sólo voz)" : "APAGADA (voz + ambiente)");
#endif
  }
  btn2Nivel = nivel2;

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

  // Líneas de retardo: RAM interna (rápida); si no hay, PSRAM
  pool = (float *)heap_caps_malloc(POOL_N * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!pool) pool = (float *)heap_caps_malloc(POOL_N * sizeof(float), MALLOC_CAP_SPIRAM);
  if (!pool) {
    // Sin memoria para los retardos: LEDs rojos parpadeando y nada más
    for (;;) {
      fill_solid(leds, NUM_LEDS, (millis() / 300) % 2 ? CRGB(120, 0, 0) : CRGB::Black);
      FastLED.show();
      delay(50);
    }
  }
  memset(pool, 0, POOL_N * sizeof(float));

  prepararDSP();
  resetEfecto(efectoActual);
  i2s_dac_init();
  i2s_mic_init();

#if MOSTRAR_ESTADO
  Serial.println("\nVOZ FX — BTN1 cambia de efecto · POT4 volumen · usa audífonos");
  anunciarEfecto(efectoActual);
#endif

  xTaskCreatePinnedToCore(audioTask,   "audio",   8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask, "control", 4096, NULL,  3, NULL, 0);
}

void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }

#endif  // SIMULADOR
