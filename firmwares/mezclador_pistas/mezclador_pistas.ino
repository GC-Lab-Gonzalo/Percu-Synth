// ==============================================================================================================================================
// PERCUSYNTH - MEZCLADOR DE PISTAS (4 pistas desde la microSD → filtros por pista → DAC + datos para las visuales) - GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo Sandoval - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
// REPOSITORIO: https://github.com/GC-Lab-Gonzalo/Percu-Synth
// ==============================================================================================================================================
// HARDWARE
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3 (PercuSynth, DevKitC-1 N16R8: hace falta la PSRAM).
// - DAC PCM5102 vía I2S — estéreo 44.1 kHz · 16-bit |LCK -> 39, DIN -> 40, BCK -> 41|
// - Módulo microSD por SPI (el mismo cableado que grabador_campo):
//       SCK -> 14 · MOSI -> 15 · MISO -> 16 · CS -> 17 · VCC 5 V si el módulo trae regulador (3.3 V si no) · GND
// - IMU MPU6050 (I2C) |SDA -> 21, SCL -> 38|  (dirección 0x68 o 0x69)
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
// - PSRAM              : OPI PSRAM    (OBLIGATORIA: el colchón de lectura de 4 s vive ahí)
// - Monitor            : 115200 baud  (ver «SALIDA SERIAL» más abajo: lo lee Resonancia)
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (driver/i2s_std.h, SPI.h, SD.h, Wire.h — todas incluidas)
// - FastLED
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// La placa toca una canción separada en 4 pistas (batería, bajo, voz, otros) que viven en la microSD,
// todas a la vez y sincronizadas a la muestra. Cada pista tiene su volumen y su filtro, y el
// PercuSynth las mezcla y las saca por el DAC: el computador no toca el audio.
// Al mismo tiempo analiza CADA pista por separado (nivel, golpes, y en la batería el bombo, la caja
// y el platillo) y lo manda 60 veces por segundo por USB a Resonancia, la app de visuales: cada
// capa visual reacciona a su instrumento y su fader es el mismo de la pista.
//
// En la tarjeta van dos archivos (los genera la conversión con ffmpeg, ver README):
//   /cancion.wav   WAV de 4 canales mono intercalados, 16 bit, 44.1 kHz: bateria, bajo, voz, otros
//   /cancion.txt   bpm=125 · primer_pulso_ms=61 · titulo=… · artista=…
// Más canciones: /cancion1.wav … /cancion9.wav, cada una con su .txt (BTN1 mantenido 2,5 s = siguiente).
// Un solo archivo intercalado se lee de corrido (353 KB/s). Cuatro archivos sueltos obligarían a la
// tarjeta a saltar entre ellos, y esos saltos son los que cortan el audio.
//
// Cadena por pista:  pista → FILTRO (pasa-bajos ↔ pasa-altos, un solo control) → VOLUMEN → paneo
// Master:            suma → bloqueador DC → limitador con lookahead (techo 0.89) → DAC
// Sin pasa-bajos de 13 kHz en el master: las pistas ya vienen mezcladas y masterizadas, y ese techo
// es para la síntesis de la placa (que sí genera aliasing); acá sólo le quitaría aire a la canción.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// Todo funciona por BANCOS: un botón elige el banco y las 4 perillas pasan a controlar ese banco.
// Al cambiar de banco, una perilla NO salta al valor nuevo: toma el control recién cuando la mueves
// (así no hay saltos de volumen ni de filtro). En Resonancia aparece qué hace cada perilla.
//
// - BTN1  toque     -> BANCO GENERAL: POT1 volumen BAJO · POT2 VOZ · POT3 BATERÍA · POT4 OTROS
//         mantener  -> PLAY / STOP (medio segundo). Mantenido 2,5 s -> SIGUIENTE CANCIÓN desde el inicio, en stop
//                      (con una sola canción en la tarjeta, vuelve al inicio)
// - BTN2 -> banco BAJO · BTN3 -> banco VOZ · BTN4 -> banco BATERÍA · BTN5 -> banco OTROS. En cada uno:
//         POT1  FILTRO: al centro abierto · a la izquierda pasa-bajos (hasta 110 Hz) · a la derecha pasa-altos (hasta 5 kHz)
//         POT2  RESONANCIA del filtro (atada al corte: nunca un pico resonante arriba)
//         POT3  EFECTO RÍTMICO, amarrado al pulso de la canción (zonas): apagado · gate 1/8 · gate 1/16 ·
//               repeat 1/2 · repeat 1/4 · repeat 1/8 · repeat 1/16
//                 gate   = corta el sonido en corcheas o semicorcheas
//                 repeat = repite el comienzo de cada pulso (un tartamudeo rítmico)
//         POT4  INTENSIDAD del efecto (cuánto corta el gate / cuánto repeat se mezcla)
// - IMU   -> no toca el sonido: inclinar la placa gira y tiñe las visuales
// - PIEZOS -> no suenan: cada golpe va a las visuales (destellos y ondas encima de la canción)
//
// - LEDs:  0  verde = sonando · ámbar = stop · naranjo = la tarjeta no alcanzó · MAGENTA parpadeando = sin tarjeta
//             parpadea en blanco cuando el banco GENERAL está elegido
//          1–4  bajo, voz, batería, otros (el orden de BTN2..BTN5): su color, con el brillo de su nivel.
//               Parpadea en blanco la del banco elegido
//          5  pulso: chartreuse en el «uno» de cada compás, blanco en los otros tiempos
//
// SALIDA SERIAL (USB CDC, 60 líneas por segundo, una por cuadro de video):
//   R,pos_ms,fase,compas,play,v1,v2,v3,v4,f1,f2,f3,f4,n1,n2,n3,n4,g1,g2,g3,g4,kick,caja,hat,piezo,vel,ax,ay,btn,
//     banco,r1,r2,r3,r4,e1,e2,e3,e4,i1,i2,i3,i4,tomado         (las pistas 1..4 son batería, bajo, voz, otros)
//     pos_ms   posición en la canción          fase   0..999 dentro del pulso      compas  pulsos contados
//     v1..v4   volumen efectivo 0..1000 (0 = mute)                f1..f4  filtro −1000..1000 (0 = abierto)
//     n1..n4   nivel de cada pista 0..1000                         g1..g4  contador de golpes de cada pista
//     kick/caja/hat  contadores de la batería       piezo, vel  contador y fuerza 0..1000 del último piezo
//     ax, ay   inclinación −1000..1000              btn  botones mantenidos (bits 0..4)
//     banco    0 general · 1 bajo · 2 voz · 3 batería · 4 otros
//     r1..r4   resonancia 0..1000       e1..e4  efecto 0..6        i1..i4  intensidad 0..1000
//     tomado   perillas que ya tomaron el control en este banco (bits 0..3)
//   Los contadores sólo suben: la app compara con el anterior y así no se pierde un golpe aunque se
//   salte una línea. Al conectar, al cambiar de canción y cada 5 s:  I,titulo,artista,bpm,duracion_ms,cancion,total
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <math.h>
#include <string.h>

// ─── Tipos (arriba del todo para que el IDE genere bien los prototipos) ───
struct SVF { float ic1, ic2, a1, a2, a3, k, comp; int modo; };                     // filtro de variable de estado (TPT), un estado por pista
struct Seguidor { float env, base, maximo, nivel; bool armado; uint32_t ultimo, golpes; };

// ==============================================================================================================================================
// CONFIGURACIÓN
// ==============================================================================================================================================
#define NUM_PISTAS        4
const char* const NOMBRE_PISTA[NUM_PISTAS] = { "bateria", "bajo", "voz", "otros" };
// Canciones en la raíz de la tarjeta: /cancion.wav, /cancion1.wav … /cancion9.wav, cada una con su .txt.
// Se cargan en ese orden; BTN1 mantenido 2,5 s pasa a la siguiente.
#define MAX_CANCIONES 11
char  cancionBase[MAX_CANCIONES][16];
int   numCanciones = 0;
volatile int  cancionActual = 0;
volatile bool pedidoCancion = false, infoNueva = true;

const float  TECHO          = 0.89f;    // −1 dBFS: techo del limitador
const float  MASTER         = 0.95f;    // las 4 pistas al máximo suman ~0.91 de pico en esta canción
const float  CURVA_VOLUMEN  = 1.8f;     // pot^1.8: la mitad del recorrido queda en ~−11 dB, como un fader real
const float  FILTRO_LP_MIN  = 110.0f;   // filtro a fondo a la izquierda: pasa-bajos en 110 Hz
const float  FILTRO_HP_MAX  = 5000.0f;  // filtro a fondo a la derecha: pasa-altos en 5 kHz
const float  HAAS_MS        = 11.0f;    // «otros» se abre en estéreo con un retardo corto en el canal derecho
const unsigned long PLAY_MS  = 500;     // BTN1 mantenido medio segundo = play / stop
const unsigned long VOLVER_MS = 2500;    // BTN1 mantenido 2,5 s = volver al inicio
const float  TOMA_PERILLA   = 0.04f;    // una perilla toma el control del banco al moverse un 4 % (no salta)

// ─── Pines ───────────────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SD_SCK    14
#define SD_MOSI   15
#define SD_MISO   16
#define SD_CS     17
const uint32_t SD_FREQ_HZ = 20000000;   // 20 MHz, igual que grabador_campo
#define SDA_PIN   21
#define SCL_PIN   38
#define LED_PIN   46
#define NUM_LEDS   6
#define LED_BRILLO 70
const uint8_t BTN_PIN[5]   = { 44, 42, 0, 45, 47 };
const uint8_t POT_PIN[4]   = { 1, 2, 8, 10 };   // POT1..POT4, el pinout fijo de PROMPT_PARA_LA_IA.md
// Pistas internas: 0 batería · 1 bajo · 2 voz · 3 otros (el orden de canales de cancion.wav)
const int GENERAL_PISTA[4] = { 1, 2, 0, 3 };   // banco general: POT1 bajo · POT2 voz · POT3 batería · POT4 otros
const int BANCO_PISTA[4]   = { 1, 2, 0, 3 };   // BTN2 bajo · BTN3 voz · BTN4 batería · BTN5 otros
#define HIST           65536                   // historia por pista para el repeat: 1,49 s (un pulso hasta 41 BPM)
const uint8_t PIEZO_PIN[4] = { 4, 5, 6, 7 };
const int     PIEZO_UMBRAL = 500;        // de 4095: sobre esto, un golpe

// ─── Audio ───────────────────────────────────────────────────
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
const float SR     = (float)SAMPLE_RATE;
const float PI_F   = 3.14159265f;

// ─── Colchón de lectura (PSRAM) ──────────────────────────────
#define RING_SEGUNDOS   4
#define RING_FRAMES     (RING_SEGUNDOS * SAMPLE_RATE)        // 176 400 cuadros × 4 pistas × 2 bytes = 1.4 MB
#define LECTURA_FRAMES  2048                                 // 16 KB por lectura de la tarjeta (~46 ms de audio)

// ==============================================================================================================================================
// ESTADO COMPARTIDO ENTRE NÚCLEOS
// ==============================================================================================================================================
// Escrituras de 32 bits alineadas, atómicas en el S3. Cada variable tiene UN solo escritor:
//   lectora → audio : escritos (cuadros puestos en el colchón), base (cuadro que es el inicio de la canción)
//   audio   → lectora: leidos  (cuadros consumidos)
//   control → audio : vol[], filtro[], reproducir
//   control → lectora: pedidoInicio
//   audio   → control: nivel[], golpes, posición
int16_t *ring = nullptr;                         // [RING_FRAMES][4]
volatile uint32_t escritos = 0, leidos = 0, base = 0;
volatile uint32_t totalFrames = 0;               // largo de la canción en cuadros
volatile bool     sdLista = false, sdError = false;
volatile bool     reproducir = false;            // lo que pide el usuario
volatile bool     sonando = false;               // lo que hace el audio (tras el fundido)
volatile bool     pedidoInicio = false;
volatile uint32_t faltantes = 0;                 // bloques en que la tarjeta no alcanzó

volatile float    vol[NUM_PISTAS]    = { 0.8f, 0.8f, 0.8f, 0.8f };
volatile float    filtro[NUM_PISTAS] = { 0, 0, 0, 0 };   // −1 pasa-bajos … 0 abierto … +1 pasa-altos
volatile float    reso[NUM_PISTAS]   = { 0, 0, 0, 0 };   // 0..1
volatile int      efecto[NUM_PISTAS] = { 0, 0, 0, 0 };   // 0 apagado · 1 gate 1/8 · 2 gate 1/16 · 3..6 repeat 1/2, 1/4, 1/8, 1/16
volatile float    intensidad[NUM_PISTAS] = { .7f, .7f, .7f, .7f };
volatile int      banco = 0;                             // 0 general · 1..4 = BTN2..BTN5
volatile uint32_t posBloque = 0;                         // posición en la canción del bloque que se está procesando
int16_t *hist = nullptr;                                 // [NUM_PISTAS][HIST] en PSRAM

volatile float    nivelPista[NUM_PISTAS] = { 0, 0, 0, 0 };
volatile uint32_t golpesPista[NUM_PISTAS] = { 0, 0, 0, 0 };
volatile uint32_t golpesKick = 0, golpesCaja = 0, golpesHat = 0;

float  bpm = 125.0f;
uint32_t primerPulso = 0;                        // en cuadros
char   titulo[64] = "", artista[64] = "";

// ==============================================================================================================================================
// DSP
// ==============================================================================================================================================
SVF      svf[NUM_PISTAS];
float    cortePista[NUM_PISTAS];
int      modoFil[NUM_PISTAS];                    // 0 = pasa-bajos, 1 = pasa-altos
float    volSuave[NUM_PISTAS] = { 0, 0, 0, 0 };
float    fundido = 0.0f;                         // 0..1: play y pausa sin clic
Seguidor seg[NUM_PISTAS];
Seguidor segKick, segCaja, segHat;
float    lpKick = 0, lpCaja1 = 0, lpCaja2 = 0, lpHat = 0;

#define HAAS_MAX 1024
float    haas[HAAS_MAX]; int haasIdx = 0; int haasN = 485;

#define LIM_LOOK 64
float limDlyL[LIM_LOOK], limDlyR[LIM_LOOK];
int   limDlyIdx = 0;
float limEnv = 0.0f, limRel = 0.0f, limGain = 1.0f;
float dcX1L = 0, dcY1L = 0, dcX1R = 0, dcY1R = 0;

// coeficientes del SVF desde el control del filtro (−1..1). Al centro las dos ramas están abiertas
// (pasa-bajos en 18 kHz o pasa-altos en 20 Hz), así que cambiar de rama ahí no se oye.
void calcularFiltro(int p, float f, float r) {
  float fc, q;
  if (f <= 0.0f) { modoFil[p] = 0; fc = 18000.0f * powf(FILTRO_LP_MIN / 18000.0f, -f); }
  else           { modoFil[p] = 1; fc = 20.0f    * powf(FILTRO_HP_MAX / 20.0f,     f); }
  // La resonancia se ata al corte: con el pasa-bajos casi abierto (corte alto) se permite poca, para
  // que nunca quede un pico resonante en los agudos (regla del proyecto: los agudos molestos son un bug).
  float tope = 1.15f - fc / 9000.0f; tope = tope < 0.15f ? 0.15f : tope > 1.0f ? 1.0f : tope;
  q = 0.707f + 0.5f * fabsf(f) + r * 7.0f * tope;
  float g = tanf(PI_F * fc / SR), k = 1.0f / q;
  svf[p].k = k; svf[p].a1 = 1.0f / (1.0f + g * (g + k)); svf[p].a2 = g * svf[p].a1; svf[p].a3 = g * svf[p].a2;
  svf[p].comp = 1.0f / (1.0f + 0.22f * (q - 0.707f));   // compensa el volumen que suma la resonancia
  svf[p].modo = modoFil[p];
  cortePista[p] = fc;
}

inline float procesarSVF(int p, float x) {
  SVF &s = svf[p];                                // coeficientes por bloque: cero divisiones por muestra
  float v3 = x - s.ic2;
  float v1 = s.a1 * s.ic1 + s.a2 * v3;
  float v2 = s.ic2 + s.a2 * s.ic1 + s.a3 * v3;
  s.ic1 = 2.0f * v1 - s.ic1; s.ic2 = 2.0f * v2 - s.ic2;
  return (s.modo ? (x - s.k * v1 - v2) : v2) * s.comp;   // pasa-altos : pasa-bajos
}

// Detector de golpes, ajustado con la canción real contra la grilla de 125 BPM (ver README):
//   env    envolvente: sube en ~1 ms, cae en ~30 ms
//   base   la media de esa envolvente en ~300 ms: lo que «venía sonando»
//   maximo el golpe más fuerte reciente (cae a la mitad en ~1,4 s): umbral relativo, sirve a cualquier volumen
// Dispara cuando env pasa 1,6 veces la base Y el 12 % del máximo, y se re-arma recién cuando env
// vuelve a bajar a 1,15 veces la base (histéresis). Sin la histéresis, la cola de un mismo golpe
// volvía a disparar: la primera versión contaba 2 a 4,5 «golpes» por pulso en el bajo y el bombo.
const float DET_ATK = 0.0224f, DET_REL = 0.000756f, DET_BASE = 0.0000756f, DET_MAX = 0.9999887f;
const uint32_t DET_REFRACTARIO = 3970;          // 90 ms
inline void seguir(Seguidor &s, float x, uint32_t ahora) {
  float a = fabsf(x);
  s.env  += (a - s.env) * (a > s.env ? DET_ATK : DET_REL);
  s.base += (s.env - s.base) * DET_BASE;
  s.maximo = s.env > s.maximo ? s.env : s.maximo * DET_MAX;
  if (a > s.nivel) s.nivel = a; else s.nivel *= 0.99985f;
  if (s.armado) {
    if (s.env > s.base * 1.6f && s.env > 0.12f * s.maximo && s.env > 0.002f && ahora - s.ultimo > DET_REFRACTARIO) {
      s.ultimo = ahora; s.golpes++; s.armado = false;
    }
  } else if (s.env < s.base * 1.15f) s.armado = true;
}

void prepararDSP() {
  for (int p = 0; p < NUM_PISTAS; p++) { memset(&svf[p], 0, sizeof(SVF)); calcularFiltro(p, 0.0f, 0.0f); memset(&seg[p], 0, sizeof(Seguidor)); }
  memset(&segKick, 0, sizeof(Seguidor)); memset(&segCaja, 0, sizeof(Seguidor)); memset(&segHat, 0, sizeof(Seguidor));
  for (int p = 0; p < NUM_PISTAS; p++) seg[p].armado = true;
  segKick.armado = segCaja.armado = segHat.armado = true;   // en cero nunca se re-armaría (0 < 0 es falso)
  memset(haas, 0, sizeof(haas)); haasN = (int)(HAAS_MS * 0.001f * SR);
  for (int i = 0; i < LIM_LOOK; i++) { limDlyL[i] = 0; limDlyR[i] = 0; }
  limRel = expf(-1.0f / (SR * 0.08f));          // el limitador suelta en ~80 ms
}

inline float techoSuave(float x) {               // lineal hasta 0.95; arriba, una curva que nunca pasa de 1
  float a = fabsf(x);
  if (a <= 0.95f) return x;
  float y = 0.95f + 0.05f * tanhf((a - 0.95f) / 0.05f);
  return x < 0 ? -y : y;
}

// Efectos rítmicos por pista, amarrados al pulso de la canción (bpm y primer pulso de cancion.txt):
//   gate   la pista se abre en la primera mitad de cada corchea / semicorchea y se cierra en la otra,
//          con rampas de 3 ms (sin rampas, cada corte es un clic)
//   repeat repite el primer trozo de cada pulso: el trozo es 1/2, 1/4, 1/8 o 1/16 de pulso. Sale de
//          la historia de la pista ANTES del filtro, así el filtro sigue actuando sobre lo repetido.
//          Cada repetición entra y sale con 1,5 ms de rampa.
// Cambiar de efecto no corta: el efecto viejo se desvanece en ~10 ms y recién ahí entra el nuevo.
const float DIV_EFECTO[7] = { 0, 0.5f, 0.25f, 0.5f, 0.25f, 0.125f, 0.0625f };   // en pulsos
int   efectoAct[NUM_PISTAS] = { 0, 0, 0, 0 };
float mezclaEf[NUM_PISTAS]  = { 0, 0, 0, 0 };
float fSuave[NUM_PISTAS] = { 0, 0, 0, 0 }, rSuave[NUM_PISTAS] = { 0, 0, 0, 0 };

// Procesa un bloque. 'entrada' puede ser nullptr (silencio). Devuelve el bloque estéreo intercalado.
uint32_t muestraGlobal = 0;
void procesarBloque(const int16_t *entrada, int16_t *salida, uint32_t pos0) {
  // parámetros del bloque
  for (int p = 0; p < NUM_PISTAS; p++) {
    fSuave[p] += (filtro[p] - fSuave[p]) * 0.15f;           // ~20 ms por bloques: la perilla no salta
    rSuave[p] += (reso[p]   - rSuave[p]) * 0.15f;
    calcularFiltro(p, fSuave[p], rSuave[p]);
  }
  float objFundido = reproducir ? 1.0f : 0.0f;
  float objVol[NUM_PISTAS];                      // la curva del fader, una vez por bloque (powf es caro en el S3)
  for (int p = 0; p < NUM_PISTAS; p++) objVol[p] = powf(vol[p], CURVA_VOLUMEN);

  // dónde estamos dentro del pulso (en muestras), una vez por bloque; después se avanza de a una
  const float Lb = SR * 60.0f / bpm;
  double rel = (double)pos0 - (double)primerPulso;
  bool conPulso = entrada && rel >= 0;
  float enPulso = conPulso ? (float)fmod(rel, (double)Lb) : 0.0f;

  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    fundido += (objFundido - fundido) * 0.003f;  // ~8 ms
    float l = 0, r = 0;
    for (int p = 0; p < NUM_PISTAS; p++) {
      float x = entrada ? entrada[n * NUM_PISTAS + p] * (1.0f / 32768.0f) : 0.0f;
      hist[p * HIST + (muestraGlobal & (HIST - 1))] = entrada ? entrada[n * NUM_PISTAS + p] : 0;

      // efecto rítmico
      int pedido = conPulso ? efecto[p] : 0;
      float objMezcla = (pedido == efectoAct[p] && pedido) ? intensidad[p] : 0.0f;
      mezclaEf[p] += (objMezcla - mezclaEf[p]) * 0.0025f;
      if (pedido != efectoAct[p] && mezclaEf[p] < 0.002f) efectoAct[p] = pedido;
      int ef = efectoAct[p];
      if (ef && mezclaEf[p] > 0.0005f) {
        float L = Lb * DIV_EFECTO[ef];
        float fase = enPulso - floorf(enPulso / L) * L;      // muestras dentro del trozo
        if (ef <= 2) {                                       // gate
          float media = 0.5f * L, env;
          if (fase < media) { env = fase < 132.0f ? fase / 132.0f : (media - fase < 132.0f ? (media - fase) / 132.0f : 1.0f); }
          else env = 0.0f;
          x *= 1.0f - mezclaEf[p] * (1.0f - env);
        } else {                                             // repeat
          uint32_t atras = (uint32_t)(enPulso - fase);       // cuántas muestras atrás empezó el trozo que se repite
          float rep = hist[p * HIST + ((muestraGlobal - atras) & (HIST - 1))] * (1.0f / 32768.0f);
          float e = fase < 66.0f ? fase / 66.0f : 1.0f;
          if (L - fase < 66.0f) e = (L - fase) / 66.0f;
          x = x * (1.0f - mezclaEf[p]) + rep * e * mezclaEf[p];
        }
      }

      float y = procesarSVF(p, x);
      volSuave[p] += (objVol[p] - volSuave[p]) * 0.004f;   // ~6 ms
      y *= volSuave[p] * fundido;
      seguir(seg[p], y, muestraGlobal);
      if (p == 0) {                                       // la batería, separada en tres
        lpKick  += (y - lpKick)  * 0.017f;                // pasa-bajos ~120 Hz
        lpCaja1 += (y - lpCaja1) * 0.44f;                 // ~4 kHz
        lpCaja2 += (y - lpCaja2) * 0.16f;                 // ~1.2 kHz
        lpHat   += (y - lpHat)   * 0.62f;                 // ~7 kHz
        seguir(segKick, lpKick, muestraGlobal);
        seguir(segCaja, lpCaja1 - lpCaja2, muestraGlobal);
        seguir(segHat,  y - lpHat, muestraGlobal);
      }
      if (p == 3) {                                       // «otros» abierto con un retardo de Haas
        float d = haas[(haasIdx - haasN + HAAS_MAX) & (HAAS_MAX - 1)];
        haas[haasIdx] = y; haasIdx = (haasIdx + 1) & (HAAS_MAX - 1);
        l += y; r += 0.85f * d + 0.15f * y;
      } else { l += y; r += y; }
    }
    if (conPulso) { enPulso += 1.0f; if (enPulso >= Lb) enPulso -= Lb; }
    l *= MASTER; r *= MASTER;

    // bloqueador de DC
    float yl = l - dcX1L + 0.9985f * dcY1L; dcX1L = l; dcY1L = yl; l = yl;
    float yr = r - dcX1R + 0.9985f * dcY1R; dcX1R = r; dcY1R = yr; r = yr;

    // limitador con lookahead (el de drum_poder)
    float pk = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
    if (pk > limEnv) limEnv = pk; else limEnv = pk + (limEnv - pk) * limRel;
    float limObj = (limEnv > TECHO) ? (TECHO / limEnv) : 1.0f;
    limGain += (limObj - limGain) * 0.020f;
    float dl = limDlyL[limDlyIdx], dr = limDlyR[limDlyIdx];
    limDlyL[limDlyIdx] = l; limDlyR[limDlyIdx] = r;
    limDlyIdx++; if (limDlyIdx >= LIM_LOOK) limDlyIdx = 0;
    l = techoSuave(dl * limGain); r = techoSuave(dr * limGain);

    int32_t li = (int32_t)(l * 32000.0f), ri = (int32_t)(r * 32000.0f);
    salida[2 * n]     = (int16_t)(li > 32767 ? 32767 : li < -32768 ? -32768 : li);
    salida[2 * n + 1] = (int16_t)(ri > 32767 ? 32767 : ri < -32768 ? -32768 : ri);
    muestraGlobal++;
  }
  for (int p = 0; p < NUM_PISTAS; p++) { nivelPista[p] = seg[p].nivel; golpesPista[p] = seg[p].golpes; }
  golpesKick = segKick.golpes; golpesCaja = segCaja.golpes; golpesHat = segHat.golpes;
}

// ==============================================================================================================================================
// TARJETA
// ==============================================================================================================================================
SPIClass spiSD(FSPI);
File     archivo;
uint32_t datosInicio = 0, datosBytes = 0;
uint8_t *lecturaBuf = nullptr;                   // interna: la tarjeta escribe más rápido acá que en PSRAM

uint32_t leerU32(const uint8_t *b) { return b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24); }
uint16_t leerU16(const uint8_t *b) { return b[0] | (b[1] << 8); }

// Recorre los trozos del WAV hasta «fmt » y «data» (ffmpeg mete un LIST entre medio)
bool abrirWav(const char *ruta) {
  if (archivo) archivo.close();
  archivo = SD.open(ruta, FILE_READ);
  if (!archivo) { Serial.printf("No está %s en la tarjeta\n", ruta); return false; }
  uint8_t h[12];
  if (archivo.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { Serial.println("cancion.wav no es un WAV"); return false; }
  bool fmtOk = false;
  while (archivo.available()) {
    uint8_t c[8];
    if (archivo.read(c, 8) != 8) break;
    uint32_t largo = leerU32(c + 4), pos = archivo.position();
    if (!memcmp(c, "fmt ", 4)) {
      uint8_t f[16]; archivo.read(f, 16);
      uint16_t canales = leerU16(f + 2), bits = leerU16(f + 14);
      uint32_t tasa = leerU32(f + 4);
      if (canales != NUM_PISTAS || bits != 16 || tasa != SAMPLE_RATE) {
        Serial.printf("cancion.wav tiene %u canales, %u bit, %u Hz: tiene que ser 4 canales, 16 bit, 44100 Hz\n", canales, bits, tasa);
        return false;
      }
      fmtOk = true;
    } else if (!memcmp(c, "data", 4)) {
      datosInicio = pos; datosBytes = largo;
      if (datosBytes == 0 || datosBytes > archivo.size() - pos) datosBytes = archivo.size() - pos;   // WAV de streaming
      totalFrames = datosBytes / (2 * NUM_PISTAS);
      return fmtOk && archivo.seek(datosInicio);
    }
    archivo.seek(pos + largo + (largo & 1));
  }
  Serial.println("cancion.wav sin bloque de datos");
  return false;
}

void leerInfo(const char *ruta) {
  bpm = 120.0f; primerPulso = 0; titulo[0] = 0; artista[0] = 0;
  File f = SD.open(ruta, FILE_READ);
  if (!f) return;
  while (f.available()) {
    String linea = f.readStringUntil('\n'); linea.trim();
    int eq = linea.indexOf('=');
    if (linea.startsWith("#") || eq < 0) continue;
    String k = linea.substring(0, eq), v = linea.substring(eq + 1); k.trim(); v.trim();
    if (k == "bpm") bpm = v.toFloat();
    else if (k == "primer_pulso_ms") primerPulso = (uint32_t)(v.toFloat() * SAMPLE_RATE / 1000.0f);
    else if (k == "titulo") v.toCharArray(titulo, sizeof(titulo));
    else if (k == "artista") v.toCharArray(artista, sizeof(artista));
  }
  f.close();
  if (bpm < 40 || bpm > 250) bpm = 120;
}

uint32_t framesEnArchivo = 0;                    // cuadros ya leídos desde datosInicio

// Carga la canción i: su .txt (bpm, primer pulso, título) y su .wav. Sólo se llama con el audio detenido.
bool cargarCancion(int i) {
  char ruta[24];
  snprintf(ruta, sizeof(ruta), "%s.txt", cancionBase[i]); leerInfo(ruta);
  snprintf(ruta, sizeof(ruta), "%s.wav", cancionBase[i]);
  if (!titulo[0]) strncpy(titulo, cancionBase[i] + 1, sizeof(titulo) - 1);
  if (!abrirWav(ruta)) return false;
  framesEnArchivo = 0; escritos = leidos; base = leidos;
  infoNueva = true;
  Serial.printf("Canción %d de %d: «%s» de %s · %.2f BPM · %u s\n", i + 1, numCanciones, titulo, artista, bpm, totalFrames / SAMPLE_RATE);
  return true;
}

bool iniciarSD() {
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD, SD_FREQ_HZ)) { Serial.println("Sin tarjeta microSD (revisa SCK 14 · MOSI 15 · MISO 16 · CS 17)"); return false; }
  numCanciones = 0;
  for (int i = -1; i <= 9 && numCanciones < MAX_CANCIONES; i++) {
    char base[16], ruta[24];
    if (i < 0) strcpy(base, "/cancion"); else snprintf(base, sizeof(base), "/cancion%d", i);
    snprintf(ruta, sizeof(ruta), "%s.wav", base);
    if (SD.exists(ruta)) strcpy(cancionBase[numCanciones++], base);
  }
  if (!numCanciones) { Serial.println("No hay /cancion.wav (ni cancion1..9.wav) en la tarjeta"); return false; }
  Serial.printf("%d canción(es) en la tarjeta\n", numCanciones);
  cancionActual = 0;
  return cargarCancion(0);
}

// Una pasada de la lectora: rellena el colchón mientras haya espacio. Al final del archivo vuelve
// al principio sin que el audio se entere: la canción queda en loop, pegada a la muestra.
bool pasoLectora() {
  if (!sdLista) return false;
  if (pedidoCancion && !sonando) {               // siguiente canción: sólo con el audio detenido
    int sig = (cancionActual + 1) % numCanciones;
    if (cargarCancion(sig)) cancionActual = sig; else cargarCancion(cancionActual);
    pedidoCancion = false; pedidoInicio = false;
  }
  if (pedidoInicio && !sonando) {                // volver al inicio: sólo con el audio detenido
    archivo.seek(datosInicio); framesEnArchivo = 0;
    escritos = leidos; base = leidos;
    pedidoInicio = false;
  }
  uint32_t libres = RING_FRAMES - (escritos - leidos);
  if (libres < LECTURA_FRAMES) return false;
  uint32_t quedan = totalFrames - framesEnArchivo;
  uint32_t n = quedan < LECTURA_FRAMES ? quedan : LECTURA_FRAMES;
  size_t bytes = n * NUM_PISTAS * 2;
  size_t got = archivo.read(lecturaBuf, bytes);
  if (got != bytes) { sdError = true; archivo.seek(datosInicio + framesEnArchivo * NUM_PISTAS * 2); return false; }
  sdError = false;
  // copiar al anillo (puede dar la vuelta)
  uint32_t w = escritos % RING_FRAMES, primero = (RING_FRAMES - w) < n ? (RING_FRAMES - w) : n;
  memcpy(ring + w * NUM_PISTAS, lecturaBuf, primero * NUM_PISTAS * 2);
  if (primero < n) memcpy(ring, lecturaBuf + primero * NUM_PISTAS * 2, (n - primero) * NUM_PISTAS * 2);
  escritos = escritos + n;
  framesEnArchivo += n;
  if (framesEnArchivo >= totalFrames) { archivo.seek(datosInicio); framesEnArchivo = 0; }
  return true;
}

void lectoraTask(void *) { for (;;) { bool trabajo = pasoLectora(); vTaskDelay(trabajo ? 1 : 4); } }

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

void audioTask(void *) {
  static int16_t entrada[BUFFER_SAMPLES * NUM_PISTAS];
  static int16_t salida[BUFFER_SAMPLES * 2];
  size_t n;
  for (;;) {
    // consume del colchón mientras suena o mientras se apaga el fundido
    bool consumir = sdLista && (reproducir || fundido > 0.0005f);
    const int16_t *src = nullptr;
    if (consumir) {
      if (escritos - leidos >= BUFFER_SAMPLES) {
        uint32_t r = leidos % RING_FRAMES, primero = (RING_FRAMES - r) < BUFFER_SAMPLES ? (RING_FRAMES - r) : BUFFER_SAMPLES;
        memcpy(entrada, ring + r * NUM_PISTAS, primero * NUM_PISTAS * 2);
        if (primero < BUFFER_SAMPLES) memcpy(entrada + primero * NUM_PISTAS, ring, (BUFFER_SAMPLES - primero) * NUM_PISTAS * 2);
        uint32_t t = totalFrames ? totalFrames : 1;
        posBloque = (leidos - base) % t;
        leidos = leidos + BUFFER_SAMPLES;
        src = entrada;
      } else if (reproducir) faltantes = faltantes + 1;
    }
    procesarBloque(src, salida, posBloque);
    sonando = consumir && src;
    i2s_channel_write(tx_chan, salida, sizeof(salida), &n, portMAX_DELAY);
  }
}

// posición dentro de la canción, en cuadros
uint32_t posicion() { uint32_t t = totalFrames ? totalFrames : 1; return (leidos - base) % t; }

// ==============================================================================================================================================
// CONTROL (core 0, 1 kHz): botones, pots, IMU, piezos, LEDs y la línea para las visuales
// ==============================================================================================================================================
CRGB leds[NUM_LEDS];
uint8_t imuAddr = 0x68;
float ax = 0, ay = 0;                            // inclinación suavizada, en g
bool  btnAbajo[5] = { false, false, false, false, false };
unsigned long btnDesde[5] = { 0, 0, 0, 0, 0 }, btnCambio[5] = { 0, 0, 0, 0, 0 };
bool  yaPlay = false, yaVolvio = false;
float potFis[4] = { 0, 0, 0, 0 };                // posición de cada perilla, 0..1, en orden físico
float potRef[4] = { 0, 0, 0, 0 };                // dónde estaba cada perilla al entrar al banco
bool  tomado[4] = { true, true, true, true };    // ¿la perilla ya tomó el control en este banco?
uint32_t golpesPiezo = 0; float velPiezo = 0;
int   piezoPico[4] = { 0, 0, 0, 0 }; unsigned long piezoDesde[4] = { 0, 0, 0, 0 }, piezoUltimo[4] = { 0, 0, 0, 0 };
const CRGB COLOR_PISTA[NUM_PISTAS] = { CRGB(255, 70, 20), CRGB(160, 40, 255), CRGB(0, 200, 255), CRGB(212, 255, 58) };

void iniciarIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Wire.beginTransmission(0x68); if (Wire.endTransmission() != 0) imuAddr = 0x69;
  Wire.beginTransmission(imuAddr); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(true);   // despertar
  Wire.beginTransmission(imuAddr); Wire.write(0x1C); Wire.write(0x00); Wire.endTransmission(true);   // ±2 g
}

bool imuVivo = true;
void leerIMU() {                                 // como test_imu: se mira que lleguen los 14 bytes, no el WHO_AM_I
  static unsigned long reintento = 0;
  if (!imuVivo && millis() - reintento < 3000) return;   // sin IMU: se reintenta cada 3 s, no 200 veces por segundo
  Wire.beginTransmission(imuAddr); Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom((int)imuAddr, 14, true);
  if (Wire.available() < 14) {
    if (imuVivo) Serial.println("IMU MPU6050 no responde: el filtro por inclinación queda quieto");
    imuVivo = false; reintento = millis(); iniciarIMU(); return;
  }
  if (!imuVivo) Serial.println("IMU MPU6050 de vuelta");
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
  for (int i = 0; i < 4; i++) { potRef[i] = potFis[i]; tomado[i] = false; }   // nada salta hasta que se mueva
}

void pasoBotones(unsigned long t) {
  for (int b = 0; b < 5; b++) {
    bool abajo = digitalRead(BTN_PIN[b]) == LOW;
    if (abajo == btnAbajo[b] || t - btnCambio[b] < 25) continue;   // antirrebote
    btnCambio[b] = t; btnAbajo[b] = abajo;
    if (abajo) {
      btnDesde[b] = t;
      if (b == 0) { yaPlay = false; yaVolvio = false; }
      if (b != banco) elegirBanco(b);            // en el flanco: BTN1 general, BTN2..5 su instrumento
    }
  }
  // BTN1 mantenido: medio segundo = play/stop · 2,5 s = volver al inicio (en stop)
  if (btnAbajo[0] && !yaPlay && t - btnDesde[0] > PLAY_MS) { reproducir = !reproducir; yaPlay = true; }
  if (btnAbajo[0] && !yaVolvio && t - btnDesde[0] > VOLVER_MS) {   // 2,5 s: siguiente canción (con una sola, vuelve al inicio)
    reproducir = false; yaVolvio = true;
    if (numCanciones > 1) pedidoCancion = true; else pedidoInicio = true;
  }
}

// Las 4 perillas según el banco. Una perilla sólo escribe cuando ya «tomó» el control (se movió).
void aplicarPerilla(int i) {
  if (!tomado[i]) { if (fabsf(potFis[i] - potRef[i]) > TOMA_PERILLA) tomado[i] = true; else return; }
  float v = potFis[i];
  if (banco == 0) { vol[GENERAL_PISTA[i]] = v; return; }
  int p = BANCO_PISTA[banco - 1];
  if (i == 0) {                                   // FILTRO: centro abierto, con zona muerta
    float f = v * 2.0f - 1.0f;
    f = fabsf(f) < 0.06f ? 0.0f : (f > 0 ? (f - 0.06f) / 0.94f : (f + 0.06f) / 0.94f);
    filtro[p] = f;
  } else if (i == 1) reso[p] = v;
  else if (i == 2) {                              // EFECTO por zonas, con histéresis para que no titile entre dos
    float z = v * 6.999f; int act = efecto[p];
    if (z < act - 0.15f || z > act + 1.15f) efecto[p] = (int)z;
  } else intensidad[p] = v;
}
// Piezos: ventana de pico de 12 ms tras cruzar el umbral, luego 60 ms sin volver a disparar
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
  float porPulso = SR * 60.0f / bpm;
  float x = ((float)pos - (float)primerPulso) / porPulso;
  if (x < 0) { compas = 0; return 0; }
  compas = (uint32_t)x;
  return x - compas;
}

void renderLEDs(unsigned long t) {
  if (!sdLista) leds[0] = (t % 600) < 300 ? CRGB(200, 0, 160) : CRGB::Black;
  else if (sdError) leds[0] = CRGB(255, 90, 0);
  else {
    static uint32_t faltVistas = 0; static unsigned long naranjoHasta = 0;
    if (faltantes != faltVistas) { faltVistas = faltantes; naranjoHasta = t + 1500; }
    leds[0] = t < naranjoHasta ? CRGB(255, 90, 0) : reproducir ? CRGB(0, 150, 40) : CRGB(150, 100, 0);
  }
  bool parpadeo = (t % 400) < 200;
  if (banco == 0 && parpadeo) leds[0] = CRGB(200, 200, 200);
  for (int k = 0; k < 4; k++) {                  // LED k+1 = el instrumento del BTN k+2
    int p = BANCO_PISTA[k];
    if (banco == k + 1 && parpadeo) { leds[1 + k] = CRGB(220, 220, 220); continue; }
    float n = nivelPista[p] * 2.5f; if (n > 1) n = 1;
    CRGB c = COLOR_PISTA[p]; c.nscale8((uint8_t)(25 + 230 * n)); leds[1 + k] = c;
  }
  uint32_t compas; float fase = pulsoFase(posicion(), compas);
  float brillo = reproducir ? expf(-fase * 7.0f) : 0.0f;
  CRGB c5 = (compas % 4 == 0) ? CRGB(212, 255, 58) : CRGB(255, 255, 255); c5.nscale8((uint8_t)(brillo * 255)); leds[5] = c5;
  FastLED.show();
}

void enviarLinea(unsigned long t) {
  static unsigned long ultInfo = 0;
  char b[360];
  if (t - ultInfo > 5000 || infoNueva) {
    ultInfo = t; infoNueva = false;
    snprintf(b, sizeof(b), "I,%s,%s,%.2f,%lu,%d,%d\n", titulo, artista, bpm, (unsigned long)(totalFrames / (SAMPLE_RATE / 1000)),
             cancionActual + 1, numCanciones);
    Serial.print(b);
  }
  uint32_t pos = posicion(), compas;
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
    potFis[scan] = leerPot(POT_PIN[scan]); aplicarPerilla(scan); scan = (scan + 1) & 3;   // un pot por pasada
    if (t - ultIMU >= 5)   { ultIMU = t; leerIMU(); }
    if (t - ultLED >= 20)  { ultLED = t; renderLEDs(t); }
    if (t - ultLinea >= 16){ ultLinea = t; enviarLinea(t); }
    vTaskDelay(1);
  }
}

// ==============================================================================================================================================
// SETUP
// ==============================================================================================================================================
// Diagnóstico de arranque: cada paso enciende un LED en azul y lo imprime por Serial. Si la placa
// se queda pegada o se reinicia, el último LED azul dice en qué paso fue, sin necesitar el computador.
//   LED0 LEDs ok · LED1 memoria PSRAM · LED2 IMU · LED3 tarjeta (verde = canción encontrada, magenta = no)
//   LED4 colchón lleno · LED5 audio en marcha. Después la tarea de control toma los LEDs.
void etapa(int n, CRGB c, const char *msg) {
  leds[n] = c; FastLED.show();
  Serial.printf("[arranque %d] %s\n", n, msg);
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // OBLIGATORIO con audio: un print por USB CDC bloquea hasta que el PC lea
  esp_log_level_set("i2c.master", ESP_LOG_NONE);   // sin IMU, el driver llenaba el puerto de errores y tapaba los datos
  delay(1500);                // tiempo para que el PC abra el puerto y vea el arranque

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRILLO);
  FastLED.clear(); FastLED.show();
  etapa(0, CRGB(0, 0, 160), "LEDs ok");

  for (int b = 0; b < 5; b++) pinMode(BTN_PIN[b], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  for (int i = 0; i < 4; i++) { potFis[i] = leerPot(POT_PIN[i]); vol[GENERAL_PISTA[i]] = potFis[i]; }   // arranca en el banco general

  ring       = (int16_t *)heap_caps_malloc(RING_FRAMES * NUM_PISTAS * 2, MALLOC_CAP_SPIRAM);
  lecturaBuf = (uint8_t *)heap_caps_malloc(LECTURA_FRAMES * NUM_PISTAS * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  hist       = (int16_t *)heap_caps_calloc(NUM_PISTAS * HIST, 2, MALLOC_CAP_SPIRAM);
  if (!ring || !lecturaBuf || !hist) {
    // Sin PSRAM (¿«PSRAM: OPI PSRAM» en el IDE?): LEDs rojos parpadeando y nada más
    Serial.printf("Sin memoria: PSRAM libre %u bytes. ¿Elegiste «PSRAM: OPI PSRAM» al compilar?\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    for (;;) { fill_solid(leds, NUM_LEDS, (millis() / 300) % 2 ? CRGB(120, 0, 0) : CRGB::Black); FastLED.show(); delay(50); }
  }
  etapa(1, CRGB(0, 0, 160), "memoria PSRAM ok");

  prepararDSP();
  iniciarIMU();
  etapa(2, CRGB(0, 0, 160), "IMU iniciado");

  sdLista = iniciarSD();
  etapa(3, sdLista ? CRGB(0, 160, 0) : CRGB(160, 0, 140), sdLista ? "tarjeta y cancion.wav ok" : "SIN tarjeta o sin /cancion.wav");
  if (sdLista) { while (escritos < RING_FRAMES / 2 && pasoLectora()) {} }   // colchón medio lleno antes de partir
  etapa(4, CRGB(0, 0, 160), "colchón listo");

  i2s_init();
  reproducir = sdLista;       // arranca sonando: sin tener que apretar BTN1
  etapa(5, CRGB(0, 0, 160), "audio en marcha");
  delay(300);
  xTaskCreatePinnedToCore(audioTask,   "audio",   8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask, "control", 6144, NULL,  3, NULL, 0);
  xTaskCreatePinnedToCore(lectoraTask, "lectora", 6144, NULL,  2, NULL, 0);
}

void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }
