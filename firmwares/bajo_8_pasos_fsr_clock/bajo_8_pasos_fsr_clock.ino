// ==============================================================================================================================================
// PERCU-SYNTH — BAJO 8 PASOS + FSR + RELOJ MIDI (secuenciador de bajo, FSR → filtro, MIDI Clock por el DIN-5) — GC Lab Chile
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
// - 6 LEDs WS2812 SMD internos de la placa |DATA -> 46| (índices 0..5 de la tira)
// - Tira WS2812 externa de 121 LEDs encadenada DESPUÉS de los 6 SMD, mismo pin |DATA -> 46|
//   (índices 6..126 · alimentación 5 V propia, GND común con la placa)
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// - FSR (sensor de presión) + resistencia de 10 kΩ en el SENSOR EXTERNO A (EXT1) |ADC -> GPIO 3|
// - Salida MIDI DIN-5 |TX -> 43| a 31250 baud — SÓLO RELOJ: Clock (24 PPQN) + Start/Stop
//
// CABLEADO DEL FSR (divisor de tensión, cualquiera de las dos formas sirve):
//   3.3V ── FSR ──┬── EXT1 (GPIO 3)        apretado = lectura ALTA
//                 10 kΩ
//                  │
//                 GND
//
//   3.3V ── 10 kΩ ──┬── EXT1 (GPIO 3)      apretado = lectura BAJA
//                    FSR
//                     │
//                    GND
//
//   La polaridad NO importa: el mapeo es una recta entre las dos lecturas medidas,
//   FSR_SUELTO y FSR_APRETADO (ver el bloque "AJUSTE DEL FSR" más abajo).
//
// El MIDI sale por Serial1 enrutado al GPIO 43, NO por Serial0: Serial0 comparte ese TX
// con el DIN-5 y además su RX cae en el GPIO 44, que acá es el BTN1. Por eso, si activas
// MOSTRAR_ESTADO para calibrar, los prints van SOLO por el USB CDC (`Serial`).
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - Flash Mode         : DIO          (¡OPI rompe I2S!)
// - PSRAM              : OPI PSRAM    (no es obligatorio)
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (incluye driver/i2s_std.h)
// - FastLED (gestor de librerías de Arduino) — para los 6 LEDs de placa
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Secuenciador de 8 pasos que toca un BAJO MONOFÓNICO lleno de armónicos:
//
//   · Dos sierras PolyBLEP (una desafinada +9 cents) + una cuadrada una octava abajo
//   · Saturación suave → filtro pasa-bajos resonante (SVF) con ENVOLVENTE DE FILTRO:
//     cada nota abre el filtro y se cierra con el decay → el "pluck" típico de bajo
//   · Envolvente de amplitud: ataque de 2 ms + decay exponencial (pot)
//   · Todas las notas en LA PENTATÓNICA MENOR (La · Do · Re · Mi · Sol), 2 octavas
//
// Esta versión es `bajo_8_pasos` con dos cosas encima, y nada más:
//
//   · FSR → FILTRO. Un sensor de presión en EXT1 (GPIO 3) ABRE el filtro por encima de
//     donde lo dejó el POT4: apretar = más brillo (hasta FSR_OCT octavas), soltar = el
//     sonido exacto del firmware original. El pot sigue mandando el punto de reposo y
//     la resonancia, como antes; el FSR es una expresión en vivo encima, igual que el
//     IMU en `espacio_modular`. (Si lo prefieres al revés, pon FSR_REEMPLAZA_POT a 1 y
//     el FSR pasa a ser TODO el cutoff.)
//
//   · RELOJ MIDI por el DIN-5. MIDI Clock a 24 PPQN al tempo del secuenciador (POT2 del
//     panel C) — sólo reloj, sin notas. Start al dar Play, Stop al parar, y el "1" del
//     Start cae en el paso 1. Los ticks salen siempre, aun parado, para que el sinte
//     externo ya tenga el tempo cuando llegue el Start. Cada paso es una semicorchea,
//     así que van 6 ticks por paso y 24 por negra.
//
// Como son 8 notas y hay 4 pots, los pots trabajan por PANELES. Al cambiar de panel
// los pots NO pisan nada: un parámetro sólo cambia cuando MUEVES su pot (desde ese
// momento la posición física del pot es el valor). Así saltar de panel nunca cambia
// las notas que ya dejaste.
//
// ARQUITECTURA: el audio corre en su propia tarea fijada al CORE 1 y los controles
// (botones, pots, LEDs) en una tarea en el CORE 0 a 1 kHz. La tarea de control sólo
// escribe pedidos (notas, parámetros, play) que el audio lee en el borde de un buffer
// o al empezar cada paso. Sin Serial: un print por USB CDC bloquea y vacía el DMA.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// - BTN1 → PLAY / STOP. Play arranca siempre desde el paso 1 y suena en el acto.
// - BTN2 → PANEL A : POT1..POT4 = nota de los pasos 1..4
// - BTN3 → PANEL B : POT1..POT4 = nota de los pasos 5..8
// - BTN4 → PANEL C : POT1 volumen · POT2 velocidad (60–200 BPM, semicorcheas) ·
//                    POT3 decay (30 ms – 1.2 s) · POT4 cutoff (50 Hz – 6 kHz)
// - BTN5 → OCTAVA siguiente (ciclo de 3: La1 · La2 · La3). Cambia también la nota que
//          está sonando.
// - FSR  → FILTRO. Aprieta y el filtro se abre (hasta FSR_OCT octavas sobre el POT4).
//          Suelto = el cutoff del pot, tal cual. Se muestrea a 1 kHz en el core 0.
// - DIN-5 → MIDI Clock 24 PPQN al tempo del panel C · Start al Play · Stop al Stop.
//
// Notas: cada pot recorre 11 notas de la pentatónica menor (2 octavas). Con el pot
// al mínimo (< 3 % del recorrido) ese paso queda en SILENCIO.
//
// LEDs de la placa:
//   0..3 = panel A/B: los 4 pasos del panel (color = nota, apagado = silencio,
//          blanco = paso sonando) · panel C: nivel de cada parámetro
//   4    = panel activo (A cian · B violeta · C naranjo) — el BRILLO sigue al FSR, así
//          se ve si el sensor responde sin tener que abrir el monitor serie
//   5    = octava (brillo) + pulso en cada paso mientras suena (fuerte en los pasos 1 y 5)
//
// Tira externa (121 LEDs): 8 EFECTOS QUE SE SORTEAN SOLOS.
//
//   BALAS      un proyectil con estela por cada paso, volando al tempo
//   ONDAS      anillos que se abren hacia los dos lados desde un punto al azar
//   VU         barra que crece desde el centro con la envolvente, con punta blanca
//   CHISPAS    puñados de píxeles que se encienden en el golpe y se apagan solos
//   ARCOÍRIS   el espectro entero corriendo al tempo
//   PLASMA     campo de ruido que respira
//   BANDAS     la tira partida en 8 bloques, uno por paso, en orden barajado
//   SERPIENTE  una cabeza blanca que va y vuelve al tempo, con su cola
//
// Cada 4 compases —y en cada Play— se sortea uno nuevo, que nunca es el que estaba, y con
// él se sortean su paleta, su dirección y un parámetro propio: el mismo efecto no se ve
// igual dos veces. No hay botón para cambiarlo: los 5 ya tienen su función.
//
// Los ocho reaccionan a las mismas señales, así que da igual cuál salga, siempre responde
// a lo que estás tocando: el GOLPE de cada paso (más fuerte en los pasos 1 y 5), la NOTA
// que suena (define el color), la ENVOLVENTE real del bajo (la publica la tarea de audio,
// no es una imitación calculada aparte) y la PRESIÓN DEL FSR, que deforma el efecto en
// vivo. El TEMPO fija cuántos LEDs se recorren por paso, así que la tira siempre va al
// ritmo del secuenciador. Un paso en silencio no dispara nada.
//
// La tira tiene su propio LIMITADOR DE CORRIENTE (ver el bloque PRESUPUESTO más abajo):
// con la tira colgada del 5 V de la placa, un efecto que prende los 121 LEDs hunde el riel
// y corta el audio. Se ajusta con un solo número, TIRA_MAX_MA.
//
// CALIBRAR EL FSR (una vez, al armarlo):
// 1. Pon MOSTRAR_ESTADO en 1 y flashea. Abre el monitor serie a 115200.
// 2. Con el FSR SUELTO, mira `raw` → ese número va en FSR_SUELTO.
// 3. Apriétalo todo lo que vayas a apretarlo tocando, mira `raw` → va en FSR_APRETADO.
// 4. Vuelve MOSTRAR_ESTADO a 0 y flashea de nuevo (un print por USB CDC bloquea el core
//    que lo hace hasta que el PC lea, y eso vacía el DMA del audio: se oye como un clic).
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <math.h>

// ─── Tipos (arriba del todo para que el IDE de Arduino genere bien los prototipos) ───
struct EstadoSVF { float ic1, ic2; };
struct Particula {    // la usan los efectos de partículas (balas, ondas, gotas)
  bool    viva;
  float   pos;        // posición (o radio) en la tira, en LEDs con decimales
  float   vel;        // multiplicador de velocidad propio
  float   origen;     // desde dónde salió (las ondas se abren hacia los dos lados)
  uint8_t hue;        // color
  uint8_t brillo;
  uint8_t largo;      // largo de la estela (LEDs)
};

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
const float INV_SR = 1.0f / (float)SAMPLE_RATE;

// ─── LEDs WS2812: 6 SMD internos + tira externa encadenada en el mismo pin ──
#define LED_PIN        46
#define STATUS_LEDS     6                     // índices 0..5 = SMD de la placa (estado)
#define TIRA_LEDS     121                     // tira externa: índices 6..126
#define NUM_LEDS      (STATUS_LEDS + TIRA_LEDS)
#define LED_BRIGHT     200
#define LED_TYPE       WS2812
#define COLOR_ORDER    GRB
#define LED_MAX_MA     600                    // tope duro de FastLED (baja el brillo global si se pasa)

// ══════════════════════════════════════════════════════════════════════════════════════════════
// PRESUPUESTO DE CORRIENTE DE LA TIRA  ← ESTO ES LO QUE SE TOCA SI CAMBIAS LA ALIMENTACIÓN
// ══════════════════════════════════════════════════════════════════════════════════════════════
// Con la tira colgada del 5 V de la placa (o del USB), un efecto que prende los 121 LEDs a la
// vez pide del orden de 2 A, el riel se hunde y **se corta el audio**. Ojo con el diagnóstico:
// esto NO es el problema de tiempo de los otros firmwares — los ocho efectos juntos no llegan
// a 0,5 ms de los 16 que hay entre frames, y el audio corre en el otro núcleo. Es eléctrico.
//
// Las balas nunca lo provocaban porque prenden ~50 LEDs con una estela que se apaga; ARCOÍRIS
// y PLASMA prenden los 121 siempre, y BANDAS también cuando aprietas el FSR.
//
// SI LE PONES FUENTE PROPIA a la tira (5 V, 2 A o más, GND común), sube TIRA_MAX_MA a 1500 y
// LED_MAX_MA a 2000 y los efectos vuelven a brillo completo. Y aunque tenga fuente propia,
// conviene un condensador de 1000 µF en la entrada de la tira: lo que hunde el riel no es la
// corriente alta sino el ESCALÓN de corriente.
const float TIRA_MAX_MA  = 300.0f;    // corriente que le dejamos consumir a la tira
const float MA_POR_LED   = 60.0f;     // un WS2812 en blanco pleno
const float RECUPERA_LED = 0.06f;     // cuánto sube la ganancia por frame al soltar (subida lenta)

// Fracción del consumo máximo teórico que corresponde a ese tope, ya descontado el brillo
// global: carga 1.0 sería los 121 LEDs en blanco pleno.
const float CARGA_MAXIMA = TIRA_MAX_MA / ((float)TIRA_LEDS * MA_POR_LED * (LED_BRIGHT / 255.0f));
float gananciaTira = 1.0f;
const unsigned long LED_REFRESH_MS = 16;      // ~60 fps: el movimiento sale suave
CRGB leds[NUM_LEDS];

// ══════════════════════════════════════════════════════════════════════════════════════════════
// EFECTOS DE LA TIRA — 8 efectos que se SORTEAN solos, todos reactivos a lo que suena
// ══════════════════════════════════════════════════════════════════════════════════════════════
// Cada 4 compases (y en cada Play) se sortea un efecto nuevo, que nunca es el que estaba, y
// con él se sortean su paleta, su dirección y un parámetro propio: el mismo efecto no se ve
// igual dos veces. No hay botón para cambiarlo — los 5 ya tienen su función y acá no se
// inventan combos.
//
// Los ocho reaccionan a las mismas cuatro señales, así que da igual cuál salga, siempre
// responde a lo que estás tocando:
//   · el GOLPE de cada paso (más fuerte en los pasos 1 y 5, que marcan el compás)
//   · la NOTA que suena (define el color)
//   · la ENVOLVENTE del bajo (`g_env`, la escribe la tarea de audio) → brillo y tamaño
//   · el FSR (`fsrFilt`) → la presión deforma el efecto en vivo
// y el TEMPO, que fija cuántos LEDs se recorren por paso.
#define NUM_EFECTOS     8
#define MAX_PART       24
#define PASOS_POR_SORTEO 64                   // 4 compases de 4/4 en semicorcheas
#define NUM_BLOQUES     8                     // = NUM_PASOS (se define más abajo, en ESCALA)

const float   LEDS_POR_PASO = 15.0f;          // LEDs recorridos en un paso (8 × 15 = 120)
const uint8_t HUE_PASO[8] ={ 0, 28, 56, 96, 130, 160, 190, 225 };  // arcoíris por paso
Particula part[MAX_PART];

uint8_t  efecto        = 0;
uint8_t  hueGiro       = 0;                   // gira toda la paleta en cada sorteo
bool     dirInv        = false;               // dirección del efecto
uint8_t  paramEfecto   = 0;                   // parámetro propio de cada efecto
uint8_t  ordenBandas[NUM_BLOQUES];            // mapa paso → bloque de la tira (se baraja)
uint32_t pasoUltimoSorteo = 0;
float    faseFx        = 0.0f;                // fase continua de los efectos que corren
float    golpe         = 0.0f;                // golpe del paso, decae solo
float    golpeAcento   = 0.0f;                // idem, sólo en los pasos 1 y 5
uint8_t  hueNota       = 0;                   // color de la nota que está sonando

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
#define BTN1_PIN   44
#define BTN2_PIN   42
#define BTN3_PIN    0
#define BTN4_PIN   45
#define BTN5_PIN   47
const unsigned long DEBOUNCE_MS = 150;

// ─── Potenciómetros ────────────────────────────────────────
#define POT1   1    // ADC1
#define POT2   2    // ADC2
#define POT3   8    // ADC8
#define POT4  10    // ADC10
const uint8_t POT_PIN[4] = { POT1, POT2, POT3, POT4 };

// ==============================================================================================================================================
// AJUSTE DEL FSR  ← ESTO ES LO QUE SE TOCA
// ==============================================================================================================================================
#define FSR_PIN   3       // Sensor externo A (EXT1) — ADC1_CH2 (el ADC2 no se puede usar con WiFi)

// Los dos extremos son LECTURAS MEDIDAS, no constantes mágicas: pon MOSTRAR_ESTADO en 1,
// mira `raw` con el FSR suelto y apretado, y copia esos dos números acá. Como el mapeo es
// una recta entre los dos, la POLARIDAD del divisor da lo mismo: si cableaste el FSR abajo
// y la resistencia arriba, pon FSR_SUELTO mayor que FSR_APRETADO y todo sigue igual.
int FSR_SUELTO   = 0;     // lectura con el FSR SIN apretar     (medido con MOSTRAR_ESTADO 1)
int FSR_APRETADO = 4095;  // lectura apretando lo que vayas a apretar tocando

const float FSR_ZONA_MUERTA = 0.06f;    // por debajo de esto el FSR no hace nada (ruido del ADC)
const float FSR_SUAVIZADO   = 0.25f;    // filtro de la lectura a 1 kHz: 1.0 = crudo, 0.1 = muy suave
const float FSR_OCT         = 3.0f;     // cuántas octavas abre el filtro a fondo de presión
#define FSR_REEMPLAZA_POT 0             // 1 = el FSR ES el cutoff y la resonancia (el POT4 deja de hacer nada)

// ==============================================================================================================================================
// MIDI DIN-5 — sólo reloj (Clock 24 PPQN + Start/Stop), nunca notas
// ==============================================================================================================================================
#define MIDI_TX_PIN   43          // TX del DIN-5
#define MIDI_RX_PIN   -1          // sin RX: su pin natural (44) es el BTN1
#define MIDI_BAUD     31250
HardwareSerial &MIDIOUT = Serial1;   // Serial0 comparte el 43 con el DIN-5 y el 44 con BTN1
const bool ENVIAR_MIDI_CLOCK = true; // false si tu sinte se vuelve loco con clock entrante

const uint8_t MIDI_CLOCK = 0xF8;
const uint8_t MIDI_START = 0xFA;
const uint8_t MIDI_STOP  = 0xFC;
#define MIDI_PPQN        24
#define PASOS_POR_PULSO   4       // cada paso es una semicorchea ⇒ 6 ticks por paso

// ==============================================================================================================================================
// MOSTRAR_ESTADO — sólo para calibrar el FSR. Déjalo en 0 para tocar.
// ==============================================================================================================================================
// Un print por USB CDC bloquea el core que lo hace hasta que el PC lea, y eso vacía el DMA
// del audio (se oye como un clic). En 0 no se compila ni una línea de Serial.
#define MOSTRAR_ESTADO 0
#if MOSTRAR_ESTADO
  #define LOG(...)   Serial.print(__VA_ARGS__)
  #define LOGLN(...) Serial.println(__VA_ARGS__)
#else
  #define LOG(...)
  #define LOGLN(...)
#endif

// ==============================================================================================================================================
// ESCALA — La pentatónica menor, 2 octavas + la tónica de arriba = 11 notas por pot
// ==============================================================================================================================================
#define NUM_PASOS   8
#define NUM_NOTAS  11
const int8_t PENTA_MENOR[5] = { 0, 3, 5, 7, 10 };    // La · Do · Re · Mi · Sol
const float  FREC_BASE      = 55.0f;                 // La1
const float  ZONA_SILENCIO  = 0.03f;                 // pot bajo esto = paso en silencio
#define NUM_OCTAVAS 3                                // BTN5: La1 · La2 · La3

// nota (0..10) + octava → semitonos sobre La1
inline int notaASemitono(int nota, int octava) {
  return octava * 12 + (nota / 5) * 12 + PENTA_MENOR[nota % 5];
}

// ==============================================================================================================================================
// TIMBRE (fijo) — lo que no está en un pot se ajusta acá
// ==============================================================================================================================================
const float DESAFINE_CENTS = 9.0f;      // segunda sierra
const float NIVEL_SIERRAS  = 0.50f;     // cada sierra
const float NIVEL_SUB      = 0.32f;     // cuadrada una octava abajo (cuerpo)
const float DRIVE          = 1.25f;     // saturación suave antes del filtro
const float ENV_FILTRO_OCT = 3.2f;      // cuánto abre el filtro cada nota (octavas)
const float RES_MIN        = 0.9f;      // Q con el cutoff arriba
const float RES_MAX        = 2.6f;      // Q con el cutoff abajo (la resonancia va atada al corte)
const float FILTRO_TECHO   = 12000.0f;
const float TAU_SOLTAR     = 0.025f;    // paso en silencio / stop: cierre de 25 ms
const float ATAQUE_S       = 0.002f;    // ataque de amplitud
const float ATAQUE_FILTRO_S = 0.0015f;  // ataque de la envolvente de filtro (anti-clic)
const float MASTER         = 0.80f;

const float DRIVE_COMP = 1.0f / (0.75f + 0.25f * DRIVE);
const float SUAVIZADO  = 1.0f - expf(-1.0f / (0.012f * SAMPLE_RATE));   // pots → 12 ms

// ==============================================================================================================================================
// PEDIDOS ENTRE NÚCLEOS
// ==============================================================================================================================================
// control → audio : notaPaso[]  pVol  pVel  pDecay  pCorte  octava  tocando
// audio → control : pasoSonando  contadorPasos
// Son escrituras de 32 bits alineadas, atómicas en el S3: no hace falta mutex.
volatile int   notaPaso[NUM_PASOS] = { 0, 5, 0, 3,  0, 4, 2, 1 };  // patrón de arranque
volatile float pVol   = 0.70f;          // 0..1
volatile float pVel   = 0.43f;          // 0..1 → 60..200 BPM (0.43 ≈ 120)
volatile float pDecay = 0.40f;          // 0..1 → 30 ms .. 1.2 s
volatile float pCorte = 0.45f;          // 0..1 → 50 Hz .. 6 kHz
volatile int   octava = 0;              // 0..2
volatile bool  tocando = false;
volatile float pFsr   = 0.0f;           // 0..1 — presión del FSR (control → audio, 1 kHz)
volatile int      pasoSonando   = -1;   // −1 = detenido
volatile uint32_t contadorPasos = 0;    // sube en cada paso (latido de los LEDs)
volatile float    g_env         = 0.0f; // envolvente del bajo 0..1 (audio → LEDs, 1 por buffer)

// Reloj MIDI: el audio CUENTA los ticks (24 por negra, con el MISMO contador de muestras
// que los pasos ⇒ cero deriva entre el secuenciador y el clock) y la tarea de control los
// MANDA a 1 kHz. Mandarlos desde el render le metería al clock los ±2,9 ms de fluctuación
// del buffer, que a 120 BPM es un 14 % de un tick y se oye como un arpegio que se tambalea.
volatile uint32_t midiTicks     = 0;    // ticks generados desde el arranque
volatile uint32_t midiStartTick = 0;    // índice del tick que es el "1" del Start pedido
volatile bool     reqMidiStart  = false;
volatile bool     reqMidiStop   = false;
uint32_t tickAcc = 0;                   // acumulador (tarea de audio): 24 por muestra vs muestras/negra

// ─── Estado de la tarea de AUDIO ───────────────────────────
bool     aTocando    = false;
int      aOctava     = 0;
int      pasoAudio   = 0;               // próximo paso a disparar
uint32_t muestrasPaso = 0;              // muestras que faltan para el próximo paso
int      notaActual  = -1;              // nota que está sonando (para re-afinar al cambiar octava)

float frec = 110.0f;
float fase1 = 0.0f, fase2 = 0.37f, faseSub = 0.0f;
float razonDesafine = 1.0f;

float env = 0.0f;       bool atacando = false;   bool soltando = false;
float envF = 0.0f;      bool atacandoF = false;
float mulDecay = 0.999f, mulDecayF = 0.999f, mulSoltar = 0.999f;

float volS = 0.7f, corteS = 0.45f, fsrS = 0.0f;   // parámetros suavizados por muestra
float svfG = 0.1f, svfK = 1.0f, svfA1 = 1.0f, svfA2 = 0.0f, svfA3 = 0.0f, compQ = 1.0f;
EstadoSVF svf = { 0.0f, 0.0f };
float dcX1 = 0.0f, dcY1 = 0.0f;

// ─── Estado de la tarea de CONTROL ─────────────────────────
#define PANEL_A 0
#define PANEL_B 1
#define PANEL_C 2
int   panel = PANEL_A;
float potEntrada[4];                    // lectura al entrar al panel
bool  potTomado[4];                     // true = el pot ya se movió y manda
int   ctlNota[NUM_PASOS];               // última nota cuantizada (histéresis)
const float UMBRAL_TOMA = 0.05f;        // cuánto hay que mover un pot para que tome el control

bool bNivel[5] = { HIGH, HIGH, HIGH, HIGH, HIGH };
unsigned long bTiempo[5] = { 0, 0, 0, 0, 0 };
const uint8_t BTN_PIN[5] = { BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN, BTN5_PIN };

float fsrFilt = 0.0f;                   // lectura del FSR filtrada (0..1), core 0
int   fsrRawMin = 4095, fsrRawMax = 0;  // recorrido visto desde el arranque (para calibrar)

float flashNivel = 0.0f;                // destello al cambiar panel / octava / play
float pulsoPaso  = 0.0f;

static i2s_chan_handle_t tx_chan;

// ─── FSR (core 0, cada pasada = 1 kHz) ─────────────────────────────────────
// Una sola lectura cruda por pasada (cada `analogRead` en el S3 cuesta decenas de
// microsegundos): el promediado lo hace el filtro de un polo, que además es lo que evita
// que el ruido del ADC se oiga como un temblor en el filtro.
void leerFSR() {
  int raw = analogRead(FSR_PIN);
  if (raw < fsrRawMin) fsrRawMin = raw;
  if (raw > fsrRawMax) fsrRawMax = raw;

  // Recta entre las dos lecturas medidas ⇒ la polaridad del divisor da lo mismo.
  float span = (float)(FSR_APRETADO - FSR_SUELTO);
  if (fabsf(span) < 1.0f) span = 1.0f;
  float f = ((float)raw - (float)FSR_SUELTO) / span;
  if (f < 0.0f) f = 0.0f;
  if (f > 1.0f) f = 1.0f;

  // Zona muerta abajo: suelto es suelto, y el sonido queda idéntico al bajo_8_pasos.
  if (f < FSR_ZONA_MUERTA) f = 0.0f;
  else                     f = (f - FSR_ZONA_MUERTA) / (1.0f - FSR_ZONA_MUERTA);

  fsrFilt += FSR_SUAVIZADO * (f - fsrFilt);
  pFsr = fsrFilt;
}

// ─── Reloj MIDI: manda lo que el audio contó (1 kHz, sin bloquear) ──────────────────
bool     midiRunning = false;
uint32_t ticksSent   = 0;

void enviarMidi() {
  if (!ENVIAR_MIDI_CLOCK) return;

  if (reqMidiStop) {                           // Stop: el audio ya cerró la nota
    reqMidiStop = false;
    if (midiRunning) { MIDIOUT.write(MIDI_STOP); midiRunning = false; }
  }
  if (reqMidiStart) {                          // Play: el "1" es el paso 1
    reqMidiStart = false;
    ticksSent = midiStartTick - 1;             // los ticks de antes del "1" no van
    if (midiRunning) MIDIOUT.write(MIDI_STOP); // hay equipos que ignoran un Start si ya corren
    MIDIOUT.write(MIDI_START);
    midiRunning = true;
  }

  // Los ticks salen SIEMPRE, aun parado: así el sinte externo ya tiene el tempo
  // cuando llegue el Start y no arranca buscando el pulso.
  uint32_t t = midiTicks;
  uint32_t n = t - ticksSent;
  if (n > 8) n = 8;                            // por si el tempo salta: nunca inundar el UART
  for (uint32_t i = 0; i < n; i++) MIDIOUT.write(MIDI_CLOCK);
  ticksSent = t;
}

// ─── Lectura de pot con sobre-muestreo (core 0: no le roba tiempo al audio) ──
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (float)(sum >> 3) / 4095.0f;
}

// ==============================================================================================================================================
// SÍNTESIS
// ==============================================================================================================================================

// ─── PolyBLEP: corrige el salto de la sierra → sin aliasing ──
inline float polyBlep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  else if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

inline float sierra(float fase, float dt) {
  return (2.0f * fase - 1.0f) - polyBlep(fase, dt);
}

inline float cuadrada(float fase, float dt) {
  float p2 = fase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
  return (sierra(fase, dt) - sierra(p2, dt)) * 0.5f;
}

// ─── Saturación suave (aproximación racional de tanh) ──────
inline float softClip(float x) {
  if (x >  3.0f) x =  3.0f;
  if (x < -3.0f) x = -3.0f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

// ─── Mapeos de los parámetros del panel C (tarea de audio) ──
inline float bpmDe(float v)      { return 60.0f + v * 140.0f; }
inline float tauDecayDe(float v) { return 0.03f * powf(40.0f, v); }
inline float corteHzDe(float v)  { return 50.0f * powf(120.0f, v); }

// ─── Disparar un paso (tarea de audio) ─────────────────────
void dispararPaso(int i) {
  int n = notaPaso[i];
  pasoSonando = i;
  contadorPasos++;
  if (n < 0) {                          // paso en silencio: se cierra la nota anterior
    soltando = true;
    atacando = false;
    return;
  }
  notaActual = n;
  frec = FREC_BASE * powf(2.0f, (float)notaASemitono(n, aOctava) / 12.0f);
  // La fase NO se reinicia y el ataque sube desde donde esté la envolvente:
  // una nota encima de otra no produce saltos en la onda → sin clic.
  atacando  = true;
  soltando  = false;
  atacandoF = true;
}

// ─── Coeficientes del SVF (cada 8 muestras) ────────────────
void calcularFiltro() {
  // El FSR viaja en el MISMO exponente que la envolvente: no cuesta ni un powf extra
  // (y `powf` en el S3 se paga caro — ver la lección de `la_partida`).
#if FSR_REEMPLAZA_POT
  float mando = fsrS;                                 // el FSR ES el cutoff
  float oct   = ENV_FILTRO_OCT * envF;
#else
  float mando = corteS;                               // el POT4 pone el reposo…
  float oct   = ENV_FILTRO_OCT * envF + FSR_OCT * fsrS;// …y el FSR abre por encima
#endif
  float fc = corteHzDe(mando) * powf(2.0f, oct);
  if (fc > FILTRO_TECHO) fc = FILTRO_TECHO;
  if (fc < 20.0f) fc = 20.0f;

  // La resonancia va atada a QUIEN mande el corte: si no, con el filtro abierto a fondo
  // queda un pico resonante arriba, que es exactamente lo que suena áspero.
  float q = RES_MAX - (RES_MAX - RES_MIN) * mando;
  svfG  = tanf((float)M_PI * fc * INV_SR);
  svfK  = 1.0f / q;
  svfA1 = 1.0f / (1.0f + svfG * (svfG + svfK));
  svfA2 = svfG * svfA1;
  svfA3 = svfG * svfA2;
  compQ = 1.0f / powf(q, 0.30f);
}

// SVF de Zavalishin (TPT): estable aunque el corte cambie rápido
inline float filtroLP(EstadoSVF &s, float x) {
  float v3 = x - s.ic2;
  float v1 = svfA1 * s.ic1 + svfA2 * v3;
  float v2 = s.ic2 + svfA2 * s.ic1 + svfA3 * v3;
  s.ic1 = 2.0f * v1 - s.ic1;
  s.ic2 = 2.0f * v2 - s.ic2;
  return v2;
}

// ==============================================================================================================================================
// LEDs (tarea de control)
// ==============================================================================================================================================
const uint8_t HUE_PANEL[3] = { 140, 195, 18 };   // A cian · B violeta · C naranjo

// ─── Tira externa: balas de luz ────────────────────────────
inline void pixelTira(int i, const CRGB &c) {       // i = 0..TIRA_LEDS-1 (suma, no pisa)
  if (i >= 0 && i < TIRA_LEDS) leds[STATUS_LEDS + i] += c;
}

// ─── Limitador de corriente de la tira ─────────────────
// Mide lo que el frame va a consumir y lo baja si se pasa. Es un limitador igual que el del
// audio, y por el mismo motivo: **baja al instante** (pasarse aunque sea un frame es el
// bajón de tensión) y **sube despacio**, porque un escalón de corriente hacia arriba es
// justo lo que hace caer el riel. No toca los 6 LEDs de estado.
void limitarCorrienteTira() {
  uint32_t suma = 0;
  for (int i = STATUS_LEDS; i < NUM_LEDS; i++) suma += leds[i].r + leds[i].g + leds[i].b;
  float carga = (float)suma / ((float)TIRA_LEDS * 3.0f * 255.0f);

  float objetivo = (carga > CARGA_MAXIMA) ? (CARGA_MAXIMA / carga) : 1.0f;
  if (objetivo < gananciaTira) gananciaTira = objetivo;                       // ataque: al tiro
  else gananciaTira += (objetivo - gananciaTira) * RECUPERA_LED;              // recuperación: lenta

  if (gananciaTira < 0.999f) {
    uint8_t g = (uint8_t)(gananciaTira * 255.0f);
    for (int i = STATUS_LEDS; i < NUM_LEDS; i++) leds[i].nscale8(g);
  }
}

inline void borrarTira() { for (int i = STATUS_LEDS; i < NUM_LEDS; i++) leds[i] = CRGB(0, 0, 0); }
inline void fundirTira(uint8_t c) { fadeToBlackBy(leds + STATUS_LEDS, TIRA_LEDS, c); }

// Dibuja un punto con sub-píxel: el reparto entre dos LEDs es lo que hace que algo que
// avanza se vea deslizarse en vez de saltar de LED en LED.
inline void puntoSuave(float x, const CRGB &c) {
  int i0 = (int)floorf(x);
  float fr = x - (float)i0;
  pixelTira(i0,     c.scale8((uint8_t)((1.0f - fr) * 255.0f)));
  pixelTira(i0 + 1, c.scale8((uint8_t)(fr * 255.0f)));
}

// Slot libre; si no hay, se roba la partícula más vieja (la que más lejos llegó).
Particula &nuevaParticula() {
  int slot = 0;
  float masLejos = -1.0f;
  for (int i = 0; i < MAX_PART; i++) {
    if (!part[i].viva) { slot = i; break; }
    if (part[i].pos > masLejos) { masLejos = part[i].pos; slot = i; }
  }
  return part[slot];
}

// ─── Sorteo del efecto ─────────────────────────────────────
// Nunca repite el que estaba sonando (repetir se nota y arruina la sorpresa), y se lleva
// por delante las partículas del anterior para que no queden colgando en pantalla.
#if MOSTRAR_ESTADO
const char *NOMBRE_EFECTO[NUM_EFECTOS] = { "BALAS", "ONDAS", "VU", "CHISPAS",
                                           "ARCOIRIS", "PLASMA", "BANDAS", "SERPIENTE" };
#endif

void sortearEfecto() {
  uint8_t nuevo = efecto;
  while (nuevo == efecto) nuevo = random8(NUM_EFECTOS);
  efecto      = nuevo;
  LOG("efecto -> "); LOGLN(NOMBRE_EFECTO[nuevo]);
  hueGiro     = random8();
  dirInv      = (random8() & 1);
  paramEfecto = random8();
  for (int i = 0; i < NUM_BLOQUES; i++) ordenBandas[i] = i;
  for (int i = NUM_BLOQUES - 1; i > 0; i--) {        // baraja: los bloques no van en orden
    int j = random8(i + 1);
    uint8_t t = ordenBandas[i]; ordenBandas[i] = ordenBandas[j]; ordenBandas[j] = t;
  }
  for (int i = 0; i < MAX_PART; i++) part[i].viva = false;
  faseFx = 0.0f;
  borrarTira();
}

// ─── Lo que pasa en cada paso que suena ────────────────────
void golpeDePaso(int paso, int nota, bool acento) {
  golpe = 1.0f;
  if (acento) golpeAcento = 1.0f;
  hueNota = HUE_PASO[paso] + (uint8_t)(nota * 5);

  // La presión del FSR entra acá, en el momento del disparo: los proyectiles y los
  // anillos nacen más largos cuanto más aprietas.
  uint8_t extra = (uint8_t)(fsrFilt * 16.0f);

  if (efecto == 0) {                                  // BALAS: una bala por paso
    Particula &p = nuevaParticula();
    p.viva = true; p.pos = 0.0f; p.vel = 1.0f; p.origen = 0.0f;
    p.hue = hueNota; p.brillo = acento ? 255 : 170;
    p.largo = (acento ? 11 : 6) + extra;
  } else if (efecto == 1) {                           // ONDAS: un anillo por paso
    Particula &p = nuevaParticula();
    p.viva = true; p.pos = 0.0f;
    p.vel = (acento ? 1.5f : 1.0f) + fsrFilt;         // apretar acelera la onda
    p.origen = (float)random16(TIRA_LEDS);
    p.hue = hueNota; p.brillo = acento ? 255 : 190;
    p.largo = (acento ? 9 : 5) + extra;
  }
}

// ══════════════════════════════════════════════════════════════════════════════════════════════
// LOS OCHO EFECTOS
// ══════════════════════════════════════════════════════════════════════════════════════════════
// `avance` = LEDs que hay que recorrer en este frame para ir al ritmo del tempo.
// `env`    = envolvente del bajo (0..1). `fsr` = presión del FSR (0..1).

// 0 · BALAS — cada paso dispara un proyectil con estela hacia el final de la tira.
void fxBalas(float avance) {
  borrarTira();
  for (int k = 0; k < MAX_PART; k++) {
    Particula &p = part[k];
    if (!p.viva) continue;
    p.pos += avance * p.vel;
    if (p.pos - (float)p.largo > (float)TIRA_LEDS) { p.viva = false; continue; }
    for (int j = 0; j <= p.largo; j++) {
      float x = p.pos - (float)j;
      if (x < 0.0f) break;
      float caida = 1.0f - (float)j / (float)(p.largo + 1);
      uint8_t v = (uint8_t)((float)p.brillo * caida * caida);   // estela cuadrática
      puntoSuave(x, CHSV(p.hue, (j == 0) ? 90 : 255, v));       // punta casi blanca
    }
  }
}

// 1 · ONDAS — cada paso abre un anillo que crece hacia los dos lados desde un punto al azar.
void fxOndas(float avance) {
  fundirTira(55);
  for (int k = 0; k < MAX_PART; k++) {
    Particula &p = part[k];
    if (!p.viva) continue;
    p.pos += avance * p.vel;
    if (p.pos > (float)TIRA_LEDS) { p.viva = false; continue; }
    float desvanece = 1.0f - p.pos / (float)TIRA_LEDS;          // se apaga al alejarse
    for (int j = 0; j < p.largo; j++) {
      float caida = 1.0f - (float)j / (float)p.largo;
      uint8_t v = (uint8_t)((float)p.brillo * caida * caida * desvanece);
      CRGB c = CHSV(p.hue + (uint8_t)(j * 3), 235, v);
      puntoSuave(p.origen + p.pos - (float)j, c);
      puntoSuave(p.origen - p.pos + (float)j, c);
    }
  }
}

// 2 · VU ESPEJO — una barra que crece desde el centro con la envolvente, con punta blanca.
void fxVU(float env, float fsr) {
  borrarTira();
  const int mitad = TIRA_LEDS / 2;
  int largo = (int)(env * (float)mitad * (1.0f + fsr * 0.6f));  // apretar la estira
  if (largo > mitad) largo = mitad;
  for (int i = 0; i < largo; i++) {
    uint8_t v = 45 + (uint8_t)(210.0f * (float)i / (float)mitad);
    CRGB c = CHSV(hueNota + hueGiro + (uint8_t)(i * 2), 240, v);
    pixelTira(mitad - 1 - i, c);
    pixelTira(mitad + i, c);
  }
  if (golpe > 0.04f) {                                          // la punta destella
    uint8_t w = (uint8_t)(golpe * 255.0f);
    pixelTira(mitad - 1 - largo, CRGB(w, w, w));
    pixelTira(mitad + largo,     CRGB(w, w, w));
  }
}

// 3 · CHISPAS — cada paso enciende un puñado de píxeles al azar que se apagan solos.
void fxChispas(float env, float fsr) {
  fundirTira(36);
  if (golpe > 0.55f) {
    int n = 4 + (int)((0.35f + env) * 14.0f * (golpeAcento > 0.5f ? 1.8f : 1.0f));
    for (int i = 0; i < n; i++)
      pixelTira(random16(TIRA_LEDS),
                CHSV(hueNota + hueGiro + random8(40), 190 + random8(66), 170 + random8(86)));
  }
  // Polvo blanco permanente mientras aprietas: la presión se ve aunque no suene nada.
  int polvo = (int)(fsr * fsr * 10.0f);
  for (int i = 0; i < polvo; i++) pixelTira(random16(TIRA_LEDS), CRGB(255, 255, 255));
}

// 4 · ARCOÍRIS — todo el espectro corriendo al tempo; el FSR lo comprime y el golpe lo blanquea.
void fxArcoiris(float avance, float env, float fsr) {
  float compresion = 1.0f + fsr * 5.0f;
  uint8_t brillo   = 35 + (uint8_t)(env * 220.0f);
  for (int i = 0; i < TIRA_LEDS; i++) {
    uint8_t h = hueGiro + (uint8_t)(faseFx * 3.0f + (float)i * compresion * 1.7f);
    leds[STATUS_LEDS + i] = CHSV(h, 235, brillo);
  }
  if (golpe > 0.04f) {                                          // banda blanca que viaja
    float x = dirInv ? (float)TIRA_LEDS * (1.0f - golpe) : (float)TIRA_LEDS * golpe;
    uint8_t w = (uint8_t)(golpe * 220.0f);
    for (int j = -2; j <= 2; j++) pixelTira((int)x + j, CRGB(w / 2, w / 2, w / 2));
  }
}

// 5 · PLASMA — campo de ruido que respira; la presión lo calienta y lo corre de color.
void fxPlasma(float env, float fsr) {
  uint16_t t = (uint16_t)(faseFx * (9.0f + fsr * 16.0f));       // apretar lo acelera
  uint8_t  escala = 18 + (paramEfecto >> 3);                    // cada sorteo lo ve distinto
  uint8_t  piso   = (uint8_t)(36.0f * (1.0f - fsr));            // …y lo "calienta": el ruido
  uint8_t  ganancia = 55 + (uint8_t)(env * 200.0f);             //   deja de recortarse abajo
  for (int i = 0; i < TIRA_LEDS; i++) {
    uint8_t n = inoise8((uint16_t)i * escala, t);
    uint8_t h = hueGiro + (hueNota >> 1) + (n >> 2) + (uint8_t)(fsr * 70.0f);
    uint8_t v = scale8(qsub8(n, piso), ganancia);
    leds[STATUS_LEDS + i] = CHSV(h, 225 - (uint8_t)(fsr * 60.0f), v);
  }
}

// 6 · BANDAS — la tira partida en 8 bloques, uno por paso, en orden barajado. Muy directo:
//     se ve exactamente qué paso está sonando.
void fxBandas(int paso, float env, float fsr) {
  fundirTira(85);
  if (paso >= 0 && paso < NUM_BLOQUES) {
    int ancho = TIRA_LEDS / NUM_BLOQUES;
    int i0 = ordenBandas[paso] * ancho;
    uint8_t v = 40 + (uint8_t)(env * 215.0f);
    // El bloque desborda a los vecinos con la presión: apretar a fondo llena la tira.
    int desborde = (int)(fsr * (float)ancho * 2.5f);
    for (int i = -desborde; i < ancho + desborde; i++) {
      float lejos = (i < 0) ? (float)(-i) : ((i >= ancho) ? (float)(i - ancho + 1) : 0.0f);
      float caida = 1.0f - lejos / (float)(desborde + 1);
      if (caida <= 0.0f) continue;
      pixelTira(i0 + i, CHSV(hueNota + hueGiro, 235, (uint8_t)((float)v * caida)));
    }
  }
  if (golpeAcento > 0.04f) {                                    // el "1" destella entera
    uint8_t w = (uint8_t)(golpeAcento * 110.0f);
    for (int i = 0; i < TIRA_LEDS; i++) pixelTira(i, CRGB(w, w, w));
  }
}

// 7 · SERPIENTE — una cabeza blanca que va y vuelve al tempo; el FSR le alarga la cola.
void fxSerpiente(float avance, float fsr) {
  fundirTira(42);
  faseFx += avance;
  float ciclo = fmodf(faseFx, (float)(TIRA_LEDS * 2));
  float cab = (ciclo < (float)TIRA_LEDS) ? ciclo : (float)(TIRA_LEDS * 2) - ciclo;  // rebota
  int largo = 6 + (int)(fsr * 34.0f) + (int)(golpe * 10.0f);
  for (int j = 0; j < largo; j++) {
    float caida = 1.0f - (float)j / (float)largo;
    uint8_t v = (uint8_t)(255.0f * caida * caida);
    float x = dirInv ? cab + (float)j : cab - (float)j;
    puntoSuave(x, (j == 0) ? CRGB(255, 255, 255)
                           : (CRGB)CHSV(hueGiro + hueNota + (uint8_t)(j * 3), 240, v));
  }
}

// ─── Despachador ───────────────────────────────────────────
void renderTira(unsigned long dtMs) {
  if (dtMs > 60) dtMs = 60;                         // un frame perdido no da un salto feo
  float msPaso = 60000.0f / ((60.0f + pVel * 140.0f) * 4.0f);
  float avance = LEDS_POR_PASO * (float)dtMs / msPaso;   // LEDs por frame al tempo actual
  float env = g_env;
  float fsr = fsrFilt;

  // El golpe decae en ~90 ms pase lo que pase con el frame rate.
  float dec = expf(-(float)dtMs / 90.0f);
  golpe *= dec;
  golpeAcento *= dec;

  switch (efecto) {
    case 0: fxBalas(avance);                 faseFx += avance; break;
    case 1: fxOndas(avance);                 faseFx += avance; break;
    case 2: fxVU(env, fsr);                  faseFx += avance; break;
    case 3: fxChispas(env, fsr);             faseFx += avance; break;
    case 4: fxArcoiris(avance, env, fsr);    faseFx += avance; break;
    case 5: fxPlasma(env, fsr);              faseFx += avance; break;
    case 6: fxBandas(pasoSonando, env, fsr); faseFx += avance; break;
    case 7: fxSerpiente(avance, fsr);        break;   // éste ya mueve faseFx por dentro
  }
}

void renderLEDs() {
  unsigned long t = millis();
  static unsigned long ultimoFrame = 0;
  static uint32_t ultimoPaso = 0;
  if (t - ultimoFrame < LED_REFRESH_MS) return;
  unsigned long dtMs = t - ultimoFrame;
  ultimoFrame = t;

  int ps = pasoSonando;
  uint32_t cp = contadorPasos;
  if (cp != ultimoPaso) {
    ultimoPaso = cp;
    bool acento = (ps == 0 || ps == 4);            // los pasos 1 y 5 marcan el compás
    pulsoPaso = acento ? 1.0f : 0.45f;
    if (tocando && ps >= 0) {
      int n = notaPaso[ps];
      if (n >= 0) golpeDePaso(ps, n, acento);      // un paso en silencio no dispara nada
    }
    // Efecto nuevo cada 4 compases: la tira no se queda nunca en lo mismo.
    if (cp - pasoUltimoSorteo >= PASOS_POR_SORTEO) { pasoUltimoSorteo = cp; sortearEfecto(); }
  }
  renderTira(dtMs);

  if (panel == PANEL_C) {
    const float val[4] = { pVol, pVel, pDecay, pCorte };
    const uint8_t hue[4] = { 96, 64, 170, 18 };
    for (int i = 0; i < 4; i++)
      leds[i] = CHSV(hue[i], 230, 10 + (uint8_t)(val[i] * 200.0f));
  } else {
    int base = (panel == PANEL_A) ? 0 : 4;
    for (int i = 0; i < 4; i++) {
      int n = notaPaso[base + i];
      if (n < 0) leds[i] = CRGB(0, 0, 0);
      else       leds[i] = CHSV((uint8_t)(n * 22), 240, 70);
      if (tocando && ps == base + i) leds[i] = CRGB(200, 200, 200);
    }
  }

  // LED 4 → panel activo; el BRILLO sigue al FSR (se ve si el sensor responde)
  leds[4] = CHSV(HUE_PANEL[panel], 255, (uint8_t)(110.0f + fsrFilt * 145.0f));

  // LED 5 → octava + pulso de cada paso
  const uint8_t OCT_VAL[NUM_OCTAVAS] = { 40, 90, 160 };
  leds[5] = tocando ? CHSV(96, 220, OCT_VAL[octava]) : CHSV(0, 220, OCT_VAL[octava] / 3);
  if (tocando && pulsoPaso > 0.02f) {
    uint8_t p = (uint8_t)(pulsoPaso * 200.0f);
    leds[5] += CRGB(p, p, p);
  }
  pulsoPaso *= 0.6f;

  if (flashNivel > 0.02f) {
    uint8_t w = (uint8_t)(flashNivel * 140.0f);
    for (int i = 0; i < STATUS_LEDS; i++) leds[i] += CRGB(w, w, w);   // el destello no toca la tira
    flashNivel *= 0.55f;
  }

  limitarCorrienteTira();      // lo último antes de mandar el frame
  FastLED.show();
}

// ==============================================================================================================================================
// CONTROLES (tarea de control)
// ==============================================================================================================================================

// Nota cuantizada con HISTÉRESIS: el ruido del ADC no hace saltar la nota en los bordes.
int cuantizarNota(int paso, float v) {
  if (v < ZONA_SILENCIO) return -1;
  float x = (v - ZONA_SILENCIO) / (1.0f - ZONA_SILENCIO);
  float f = x * (float)(NUM_NOTAS - 1);
  int cand = (int)(f + 0.5f);
  if (cand < 0) cand = 0;
  if (cand > NUM_NOTAS - 1) cand = NUM_NOTAS - 1;
  int cur = ctlNota[paso];
  if (cur >= 0 && cand != cur && fabsf(f - (float)cur) < 0.65f) cand = cur;
  return cand;
}

// Al entrar a un panel se anota dónde está cada pot; ninguno manda hasta que se mueva.
void entrarPanel(int p) {
  panel = p;
  for (int i = 0; i < 4; i++) {
    potEntrada[i] = readPot(POT_PIN[i]);
    potTomado[i]  = false;
  }
  flashNivel = 0.5f;
}

void aplicarPot(int i, float v) {
  switch (panel) {
    case PANEL_A:
    case PANEL_B: {
      int paso = (panel == PANEL_A ? 0 : 4) + i;
      int n = cuantizarNota(paso, v);
      if (n != ctlNota[paso]) { ctlNota[paso] = n; notaPaso[paso] = n; }
      break;
    }
    case PANEL_C:
      if      (i == 0) pVol   = v;
      else if (i == 1) pVel   = v;
      else if (i == 2) pDecay = v;
      else             pCorte = v;
      break;
  }
}

void pasoControl() {
  unsigned long tms = millis();

  // ── Botones: flanco de presión, uno por función ──
  for (int b = 0; b < 5; b++) {
    bool nivel = digitalRead(BTN_PIN[b]);
    if (nivel == LOW && bNivel[b] == HIGH && (tms - bTiempo[b]) > DEBOUNCE_MS) {
      bTiempo[b] = tms;
      // El instante exacto de una pulsación es la mejor fuente de azar que hay en una
      // placa sin reloj: sin esto, los "efectos aleatorios" salen siempre en el mismo orden.
      random16_add_entropy((uint16_t)micros());
      switch (b) {
        case 0:                                                   // BTN1 play / stop
          tocando = !tocando;
          if (tocando) { sortearEfecto(); pasoUltimoSorteo = contadorPasos; }
          flashNivel = 0.4f;
          break;
        case 1: entrarPanel(PANEL_A); break;                      // BTN2 panel A
        case 2: entrarPanel(PANEL_B); break;                      // BTN3 panel B
        case 3: entrarPanel(PANEL_C); break;                      // BTN4 panel C
        case 4: octava = (octava + 1) % NUM_OCTAVAS; flashNivel = 0.7f; break;  // BTN5
      }
    }
    bNivel[b] = nivel;
  }

  // ── Pots: uno por pasada en rotación ──
  static uint8_t scan = 0;
  int i = scan; scan = (scan + 1) & 3;
  float v = readPot(POT_PIN[i]);
  if (!potTomado[i] && fabsf(v - potEntrada[i]) > UMBRAL_TOMA) potTomado[i] = true;
  if (potTomado[i]) aplicarPot(i, v);

  // ── FSR → filtro (cada pasada) y reloj MIDI por el DIN-5 ──
  leerFSR();
  enviarMidi();

#if MOSTRAR_ESTADO
  static unsigned long ultimoLog = 0;
  if (tms - ultimoLog > 250) {
    ultimoLog = tms;
    LOG("FSR raw="); LOG(analogRead(FSR_PIN));
    LOG("  min=");   LOG(fsrRawMin);
    LOG("  max=");   LOG(fsrRawMax);
    LOG("  ->");     LOG(fsrFilt, 3);
    LOG("   (FSR_SUELTO="); LOG(FSR_SUELTO);
    LOG(" FSR_APRETADO=");  LOG(FSR_APRETADO);
    LOG(")  BPM=");  LOG((int)bpmDe(pVel));
    LOG("  ticks="); LOG(midiTicks);
    LOG("  efecto="); LOG(NOMBRE_EFECTO[efecto]);
    LOG("  carga tira="); LOGLN(gananciaTira, 2);   // < 1.00 = el limitador esta actuando
  }
#endif

  renderLEDs();
}

// ==============================================================================================================================================
// SETUP
// ==============================================================================================================================================
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;   // ≈ 12 ms de cola: el Play suena en el acto
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

void renderBuffer();
#ifndef SIMULADOR
void audioTask(void *);
void controlTask(void *);
#endif

void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);

#if MOSTRAR_ESTADO
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);          // sin esto un print bloquea el core hasta que el PC lea
  LOGLN("");
  LOGLN("BAJO 8 PASOS + FSR + RELOJ MIDI - GC Lab Chile");
  LOGLN("Aprieta y suelta el FSR: anota 'raw' suelto (-> FSR_SUELTO) y apretado (-> FSR_APRETADO).");
#endif

  // MIDI DIN-5: sólo TX. El RX iría en el 44, que es el BTN1, así que se deja sin abrir.
  MIDIOUT.begin(MIDI_BAUD, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);

  for (int b = 0; b < 5; b++) pinMode(BTN_PIN[b], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  razonDesafine = powf(2.0f, DESAFINE_CENTS / 1200.0f);
  mulSoltar = expf(-1.0f / (TAU_SOLTAR * SAMPLE_RATE));
  for (int p = 0; p < NUM_PASOS; p++) ctlNota[p] = notaPaso[p];
  calcularFiltro();

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHT);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_MAX_MA);
  FastLED.clear();
  FastLED.show();

  entrarPanel(PANEL_A);
  random16_set_seed((uint16_t)micros());
  sortearEfecto();
  i2s_init();

#ifndef SIMULADOR
  // El audio tiene el core 1 para él solo; botones, pots y LEDs van al core 0.
  xTaskCreatePinnedToCore(audioTask,   "audio",   8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask, "control", 4096, NULL,  3, NULL, 0);
#endif
}

// ==============================================================================================================================================
// TAREA DE AUDIO — un buffer de 128 muestras (core 1)
// ==============================================================================================================================================
void renderBuffer() {
  // ── Borde del buffer: aplicar los pedidos de la tarea de control ──
  bool quiereTocar = tocando;
  if (quiereTocar && !aTocando) {          // Play: desde el paso 1, en la primera muestra
    pasoAudio = 0;
    muestrasPaso = 0;
    // El reloj MIDI se reancla al MISMO "1": este tick es el paso 1. La tarea de control
    // tira los ticks anteriores y manda Start justo antes de él.
    tickAcc = 0;
    midiStartTick = midiTicks + 1;
    midiTicks++;
    reqMidiStart = true;
  }
  if (!quiereTocar && aTocando) {          // Stop: se cierra la nota
    soltando = true;
    atacando = false;
    pasoSonando = -1;
    reqMidiStop = true;
  }
  aTocando = quiereTocar;

  int oc = octava;
  if (oc != aOctava) {                     // la octava se oye en el acto, sin esperar al paso
    aOctava = oc;
    if (notaActual >= 0)
      frec = FREC_BASE * powf(2.0f, (float)notaASemitono(notaActual, aOctava) / 12.0f);
  }

  // Largo del paso (semicorchea). Se fija al EMPEZAR cada paso: subir la velocidad
  // a mitad de un paso no dispara notas de más.
  uint32_t largoPaso = (uint32_t)(SAMPLE_RATE * 60.0f / (bpmDe(pVel) * 4.0f));
  // El reloj se cuenta con ESTE mismo número: el clock no puede irse del secuenciador.
  uint32_t muestrasPulso = largoPaso * PASOS_POR_PULSO;
  float tau = tauDecayDe(pDecay);
  mulDecay  = expf(-1.0f / (tau * SAMPLE_RATE));
  mulDecayF = expf(-1.0f / (tau * 0.55f * SAMPLE_RATE));   // el filtro cierra antes que el volumen
  float volT = pVol * pVol;                                // curva de volumen más natural
  float corteT = pCorte;
  float fsrT   = pFsr;

  const float incAtaque  = 1.0f / (ATAQUE_S * SAMPLE_RATE);
  const float incAtaqueF = 1.0f / (ATAQUE_FILTRO_S * SAMPLE_RATE);

  int16_t buffer[BUFFER_SAMPLES * 2];

  for (int i = 0; i < BUFFER_SAMPLES; i++) {
    // ── Secuenciador (preciso a la muestra) ──
    if (aTocando) {
      if (muestrasPaso == 0) {
        dispararPaso(pasoAudio);
        pasoAudio = (pasoAudio + 1) % NUM_PASOS;
        muestrasPaso = largoPaso;
      }
      muestrasPaso--;
    }

    // ── Reloj MIDI: 24 ticks por negra, contados con el mismo contador de muestras que
    //    los pasos (acumulador entero ⇒ cero deriva). Corren SIEMPRE, aun parado. ──
    tickAcc += MIDI_PPQN;
    while (tickAcc >= muestrasPulso) { tickAcc -= muestrasPulso; midiTicks++; }

    // ── Parámetros suavizados ──
    volS   += (volT   - volS)   * SUAVIZADO;
    corteS += (corteT - corteS) * SUAVIZADO;
    fsrS   += (fsrT   - fsrS)   * SUAVIZADO;   // el FSR entra al filtro sin escalones

    // ── Envolventes ──
    if (atacando) {
      env += incAtaque;
      if (env >= 1.0f) { env = 1.0f; atacando = false; }
    } else {
      env *= soltando ? mulSoltar : mulDecay;
      if (env < 1.0e-5f) env = 0.0f;
    }
    if (atacandoF) {
      envF += incAtaqueF;
      if (envF >= 1.0f) { envF = 1.0f; atacandoF = false; }
    } else {
      envF *= mulDecayF;
    }

    if ((i & 7) == 0) calcularFiltro();

    // ── Osciladores: 2 sierras + cuadrada una octava abajo ──
    float dt1 = frec * INV_SR;
    float dt2 = dt1 * razonDesafine;
    float dtS = dt1 * 0.5f;
    fase1   += dt1; if (fase1   >= 1.0f) fase1   -= 1.0f;
    fase2   += dt2; if (fase2   >= 1.0f) fase2   -= 1.0f;
    faseSub += dtS; if (faseSub >= 1.0f) faseSub -= 1.0f;

    float mezcla = (sierra(fase1, dt1) + sierra(fase2, dt2)) * NIVEL_SIERRAS
                 + cuadrada(faseSub, dtS) * NIVEL_SUB;

    // ── Saturación → filtro con envolvente → amplitud ──
    float x = softClip(mezcla * DRIVE) * DRIVE_COMP + 1.0e-18f;   // anti-denormal
    float y = filtroLP(svf, x) * compQ * env;

    // Bloqueador de continua
    float dc = y - dcX1 + 0.9995f * dcY1;
    dcX1 = y; dcY1 = dc;

    float out = softClip(dc * volS * MASTER) * 30000.0f;
    if (out >  32000.0f) out =  32000.0f;
    if (out < -32000.0f) out = -32000.0f;
    int16_t s = (int16_t)out;
    buffer[i * 2]     = s;
    buffer[i * 2 + 1] = s;
  }

  // Una sola escritura por buffer: los efectos de la tira reaccionan a la envolvente real
  // del bajo, no a una imitación calculada en el core 0.
  g_env = env;

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);
}

// ==============================================================================================================================================
// Las dos tareas (y el `loop()` de Arduino, que acá no hace nada)
// ==============================================================================================================================================
#ifdef SIMULADOR
void loop() { pasoControl(); renderBuffer(); }
#else
void audioTask(void *)   { for (;;) renderBuffer(); }                     // core 1, prioridad 10
void controlTask(void *) { for (;;) { pasoControl(); vTaskDelay(1); } }   // core 0, 1 kHz
void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }
#endif
