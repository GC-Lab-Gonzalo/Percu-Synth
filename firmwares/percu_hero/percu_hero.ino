// ==============================================================================================================================================
// PERCU-SYNTH — PERCU HERO (juego de ritmo en la OLED, con la música sintetizada en la placa) — GC Lab Chile
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
// - Pantalla OLED 0.96" SSD1306 128×64 I2C en el conector OLED de la V2.0 |SDA -> 21, SCL -> 38| (dir. 0x3C)
//   HORIZONTAL, como viene montada
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 6 LEDs SMD WS2812 de la placa |DATA -> 46|
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - Flash Mode         : DIO          (¡OPI rompe I2S!)
// - PSRAM              : OPI PSRAM    (no se usa, pero es el ajuste de siempre)
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (incluye driver/i2s_std.h y Wire)
// - FastLED
// - La SSD1306 se maneja directo por I2C (sin Adafruit ni U8g2)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// El Percu Hero del navegador (videogame/percu_hero), pero ENTERO EN LA PLACA: la canción «Turno de noche»
// se sintetiza y sale por el DAC, las notas bajan por la OLED y los 5 botones son los 5 carriles. Sin PC.
//
//   · La BASE suena siempre: bombo de tres bandas a negras, martillos y pistones en 3+3+2, hats, clap y un
//     bajo sucio que vive entre los bombos. Re frigio dominante (hijaz), un solo acorde, 128 BPM.
//   · La MELODÍA es del jugador y sólo suena si acierta. Va SIEMPRE en la grilla: mientras vienes acertando,
//     cada nota entra exacta sobre la base; al fallar se corta y se calla hasta el próximo acierto.
//   · La canción CRECE por secciones (energía): se abre el filtro de la base, sube el nivel, el bajo se
//     abre y ensucia, la melodía cambia de timbre. Dos quiebres con dron y una subida antes del final.
//   · ENERGÍA: cada falla quita, cada acierto devuelve (más con PERFECTO y con el multiplicador alto).
//     En cero se acaba la partida.
//
// Todo lo musical está escrito como TEXTO más abajo (BAT, BAJO, SECCIONES), igual que en el juego web.
// El audio tiene el core 1 para él solo y es el único dueño del juego; los botones (1 kHz) y la pantalla
// van en el core 0 y sólo le dejan eventos con la hora exacta del toque.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// EN EL MENÚ:
// - BTN1 / BTN5 : dificultad anterior / siguiente (FÁCIL · NORMAL · DURO)
// - BTN3        : empezar
// JUGANDO:
// - BTN1 … BTN5 : los cinco carriles, de izquierda a derecha. Aprieta cuando la nota llega a la casilla.
//                 Las notas con cola se mantienen apretadas hasta que termina la cola.
// AL TERMINAR (o al quedar sin energía):
// - BTN3        : otra vez
// - BTN1 / BTN5 : volver al menú
// LEDS:
// - LED 1 … 5   : el botón apretado (crema) y el acierto (ámbar); rojo al fallar
// - LED 6       : late con el bombo; magenta si un bloque de audio se pasa del 80 % de su tiempo
// ==============================================================================================================================================
#pragma GCC optimize ("O2")
#include <Arduino.h>
#include <Wire.h>
#include <FastLED.h>
#include "driver/i2s_std.h"

// ── Pines ──
#define I2S_LCK 39
#define I2S_DIN 40
#define I2S_BCK 41
#define OLED_SDA 21
#define OLED_SCL 38
#define OLED_DIR 0x3C
#define I2C_HZ   800000
#define LED_PIN  46
#define NUM_LEDS 6
const uint8_t BTN_PIN[5] = { 44, 42, 0, 45, 47 };

#define SAMPLE_RATE    44100
#define BUFFER_SAMPLES 128
const float SR = 44100.0f;

// ── Ajustes del juego ──
const float LATENCIA_MS   = 13.0f;     // cola del DMA (4×128) + DAC: lo que tarda en OÍRSE lo que se rinde
const float PANTALLA_MS   = 12.0f;     // lo que tarda un cuadro en llegar a la OLED: se dibuja ese poco adelantado
const float V_PERFECTO    = 0.045f, V_BIEN = 0.100f;                // ventanas de acierto (s a cada lado)
const float VIDA_INICIAL  = 0.70f, VIDA_BIEN = 0.014f, VIDA_PERFECTO = 0.022f, VIDA_POR_MULT = 0.004f, VIDA_EN_FALSO = 0.025f;
const float DIF_VENTANA[3] = { 1.9f, 1.6f, 1.3f };                  // segundos de pista a la vista (hasta el horizonte)
const float DIF_GOLPE[3]   = { 0.06f, 0.08f, 0.10f };               // energía que quita cada falla
const char *DIF_NOMBRE[3]  = { "FACIL", "NORMAL", "DURO" };
const float VOLUMEN = 0.85f, MEZCLA = 0.50f;      // MEZCLA: nivel de la suma antes del limitador (con 1.0 el limitador bajaba 6 dB)
// MEDIR = 1 (arduino-cli … --build-property "compiler.cpp.extra_flags=-DMEDIR=1"): la canción parte sola en DURO,
// se toca sola, y por los dos conectores USB (Serial y Serial0, 115200) sale cada 2 s el % del tiempo de bloque que usa el audio.
#ifndef MEDIR
#define MEDIR 0
#endif

// ==============================================================================================================================================
// ESTRUCTURAS (arriba de todo: arduino-cli genera los prototipos antes de la primera función)
// ==============================================================================================================================================
struct BQ  { float b0, b1, b2, a1, a2, z1, z2; };
struct SVF { float g, k, a1, a2, a3, s1, s2; };
// Un golpe de percusión: hasta dos parciales senoidales que caen + una banda de ruido, cada cosa con su envolvente.
struct Golpe {
  bool  activo;
  float fase1, fase2, f, fFin, kF, r2, a1, a2;
  float e1, k1, e2, k2, eN, kN, pico1, pico2, picoN;
  int   atk;
  BQ    bp; float fc, fcFin, kFc, q; int cuentaFc;
  float panL, panR; uint8_t bus;           // 0 bombo · 1 resto · 2 directo · 3 melodía
  float llave; BQ bp0;                     // lo ya calculado para estos parámetros (ver golpear)
};
struct TimbreMel { uint8_t tipo; float det, ruido, pre; bool banda; float corte, oct, q, tauF, nivel, tau; };
struct Voz {
  bool  activo, sost, soltando;
  int   nota; uint8_t tm;
  float f, fase[3], faseM, vel;
  float amp, pico, kAmp, objetivo; int atk;
  float fenv, kFenv, idx, kIdx;
  SVF   fl;
};
struct Nota { int32_t t, dur; float f, vel; uint8_t carril, tm, estado; bool sost; int8_t voz; };   // estado: 0 pendiente · 1 acertada · 2 fallada
struct Compas { uint8_t bat; int8_t bajo; bool fill, plato, golpe, sube; float e; int32_t dron, subida; uint8_t fondo; float dens; };
struct Seccion {
  uint8_t bat[4]; uint8_t nBat; int8_t bajo; bool fill; float e0, e1; uint8_t tm; bool dron, sube, golpe, alto;
  uint8_t n; const char *comp[12]; const char *doble[12];
};
struct Evento { int8_t carril; bool abajo; int32_t t; };

// ==============================================================================================================================================
// LA CANCIÓN — «Turno de noche» (todo como texto, igual que en videogame/percu_hero)
// ==============================================================================================================================================
const float BPM = 128.0f;
const int   ESCALA[7] = { 0, 1, 4, 5, 7, 8, 10 };        // Re hijaz: Re Mi♭ Fa# Sol La Si♭ Do
const float RE4 = 293.665f, RE2 = 73.416f;
const int   CARRILES_BASE[5] = { 1, 2, 3, 5, 8 };        // Re Mi♭ Fa# La Re
const int   CARRILES_ALTO[5] = { 5, 6, 7, 8, 9 };        // La Si♭ Do Re Mi♭

// BATERÍA — 7 líneas de 16: BOMBO · HAT · MARTILLO · PISTÓN · CLAP · ABIERTO · TOM      X fuerte · x normal · . fantasma
enum { P0, P1, P2, P3, P_ENT, P_ENT2, P_QUI, P_SUB, P_CIE, NUM_PAT };
const char *BAT[NUM_PAT][7] = {
  { "X---X---X---X---", "x.X.x.X.x.X.x.X.", "X--x--x---X--x--", "-------x-------x", "----x-------x---", "--x---x---x---x-", "----------------" },
  { "X---X---X---X---", "xxXxxxXxxxXxxxXx", "X--x--x---X-xx-x", "-------x-----x-x", "----X-------X---", "--x---x---x---x-", "----------------" },
  { "X---X---X--xX---", ".x.x.x.x.x.x.x.x", "--x--x--x--x--x-", "x-------x-------", "------------X---", "--X---X---X---X-", "-------------x.x" },
  { "X--xX---X--xX---", "x.xXx.xXx.xXx.xX", "----X-------X---", "--x---x---x---x-", "----X-------X---", "----------------", "---------x--x--x" },
  { "X---X---X---X---", "x...x...x...x...", "----------------", "----------------", "----------------", "----------------", "----------------" },
  { "X---X---X---X---", "x.x.x.x.x.x.x.x.", "X--x--x---X--x--", "----------------", "----------------", "----------------", "----------------" },
  { "----------------", ".x.x.x.x.x.x.x.x", "----------------", "x-------x-------", "----------------", "----------------", "----------------" },
  { "X-------X-------", ".x.x.x.x.x.x.x.x", "----x---x-x-xxxx", "x-------x-------", "----------------", "----------------", "----------------" },
  { "X---------------", "----------------", "X---------------", "X---------------", "----------------", "----------------", "----------------" },
};
const char *FILL[7] = { "X-xX", "----", "xxXX", "x-x-", "----", "----", "x.x-" };      // pasos 13–16: REEMPLAZA a la batería
// BAJO — x fundamental · 2 la b2 · 5 la quinta. Nunca cae en una negra. A suena tres compases y B en el cuarto.
const char *BAJO[4][2] = {
  { "--x---x---x---x-", "--x---x---x---2-" }, { "--xx--x---xx--x-", "--xx--x---xx--2-" },
  { "--xx--xx--xx--xx", "--xx--xx--xx--25" }, { "-xxx-xxx-xxx-xxx", "-xxx-xxx-xxx-x25" } };

// TIMBRES DE LA MELODÍA: tipo 0 dos sierras · 1 FM · 2 quintas
#define NUM_TMEL 4
const TimbreMel TMEL[NUM_TMEL] = {
  //tipo det      ruido  pre    banda  corte oct   q      tauF   nivel  tau
  { 0, 1.0175f, 0.30f, 0.6f, true,  1.2f, 2.4f, 5.0f,  0.06f, 2.50f, 0.17f },   // STAB: nota de ruido pasa-banda
  { 0, 1.0058f, 0.12f, 0.5f, false, 1.6f, 3.0f, 1.33f, 0.10f, 0.60f, 0.22f },   // ACERO
  { 1, 2.0f,    0.10f, 0.5f, false, 5.0f, 1.2f, 1.15f, 0.08f, 0.46f, 0.20f },   // FM
  { 2, 1.5f,    0.05f, 0.9f, false, 5.0f, 1.6f, 1.12f, 0.09f, 0.50f, 0.24f },   // QUINTAS
};

// LA FORMA — energía e0→e1 (0..1), timbre de la melodía, dron en los quiebres, golpe al entrar.
// EL CHART — un string de 16 por compás: 1..5 = carril · = la nota sigue sostenida · - nada. `doble` = las notas
// dobles del mismo compás, que sólo entran en DURO. FÁCIL deja las negras y el «2» del 3+3+2.
#define NUM_SEC 10
const Seccion SECCIONES[NUM_SEC] = {
  { { P_ENT, P_ENT2 }, 2, -1, true, 0.25f, 0.5f, 0, false, false, false, false, 2,
    { "----------------", "----------------" }, {} },
  { { P0 }, 1, -1, false, 0.55f, 0.55f, 0, false, false, false, false, 4,
    { "1-----1---------", "1-----1---2-----", "1-----1---------", "1-----2---1---4-" }, {} },
  { { P0 }, 1, 0, true, 0.6f, 0.7f, 0, false, false, false, false, 4,
    { "1--1--2-1-------", "1--1--2-1-1--24-", "1--1--2-1-------", "1--1--2-1-1--24-" }, {} },
  { { P1 }, 1, 1, true, 0.75f, 0.9f, 1, false, false, false, false, 8,
    { "1--1--2-1-3--2--", "1--1--2-1-1--24-", "1--1--2-3-2--1--", "4--4--3-2-1-----",
      "1--1--2-1-3--2--", "1--1--2-1-1--24-", "5--4--3-2-3--2--", "1-1-2-2-3-3-4---" },
    { "4---------------", 0, 0, 0, "4---------------" } },
  { { P_QUI }, 1, -1, false, 0.2f, 0.2f, 1, true, false, false, false, 4,
    { "1=======2=======", "1=======3=======", "2=======1=======", "4===========2-1-" }, {} },
  { { P2 }, 1, 2, true, 0.8f, 0.9f, 0, false, false, true, false, 8,
    { "1-1-1-2-1-1-1-4-", "1-1-1-2-1-1-2-2-", "1-1-1-2-1-1-1-4-", "3-3-2-2-1-1-2-4-",
      "1-1-1-2-1-1-1-4-", "1-1-1-2-3-3-2-2-", "4-4-3-3-2-2-1-1-", "1-2-3-4-5-4-3-2-" }, {} },
  { { P3 }, 1, 3, true, 0.95f, 0.95f, 2, false, false, false, true, 8,
    { "1--21--21--21--4", "1--21--21--2412-", "1--21--21--21--4", "4--34--34--3421-",
      "1--21--21--21--5", "1--21--21--2412-", "4--54--54--54--3", "4--34--32--21---" },
    { 0, 0, 0, 0, "4---------------" } },
  { { P_QUI, P_QUI, P_SUB, P_SUB }, 4, -1, true, 0.15f, 0.6f, 1, true, true, false, false, 4,
    { "1=======--------", "2=======--------", "3=======4=======", "5===========----" }, {} },
  { { P1 }, 1, 3, true, 1.0f, 1.0f, 3, false, false, true, false, 12,
    { "1--1--2-1-3--24-", "1--1--2-1-1--24-", "5--4--3-2-3--2--", "1-1-2-2-3-3-4-4-",
      "1--1--2-1-3--24-", "1-1-1-2-1-1-2-4-", "5--4--3-2-3--2--", "1-2-3-4-5-4-3-2-",
      "1--1--2-1-3--24-", "1--21--21--2412-", "5--4--3-2-3--2--", "1-1-2-2-3-4-5-5-" },
    { "4---------------", 0, "2---------------", 0, "4---------------", 0, "2---------------", 0,
      "4--4--5-4-------", 0, "2--1------------", 0 } },
  { { P_CIE }, 1, -1, false, 1.0f, 1.0f, 3, false, false, true, false, 1,
    { "1===============" }, { "5===============" } },
};

// EL FONDO de cada sección, por bloques de 4 compases: A hiperespacio · B planetas · C túnel (se pueden sumar)
const char *FONDOS[NUM_SEC][3] = { { "A" }, { "A" }, { "C" }, { "B", "A" }, { "B" }, { "C", "A" }, { "AB", "C" }, { "B", "C" }, { "AC", "AB", "ABC" }, { "ABC" } };

// ==============================================================================================================================================
// ESTADO
// ==============================================================================================================================================
enum { E_MENU, E_JUEGO, E_PERDIO, E_FIN };
#define MAX_NOTAS   400
#define MAX_COMPAS  64
#define NUM_VOCES   6

i2s_chan_handle_t tx_chan;
CRGB leds[NUM_LEDS];

// — del audio (core 1): nadie más los escribe —
Nota    notas[MAX_NOTAS]; int nNotas = 0, notaProg = 0, primPend = 0;
Compas  pista[MAX_COMPAS]; int nCompas = 0; int32_t totalPasos = 0;
double  muestrasPorPaso = 0, sigPaso = 0; int32_t pasoIdx = 0, mCancion = 0, mFin = 0, sigPasoM = 0, sigNotaM = 0x7FFFFFFF;
bool    sonando = false, puerta = true;
float   vida = VIDA_INICIAL, puntos = 0; int racha = 0, rachaMax = 0, nPerf = 0, nBien = 0, nFalla = 0;
float   E = 0.5f, eSuave = 0.5f, masterObj = 1.0f, master = 1.0f;
int     difActual = 1; int pasoHat = 0;
uint32_t semilla = 22222;

// — publicados para el core 0 (lectura) —
volatile int      estadoPub = E_MENU;
volatile int32_t  pubMuestra = 0; volatile uint32_t pubMicros = 0;
volatile int      vidaPub = 70, puntosPub = 0, rachaPub = 0, multPub = 1, nPerfPub = 0, nBienPub = 0, nTotalPub = 0, avancePub = 0;
volatile int      juicioPub = 0; volatile uint32_t juicioMs = 0;       // 1 perfecto · 2 bien · 3 falla
volatile uint32_t aciertoMs[5] = { 0, 0, 0, 0, 0 }, bomboMs = 0, fallaMs = 0, martilloMs = 0;
volatile bool     cargaAlta = false;
volatile uint32_t medSuma = 0, medMax = 0, medN = 0; volatile float medLim = 1.0f;

// — del core 0 hacia el audio —
volatile bool     abajo[5] = { false, false, false, false, false };
Evento            cola[16]; volatile uint8_t colaW = 0, colaR = 0;
volatile int      reqEmpezar = -1;        // dificultad con la que empezar, o -1
volatile bool     reqMenu = false;
volatile int      difMenu = 1;

// ==============================================================================================================================================
// DSP
// ==============================================================================================================================================
#define TABLA 1024
float SENO[TABLA + 1];
const float DITHER = 1e-18f;              // anti-subnormales, a la entrada de cada filtro
static inline float seno(float fase) { float x = fase * TABLA; int i = (int)x; float f = x - i; return SENO[i] + (SENO[i + 1] - SENO[i]) * f; }
static inline float ruido() { semilla = semilla * 1664525u + 1013904223u; return (int32_t)semilla * (1.0f / 2147483648.0f); }
static inline float tanRapido(float x) { float x2 = x * x; return x * (15.0f - x2) / (15.0f - 6.0f * x2); }       // Padé
static inline float exp2Rapido(float x) { int n = (int)x; float f = x - n; float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * 0.0555041f)); return p * (float)(1 << n); }   // 2^x, x ≥ 0
static inline float decae(float tau) { return expf(-1.0f / (tau * SR)); }
static inline float decaeR(float tau) { return 1.0f - 1.0f / (tau * SR); }      // lo mismo sin expf (tau·SR ≫ 1): para lo que se calcula en cada nota
static inline float recorta(float x) { return x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x); }
static inline float satura(float x) { x *= 0.6f; if (x > 1.5f) return 1.0f; if (x < -1.5f) return -1.0f; return x - 0.1481481f * x * x * x; }   // cúbica, sin división
static inline float pasoBQ(BQ &q, float x) { x += DITHER; float y = q.b0 * x + q.z1; q.z1 = q.b1 * x - q.a1 * y + q.z2; q.z2 = q.b2 * x - q.a2 * y; return y; }
void bqBPF(BQ &c, float fc, float Q) {     // pasa-banda de pico 0 dB
  float w = 2.0f * PI * fc / SR, al = sinf(w) / (2.0f * Q), a0 = 1.0f + al;
  c.b0 = al / a0; c.b1 = 0; c.b2 = -al / a0; c.a1 = -2.0f * cosf(w) / a0; c.a2 = (1.0f - al) / a0;
}
static inline void svfCoef(SVF &s, float fc, float Q) {
  if (fc > 17000.0f) fc = 17000.0f;
  s.g = tanRapido(PI * fc / SR); s.k = 1.0f / Q;
  s.a1 = 1.0f / (1.0f + s.g * (s.g + s.k)); s.a2 = s.g * s.a1; s.a3 = s.g * s.a2;
}
static inline void svfPaso(SVF &s, float x, float &lp, float &bp) {   // TPT (Zavalishin)
  x += DITHER;
  float v3 = x - s.s2, v1 = s.a1 * s.s1 + s.a2 * v3, v2 = s.s2 + s.a2 * s.s1 + s.a3 * v3;
  s.s1 = 2.0f * v1 - s.s1; s.s2 = 2.0f * v2 - s.s2; lp = v2; bp = v1;
}
static inline float sierra(float &f, float inc) {      // PolyBLEP
  f += inc; if (f >= 1.0f) f -= 1.0f;
  float y = 2.0f * f - 1.0f;
  if (f < inc) { float t = f / inc; y -= t + t - t * t - 1.0f; }
  else if (f > 1.0f - inc) { float t = (f - 1.0f) / inc; y -= t * t + t + t + 1.0f; }
  return y;
}
float frecGrado(int grado, float base) { int i = grado - 1, o = i / 7; return base * powf(2.0f, (ESCALA[i % 7] + 12 * o) / 12.0f); }

// ── Percusión ──
enum { G_BOMBO, G_HAT, G_MARTILLO, G_PISTON, G_CLAP, G_ABIERTO, G_TOM, G_PLATO, G_GRANDE, G_SECO, NUM_GOLPES };
Golpe golpes[NUM_GOLPES];
// f0→f1 con tauF · niveles de las dos parciales y sus taus · ruido fc0→fc1, Q, nivel y tau · pan · bus
void golpear(int cual, float f0, float f1, float tauF, float r2, float a1, float tau1, float a2, float tau2,
             float fc0, float fc1, float q, float aN, float tauN, float pan, uint8_t bus) {
  Golpe &g = golpes[cual];
  // Los coeficientes (exponenciales, senos del filtro y del paneo) dependen sólo del instrumento, no de la
  // fuerza del golpe: se calculan la primera vez y se guardan. Calcularlos en cada disparo costaba ~40 % de
  // un bloque en los pasos con cinco golpes a la vez (expf/powf/sinf son caros en el S3).
  float llave = tauF + tau1 * 3.0f + tau2 * 7.0f + tauN * 11.0f + fc0 * 0.001f + fc1 * 0.0007f + q + pan * 5.0f + f1;
  if (llave != g.llave) {
    g.llave = llave;
    g.kF = decae(tauF > 0 ? tauF : 0.01f); g.k1 = decae(tau1); g.k2 = decae(tau2 > 0 ? tau2 : tau1); g.kN = decae(tauN > 0 ? tauN : 0.01f);
    g.kFc = (fc0 != fc1) ? powf(decae(0.10f), 32.0f) : 1.0f;
    bqBPF(g.bp0, fc0, q); g.bp0.z1 = g.bp0.z2 = 0;
    float p = 0.5f + 0.5f * pan; g.panL = cosf(p * 1.5708f) * 1.4142f; g.panR = sinf(p * 1.5708f) * 1.4142f;
  }
  g.f = f0; g.fFin = f1; g.r2 = r2;
  g.pico1 = a1; g.pico2 = a2; g.picoN = aN; g.a1 = a1; g.a2 = a2;
  g.atk = 66;                                           // 1.5 ms de subida DESDE donde esté la envolvente (sin clic al redisparar)
  g.fc = fc0; g.fcFin = fc1; g.q = q; g.cuentaFc = 0;
  if (aN > 0) { float z1 = g.bp.z1, z2 = g.bp.z2; g.bp = g.bp0; g.bp.z1 = z1; g.bp.z2 = z2; }
  g.bus = bus; g.activo = true;
}
void golpeBombo(float v) {                              // TRES bandas: fundamental, 2.º armónico y mazo de RUIDO
  golpear(G_BOMBO, 48 * 4.5f, 48, 0.016f, 2.0f, 1.0f * v, 0.16f, 0.45f * v, 0.09f, 2200, 2200, 0.8f, 0.5f * v, 0.010f, 0, 0);
  bomboMs = millis();
}
void golpeHat(float v)      { golpear(G_HAT, 0, 0, 0, 0, 0, 0.01f, 0, 0, 8500, 8500, 1.0f, 0.36f * v * (0.45f + 0.75f * E), 0.028f, (pasoHat++ & 1) ? 0.4f : -0.4f, 1); }
void golpeMartillo(float v) { martilloMs = millis(); golpear(G_MARTILLO, 115 * 1.6f, 115, 0.015f, 2.0f, 0.50f * v, 0.045f, 0.20f * v, 0.040f, 1700, 1700, 0.9f, 0.75f * v, 0.055f, -0.3f, 1); }
void golpePiston(float v)   { golpear(G_PISTON, 70 * 2.1f, 70, 0.06f, 0, 0.50f * v, 0.12f, 0, 0, 3000, 300, 1.2f, 0.60f * v, 0.16f, 0.3f, 1); }
void golpeClap(float v)     { golpear(G_CLAP, 0, 0, 0, 0, 0, 0.01f, 0, 0, 1250, 1250, 1.3f, 0.60f * v, 0.07f, 0, 1); }
void golpeAbierto(float v)  { golpear(G_ABIERTO, 0, 0, 0, 0, 0, 0.01f, 0, 0, 7000, 7000, 0.5f, 0.26f * v * (0.5f + 0.6f * E), 0.12f, -0.5f, 1); }
void golpeTom(float v)      { golpear(G_TOM, 100 * 1.4f, 100, 0.03f, 0, 0.48f * v, 0.14f, 0, 0, 900, 900, 0.8f, 0.2f * v, 0.012f, 0.2f, 1); }
void golpePlato()           { golpear(G_PLATO, 0, 0, 0, 0, 0, 0.01f, 0, 0, 6000, 5200, 0.6f, 0.32f, 0.38f, 0, 1); }
void golpeGrande()          { golpear(G_GRANDE, 98, 30, 0.20f, 0, 0.9f, 0.45f, 0, 0, 600, 180, 0.7f, 0.55f, 0.45f, 0, 2); }
void golpeSeco()            { golpear(G_SECO, 0, 0, 0, 0, 0, 0.01f, 0, 0, 700, 700, 1.5f, 0.35f, 0.02f, 0, 3); }

// ── Bajo: UNA voz que nunca se apaga (sierra + cuadrada en la misma octava → saturación → pasa-bajos) ──
float bjFase = 0, bjFase2 = 0.5f, bjF = RE2, bjAmp = 0, bjFenv = 0, bjKdec = 0.999f, bjPre = 0.7f, bjBase = 200, bjPico = 800, bjQ = 2;
int   bjT = 100000; SVF bjFl;
void notaBajo(float f) {
  bjF = f; bjT = 0; bjPre = 0.5f + 0.4f * E; bjQ = 1.5f + 1.5f * E;
  bjBase = f * (2.0f + 3.0f * E); bjPico = bjBase * exp2Rapido(1.5f + 1.2f * E); bjFenv = 1.0f;
  bjKdec = decaeR(0.055f + 0.03f * E);
}

// ── Melodía del jugador ──
Voz voces[NUM_VOCES];
int dispararNota(int idx) {
  Nota &n = notas[idx];
  for (int i = 0; i < NUM_VOCES; i++) {                  // la melodía es una línea: la nota nueva despide a las anteriores (30 ms),
    Voz &o = voces[i];                                    // salvo las sostenidas y las que entran junto con ella (las dobles)
    if (o.activo && !o.sost && !o.soltando && notas[o.nota].t < n.t) soltarVoz(i, 0.030f);
  }
  int libre = -1; for (int i = 0; i < NUM_VOCES; i++) if (!voces[i].activo) { libre = i; break; }
  if (libre < 0) return -1;                             // sin robar: una voz robada es un clic
  Voz &v = voces[libre]; const TimbreMel &T = TMEL[n.tm];
  v.activo = true; v.sost = n.dur > 0; v.soltando = false; v.nota = idx; v.tm = n.tm; v.f = n.f; v.vel = n.vel;
  v.fase[0] = 0; v.fase[1] = 0.37f; v.fase[2] = 0.71f; v.faseM = 0;
  v.amp = 0; v.pico = T.nivel * (0.55f + 0.45f * n.vel); v.atk = 88;
  v.objetivo = v.sost ? v.pico * 0.4f : 0.0f; v.kAmp = decaeR(v.sost ? 0.10f : T.tau);
  v.fenv = 1.0f; v.kFenv = 1.0f - 16.0f / (T.tauF * (v.sost ? 2.5f : 1.0f) * SR);
  v.idx = 4.5f * n.vel; v.kIdx = decaeR(0.09f);
  v.fl.s1 = v.fl.s2 = 0;
  svfCoef(v.fl, fminf(n.f * T.corte * exp2Rapido(T.oct + (n.vel - 0.7f) * 2.2f), 12000.0f), T.q);
  n.voz = libre; return libre;
}
void soltarVoz(int i, float tau) { if (i < 0) return; Voz &v = voces[i]; if (!v.activo) return; v.soltando = true; v.sost = false; v.atk = 0; v.objetivo = 0; v.kAmp = decaeR(tau); }

// ── Dron, subida y eco ──
bool  drActivo = false, drSube = false; float drFase[3] = { 0, 0.3f, 0 }, drNivel = 0, drFc = 170, drKfc = 1; int32_t drT = 0, drDur = 0, drSubeM = 1; SVF drFl;
bool  suActivo = false; float suFc = 250, suKfc = 1, suNivel = 0; int32_t suT = 0, suDur = 0; BQ suBp;
#define ECO_MAX 16384
int16_t ecoI[ECO_MAX], ecoD[ECO_MAX]; int ecoPos = 0, ecoLargo = 15504; float ecoLpI = 0, ecoLpD = 0;

// ── Buses y master ──
SVF   restoL, restoR; float fRestoHz = 3000, nivelResto = 0.8f, nivelBombo = 0.8f, agacha = 1.0f, agachaObj = 1.0f; int agachaT = 100000;
#define LOOK 64
float lookL[LOOK], lookR[LOOK]; int lookPos = 0; float limPico = 0, limGan = 1.0f, dcL = 0, dcR = 0, dcXL = 0, dcXR = 0;
int16_t salida[BUFFER_SAMPLES * 2];

// ==============================================================================================================================================
// SECUENCIADOR
// ==============================================================================================================================================
static inline float velDe(char c) { return c == 'X' ? 1.0f : c == 'x' ? 0.62f : c == '.' ? 0.24f : 0.0f; }
void tocarPaso(int32_t p) {
  const Compas &c = pista[p / 16]; int s = p % 16; float v;
  E = c.e;
  if (s == 0) {
    if (c.dron)   { drActivo = true; drT = 0; drDur = c.dron; drSube = c.sube; drFc = 170; drKfc = powf((c.sube ? 2600.0f : 480.0f) / 170.0f, 16.0f / c.dron); drSubeM = (int32_t)fminf(1.5f * SR, c.dron / 3.0f); drFl.s1 = drFl.s2 = 0; }
    if (c.subida) { suActivo = true; suT = 0; suDur = c.subida; suFc = 250; suKfc = powf(6500.0f / 250.0f, 32.0f / c.subida); }
    if (c.golpe)  golpeGrande();
    if (c.plato)  golpePlato();
  }
  bool fill = c.fill && s >= 12;
  #define CAR(i) (fill ? FILL[i][s - 12] : BAT[c.bat][i][s])
  if ((v = velDe(CAR(0))) > 0) { golpeBombo(v); agachaObj = 0.74f - 0.34f * E; agachaT = 0; }
  if ((v = velDe(CAR(1))) > 0) golpeHat(v);
  if ((v = velDe(CAR(2))) > 0) golpeMartillo(v);
  if ((v = velDe(CAR(3))) > 0) golpePiston(v);
  if ((v = velDe(CAR(4))) > 0) golpeClap(v);
  if ((v = velDe(CAR(5))) > 0) golpeAbierto(v);
  if ((v = velDe(CAR(6))) > 0) golpeTom(v);
  #undef CAR
  if (c.bajo >= 0 && !fill) {
    char b = BAJO[c.bajo >> 1][c.bajo & 1][s];
    if (b == 'x') notaBajo(RE2); else if (b == '2') notaBajo(RE2 * 1.059463f); else if (b == '5') notaBajo(RE2 * 1.498307f);
  }
}

// Arma la pista y las notas del jugador según la dificultad.
void construir(int dif) {
  nNotas = 0; nCompas = 0; muestrasPorPaso = (double)SR * 60.0 / BPM / 4.0;
  for (int si = 0; si < NUM_SEC; si++) {
    const Seccion &s = SECCIONES[si];
    for (int i = 0; i < s.n; i++) {
      Compas &c = pista[nCompas];
      c.bat = s.bat[i < s.nBat ? i : s.nBat - 1];
      c.bajo = s.bajo < 0 ? -1 : (int8_t)(s.bajo * 2 + ((i % 4 == 3) ? 1 : 0));
      c.fill = s.fill && i == s.n - 1;
      c.plato = i == 0 && si > 0 && SECCIONES[si - 1].fill;
      c.golpe = s.golpe && i == 0; c.sube = s.sube;
      c.dron = (s.dron && i == 0) ? (int32_t)(s.n * 16 * muestrasPorPaso) : 0;
      c.subida = (s.sube && i == 0) ? (int32_t)(s.n * 16 * muestrasPorPaso) : 0;
      c.e = s.e0 + (s.e1 - s.e0) * i / (float)(s.n > 1 ? s.n - 1 : 1);
      { int b = i / 4; while (b > 0 && !FONDOS[si][b]) b--; c.fondo = 0; c.dens = 0;
        for (const char *f = FONDOS[si][b]; *f; f++) c.fondo |= 1 << (*f - 'A'); }
      for (int li = 0; li < 2; li++) {
        const char *ln = li == 0 ? s.comp[i] : s.doble[i];
        if (!ln || (li > 0 && dif < 2)) continue;                    // las dobles sólo en DURO
        for (int p = 0; p < 16; p++) {
          char ch = ln[p]; if (ch < '1' || ch > '5') continue;
          int largo = 0; while (p + 1 + largo < 16 && ln[p + 1 + largo] == '=') largo++;
          if (dif == 0 && !(p % 4 == 0 || p % 8 == 6)) continue;     // FÁCIL: negras y el «2» del 3+3+2
          if (nNotas >= MAX_NOTAS) continue;
          Nota &n = notas[nNotas++]; int carril = ch - '1';
          n.t = (int32_t)((nCompas * 16 + p) * muestrasPorPaso);
          n.dur = largo ? (int32_t)((largo + 0.5f) * muestrasPorPaso) : 0;
          n.f = frecGrado((s.alto ? CARRILES_ALTO : CARRILES_BASE)[carril], RE4);
          n.vel = (p == 0 || li > 0) ? 1.0f : (p % 4 == 0 ? 0.86f : 0.70f);     // acentos: el «1» y las dobles
          n.carril = carril; n.tm = s.tm; n.estado = 0; n.sost = false; n.voz = -1;
        }
      }
      nCompas++;
    }
  }
  for (int i = 1; i < nNotas; i++) {                    // orden por tiempo (las dobles llegan desordenadas)
    Nota x = notas[i]; int j = i - 1;
    while (j >= 0 && notas[j].t > x.t) { notas[j + 1] = notas[j]; j--; }
    notas[j + 1] = x;
  }
  totalPasos = nCompas * 16;
  for (int i = 0; i < nNotas; i++) {                    // densidad de notas de cada compás (0..1): con ella se llena el fondo
    Compas &c = pista[(int)(notas[i].t / (16 * muestrasPorPaso) + 1e-4)]; c.dens = fminf(1.0f, c.dens + 1.0f / 9.0f);
  }
}

void empezar(int dif) {
  difActual = dif; construir(dif);
  for (int i = 0; i < NUM_VOCES; i++) voces[i].activo = false;
  for (int i = 0; i < NUM_GOLPES; i++) golpes[i].activo = false;
  drActivo = suActivo = false; bjAmp = 0; bjT = 100000;
  memset(ecoI, 0, sizeof(ecoI)); memset(ecoD, 0, sizeof(ecoD));
  ecoLargo = (int)(3 * muestrasPorPaso); if (ecoLargo > ECO_MAX) ecoLargo = ECO_MAX;
  vida = VIDA_INICIAL; puntos = 0; racha = rachaMax = nPerf = nBien = nFalla = 0; puerta = true;
  notaProg = primPend = 0; pasoIdx = 0; sigPaso = 0; sigPasoM = 0; sigNotaM = nNotas ? notas[0].t : 0x7FFFFFFF;
  mCancion = -(int32_t)(0.8f * SR);                    // un respiro antes del primer bombo
  mFin = (int32_t)(totalPasos * muestrasPorPaso + 1.2 * SR);
  colaR = colaW; juicioPub = 0; master = masterObj = 1.0f; E = eSuave = 0.3f;
  nTotalPub = nNotas; sonando = true; estadoPub = E_JUEGO;
}

// ==============================================================================================================================================
// EL JUEGO (corre en la tarea de audio, al principio de cada bloque: un solo dueño del estado)
// ==============================================================================================================================================
static inline int multiplicador() { int m = 1 + racha / 10; return m > 4 ? 4 : m; }
void danar(float cuanto) {
  vida -= cuanto;
  if (vida <= 0 && estadoPub == E_JUEGO) {              // sin energía: la música se cierra y se apaga
    vida = 0; estadoPub = E_PERDIO; masterObj = 0.0f; mFin = mCancion + (int32_t)(1.6f * SR);
    for (int i = 0; i < NUM_VOCES; i++) soltarVoz(i, 0.05f);
  }
}
void fallar(Nota &n) {
  n.estado = 2; racha = 0; nFalla++; puerta = false; juicioPub = 3; juicioMs = millis(); fallaMs = millis();
  if (n.voz >= 0 && voces[n.voz].activo && voces[n.voz].nota == (int)(&n - notas)) soltarVoz(n.voz, 0.004f);
  danar(DIF_GOLPE[difActual]);
}
void acertar(int idx, int32_t d, int32_t mOido) {
  Nota &n = notas[idx]; bool perfecto = abs(d) <= (int32_t)(V_PERFECTO * SR);
  n.estado = 1; racha++; if (racha > rachaMax) rachaMax = racha;
  if (perfecto) nPerf++; else nBien++;
  puntos += (perfecto ? 100 : 60) * multiplicador();
  vida = fminf(1.0f, vida + (perfecto ? VIDA_PERFECTO : VIDA_BIEN) + VIDA_POR_MULT * (multiplicador() - 1));
  juicioPub = perfecto ? 1 : 2; juicioMs = millis(); aciertoMs[n.carril] = millis();
  if (n.voz == -1 && n.t <= mCancion) dispararNota(idx);  // llegó tarde y no estaba sonando: suena ya
  if (n.dur > 0) n.sost = true;
  puerta = true;                                         // si llegó antes, la nota la dispara el reloj en su paso
}
void logicaJuego() {
  if (estadoPub != E_JUEGO) { colaR = colaW; return; }
  int32_t mOido = mCancion - (int32_t)(LATENCIA_MS * 0.001f * SR), vBien = (int32_t)(V_BIEN * SR);
  while (colaR != colaW) {                               // toques que dejó el core 0, con su hora
    Evento ev = cola[colaR]; colaR = (colaR + 1) & 15;
    if (ev.abajo) {
      int hallada = -1;
      for (int i = primPend; i < nNotas && notas[i].t <= ev.t + vBien; i++)
        if (notas[i].estado == 0 && notas[i].carril == ev.carril && abs(notas[i].t - ev.t) <= vBien) { hallada = i; break; }
      if (hallada >= 0) acertar(hallada, ev.t - notas[hallada].t, mOido);
      else { golpeSeco(); if (ev.t > 0) { racha = 0; danar(VIDA_EN_FALSO); } }
    } else {
      for (int i = 0; i < NUM_VOCES; i++) {
        Voz &v = voces[i];
        if (v.activo && v.sost && notas[v.nota].carril == ev.carril && notas[v.nota].sost) { notas[v.nota].sost = false; soltarVoz(i, 0.05f); }
      }
    }
  }
#if MEDIR
  vida = 1.0f; for (int i = primPend; i < nNotas && notas[i].t <= mOido; i++) if (notas[i].estado == 0) acertar(i, 0, mOido);
#endif
  while (primPend < nNotas && notas[primPend].estado != 0) primPend++;
  for (int i = primPend; i < nNotas && notas[i].t < mOido - vBien; i++) if (notas[i].estado == 0) fallar(notas[i]);
  for (int i = 0; i < NUM_VOCES; i++) {                  // las sostenidas: suman mientras se aguantan y terminan solas
    Voz &v = voces[i]; if (!v.activo || !v.sost) continue; Nota &n = notas[v.nota];
    if (n.sost) puntos += (BUFFER_SAMPLES / (float)muestrasPorPaso) * 10.0f * multiplicador();
    if (mOido >= n.t + n.dur) { n.sost = false; soltarVoz(i, 0.05f); }
  }
  if (estadoPub == E_JUEGO && mCancion > mFin) { estadoPub = E_FIN; sonando = false; }
  vidaPub = (int)(vida * 100.0f + 0.5f); puntosPub = (int)puntos; rachaPub = racha; multPub = multiplicador();
  nPerfPub = nPerf; nBienPub = nBien; avancePub = (int)(100.0f * fmaxf(0, (float)mCancion) / (float)(totalPasos * muestrasPorPaso));
}

// ==============================================================================================================================================
// RENDER (core 1)
// ==============================================================================================================================================
void renderBuffer() {
  uint32_t ciclos0 = ESP.getCycleCount();
  if (reqMenu) { reqMenu = false; sonando = false; estadoPub = E_MENU; for (int i = 0; i < NUM_VOCES; i++) voces[i].activo = false; }
  if (reqEmpezar >= 0) { int d = reqEmpezar; reqEmpezar = -1; empezar(d); }
  logicaJuego();
  if (estadoPub == E_PERDIO && mCancion > mFin) sonando = false;

  // por bloque: la energía de la sección mueve el filtro y los niveles de la base
  eSuave += (E - eSuave) * 0.005f;
  fRestoHz = 700.0f * exp2Rapido(eSuave * 4.7f); nivelResto = 0.42f + 0.72f * eSuave; nivelBombo = 0.34f + 0.50f * eSuave;
  svfCoef(restoL, fRestoHz, 0.8f); restoR.g = restoL.g; restoR.k = restoL.k; restoR.a1 = restoL.a1; restoR.a2 = restoL.a2; restoR.a3 = restoL.a3;
  master += (masterObj - master) * 0.008f;

  for (int i = 0; i < BUFFER_SAMPLES; i++) {
    if (sonando) {
      // (comparaciones ENTERAS: el S3 no tiene FPU de doble precisión y un double por muestra costaba un tercio del bloque)
      while (pasoIdx < totalPasos && mCancion >= sigPasoM) { tocarPaso(pasoIdx); pasoIdx++; sigPaso += muestrasPorPaso; sigPasoM = (int32_t)sigPaso; }
      while (mCancion >= sigNotaM && notaProg < nNotas) {       // la melodía entra en SU muestra, sobre la grilla
        Nota &n = notas[notaProg];
        if (puerta && n.estado != 2 && n.voz == -1 && estadoPub == E_JUEGO) dispararNota(notaProg);
        notaProg++; sigNotaM = notaProg < nNotas ? notas[notaProg].t : 0x7FFFFFFF;
      }
      mCancion++;
    }
    float bom = 0, rL = 0, rR = 0, dL = 0, dR = 0, mel = 0;

    // ── percusión ──
    for (int gi = 0; gi < NUM_GOLPES; gi++) {
      Golpe &g = golpes[gi]; if (!g.activo) continue;
      if (g.atk > 0) { float k = 1.0f / g.atk; g.e1 += (g.pico1 - g.e1) * k; g.e2 += (g.pico2 - g.e2) * k; g.eN += (g.picoN - g.eN) * k; g.atk--; }
      else { g.e1 *= g.k1; g.e2 *= g.k2; g.eN *= g.kN; }
      float s = 0;
      if (g.pico1 > 0) {
        g.f = g.fFin + (g.f - g.fFin) * g.kF;
        g.fase1 += g.f * (1.0f / SR); if (g.fase1 >= 1.0f) g.fase1 -= 1.0f;
        s = seno(g.fase1) * g.e1;
        if (g.pico2 > 0) { g.fase2 += g.f * g.r2 * (1.0f / SR); if (g.fase2 >= 1.0f) g.fase2 -= 1.0f; s += seno(g.fase2) * g.e2; }
      }
      if (g.picoN > 0) {
        if (g.kFc != 1.0f && ++g.cuentaFc >= 32) { g.cuentaFc = 0; g.fc = g.fcFin + (g.fc - g.fcFin) * g.kFc; bqBPF(g.bp, g.fc, g.q); }
        s += pasoBQ(g.bp, ruido()) * g.eN;
      }
      if (g.atk == 0 && g.e1 < 3e-5f && g.e2 < 3e-5f && g.eN < 3e-5f) { g.activo = false; g.e1 = g.e2 = g.eN = 0; g.bp.z1 = g.bp.z2 = 0; }
      if (g.bus == 0) bom += s; else if (g.bus == 1) { rL += s * g.panL; rR += s * g.panR; } else if (g.bus == 2) { dL += s; dR += s; } else mel += s;
    }
    bom = satura(bom * 2.4f);                             // el bombo tiene UNA parcial fuerte: se puede saturar

    // ── bajo ──
    {
      float inc = bjF * (1.0f / SR);
      float sa = sierra(bjFase, inc), sb = sierra(bjFase2, inc);
      float x = satura((sa + (sa - sb) * 0.40f) * bjPre * 2.4f);
      if (bjT < 265) bjAmp += (0.46f - bjAmp) * 0.02f; else bjAmp *= bjKdec;
      if (bjT < 100000) bjT++;
      if ((i & 15) == 0) { bjFenv *= 0.9920f; svfCoef(bjFl, bjBase + (bjPico - bjBase) * bjFenv, bjQ); }
      float lp, bp; svfPaso(bjFl, x, lp, bp);
      lp *= bjAmp; rL += lp; rR += lp;
    }

    // ── melodía del jugador ──
    for (int vi = 0; vi < NUM_VOCES; vi++) {
      Voz &v = voces[vi]; if (!v.activo) continue; const TimbreMel &T = TMEL[v.tm];
      float inc = v.f * (1.0f / SR), s;
      if (T.tipo == 1) {                                  // FM: el índice cae con la nota
        v.faseM += inc * T.det; if (v.faseM >= 1.0f) v.faseM -= 1.0f;
        v.idx = 1.1f + (v.idx - 1.1f) * v.kIdx;
        v.fase[0] += inc; if (v.fase[0] >= 1.0f) v.fase[0] -= 1.0f;
        float ph = v.fase[0] + seno(v.faseM) * v.idx * 0.0796f + 1.0f; ph -= (int)ph;
        s = seno(ph) + sierra(v.fase[1], inc * 0.5f) * 0.35f;
      } else if (T.tipo == 2) s = sierra(v.fase[0], inc) * 0.6f + sierra(v.fase[1], inc * T.det) * 0.5f + sierra(v.fase[2], inc * 0.502f) * 0.5f;
      else s = (sierra(v.fase[0], inc) + sierra(v.fase[1], inc * T.det)) * 0.6f;
      s = satura((s + ruido() * T.ruido) * T.pre * 2.4f);
      if ((i & 15) == 0) { v.fenv *= v.kFenv; svfCoef(v.fl, fminf(v.f * T.corte * exp2Rapido((T.oct + (v.vel - 0.7f) * 2.2f) * v.fenv), 12000.0f), T.q); }
      float lp, bp; svfPaso(v.fl, s, lp, bp);
      s = T.banda ? bp * v.fl.k : lp;                     // pasa-banda de pico 0 dB
      if (v.atk > 0) { v.amp += (v.pico - v.amp) / v.atk; v.atk--; } else v.amp = v.objetivo + (v.amp - v.objetivo) * v.kAmp;
      mel += s * v.amp;
      if (v.atk == 0 && v.objetivo == 0 && v.amp < 2e-4f) { v.activo = false; if (notas[v.nota].voz == vi) notas[v.nota].voz = -2; }
    }

    // ── dron y subida ──
    if (drActivo) {
      float s = sierra(drFase[0], RE2 * 0.9965f / SR) * 0.5f + sierra(drFase[1], RE2 * 1.0035f / SR) * 0.5f;
      drFase[2] += RE2 * 0.5f / SR; if (drFase[2] >= 1.0f) drFase[2] -= 1.0f; s += seno(drFase[2]) * 0.9f;
      if ((drT & 15) == 0) { drFc *= drKfc; svfCoef(drFl, drFc, 2.5f); }
      float lp, bp; svfPaso(drFl, s, lp, bp);
      float obj = drT < drDur - 2600 ? 0.13f * fminf(1.0f, (float)drT / drSubeM) : 0.0f;
      drNivel += (obj - drNivel) * 0.002f;
      dL += lp * drNivel; dR += lp * drNivel;
      if (++drT > drDur + 4000) { drActivo = false; drNivel = 0; }
    }
    if (suActivo) {
      if ((suT & 31) == 0) { suFc *= suKfc; bqBPF(suBp, suFc, 1.4f); }
      float n = pasoBQ(suBp, ruido()) * 0.34f * ((float)suT / suDur);
      dL += n; dR += n;
      if (++suT >= suDur) { suActivo = false; suBp.z1 = suBp.z2 = 0; }
    }

    // ── buses: el bombo agacha la base y un pasa-bajos la abre según la sección ──
    if (agachaT < 1764) { agacha += (agachaObj - agacha) * 0.0055f; agachaT++; } else agacha += (1.0f - agacha) * 0.00045f;
    float lp, bp;
    svfPaso(restoL, rL * agacha, lp, bp); rL = lp * nivelResto;
    svfPaso(restoR, rR * agacha, lp, bp); rR = lp * nivelResto;

    // ── eco ping-pong de la melodía, a corchea con puntillo ──
    float eI = ecoI[ecoPos] * (1.0f / 32768.0f), eD = ecoD[ecoPos] * (1.0f / 32768.0f);
    ecoLpI += (mel * 0.22f + eD * 0.42f - ecoLpI) * 0.31f; ecoLpD += (eI * 0.42f - ecoLpD) * 0.31f;
    ecoI[ecoPos] = (int16_t)(recorta(ecoLpI) * 32767.0f);
    ecoD[ecoPos] = (int16_t)(recorta(ecoLpD) * 32767.0f);
    if (++ecoPos >= ecoLargo) ecoPos = 0;

    float L = (bom * nivelBombo + rL + dL + mel * 0.42f + eI * 0.85f) * master * MEZCLA;
    float R = (bom * nivelBombo + rR + dR + mel * 0.42f + eD * 0.85f) * master * MEZCLA;

    // ── bloqueador de continua + limitador con anticipación ──
    float yL = L - dcXL + 0.9985f * dcL; dcXL = L; dcL = yL; float yR = R - dcXR + 0.9985f * dcR; dcXR = R; dcR = yR;
    float aL = fabsf(yL), aR = fabsf(yR), pico = aL > aR ? aL : aR;
    limPico = pico > limPico ? pico : limPico * 0.9995f;
    float obj = limPico > 0.93f ? 0.93f / limPico : 1.0f;
    limGan += (obj - limGan) * (obj < limGan ? 0.09f : 0.0015f);
    float oL = lookL[lookPos] * limGan * VOLUMEN, oR = lookR[lookPos] * limGan * VOLUMEN;
    lookL[lookPos] = yL; lookR[lookPos] = yR; if (++lookPos >= LOOK) lookPos = 0;
    if (oL > 1.0f) oL = 1.0f; else if (oL < -1.0f) oL = -1.0f;
    if (oR > 1.0f) oR = 1.0f; else if (oR < -1.0f) oR = -1.0f;
    salida[2 * i] = (int16_t)(oL * 32767.0f); salida[2 * i + 1] = (int16_t)(oR * 32767.0f);
  }
  pubMuestra = mCancion; pubMicros = micros();
  uint32_t ciclos = ESP.getCycleCount() - ciclos0;
  cargaAlta = ciclos > (uint32_t)(0.8f * 240000000.0f * BUFFER_SAMPLES / SR);
#if MEDIR
  medSuma += ciclos / 64; if (ciclos > medMax) medMax = ciclos; medN = medN + 1; if (limGan < medLim) medLim = limGan;
#endif
  size_t escritos;
  i2s_channel_write(tx_chan, salida, sizeof(salida), &escritos, portMAX_DELAY);
}

// ==============================================================================================================================================
// BOTONES (core 0, 1 kHz): cada toque viaja al audio con su hora
// ==============================================================================================================================================
bool bEstado[5] = { false, false, false, false, false }; uint32_t bCambio[5] = { 0, 0, 0, 0, 0 };
static inline int32_t muestraOida() {     // la muestra de la canción que se está OYENDO en este instante
  int32_t m = pubMuestra; uint32_t us = pubMicros;
  return m + (int32_t)((micros() - us) * 0.0441f) - (int32_t)((LATENCIA_MS + 2.9f) * 44.1f);
}
void pasoControl() {
  uint32_t ahora = millis();
  for (int b = 0; b < 5; b++) {
    bool ap = digitalRead(BTN_PIN[b]) == LOW;
    if (ap == bEstado[b] || ahora - bCambio[b] < 12) continue;       // 12 ms de anti-rebote después de cada cambio
    bEstado[b] = ap; bCambio[b] = ahora; abajo[b] = ap;
    int est = estadoPub;
    if (est == E_JUEGO) {
      uint8_t sig = (colaW + 1) & 15;
      if (sig != colaR) { cola[colaW].carril = b; cola[colaW].abajo = ap; cola[colaW].t = muestraOida(); colaW = sig; }
    } else if (ap) {
      if (est == E_MENU) { if (b == 0 && difMenu > 0) difMenu = difMenu - 1; else if (b == 4 && difMenu < 2) difMenu = difMenu + 1; else if (b == 2) reqEmpezar = difMenu; }
      else { if (b == 2) reqEmpezar = difMenu; else if (b == 0 || b == 4) reqMenu = true; }
    }
  }
}

// ==============================================================================================================================================
// PANTALLA Y LEDS (core 0, prioridad baja)
// ==============================================================================================================================================
uint8_t fb[1024];
static inline void px(int x, int y, bool v) { if ((unsigned)x > 127 || (unsigned)y > 63) return; if (v) fb[x + (y >> 3) * 128] |= 1 << (y & 7); else fb[x + (y >> 3) * 128] &= ~(1 << (y & 7)); }
void rect(int x, int y, int w, int h, bool v) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) px(i, j, v); }
void marco(int x, int y, int w, int h) { rect(x, y, w, 1, true); rect(x, y + h - 1, w, 1, true); rect(x, y, 1, h, true); rect(x + w - 1, y, 1, h, true); }
// Fuente 5x7 (columnas, bit 0 arriba), de ' ' a 'Z'
const uint8_t FUENTE[59][5] = {
  {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
  {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x56,0x20,0x50}, {0x00,0x08,0x07,0x03,0x00},
  {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x2A,0x1C,0x7F,0x1C,0x2A}, {0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x80,0x70,0x30,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x00,0x60,0x60,0x00}, {0x20,0x10,0x08,0x04,0x02},
  {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x72,0x49,0x49,0x49,0x46}, {0x21,0x41,0x49,0x4D,0x33},
  {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x31}, {0x41,0x21,0x11,0x09,0x07},
  {0x36,0x49,0x49,0x49,0x36}, {0x46,0x49,0x49,0x29,0x1E}, {0x00,0x00,0x14,0x00,0x00}, {0x00,0x40,0x34,0x00,0x00},
  {0x00,0x08,0x14,0x22,0x41}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x59,0x09,0x06},
  {0x3E,0x41,0x5D,0x59,0x4E}, {0x7C,0x12,0x11,0x12,0x7C}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x41,0x3E}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x41,0x51,0x73},
  {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
  {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x1C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x26,0x49,0x49,0x49,0x32},
  {0x03,0x01,0x7F,0x01,0x03}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
  {0x63,0x14,0x08,0x14,0x63}, {0x03,0x04,0x78,0x04,0x03}, {0x61,0x59,0x49,0x4D,0x43},
};
int anchoTexto(const char *s, int esc) { int n = (int)strlen(s); return n ? n * (esc > 1 ? 6 * esc - 1 : 6) - esc : 0; }
void texto(int x, int y, const char *s, int esc) {
  for (; *s; s++) {
    int c = (unsigned char)*s; if (c >= 'a' && c <= 'z') c -= 32;
    const uint8_t *g = (c >= 32 && c <= 'Z') ? FUENTE[c - 32] : FUENTE[0];
    for (int col = 0; col < 5; col++) for (int f = 0; f < 8; f++) if (g[col] >> f & 1) rect(x + col * esc, y + f * esc, esc, esc, true);
    x += esc > 1 ? 6 * esc - 1 : 6;
  }
}
void centrado(int y, const char *s, int esc) { texto((128 - anchoTexto(s, esc)) / 2, y, s, esc); }
// Dígitos chicos 3x5 para el puntaje
const uint8_t CHICO[10][3] = { {0x1F,0x11,0x1F}, {0x12,0x1F,0x10}, {0x1D,0x15,0x17}, {0x15,0x15,0x1F}, {0x07,0x04,0x1F},
                               {0x17,0x15,0x1D}, {0x1F,0x15,0x1D}, {0x01,0x01,0x1F}, {0x1F,0x15,0x1F}, {0x17,0x15,0x1F} };
void numeroChico(int xDer, int y, int n) {     // alineado a la derecha
  if (n < 0) n = 0; int x = xDer - 2;
  do { const uint8_t *g = CHICO[n % 10]; for (int c = 0; c < 3; c++) for (int f = 0; f < 5; f++) if (g[c] >> f & 1) px(x + c, y + f, true); n /= 10; x -= 4; } while (n > 0);
}

void oledComandos(const uint8_t *c, int n) { Wire.beginTransmission(OLED_DIR); Wire.write((uint8_t)0x00); Wire.write(c, n); Wire.endTransmission(); }
void oledIniciar() {
  Wire.begin(OLED_SDA, OLED_SCL, I2C_HZ);
  const uint8_t init[] = { 0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
                           0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0x2E, 0xAF };
  oledComandos(init, sizeof(init));
}
void oledCuadro() {
  const uint8_t ventana[] = { 0x21, 0, 127, 0x22, 0, 7 };
  oledComandos(ventana, sizeof(ventana));
  for (int i = 0; i < 1024; i += 64) { Wire.beginTransmission(OLED_DIR); Wire.write((uint8_t)0x40); Wire.write(fb + i, 64); Wire.endTransmission(); }
}

// ── LA VISTA: un viaje. La pista va en perspectiva (las notas vienen hacia ti desde el horizonte) y detrás pasa
//    la galaxia en tres formas que se turnan: A hiperespacio de trazos · B planetas · C túnel de anillos.
//    La velocidad es la energía de la sección y salta con cada bombo; los anillos giran con los martillos.
//    ZONA DIFÍCIL: con muchas notas por compás hay más estrellas, se suma el hiperespacio, cruzan asteroides,
//    los bordes de la pista engordan y la pantalla golpea con el bombo.
#define HX 64
#define Y_TOP 8
#define Y_HIT 55
const float W_TOP = 14.0f, W_BOT = 98.0f, KP = 2.4f, AP = 1.0f / (1.0f + KP);
int   tiembla = 0;
float viaje = 0, rotAnillos = 0, zona = 0; uint32_t usPrevio = 0;
static inline void pxv(int x, int y, bool v) { px(x, y + tiembla, v); }
static inline float proy(float z) { if (z < -0.075f) z = -0.075f; return (1.0f / (1.0f + KP * z) - AP) / (1.0f - AP); }   // 1 en la línea de golpe, 0 en el horizonte
static inline float semiAncho(float p) { return (W_TOP + (W_BOT - W_TOP) * p) * 0.5f; }
static inline int yDe(float p) { return Y_TOP + (int)floorf((Y_HIT - Y_TOP) * p + 0.5f); }
static inline float azar(float n) { float x = sinf(n * 127.1f + 311.7f) * 43758.5453f; return x - floorf(x); }
void linea(int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (int n = 0; n < 220; n++) { pxv(x0, y0, true); if (x0 == x1 && y0 == y1) break; int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
}
void disco(int cx, int cy, float r, bool v) {
  int ri = (int)(r + 0.5f);
  for (int dy = -ri; dy <= ri; dy++) { int w = (int)sqrtf(fmaxf(0.0f, r * r - dy * dy)); for (int x = cx - w; x <= cx + w; x++) pxv(x, cy + dy, v); }
}
// franja de la pista entre las fracciones f0…f1 (0…1 a lo ancho) y las profundidades pLejos…pCerca
void franja(float f0, float f1, float pLejos, float pCerca, bool v) {
  int ya = yDe(pLejos), yb = yDe(pCerca);
  for (int y = ya; y <= yb; y++) {
    if (y > 63) break;
    float hw = semiAncho((float)(y - Y_TOP) / (Y_HIT - Y_TOP));
    int xa = HX + (int)floorf((f0 * 2.0f - 1.0f) * hw + 0.5f), xb = HX + (int)floorf((f1 * 2.0f - 1.0f) * hw + 0.5f);
    for (int x = xa; x <= xb; x++) pxv(x, y, v);
  }
}
void planeta(int x, int y, float r, int tipo) {
  if (tipo == 0) {                                      // con anillo
    float ca = cosf(-0.35f), sa = sinf(-0.35f); int ax = 0, ay = 0;
    for (int s = 0; s <= 16; s++) { float a = s * 0.3927f, ex = cosf(a) * r * 1.9f, ey = sinf(a) * r * 0.45f; int qx = x + (int)(ex * ca - ey * sa), qy = y + (int)(ex * sa + ey * ca); if (s) linea(ax, ay, qx, qy); ax = qx; ay = qy; }
    disco(x, y, r, true);
    if (r > 3) for (int s = 0; s <= 8; s++) { float a = s * 0.3927f, ex = cosf(a) * r * 1.9f, ey = sinf(a) * r * 0.45f; pxv(x + (int)(ex * ca - ey * sa), y + (int)(ex * sa + ey * ca), false); }
  } else if (tipo == 1) { disco(x, y, r, true); disco(x + (int)(r * 0.4f), y - (int)(r * 0.25f), r * 0.9f, false); }      // luna en creciente
  else { disco(x, y, r, true); if (r > 3) for (int i = -(int)r; i <= (int)r; i++) pxv(x + i, y + (int)(r * 0.25f), false); }  // con una banda
}
void fondo(int modo, float e, float dens) {
  uint32_t us = micros(), ms = millis(); float dt = (us - usPrevio) * 1e-6f; usPrevio = us; if (dt > 0.1f) dt = 0.1f;
  float eb = ms - bomboMs < 120 ? 1.0f - (ms - bomboMs) / 120.0f : 0.0f, em = ms - martilloMs < 110 ? 1.0f - (ms - martilloMs) / 110.0f : 0.0f;
  float vel = (0.10f + 0.55f * e) * (1.0f + 0.9f * eb);
  viaje += vel * dt; rotAnillos += dt * (0.12f + 2.2f * em); zona += (dens - zona) * fminf(1.0f, dt * 1.6f);
  tiembla = (zona > 0.6f && eb > 0.5f) ? 1 : 0;
  if (modo & 2) {                                       // B: estrellas lentas y planetas que pasan por los lados
    for (int i = 0; i < 14; i++) {
      float u = viaje * 0.08f + azar(i + 500) * 9.0f, c = floorf(u), z = u - c, ang = azar(i * 7.7f + c * 1.9f) * 6.2832f, r = 4.0f / (powf(1.0f - z, 1.3f) + 0.04f);
      pxv(HX + (int)(cosf(ang) * r), Y_TOP + (int)(sinf(ang) * r), true);
    }
    for (int j = 0; j < 3; j++) {
      float u = viaje * 0.16f + j / 3.0f + 0.13f, c = floorf(u), z = u - c, k = c * 3.0f + j;
      float lado = ((int)k & 1) ? 1.0f : -1.0f, ang = (azar(k * 5.3f) - 0.55f) * 0.9f, fac = 1.0f / (powf(1.0f - z, 1.5f) + 0.045f);
      float dist = 2.6f * fac + 12.0f, rad = (0.7f + 1.3f * azar(k * 9.1f)) * fac * 0.55f; if (rad < 1.0f) continue;
      planeta(HX + (int)(lado * cosf(ang) * dist), Y_TOP + 10 + (int)(sinf(ang) * dist), rad, (int)(azar(k * 2.7f) * 3.0f));
    }
  }
  if ((modo & 1) || zona > 0.55f) {                     // A: hiperespacio (en zona densa se suma siempre)
    int N = 22 + (int)(44.0f * zona); float largo = 0.03f + 0.16f * vel;
    for (int i = 0; i < N; i++) {
      float u = viaje * 0.55f + azar(i) * 7.0f, c = floorf(u), z = u - c, ang = azar(i * 13.7f + c * 3.1f) * 6.2832f;
      float z0 = z - largo; if (z0 < 0) z0 = 0;
      float r1 = 4.5f / (powf(1.0f - z, 1.7f) + 0.03f), r0 = 4.5f / (powf(1.0f - z0, 1.7f) + 0.03f); if (r0 > 80.0f) continue; if (r1 > 90.0f) r1 = 90.0f;
      float co = cosf(ang), si = sinf(ang);
      linea(HX + (int)(co * r0), Y_TOP + (int)(si * r0), HX + (int)(co * r1), Y_TOP + (int)(si * r1));
    }
  }
  if (modo & 4) {                                       // C: túnel de anillos hexagonales
    for (int i = 0; i < 5; i++) {
      float u = viaje * 0.35f + i / 5.0f, c = floorf(u), z = u - c, id = c * 5.0f + i;
      float r = 3.6f / (powf(1.0f - z, 1.6f) + 0.035f) * (1.0f + 0.05f * eb); if (r > 95.0f || r < 2.0f) continue;
      float rot = rotAnillos + id * 0.21f; int ax = 0, ay = 0;
      for (int s = 0; s <= 6; s++) { float a = rot + s * 1.0472f; int qx = HX + (int)(cosf(a) * r), qy = Y_TOP + (int)(sinf(a) * r * 0.8f); if (s) linea(ax, ay, qx, qy); ax = qx; ay = qy; }
    }
  }
  int cuantos = (int)(zona * 7.0f);                     // asteroides cerca de la pista
  for (int j = 0; j < cuantos; j++) {
    float u = viaje * 0.5f + azar(j + 900) * 5.0f, c = floorf(u), z = u - c, k = c * 7.0f + j;
    float lado = ((int)k & 1) ? 1.0f : -1.0f, ang = (azar(k * 4.1f) - 0.3f) * 1.4f, fac = 1.0f / (powf(1.0f - z, 1.6f) + 0.04f);
    float dist = 3.4f * fac + 8.0f, r = 0.5f * (1.0f + 1.5f * azar(k * 6.7f)) * fac * 0.5f; if (r < 1.0f || r > 9.0f) continue;
    int x = HX + (int)(lado * cosf(ang) * dist), y = Y_TOP + (int)(sinf(ang) * dist);
    disco(x, y, r, true); if (r > 2.5f) disco(x + 1, y - 1, r * 0.45f, false);
  }
}
void dibujarJuego() {
  int32_t ahoraM = muestraOida() + (int32_t)(PANTALLA_MS * 44.1f);
  float ventM = DIF_VENTANA[difActual] * SR;
  int compas = (int)(ahoraM / (16 * muestrasPorPaso)); if (compas < 0) compas = 0; if (compas >= nCompas) compas = nCompas - 1;
  const Compas &cp = pista[compas];
  fondo(cp.fondo, cp.e, cp.dens);
  // la pista tapa el fondo; bordes (dobles en zona difícil) y carriles punteados
  float pFin = proy(-1.0f);
  franja(0.0f, 1.0f, 0.0f, pFin + 0.2f, false);
  for (int y = Y_TOP; y <= 63; y++) {
    float hw = semiAncho((float)(y - Y_TOP) / (Y_HIT - Y_TOP));
    int xi = HX - (int)floorf(hw + 0.5f), xd = HX + (int)floorf(hw + 0.5f);
    pxv(xi, y, true); pxv(xd, y, true);
    if (zona > 0.6f) { pxv(xi - 1, y, true); pxv(xd + 1, y, true); }
    if (y % 3 == 0) for (int c = 1; c < 5; c++) pxv(HX + (int)floorf((c * 0.4f - 1.0f) * hw + 0.5f), y, true);
  }
  for (int c = 0; c < 5; c++) {                                          // casillas: marco, llenas al apretar
    franja((c + 0.10f) / 5, (c + 0.90f) / 5, proy(0.022f), proy(-0.022f), true);
    if (!abajo[c]) franja((c + 0.22f) / 5, (c + 0.78f) / 5, proy(0.012f), proy(-0.012f), false);
  }
  for (int i = 0; i < nNotas; i++) {
    const Nota &n = notas[i];
    float z = (n.t - ahoraM) / ventM; if (z > 1.0f) break;
    float zFin = (n.t + n.dur - ahoraM) / ventM; if (zFin < -0.075f || n.estado == 2) continue;
    if (n.dur > 0 && (n.estado == 0 || n.sost)) {                        // la cola de una sostenida
      float zc = n.sost ? 0.0f : (z < -0.075f ? -0.075f : z);
      if (zFin > zc) franja((n.carril + 0.36f) / 5, (n.carril + 0.64f) / 5, proy(zFin > 1.0f ? 1.0f : zFin), proy(zc), true);
    }
    if (n.estado == 0 && z > -0.075f) franja((n.carril + 0.13f) / 5, (n.carril + 0.87f) / 5, proy(z + 0.018f > 1.0f ? 1.0f : z + 0.018f), proy(z - 0.018f), true);
  }
  uint32_t ms = millis();
  for (int c = 0; c < 5; c++) if (ms - aciertoMs[c] < 90) franja((c + 0.10f) / 5, (c + 0.90f) / 5, proy(0.16f), proy(0.04f), true);    // chispa del acierto
  tiembla = 0;
  // arriba: multiplicador y racha a la izquierda, juicio al centro, puntaje a la derecha
  rect(0, 0, 128, 7, false);
  char t[8]; snprintf(t, sizeof(t), "X%d", (int)multPub); texto(0, 0, t, 1);
  if (rachaPub > 1) numeroChico(30, 1, rachaPub);
  if (ms - juicioMs < 420) { int j = juicioPub; const char *w = j == 1 ? "PERF" : j == 2 ? "BIEN" : "MAL"; texto(64 - anchoTexto(w, 1) / 2, 0, w, 1); }
  numeroChico(127, 1, puntosPub);
  // energía: una barra continua y con marco a la izquierda, de arriba abajo. Cada falla se ve bajar (unos 4 px);
  // al recibir el golpe el marco se engrosa un instante, y con poca energía la barra parpadea.
  int alto = (vidaPub * 49 + 50) / 100; bool poca = vidaPub <= 25 && (ms / 160) % 2 == 0, golpeada = ms - fallaMs < 160;
  rect(0, 9, 11, 55, false);
  marco(0, 10, 8, 53);
  if (golpeada) marco(1, 11, 6, 51);
  if (!poca && alto > 0) rect(2, 61 - alto, 4, alto, true);
}
void dibujarMenu() {
  centrado(2, "PERCU HERO", 2);
  centrado(24, "TURNO DE NOCHE", 1);
  char t[24]; snprintf(t, sizeof(t), "%s %s %s", difMenu > 0 ? "<" : " ", DIF_NOMBRE[difMenu], difMenu < 2 ? ">" : " ");
  centrado(38, t, 1);
  centrado(54, "BTN3 EMPIEZA", 1);
}
void dibujarFin(bool perdio) {
  char t[28];
  if (perdio) { centrado(2, "SIN ENERGIA", 2); snprintf(t, sizeof(t), "LLEGASTE AL %d%%", (int)avancePub); centrado(24, t, 1); }
  else {
    int total = nTotalPub, pct = total ? (100 * (nPerfPub + nBienPub) + total / 2) / total : 0;
    const char *nota = pct >= 96 ? "S" : pct >= 88 ? "A" : pct >= 74 ? "B" : pct >= 55 ? "C" : "D";
    snprintf(t, sizeof(t), "NOTA %s", nota); centrado(2, t, 2);
    snprintf(t, sizeof(t), "ACIERTOS %d%%", pct); centrado(24, t, 1);
  }
  snprintf(t, sizeof(t), "PUNTOS %d", (int)puntosPub); centrado(36, t, 1);
  centrado(54, "BTN3 OTRA  BTN1 MENU", 1);
}
void actualizarLeds() {
  uint32_t ms = millis();
  for (int c = 0; c < 5; c++) {
    uint32_t d = ms - aciertoMs[c];
    if (ms - fallaMs < 120) leds[c] = CRGB(90, 0, 0);
    else if (d < 160) { uint8_t k = 255 - d * 255 / 160; leds[c] = CRGB(k, (uint8_t)(k * 0.62f), (uint8_t)(k * 0.10f)); }
    else leds[c] = abajo[c] ? CRGB(40, 36, 28) : CRGB::Black;
  }
  uint32_t db = ms - bomboMs; uint8_t k = db < 120 ? 120 - db : 0;
  leds[5] = cargaAlta ? CRGB(120, 0, 120) : CRGB(k, (uint8_t)(k * 0.6f), 0);
  FastLED.show();
}
void tareaPantalla(void *) {
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);                 // acá, no en setup(): las interrupciones del RMT quedan en el core 0
  FastLED.setBrightness(70); FastLED.clear(true);
  oledIniciar();
  for (;;) {
    memset(fb, 0, sizeof(fb));
    int est = estadoPub;
    if (est == E_JUEGO) dibujarJuego(); else if (est == E_MENU) dibujarMenu(); else dibujarFin(est == E_PERDIO);
    oledCuadro(); actualizarLeds();
    vTaskDelay(2);
  }
}

// ==============================================================================================================================================
// SETUP
// ==============================================================================================================================================
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;   // ≈ 12 ms de cola
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
void audioTask(void *)   { for (;;) renderBuffer(); }                     // core 1, prioridad 10
void controlTask(void *) {                                                // core 0, 1 kHz
  for (;;) {
    pasoControl(); vTaskDelay(1);
#if MEDIR
    static uint32_t ultimo = 0;
    if (millis() - ultimo >= 2000 && medN > 0) {
      ultimo = millis(); const float tope = 240000000.0f * BUFFER_SAMPLES / SR;
      Serial.printf("compas %d  audio %.0f%% medio  %.0f%% max  limitador %.2f  estado %d\n", (int)(mCancion / (16 * muestrasPorPaso)),
                     100.0f * (medSuma * 64.0f / medN) / tope, 100.0f * medMax / tope, (float)medLim, (int)estadoPub);
      Serial0.printf("compas %d  audio %.0f%% medio  %.0f%% max  limitador %.2f\n", (int)(mCancion / (16 * muestrasPorPaso)),
                     100.0f * (medSuma * 64.0f / medN) / tope, 100.0f * medMax / tope, (float)medLim);
      medSuma = 0; medMax = 0; medN = 0; medLim = 1.0f;
      if (estadoPub != E_JUEGO) reqEmpezar = 2;
    }
#endif
  }
}

void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);
  for (int b = 0; b < 5; b++) pinMode(BTN_PIN[b], INPUT_PULLUP);
  for (int i = 0; i <= TABLA; i++) SENO[i] = sinf(2.0f * PI * i / TABLA);
  memset(golpes, 0, sizeof(golpes)); memset(voces, 0, sizeof(voces));
  memset(&restoL, 0, sizeof(restoL)); memset(&restoR, 0, sizeof(restoR)); memset(&bjFl, 0, sizeof(bjFl)); memset(&drFl, 0, sizeof(drFl)); memset(&suBp, 0, sizeof(suBp));
  svfCoef(bjFl, 200, 2); svfCoef(drFl, 170, 2.5f);
  semilla ^= (uint32_t)micros();
#if MEDIR
  Serial.begin(115200); Serial.setTxTimeoutMs(0);   // USB CDC sin bloquear: si nadie lee, se pierde la línea y el audio no se entera
  Serial0.begin(115200, SERIAL_8N1, -1, 43);        // y también por el conector izquierdo (sólo TX: el RX del UART0 es el BTN1)
#endif
  i2s_init();
  xTaskCreatePinnedToCore(audioTask,     "audio",    8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask,   "control",  4096, NULL,  3, NULL, 0);
  xTaskCreatePinnedToCore(tareaPantalla, "pantalla", 6144, NULL,  1, NULL, 0);
}
void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }
