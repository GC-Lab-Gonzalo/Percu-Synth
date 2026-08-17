// ==============================================================================================================================================
// PERCU-SYNTH — ESPACIO MODULAR · Ambientes de película (IMU) — GC Lab Chile
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
// - ESP32 Arduino core >= 3.x (incluye driver/i2s_std.h)
// - Wire.h (I2C, incluida en el core) — para el MPU6050
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Máquina de ambientes de película. UNA SOLA VOZ monofónica sobre un dron continuo,
// con el espacio como protagonista. Un solo panel: 5 botones y 4 pots, cada uno con
// una función y nada más. No hay combos, no hay paneles ocultos, no hay pots que
// cambien de significado ni que se congelen.
//
// LOS 24 PATRONES SON TEMAS, NO FIGURAS. Ésa es la diferencia entre música de
// película y un arpegio. Un tema se define tanto por su RITMO LARGO-CORTO como por
// sus notas, y usa intervalos que significan algo:
//   · la subida 1 → 5 → 8 (la llamada de trompa, el gesto heroico)
//   · el descenso 8 → 7 → 6 → 5 (el lamento)
//   · el suspiro 6 → 5 (la apoyatura que duele)
//   · el pedal: una nota que insiste mientras una sombra entra por debajo
// Y sobre todo: CADA NOTA DURA HASTA LA SIGUIENTE. Los silencios del patrón no son
// silencios, son la duración de la nota anterior. Por eso hay notas de cuatro
// tiempos y notas de medio, que es lo que convierte una lista de alturas en un tema.
//
// El resto de la música:
//   · Un DRON continuo sobre la tónica (fundamental + quinta + octava abajo) que no
//     para nunca y pasa por el mismo filtro. Es el pedal sobre el que se apoya una
//     escena. Como ya está, los patrones no necesitan bajo propio y la voz queda
//     libre para cantar.
//   · ARMONÍA MUY LENTA: la progresión tiene 4 tramos y cada tramo dura un ciclo
//     entero de 4 compases (de 9 a 38 segundos por acorde según el tempo).
//   · PROGRESIONES NO FUNCIONALES: nada de i-VI-III-VII ni de cadencias V-i. La
//     tónica ocupa la mitad del ciclo y lo que ocurre es un desplazamiento modal.
//   · Los valores del patrón son grados RELATIVOS a la fundamental del tramo, así
//     que el mismo tema se transporta. Todo se calcula en GRADOS de la escala, o sea
//     que cada nota está en tono por construcción.
//
// El FILTRO es 100 % tuyo: solo lo tocan el POT3 y el IMU. Nada automático lo mueve.
// Suena grande siendo mono porque el ECO PING-PONG mantiene sonando las notas
// anteriores cuando llega la siguiente (truco de la escuela de Berlín / Eno).
//
// Sin LEDs a propósito: todo el presupuesto de CPU va al audio.
// ==============================================================================================================================================
// FUNCIONAMIENTO — un solo panel, todo directo
// ==============================================================================================================================================
// LOS 4 POTS — siempre lo mismo, siempre vivos, sin congelarse nunca:
//   · POT1 (ADC1)  → VOLUMEN
//   · POT2 (ADC2)  → TEMPO   (25 – 110 BPM, un paso = corchea). Un ciclo son 4
//                    compases = 32 pasos, y cada tramo de la progresión dura un ciclo
//                    entero, así que abajo del todo un acorde dura ~38 s.
//   · POT3 (ADC8)  → FILTRO  (corte de 80 Hz a 8 kHz, escala exponencial)
//   · POT4 (ADC10) → ESPACIO (eco ping-pong + reverb, de seco a nebulosa)
//
// EL IMU — un solo eje: la aceleración en X multiplica el corte del filtro hasta
// x8 (tres octavas) por encima de donde tengas el POT3. Inclinar = barrido.
//
// LOS 5 BOTONES — cada uno se oye en el acto:
//   · BTN1 (44) → Play / Stop. En cada Play se SORTEA EL SENTIMIENTO: sale un modo
//                 nuevo (y con él su progresión), nunca el que acababa de sonar.
//   · BTN2 (42) → TEMA AL AZAR: otro de los 24, manteniendo el sentimiento. Tampoco
//                 repite el que estaba sonando.
//   · BTN3 (0)  → MODO en orden, para recorrerlos a propósito cuando el azar te deja
//                 en uno que te gusta y quieres oír los vecinos.
//   · BTN4 (45) → TONALIDAD: sube la tónica una cuarta justa (ciclo de cuartas)
//   · BTN5 (47) → OCTAVA base (-2 → -1 → 0 → +1 respecto a C3)
//
// LOS 8 MODOS son el "sentimiento", y la paleta es toda cinematográfica — oscura,
// épica, fantástica, nórdica y árabe:
//   0 EÓLICO   épico melancólico          4 LIDIO           fantástico, de maravilla
//   1 FRIGIO   oscuro, amenazante         5 ÁRABE           frigio dominante / hijaz
//   2 NÓRDICO  folk vikingo/celta         6 BIZANTINO       doble armónica
//   3 MIXOLIDIO heroico, de aventura      7 MENOR ARMÓNICA  épico dramático
//
// LOS 24 TEMAS, agrupados por gesto:
//   LLAMADAS (notas largas, intervalos abiertos): LLAMADA · JURAMENTO · ESTANDARTE · CUMBRE
//   LAMENTOS (el descenso, la nota que duele):    LAMENTO · SUSPIRO · CAÍDA · DUELO
//   PEDALES  (una nota que insiste):              VIGILIA · SOMBRA · PLEGARIA · ORÁCULO
//   CIMIENTOS (con el golpe grave):               CIMIENTO · TITÁN · ABISMO · RUINA
//   ARCOS    (pregunta y respuesta):              ARCO · TRAVESÍA · REGRESO · PROMESA
//   MOVIDOS  (cuando la escena avanza):           CABALGATA · TORMENTA · RITUAL · ASEDIO
//
// EL TIMBRE ES FIJO y no se toca desde los controles: onda diente de sierra, todas
// las capas encendidas (sub + quinta + octava), dron a nivel medio, envolvente de
// pad y resonancia media. Si alguna vez quieres cambiarlo, están todos juntos en el
// bloque "TIMBRE FIJO" más abajo, cada uno en una línea.
//
// POR SERIAL (115200) sale una línea cada vez que aprietas un botón, con el modo, el
// tema y la tonalidad que están sonando. Es para que cuando algo te guste sepas qué
// era. Se apaga poniendo MOSTRAR_ESTADO en 0.
//
// MODO DE USO:
// 1. BTN1 arranca con un sentimiento sorteado. Espera: la armonía se mueve cada 4
//    compases. Si no te convence, BTN1 dos veces y sale otro.
// 2. POT3 (filtro) es el control más audible; POT4 (espacio) hacia arriba.
// 3. Con el sentimiento puesto, BTN2 va sorteando temas hasta que uno enganche.
// 4. BTN3 recorre los modos en orden; BTN4 mueve la tonalidad.
// 5. Inclina la placa: el eje X abre el filtro.
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
#define SDA_PIN     21
#define SCL_PIN     38
#define IMU_ADDR    0x68
const unsigned long IMU_READ_INTERVAL = 10;
const float IMU_FILTER_ALPHA = 0.15f;

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
#define BTN1   44
#define BTN2   42
#define BTN3    0
#define BTN4   45
#define BTN5   47
const unsigned long DEBOUNCE_MS = 180;

// ─── Potenciómetros ────────────────────────────────────────
#define POT1    1
#define POT2    2
#define POT3    8
#define POT4   10

// ══════════════════════════════════════════════════════════════════════════════
//  TIMBRE FIJO — no se toca desde los controles. Todo lo que antes vivía en el
//  Panel 2 está aquí, cada cosa en una línea. Cámbialo aquí si alguna vez quieres.
// ══════════════════════════════════════════════════════════════════════════════
const int   ONDA      = 0;        // 0 sierra · 1 pulso · 2 triangular · 3 seno
const float ATAQUE    = 0.15f;    // s
const float COLA      = 2.20f;    // s (release; las notas se solapan por la cola)
const float GLIDE     = 0.06f;    // s de portamento entre notas
const float LARGO     = 1.00f;    // 1.0 = cada nota dura EXACTAMENTE hasta la siguiente
const float Q_FIJA    = 2.20f;    // resonancia del filtro
const float DRON_NIV  = 0.60f;    // nivel del dron (0 = apagado)
// Capas del banco de osciladores: principal + 2 gemelos + sub + quinta + octava
const float NIVEL_OSC[6] = { 1.00f, 0.62f, 0.62f, 0.85f, 0.40f, 0.30f };
const float DETUNE    = 8.0f;     // cents entre los gemelos

// ══════════════════════════════════════════════════════════════════════════════
//  TIPOS (arriba de todo: el preprocesador del .ino inserta los prototipos justo
//  antes de la PRIMERA función, así que los structs de las firmas van antes)
// ══════════════════════════════════════════════════════════════════════════════
struct Biquad   { float b0, b1, b2, a1, a2; float x1, x2, y1, y2; };
struct BtnState { uint8_t pin; bool last; unsigned long lastPress; };

// ══════════════════════════════════════════════════════════════════════════════
//  AZAR (xorshift32). Solo elige entre opciones que ya son válidas — qué modo, qué
//  tema —, nunca inventa notas.
// ══════════════════════════════════════════════════════════════════════════════
uint32_t rngState = 0x1F35A7C3;

inline uint32_t rnd32() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}
// El instante EXACTO en que aprietas el botón es la mejor fuente de entropía que hay
// en una placa sin reloj: se mezcla en cada sorteo.
inline int sortear(int n) {
  rngState ^= (uint32_t)micros() * 2654435761u;
  return (int)(rnd32() % (uint32_t)n);
}
inline int sortearDistinto(int n, int actual) {
  if (n < 2) return 0;
  int v = sortear(n - 1);
  return (v >= actual) ? v + 1 : v;
}

// ══════════════════════════════════════════════════════════════════════════════
//  MÚSICA
// ══════════════════════════════════════════════════════════════════════════════
#define NUM_MODOS 8
const int8_t MODE_INT[NUM_MODOS][7] = {
  {0, 2, 3, 5, 7, 8, 10},   // 0 EÓLICO             menor natural — épico melancólico
  {0, 1, 3, 5, 7, 8, 10},   // 1 FRIGIO             oscuro, amenazante
  {0, 2, 3, 5, 7, 9, 10},   // 2 NÓRDICO (dórico)   folk vikingo/celta
  {0, 2, 4, 5, 7, 9, 10},   // 3 MIXOLIDIO          heroico, de aventura
  {0, 2, 4, 6, 7, 9, 11},   // 4 LIDIO              fantástico, de maravilla
  {0, 1, 4, 5, 7, 8, 10},   // 5 ÁRABE              frigio dominante / hijaz
  {0, 1, 4, 5, 7, 8, 11},   // 6 BIZANTINO          doble armónica
  {0, 2, 3, 5, 7, 8, 11}    // 7 MENOR ARMÓNICA     épico dramático
};

// Una progresión por modo. Grados del modo (0 = tónica), 4 tramos, y CADA TRAMO DURA
// UN CICLO ENTERO de 4 compases. No son funcionales: la tónica ocupa la mitad y el
// movimiento es un desplazamiento modal, una sombra que entra y se va. Todas usan
// solo tríadas consonantes del modo (en estas escalas exóticas aparecen disminuidas
// y aumentadas que suenan a error).
const uint8_t PROG[NUM_MODOS][4] = {
  {0, 0, 5, 0},   // 0 EÓLICO         i .... VI ... i
  {0, 0, 1, 0},   // 1 FRIGIO         i .... II ... i
  {0, 0, 6, 3},   // 2 NÓRDICO        i .... VII .. IV
  {0, 0, 3, 6},   // 3 MIXOLIDIO      I .... IV ... VII
  {0, 0, 1, 0},   // 4 LIDIO          I .... II ... I
  {0, 0, 1, 0},   // 5 ÁRABE          I .... II ... I
  {0, 0, 3, 0},   // 6 BIZANTINO      I .... iv ... I
  {0, 0, 3, 0}    // 7 MENOR ARMÓNICA i .... iv ... i
};

// ── LOS 24 TEMAS ──────────────────────────────────────────────────────────────
// 32 pasos de corchea = 4 compases de 4/4. Los valores son GRADOS RELATIVOS a la
// fundamental del tramo: 1 = fundamental, 5 = quinta, 8 = octava, y 2·4·6·7 son los
// grados intermedios. 9 = fundamental dos octavas abajo (golpe grave).
//
// EL 0 NO ES SILENCIO: es la CONTINUACIÓN de la nota anterior. Cada nota suena hasta
// que llega la siguiente, así que la separación entre valores ES la duración. De ahí
// salen las notas de cuatro tiempos y las de medio — el ritmo largo-corto que
// convierte una lista de alturas en un tema.
//
// Los temas usan a propósito los grados 2, 4, 6 y 7 además de los del acorde: la nota
// que define un modo casi nunca es del acorde (la 2ª bemol del frigio, el #4 del
// lidio, el 7 bemol del mixolidio, el si natural de la doble armónica).
#define NUM_TEMAS 24
#define PASOS_CICLO 32
const uint8_t TEMA[NUM_TEMAS][PASOS_CICLO] = {
  // ─── LLAMADAS: notas largas e intervalos abiertos. El gesto heroico.
  /* 0 LLAMADA    */ {1,0,0,0,0,0,0,0, 5,0,0,0,0,0,0,0, 8,0,0,0,0,0,0,0, 5,0,0,0,0,0,0,0},
  /* 1 JURAMENTO  */ {1,0,0,0,5,0,0,0, 8,0,0,0,0,0,0,0, 6,0,0,0,5,0,0,0, 1,0,0,0,0,0,0,0},
  /* 2 ESTANDARTE */ {1,0,1,0,5,0,0,0, 8,0,0,0,0,0,0,0, 5,0,0,0,8,0,0,0, 5,0,0,0,0,0,0,0},
  /* 3 CUMBRE     */ {5,0,0,0,0,0,0,0, 8,0,0,0,0,0,0,0, 0,0,0,0,6,0,0,0, 5,0,0,0,0,0,0,0},

  // ─── LAMENTOS: el descenso, la nota que duele.
  /* 4 LAMENTO    */ {8,0,0,0,0,0,7,0, 6,0,0,0,0,0,0,0, 5,0,0,0,0,0,4,0, 3,0,0,0,0,0,0,0},
  /* 5 SUSPIRO    */ {6,0,5,0,0,0,0,0, 0,0,0,0,0,0,0,0, 6,0,5,0,0,0,0,0, 3,0,0,0,0,0,0,0},
  /* 6 CAIDA      */ {8,0,0,7,0,0,6,0, 5,0,0,0,0,0,0,0, 4,0,0,3,0,0,2,0, 1,0,0,0,0,0,0,0},
  /* 7 DUELO      */ {5,0,0,0,6,0,0,0, 5,0,0,0,0,0,0,0, 4,0,0,0,3,0,0,0, 2,0,0,0,0,0,0,0},

  // ─── PEDALES: una nota que insiste mientras entra la sombra.
  /* 8 VIGILIA    */ {1,0,0,0,1,0,0,0, 2,0,0,0,1,0,0,0, 1,0,0,0,1,0,0,0, 7,0,0,0,1,0,0,0},
  /* 9 SOMBRA     */ {2,0,0,0,1,0,0,0, 0,0,0,0,0,0,0,0, 2,0,0,0,1,0,0,0, 7,0,0,0,0,0,0,0},
  /*10 PLEGARIA   */ {5,0,0,0,0,0,0,0, 5,0,0,0,6,0,0,0, 5,0,0,0,0,0,0,0, 4,0,0,0,3,0,0,0},
  /*11 ORACULO    */ {1,0,0,0,2,0,0,0, 3,0,0,0,2,0,0,0, 1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},

  // ─── CIMIENTOS: con el golpe grave.
  /*12 CIMIENTO   */ {9,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 5,0,0,0,0,0,0,0, 8,0,0,0,0,0,0,0},
  /*13 TITAN      */ {9,0,0,0,0,0,0,0, 5,0,0,0,0,0,0,0, 9,0,0,0,0,0,0,0, 6,0,0,0,5,0,0,0},
  /*14 ABISMO     */ {9,0,0,0,0,0,0,0, 0,0,0,0,2,0,0,0, 1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
  /*15 RUINA      */ {9,0,0,0,0,0,0,0, 4,0,0,0,3,0,0,0, 9,0,0,0,0,0,0,0, 2,0,0,0,1,0,0,0},

  // ─── ARCOS: pregunta y respuesta a lo largo de los 4 compases.
  /*16 ARCO       */ {1,0,0,0,3,0,0,0, 5,0,0,0,0,0,0,0, 8,0,0,0,6,0,0,0, 5,0,0,0,0,0,0,0},
  /*17 TRAVESIA   */ {1,0,0,0,2,0,0,0, 4,0,0,0,5,0,0,0, 7,0,0,0,8,0,0,0, 5,0,0,0,0,0,0,0},
  /*18 REGRESO    */ {8,0,0,0,6,0,0,0, 5,0,0,0,3,0,0,0, 2,0,0,0,4,0,0,0, 1,0,0,0,0,0,0,0},
  /*19 PROMESA    */ {3,0,0,0,5,0,0,0, 4,0,0,0,3,0,0,0, 5,0,0,0,8,0,0,0, 5,0,0,0,0,0,0,0},

  // ─── MOVIDOS: cuando la escena tiene que avanzar.
  /*20 CABALGATA  */ {1,0,5,0,1,0,5,0, 6,0,5,0,3,0,1,0, 1,0,5,0,1,0,5,0, 7,0,5,0,2,0,1,0},
  /*21 TORMENTA   */ {1,0,2,0,3,0,5,0, 6,0,5,0,3,0,2,0, 1,0,2,0,4,0,5,0, 7,0,6,0,5,0,0,0},
  /*22 RITUAL     */ {1,0,0,2,0,0,1,0, 3,0,0,2,0,0,1,0, 5,0,0,4,0,0,3,0, 2,0,0,1,0,0,0,0},
  /*23 ASEDIO     */ {5,0,5,0,6,0,5,0, 8,0,0,0,5,0,0,0, 5,0,5,0,4,0,3,0, 2,0,0,0,1,0,0,0}
};

// ══════════════════════════════════════════════════════════════════════════════
//  VOZ MONOFÓNICA — banco de 6 osciladores sobre UNA nota
// ══════════════════════════════════════════════════════════════════════════════
#define NUM_OSC 6
const float OSC_RATIO[NUM_OSC]  = { 1.0f, 1.0f, 1.0f, 0.5f, 1.498307f, 2.0f };
const float OSC_DETUNE[NUM_OSC] = { 0.0f, +1.0f, -0.7f, 0.0f, +0.35f, -0.5f };
const float OSC_PAN[NUM_OSC]    = {-0.15f, +0.75f, -0.75f, 0.0f, -0.45f, +0.55f };
float oscPhase[NUM_OSC]  = {0, 0, 0, 0, 0, 0};
float oscDetMul[NUM_OSC] = {1, 1, 1, 1, 1, 1};

// ─── EL DRON: lo que convierte esto en ambiente ────────────────────────────────
// Tres osciladores fijos sobre la TÓNICA que suenan de forma continua, sin
// envolvente, y pasan por el mismo filtro. Es una sola altura sostenida, no
// polifonía: es el pedal sobre el que se apoya una escena.
#define NUM_DRON 3
const float DRON_RATIO[NUM_DRON] = { 1.0f, 1.498307f, 0.5f };
const float DRON_PAN[NUM_DRON]   = {-0.55f, +0.60f, 0.0f };
const float DRON_DET[NUM_DRON]   = { +3.0f, -4.0f, +2.0f };   // cents: batido lento natural
float dronPhase[NUM_DRON]  = {0, 0, 0};
float dronDetMul[NUM_DRON] = {1, 1, 1};
float dronFreq  = 65.41f;
float dronGain  = 0.0f;

float freqTarget = 130.81f;
float freqCur    = 130.81f;
float glideK     = 1.0f;
float env        = 0.0f;
uint8_t envStage = 2;               // 0 attack · 1 sustain · 2 release
float attackInc  = 0.01f;
float relCoef    = 0.9999f;
uint32_t gateLeft = 0;
float voiceGain  = 0.1f;

// ─── Controles continuos (los 4 pots, siempre vivos) ───────
float g_volume   = 0.45f;
float cutoffPot  = 1200.0f;
float espacio    = 0.45f;
float cutoffSm = 1200.0f, espacioSm = 0.45f;

// ══════════════════════════════════════════════════════════════════════════════
//  SECUENCIADOR
// ══════════════════════════════════════════════════════════════════════════════
bool  isPlaying   = false;
int   modeIdx     = 0;
int   temaIdx     = 0;
int   rootSemi    = 0;
int   octaveIndex = 2;
const int OCTAVE_SEMITONES[4] = {-24, -12, 0, 12};
const float BASE_FREQ = 130.81f;    // C3

int   progIdx     = 0;              // tramo de la progresión (0..3), uno por ciclo
int   stepInCiclo = 0;              // paso dentro del ciclo (0..31)

uint32_t sampleInStep   = 0;
uint32_t samplesPerStep = 6890;     // largo de paso que pide el POT2
// Largo del paso EN CURSO. Se copia de samplesPerStep al empezar cada paso, así el
// pot de tempo solo afecta al paso siguiente y nunca corta el actual de golpe: eso
// era lo que hacía atropellarse las notas al barrer la perilla.
uint32_t pasoActual     = 6890;
int      bpmCur         = 62;

bool  stepRest      = true;
float stepFreq      = 130.81f;
float stepPan       = 0.0f;
uint32_t stepGate   = 6890;
// El paneo se suaviza por muestra: aplicarlo de golpe hace saltar las ganancias L/R
// en medio de una nota que ya suena, y eso es un chasquido.
float panCur        = 0.0f;

// ─── IMU (UN SOLO EJE: X) ──────────────────────────────────
float filtered_x = 0.0f;
unsigned long lastIMURead = 0;
uint8_t imuFails = 0;

// ─── Filtro biquad LPF resonante ESTÉREO ───────────────────
Biquad filtL = {0, 0, 0, 0, 0, 0, 0, 0, 0};
Biquad filtR = {0, 0, 0, 0, 0, 0, 0, 0, 0};

// ══════════════════════════════════════════════════════════════════════════════
//  ESPACIO: eco ping-pong + reverb
// ══════════════════════════════════════════════════════════════════════════════
// El tiempo del eco es FIJO y NO se sincroniza al tempo. Sincronizarlo obliga a
// mover el puntero de lectura cada vez que cambia el BPM, y eso se oye como un
// barrido de altura mientras giras la perilla de velocidad. Un eco ambiental no
// necesita ir a tiempo.
#define DELAY_MAX 33075
#define DELAY_FIJO 19845            // 0.45 s
int16_t dlyL[DELAY_MAX];
int16_t dlyR[DELAY_MAX];
int   dlyWrite = 0;
float dlyFb = 0.45f, dlyMix = 0.35f;

#define NCOMB 6
const int COMB_LEN[NCOMB] = {1116, 1188, 1277, 1356, 1422, 1491};
#define COMB_TOTAL (1116 + 1188 + 1277 + 1356 + 1422 + 1491)
float combBuf[COMB_TOTAL];
int   combOff[NCOMB], combIdx[NCOMB];
float combStore[NCOMB] = {0, 0, 0, 0, 0, 0};

#define NAP 4
const int AP_LEN[NAP] = {556, 441, 341, 225};
#define AP_TOTAL (556 + 441 + 341 + 225)
float apBuf[AP_TOTAL];
int   apOff[NAP], apIdx[NAP];

float revFb = 0.80f, revDamp = 0.28f, revMix = 0.25f;

// ─── Botones ───────────────────────────────────────────────
// Sin combos: los cinco disparan en el flanco de bajada, que es lo más inmediato.
BtnState bPlay  = {BTN1, HIGH, 0};
BtnState bTema  = {BTN2, HIGH, 0};
BtnState bModo  = {BTN3, HIGH, 0};
BtnState bTono  = {BTN4, HIGH, 0};
BtnState bOct   = {BTN5, HIGH, 0};

static i2s_chan_handle_t tx_chan;

// ══════════════════════════════════════════════════════════════════════════════
//  UTILIDADES
// ══════════════════════════════════════════════════════════════════════════════
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (float)(sum >> 3) / 4095.0f;
}

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

inline float softSat(float x) {
  if (x >  2.5f) x =  2.5f;
  if (x < -2.5f) x = -2.5f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

int degreeSemi(int degree) {
  int oct = 0;
  while (degree >= 7) { degree -= 7; oct += 12; }
  while (degree <  0) { degree += 7; oct -= 12; }
  return MODE_INT[modeIdx][degree] + oct;
}

// ══════════════════════════════════════════════════════════════════════════════
//  OSCILADORES
// ══════════════════════════════════════════════════════════════════════════════
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

inline float fastSin(float ph) {
  float x = 2.0f * ph - 1.0f;
  float y = 4.0f * x * (1.0f - fabsf(x));
  return 0.225f * (y * fabsf(y) - y) + y;
}

inline float oscOut(float phase, float dt) {
  if (ONDA == 0) {                            // SIERRA
    return (2.0f * phase - 1.0f) - polyBlep(phase, dt);
  } else if (ONDA == 1) {                     // PULSO
    float s1 = (2.0f * phase - 1.0f) - polyBlep(phase, dt);
    float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
    float s2 = (2.0f * p2 - 1.0f) - polyBlep(p2, dt);
    return (s1 - s2) * 0.6f;
  } else if (ONDA == 2) {                     // TRIANGULAR
    return (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
  }
  return fastSin(phase);                      // SENO
}

// ══════════════════════════════════════════════════════════════════════════════
//  IMU — solo el eje X
// ══════════════════════════════════════════════════════════════════════════════
void initIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B); Wire.write(0);
  Wire.endTransmission(true);
  delay(20);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x1C); Wire.write(0x00);
  Wire.endTransmission(true);
  delay(50);
}

void readIMU() {
  unsigned long t = millis();
  if (t - lastIMURead < IMU_READ_INTERVAL) return;
  lastIMURead = t;

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, 6, true);
  if (Wire.available() >= 6) {
    int16_t rx = (Wire.read() << 8) | Wire.read();
    (void)((Wire.read() << 8) | Wire.read());   // Y descartado (un solo eje)
    (void)((Wire.read() << 8) | Wire.read());   // Z descartado
    filtered_x = filtered_x * (1.0f - IMU_FILTER_ALPHA) + (rx / 16384.0f) * IMU_FILTER_ALPHA;
    imuFails = 0;
  } else {
    if (++imuFails > 40) { imuFails = 0; initIMU(); }
  }
}

// ══════════════════════════════════════════════════════════════════════════════
//  FILTRO
// ══════════════════════════════════════════════════════════════════════════════
void setBiquad(Biquad &f, float cutoff, float Q) {
  if (cutoff < 40.0f)    cutoff = 40.0f;
  if (cutoff > 12000.0f) cutoff = 12000.0f;

  float omega = 2.0f * (float)M_PI * cutoff / SAMPLE_RATE;
  float s = sinf(omega), c = cosf(omega);
  float alpha = s / (2.0f * Q);

  float b0 = (1.0f - c) * 0.5f;
  float b1 =  1.0f - c;
  float b2 = (1.0f - c) * 0.5f;
  float a0 =  1.0f + alpha;
  float a1 = -2.0f * c;
  float a2 =  1.0f - alpha;

  f.b0 = b0 / a0; f.b1 = b1 / a0; f.b2 = b2 / a0;
  f.a1 = a1 / a0; f.a2 = a2 / a0;
}

inline float runBiquad(Biquad &f, float in) {
  float out = f.b0 * in + f.b1 * f.x1 + f.b2 * f.x2 - f.a1 * f.y1 - f.a2 * f.y2;
  f.x2 = f.x1; f.x1 = in;
  f.y2 = f.y1; f.y1 = out;
  return out;
}

// ══════════════════════════════════════════════════════════════════════════════
//  NOTAS
// ══════════════════════════════════════════════════════════════════════════════
float notaDe(uint8_t v, int chordRoot, float &pan) {
  int deg, octShift;
  if (v == 9) {                       // golpe grave: fundamental dos octavas abajo
    deg = chordRoot; octShift = -24; pan = 0.0f;
  } else {
    deg = chordRoot + (int)(v - 1); octShift = 0;
    pan = ((float)(v - 1) * 0.16f - 0.5f) * 0.6f;   // grave izquierda, agudo derecha
  }
  int semi = degreeSemi(deg) + rootSemi + OCTAVE_SEMITONES[octaveIndex] + octShift;
  float f = BASE_FREQ * powf(2.0f, semi * (1.0f / 12.0f));
  // Piso en E1: más abajo solo gasta headroom (el sub va otra octava por debajo)
  while (f <   41.20f) f *= 2.0f;
  while (f > 2200.0f)  f *= 0.5f;
  return f;
}

// Cuántos pasos faltan hasta la próxima nota del tema (dando la vuelta al ciclo).
// De aquí sale la DURACIÓN: cada nota suena hasta que llega la siguiente, y por eso
// hay notas de cuatro tiempos y notas de medio.
int pasosHastaSiguiente(int desde) {
  for (int d = 1; d <= PASOS_CICLO; d++)
    if (TEMA[temaIdx][(desde + d) % PASOS_CICLO] != 0) return d;
  return PASOS_CICLO;
}

void actualizarDron() {
  int semi = rootSemi + OCTAVE_SEMITONES[octaveIndex] - 12;
  float f = BASE_FREQ * powf(2.0f, semi * (1.0f / 12.0f));
  while (f < 32.70f) f *= 2.0f;
  while (f > 110.0f) f *= 0.5f;
  dronFreq = f;
}

// ── Estado por Serial ─────────────────────────────────────────────────────────
#define MOSTRAR_ESTADO 1
const char* NOM_MODO[NUM_MODOS] = {
  "EOLICO", "FRIGIO", "NORDICO", "MIXOLIDIO", "LIDIO", "ARABE", "BIZANTINO", "MEN.ARMONICA"
};
const char* NOM_TEMA[NUM_TEMAS] = {
  "LLAMADA",  "JURAMENTO","ESTANDARTE","CUMBRE",
  "LAMENTO",  "SUSPIRO",  "CAIDA",     "DUELO",
  "VIGILIA",  "SOMBRA",   "PLEGARIA",  "ORACULO",
  "CIMIENTO", "TITAN",    "ABISMO",    "RUINA",
  "ARCO",     "TRAVESIA", "REGRESO",   "PROMESA",
  "CABALGATA","TORMENTA", "RITUAL",    "ASEDIO"
};
const char* NOM_NOTA[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

void mostrarEstado() {
#if MOSTRAR_ESTADO
  Serial.printf("MODO %-12s  TEMA %-11s  TONICA %-2s  OCTAVA %+d  BPM %d\n",
                NOM_MODO[modeIdx], NOM_TEMA[temaIdx], NOM_NOTA[rootSemi],
                octaveIndex - 2, bpmCur);
#endif
}

void prepareStep() {
  uint8_t v = TEMA[temaIdx][stepInCiclo];
  if (v == 0) { stepRest = true; return; }    // no es silencio: sigue la nota anterior
  stepRest = false;
  float pan;
  stepFreq = notaDe(v, (int)PROG[modeIdx][progIdx], pan);
  stepPan  = pan;
  stepGate = (uint32_t)(pasosHastaSiguiente(stepInCiclo) * pasoActual * LARGO);
  if (stepGate < 256) stepGate = 256;
}

void fireHit() {
  freqTarget = stepFreq;
  if (env < 0.0015f) freqCur = stepFreq;
  envStage = 0;
  gateLeft = stepGate;
}

void reiniciarYSonar() {
  progIdx = 0;
  stepInCiclo = 0;
  sampleInStep = 0;
  pasoActual = samplesPerStep;
  actualizarDron();
  mostrarEstado();
  if (!isPlaying) return;
  prepareStep();
  if (!stepRest) fireHit();
}

void applyBpm(int bpm) {
  if (bpm < 20)  bpm = 20;
  if (bpm > 160) bpm = 160;
  if (bpm == bpmCur) return;
  bpmCur = bpm;
  // Paso = corchea. NO se toca sampleInStep ni pasoActual: el paso en curso termina
  // con su largo de siempre y el tempo nuevo entra en el siguiente. Sin eso, subir el
  // tempo acorta el paso actual de golpe y dispara una nota — al barrer la perilla,
  // decenas de notas atropelladas.
  samplesPerStep = (uint32_t)((uint64_t)SAMPLE_RATE * 60 / (bpm * 2));
}

// ══════════════════════════════════════════════════════════════════════════════
//  I2S
// ══════════════════════════════════════════════════════════════════════════════
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

// ══════════════════════════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════════════════════════
void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);
#if MOSTRAR_ESTADO
  Serial.begin(115200);
#endif

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(BTN4, INPUT_PULLUP);
  pinMode(BTN5, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Semilla del azar: ruido de los bits bajos del ADC + reloj
  rngState ^= (uint32_t)micros() * 2654435761u;
  for (int i = 0; i < 12; i++) rngState ^= ((uint32_t)analogRead(POT1) << (i * 2)) + rnd32();
  if (rngState == 0) rngState = 0x9E3779B9;

  for (int i = 0; i < DELAY_MAX; i++) { dlyL[i] = 0; dlyR[i] = 0; }
  int off = 0;
  for (int i = 0; i < NCOMB; i++) { combOff[i] = off; combIdx[i] = 0; off += COMB_LEN[i]; }
  for (int i = 0; i < COMB_TOTAL; i++) combBuf[i] = 0.0f;
  off = 0;
  for (int i = 0; i < NAP; i++) { apOff[i] = off; apIdx[i] = 0; off += AP_LEN[i]; }
  for (int i = 0; i < AP_TOTAL; i++) apBuf[i] = 0.0f;

  bpmCur = -1; applyBpm(62);
  pasoActual = samplesPerStep;

  for (int i = 0; i < NUM_DRON; i++) dronDetMul[i] = powf(2.0f, DRON_DET[i] / 1200.0f);
  for (int i = 0; i < NUM_OSC;  i++) oscDetMul[i]  = powf(2.0f, (OSC_DETUNE[i] * DETUNE) / 1200.0f);
  actualizarDron();

  // Ganancia normalizada por la suma de niveles (apilar capas no sube el volumen) y
  // por una potencia suave de la Q (la resonancia no mete el sonido en el saturador).
  float lvlSum = 0.0f;
  for (int i = 0; i < NUM_OSC; i++) lvlSum += NIVEL_OSC[i];
  voiceGain = (0.42f / lvlSum) / powf(Q_FIJA, 0.30f);

  attackInc = 1.0f / (ATAQUE * SAMPLE_RATE);
  relCoef   = expf(-1.0f / (COLA * SAMPLE_RATE));
  glideK    = (GLIDE <= 0.0f) ? 1.0f : (1.0f - expf(-1.0f / (GLIDE * SAMPLE_RATE)));

  initIMU();
  readIMU();
  setBiquad(filtL, cutoffSm, Q_FIJA);
  setBiquad(filtR, cutoffSm, Q_FIJA);

  i2s_init();
}

// ══════════════════════════════════════════════════════════════════════════════
//  LOOP
// ══════════════════════════════════════════════════════════════════════════════
void loop() {
  // ─── Los 5 botones, todos en el flanco de bajada ────────────────────────────
  if (buttonPressed(bPlay)) {
    isPlaying = !isPlaying;
    if (isPlaying) {
      // Cada Play trae un SENTIMIENTO NUEVO: se sortea el modo (y con él su
      // progresión). Nunca sale el mismo que acababa de sonar.
      modeIdx = sortearDistinto(NUM_MODOS, modeIdx);
      reiniciarYSonar();
    } else {
      gateLeft = 0; envStage = 2;
    }
  }
  if (buttonPressed(bTema)) {                 // TEMA al azar, mismo sentimiento
    temaIdx = sortearDistinto(NUM_TEMAS, temaIdx);
    reiniciarYSonar();
  }
  if (buttonPressed(bModo)) {                 // MODO en orden
    modeIdx = (modeIdx + 1) % NUM_MODOS;
    reiniciarYSonar();
  }
  if (buttonPressed(bTono)) {                 // TONALIDAD, por cuartas
    rootSemi = (rootSemi + 5) % 12;
    reiniciarYSonar();
  }
  if (buttonPressed(bOct)) {                  // OCTAVA base
    octaveIndex = (octaveIndex + 1) % 4;
    reiniciarYSonar();
  }

  readIMU();

  // ─── POTS: siempre los mismos 4, siempre vivos, sin congelarse ──────────────
  static const uint8_t POT_PIN[4] = { POT1, POT2, POT3, POT4 };
  static uint8_t potScan = 0;
  int pi = potScan; potScan = (potScan + 1) & 3;
  float pv = readPot(POT_PIN[pi]);
  switch (pi) {
    case 0: g_volume  = pv * pv; break;                          // VOLUMEN
    case 1: applyBpm(25 + (int)(pv * 85.0f + 0.5f)); break;      // TEMPO 25-110 BPM
    case 2: cutoffPot = 80.0f * powf(100.0f, pv); break;         // FILTRO 80 Hz - 8 kHz
    case 3: espacio   = pv; break;                               // ESPACIO
  }

  cutoffSm  += (cutoffPot - cutoffSm)  * 0.15f;    // ~25 ms: directo pero sin chasquear
  espacioSm += (espacio   - espacioSm) * 0.06f;

  // El dron suena mientras haya Play; al parar se funde con todo lo demás
  float dronObjetivo = isPlaying ? DRON_NIV : 0.0f;
  dronGain += (dronObjetivo - dronGain) * 0.02f;

  // ─── FILTRO: solo POT3 y el IMU. Nada automático lo toca. ───────────────────
  float xa = fabsf(filtered_x); if (xa > 1.0f) xa = 1.0f;
  float cut = cutoffSm * powf(8.0f, xa);          // el IMU multiplica hasta x8
  if (cut > 12000.0f) cut = 12000.0f;
  setBiquad(filtL, cut * 0.97f, Q_FIJA);
  setBiquad(filtR, cut * 1.03f, Q_FIJA);

  // ─── Espacio ────────────────────────────────────────────────────────────────
  dlyFb   = 0.18f + espacioSm * 0.52f;
  dlyMix  = espacioSm * 0.50f;
  revFb   = 0.70f + espacioSm * 0.22f;
  revMix  = espacioSm * espacioSm * 0.50f;
  revDamp = 0.42f - espacioSm * 0.20f;

  // ══════════════════════════════════════════════════════════════════════════
  //  GENERACIÓN DE AUDIO
  // ══════════════════════════════════════════════════════════════════════════
  int16_t buffer[BUFFER_SAMPLES * 2];

  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    if (isPlaying) {
      if (sampleInStep >= pasoActual) {
        sampleInStep = 0;
        pasoActual = samplesPerStep;           // el tempo nuevo entra AQUÍ, no antes
        stepInCiclo++;
        if (stepInCiclo >= PASOS_CICLO) {      // 4 compases enteros → siguiente tramo
          stepInCiclo = 0;
          progIdx = (progIdx + 1) & 3;
        }
        prepareStep();
        if (!stepRest) fireHit();
      }
      sampleInStep++;
    }

    if (gateLeft > 0) { if (--gateLeft == 0) envStage = 2; }

    if (envStage == 0) {
      env += attackInc;
      if (env >= 1.0f) { env = 1.0f; envStage = 1; }
    } else if (envStage == 2) {
      env *= relCoef;
      if (env < 0.00008f) env = 0.0f;
    }

    freqCur += (freqTarget - freqCur) * glideK;
    panCur  += (stepPan - panCur) * 0.002f;      // ~11 ms

    // ── Banco de osciladores (una sola nota) ──
    float sumL = 0.0f, sumR = 0.0f;
    for (int i = 0; i < NUM_OSC; i++) {
      float f  = freqCur * OSC_RATIO[i] * oscDetMul[i];
      float dt = f * (1.0f / SAMPLE_RATE);
      if (dt > 0.45f) continue;

      oscPhase[i] += dt;
      if (oscPhase[i] >= 1.0f) oscPhase[i] -= 1.0f;

      float w = oscOut(oscPhase[i], dt) * NIVEL_OSC[i];

      float p = OSC_PAN[i] * 0.6f + panCur;
      if (p >  1.0f) p =  1.0f;
      if (p < -1.0f) p = -1.0f;
      sumL += w * (0.5f * (1.0f - p));
      sumR += w * (0.5f * (1.0f + p));
    }

    float amp = env * voiceGain;
    sumL *= amp; sumR *= amp;

    // ── EL DRON: continuo, sin envolvente, al mismo bus antes del filtro ──
    if (dronGain > 0.0005f) {
      float dg = dronGain * voiceGain * 0.55f;
      for (int i = 0; i < NUM_DRON; i++) {
        float f  = dronFreq * DRON_RATIO[i] * dronDetMul[i];
        float dt = f * (1.0f / SAMPLE_RATE);
        dronPhase[i] += dt;
        if (dronPhase[i] >= 1.0f) dronPhase[i] -= 1.0f;
        float w = oscOut(dronPhase[i], dt) * dg;
        float p = DRON_PAN[i];
        sumL += w * (0.5f * (1.0f - p));
        sumR += w * (0.5f * (1.0f + p));
      }
    }

    float L = runBiquad(filtL, sumL);
    float R = runBiquad(filtR, sumR);

    // ── Eco ping-pong, de tiempo fijo (no se sincroniza al tempo) ──
    int ri = dlyWrite - DELAY_FIJO;
    if (ri < 0) ri += DELAY_MAX;
    float eL = dlyL[ri] * (1.0f / 32767.0f);
    float eR = dlyR[ri] * (1.0f / 32767.0f);

    // Saturación SUAVE dentro del lazo: recortar duro acá deja recirculando la
    // distorsión hasta que se apaga el eco.
    float wL = softSat((L + R) * 0.35f + eR * dlyFb);
    float wR = softSat(eL * dlyFb);
    dlyL[dlyWrite] = (int16_t)(wL * 32000.0f);
    dlyR[dlyWrite] = (int16_t)(wR * 32000.0f);
    if (++dlyWrite >= DELAY_MAX) dlyWrite = 0;

    L += eL * dlyMix;
    R += eR * dlyMix;

    // ── Reverb ──
    float rin = (L + R) * 0.20f;
    float acc = 0.0f;
    for (int c = 0; c < NCOMB; c++) {
      int idx = combOff[c] + combIdx[c];
      float y = combBuf[idx];
      acc += y;
      combStore[c] = y * (1.0f - revDamp) + combStore[c] * revDamp;
      combBuf[idx] = rin + combStore[c] * revFb;
      if (++combIdx[c] >= COMB_LEN[c]) combIdx[c] = 0;
    }
    acc *= (1.0f / NCOMB);

    float rL = acc, rR = acc;
    for (int a = 0; a < 2; a++) {
      int idx = apOff[a] + apIdx[a];
      float bufv = apBuf[idx];
      float out  = -rL + bufv;
      apBuf[idx] = rL + bufv * 0.5f;
      rL = out;
      if (++apIdx[a] >= AP_LEN[a]) apIdx[a] = 0;
    }
    for (int a = 2; a < 4; a++) {
      int idx = apOff[a] + apIdx[a];
      float bufv = apBuf[idx];
      float out  = -rR + bufv;
      apBuf[idx] = rR + bufv * 0.5f;
      rR = out;
      if (++apIdx[a] >= AP_LEN[a]) apIdx[a] = 0;
    }

    L += rL * revMix;
    R += rR * revMix;

    // ── Salida ──
    float gm = g_volume * 1.25f;
    float sL = softSat(L * gm);
    float sR = softSat(R * gm);

    buffer[n * 2]     = (int16_t)(sL * 30000.0f);
    buffer[n * 2 + 1] = (int16_t)(sR * 30000.0f);
  }

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);
}
