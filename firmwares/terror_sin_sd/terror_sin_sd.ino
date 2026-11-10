// ==============================================================================================================================================
// PERCU-SYNTH — TERROR SIN SD (el mismo tráiler de terror en 4 paneles, con 10 voces guardadas en la flash) — GC Lab Chile
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
// - Microcontrolador ESP32-S3 (las voces van en su flash: no hace falta microSD)
// - DAC PCM5102 vía I2S — estéreo 44.1 kHz · 16-bit |LCK -> 39, DIN -> 40, BCK -> 41|
// - 6 LEDs WS2812 SMD de la placa + tira de 10 WS2812 encadenada después, mismo pin |DATA -> 46| (índices 0..5 y 6..15)
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - Flash Mode         : DIO          (¡OPI rompe I2S!)
// - PSRAM              : OPI PSRAM    (la memoria del glitch va ahí; sin PSRAM, glitches más cortos)
// - Partition Scheme   : Huge APP (3MB No OTA/1MB SPIFFS)   ← OBLIGATORIO: las voces ocupan 2.2 MB de flash
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (incluye driver/i2s_std.h)
// - voces.h (en esta carpeta, ya viene hecho; sin él compila con voces sintetizadas)
// - FastLED (gestor de librerías de Arduino)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Música de tráiler de terror que se toca en vivo. Cada Play sortea un tema nuevo y arranca con una
// INTRO lenta: un latido que se va acelerando y un viento que crece durante 16 s, hasta que entra la
// percusión con un golpe. Debajo siempre suena un dron grave, y la TENSIÓN (POT3 del General) va
// sumando el bajo que pulsa, un tic, un arpegio que sube y los tambores; arriba del todo un braam en
// cada frase y glitches al final de cada una. Encima se agregan CAPAS que crecen de a poco en tono y
// presencia (cuerdas que suben, coro oscuro, tono de tensión, caja de música) y VOCES guardadas en el firmware
// (risas, lamentos, susurros, niños, bebés) con pitch, reverb, eco y reversa.
//
// ARMONÍA: cada tema es una progresión de acordes y cada acorde trae su propia escala (frigio, menor
// armónica, húngara menor, frigio dominante, lidio…). Bajo, arpegio, braam, cuerdas, coro y caja de
// música tocan SÓLO notas de la escala del acorde que suena, y el dron se queda en la tónica (el pedal
// del terror) mientras esa nota pertenezca al acorde; si no, se va a la fundamental del acorde.
//
// ARQUITECTURA: audio en su tarea en el CORE 1. En el CORE 0: controles a 1 kHz y los
// LEDs. Los controles sólo dejan PEDIDOS que el audio aplica al borde de un bloque. Sin acelerómetro:
// sus lecturas interrumpían la música.
// ==============================================================================================================================================
// FUNCIONAMIENTO — 4 PANELES. BTN4 → siguiente panel (General → Bajo → Capas → Voces), en todos.
// ==============================================================================================================================================
//   Al cambiar de panel cada pot queda "suelto" y no cambia nada hasta que lo muevas un poco (sin
//   saltos). El LED 5 dice el panel: rojo General · naranjo Bajo · ámbar Capas · violeta Voces.
//
// GENERAL   BTN1 play/stop: cada Play es un tema nuevo y empieza con la intro (latidos + viento, 16 s)
//           BTN2 tema siguiente, sin parar · BTN3 SUSPENSO: se corta todo, latidos y viento, y vuelve
//           a entrar con un golpe a los 10 s (BTN3 otra vez = entra ya)
//           POT1 volumen · POT2 tempo (60–140) · POT3 TENSIÓN · POT4 filtro general (oscuro ↔ abierto)
// BAJO      BTN1 suena/calla · BTN2 patrón (8 ritmos)
//           POT1 volumen · POT2 filtro · POT3 caída (40 ms – 1.2 s) · POT4 resonancia
// CAPAS     BTN1 CUERDAS que suben · BTN2 CORO oscuro · BTN3 TONO de tensión · BTN5 CAJA DE MÚSICA
//           Un toque la hace entrar y crecer de a poco (tono y presencia); otro toque la apaga despacio
//           POT1 volumen cuerdas · POT2 volumen coro · POT3 volumen tono · POT4 volumen caja
// VOCES     BTN1 dispara una voz de la categoría (nunca la misma dos veces seguidas) · BTN2 categoría
//           (RISAS · LAMENTOS · SUSURROS · NIÑOS · BEBÉS) · BTN3 reversa sí/no
//           POT1 volumen · POT2 pitch (2 octavas abajo … 1 arriba, centro = como está) · POT3 reverb · POT4 eco
//
// - LEDs → todo en rojo: la tira late con los tambores, un punto sube con el arpegio, en la intro y el
//          suspenso la brasa crece con el viento, el golpe de entrada es un estrobo. LEDs de la placa:
//          0 tambores · 1 bajo · 2 arpegio · 3 capas · 4 voces · 5 panel (magenta = el audio se quedó sin tiempo)
//
// SIN SD: la copia de terror que no necesita tarjeta. Trae 2 voces por categoría (10 en total, 25 s,
// mono 44.1 kHz) dentro del firmware, en la flash, y las lee de ahí directamente. Para cambiarlas:
// generar_voces_h.py arma voces.h con hasta 4 por categoría (lo que quepa en la partición Huge APP).
//
// MODO DE USO:
// 1. Aprieta BTN1: escucha la intro (latidos y viento) hasta que entra la percusión.
// 2. Sube el POT3 de a poco: van entrando el bajo, el tic, el arpegio y los tambores.
// 3. BTN4 hasta el panel Capas (LED 5 ámbar): BTN1, BTN2, BTN3 y BTN5 agregan capas que crecen solas.
// 4. BTN4 hasta Voces (violeta): BTN2 elige la categoría, BTN1 dispara, POT2 pitch, POT3 reverb.
// 5. En el General, BTN3 corta a un suspenso que vuelve a entrar con un golpe.
// ==============================================================================================================================================

// El core de Arduino compila con -Os (tamaño). Este sketch es casi todo bucles de audio: con O2
// (velocidad) el mismo código usa bastante menos CPU por bloque (medido en industrial_matriz).
#pragma GCC optimize ("O2")
#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

// ─── Tipos (arriba del todo para que el IDE de Arduino genere bien los prototipos) ───
struct Filtro { float ic1, ic2, a1, a2, a3, k; };              // SVF de Zavalishin (TPT)
struct BQ     { float b0, b1, b2, a1, a2, z1, z2; };           // biquad RBJ (DF2 transpuesta)
// Un golpe de percusión: TODO lo que define su sonido (el motor de industrial_matriz_v2) + su envío a la reverb
struct Golpe {
  float amp;
  float f, r2, a0, a1;               // capa tonal: frecuencia, razón de la 2.ª parcial (0 = sin), niveles (a0 = 0: sin capa tonal)
  float eDec, cuanto, caidaDec;      // caída del tono (tau), cuánto cae el pitch y en qué tau
  float nAmt, nDec, nAcc;            // ruido: nivel, tau normal y tau con acento
  float fc0, fc1, barre, q;          // pasa-banda del ruido: de fc0 a fc1 en `barre` s
  float hp;                          // pasa-altos de carril (0 = sin)
  float pan, panRnd;
  float rev;                         // envío a la reverb
};
struct VozInd {
  bool  viva, conBanda;
  float amp, gL, gR, rev;
  float ph[2], inc[2], a[2];
  float env, eC, caida, caidaC, cuanto;
  float nz, nC, nAmt;
  float fc0, fc1, barre, barreInc, q;
  Filtro s; BQ banda;
};
struct VozPluck { bool viva, atk; int nota; float f1, inc1, inv1, amp, fenv, base, oct, q, eC, gL, gR; Filtro s; };
struct VozMuestra { bool viva, reversa, sintetica; int cat, s; int32_t pos; float frac, amp, obj, k, lp, t, dur, rev, eco; Filtro f1, f2; uint32_t sem; };
struct VozSost { float ph, inc, inv, amp, obj; int nota, notaObj; };   // una voz sostenida que cambia de nota callándose y volviendo a entrar
struct Acorde { int8_t semi; uint8_t modo; uint8_t compases; };      // semitonos sobre la tónica · escala del acorde · cuánto dura
struct Tema {
  const char *nombre;
  uint8_t     pc;                    // tónica: 0 = Do … 11 = Si
  Acorde      prog[4];
  const char *arp[2];                // arpegio: calmo · intenso
  const char *bat[2][5];             // tambores: TAIKO · TOM · CAJA · METAL · TIC, calmo e intenso
  uint8_t     caja;                  // qué melodía de la caja de música trae
};
// voces.h viaja junto al .ino (Arduino lo abre como pestaña). Si alguien recibe solo el .ino, compila
// igual: cada voz pasa a ser el susurro sintetizado, y entonces tampoco hace falta la partición Huge APP.
#if __has_include("voces.h")
  #include "voces.h"                             // las voces, en la flash (generar_voces_h.py)
#else
  #warning "Falta voces.h en la carpeta del .ino: compila igual, pero las voces seran susurros sintetizados."
  #define VOCES_FLASH_MAX 4
  const int16_t *const VOZ_DATOS[5][VOCES_FLASH_MAX] = {};
  const uint32_t VOZ_LARGO[5][VOCES_FLASH_MAX] = {};
  const int VOZ_CUANTAS[5] = { 0, 0, 0, 0, 0 };
#endif
struct Ctl { bool tocar; int tema; bool bajoOn; int patBajo; bool capa[4]; int cat; bool reversa; };

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
const float SR = (float)SAMPLE_RATE;
const float PI_F = 3.14159265f;

// ─── LEDs: 6 SMD de la placa + tira de 10 en el mismo pin ──
#define LED_PIN      46
#define LEDS_PLACA    6
#define LEDS_TIRA    10
#define NUM_LEDS     (LEDS_PLACA + LEDS_TIRA)
#define LED_BRILLO  180
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
// Con todo en rojo, 16 LEDs al máximo piden ~0.3 A. Un escalón de corriente grande colgado del 5 V
// de la placa hunde el riel y corta el audio (lección de bajo_8_pasos_fsr_clock): por eso el estrobo
// es rojo con apenas un tinte blanco, y FastLED tiene este tope duro.
#define LED_MAX_MA  400
const unsigned long LED_REFRESH_MS = 20;
CRGB leds[NUM_LEDS];

// ─── Voces ─────────────────────────────────────────────────
#define NUM_CAT        5
const char *NOMBRE_CAT[NUM_CAT]  = { "RISAS", "LAMENTOS", "SUSURROS", "NIÑOS", "BEBÉS" };

// ─── Botones y pots ────────────────────────────────────────
const uint8_t BTN_PIN[5] = { 44, 42, 0, 45, 47 };   // BTN1..BTN5 (orden correcto de la placa)
const uint8_t POT_PIN[4] = { 1, 2, 8, 10 };
const unsigned long DEBOUNCE_MS = 25;

#if ARDUINO_USB_CDC_ON_BOOT
  #define LOG(...)   do { Serial.printf(__VA_ARGS__); Serial0.printf(__VA_ARGS__); } while (0)
#else
  #define LOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#endif

// ==============================================================================================================================================
// SONIDO — lo fijo se ajusta acá
// ==============================================================================================================================================
// MEDIR 1 = diagnóstico: arranca tocando y cada 2 s imprime por Serial cuánto CPU usa el audio.
#ifndef MEDIR
#define MEDIR 0
#endif

const float BPM_MIN = 60.0f, BPM_RANGO = 80.0f;   // POT2 del General: 60 … 140
const float MASTER  = 0.70f;                      // ganancia general con el POT1 al máximo
const float TECHO   = 0.89f;                      // −1 dBFS: techo del limitador
#define LIM_LOOK 64

// LA TENSIÓN (POT3 del General): cada capa entra entre dos puntos del recorrido, con un fundido entre ellos
const float T_PULSO0 = 0.08f, T_PULSO1 = 0.22f;   // el bajo que pulsa
const float T_TIC0   = 0.18f, T_TIC1   = 0.48f;   // el reloj (densidad: primero las X, después las x)
const float T_ARP0   = 0.30f, T_ARP1   = 0.45f;   // el arpegio
const float T_BAT0   = 0.42f, T_BAT1   = 0.95f;   // los tambores (densidad)
const float T_INTENSO = 0.70f;                    // desde acá, los patrones intensos (±0.04 de histéresis)
const float T_SUBE    = 0.60f;                    // desde acá el arpegio sube un escalón cada 2 compases
const float T_BRAAM   = 0.85f;                    // desde acá, un braam en el "1" de cada frase de 4 compases
const float T_GLITCH  = 0.80f;                    // desde acá, un glitch en el último pulso de la frase (la mitad de las veces)

// Niveles de cada capa
const float NIVEL_DRON   = 0.15f;
const float NIVEL_PULSO  = 0.50f;
const float NIVEL_ARP    = 0.11f;
const float NIVEL_PERC   = 1.10f;
const float NIVEL_BRAAM  = 0.30f;
const float NIVEL_SUB    = 0.55f;
const float NIVEL_LATIDO = 0.30f;
const float NIVEL_VIENTO = 0.20f;
const float NIVEL_VOZ    = 1.30f;
const float NIVEL_CUERDAS = 0.40f, NIVEL_CORO = 0.95f, NIVEL_TONO = 0.20f, NIVEL_CAJA = 0.30f;
const float SC_DRON = 0.35f, SC_PULSO = 0.30f;    // cuánto agacha el tambor grande al dron y al pulso

// INTRO (cada Play) y SUSPENSO (BTN3 del General): la música calla, quedan el dron que crece, un latido
// que se acelera y un viento que sube; al final, el golpe de entrada y la música desde el "1".
const float INTRO_S     = 16.0f;
const float SUSPENSO_S  = 10.0f;
const float LAT_INI_S   = 1.35f, LAT_FIN_S = 0.62f;   // un latido cada 1.35 s al principio (44 por minuto) … cada 0.62 s al final (97)
const float SUS_DRON0   = 0.12f, SUS_DRON1 = 0.45f;   // el dron va de −18 dB a −7 dB durante la espera
const float SUS_ENTRA_S = 0.010f;                     // el corte al entrar al suspenso: 10 ms, de golpe pero sin clic

// BAJO (panel Bajo): dos sierras + cuadrada una octava abajo → pasa-bajos con envolvente
const float PULSO_CORTE_MIN = 50.0f, PULSO_CORTE_OCT = 4.6f;  // POT2: 50 Hz … 1.2 kHz
const float PULSO_DEC_MIN   = 0.040f, PULSO_DEC_RANGO = 30.0f; // POT3: 40 ms … 1.2 s
const float PULSO_Q_MIN     = 0.7f, PULSO_Q_RANGO = 4.3f;     // POT4: resonancia (se achica sola con el filtro abierto)
#define NUM_PAT_BAJO 8
// 16 caracteres relativos a la fundamental del acorde: x fundamental · o octava · 5 quinta ·
// b el 2.º grado de su escala (en frigio, el semitono) · - nada. Mayúscula = acento.
const char *PAT_BAJO[NUM_PAT_BAJO] = {
  "X-x-x-x-X-x-x-x-",   // corcheas: el pulso de tráiler
  "X--xX--xX--xX-x-",   // 3-3-2
  "XxxxXxxxXxxxXxxx",   // semicorcheas
  "X-xxX-xxX-xxX-xo",   // galope
  "X--x--x-X--x--x-",   // tresillo
  "X-------X-x-----",   // lento
  "XxbxXxbxXxbxXxbx",   // con el semitono
  "X-x-X-ox-xX-x-5o",   // saltos de octava y quinta
};

// BRAAM: tres notas (fundamental, octava, quinta arriba) con dos sierras cada una → saturación suave →
// pasa-bajos que se abre de golpe y se cierra lento.
const float BRAAM_DEC = 1.40f, BRAAM_FENV = 0.45f, BRAAM_CORTE = 130.0f, BRAAM_OCT = 4.3f, BRAAM_DRIVE = 1.6f;

// CAPAS (panel Capas): cada una entra en CAPA_CRECE_S y se apaga en CAPA_APAGA_S
const float CAPA_CRECE_S = 24.0f, CAPA_APAGA_S = 4.0f;
const float CUERDAS_PASO_S = 3.5f;      // las cuerdas suben una nota del acorde cada tanto, hasta 7 escalones
const float TONO_OCTAVA_S  = 14.0f;     // el tono de tensión sube una octava en este tiempo, sin terminar nunca (Shepard)
const float CAJA_DEFORMA_S = 40.0f;     // en este tiempo la caja de música llega a su peor desafinación

// REVERB (un solo bus) y ECO (arpegio y voces)
const float REV_FB = 0.90f, REV_RETORNO = 1.6f;
const float ECO_FEEDBACK = 0.45f, ECO_MEZCLA = 0.45f, ECO_ENVIO_ARP = 0.8f;

// VOCES: el pitch del POT2 del panel Voces
const float VOZ_SEMI_ABAJO = 24.0f, VOZ_SEMI_ARRIBA = 12.0f, VOZ_CENTRO = 0.05f;   // ±5 % al centro = sin cambio
const float SUSURRO_S = 1.5f;                     // la voz sintetizada de una categoría vacía

// GLITCH
const float GLITCH_MIN_S = 0.35f;                 // nunca dos glitches más juntos que esto

// ══ ESCALAS ══ Cada acorde de un tema trae la suya. La tríada (grados 0, 2, 4) siempre es consonante.
#define M_EOLICO     0
#define M_FRIGIO     1
#define M_ARMONICA   2
#define M_FRIGIO_DOM 3
#define M_HUNGARA    4
#define M_LIDIO      5
#define M_DORICO     6
#define M_HUNG_6     7   // la húngara menor vista desde su VI
#define M_HUNG_5     8   // la húngara menor vista desde su V
#define NUM_MODOS    9
const int MODOS[NUM_MODOS][7] = {
  { 0, 2, 3, 5, 7, 8, 10 },   // eólico (menor natural)
  { 0, 1, 3, 5, 7, 8, 10 },   // frigio: la 2.ª menor, el semitono del terror
  { 0, 2, 3, 5, 7, 8, 11 },   // menor armónica
  { 0, 1, 4, 5, 7, 8, 10 },   // frigio dominante
  { 0, 2, 3, 6, 7, 8, 11 },   // húngara menor: cuarta aumentada y séptima mayor
  { 0, 2, 4, 6, 7, 9, 11 },   // lidio
  { 0, 2, 3, 5, 7, 9, 10 },   // dórico
  { 0, 3, 4, 6, 7, 10, 11 },  // húngara desde el VI
  { 0, 1, 4, 5, 7, 8, 11 },   // húngara desde el V
};

// ══ TEMAS ════════════════════════════════════════════════════════════════════════════════════════════
// ARPEGIO — 16 caracteres: 0..7 = nota del acorde (0 fund · 1 3.ª · 2 5.ª · 3 8.ª · 4 10.ª · 5 12.ª ·
//   6 15.ª · 7 17.ª), b = el 2.º grado (la nota de paso), - nada. Todos suben: es la escalera.
// TAMBORES — 5 líneas de 16 (un compás): TAIKO · TOM · CAJA · METAL · TIC. X fuerte · x normal ·
//   . fantasma · - nada. La letra es también la PRIORIDAD: con poca tensión suenan sólo las X.
#define NUM_TEMAS 5
const Tema TEMAS[NUM_TEMAS] = {
  // ── CRIPTA ── Re frigio: Rem·4 · Mi♭·2 · Rem·2. El semitono de arriba que vuelve a caer.
  { "CRIPTA", 2, { { 0, M_FRIGIO, 4 }, { 1, M_LIDIO, 2 }, { 0, M_FRIGIO, 2 }, { 0, 0, 0 } },
    { "0-1-2-3-0-1-2-3-", "0123012301230123" },
    { { "X-------x-------", "------------x-x-", "--------X-------", "X---------------", "x-x-x-x-x-x-x-x-" },
      { "X--x--x-X--x-xX-", "--x---x---x-xxXX", "----X-------X-.X", "X-------x-------", "xxXxxxXxxxXxxxXx" } }, 0 },
  // ── RITUAL ── Mi menor armónica: Mim · Do · Lam · Si (la dominante mayor).
  { "RITUAL", 4, { { 0, M_ARMONICA, 2 }, { 8, M_LIDIO, 2 }, { 5, M_DORICO, 2 }, { 7, M_FRIGIO_DOM, 2 } },
    { "012-012-012-0123", "0123123423453456" },
    { { "X-----x-X-------", "-----------x--x-", "--------X-------", "X---------------", "x---x---x---x---" },
      { "X-xX--x-X-xX--x-", "---x---x---x-xXX", "----X--.----X--X", "X---------------", "x-x-X-x-x-x-X-x-" } }, 1 },
  // ── ABISMO ── Do# eólico con mediantes cromáticas: Do#m · Lam · Do#m · Solm (a un tritono).
  { "ABISMO", 1, { { 0, M_EOLICO, 2 }, { 8, M_EOLICO, 2 }, { 0, M_EOLICO, 2 }, { 6, M_FRIGIO, 2 } },
    { "0b120b120b120b12", "0b12b12312342345" },
    { { "X---------------", "--------x-----x-", "--------X-------", "X---------------", "x-x-x-x-x-x-x-x-" },
      { "X--X--X---X--X--", "--x--x----x-xxxX", "----X-------X---", "X-------X-------", "xXxxxXxxxXxxxXxx" } }, 2 },
  // ── MANICOMIO ── Fa húngara menor: Fam·4 · Re♭·2 · Do·2 (todo dentro de la misma escala).
  { "MANICOMIO", 5, { { 0, M_HUNGARA, 4 }, { 8, M_HUNG_6, 2 }, { 7, M_HUNG_5, 2 }, { 0, 0, 0 } },
    { "0124012401240124", "0123401234012345" },
    { { "X-------X-------", "------x-------x-", "--------X-------", "----X-----------", "x-x-x-x-x-x-x-x-" },
      { "X-xxX-xxX-xxX-xX", "-x---x---x---xXX", "----X-------X-XX", "X-------x---x---", "XxxxXxxxXxxxXxxx" } }, 3 },
  // ── ESPIRAL ── Mi frigio que se corre medio tono: Mim · Fam · Mim · Si♭m (a un tritono).
  { "ESPIRAL", 4, { { 0, M_FRIGIO, 2 }, { 1, M_EOLICO, 2 }, { 0, M_FRIGIO, 2 }, { 6, M_FRIGIO, 2 } },
    { "0-1-2-3-4-5-6-7-", "0123456701234567" },
    { { "X--x--x---------", "----------x--x--", "--------X-------", "X---------------", "x--x--x-x--x--x-" },
      { "X--x--x-X--x--x-", "--x--x--xxx-xxXX", "----X-------X---", "X---------x-----", "xxXxxXxxXxxXxxXx" } }, 0 },
};
const int8_t TONOS_ARP[11] = { 0, 2, 4, 7, 9, 11, 14, 16, 18, 21, 23 };   // notas del acorde, en grados de su escala

// CAJA DE MÚSICA: canciones de cuna escritas a mano, 32 pasos (2 compases). 1..9 = grado de la escala
// del acorde contado desde su fundamental (1 fund · 3 tercera · 5 quinta · 8 octava), - silencio.
#define NUM_CAJA 4
const char *MEL_CAJA[NUM_CAJA] = {
  "5---3---1---3---5-6-5-3-2---1---",   // la canción de cuna que cae
  "1---5---8---7---6---5---4---3---",   // el descenso lento desde la octava
  "3-2-1---3-2-1---5-4-3---2-------",   // la pregunta que no se responde
  "8---5---6---5---3---2---1-------",   // la que se apaga
};

// Registros (MIDI): cada voz vive en su octava, plegada desde la fundamental del acorde
const int DRON_DESDE    = 28;   // Mi1 … Re#2
const int PULSO_DESDE   = 33;   // La1 … Sol#2
const int ARP_DESDE     = 52;   // Mi3 … Re#4
const int CUERDAS_DESDE = 40;   // Mi2 … y suben
const int CORO_DESDE    = 50;   // Re3 … Do#4
const int CAJA_DESDE    = 74;   // Re5 … Do#6

// ══ TAMBORES ══ Las reglas de drum_poder e industrial_matriz: arriba de 1 kHz todo es RUIDO filtrado
// (pasa-banda), el metal es ruido y nunca parciales senoidales afinados (sonaban a olla), las dos
// parciales de un parche NO se saturan, cada tambor es UN parche con fast-kill de 3 ms.
#define NUM_PERC  5
#define I_TAIKO   0
#define I_TOM     1
#define I_CAJA    2
#define I_METAL   3
#define I_TIC     4
//   amp     f       r2     a0     a1    eDec   cuanto caidaD  nAmt  nDec   nAcc   fc0      fc1      barre  q     hp      pan   panRnd rev
const Golpe GOLPES[NUM_PERC] = {
  { 0.95f, 62.0f,  1.59f, 0.90f, 0.22f, 0.38f, 0.60f, 0.025f, 0.55f, 0.012f, 0.016f, 900.0f,  900.0f,  0,     0.8f,  30.0f,  0.50f, 0.08f, 0.35f },   // taiko: el tambor grande del tráiler
  { 0.60f, 118.0f, 1.50f, 0.80f, 0.20f, 0.22f, 0.35f, 0.030f, 0.45f, 0.010f, 0.012f, 1200.0f, 1200.0f, 0,     0.8f,  60.0f,  0.50f, 0.25f, 0.30f },   // tom
  { 0.60f, 185.0f, 1.56f, 0.40f, 0.25f, 0.080f, 0.12f, 0.020f, 1.20f, 0.140f, 0.200f, 3200.0f, 1600.0f, 0.10f, 0.9f,  150.0f, 0.50f, 0.05f, 0.45f },   // caja de tráiler
  { 0.42f, 0.0f,   0,     0,     0,    0.01f, 0,     0.01f,  1.30f, 0.300f, 0.450f, 4200.0f, 2600.0f, 0.20f, 2.2f,  0,      0.50f, 0.20f, 0.55f },   // golpe metálico (sólo ruido)
  { 0.26f, 0.0f,   0,     0,     0,    0.01f, 0,     0.01f,  0.90f, 0.010f, 0.018f, 6500.0f, 6500.0f, 0,     1.4f,  0,      0.50f, 0.30f, 0.05f },   // tic del reloj
};

// Eventos (para los LEDs)
#define EV_PULSO    5
#define EV_ARP      6
#define EV_BRAAM    7
#define EV_SUSTO    8
#define EV_GLITCH   9
#define EV_VOZ     10
#define EV_LATIDO  11
#define EV_CAJA    12
#define NUM_EV     13

// Paneles
#define P_GENERAL   0
#define P_BAJO      1
#define P_CAPAS     2
#define P_VOCES     3
#define NUM_PANELES 4
const char *NOMBRE_PANEL[NUM_PANELES] = { "GENERAL", "BAJO", "CAPAS", "VOCES" };
#define C_CUERDAS 0
#define C_CORO    1
#define C_TONO    2
#define C_CAJA    3
const char *NOMBRE_CAPA[4] = { "CUERDAS", "CORO", "TONO", "CAJA DE MUSICA" };

// ==============================================================================================================================================
// PEDIDOS ENTRE NÚCLEOS (cada variable tiene UN solo escritor)
// ==============================================================================================================================================
// control → audio
volatile Ctl      reqC;
volatile uint32_t reqSuspenso = 0, reqVoz = 0;
// Pots de cada panel, ya suavizados:
//   [P_GENERAL] vol · tempo · tensión · filtro      [P_BAJO]  vol · filtro · caída · resonancia
//   [P_CAPAS]   vol cuerdas · coro · tono · caja    [P_VOCES] vol · pitch · reverb · eco
volatile float    pPot[NUM_PANELES][4] = {
  { 0.70f, 0.50f, 0.00f, 1.00f },
  { 0.85f, 0.45f, 0.30f, 0.30f },
  { 0.75f, 0.75f, 0.70f, 0.75f },
  { 0.80f, 0.50f, 0.40f, 0.00f },
};
// audio → LEDs y Serial
volatile uint32_t golpes[NUM_EV];
volatile float    velGolpe[NUM_EV];
volatile uint32_t pasoTotalPub = 0, contadorPasos = 0;
volatile float    envPulsoPub = 0.0f, tensionPub = 0.0f, arpAlturaPub = 0.0f, vozNivelPub = 0.0f, bpmPub = 100.0f, esperaPub = 0.0f, capasPub = 0.0f;
volatile bool     tocandoPub = false, suspensoPub = false, glitchPub = false, vozRevPub = false;
volatile int      temaPub = 0, panelPub = 0;
volatile uint32_t panelCambioMs = 0;
volatile uint32_t cargaAltaHasta = 0;
volatile uint32_t medMax = 0, medSuma = 0, medN = 0, medTecho = 0;
volatile float    medLimMin = 1.0f, medPico = 0.0f;

// Cuántas voces hay de cada categoría: las trae voces.h
int     numMuestras[NUM_CAT] = { VOZ_CUANTAS[0], VOZ_CUANTAS[1], VOZ_CUANTAS[2], VOZ_CUANTAS[3], VOZ_CUANTAS[4] };

// ==============================================================================================================================================
// DSP — utilidades (sin libm por muestra)
// ==============================================================================================================================================
#define TABLA 1024
float SENO[TABLA + 1];
static inline float seno(float fase) { float x = fase * TABLA; int i = (int)x; float f = x - i; return SENO[i] + (SENO[i + 1] - SENO[i]) * f; }
static inline float ruido(uint32_t &s) { s = s * 1664525u + 1013904223u; return (int32_t)s * (1.0f / 2147483648.0f); }
static inline float tanRapido(float x) { float x2 = x * x; return x * (15.0f - x2) / (15.0f - 6.0f * x2); }   // Padé, < 0.2 % hasta 13 kHz
static inline float exp2Rapido(float x) {          // 2^x para x ≥ 0, error < 0.2 %
  int e = (int)x; float f = x - e;
  float p = 1.0f + f * (0.6951786f + f * (0.2261487f + f * 0.0782451f));
  union { float f; int32_t i; } u; u.f = p; u.i += e << 23; return u.f;
}
static inline float decaimiento(float tau) { return expf(-1.0f / (tau * SR)); }
static inline float softClip(float x) {
  if (x > 3.0f) return 1.0f; if (x < -3.0f) return -1.0f;
  float x2 = x * x; return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
static inline float sierra(float &f, float inc, float inv) {   // PolyBLEP; inv = 1/inc
  float y = 2.0f * f - 1.0f;
  if (f < inc) { float t = f * inv; y -= t + t - t * t - 1.0f; }
  else if (f > 1.0f - inc) { float t = (f - 1.0f) * inv; y -= t * t + t + t + 1.0f; }
  f += inc; if (f >= 1.0f) f -= 1.0f;
  return y;
}
static inline float cuadrada(float &f, float inc, float inv) {   // PolyBLEP: sierra − sierra desplazada media vuelta
  float p2 = f + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
  float a = 2.0f * f - 1.0f, b = 2.0f * p2 - 1.0f;
  if (f < inc) { float t = f * inv; a -= t + t - t * t - 1.0f; } else if (f > 1.0f - inc) { float t = (f - 1.0f) * inv; a -= t * t + t + t + 1.0f; }
  if (p2 < inc) { float t = p2 * inv; b -= t + t - t * t - 1.0f; } else if (p2 > 1.0f - inc) { float t = (p2 - 1.0f) * inv; b -= t * t + t + t + 1.0f; }
  f += inc; if (f >= 1.0f) f -= 1.0f;
  return (a - b) * 0.5f;
}
// Los filtros llevan su propio piso de ruido: alimentados con silencio, sus estados caen a SUBNORMALES
// y cada multiplicación cuesta cien veces más (lección de triphop_viaje). 1e-18 está a −360 dB.
const float DITHER = 1e-18f;
static inline void coefFiltro(Filtro &s, float fc, float q) {
  if (fc > 12500.0f) fc = 12500.0f; if (fc < 20.0f) fc = 20.0f;
  float g = tanRapido(PI_F * fc / SR); s.k = 1.0f / q;
  s.a1 = 1.0f / (1.0f + g * (g + s.k)); s.a2 = g * s.a1; s.a3 = g * s.a2;
}
static inline void copiarCoef(Filtro &d, const Filtro &s) { d.a1 = s.a1; d.a2 = s.a2; d.a3 = s.a3; d.k = s.k; }
static inline void pasoFiltro(Filtro &s, float x, float &lp, float &bp) {
  x += DITHER;
  float v3 = x - s.ic2, v1 = s.a1 * s.ic1 + s.a2 * v3, v2 = s.ic2 + s.a2 * s.ic1 + s.a3 * v3;
  s.ic1 = 2.0f * v1 - s.ic1; s.ic2 = 2.0f * v2 - s.ic2;
  lp = v2; bp = v1 * s.k;                          // pasa-banda con ganancia 1 en el centro
}
static inline float pasoBQ(BQ &q, float x) { x += DITHER; float y = q.b0 * x + q.z1; q.z1 = q.b1 * x - q.a1 * y + q.z2; q.z2 = q.b2 * x - q.a2 * y; return y; }
void bqHPF(BQ &c, float fc, float Q) {
  float w = 2.0f * PI_F * fc / SR, s = sinf(w), co = cosf(w), al = s / (2.0f * Q), a0 = 1.0f + al;
  c.b0 = (1.0f + co) * 0.5f / a0; c.b1 = -(1.0f + co) / a0; c.b2 = c.b0; c.a1 = -2.0f * co / a0; c.a2 = (1.0f - al) / a0; c.z1 = c.z2 = 0.0f;
}
void bqLPF(BQ &c, float fc, float Q) {
  float w = 2.0f * PI_F * fc / SR, s = sinf(w), co = cosf(w), al = s / (2.0f * Q), a0 = 1.0f + al;
  c.b0 = (1.0f - co) * 0.5f / a0; c.b1 = (1.0f - co) / a0; c.b2 = c.b0; c.a1 = -2.0f * co / a0; c.a2 = (1.0f - al) / a0; c.z1 = c.z2 = 0.0f;
}
static inline float techoSuave(float x) {
  float a = fabsf(x); if (a <= 0.95f) return x;
  float y = 0.95f + 0.05f * tanhf((a - 0.95f) / 0.05f); return x < 0 ? -y : y;
}
static inline float hz(float midi) { return 440.0f * exp2f((midi - 69.0f) / 12.0f); }
static inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static inline float rampa(float x, float a, float b) { return clamp01((x - a) / (b - a)); }
static inline float ganVol(float v) { return 1.3f * v * v; }   // pot de volumen → ganancia: cuadrática, como un fader
// Corte de un pasa-bajos de bus: 150 Hz … 19 kHz; arriba del 92 % se abre del todo
static inline float corteBus(float p) { return 150.0f * exp2Rapido(7.0f * clamp01(p)); }
static inline float abiertoBus(float p) { return clamp01((p - 0.90f) / 0.08f); }

// Generador de composición — separado del ruido de audio
uint32_t rngComp = 0x9E3779B9u;
static inline float frnd() { rngComp ^= rngComp << 13; rngComp ^= rngComp >> 17; rngComp ^= rngComp << 5; return (rngComp >> 8) * (1.0f / 16777216.0f); }

// ==============================================================================================================================================
// ESTADO DE LA SÍNTESIS (lo toca sólo la tarea de audio)
// ==============================================================================================================================================
Ctl C;
int tema = 0, acordeIdx = -1, raizAcorde = 0, modoAcorde = 0;   // raizAcorde: semitonos sobre la tónica
uint32_t semillaRuido = 22222u, semVientoL = 12345u, semVientoR = 67891u;
float dRapido3, dRapido, dSc;

// Secuenciador
bool     aTocando = false;
float    sps = 0.0f, faltan = 0.0f, bpm = 100.0f;
uint32_t pasoTotal = 0xFFFFFFFFu, muestraGlobal = 0;
bool     saltarBateria = false;                    // el paso que sigue al golpe de entrada: su tambor no se pisa
bool     intenso = false;
float    tensS = 0.0f, densTic = 0.0f, densBat = 0.0f;
float    prio[NUM_PERC][16]; int temaPrio = -1, intPrio = -1;

// Espera (intro y suspenso): latido que se acelera, viento que sube y el golpe de entrada al final
bool     espera = false;
uint32_t espT = 0, espLargo = 1;
float    latFalta = 0.0f; int latFase = 0;

// Tambores
VozInd ind[NUM_PERC][2]; int sigInd[NUM_PERC];
float  scObj = 0.0f, sc = 0.0f;

// Bajo
struct { float f1, f2, fs, env, envF, inc, acento, eC, eF; bool atk, atkF; Filtro s; } pul;
float pulsoCorteS = 0.45f, pulsoQS = 0.3f;
// Arpegio
#define VOCES_ARP 3
VozPluck arps[VOCES_ARP]; int ladoArp = 0;
float dArpFenv;
// Dron
struct { float ph[6], inc[6], inv[6], sub, subInc, lfo, amp, obj; int nota, notaObj; Filtro sL, sR; } dron;
// Braam y sub
struct { bool vivo, atk; float ph[6], inc[6], inv[6], amp, eC, fenv, vel; Filtro sL, sR; } br;
struct { bool vivo; float ph, f, fObj, k, env, eC; } sub;
float dBrFenv;
// Latido y viento
struct { bool vivo; float ph, f, env, eC; } lat;
struct { Filtro aL, aR, bL, bR; float racha, rachaObj, rachaFalta; } vto;
// Capas
float    pres[4] = { 0, 0, 0, 0 };                // presencia de cada capa, 0 … 1
uint32_t tOn[4] = { 0, 0, 0, 0 };                 // muestras desde que se prendió
#define VOCES_CUERDA 4
VozSost  cue[VOCES_CUERDA]; Filtro cueL, cueR; float cueTrem = 0.0f;
VozSost  cor[3]; Filtro corF[3]; float corVib = 0.0f;
float    tonFase = 0.0f, tonPh[7];
struct { float ph[2], inc[2], env[2], lfo; int sig; } cajaM;
// Voces
#define VOCES_VOZ 2                                // la que sonaba se apaga en 4 ms al disparar otra
VozMuestra voces[VOCES_VOZ]; int sigVoz = 0, ultimaVoz[NUM_CAT] = { -1, -1, -1, -1, -1 };
float pasoVoz = 1.0f;

// Reverb (Freeverb chico: 4 peines + 2 pasa-todo por lado)
#define RV_N1 1116
#define RV_N2 1188
#define RV_N3 1277
#define RV_N4 1356
#define RV_N5 1422
#define RV_N6 1491
#define RV_N7 1557
#define RV_N8 1617
#define RV_A1 556
#define RV_A2 441
#define RV_A3 579
#define RV_A4 467
float rvC1[RV_N1], rvC2[RV_N2], rvC3[RV_N3], rvC4[RV_N4], rvC5[RV_N5], rvC6[RV_N6], rvC7[RV_N7], rvC8[RV_N8];
float rvA1[RV_A1], rvA2[RV_A2], rvA3[RV_A3], rvA4[RV_A4];
int   rvI[8], rvJ[4]; float rvLp[8];
// Eco ping-pong (corchea con puntillo, cruza de cabezal al cambiar el tempo)
#define ECO_MAX 32768
int16_t ecoL[ECO_MAX], ecoR[ECO_MAX];
int     ecoIdx = 0, ecoN = 15000, ecoN2 = 15000; float ecoXf = 1.0f, ecoLpL = 0, ecoLpR = 0, ecoHpL = 0, ecoHpR = 0;

// Glitch: memoria de la mezcla (en PSRAM si hay)
#define GL_NADA       0
#define GL_TARTAMUDEO 1
#define GL_REVERSA    2
#define GL_FRENO      3
#define GL_CORTE      4
int16_t *histL = nullptr, *histR = nullptr; uint32_t histN = 0, histW = 0;
struct { int tipo; bool congelado; float t, dur, ini, pos, vel, grano, mezcla, p; } gl;
uint32_t ultimoGlitch = 0;

// Buses y master
float bufSc[BUFFER_SAMPLES], bufMusL[BUFFER_SAMPLES], bufMusR[BUFFER_SAMPLES], bufDronL[BUFFER_SAMPLES], bufDronR[BUFFER_SAMPLES];
float bufHitL[BUFFER_SAMPLES], bufHitR[BUFFER_SAMPLES], bufVozL[BUFFER_SAMPLES], bufVozR[BUFFER_SAMPLES];
float bufCapL[BUFFER_SAMPLES], bufCapR[BUFFER_SAMPLES];
float bufRev[BUFFER_SAMPLES], bufEco[BUFFER_SAMPLES], bufLat[BUFFER_SAMPLES], bufPul[BUFFER_SAMPLES];
float gMusS = 0.0f, gDronS = 0.0f, volS = 0.5f, gPulsoS = 0.0f, gArpS = 0.0f, gBajoS = 1.0f, gVozS = 1.0f, lpfS = 1.0f;
float gCapS[4] = { 1, 1, 1, 1 };
Filtro fMasL, fMasR;
float dcX1L = 0, dcY1L = 0, dcX1R = 0, dcY1R = 0;
float limDlyL[LIM_LOOK], limDlyR[LIM_LOOK]; int limDlyIdx = 0;
float limEnv = 0.0f, limGain = 1.0f, limRel = 0.0f;
BQ techoL, techoR;

void setBPM(float b) {
  if (b < BPM_MIN) b = BPM_MIN; if (b > BPM_MIN + BPM_RANGO) b = BPM_MIN + BPM_RANGO;
  bpm = b; sps = SR * 60.0f / bpm / 4.0f; bpmPub = b;
}

void prepararSintesis() {
  for (int i = 0; i <= TABLA; i++) SENO[i] = sinf(2.0f * PI_F * i / TABLA);
  dRapido3 = decaimiento(0.003f); dRapido = decaimiento(0.0015f); dSc = decaimiento(0.18f);
  dArpFenv = decaimiento(0.07f); dBrFenv = decaimiento(BRAAM_FENV);
  for (int i = 0; i < LIM_LOOK; i++) { limDlyL[i] = 0; limDlyR[i] = 0; }
  limRel = expf(-1.0f / (SR * 0.08f));
  bqLPF(techoL, 13000.0f, 0.7071f); bqLPF(techoR, 13000.0f, 0.7071f);   // DESPUÉS del limitador
  coefFiltro(fMasL, 12000.0f, 0.75f); fMasR = fMasL;
  memset(&gl, 0, sizeof(gl));
  dron.nota = dron.notaObj = -1;
  for (int k = 0; k < VOCES_CUERDA; k++) cue[k].nota = cue[k].notaObj = -1;
  for (int k = 0; k < 3; k++) cor[k].nota = cor[k].notaObj = -1;
  for (int k = 0; k < 7; k++) tonPh[k] = 0.13f * k;
  vto.racha = 0.5f; vto.rachaObj = 0.5f;
  setBPM(100.0f);
}

// ─── Armonía ───────────────────────────────────────────────
int notaModo(int raiz, int modo, int g) {          // grado de la escala del acorde (puede ser negativo) → nota MIDI
  int o = (g >= 0) ? g / 7 : -((-g + 6) / 7);
  return raiz + 12 * o + MODOS[modo][g - 7 * o];
}
int plegar(int n, int desde) { while (n < desde) n += 12; while (n >= desde + 12) n -= 12; return n; }
bool enEscala(int pc, int raiz, int modo) {         // ¿la clase de altura pc está en la escala del acorde?
  int d = ((pc - raiz) % 12 + 12) % 12;
  for (int k = 0; k < 7; k++) if (MODOS[modo][k] == d) return true;
  return false;
}
int acordeDe(uint32_t compas) {                     // índice del acorde que suena en ese compás
  const Tema &T = TEMAS[tema];
  int total = 0;
  for (int k = 0; k < 4 && T.prog[k].compases; k++) total += T.prog[k].compases;
  int pos = (int)(compas % (uint32_t)total);
  for (int k = 0; k < 4 && T.prog[k].compases; k++) { if (pos < T.prog[k].compases) return k; pos -= T.prog[k].compases; }
  return 0;
}
// Prioridad de cada golpe del patrón desde su letra: X primero, x después, los fantasmas al final.
// El azar dentro de cada letra es fijo por pista y paso: subir la tensión agrega golpes de a poco y
// nunca cambia los que ya sonaban.
void armarPrioridades() {
  const Tema &T = TEMAS[tema];
  for (int t = 0; t < NUM_PERC; t++) {
    const char *lin = T.bat[intenso ? 1 : 0][t];
    for (int s = 0; s < 16; s++) {
      uint32_t h = (uint32_t)(t * 16 + s + 1) * 2654435761u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
      float r = (h & 0xFFFF) / 65535.0f;
      char c = lin[s];
      prio[t][s] = c == 'X' ? 0.02f + 0.10f * r : c == 'x' ? 0.20f + 0.30f * r : c == '.' ? 0.55f + 0.40f * r : 9.0f;
    }
  }
  temaPrio = tema; intPrio = intenso ? 1 : 0;
}

// ─── Disparos ──────────────────────────────────────────────
void publicarGolpe(int ev, float v) { velGolpe[ev] = v; golpes[ev] = golpes[ev] + 1; }
static inline float velDe(char c) { return c == 'X' ? 1.0f : (c == 'x' ? 0.78f : 0.52f); }

void golpeInd(int t, float vel, bool acento, float largo) {
  VozInd &a = ind[t][sigInd[t]];
  if (a.viva) { a.eC = a.nC = dRapido3; }          // fast-kill: el parche anterior se calla en 3 ms
  sigInd[t] ^= 1;
  VozInd &v = ind[t][sigInd[t]];
  memset(&v, 0, sizeof(v));
  const Golpe &g = GOLPES[t];
  v.viva = true; v.amp = g.amp * vel; v.rev = g.rev;
  if (g.a0 > 0.0f) {
    v.inc[0] = g.f / SR; v.a[0] = g.a0;
    if (g.r2 > 0.0f) { v.inc[1] = g.f * g.r2 / SR; v.a[1] = g.a1; }
    v.env = 1.0f; v.eC = decaimiento(g.eDec * largo);
    v.cuanto = g.cuanto; v.caida = 1.0f; v.caidaC = decaimiento(g.caidaDec);
  }
  v.nz = 1.0f; v.nAmt = g.nAmt; v.nC = decaimiento((acento ? g.nAcc : g.nDec) * largo);
  v.fc0 = g.fc0; v.fc1 = g.fc1; v.q = g.q;
  if (g.barre > 0.0f) v.barreInc = 1.0f / (g.barre * SR);
  if (g.hp > 0.0f) { bqHPF(v.banda, g.hp, 0.707f); v.conBanda = true; }
  coefFiltro(v.s, v.fc0, v.q);
  float pan = clamp01(g.pan + g.panRnd * (2.0f * frnd() - 1.0f));
  v.gL = sqrtf(1.0f - pan); v.gR = sqrtf(pan);
  if (t == I_TAIKO) scObj = 1.0f;
  publicarGolpe(t, vel);
}

float pulsoDec = 0.11f;
void golpePulso(int nota, bool acento) {           // sube desde donde estaba: sin clic, sin reiniciar la fase
  pul.inc = hz((float)nota) / SR;
  pul.acento = acento ? 1.0f : 0.0f;
  pul.atk = true; pul.atkF = true;
  pul.eC = decaimiento(pulsoDec * (acento ? 1.6f : 1.0f));
  pul.eF = decaimiento(pulsoDec * 0.55f * (acento ? 1.4f : 1.0f));
  publicarGolpe(EV_PULSO, acento ? 1.0f : 0.7f);
}

void golpeArp(int nota, float altura) {            // una voz por altura, alternando lados
  ladoArp ^= 1;
  float pan = ladoArp ? 0.30f : 0.70f;
  int k = -1;
  for (int i = 0; i < VOCES_ARP; i++) if (arps[i].viva && arps[i].nota == nota) k = i;
  if (k < 0) for (int i = 0; i < VOCES_ARP; i++) if (!arps[i].viva) { k = i; break; }
  if (k < 0) { float menor = 9.0f; for (int i = 0; i < VOCES_ARP; i++) if (arps[i].amp < menor) { menor = arps[i].amp; k = i; } }
  VozPluck &v = arps[k];
  float f = hz((float)nota);
  if (!v.viva) { v.f1 = 0.13f; v.amp = 0.0f; v.s.ic1 = v.s.ic2 = 0.0f; }
  v.inc1 = f / SR; v.inv1 = 1.0f / v.inc1;
  v.nota = nota; v.base = f * 1.6f; v.oct = 1.6f + 1.4f * tensS; v.q = 0.9f + 0.6f * tensS; v.fenv = 1.0f; v.atk = true; v.viva = true;
  v.eC = decaimiento(0.16f);
  v.gL = sqrtf(1.0f - pan); v.gR = sqrtf(pan);
  arpAlturaPub = altura;
  publicarGolpe(EV_ARP, 0.7f);
}

void golpeBraam(float vel) {                       // fundamental, octava y quinta de arriba; dos sierras cada una
  const int n0 = plegar(TEMAS[tema].pc + raizAcorde, DRON_DESDE);
  const int notas[3] = { n0, n0 + 12, n0 + 19 };
  for (int k = 0; k < 3; k++) {
    float f = hz((float)notas[k]);
    br.inc[2 * k] = f * 0.9942f / SR; br.inc[2 * k + 1] = f * 1.0058f / SR;     // ±10 cents, uno a cada lado
    br.inv[2 * k] = 1.0f / br.inc[2 * k]; br.inv[2 * k + 1] = 1.0f / br.inc[2 * k + 1];
    if (!br.vivo) { br.ph[2 * k] = 0.11f * k; br.ph[2 * k + 1] = 0.53f + 0.07f * k; }
  }
  if (!br.vivo) { br.amp = 0.0f; br.sL.ic1 = br.sL.ic2 = br.sR.ic1 = br.sR.ic2 = 0.0f; }
  br.vivo = true; br.atk = true; br.fenv = 1.0f; br.vel = vel; br.eC = decaimiento(BRAAM_DEC);
  publicarGolpe(EV_BRAAM, vel);
}

void golpeSub(float vel) {                         // la caída del sub: de 150 Hz a 36 Hz
  if (!sub.vivo) sub.ph = 0.0f;
  sub.vivo = true; sub.f = 150.0f; sub.fObj = 36.0f; sub.k = 1.0f - decaimiento(0.12f);
  sub.env = vel; sub.eC = decaimiento(0.90f);
}

void golpeLatido(float vel) {
  if (!lat.vivo) lat.ph = 0.0f;
  lat.vivo = true; lat.f = 92.0f; lat.env = vel; lat.eC = decaimiento(0.15f);
  publicarGolpe(EV_LATIDO, vel);
}

void golpeCaja(int nota) {
  const int k = cajaM.sig; cajaM.sig ^= 1;
  cajaM.inc[k] = hz((float)nota) / SR; cajaM.env[k] = 1.0f;
  publicarGolpe(EV_CAJA, 0.8f);
}

// ─── Glitch ────────────────────────────────────────────────
void iniciarGlitch(int tipo, float dur) {
  if (!histN || gl.tipo != GL_NADA) return;
  if (muestraGlobal - ultimoGlitch < (uint32_t)(GLITCH_MIN_S * SR)) return;
  ultimoGlitch = muestraGlobal;
  gl.tipo = tipo; gl.t = 0.0f; gl.p = 0.0f; gl.mezcla = 0.0f; gl.vel = 1.0f;
  const float maxLargo = (float)histN - 512.0f;
  switch (tipo) {
    case GL_TARTAMUDEO: gl.grano = fminf(2.0f * sps, maxLargo); gl.ini = (float)histW - gl.grano; gl.dur = dur; gl.congelado = true; break;
    case GL_REVERSA:    gl.dur = fminf(dur, maxLargo); gl.ini = (float)histW - 1.0f; gl.congelado = true; break;
    case GL_FRENO:      gl.dur = fminf(dur, 2.0f * maxLargo); gl.pos = (float)histW - 0.5f * gl.dur; gl.congelado = true; break;
    default:            gl.dur = dur; gl.congelado = false; break;
  }
  publicarGolpe(EV_GLITCH, 1.0f);
}
// ─── Entrada, espera y voces ───────────────────────────────
// El GOLPE DE ENTRADA: braam + taiko + metal + caída del sub. Es el "1" nuevo: la música sigue desde ahí.
void golpeEntrada(float vel) {
  if (aTocando) { raizAcorde = TEMAS[tema].prog[0].semi; modoAcorde = TEMAS[tema].prog[0].modo; }   // el braam ya en el acorde con que vuelve la música
  golpeBraam(vel); golpeSub(vel);
  golpeInd(I_TAIKO, vel, true, 1.6f);
  golpeInd(I_METAL, vel, true, 1.3f);
  espera = false;
  gMusS = 1.0f;                                    // la música vuelve de golpe: durante la espera no se disparó nada, así que no hay escalón
  if (aTocando) { pasoTotal = 0xFFFFFFFFu; faltan = 0.0f; saltarBateria = true; acordeIdx = -1; }
  publicarGolpe(EV_SUSTO, vel);
}
void entrarEspera(float segundos) {
  espera = true; espT = 0; espLargo = (uint32_t)(segundos * SR);
  latFalta = 0.35f * SR; latFase = 0;
}

void golpeVoz() {
  VozMuestra &a = voces[sigVoz];
  if (a.viva) { a.obj = 0.0f; a.k = 1.0f - decaimiento(0.004f); }   // la que sonaba se apaga en 4 ms
  sigVoz ^= 1;
  VozMuestra &v = voces[sigVoz];
  memset(&v, 0, sizeof(v));
  const int cat = C.cat;
  v.viva = true; v.reversa = C.reversa; v.obj = 1.0f; v.k = 1.0f - decaimiento(0.003f);
  if (numMuestras[cat] > 0) {
    int n = numMuestras[cat], s = (int)(frnd() * n);
    if (n > 1 && s == ultimaVoz[cat]) s = (s + 1 + (int)(frnd() * (n - 1))) % n;   // nunca la misma dos veces seguidas
    ultimaVoz[cat] = s; v.cat = cat; v.s = s; v.pos = 0;
  } else {                                         // una categoría vacía en voces.h: un susurro sintetizado
    v.sintetica = true; v.dur = SUSURRO_S * SR; v.sem = 0x2545F491u ^ (uint32_t)muestraGlobal;
  }
  vozRevPub = v.reversa;
  publicarGolpe(EV_VOZ, 1.0f);
}

// La caja de música: un paso de su canción de cuna. Con la música andando la lleva el secuenciador;
// detenida, lleva su propio reloj (una capa prendida tiene que sonar aunque no haya Play).
uint32_t cajaPasoLibre = 0; float cajaFalta = 0.0f;
void pasoCaja(uint32_t paso) {
  if (pres[C_CAJA] <= 0.01f) return;
  const int raiz = TEMAS[tema].pc + raizAcorde;
  char c = MEL_CAJA[TEMAS[tema].caja % NUM_CAJA][paso & 31];
  if (c >= '1' && c <= '9') golpeCaja(notaModo(plegar(raiz, CAJA_DESDE), modoAcorde, c - '1'));
}

// ─── Un paso del secuenciador (semicorchea) ─────────────────
void avanzarPaso() {
  pasoTotal++;
  const int s = pasoTotal & 15;
  const uint32_t compas = pasoTotal >> 4;
  const Tema &T = TEMAS[tema];
  const int k = acordeDe(compas);
  if (k != acordeIdx) {                            // acorde nuevo: el dron se queda en la tónica si cabe en su escala
    acordeIdx = k; raizAcorde = T.prog[k].semi; modoAcorde = T.prog[k].modo;
    const bool pedal = enEscala(T.pc, T.pc + raizAcorde, modoAcorde);
    dron.notaObj = plegar(pedal ? T.pc : T.pc + raizAcorde, DRON_DESDE);
  }
  pasoTotalPub = pasoTotal; contadorPasos = contadorPasos + 1;
  const int raiz = T.pc + raizAcorde;

  // ── CAJA DE MÚSICA ── suena también durante la espera: es una capa, la manejas tú
  pasoCaja(pasoTotal);
  if (espera) return;                              // en la intro y el suspenso el reloj sigue, pero la música no suena

  // ── BAJO ──
  if (gPulsoS > 0.01f && C.bajoOn) {
    char c = PAT_BAJO[C.patBajo][s];
    if (c != '-') {
      int g = 0;
      switch (c | 0x20) { case 'o': g = 7; break; case '5': g = 4; break; case 'b': g = 1; break; }
      golpePulso(notaModo(plegar(raiz, PULSO_DESDE), modoAcorde, g), c >= 'A' && c <= 'Z');
    }
  }
  // ── ARPEGIO ── sube un escalón cada 2 compases con la tensión alta: la escalera
  if (gArpS > 0.01f) {
    char c = T.arp[intenso ? 1 : 0][s];
    if (c != '-') {
      const int sube = tensS > T_SUBE ? (int)((compas & 7) >> 1) : 0;
      int idx = (c == 'b') ? 0 : c - '0';
      idx += sube; if (idx > 10) idx = 10;
      int g = TONOS_ARP[idx] + (c == 'b' ? 1 : 0);
      golpeArp(notaModo(plegar(raiz, ARP_DESDE), modoAcorde, g), (float)idx / 10.0f);
    }
  }
  // ── TAMBORES ──
  const int intAhora = intenso ? 1 : 0;
  if (temaPrio != tema || intPrio != intAhora) armarPrioridades();
  if (!saltarBateria) {
    for (int t = 0; t < NUM_PERC; t++) {
      const float dens = t == I_TIC ? densTic : densBat;
      if (prio[t][s] >= dens) continue;
      char c = T.bat[intAhora][t][s];
      golpeInd(t, velDe(c) * (0.92f + 0.08f * frnd()), c == 'X', 1.0f);
    }
  }
  saltarBateria = false;
  // ── la forma con la tensión arriba: braam al comienzo de la frase, glitch al final ──
  if (s == 0 && (compas & 3) == 0 && tensS > T_BRAAM && compas > 0) golpeBraam(0.8f);
  if (s == 12 && (compas & 3) == 3 && tensS > T_GLITCH && frnd() < 0.5f)
    iniciarGlitch(frnd() < 0.5f ? GL_TARTAMUDEO : GL_REVERSA, 4.0f * sps);   // termina justo en el "1"
}

// ==============================================================================================================================================
// RENDER POR BLOQUES — cada voz de corrido, con su estado en variables locales
// ==============================================================================================================================================
IRAM_ATTR void renderSidechain(float *o, int m) {  // baja en ~2 ms al llegar el tambor grande y vuelve en ~180 ms
  float obj = scObj, s = sc;
  for (int i = 0; i < m; i++) { obj *= dSc; s += (obj - s) * 0.012f; o[i] = s; }
  scObj = obj; sc = s;
}

IRAM_ATTR void renderInd(float *oL, float *oR, float *rv, int m) {
  uint32_t sem = semillaRuido;
  const uint32_t c = muestraGlobal;
  for (int t = 0; t < NUM_PERC; t++) for (int k = 0; k < 2; k++) {
    VozInd &v = ind[t][k]; if (!v.viva) continue;
    float p0 = v.ph[0], p1 = v.ph[1];
    const float n0 = v.inc[0], n1 = v.inc[1], a0 = v.a[0], a1 = v.a[1];
    const bool dosParciales = a1 != 0.0f, conBanda = v.conBanda;
    float env = v.env, caida = v.caida, nz = v.nz, barre = v.barre;
    const float eC = v.eC, caidaC = v.caidaC, cuanto = v.cuanto, nC = v.nC, nAmt = v.nAmt, amp = v.amp * NIVEL_PERC;
    const float gL = v.gL, gR = v.gR, barreInc = v.barreInc, rev = v.rev;
    Filtro s = v.s; BQ banda = v.banda;
    for (int i = 0; i < m; i++) {
      float y = 0.0f;
      if (env > 1e-5f) {                           // capa tonal (el parche)
        float mul = 1.0f + cuanto * caida; caida *= caidaC;
        p0 += n0 * mul; if (p0 >= 1.0f) p0 -= 1.0f;
        float tt = seno(p0) * a0;
        if (dosParciales) { p1 += n1 * mul; if (p1 >= 1.0f) p1 -= 1.0f; tt += seno(p1) * a1; }
        y = tt * env;
        env *= eC;
      }
      if (barreInc > 0.0f && ((c + i) & 15) == 0) {
        barre += barreInc * 16.0f; if (barre > 1.0f) barre = 1.0f;
        coefFiltro(s, v.fc0 + (v.fc1 - v.fc0) * barre, v.q);
      }
      if (nz > 1e-5f) {                            // capa de ruido (mazo, bordonera, metal)
        float lp, bp; pasoFiltro(s, ruido(sem), lp, bp);
        y += bp * nz * nAmt; nz *= nC;
      }
      if (conBanda) y = pasoBQ(banda, y);
      y *= amp;
      oL[i] += y * gL; oR[i] += y * gR; rv[i] += y * rev;
    }
    v.ph[0] = p0; v.ph[1] = p1; v.env = env; v.caida = caida; v.nz = nz; v.barre = barre;
    v.s = s; v.banda = banda;
    if (env < 1e-3f) v.eC = dRapido;               // −60 dB → 1.5 ms → se libera a −82 dB
    if (nz < 1e-3f) v.nC = dRapido;
    if (env < 8e-5f && nz < 8e-5f) v.viva = false;
  }
  semillaRuido = sem;
}

// BAJO: dos sierras +9 cents y una cuadrada una octava abajo → saturación suave → SVF con envolvente.
// Filtro, caída y resonancia vienen del panel Bajo; la resonancia se achica sola con el filtro abierto
// (nunca un pico chillón arriba).
IRAM_ATTR void renderPulso(float *o, const float *scb, int m) {
  if (!pul.atk && pul.env < 1e-5f) return;
  float f1 = pul.f1, f2 = pul.f2, fs = pul.fs, env = pul.env, envF = pul.envF;
  bool atk = pul.atk, atkF = pul.atkF;
  const float inc = pul.inc, i2 = inc * 1.0052f, iS = inc * 0.5f, v1 = 1.0f / inc, v2 = 1.0f / i2, vS = 1.0f / iS;
  const float eC = pul.eC, eF = pul.eF;
  const float corte = PULSO_CORTE_MIN * exp2Rapido(PULSO_CORTE_OCT * pulsoCorteS), oct = 2.2f + 1.3f * pul.acento;
  const float q = (PULSO_Q_MIN + PULSO_Q_RANGO * pulsoQS) * (1.0f - 0.55f * pulsoCorteS);
  const float nivel = NIVEL_PULSO * (0.85f + 0.3f * pul.acento) / (0.75f + 0.25f * 1.6f) / sqrtf(sqrtf(fmaxf(q, 0.7f)));
  Filtro s = pul.s; const uint32_t c = muestraGlobal;
  for (int i = 0; i < m; i++) {
    if (atk) { env += 1.0f / 88.0f; if (env >= 1.0f) { env = 1.0f; atk = false; } } else env *= env < 1e-3f ? dRapido : eC;
    if (atkF) { envF += 1.0f / 66.0f; if (envF >= 1.0f) { envF = 1.0f; atkF = false; } } else envF *= eF;
    if (((c + i) & 15) == 0) coefFiltro(s, corte * exp2Rapido(oct * envF), q < 0.7f ? 0.7f : q);
    float x = (sierra(f1, inc, v1) + sierra(f2, i2, v2)) * 0.5f + cuadrada(fs, iS, vS) * 0.34f;
    float lp, bp; pasoFiltro(s, softClip(x * 1.6f), lp, bp);
    o[i] += lp * nivel * env * (1.0f - SC_PULSO * scb[i]);
  }
  pul.f1 = f1; pul.f2 = f2; pul.fs = fs; pul.env = env; pul.envF = envF; pul.atk = atk; pul.atkF = atkF; pul.s = s;
}

// ARPEGIO: una sierra PolyBLEP → pasa-bajos con envolvente → VCA, con envío al eco y a la reverb
IRAM_ATTR void renderArp(float *oL, float *oR, float *eco, float *rv, int m) {
  const uint32_t c = muestraGlobal;
  for (int k = 0; k < VOCES_ARP; k++) {
    VozPluck &v = arps[k]; if (!v.viva) continue;
    float f1 = v.f1, amp = v.amp, fenv = v.fenv; bool atk = v.atk; Filtro s = v.s;
    const float i1 = v.inc1, w1 = v.inv1, eC = v.eC, base = v.base, oct = v.oct, q = v.q;
    const float gL = v.gL * NIVEL_ARP, gR = v.gR * NIVEL_ARP;
    for (int i = 0; i < m; i++) {
      if (atk) { amp += 1.0f / 132.0f; if (amp >= 1.0f) { amp = 1.0f; atk = false; } }   // 3 ms, desde donde estaba
      else { amp *= amp < 1e-3f ? dRapido : eC; if (amp < 8e-5f) { v.viva = false; break; } }
      fenv *= dArpFenv;
      if (((c + i + 7 * k) & 31) == 0) coefFiltro(s, base * exp2Rapido(oct * fenv), q);
      float lp, bp; pasoFiltro(s, sierra(f1, i1, w1), lp, bp);
      float y = lp * amp;
      oL[i] += y * gL; oR[i] += y * gR;
      eco[i] += y * NIVEL_ARP * ECO_ENVIO_ARP; rv[i] += y * NIVEL_ARP * 0.25f;
    }
    v.f1 = f1; v.amp = amp; v.fenv = fenv; v.atk = atk; v.s = s;
  }
}

// DRON: fundamental, quinta y octava con dos sierras cada una (una a cada lado, ±7 cents) → pasa-bajos
// que respira muy lento + un seno sub. Cambia de nota sólo cuando el acorde saca a la tónica de su
// escala, y lo hace callándose y volviendo a entrar (un dron que se desliza suena a sirena).
IRAM_ATTR void renderDron(float *oL, float *oR, const float *scb, int m, float objetivo, float abre) {
  if (dron.notaObj != dron.nota) {
    dron.obj = 0.0f;
    if (dron.amp < 0.01f || dron.nota < 0) {
      dron.nota = dron.notaObj;
      const float f = hz((float)dron.nota), r[3] = { 1.0f, 1.4983f, 2.0f }, d[6] = { 0.9960f, 1.0040f, 0.9965f, 1.0035f, 0.9955f, 1.0045f };
      for (int k = 0; k < 6; k++) { dron.inc[k] = f * r[k >> 1] * d[k] / SR; dron.inv[k] = 1.0f / dron.inc[k]; }
      dron.subInc = f / SR;
    }
  } else dron.obj = objetivo;
  if (dron.nota < 0) { dron.amp = 0.0f; return; }
  if (dron.amp < 1e-5f && dron.obj == 0.0f) return;
  float amp = dron.amp, lfo = dron.lfo, sb = dron.sub;
  float p[6]; for (int k = 0; k < 6; k++) p[k] = dron.ph[k];
  const float obj = dron.obj, kA = obj > amp ? 1.0f - decaimiento(0.25f) : 1.0f - decaimiento(0.04f);
  const float f0 = dron.subInc * SR;
  Filtro sL = dron.sL, sR = dron.sR; const uint32_t c = muestraGlobal;
  for (int i = 0; i < m; i++) {
    amp += (obj - amp) * kA;
    if (((c + i) & 31) == 0) {
      lfo += 0.07f * 32.0f / SR; if (lfo >= 1.0f) lfo -= 1.0f;
      coefFiltro(sL, f0 * (4.0f + 6.0f * abre) * exp2Rapido(0.8f + 0.8f * seno(lfo)) * 0.57f, 1.3f);
      copiarCoef(sR, sL);
    }
    float l = sierra(p[0], dron.inc[0], dron.inv[0]) + 0.7f * sierra(p[2], dron.inc[2], dron.inv[2]) + 0.5f * sierra(p[4], dron.inc[4], dron.inv[4]);
    float r = sierra(p[1], dron.inc[1], dron.inv[1]) + 0.7f * sierra(p[3], dron.inc[3], dron.inv[3]) + 0.5f * sierra(p[5], dron.inc[5], dron.inv[5]);
    sb += dron.subInc; if (sb >= 1.0f) sb -= 1.0f;
    float lpL, lpR, bp; pasoFiltro(sL, l * 0.45f, lpL, bp); pasoFiltro(sR, r * 0.45f, lpR, bp);
    const float s0 = seno(sb) * 0.8f, g = amp * NIVEL_DRON * (1.0f - SC_DRON * scb[i]);
    oL[i] += (lpL + s0) * g; oR[i] += (lpR + s0) * g;
  }
  for (int k = 0; k < 6; k++) dron.ph[k] = p[k];
  dron.amp = amp; dron.lfo = lfo; dron.sub = sb; dron.sL = sL; dron.sR = sR;
}

// BRAAM + SUB: tres notas con dos sierras cada una → saturación suave → pasa-bajos que se abre de
// golpe y se cierra lento. El sub es un seno con UNA parcial: ése sí se satura para que se oiga.
IRAM_ATTR void renderBraam(float *oL, float *oR, float *rv, int m) {
  if (br.vivo) {
    float amp = br.amp, fenv = br.fenv; bool atk = br.atk;
    float p[6]; for (int k = 0; k < 6; k++) p[k] = br.ph[k];
    const float eC = br.eC, g = NIVEL_BRAAM * br.vel, oct = BRAAM_OCT * (0.7f + 0.3f * br.vel);
    Filtro sL = br.sL, sR = br.sR; const uint32_t c = muestraGlobal;
    for (int i = 0; i < m; i++) {
      if (atk) { amp += 1.0f / 265.0f; if (amp >= 1.0f) { amp = 1.0f; atk = false; } }   // 6 ms
      else { amp *= amp < 1e-3f ? dRapido : eC; if (amp < 8e-5f) { br.vivo = false; break; } }
      fenv *= dBrFenv;
      if (((c + i) & 15) == 0) { coefFiltro(sL, BRAAM_CORTE * exp2Rapido(oct * fenv), 1.25f); copiarCoef(sR, sL); }
      float l = sierra(p[0], br.inc[0], br.inv[0]) + 0.8f * sierra(p[2], br.inc[2], br.inv[2]) + 0.6f * sierra(p[4], br.inc[4], br.inv[4]);
      float r = sierra(p[1], br.inc[1], br.inv[1]) + 0.8f * sierra(p[3], br.inc[3], br.inv[3]) + 0.6f * sierra(p[5], br.inc[5], br.inv[5]);
      float lpL, lpR, bp;
      pasoFiltro(sL, softClip(l * 0.42f * BRAAM_DRIVE), lpL, bp);
      pasoFiltro(sR, softClip(r * 0.42f * BRAAM_DRIVE), lpR, bp);
      const float a = amp * g;
      oL[i] += lpL * a; oR[i] += lpR * a; rv[i] += (lpL + lpR) * a * 0.30f;
    }
    for (int k = 0; k < 6; k++) br.ph[k] = p[k];
    br.amp = amp; br.fenv = fenv; br.atk = atk; br.sL = sL; br.sR = sR;
  }
  if (sub.vivo) {
    float ph = sub.ph, f = sub.f, env = sub.env;
    const float fo = sub.fObj, k = sub.k, eC = sub.eC;
    for (int i = 0; i < m; i++) {
      f += (fo - f) * k;
      ph += f * (1.0f / SR); if (ph >= 1.0f) ph -= 1.0f;
      float y = softClip(seno(ph) * 1.8f) * env * NIVEL_SUB;
      oL[i] += y; oR[i] += y;
      env *= env < 1e-3f ? dRapido : eC;
    }
    sub.ph = ph; sub.f = f; sub.env = env;
    if (env < 8e-5f) sub.vivo = false;
  }
}

// LATIDO y VIENTO de la espera. El latido: un seno grave que cae de 92 a 46 Hz, con UNA parcial (se
// satura un poco para que se oiga en un parlante chico). El viento: dos bandas de ruido por lado que
// se mueven con rachas al azar y suben de a poco hacia el final, cada vez más fuerte.
IRAM_ATTR void renderEspera(float *o, float *oL, float *oR, float *rv, int m, float p) {
  if (lat.vivo) {
    float ph = lat.ph, f = lat.f, env = lat.env;
    for (int i = 0; i < m; i++) {
      f += (46.0f - f) * 0.0008f;
      ph += f * (1.0f / SR); if (ph >= 1.0f) ph -= 1.0f;
      o[i] += softClip(seno(ph) * 1.5f) * env * NIVEL_LATIDO;
      env *= env < 1e-3f ? dRapido : lat.eC;
    }
    lat.ph = ph; lat.f = f; lat.env = env;
    if (env < 8e-5f) lat.vivo = false;
  }
  if (!espera) return;
  uint32_t a = semVientoL, b = semVientoR;
  vto.rachaFalta -= m;
  if (vto.rachaFalta <= 0.0f) { vto.rachaObj = 0.25f + 0.75f * frnd(); vto.rachaFalta = (0.4f + 1.6f * frnd()) * SR; }
  vto.racha += (vto.rachaObj - vto.racha) * 0.02f;
  const float sube = p * p;
  const float fc = 220.0f * exp2Rapido(3.8f * sube + 0.6f * vto.racha);            // de ~250 Hz a ~4 kHz
  coefFiltro(vto.aL, fc, 1.1f); copiarCoef(vto.aR, vto.aL);
  coefFiltro(vto.bL, fc * 2.3f, 5.0f); copiarCoef(vto.bR, vto.bL);                 // el aullido, más angosto
  const float g = NIVEL_VIENTO * (0.15f + 0.85f * sube) * (0.6f + 0.4f * vto.racha);
  Filtro aL = vto.aL, aR = vto.aR, bL = vto.bL, bR = vto.bR;
  for (int i = 0; i < m; i++) {
    float lp, l1, r1, l2, r2;
    const float nl = ruido(a), nr = ruido(b);
    pasoFiltro(aL, nl, lp, l1); pasoFiltro(aR, nr, lp, r1);
    pasoFiltro(bL, nl, lp, l2); pasoFiltro(bR, nr, lp, r2);
    const float yl = (l1 + 0.45f * l2) * g, yr = (r1 + 0.45f * r2) * g;
    oL[i] += yl; oR[i] += yr; rv[i] += (yl + yr) * 0.25f;
  }
  vto.aL = aL; vto.aR = aR; vto.bL = bL; vto.bR = bR; semVientoL = a; semVientoR = b;
}

// Una voz sostenida que cambia de nota: se calla en ~10 ms, se reafina en el silencio y vuelve a entrar
// (reafinar una voz sonando es un portamento, y un pad que se desliza entre acordes suena a sirena).
static inline void retarget(VozSost &v, int nota, float objetivo) {
  v.notaObj = nota;
  if (v.nota != v.notaObj) {
    v.obj = 0.0f;
    if (v.amp < 0.02f || v.nota < 0) { v.nota = v.notaObj; v.inc = hz((float)v.nota) / SR; v.inv = 1.0f / v.inc; }
  } else v.obj = objetivo;
}

// CUERDAS QUE SUBEN: cuatro notas del acorde, una sierra cada una, a dos lados; el filtro y el
// volumen crecen con la presencia, y la voz completa sube una nota del acorde cada CUERDAS_PASO_S.
// Con la tensión alta tiemblan (trémolo de arco).
IRAM_ATTR void renderCuerdas(float *oL, float *oR, float *rv, int m, int raiz, float gan) {
  const float pr = pres[C_CUERDAS];
  if (pr < 1e-4f) { for (int k = 0; k < VOCES_CUERDA; k++) { cue[k].amp = 0.0f; cue[k].nota = -1; } return; }
  int sube = (int)(tOn[C_CUERDAS] / (uint32_t)(CUERDAS_PASO_S * SR)); if (sube > 7) sube = 7;
  for (int k = 0; k < VOCES_CUERDA; k++) retarget(cue[k], notaModo(plegar(raiz, CUERDAS_DESDE), modoAcorde, TONOS_ARP[k + sube]), 1.0f);
  coefFiltro(cueL, 260.0f * exp2Rapido(3.6f * pr), 0.9f); copiarCoef(cueR, cueL);
  const float g = NIVEL_CUERDAS * gan * pr * sqrtf(pr), trem = 0.45f * tensS;
  const float kE = 1.0f - decaimiento(0.010f), kS = 1.0f - decaimiento(0.080f);
  float tr = cueTrem; Filtro sL = cueL, sR = cueR;
  for (int i = 0; i < m; i++) {
    tr += 7.5f / SR; if (tr >= 1.0f) tr -= 1.0f;
    float l = 0.0f, r = 0.0f;
    for (int k = 0; k < VOCES_CUERDA; k++) {
      VozSost &v = cue[k]; if (v.nota < 0) continue;
      v.amp += (v.obj - v.amp) * (v.obj > v.amp ? kS : kE);
      const float y = sierra(v.ph, v.inc, v.inv) * v.amp;
      if (k & 1) { r += y * 0.8f; l += y * 0.35f; } else { l += y * 0.8f; r += y * 0.35f; }
    }
    float lpL, lpR, bp; pasoFiltro(sL, l, lpL, bp); pasoFiltro(sR, r, lpR, bp);
    const float a = g * (1.0f - trem * (0.5f + 0.5f * seno(tr)));
    oL[i] += lpL * a; oR[i] += lpR * a; rv[i] += (lpL + lpR) * a * 0.35f;
  }
  cueTrem = tr; cueL = sL; cueR = sR;
}

// CORO OSCURO: tres notas del acorde en sierra con un vibrato lento → tres formantes que van de "uu"
// a "aa" con la presencia (la vocal se abre: ése es su crecer en tono).
IRAM_ATTR void renderCoro(float *oL, float *oR, float *rv, int m, int raiz, float gan) {
  const float pr = pres[C_CORO];
  if (pr < 1e-4f) { for (int k = 0; k < 3; k++) { cor[k].amp = 0.0f; cor[k].nota = -1; } return; }
  for (int k = 0; k < 3; k++) retarget(cor[k], notaModo(plegar(raiz, CORO_DESDE), modoAcorde, TONOS_ARP[k]), 1.0f);
  const float F[3][2] = { { 300.0f, 730.0f }, { 870.0f, 1090.0f }, { 2240.0f, 2440.0f } }, GF[3] = { 1.0f, 0.55f, 0.28f }, QF[3] = { 6.0f, 8.0f, 9.0f };
  for (int k = 0; k < 3; k++) coefFiltro(corF[k], F[k][0] + (F[k][1] - F[k][0]) * pr, QF[k]);
  const float g = NIVEL_CORO * gan * pr * sqrtf(pr);
  const float kE = 1.0f - decaimiento(0.010f), kS = 1.0f - decaimiento(0.150f);
  float vb = corVib; const uint32_t c = muestraGlobal;
  float incV[3]; for (int k = 0; k < 3; k++) incV[k] = cor[k].inc;
  for (int i = 0; i < m; i++) {
    if (((c + i) & 15) == 0) {
      vb += 5.0f * 16.0f / SR; if (vb >= 1.0f) vb -= 1.0f;
      for (int k = 0; k < 3; k++) incV[k] = cor[k].inc * (1.0f + 0.0035f * seno(vb + 0.31f * k >= 1.0f ? vb + 0.31f * k - 1.0f : vb + 0.31f * k));
    }
    float x = 0.0f;
    for (int k = 0; k < 3; k++) {
      VozSost &v = cor[k]; if (v.nota < 0) continue;
      v.amp += (v.obj - v.amp) * (v.obj > v.amp ? kS : kE);
      x += sierra(v.ph, incV[k], 1.0f / incV[k]) * v.amp;
    }
    float y = 0.0f, lp, bp;
    for (int k = 0; k < 3; k++) { pasoFiltro(corF[k], x, lp, bp); y += bp * GF[k]; }
    y *= g;
    oL[i] += y; oR[i] += y; rv[i] += y * 0.6f;
  }
  corVib = vb;
}

// TONO DE TENSIÓN: un tono de Shepard–Risset. Siete senos a una octava entre sí con una campana de
// volumen sobre la altura: todos suben sin parar y el que se va por arriba vuelve por abajo en
// silencio, así el tono sube y sube y nunca llega. No es una nota: es la tensión misma.
IRAM_ATTR void renderTono(float *oL, float *oR, float *rv, int m, float gan) {
  const float pr = pres[C_TONO];
  if (pr < 1e-4f) return;
  const float base = 27.5f * exp2f((float)TEMAS[tema].pc / 12.0f);
  float inc[7], w[7];
  for (int k = 0; k < 7; k++) {
    float oct = k + tonFase; if (oct >= 7.0f) oct -= 7.0f;
    inc[k] = base * exp2Rapido(oct) / SR;
    const float d = (oct - 3.5f) * (1.0f / 1.1f);
    w[k] = expf(-0.5f * d * d);
  }
  const float g = NIVEL_TONO * gan * pr * sqrtf(pr);
  for (int i = 0; i < m; i++) {
    float y = 0.0f;
    for (int k = 0; k < 7; k++) { tonPh[k] += inc[k]; if (tonPh[k] >= 1.0f) tonPh[k] -= 1.0f; y += w[k] * seno(tonPh[k]); }
    y *= g;
    oL[i] += y; oR[i] += y; rv[i] += y * 0.3f;
  }
  tonFase += (float)m / (TONO_OCTAVA_S * SR); if (tonFase >= 1.0f) tonFase -= 1.0f;
}

// CAJA DE MÚSICA: seno + octava + duodécima (sólo armónicos enteros: las parciales inarmónicas sonaban
// a olla), caída rápida. Con el tiempo se va DEFORMANDO: un vaivén de afinación que crece y una caída
// de altura, como una caja a la que se le acaba la cuerda.
IRAM_ATTR void renderCaja(float *oL, float *oR, float *eco, float *rv, int m, float gan) {
  const float pr = pres[C_CAJA];
  if (pr < 1e-4f && cajaM.env[0] < 1e-4f && cajaM.env[1] < 1e-4f) return;
  const float deforma = clamp01((float)tOn[C_CAJA] / (CAJA_DEFORMA_S * SR));
  const float dCaja = decaimiento(0.55f);
  const float g = NIVEL_CAJA * gan * pr;
  float lfo = cajaM.lfo; const uint32_t c = muestraGlobal;
  float mul = 1.0f;
  for (int i = 0; i < m; i++) {
    if (((c + i) & 15) == 0) {
      lfo += 0.7f * 16.0f / SR; if (lfo >= 1.0f) lfo -= 1.0f;
      mul = (1.0f - 0.028f * deforma) * (1.0f + 0.020f * deforma * seno(lfo));   // hasta −½ semitono y ±35 cents
    }
    float y = 0.0f;
    for (int k = 0; k < 2; k++) {
      if (cajaM.env[k] < 1e-4f) continue;
      float &ph = cajaM.ph[k]; ph += cajaM.inc[k] * mul; if (ph >= 1.0f) ph -= 1.0f;
      float p2 = ph + ph; if (p2 >= 1.0f) p2 -= 1.0f;
      float p3 = ph * 3.0f; p3 -= (int)p3;
      y += (seno(ph) + 0.32f * seno(p2) + 0.10f * seno(p3)) * cajaM.env[k];
      cajaM.env[k] *= dCaja;
    }
    y *= g;
    oL[i] += y * 0.9f; oR[i] += y * 0.7f; eco[i] += y * 0.35f; rv[i] += y * 0.45f;
  }
  cajaM.lfo = lfo;
}

// VOCES: la muestra guardada en la flash leída al derecho o al revés, a la velocidad del pitch del panel
// Voces (interpolación lineal), con fundidos de 3–5 ms en los bordes y sus envíos a reverb y eco. Una
// categoría vacía suena como un susurro: ruido por dos formantes, con el mismo pitch y la misma reversa.
IRAM_ATTR void renderVoces(float *oL, float *oR, float *rv, float *eco, int m, float revV, float ecoV, float &nivel) {
  float pico = 0.0f;
  for (int k = 0; k < VOCES_VOZ; k++) {
    VozMuestra &v = voces[k]; if (!v.viva) continue;
    float amp = v.amp, lp = v.lp; const float obj = v.obj, kk = v.k;
    float paso = pasoVoz;
    if (!v.sintetica) {
      // Se lee directo de la flash. Al revés, la muestra i es la i-ésima contada desde el final.
      const int16_t *d = VOZ_DATOS[v.cat][v.s];
      const int32_t n = (int32_t)VOZ_LARGO[v.cat][v.s], borde = (int32_t)(220.0f * paso) + 2;
      const bool rev = v.reversa;
      int32_t pos = v.pos; float fr = v.frac;
      for (int i = 0; i < m; i++) {
        fr += paso; int adv = (int)fr; fr -= adv; pos += adv;
        if (pos >= n - 1) { v.viva = false; break; }
        const float a0 = rev ? d[n - 1 - pos] : d[pos], a1 = rev ? d[n - 2 - pos] : d[pos + 1];
        float x = (a0 + (a1 - a0) * fr) * (1.0f / 32768.0f);
        const int32_t falta = n - 1 - pos;
        const float fin = falta < borde ? (float)falta / (float)borde : 1.0f;
        amp += (obj - amp) * kk;
        lp += (x - lp) * 0.55f;                    // pasa-bajos suave: el pitch hacia arriba no chilla
        float y = lp * amp * fin * NIVEL_VOZ;
        oL[i] += y; oR[i] += y; rv[i] += y * revV; eco[i] += y * ecoV;
        float aa = fabsf(y); if (aa > pico) pico = aa;
      }
      v.pos = pos; v.frac = fr;
    } else {
      uint32_t sem = v.sem; float t = v.t; const float dur = v.dur;
      Filtro f1 = v.f1, f2 = v.f2;
      for (int i = 0; i < m; i++) {
        float p = t / dur; if (p >= 1.0f) { v.viva = false; break; }
        if (v.reversa) p = 1.0f - p;
        if ((i & 15) == 0) {                       // "jjjaaa…": la vocal se abre y se cierra
          float abre = p < 0.25f ? p * 4.0f : 1.0f - (p - 0.25f) * 1.2f; if (abre < 0.0f) abre = 0.0f;
          coefFiltro(f1, (420.0f + 480.0f * abre) * paso, 5.0f);
          coefFiltro(f2, (1300.0f + 900.0f * abre) * paso, 6.0f);
        }
        float e = p < 0.12f ? p / 0.12f : (1.0f - p) / 0.88f; e = e * e;
        float n = ruido(sem), lpA, bp1, bp2;
        pasoFiltro(f1, n, lpA, bp1); pasoFiltro(f2, n, lpA, bp2);
        amp += (obj - amp) * kk;
        float y = (bp1 + 0.7f * bp2) * e * amp * NIVEL_VOZ * 1.5f;
        oL[i] += y; oR[i] += y; rv[i] += y * revV; eco[i] += y * ecoV;
        float a = fabsf(y); if (a > pico) pico = a;
        t += paso;
      }
      v.sem = sem; v.t = t; v.f1 = f1; v.f2 = f2;
    }
    v.amp = amp; v.lp = lp;
    if (obj == 0.0f && amp < 1e-4f) v.viva = false;
  }
  nivel = pico;
}

// ECO ping-pong a corchea con puntillo (sigue al tempo cruzando de cabezal en 50 ms)
IRAM_ATTR void renderEco(const float *in, float *oL, float *oR, int m) {
  int objetivo = (int)(3.0f * sps); if (objetivo > ECO_MAX - 2) objetivo = ECO_MAX - 2;
  if (ecoXf >= 1.0f && abs(objetivo - ecoN) > 32) { ecoN2 = objetivo; ecoXf = 0.0f; }
  float xf = ecoXf, lpL = ecoLpL, lpR = ecoLpR, hpL = ecoHpL, hpR = ecoHpR; int ei = ecoIdx;
  for (int i = 0; i < m; i++) {
    int r1 = ei - ecoN; if (r1 < 0) r1 += ECO_MAX;
    float dl = ecoL[r1] * (1.0f / 32768.0f), dr = ecoR[r1] * (1.0f / 32768.0f);
    if (xf < 1.0f) {
      int r2 = ei - ecoN2; if (r2 < 0) r2 += ECO_MAX;
      dl += (ecoL[r2] * (1.0f / 32768.0f) - dl) * xf; dr += (ecoR[r2] * (1.0f / 32768.0f) - dr) * xf;
      xf += 1.0f / 2205.0f; if (xf >= 1.0f) { xf = 1.0f; ecoN = ecoN2; }
    }
    lpL += (dl - lpL) * 0.45f; lpR += (dr - lpR) * 0.45f;      // cada vuelta más oscura
    hpL += (lpL - hpL) * 0.010f; hpR += (lpR - hpR) * 0.010f;  // y sin barro abajo
    float inL = in[i] + ECO_FEEDBACK * (lpR - hpR), inR = ECO_FEEDBACK * (lpL - hpL);   // ping-pong
    if (inL > 0.99f) inL = 0.99f; if (inL < -0.99f) inL = -0.99f;
    if (inR > 0.99f) inR = 0.99f; if (inR < -0.99f) inR = -0.99f;
    ecoL[ei] = (int16_t)(inL * 32767.0f); ecoR[ei] = (int16_t)(inR * 32767.0f);
    ei = (ei + 1) & (ECO_MAX - 1);
    oL[i] += dl * ECO_MEZCLA; oR[i] += dr * ECO_MEZCLA;
  }
  ecoIdx = ei; ecoXf = xf; ecoLpL = lpL; ecoLpR = lpR; ecoHpL = hpL; ecoHpR = hpR;
}

// REVERB: peines con amortiguación + pasa-todos; la entrada se normaliza por su propia realimentación
static inline float peine(float *b, int n, int &i, float &lp, float x) {
  float y = b[i]; lp += (y - lp) * 0.42f;
  b[i] = x + lp * REV_FB; if (++i >= n) i = 0; return y;
}
static inline float pasaTodo(float *b, int n, int &i, float x) {
  float d = b[i], y = d - 0.5f * x; b[i] = x + 0.5f * d; if (++i >= n) i = 0; return y;
}
IRAM_ATTR void renderReverb(const float *in, float *oL, float *oR, int m) {
  const float kIn = 0.28f * (1.0f - REV_FB);
  for (int i = 0; i < m; i++) {
    const float x = in[i] * kIn + DITHER;
    float cl = peine(rvC1, RV_N1, rvI[0], rvLp[0], x) + peine(rvC3, RV_N3, rvI[2], rvLp[2], x)
             + peine(rvC5, RV_N5, rvI[4], rvLp[4], x) + peine(rvC7, RV_N7, rvI[6], rvLp[6], x);
    float cr = peine(rvC2, RV_N2, rvI[1], rvLp[1], x) + peine(rvC4, RV_N4, rvI[3], rvLp[3], x)
             + peine(rvC6, RV_N6, rvI[5], rvLp[5], x) + peine(rvC8, RV_N8, rvI[7], rvLp[7], x);
    float wl = pasaTodo(rvA2, RV_A2, rvJ[1], pasaTodo(rvA1, RV_A1, rvJ[0], cl));
    float wr = pasaTodo(rvA4, RV_A4, rvJ[3], pasaTodo(rvA3, RV_A3, rvJ[2], cr));
    oL[i] += wl * REV_RETORNO; oR[i] += wr * REV_RETORNO;
  }
}

volatile uint32_t medSec[4];
#if MEDIR && !defined(SIMULADOR)
#define CICLOS() ESP.getCycleCount()
#else
#define CICLOS() 0u
#endif
float nivelVozBloque = 0.0f, revVozB = 0.4f, ecoVozB = 0.0f;
void renderTramo(int off, int m, float objDron, float abreDron, float pEsp) {
  renderSidechain(bufSc + off, m);
  renderInd(bufMusL + off, bufMusR + off, bufRev + off, m);
  renderPulso(bufPul + off, bufSc + off, m);        // el bajo va al centro: se suma a los dos lados en el master
  renderArp(bufMusL + off, bufMusR + off, bufEco + off, bufRev + off, m);
  renderDron(bufDronL + off, bufDronR + off, bufSc + off, m, objDron, abreDron);
  renderBraam(bufHitL + off, bufHitR + off, bufRev + off, m);
  renderEspera(bufLat + off, bufHitL + off, bufHitR + off, bufRev + off, m, pEsp);
  const int raiz = TEMAS[tema].pc + raizAcorde;
  renderCuerdas(bufCapL + off, bufCapR + off, bufRev + off, m, raiz, gCapS[C_CUERDAS]);
  renderCoro(bufCapL + off, bufCapR + off, bufRev + off, m, raiz, gCapS[C_CORO]);
  renderTono(bufCapL + off, bufCapR + off, bufRev + off, m, gCapS[C_TONO]);
  renderCaja(bufCapL + off, bufCapR + off, bufEco + off, bufRev + off, m, gCapS[C_CAJA]);
  float nv = 0.0f; renderVoces(bufVozL + off, bufVozR + off, bufRev + off, bufEco + off, m, revVozB, ecoVozB, nv);
  if (nv > nivelVozBloque) nivelVozBloque = nv;
  muestraGlobal += m;
}

// ==============================================================================================================================================
// UN BLOQUE DE AUDIO (128 muestras ≈ 2.9 ms)
// ==============================================================================================================================================
static i2s_chan_handle_t tx_chan;
int16_t salida[BUFFER_SAMPLES * 2];
uint32_t vistoSuspenso = 0, vistoVoz = 0;
bool capaVista[4] = { false, false, false, false };

IRAM_ATTR void calcularBloque() {
  // ── Borde del buffer: aplicar los pedidos ──
  memcpy(&C, (const void *)&reqC, sizeof(C));
  if (C.tocar && !aTocando) {                      // Play: el tema nuevo y la intro desde el silencio
    tema = C.tema; temaPub = tema; pasoTotal = 0xFFFFFFFFu; faltan = 0.0f; acordeIdx = -1;
    gMusS = 0.0f; gDronS = 0.0f; entrarEspera(INTRO_S);
  }
  if (!C.tocar && aTocando) espera = false;
  if (C.tema != tema && C.tocar && aTocando) { tema = C.tema; temaPub = tema; acordeIdx = -1; }   // BTN2: el tema nuevo entra en el próximo paso
  aTocando = C.tocar; tocandoPub = aTocando;

  if (reqSuspenso != vistoSuspenso) {              // BTN3 del General: entra al suspenso; otra vez, entra la música ya
    vistoSuspenso = reqSuspenso;
    if (aTocando) { if (espera) golpeEntrada(1.0f); else entrarEspera(SUSPENSO_S); }
  }
  if (reqVoz != vistoVoz) { vistoVoz = reqVoz; golpeVoz(); }
  for (int k = 0; k < 4; k++) if (C.capa[k] != capaVista[k]) { capaVista[k] = C.capa[k]; if (C.capa[k]) tOn[k] = 0; }
  if (!aTocando) {                                 // detenido: la caja de música lleva su propio reloj
    cajaFalta -= BUFFER_SAMPLES;
    if (cajaFalta <= 0.0f) { cajaFalta += sps; pasoCaja(cajaPasoLibre++); }
  }

  float pEsp = 0.0f;
  if (espera) {                                    // el latido que se acelera y el golpe al final
    espT += BUFFER_SAMPLES;
    pEsp = clamp01((float)espT / (float)espLargo);
    latFalta -= BUFFER_SAMPLES;
    if (latFalta <= 0.0f) {
      const float periodo = LAT_INI_S + (LAT_FIN_S - LAT_INI_S) * pEsp;
      const float fuerza = 0.55f + 0.45f * pEsp;
      golpeLatido(latFase == 0 ? fuerza : fuerza * 0.65f);
      latFalta += (latFase == 0 ? 0.24f * periodo : 0.76f * periodo) * SR; latFase ^= 1;
    }
    if (espT >= espLargo) golpeEntrada(1.0f);
  }
  suspensoPub = espera; esperaPub = pEsp;

  // ── Pots (por bloque; las ganancias se interpolan por muestra) ──
  float b = BPM_MIN + BPM_RANGO * pPot[P_GENERAL][1];
  if (fabsf(b - bpm) > 0.6f) setBPM(roundf(b));   // BPM entero con histéresis: el ruido del ADC no hace bailar el tempo
  tensS += (pPot[P_GENERAL][2] - tensS) * 0.05f; tensionPub = tensS;
  if (tensS > T_INTENSO + 0.04f) intenso = true; else if (tensS < T_INTENSO - 0.04f) intenso = false;
  densTic = rampa(tensS, T_TIC0, T_TIC1) * 1.05f;
  densBat = rampa(tensS, T_BAT0, T_BAT1) * 1.05f;
  lpfS += (pPot[P_GENERAL][3] - lpfS) * 0.15f;
  pulsoCorteS += (pPot[P_BAJO][1] - pulsoCorteS) * 0.15f;
  pulsoQS += (pPot[P_BAJO][3] - pulsoQS) * 0.15f;
  pulsoDec = PULSO_DEC_MIN * powf(PULSO_DEC_RANGO, pPot[P_BAJO][2]);
  {
    const float p = pPot[P_VOCES][1];
    float semi = 0.0f;
    if (p < 0.5f - VOZ_CENTRO)      semi = -VOZ_SEMI_ABAJO * (0.5f - VOZ_CENTRO - p) / (0.5f - VOZ_CENTRO);
    else if (p > 0.5f + VOZ_CENTRO) semi =  VOZ_SEMI_ARRIBA * (p - 0.5f - VOZ_CENTRO) / (0.5f - VOZ_CENTRO);
    pasoVoz += (exp2f(semi / 12.0f) - pasoVoz) * 0.3f;
  }
  revVozB += (pPot[P_VOCES][2] * 1.3f - revVozB) * 0.2f;
  ecoVozB += (pPot[P_VOCES][3] * 0.9f - ecoVozB) * 0.2f;
  gVozS += (ganVol(pPot[P_VOCES][0]) - gVozS) * 0.25f;
  gBajoS += ((C.bajoOn ? ganVol(pPot[P_BAJO][0]) : 0.0f) - gBajoS) * 0.25f;
  // capas: la presencia crece de a poco mientras está prendida y se apaga despacio al soltarla
  for (int k = 0; k < 4; k++) {
    if (C.capa[k]) { pres[k] += (float)BUFFER_SAMPLES / (CAPA_CRECE_S * SR); if (pres[k] > 1.0f) pres[k] = 1.0f; tOn[k] += BUFFER_SAMPLES; }
    else { pres[k] -= (float)BUFFER_SAMPLES / (CAPA_APAGA_S * SR); if (pres[k] < 0.0f) pres[k] = 0.0f; }
    gCapS[k] += (ganVol(pPot[P_CAPAS][k]) - gCapS[k]) * 0.25f;
  }
  capasPub = fmaxf(fmaxf(pres[0], pres[1]), fmaxf(pres[2], pres[3]));
  // ganancias de las capas de la tensión
  const float objPulso = aTocando ? rampa(tensS, T_PULSO0, T_PULSO1) : 0.0f;
  const float objArp   = aTocando ? rampa(tensS, T_ARP0, T_ARP1) : 0.0f;
  gPulsoS += (objPulso - gPulsoS) * 0.1f; gArpS += (objArp - gArpS) * 0.1f;
  const float objDron  = aTocando ? 1.0f : 0.0f;
  const float abreDron = 0.3f * tensS;
  const float gMus0 = gMusS, gDron0 = gDronS, v0 = volS;
  const float objMus = espera ? 0.0f : 1.0f, objDronG = espera ? SUS_DRON0 + (SUS_DRON1 - SUS_DRON0) * pEsp : 1.0f;
  const float kSus = 1.0f - expf(-(float)BUFFER_SAMPLES / (SUS_ENTRA_S * SR));
  gMusS += (objMus - gMusS) * (espera ? kSus : 0.5f);
  gDronS += (objDronG - gDronS) * (espera && gDronS > objDronG ? kSus : 0.03f);
  volS += (pPot[P_GENERAL][0] * pPot[P_GENERAL][0] - volS) * 0.15f;
  coefFiltro(fMasL, corteBus(lpfS), 0.75f); copiarCoef(fMasR, fMasL);
  const float abMas = abiertoBus(lpfS);

  for (int i = 0; i < BUFFER_SAMPLES; i++) {
    bufMusL[i] = 0.0f; bufMusR[i] = 0.0f; bufDronL[i] = 0.0f; bufDronR[i] = 0.0f;
    bufHitL[i] = 0.0f; bufHitR[i] = 0.0f; bufVozL[i] = 0.0f; bufVozR[i] = 0.0f;
    bufCapL[i] = 0.0f; bufCapR[i] = 0.0f;
    bufRev[i] = 0.0f; bufEco[i] = 0.0f; bufLat[i] = 0.0f; bufPul[i] = 0.0f;
  }
  // ── Síntesis por tramos: los pasos caen en la muestra exacta ──
  uint32_t c0 = CICLOS();
  nivelVozBloque = 0.0f;
  int hecho = 0;
  while (hecho < BUFFER_SAMPLES) {
    int m2 = BUFFER_SAMPLES - hecho;
    if (aTocando) {
      if (faltan <= 0.0f) { avanzarPaso(); faltan += sps; }
      int hasta = (int)ceilf(faltan); if (hasta < 1) hasta = 1;
      if (hasta < m2) m2 = hasta;
      faltan -= (float)m2;
    }
    renderTramo(hecho, m2, objDron, abreDron, pEsp);
    hecho += m2;
  }
  float rvL[BUFFER_SAMPLES], rvR[BUFFER_SAMPLES], ecL[BUFFER_SAMPLES], ecR[BUFFER_SAMPLES];
  for (int i = 0; i < BUFFER_SAMPLES; i++) { rvL[i] = rvR[i] = ecL[i] = ecR[i] = 0.0f; }
  renderEco(bufEco, ecL, ecR, BUFFER_SAMPLES);
  renderReverb(bufRev, rvL, rvR, BUFFER_SAMPLES);
  envPulsoPub = pul.env; vozNivelPub = nivelVozBloque * gVozS; glitchPub = gl.tipo != GL_NADA;
  uint32_t c1 = CICLOS();

  // ── Master ──
  //   música (tambores · bajo · arpegio: calla en la espera) · dron · capas · voces · eco · reverb (sus
  //   colas siguen sonando en el silencio) → filtro general → golpes y latido (por fuera del filtro) →
  //   DC → GLITCH → limitador → 13 kHz
  const float inv = 1.0f / BUFFER_SAMPLES;
  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    const float x = (float)n * inv;
    const float gM = gMus0 + (gMusS - gMus0) * x, gD = gDron0 + (gDronS - gDron0) * x, vv = (v0 + (volS - v0) * x) * MASTER;
    const float pul2 = bufPul[n] * gBajoS;
    float l = (bufMusL[n] + pul2) * gM + bufDronL[n] * gD + bufCapL[n] + bufVozL[n] * gVozS + ecL[n] + rvL[n];
    float r = (bufMusR[n] + pul2) * gM + bufDronR[n] * gD + bufCapR[n] + bufVozR[n] * gVozS + ecR[n] + rvR[n];
    float lp, bp;
    pasoFiltro(fMasL, l, lp, bp); l += (lp - l) * (1.0f - abMas);      // filtro general
    pasoFiltro(fMasR, r, lp, bp); r += (lp - r) * (1.0f - abMas);
    l = (l + bufHitL[n] + bufLat[n]) * vv; r = (r + bufHitR[n] + bufLat[n]) * vv;
    float yl = l - dcX1L + 0.9985f * dcY1L; dcX1L = l; dcY1L = yl; l = yl;
    float yr = r - dcX1R + 0.9985f * dcY1R; dcX1R = r; dcY1R = yr; r = yr;

    // GLITCH: la mezcla se guarda en la memoria; el glitch la relee (tartamudeo, reversa, freno) o la corta
    if (histN) {
      if (!gl.congelado) {
        float hl = l * 16384.0f, hr = r * 16384.0f;
        if (hl > 32767.0f) hl = 32767.0f; if (hl < -32768.0f) hl = -32768.0f;
        if (hr > 32767.0f) hr = 32767.0f; if (hr < -32768.0f) hr = -32768.0f;
        histL[histW] = (int16_t)hl; histR[histW] = (int16_t)hr; histW = (histW + 1) & (histN - 1);
      }
      if (gl.tipo != GL_NADA) {
        float wl = l, wr = r, ventana = 1.0f;
        const float t = gl.t, msk = (float)histN;
        switch (gl.tipo) {
          case GL_TARTAMUDEO: {                    // 1/8 → 1/16 → 1/32 → 1/64: un redoble que se cierra
            float g = t < 4.0f * sps ? gl.grano : t < 6.0f * sps ? gl.grano * 0.5f : t < 7.0f * sps ? gl.grano * 0.25f : gl.grano * 0.125f;
            float p = gl.p; while (p >= g) p -= g;    // la fase dentro del grano, sin fmodf por muestra (en el S3 es lenta)
            gl.p = p + 1.0f;
            ventana = fminf(1.0f, fminf(p, g - p) * (1.0f / 66.0f));
            float q = gl.ini + p; if (q < 0.0f) q += msk; uint32_t k = (uint32_t)q & (histN - 1);
            wl = histL[k] * (1.0f / 16384.0f); wr = histR[k] * (1.0f / 16384.0f);
          } break;
          case GL_REVERSA: {
            float q = gl.ini - t; if (q < 0.0f) q += msk; uint32_t k = (uint32_t)q & (histN - 1);
            wl = histL[k] * (1.0f / 16384.0f); wr = histR[k] * (1.0f / 16384.0f);
          } break;
          case GL_FRENO: {                         // la cinta se frena: la velocidad cae de 1 a 0
            gl.vel = 1.0f - t / gl.dur; if (gl.vel < 0.0f) gl.vel = 0.0f;
            float q = gl.pos; if (q < 0.0f) q += msk;
            uint32_t k = (uint32_t)q & (histN - 1), k2 = (k + 1) & (histN - 1); float fr = q - floorf(q);
            wl = (histL[k] + (histL[k2] - histL[k]) * fr) * (1.0f / 16384.0f);
            wr = (histR[k] + (histR[k2] - histR[k]) * fr) * (1.0f / 16384.0f);
            gl.pos += gl.vel; if (gl.pos >= msk) gl.pos -= msk;
            ventana = fminf(1.0f, gl.vel * 6.0f);
          } break;
          default: {                               // corte a fusas, con rampas de 1 ms
            const float per = sps * 0.5f;
            float p = gl.p; while (p >= per) p -= per;
            gl.p = p + 1.0f;
            const float f = p / per;
            ventana = f < 0.5f ? fminf(1.0f, fminf(f, 0.5f - f) * per / 44.0f) : 0.0f;
          } break;
        }
        const float objMez = gl.t < gl.dur - 64.0f ? 1.0f : 0.0f;
        gl.mezcla += (objMez - gl.mezcla) * 0.05f;
        l += (wl * ventana - l) * gl.mezcla; r += (wr * ventana - r) * gl.mezcla;
        gl.t += 1.0f;
        if (gl.t >= gl.dur && gl.mezcla < 0.002f) { gl.tipo = GL_NADA; gl.congelado = false; }
      }
    }

    float pk = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
    if (pk > medPico) medPico = pk;
    if (pk > limEnv) limEnv = pk; else limEnv = pk + (limEnv - pk) * limRel;
    float limObj = (limEnv > TECHO) ? (TECHO / limEnv) : 1.0f;
    limGain += (limObj - limGain) * 0.020f;        // ganancia suavizada: un escalón por muestra es un clic
    float sl = limDlyL[limDlyIdx], sr2 = limDlyR[limDlyIdx];
    limDlyL[limDlyIdx] = l; limDlyR[limDlyIdx] = r;
    limDlyIdx++; if (limDlyIdx >= LIM_LOOK) limDlyIdx = 0;
    if (limGain < medLimMin) medLimMin = limGain;
    l = pasoBQ(techoL, sl * limGain); r = pasoBQ(techoR, sr2 * limGain);
    if (fabsf(l) > 0.95f || fabsf(r) > 0.95f) medTecho = medTecho + 1;
    l = techoSuave(l); r = techoSuave(r);
    int32_t li = (int32_t)(l * 32000.0f), ri = (int32_t)(r * 32000.0f);
    salida[2 * n]     = (int16_t)(li > 32767 ? 32767 : li < -32768 ? -32768 : li);
    salida[2 * n + 1] = (int16_t)(ri > 32767 ? 32767 : ri < -32768 ? -32768 : ri);
  }
  medSec[0] += (c1 - c0) >> 6; medSec[1] += (CICLOS() - c1) >> 6;
}

// ==============================================================================================================================================
// LUCES ROJAS (tarea de LEDs, core 0)
// ==============================================================================================================================================
uint32_t vistoGolpe[NUM_EV];
float    flash[NUM_EV];
bool     nuevo[NUM_EV];
uint32_t rngLed = 0xA5A5A5A5u;
static inline float lrnd() { rngLed ^= rngLed << 13; rngLed ^= rngLed >> 17; rngLed ^= rngLed << 5; return (rngLed >> 8) * (1.0f / 16777216.0f); }
unsigned long sustoMs = 0;
bool     huboSusto = false;
float    pulsoNegra = 0.0f, ondaVoz = 0.0f;
uint8_t  cajaLed[3] = { 0, 4, 8 };
int      ampolleta = 3;
uint32_t vistoContador = 0;
const CRGB COLOR_PANEL[NUM_PANELES] = { CRGB(200, 0, 0), CRGB(200, 60, 0), CRGB(190, 120, 0), CRGB(120, 0, 160) };   // rojo · naranjo · ámbar · violeta

void leerEventos() {
  for (int ev = 0; ev < NUM_EV; ev++) {
    uint32_t g = golpes[ev];
    nuevo[ev] = (g != vistoGolpe[ev]); vistoGolpe[ev] = g;
    if (nuevo[ev]) flash[ev] = 0.5f + 0.5f * velGolpe[ev];
  }
  if (nuevo[EV_SUSTO]) { sustoMs = millis(); huboSusto = true; }
  if (nuevo[I_CAJA]) for (int k = 0; k < 3; k++) cajaLed[k] = (uint8_t)(lrnd() * 9.99f);
  uint32_t cp = contadorPasos;
  if (cp != vistoContador) { vistoContador = cp; if ((pasoTotalPub & 3) == 0) pulsoNegra = 1.0f; }
}

void renderTira(unsigned long t) {
  float v[LEDS_TIRA], w[LEDS_TIRA];
  const bool corre = tocandoPub, sus = suspensoPub;
  const float resp = 0.5f + 0.5f * sinf(t * 0.0016f);
  const float base = corre ? 0.05f + 0.05f * resp : 0.02f + 0.03f * resp;
  for (int i = 0; i < LEDS_TIRA; i++) { v[i] = base; w[i] = 0.0f; }
  if (sus) {                                       // la espera: casi nada, una brasa que crece con el viento, el latido y una ampolleta que se muere
    const float e = esperaPub;
    for (int i = 0; i < LEDS_TIRA; i++) v[i] = 0.04f + 0.30f * e * e * (0.6f + 0.4f * lrnd()) + (0.10f + 0.25f * e) * flash[EV_LATIDO];
    if (lrnd() < 0.02f) ampolleta = (int)(lrnd() * 9.99f);
    if (lrnd() < 0.35f) v[ampolleta] += 0.12f * lrnd();
  } else {
    const float todo = 0.85f * flash[I_TAIKO] + 0.35f * flash[I_TOM] + 0.15f * flash[EV_PULSO] + 0.55f * flash[EV_BRAAM] + 0.45f * flash[I_METAL];
    for (int i = 0; i < LEDS_TIRA; i++) { v[i] += todo; w[i] += 0.15f * flash[I_METAL]; }
    for (int k = 0; k < 3; k++) v[cajaLed[k]] += 0.8f * flash[I_CAJA];
    v[(pasoTotalPub >> 1) % LEDS_TIRA] += 0.18f * flash[I_TIC];
    const float pa = arpAlturaPub * (LEDS_TIRA - 1);                 // el arpegio sube y la luz sube con él
    for (int i = 0; i < LEDS_TIRA; i++) { float d = fabsf(i - pa); if (d < 1.5f) v[i] += 0.9f * flash[EV_ARP] * (1.0f - d / 1.5f); }
  }
  for (int i = 0; i < LEDS_TIRA; i++) v[i] += 0.18f * capasPub * (0.5f + 0.5f * sinf(t * 0.003f + i * 0.6f));   // las capas: una ola lenta
  if (nuevo[EV_CAJA]) v[(int)(lrnd() * 9.99f)] += 0.6f;
  if (vozNivelPub > 0.01f) {                       // la voz: una onda que recorre la tira (al revés si la voz va al revés)
    ondaVoz += vozRevPub ? -0.35f : 0.35f; if (ondaVoz > 1000.0f || ondaVoz < -1000.0f) ondaVoz = 0.0f;
    for (int i = 0; i < LEDS_TIRA; i++) { float d = fabsf(fmodf(i - ondaVoz + 100.0f * LEDS_TIRA, (float)LEDS_TIRA) - LEDS_TIRA * 0.5f); v[i] += clamp01(vozNivelPub * 2.0f) * clamp01(1.0f - d / 2.5f); }
  }
  if (glitchPub) for (int i = 0; i < LEDS_TIRA; i++) v[i] = lrnd() < 0.5f ? 0.75f : 0.0f;
  if (huboSusto) {                                 // el golpe de entrada: estrobo y después la brasa que se apaga
    unsigned long dt = t - sustoMs;
    if (dt < 320) { bool on = ((dt / 40) & 1) == 0; for (int i = 0; i < LEDS_TIRA; i++) { v[i] = on ? 1.0f : 0.0f; w[i] = on ? 0.45f : 0.0f; } }
    else if (dt < 1600) { float e = 1.0f - (dt - 320) / 1280.0f; e *= e; for (int i = 0; i < LEDS_TIRA; i++) v[i] = fmaxf(v[i], e); }
  }
  for (int i = 0; i < LEDS_TIRA; i++) {
    float r = clamp01(v[i]), b = clamp01(w[i]);
    leds[LEDS_PLACA + i] = CRGB((uint8_t)(r * 255.0f), (uint8_t)(b * 110.0f), (uint8_t)(b * 80.0f));
  }
}

void renderPlaca(unsigned long t) {
  const float f[5] = { fmaxf(fmaxf(flash[I_TAIKO], flash[I_TOM]), fmaxf(flash[I_CAJA], flash[I_METAL])), flash[EV_PULSO], flash[EV_ARP],
                       fmaxf(capasPub, flash[EV_CAJA]), clamp01(vozNivelPub * 2.0f) };
  for (int i = 0; i < 5; i++) leds[i] = CRGB((uint8_t)(10 + 150 * clamp01(f[i])), 0, 0);
  if (suspensoPub) for (int i = 0; i < 3; i++) leds[i] = CRGB((uint8_t)(4 + 60 * flash[EV_LATIDO]), 0, 0);
  const CRGB cp = COLOR_PANEL[panelPub];
  const float pulso = tocandoPub ? 0.25f + 0.75f * pulsoNegra : 0.25f;
  leds[5] = CRGB((uint8_t)(cp.r * pulso), (uint8_t)(cp.g * pulso), (uint8_t)(cp.b * pulso));
  if (millis() - panelCambioMs < 350) for (int i = 0; i < 6; i++) leds[i] = cp;   // recién cambiado: todos del color del panel
  if (huboSusto && t - sustoMs < 320) { bool on = (((t - sustoMs) / 40) & 1) == 0; for (int i = 0; i < 6; i++) leds[i] = on ? CRGB(200, 30, 20) : CRGB(0, 0, 0); }
  if (millis() < cargaAltaHasta) leds[5] = CRGB(140, 0, 140);
}

void renderLEDs() {
  static unsigned long ultimo = 0;
  unsigned long t = millis();
  if (t - ultimo < LED_REFRESH_MS) return;
  ultimo = t;
  leerEventos();
  renderTira(t);
  renderPlaca(t);
  FastLED.show();
  for (int ev = 0; ev < NUM_EV; ev++) flash[ev] *= ev == EV_BRAAM ? 0.93f : 0.80f;
  pulsoNegra *= 0.80f;
}

// ==============================================================================================================================================
// CONTROLES (tarea de control, core 0, 1 kHz)
// ==============================================================================================================================================
bool bNivel[5] = { HIGH, HIGH, HIGH, HIGH, HIGH };
unsigned long bTiempo[5];
Ctl ctl;
int panel = P_GENERAL;
uint32_t rngCtl = 0x1234567u;
static inline float crnd() { rngCtl ^= rngCtl << 13; rngCtl ^= rngCtl >> 17; rngCtl ^= rngCtl << 5; return (rngCtl >> 8) * (1.0f / 16777216.0f); }

float readPot(uint8_t pin) {
  uint32_t s = 0;
  for (int i = 0; i < 4; i++) s += analogRead(pin);   // 4 y no 8: cada analogRead del S3 cuesta decenas de µs
  return (float)(s >> 2) / 4095.0f;
}
// Pick-up: al cambiar de panel cada pot queda "suelto" hasta que lo muevas un 4 %; desde ahí su
// posición es el valor. Así cambiar de panel nunca hace saltar nada.
float potEntrada[4];
bool  potTomado[4] = { true, true, true, true };
const float UMBRAL_TOMA = 0.04f;
void soltarPots() { for (int i = 0; i < 4; i++) { potEntrada[i] = readPot(POT_PIN[i]); potTomado[i] = false; } }
void publicarCtl() { memcpy((void *)&reqC, &ctl, sizeof(ctl)); }

int temaNuevo() {                                  // nunca el mismo; el instante exacto del botón es el azar
  rngCtl ^= (uint32_t)micros() * 2654435761u;
  return (ctl.tema + 1 + (int)(crnd() * (NUM_TEMAS - 1))) % NUM_TEMAS;
}

// Un botón = una función, en el flanco de presión. BTN4 pasa al siguiente panel en todos.
void accionBoton(int b) {
  if (b == 3) {
    panel = (panel + 1) % NUM_PANELES; panelPub = panel; panelCambioMs = millis(); soltarPots();
    LOG("panel %s\n", NOMBRE_PANEL[panel]);
    return;
  }
  switch (panel) {
    case P_GENERAL:
      if (b == 0) {
        ctl.tocar = !ctl.tocar;
        if (ctl.tocar) { ctl.tema = temaNuevo(); LOG("PLAY · tema %s\n", TEMAS[ctl.tema].nombre); } else LOG("STOP\n");
      } else if (b == 1) { ctl.tema = temaNuevo(); LOG("tema %s\n", TEMAS[ctl.tema].nombre); }
      else if (b == 2) reqSuspenso = reqSuspenso + 1;
      break;
    case P_BAJO:
      if (b == 0) ctl.bajoOn = !ctl.bajoOn;
      else if (b == 1) { ctl.patBajo = (ctl.patBajo + 1) % NUM_PAT_BAJO; LOG("bajo: patrón %d\n", ctl.patBajo + 1); }
      break;
    case P_CAPAS: {
      const int k = b == 4 ? C_CAJA : b;           // BTN1 cuerdas · BTN2 coro · BTN3 tono · BTN5 caja de música
      ctl.capa[k] = !ctl.capa[k];
      LOG("%s %s\n", NOMBRE_CAPA[k], ctl.capa[k] ? "entra" : "sale");
    } break;
    case P_VOCES:
      if (b == 0) reqVoz = reqVoz + 1;
      else if (b == 1) { ctl.cat = (ctl.cat + 1) % NUM_CAT; LOG("voces: %s (%d)\n", NOMBRE_CAT[ctl.cat], numMuestras[ctl.cat]); }
      else if (b == 2) { ctl.reversa = !ctl.reversa; LOG("voces %s\n", ctl.reversa ? "al revés" : "al derecho"); }
      break;
  }
  publicarCtl();
}

void pasoControl() {
  unsigned long ahora = millis();
  for (int b = 0; b < 5; b++) {
    bool nivel = digitalRead(BTN_PIN[b]);
    if (nivel == LOW && bNivel[b] == HIGH && (ahora - bTiempo[b]) > DEBOUNCE_MS) { bTiempo[b] = ahora; accionBoton(b); }
    bNivel[b] = nivel;
  }
  static uint8_t scan = 0;
  int i = scan; scan = (scan + 1) & 3;
  float v = readPot(POT_PIN[i]);
  if (!potTomado[i] && fabsf(v - potEntrada[i]) > UMBRAL_TOMA) potTomado[i] = true;
  if (potTomado[i]) pPot[panel][i] += (v - pPot[panel][i]) * 0.3f;
}

// ==============================================================================================================================================
// SETUP
// ==============================================================================================================================================
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear    = true;
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;         // ≈ 12 ms de cola: los botones suenan en el acto
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

#ifndef SIMULADOR
void audioTask(void *);
void controlTask(void *);
void ledTask(void *);
#endif

void iniciarLEDs() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRILLO);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_MAX_MA);
  FastLED.clear();
  FastLED.show();
}

void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // OBLIGATORIO con audio: un print por USB CDC bloquea hasta que el PC lea
#if ARDUINO_USB_CDC_ON_BOOT
  Serial0.begin(115200);
#endif
  for (int b = 0; b < 5; b++) pinMode(BTN_PIN[b], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  prepararSintesis();
  uint32_t semilla = (uint32_t)micros();
  for (int i = 0; i < 16; i++) semilla = semilla * 31u + (uint32_t)analogRead(POT_PIN[i & 3]);
  rngComp ^= semilla; rngCtl ^= semilla * 2246822519u;
  memset(&ctl, 0, sizeof(ctl));
  ctl.tema = (int)(semilla % NUM_TEMAS); ctl.bajoOn = true;
  publicarCtl();
  for (int p = 0; p < 4; p++) pPot[P_GENERAL][p] = readPot(POT_PIN[p]);   // el General ya en su lugar al arrancar
  tensS = pPot[P_GENERAL][2]; lpfS = pPot[P_GENERAL][3];

  // La memoria del glitch: 0.74 s en PSRAM; sin PSRAM, 0.19 s en RAM interna (los glitches largos se acortan)
  histN = 32768;
  histL = (int16_t *)heap_caps_malloc(histN * 2, MALLOC_CAP_SPIRAM); histR = (int16_t *)heap_caps_malloc(histN * 2, MALLOC_CAP_SPIRAM);
  if (!histL || !histR) {
    histN = 8192;
    histL = (int16_t *)heap_caps_malloc(histN * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); histR = (int16_t *)heap_caps_malloc(histN * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!histL || !histR) histN = 0;
  }
  if (histN) { memset(histL, 0, histN * 2); memset(histR, 0, histN * 2); }

#ifdef SIMULADOR
  iniciarLEDs();
#endif
  i2s_init();
#if MEDIR
  ctl.tocar = true; publicarCtl();
#endif

#ifndef SIMULADOR
  // El audio tiene el core 1 para él solo; controles y LEDs van al core 0.
  xTaskCreatePinnedToCore(audioTask,   "audio",   8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask, "control", 6144, NULL,  3, NULL, 0);
  xTaskCreatePinnedToCore(ledTask,     "leds",    4096, NULL,  1, NULL, 0);
#endif
}

// ==============================================================================================================================================
// TAREAS
// ==============================================================================================================================================
#ifdef SIMULADOR
void loop() {
  pasoControl(); calcularBloque();
  size_t w; i2s_channel_write(tx_chan, salida, sizeof(salida), &w, portMAX_DELAY);
  renderLEDs();
}
#else
// Presupuesto de un bloque: 128 muestras a 240 MHz = 696 000 ciclos. Si un bloque usa más del 80 %,
// el LED 5 se pone magenta medio segundo: así se distingue un problema de TIEMPO de uno de señal.
const uint32_t CICLOS_BLOQUE = (uint32_t)(240000000.0 * BUFFER_SAMPLES / SAMPLE_RATE);
void audioTask(void *) {
  for (;;) {
    uint32_t c0 = ESP.getCycleCount();
    calcularBloque();
    uint32_t ciclos = ESP.getCycleCount() - c0;
    if (ciclos > (CICLOS_BLOQUE / 10) * 8) cargaAltaHasta = millis() + 500;
    if (ciclos > medMax) medMax = ciclos;
    medSuma = medSuma + ciclos / 64; medN = medN + 1;
    size_t w;
    i2s_channel_write(tx_chan, salida, sizeof(salida), &w, portMAX_DELAY);
  }
}
void controlTask(void *) {
  LOG("TERROR SIN SD · %d voces en la flash\n", numMuestras[0] + numMuestras[1] + numMuestras[2] + numMuestras[3] + numMuestras[4]);
  for (;;) {
    pasoControl();
#if MEDIR
    static unsigned long tMed = 0;
    if (millis() - tMed >= 2000) {
      tMed = millis();
      uint32_t n = medN ? medN : 1;
      LOG("CPU audio: prom %.0f %%  max %.0f %% (síntesis %.0f · master %.0f) | pico antes del limitador %.2f  ganancia min %.2f  muestras en techo %lu | tensión %.2f\n",
        100.0 * (double)medSuma * 64.0 / n / CICLOS_BLOQUE, 100.0 * (double)medMax / CICLOS_BLOQUE,
        100.0 * 64 * medSec[0] / n / CICLOS_BLOQUE, 100.0 * 64 * medSec[1] / n / CICLOS_BLOQUE,
        (double)medPico, (double)medLimMin, (unsigned long)medTecho, (double)tensionPub);
      medSec[0] = medSec[1] = 0; medMax = 0; medSuma = 0; medN = 0; medTecho = 0; medLimMin = 1.0f; medPico = 0.0f;
    }
#endif
    vTaskDelay(1);
  }
}
// Los LEDs se inicializan DENTRO de su tarea, en el core 0: las interrupciones del RMT corren en el
// núcleo que lo inicializó (hecho en setup(), que corre en el core 1, le robaba tiempo al audio).
void ledTask(void *) { iniciarLEDs(); for (;;) { renderLEDs(); vTaskDelay(2); } }
void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }
#endif
