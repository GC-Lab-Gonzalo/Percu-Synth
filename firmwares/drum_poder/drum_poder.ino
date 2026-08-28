// ==============================================================================================================================================
// PERCU-SYNTH — DRUM PODER: drum machine de sonidos con PESO + líneas de bajo por MIDI DIN-5 — GC Lab Chile
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
// - Salida MIDI DIN-5 (UART, 31250 baud) |TX -> 43|  <- las líneas de bajo salen por acá
// - 6 LEDs WS2812 SMD internos de la placa |DATA -> 46| (sólo indicadores, no controlan nada)
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - Flash Mode         : DIO         (¡OPI rompe I2S!)
// - PSRAM              : OPI PSRAM
// - Partition Scheme   : Default 4MB with spiffs
// - CPU Frequency      : 240 MHz
//
// OJO CON EL SERIAL: este firmware NO IMPRIME NADA, por dos razones. La primera es que en esta
//   placa el UART0 sale por los GPIO 43/44 y el 43 es justo el TX del DIN-5, asi que cualquier
//   `Serial0.print` saldria por el cable MIDI como basura (y el RX, el 44, es el BTN1). La
//   segunda es que un print por USB CDC bloquea al nucleo que lo hace hasta que el host lea, y
//   mientras tanto el DMA del DAC se queda sin datos: eso se oye como un chasquido.
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core >= 3.x (incluye driver/i2s_std.h)
// - FastLED (gestor de librerías Arduino) — para los 6 LEDs SMD de la placa
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Drum machine 100 % sintetizada (sin samples). ES PERCUSIÓN Y NADA MÁS: no toca una sola nota,
// ni por el parlante ni por el MIDI. Por el DIN-5 sale el RELOJ, y la melodía la pone el sinte
// que lo recibe.
//
// DOS REGLAS QUE MANDAN SOBRE TODO EL RESTO DEL ARCHIVO:
//
//   1) LA PERCUSIÓN NO HACE MELODÍA. NINGUNA. El SUB suena la TÓNICA del groove y no se mueve
//      nunca; el bombo, los toms y el cuerpo de la caja son percusión afinada con SU altura
//      fija; y todo lo que vive arriba de 1 kHz (hats, metal, clap, FX) es RUIDO FILTRADO: no
//      tiene nota, así que no puede hacer melodía. Verificado: ninguna voz cambia de altura.
//
//   2) ARRIBA DE LOS GRAVES TODO ES CORTO. El presupuesto de decay va por banda:
//        abajo de 100 Hz .... hasta 0.70 s de tau  (sub y bombo: acá vive el peso)
//        100–250 Hz ......... hasta 0.22 s         (toms)
//        250 Hz–1 kHz ....... hasta 0.10 s         (cuerpo de la caja)
//        arriba de 1 kHz .... hasta 0.14 s, y sólo el plato llega a 0.50 s
//      Medido a -40 dB del pico: sub 2.1 s · bombo 1.3 s · tom 0.7 s · caja 0.5 s · clap 0.25 s ·
//      hat 0.13 s · tick 0.07 s. El peso abajo y la definición arriba salen de ahí.
//
// LAS 8 PISTAS, cada una en su banda (esto es "estar bien mezclado": nadie pisa a nadie):
//   SUB      33–45 Hz de fundamental + 2o y 3er armonico  -> el peso. Los armonicos NO son
//            decoracion: en un parlante chico la fundamental no existe y lo que se oye son
//            ellos (el oido reconstruye la que falta). El 2o esta en -9.4 dB y el 3o en
//            -17.7 dB; con el 2o en -16 y sin el 3o el sub "estaba" pero no se sentia.
//   BOMBO    39–54 Hz de fundamental + PECHO en 80–110 Hz + mazo en 0.9–1.9 kHz -> el ancla
//   TOMS     88–225 Hz, pasa-altos en 70 Hz            -> el medio-grave
//   CAJA     175–345 Hz de cuerpo + 1.9–3 kHz de ruido -> pasa-altos en 150 Hz
//   CLAP     0.9–3.5 kHz de ruido                      -> el snap electrónico
//   FX       barridos de ruido 1.3–7 kHz                  -> transiciones, no groove
//   HATS     6–8 kHz de ruido pasa-BANDA               -> aire, nunca "shhh"
//   METAL    3.8–5.4 kHz de ruido pasa-BANDA           -> chasquido corto y plato
//
// Los ritmos NO son aleatorios: son 12 GROOVES escritos a mano con compases impares de verdad
// (7/8, 5/8+7/8, 9/8, 13/8, 5/4, 12/8), con el aire como parte del groove.
//
// POR EL MIDI SALE EL RELOJ, NO NOTAS. Esto tuvo tres versiones con bajo adentro (una línea
// escrita por groove, después una tabla de ritmos de bajo sobre una grilla, después patrones
// derivados del bombo) y las tres se descartaron. La conclusión es la buena: **el bajo lo pone
// el sinte**. Elegir allá el sonido y la secuencia es infinitamente más expresivo que cualquier
// tabla metida acá adentro, y el único trabajo real de esta máquina es dar un pulso que no se
// mueva. Así que el DIN-5 manda **MIDI Clock a 24 PPQN** y **Start**, y con eso el secuenciador,
// el arpegiador o el delay del sinte corren enganchados a la batería.
//
// Dos cosas que hacen que ese pulso sirva:
//   · El reloj se CUENTA con el contador de muestras del audio (así no puede irse de fase con la
//     batería: las dos cosas salen del mismo contador) pero se MANDA desde la tarea de control,
//     a 1 kHz. Mandarlo desde el render metía la fluctuación del buffer — ±2.9 ms, que a 120 BPM
//     es un 14 % del tick y se oye como un arpegio tembloroso.
//   · Se manda STOP+START cada vez que se reubica el "1" (al cambiar de groove y en el primer
//     tap de una serie de tap tempo). Sin eso el sinte sigue en el paso donde estaba y su
//     secuencia queda corrida contra la batería para siempre.
// Medido en los 12 grooves: el tempo del reloj cae dentro del 0.04 % del BPM del groove.
//
// Y LA MASTERIZACIÓN (cadena fija, en este orden, y el orden es la mitad del asunto):
//   sidechain del bombo -> sala corta (send fijo por pista, 0 en el bombo y el sub) ->
//   saturación de CUERPO en dos bandas (POT3) -> FILTRO pasa-bajos resonante (POT2) ->
//   compresor de bus (1:2.8 fijo) -> pasa-altos de 30 Hz -> LIMITADOR con lookahead (techo 0.92)
//   -> pasa-bajos de 13 kHz -> volumen (POT1) -> techo final con rodilla
//
//   · Toda la distorsión va ANTES del filtro: una no linealidad después reinyecta agudos que el
//     filtro ya no puede sacar.
//   · El pasa-bajos de 13 kHz va DESPUÉS del limitador: un limitador multiplica por una ganancia
//     que se mueve, y multiplicar es modular, o sea que genera bandas laterales.
//   · El limitador mira 64 muestras hacia adelante. Sin lookahead siempre llega tarde al golpe y
//     el que termina agarrando el pico es el clipper del final: distorsión en cada golpe fuerte.
// FUNCIONAMIENTO (botones — un boton, una funcion, en el flanco de presion. Sin combos.)
// ==============================================================================================================================================
// - BTN1 (44) -> GROOVE: pasa al siguiente de los 12 ritmos. Cambia el compas y el tempo
//               sugerido, reubica el "1" al instante y manda Stop+Start por MIDI para que el
//               sinte externo arranque su secuencia en ese mismo "1".
// - BTN2 (42) -> KIT: cicla los 5 timbres. Cambia la sintesis al instante; el groove sigue.
// - BTN3 (0)  -> TAP TEMPO: marca el pulso (negras). Con 2 toques fija el BPM (50-220). El
//               primer toque despues de 2 s de silencio reubica el "1" (y realinea el sinte).
// - BTN4 (45) -> MEDIO TIEMPO (mantener): cada paso del patron dura el doble. El reloj maestro
//               sigue corriendo por debajo -> al soltar retoma sin desfase.
// - BTN5 (47) -> REDOBLE (mantener): fill de toms y caja que acelera; al soltar, platillo. El
//               bombo y el sub siguen debajo para no perder el piso.
// ==============================================================================================================================================
// FUNCIONAMIENTO (potenciometros — cuatro, sin paneles: la posicion de la perilla ES el valor)
// ==============================================================================================================================================
// - POT1 (ADC1)  -> VOLUMEN master. A cero es el mute: la maquina siempre corre.
// - POT2 (ADC2)  -> FILTRO: corte del pasa-bajos global, 200 Hz -> abierto. Es el control con
//                  el que se oscurece todo si algo suena al filo. La resonancia solo aparece
//                  cuando el corte ya bajo de 2.5 kHz (arriba de eso el filtro es
//                  transparente): un pico resonante en los agudos es exactamente lo que
//                  molesta. Cada golpe abre un poco el corte, asi el filtro respira.
// - POT3 (ADC8)  -> CUERPO: saturacion en dos bandas — los graves y medios engordan (hasta
//                  x3.6) y los agudos casi no se tocan (x1.2). Suma peso, no suciedad.
// - POT4 (ADC10) -> BEAT REPEAT: OFF · x2 (loop de 8 pasos) · x4 (4) · x8 (2) · x16 (1),
//                  sobre un reloj maestro que nunca se detiene -> al volver a OFF retoma en
//                  tiempo, nunca desfasado.
// ==============================================================================================================================================
// FUNCIONAMIENTO (LEDs — 6 SMD de la placa, solo indicadores)
// ==============================================================================================================================================
// - Color base = KIT activo (cian · violeta · ambar · verde · rojo).
// - LED0 bombo · LED1 caja/clap · LED2 hats/ride · LED3 toms · LED4 FX/sub · LED5 pulso.
// - Al cambiar de groove o de kit, N LEDs marcan el numero elegido.
// - BEAT REPEAT activo -> N LEDs rojos (1 = x2 ... 4 = x16).
// - MEDIO TIEMPO -> LED5 amarillo fijo · REDOBLE -> estrobo blanco.
// - No hay Serial: los LEDs son TODA la informacion de estado que da la maquina.
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <math.h>

// --- I2S PCM5102 -------------------------------------------
#define I2S_LCK      39
#define I2S_DIN      40
#define I2S_BCK      41
#define SAMPLE_RATE  44100
#define BUF_SAMPLES  128          // ~2.9 ms · 4 descriptores DMA ~12 ms de cola

// --- MIDI DIN-5 --------------------------------------------
#define MIDI_TX_PIN   43          // TX del DIN-5 (ver nota del encabezado sobre Serial0)
#define MIDI_BAUD     31250
#define MIDI_CH_BAJO  1           // canal 1..16
#define MIDI_RX_PIN   -1          // sin RX: su pin natural (44) es el BTN1

// El MIDI sale por Serial1, NO por Serial0: Serial0 comparte el TX (43) con el DIN-5 y su RX
// es el 44 (BTN1). Serial1 se enruta al 43 por la matriz de GPIO y deja el 44 en paz.
HardwareSerial &MIDIOUT = Serial1;

const bool ENVIAR_MIDI_CLOCK = true;   // MIDI Clock 24 PPQN + Start: el arp/delay del
                                       // sinte externo engancha solo. Ponlo en false si
                                       // tu sinte se vuelve loco con clock entrante.
// NO HAY SERIAL DE DIAGNOSTICO, y es a proposito. Un `Serial.print` por USB CDC escribe en una
// cola que el host tiene que vaciar; mientras tanto el nucleo que imprime no rellena el DMA del
// DAC. En un firmware que ya va justo de tiempo eso es un chasquido. Toda la informacion de
// estado sale por los 6 LEDs (N LEDs = el numero elegido).

// --- Pines -------------------------------------------------
const uint8_t BTN_PIN[5] = {44, 42, 0, 45, 47};   // BTN1..BTN5 (orden correcto de la placa)
// Los 4 pots, en el orden de la placa. Su significado depende del PANEL (ver más abajo):
// panel A = los mismos cuatro controles de drum_ruido · panel B = las notas MIDI.
#define POT1        1
#define POT2        2
#define POT3        8
#define POT4        10
const uint8_t POT_PIN[4] = { POT1, POT2, POT3, POT4 };

#define LED_PIN     46
#define NUM_LEDS    6
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define LED_BRIGHT  70

// --- Secuenciador ------------------------------------------
#define MAX_STEPS   64            // alcanza para 7/8x4, 13/8x2, 12/8x2, 4/4x4...
#define NUM_TRACKS  8
#define NUM_LINES   8             // una cadena de texto por pista

#define T_KICK   0                // bombo: cuerpo grave largo + mazo cortísimo
#define T_SNARE  1                // caja: cuerpo afinado corto + bordonera
#define T_TOM    2                // toms: 3 parches, medio-grave
#define T_HAT    3                // hats: RUIDO pasa-banda, cortísimo
#define T_METAL  4                // metal: RUIDO pasa-banda (chasquido / plato). CERO tono
#define T_CLAP   5                // clap: RUIDO en la banda del snap
#define T_FX     6                // FX: barridos de ruido (caída y riser). Transiciones, no groove
#define T_SUB    7                // sub: la única voz afinada, y va abajo de 60 Hz

// 24 voces. Con todo lo de arriba de los graves corto, la ocupación bajó mucho: lo que agota la
// polifonía son las colas largas, no la cantidad de pistas. Y una voz agotada significa ROBO DE
// VOZ, o sea un golpe cortado en seco a mitad de camino: un clic. El simulador cuenta los robos
// y tienen que ser CERO tocando normal. Eran 14 y alcanzaban justo tocando el patron pelado;
// con el redoble automatico, el plato y el beat repeat encima el margen se acababa, asi que hay
// 10 de colchon: una voz inactiva no cuesta nada (se saltea con un `if`), y el robo de voz es
// lo unico que vuelve a meter un corte en seco en la mezcla.
#define MAX_VOICES 24
#define MAX_PEND    8             // golpes agendados (rolls, flams, doble bombo)

#define NUM_GROOVES 12
#define NUM_KITS     5
#define NUM_BASSMODE 5

const bool FILL_AUTO = true;      // redoble automático en el último compás de cada 4a
                                  // repetición de la frase. En false, la frase es literal.

// ==============================================================================================
// TIPOS — van ARRIBA DEL TODO, antes de la primera función.
// El IDE de Arduino inserta los prototipos automáticos JUSTO ANTES de la primera definición de
// función del archivo: cualquier struct declarado más abajo queda por debajo de los prototipos
// que lo usan y el compilador tira que Biq no nombra un tipo. (Misma nota que en drum_ruido.)
// ==============================================================================================

// Biquad RBJ (Direct Form I)
struct Biq  { float b0, b1, b2, a1, a2; };
struct BiqZ { float x1, x2, y1, y2; };

// Un KIT completo: redefine la síntesis de las 8 pistas.
//
// DOS REGLAS QUE MANDAN SOBRE TODO LO DEMÁS EN ESTA TABLA:
//
// 1) ARRIBA DE LOS GRAVES NADA ES MELÓDICO. Las únicas voces con altura definida son el SUB
//    (abajo de 60 Hz), el bombo, los toms y el cuerpo de la caja — o sea instrumentos de
//    percusión afinados, con SU altura fija, que no siguen la armonía. Todo lo que vive arriba
//    de 1 kHz (hats, metal, clap, tick) es RUIDO FILTRADO: no tiene nota, no puede hacer
//    melodía. La melodía es cosa del MIDI, no de la batería.
// 2) ARRIBA DE LOS GRAVES TODO ES CORTO. El presupuesto de decay va por banda:
//       abajo de 100 Hz .... hasta 0.70 s   (sub y bombo: acá vive el peso)
//       100–250 Hz ......... hasta 0.22 s   (toms)
//       250 Hz–1 kHz ....... hasta 0.10 s   (cuerpo de la caja)
//       arriba de 1 kHz .... hasta 0.14 s, y sólo el plato llega a 0.50 s
//    Son taus de exponencial, así que el golpe se oye ~3·tau. Una cola larga arriba es lo que
//    convierte una batería en una nube.
struct Kit {
  const char *name;
  uint8_t hue;
  // BOMBO — TRES BANDAS, que es lo que hace que un bombo sintetizado pese:
  //   kFreq   : dónde ATERRIZA la fundamental (39–54 Hz). Es el peso.
  //   kPunch  : nivel del 2º armónico, o sea la banda de 80–110 Hz. Es la PEGADA EN EL PECHO,
  //             y es la que hace que el bombo se sienta grande en un parlante que no da 40 Hz.
  //             Sin esta banda un bombo sintetizado suena a "boom" flojo y lejano.
  //   kKnock  : el mazo — una banda de RUIDO de 14 ms centrada en kKnockF. Es la DEFINICIÓN
  //             (dónde cae el golpe). Con un seno en vez de ruido esto es un pitido pegado al
  //             bombo, que es el error que tuvo este firmware durante tres versiones.
  //   kClick  : el click del parche, 5 ms de ruido pasa-altos.
  //   kRatio / kDropMs : desde cuánto más arriba arranca la fundamental y en cuánto cae.
  //   kSat    : saturación propia. Acá SÍ va, porque el bombo tiene una sola parcial fuerte:
  //             le genera armónicos y es otra parte de por qué se oye en chico.
  float kFreq, kRatio, kDropMs, kDec, kPunch, kKnock, kKnockF, kClick, kSat;
  // CAJA: 2 parciales del cuerpo, decay del cuerpo, bordonera (fc, Q, decay), mezcla
  //       cuerpo<->ruido, nivel del crack
  float sF1, sF2, sDec, sFc, sQ, sNDec, sMix, sCrack;
  // TOMS: frecuencias grave/medio/agudo, caída de pitch, decay, ruido de parche
  float tF1, tF2, tF3, tDrop, tDec, tNoise;
  // HATS: centro del pasa-banda, Q, decay cerrado / abierto, nivel
  float hFc, hQ, hDecC, hDecO, hAmt;
  // METAL: centro del pasa-banda, Q, decay del chasquido corto / del plato largo, nivel.
  //        Es ruido y NADA MÁS que ruido: cero parciales, cero altura, cero melodía.
  float mFc, mQ, mDecCorto, mDecLargo, mAmt;
  // CLAP: pasa-banda (fc, Q), decay de cada palmada, decay de la cola
  float cFc, cQ, cDec, cTail;
  // FX: barridos de ruido para las transiciones. NO es un instrumento de groove — es un efecto,
  //     y va en los puntos de giro de la frase. fxFcIni -> fxFcFin es el barrido del pasa-banda
  //     (la caída va de agudo a medio; el riser al revés, con swell). Q BAJO a propósito: un
  //     pasa-banda de ruido con Q alto barriendo se oye como un silbido de pájaro, no como aire.
  float fxFcIni, fxFcFin, fxQ, fxDec, fxAmt;
  // SUB: multiplicador de ataque de pitch, decay, saturación, ruido de impacto (ms)
  float bFreqMul, bDec, bSat, bNoiseMs;
  // AMBIENTE: cuánta sala y qué tan larga. Corta a propósito: esto es percusión, no un pad.
  float revSend, revFb;
};

// Un GROOVE: el ritmo escrito a mano, con su compás, su tempo y su tónica.
// Las 8 líneas son cadenas de texto, una por pista, en la gramática de más abajo.
struct Groove {
  const char *name;
  const char *compas;
  uint8_t  stepsPerBar;      // semicorcheas por compás (4/4=16 · 7/8=14 · 13/8=26...)
  uint8_t  bars;
  uint16_t bpm;
  uint8_t  rootMidi;         // tónica: la altura del SUB (nota MIDI). Nada más la usa
  uint8_t  swing;            // % de swing en las semicorcheas impares (0 = recto)
  const char *ln[NUM_LINES]; // BOMBO CAJA TOMS HATS METAL CLAP FX SUB
};

// Voz de un golpe: DOS capas con envolventes independientes (tono y ruido) + el
// pasa-altos de banda de la pista (la mezcla), que sí actúa sobre la voz completa.
struct Voice {
  bool    active;
  uint8_t type;
  float   amp;
  float   atk, atkInc;
  // Capa TONAL
  float   env, envCoef;
  float   ph, f, fEnd, fCoef;
  float   ph2, ratio2, amp2;
  float   ph3, ratio3, amp3;
  uint8_t wave;
  float   sat;
  // Capa de RUIDO (su propia envolvente: la pegada)
  float   nEnv, nCoef, nAmt;
  // Capa TRANSITORIA: la TERCERA envolvente. Es el mazo del bombo (un tono medio corto),
  // el crack de 4 kHz de la caja y la cola del clap. Con dos capas no alcanza: el mazo
  // tiene que morir en 20 ms mientras el cuerpo grave sigue 300 ms, y eso pide su propia
  // envolvente. t3IsNoise decide si esta capa es ruido (pasa por bq2) o un seno.
  float   t3Env, t3Coef, t3Amt, t3Ph, t3F;
  bool    t3IsNoise;
  bool    useF1, useF2;
  // Barrido del pasa-banda del ruido (sólo lo usa el FX): se recalcula una vez por buffer
  float   swFc, swFcFin, swQ, swT, swInc;
  Biq     bq1; BiqZ bz1;      // pasa-altos / pasa-banda del ruido
  Biq     bq2; BiqZ bz2;      // resonancia metálica encima
  // Banda de la pista (pasa-altos sobre la voz entera = el hueco del bombo se respeta)
  bool    useLane;
  Biq     lane; BiqZ laneZ;
  float   send;               // envío a la reverb
  // Estéreo
  float   lg, rg;
  // Ranura MONOFÓNICA que ocupa esta voz (-1 = pista polifónica). Una batería de verdad
  // tiene UN parche por instrumento: el bombo no puede sonar dos veces encima de sí mismo,
  // y un hi-hat cerrado CORTA al abierto (el choke). Además de ser lo real, es lo que
  // impide que un doble pedal a 168 BPM apile catorce bombos y se coma la polifonía.
  int8_t  slot;
  // Se esta muriendo? Lo ponen fastKill() y el apagado final. Importa por dos razones:
  //  · una voz en ATAQUE ignoraba el fastKill (el decaimiento vive en la rama `else` del
  //    ataque), asi que el choke no la tocaba hasta que el ataque terminara;
  //  · y el apagado final ya no corta en seco: acelera la envolvente a 1.5 ms y recien
  //    entonces libera la voz (ver el apagado en el bucle de render).
  bool    dying;
};

// Golpe agendado en el futuro (rolls de hat, flam de caja, doble bombo)
struct Pend { int32_t left; uint8_t track; uint8_t vel; uint8_t aux; };

struct Btn { uint8_t pin; bool last; uint32_t tDown; };

// ==============================================================================================
// Utilidades DSP
// ==============================================================================================

// Ruido de audio (LCG propio: no consume el generador de la composición)
static uint32_t nrng = 0x9E3779B9u;
inline float noiseF() {
  nrng = nrng * 1664525u + 1013904223u;
  return (float)(int32_t)nrng * (1.0f / 2147483648.0f);
}

// Generador de humanización (velocidades, panorama del perc) — separado del ruido de audio
static uint32_t prng = 0x12345677u;
inline uint32_t rnd32() { prng = prng * 1664525u + 1013904223u; return prng; }
inline float    frnd()  { return (float)(rnd32() >> 8) * (1.0f / 16777216.0f); }   // 0..1

// Seno por tabla (257 entradas -> interpolación lineal sin caso especial en el borde)
static float sineLUT[257];
inline float sineAt(float ph) {
  while (ph >= 1.0f) ph -= 1.0f;
  while (ph <  0.0f) ph += 1.0f;
  float x = ph * 256.0f;
  int   i = (int)x;
  float f = x - (float)i;
  return sineLUT[i] + (sineLUT[i + 1] - sineLUT[i]) * f;
}

// Semitonos -> razón de frecuencia (-24 ... +36)
#define SEMI_MIN  -24
#define SEMI_N     61
static float semiLUT[SEMI_N];
inline float semiRatio(int s) {
  if (s < SEMI_MIN) s = SEMI_MIN;
  if (s > SEMI_MIN + SEMI_N - 1) s = SEMI_MIN + SEMI_N - 1;
  return semiLUT[s - SEMI_MIN];
}

// PolyBLEP: redondea el salto de sierra/cuadrada. Sin esto, una cuadrada a 3 kHz
// devuelve armónicos por encima de Nyquist que se pliegan hacia abajo como tonos
// inarmónicos — el chillido que no pertenece a ninguna nota.
inline float polyBlep(float t, float dt) {
  if (t < dt)             { t /= dt;               return t + t - t * t - 1.0f; }
  else if (t > 1.0f - dt) { t = (t - 1.0f) / dt;   return t * t + t + t + 1.0f; }
  return 0.0f;
}

inline float oscWave(uint8_t wave, float ph, float dt) {
  if (wave == 0) return sineAt(ph);
  if (wave == 1) return (2.0f * ph - 1.0f) - polyBlep(ph, dt);
  float p2 = ph + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
  float a = (2.0f * ph - 1.0f) - polyBlep(ph, dt);
  float b = (2.0f * p2 - 1.0f) - polyBlep(p2, dt);
  return (a - b) * 0.5f;
}

inline float biqProc(const Biq &c, BiqZ &z, float x) {
  float y = c.b0 * x + c.b1 * z.x1 + c.b2 * z.x2 - c.a1 * z.y1 - c.a2 * z.y2;
  z.x2 = z.x1; z.x1 = x;
  z.y2 = z.y1; z.y1 = y;
  return y;
}

inline void biqClear(BiqZ &z) { z.x1 = z.x2 = z.y1 = z.y2 = 0.0f; }

void makeLPF(Biq &c, float fc, float Q) {
  if (fc < 40.0f)    fc = 40.0f;
  if (fc > 19000.0f) fc = 19000.0f;
  if (Q  < 0.4f)     Q  = 0.4f;
  float w = 2.0f * (float)M_PI * fc / SAMPLE_RATE;
  float s = sinf(w), co = cosf(w), al = s / (2.0f * Q);
  float a0 = 1.0f + al;
  c.b0 = ((1.0f - co) * 0.5f) / a0;
  c.b1 = (1.0f - co) / a0;
  c.b2 = c.b0;
  c.a1 = (-2.0f * co) / a0;
  c.a2 = (1.0f - al) / a0;
}

void makeBPF(Biq &c, float fc, float Q) {
  if (fc < 60.0f)    fc = 60.0f;
  if (fc > 17000.0f) fc = 17000.0f;
  if (Q  < 0.4f)     Q  = 0.4f;
  float w = 2.0f * (float)M_PI * fc / SAMPLE_RATE;
  float s = sinf(w), co = cosf(w), al = s / (2.0f * Q);
  float a0 = 1.0f + al;
  c.b0 =  al / a0;
  c.b1 =  0.0f;
  c.b2 = -al / a0;
  c.a1 = (-2.0f * co) / a0;
  c.a2 = (1.0f - al) / a0;
}

void makeHPF(Biq &c, float fc, float Q) {
  if (fc < 20.0f)    fc = 20.0f;
  if (fc > 17000.0f) fc = 17000.0f;
  if (Q  < 0.4f)     Q  = 0.4f;
  float w = 2.0f * (float)M_PI * fc / SAMPLE_RATE;
  float s = sinf(w), co = cosf(w), al = s / (2.0f * Q);
  float a0 = 1.0f + al;
  c.b0 =  ((1.0f + co) * 0.5f) / a0;
  c.b1 = -(1.0f + co) / a0;
  c.b2 =  c.b0;
  c.a1 = (-2.0f * co) / a0;
  c.a2 = (1.0f - al) / a0;
}

// Shelvings RBJ para el EQ tilt del master. Un tilt (grave abajo / agudo arriba, o al
// revés) siempre suena musical; por eso el POT2 es un tilt y no un filtro con resonancia.
void makeLowShelf(Biq &c, float fc, float dB, float S) {
  float A  = powf(10.0f, dB / 40.0f);
  float w  = 2.0f * (float)M_PI * fc / SAMPLE_RATE;
  float co = cosf(w), s = sinf(w);
  float al = s * 0.5f * sqrtf((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);
  float sq = 2.0f * sqrtf(A) * al;
  float a0 = (A + 1.0f) + (A - 1.0f) * co + sq;
  c.b0 = (A * ((A + 1.0f) - (A - 1.0f) * co + sq)) / a0;
  c.b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * co)) / a0;
  c.b2 = (A * ((A + 1.0f) - (A - 1.0f) * co - sq)) / a0;
  c.a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * co)) / a0;
  c.a2 = ((A + 1.0f) + (A - 1.0f) * co - sq) / a0;
}

void makeHighShelf(Biq &c, float fc, float dB, float S) {
  float A  = powf(10.0f, dB / 40.0f);
  float w  = 2.0f * (float)M_PI * fc / SAMPLE_RATE;
  float co = cosf(w), s = sinf(w);
  float al = s * 0.5f * sqrtf((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);
  float sq = 2.0f * sqrtf(A) * al;
  float a0 = (A + 1.0f) - (A - 1.0f) * co + sq;
  c.b0 = (A * ((A + 1.0f) + (A - 1.0f) * co + sq)) / a0;
  c.b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * co)) / a0;
  c.b2 = (A * ((A + 1.0f) + (A - 1.0f) * co - sq)) / a0;
  c.a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * co)) / a0;
  c.a2 = ((A + 1.0f) - (A - 1.0f) * co - sq) / a0;
}

// Techo final del master: LINEAL hasta 0.95 y con rodilla suave sólo arriba de eso. Importa que
// sea lineal en la zona normal: un softClip como el de abajo distorsiona un poco en TODO su
// rango, y esos armónicos nacen DESPUÉS del pasa-bajos de 13 kHz, así que ya no hay nada que los
// filtre — es basura audible como crujido en los transitorios. Medido a 15.5 kHz: con softClip
// quedaba a -50 dB del grave; con esta rodilla queda 15 dB más abajo.
inline float techo(float x) {
  const float K = 0.95f;
  if (x >  K) { float t = (x - K) / (1.0f - K); if (t > 1.0f) t = 1.0f; return K + (1.0f - K) * (t - 0.5f * t * t); }
  if (x < -K) { float t = (-x - K) / (1.0f - K); if (t > 1.0f) t = 1.0f; return -(K + (1.0f - K) * (t - 0.5f * t * t)); }
  return x;
}

// Saturación suave (aproximación de tanh, sin llamar a tanhf en el bucle de audio)
inline float softClip(float x) {
  if (x >  3.0f) return  1.0f;
  if (x < -3.0f) return -1.0f;
  float x2 = x * x;
  return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// tau de una exponencial -> coeficiente por muestra. El golpe se oye ~3·tau.
inline float decCoef(float seconds) {
  if (seconds < 0.003f) seconds = 0.003f;
  return expf(-1.0f / (seconds * (float)SAMPLE_RATE));
}

// ==============================================================================================
// KITS — los 5 timbres. Todo lo que define el CARÁCTER de un sonido vive acá; lo que define
// su LUGAR EN LA MEZCLA (banda, ganancia, reverb) está en las tablas de abajo y es fijo.
// ==============================================================================================
// Sobre los DECAYS: son la tau de una exponencial, así que el golpe suena ~3·tau. El bombo
// va entre 0.14 y 0.34 (0.4–1.0 s reales); más que eso deja de ser un golpe y se vuelve un
// zumbido grave que tapa el resto. Los hats abiertos no pasan de 0.19.
const Kit KITS[NUM_KITS] = {
  // ---- RITUAL (cian) — el golpe grande: bombo profundo con mucho pecho, caja gorda y afinada,
  //      hats de aire, sala corta. El más "Sleep Token" de los cinco.
  { "RITUAL", HUE_AQUA,
    42.0f, 5.6f, 26.0f, 0.30f, 0.45f, 0.35f, 1300.0f, 0.14f, 1.55f,
    190.0f, 295.0f, 0.085f, 2200.0f, 0.90f, 0.115f, 0.58f, 0.30f,
    105.0f, 150.0f, 205.0f, 0.10f, 0.16f, 0.10f,
    6800.0f, 0.50f, 0.028f, 0.110f, 0.80f,
    4000.0f, 0.50f, 0.10f, 0.40f, 1.00f,
    1500.0f, 1.1f, 0.009f, 0.055f,
    6500.0f, 1400.0f, 0.65f, 0.20f, 0.85f,
    1.40f, 0.45f, 1.30f, 5.0f,
    0.22f, 0.60f },

  // ---- MONOLITO (violeta) — seco y orgánico, con los toms adelante. Casi sin sala.
  { "MONOLITO", HUE_PURPLE,
    48.0f, 5.8f, 22.0f, 0.22f, 0.38f, 0.50f, 1600.0f, 0.22f, 1.40f,
    210.0f, 320.0f, 0.070f, 2700.0f, 1.20f, 0.095f, 0.52f, 0.42f,
    96.0f, 132.0f, 186.0f, 0.12f, 0.20f, 0.08f,
    7200.0f, 0.55f, 0.026f, 0.095f, 0.72f,
    4400.0f, 0.55f, 0.09f, 0.30f, 0.95f,
    1400.0f, 1.4f, 0.008f, 0.040f,
    6000.0f, 1500.0f, 0.70f, 0.16f, 0.75f,
    1.30f, 0.32f, 1.20f, 4.0f,
    0.12f, 0.50f },

  // ---- PRISMA (ámbar) — rápido y apretado: aguanta dobles de semicorchea porque TODO es corto.
  { "PRISMA", HUE_ORANGE,
    52.0f, 6.4f, 16.0f, 0.16f, 0.34f, 0.60f, 1900.0f, 0.32f, 1.40f,
    225.0f, 345.0f, 0.060f, 3000.0f, 1.00f, 0.085f, 0.62f, 0.50f,
    112.0f, 160.0f, 224.0f, 0.09f, 0.14f, 0.11f,
    7600.0f, 0.55f, 0.020f, 0.085f, 0.74f,
    4800.0f, 0.50f, 0.07f, 0.28f, 1.00f,
    1800.0f, 1.2f, 0.007f, 0.035f,
    6800.0f, 1600.0f, 0.70f, 0.15f, 0.70f,
    1.50f, 0.24f, 1.20f, 5.0f,
    0.14f, 0.52f },

  // ---- NEXO (verde) — híbrido electrónico: bombo 808 profundo con mucho pecho, sub largo, y
  //      todo lo de arriba cortísimo y apretado (909).
  { "NEXO", HUE_GREEN,
    39.0f, 4.8f, 34.0f, 0.30f, 0.52f, 0.20f, 1100.0f, 0.28f, 1.70f,
    195.0f, 305.0f, 0.055f, 2300.0f, 1.50f, 0.085f, 0.70f, 0.32f,
    100.0f, 140.0f, 200.0f, 0.14f, 0.12f, 0.05f,
    8000.0f, 0.55f, 0.018f, 0.075f, 0.68f,
    5200.0f, 0.45f, 0.06f, 0.22f, 0.90f,
    1200.0f, 1.6f, 0.006f, 0.060f,
    6500.0f, 1400.0f, 0.60f, 0.22f, 0.95f,
    1.20f, 0.55f, 1.35f, 3.0f,
    0.16f, 0.55f },

  // ---- ABISMO (rojo) — el más grave y el más grande: aterriza en 39 Hz con el pecho al máximo.
  { "ABISMO", HUE_RED,
    37.0f, 7.2f, 42.0f, 0.30f, 0.58f, 0.24f, 900.0f, 0.18f, 1.75f,
    175.0f, 262.0f, 0.100f, 1900.0f, 0.80f, 0.140f, 0.50f, 0.24f,
    88.0f, 124.0f, 172.0f, 0.11f, 0.22f, 0.13f,
    6200.0f, 0.48f, 0.032f, 0.130f, 0.76f,
    3600.0f, 0.50f, 0.12f, 0.50f, 1.05f,
    1300.0f, 0.9f, 0.011f, 0.075f,
    5500.0f, 1300.0f, 0.60f, 0.21f, 1.00f,
    1.70f, 0.70f, 1.45f, 12.0f,
    0.30f, 0.68f },
};

uint8_t kit = 0;

// ==============================================================================================
// LA MEZCLA (fija — esto es lo que hace que suene "producido" y no a suma de sintetizadores)
// ==============================================================================================
// Ganancia: el peso vive abajo. El bombo manda, el sub va casi tan arriba como él (los dos
// son el instrumento), la caja los sigue, y TODO lo agudo va contenido — eran justo esos los
// que molestaban.
const float TRACK_GAIN[NUM_TRACKS] = {
  1.00f,   // BOMBO
  0.85f,   // CAJA
  0.60f,   // TOMS
  0.28f,   // HATS
  0.26f,   // METAL
  0.34f,   // CLAP
  0.30f,   // FX
  0.85f    // SUB
};

// Pasa-altos de banda por pista (Hz; 0 = no lleva). Es el corte que le deja el grave entero al
// bombo y al sub: nada de lo que vive arriba aporta nada abajo de su banda, así que se le saca.
// Los pasa-banda de Q bajo (hats, metal, tick) tienen faldas anchas: un hat centrado en 6.8 kHz
// con Q 0.6 todavía deja pasar energía en 250–900 Hz, que es la banda del cuerpo de la caja
// (medido: sólo 15 dB abajo). El pasa-altos de banda es lo que le corta esa falda.
const float TRACK_HP[NUM_TRACKS] = { 0.0f, 150.0f, 70.0f, 2500.0f, 1500.0f, 900.0f, 700.0f, 0.0f };

// Panorama: el ancla (bombo, caja, sub) al centro; lo agudo se abre un poco.
const float TRACK_PAN[NUM_TRACKS] = { 0.50f, 0.50f, 0.50f, 0.60f, 0.62f, 0.40f, 0.55f, 0.50f };

// Envío a la sala, relativo (se multiplica por el revSend del kit). El bombo Y EL SUB van en 0:
// mojar el grave es la forma más rápida de perder el golpe y llenar de barro el sub.
const float TRACK_SEND[NUM_TRACKS] = { 0.00f, 1.00f, 0.35f, 0.10f, 0.45f, 0.70f, 0.60f, 0.00f };

// ==============================================================================================
// LOS 12 GROOVES — escritos a mano, un carácter por semicorchea
// ==============================================================================================
// GRAMÁTICA (la misma para todas las pistas salvo donde se indica):
//   -  silencio            .  fantasma (vel 40)      x  normal (vel 92)      X  acento (vel 124)
//
// Y los caracteres propios de cada pista:
//   BOMBO   d  doble de fusas en un paso        D  ráfaga de 4 semifusas (doble bombo)
//   CAJA    f  flam (2 golpes a 18 ms)          r  redoble x2      R  redoble x4
//   TOMS    1 2 3  grave/medio/agudo normal · L M H  acentuados · l m h  fantasma
//   HATS    o  abierto      O  abierto acentuado     r  x2 (fusas)   R  x4 (semifusas, trap)
//   METAL   c/C  plato (el único agudo con cola)
//   FX      x/X  caída (barrido de ruido de agudo a medio)     r/R  riser (al revés, con swell)
//
// El BAJO ya NO se escribe acá: tiene su propio menú (BTN2+BTN4) y se arma como
// RITMO x FIGURA x GRILLA x LARGO. Ver la sección "EL BAJO" más abajo.
//
// CÓMO ESTÁN ESCRITOS (esto es la mitad del sonido):
//   · EL AIRE ES PARTE DEL GROOVE. La pegada de este idioma no viene de llenar los 16 pasos,
//     viene del silencio alrededor del golpe grande. La primera versión tenía la campana del
//     ride marcando cada agrupación y percusión en cada compás: se oía como una nube.
//   · EL GRAVE SUENA EN TODOS. El SUB entra en los apoyos, doblando al bombo: es la banda
//     donde vive el peso y estaba desaprovechada.
//   · EL METAL ES UN ACENTO, NO UN PULSO. Marca el "1" y los puntos de giro, y nada más.
//   · ARRIBA NO HAY NINGÚN INSTRUMENTO DE GROOVE MÁS QUE LOS HATS. Hubo un tick de baqueta
//     doblando los acentos y se oía como percusión latina — que no es lo que hace esta máquina.
//     En su lugar están los FX, y son transiciones: un barrido por frase, no un pulso.
//
// Cada compás va en su propia cadena; el compilador las pega (literales adyacentes).
//
// La PROGRESIÓN avanza un grado por compás, en ciclo de 4. Como varios grooves tienen frases
// de 2 compases, la armonía tarda dos vueltas en cerrar: eso es lo que hace que un loop de
// 2 compases no se sienta como un loop de 2 compases. Y sólo afecta al bajo MIDI y al sub.
const Groove GROOVES[NUM_GROOVES] = {

  // ============ 1. CADENA — 4/4 medio tiempo. El golpe grande: caja en el 3, bombo escaso y
  //              empujado, hats de trap por encima y el sub doblando los apoyos.
  { "CADENA", "4/4 medio tiempo", 16, 2, 76, 26, 0, {
    /*BOMBO*/ "X-----x-----x---"  "X-----x---x-x---",
    /*CAJA */ "--------X-------"  "--------X-----x.",
    /*TOMS */ "----------------"  "------------2-1-",
    /*HATS */ "x-x-x-x-x-x-xRx-"  "x-x-R-x-x-x-RRx-",
    /*METAL*/ "C---------------"  "----------------",
    /*CLAP */ "--------x-------"  "--------x-------",
    /*FX   */ "----------------"  "--------------R-",
    /*SUB  */ "X-------X-------"  "X-------X-------" } },

  // ============ 2. VELO — 4/4 medio tiempo lento. Dos golpes por compás y el resto es aire.
  //              Para el kit ABISMO: sub de 0.7 s abajo y casi nada arriba.
  { "VELO", "4/4 medio tiempo lento", 16, 2, 66, 25, 0, {
    /*BOMBO*/ "X-----------x---"  "X---------x-----",
    /*CAJA */ "--------X-------"  "--------X---X---",
    /*TOMS */ "----------------"  "-------------321",
    /*HATS */ "--------------o-"  "------o-------o-",
    /*METAL*/ "C---------------"  "----------------",
    /*CLAP */ "--------x-------"  "--------x---x---",
    /*FX   */ "----------------"  "-------------R--",
    /*SUB  */ "X---------------"  "X-------X-------" } },

  // ============ 3. OFRENDA — 4/4 con swing y fantasmas. El groove lineal, casi góspel, con la
  //              caja hablando bajito entre los acentos y el tick marcando la baqueta.
  { "OFRENDA", "4/4 con swing", 16, 2, 92, 28, 12, {
    /*BOMBO*/ "X--.--x-..x---x-"  "X--.--x-..x-x-.-",
    /*CAJA */ ".-.-X-.-.-.-X-.-"  ".-.-X-.-.-.-X.x.",
    /*TOMS */ "----------------"  "--------------1-",
    /*HATS */ "x-o-x-x-x-o-x-x-"  "x-o-x-x-x-o-xrxr",
    /*METAL*/ "----------------"  "----------------",
    /*CLAP */ "----X-------X---"  "----X-------X---",
    /*FX   */ "----------------"  "--------------x-",
    /*SUB  */ "X-------X-------"  "X-------X-------" } },

  // ============ 4. ESPIRAL — 7/8 agrupado 3+2+2. El bombo marca la agrupación, la caja cae
  //              siempre en el mismo lugar (por eso el compás no se pierde) y el metal sólo
  //              marca el arranque de la frase.
  { "ESPIRAL", "7/8 (3+2+2)", 14, 4, 128, 26, 0, {
    /*BOMBO*/ "X-----X-------"  "X-----X---X---"  "X---X-X-------"  "X-----X-x-----",
    /*CAJA */ "----------X---"  "----------X---"  "----------X-.-"  "----------X.x.",
    /*TOMS */ "--------------"  "--------------"  "------------2-"  "------2-----1-",
    /*HATS */ "x-x-x-x-x-x-x-"  "x-x-x-x-x-x-x-"  "x-x-x-x-x-x-x-"  "x-x-x-x-xrxrx-",
    /*METAL*/ "C-------------"  "--------------"  "--------------"  "--------------",
    /*CLAP */ "--------------"  "----------x---"  "--------------"  "----------x---",
    /*FX   */ "--------------"  "--------------"  "--------------"  "----------R---",
    /*SUB  */ "X-------------"  "X-----X-------"  "X-------------"  "X-----X-------" } },

  // ============ 5. CISMA — 5/8 + 7/8 alternados (12 corcheas por vuelta). Cada compás son dos
  //              sub-compases de largo distinto; lo que lo mantiene de pie es que el metal cae
  //              siempre en el arranque de cada uno. El "compás" son las dos mitades: 10 + 14.
  { "CISMA", "5/8 + 7/8", 24, 2, 122, 26, 0, {
    /*BOMBO*/ "X---X-----" "X-----X-------"   "X---X-x---" "X-----X---X---",
    /*CAJA */ "------X---" "------X-------"   "------X---" "------X---X-.-",
    /*TOMS */ "----------" "--------------"   "----------" "----------2-1-",
    /*HATS */ "x-x-x-x-x-" "x-x-x-x-x-x-x-"   "x-x-x-x-x-" "x-x-x-x-xrx-x-",
    /*METAL*/ "C---------" "--------------"  "----------" "--------------",
    /*CLAP */ "----------" "--------------"   "------x---" "------x-------",
    /*FX   */ "----------" "--------------"  "----------" "----------R---",
    /*SUB  */ "X---------" "X-------------"   "X---------" "X-------------" } },

  // ============ 6. LABERINTO — 9/8 agrupado 3+3+3. El bombo dibuja los tres grupos y la caja
  //              va corrida contra ellos; el sub apoya el 1 y el medio.
  { "LABERINTO", "9/8 (3+3+3)", 18, 2, 112, 35, 0, {
    /*BOMBO*/ "X-----X-----X-----"  "X-----X-x---X---x-",
    /*CAJA */ "--------X-------X-"  "--------X-------X.",
    /*TOMS */ "------------------"  "--------------21--",
    /*HATS */ "x-x-x-x-x-x-x-x-x-"  "x-x-x-x-x-x-x-xrxr",
    /*METAL*/ "C-----------------"  "------------------",
    /*CLAP */ "------------------"  "--------x---------",
    /*FX   */ "------------------"  "----------------R-",
    /*SUB  */ "X-----------X-----"  "X-----------X-----" } },

  // ============ 7. TRECE — 13/8 agrupado 3+3+3+2+2. El compás raro por excelencia; se sostiene
  //              porque la caja cae en dos puntos fijos y el bombo marca los grupos.
  { "TRECE", "13/8 (3+3+3+2+2)", 26, 2, 120, 28, 0, {
    /*BOMBO*/ "X-----X-----X-----X---X---"  "X-----X-x---X-----X---X-x-",
    /*CAJA */ "--------X-----------X-----"  "--------X-----------X---.x",
    /*TOMS */ "--------------------------"  "------------------------21",
    /*HATS */ "x-x-x-x-x-x-x-x-x-x-x-x-x-"  "x-x-x-x-x-x-x-x-x-x-x-xrxr",
    /*METAL*/ "C-------------------------"  "--------------------------",
    /*CLAP */ "--------------------------"  "--------x-----------x-----",
    /*FX   */ "--------------------------"  "------------------------R-",
    /*SUB  */ "X-----------X---------X---"  "X-----------X---------X---" } },

  // ============ 8. VORTICE — 4/4 con doble bombo. Semicorcheas de bombo corriendo debajo de un
  //              backbeat firme, y el bajo al unísono. Acá el bombo TIENE que ser corto.
  { "VORTICE", "4/4 doble bombo", 16, 2, 168, 28, 0, {
    /*BOMBO*/ "XxxxX-x-XxxxX-x-"  "XxxxX-x-Xxxxxxxx",
    /*CAJA */ "----X-------X---"  "----X-------X-x.",
    /*TOMS */ "----------------"  "--------2-1-----",
    /*HATS */ "x-x-x-x-x-x-x-x-"  "x-x-x-x-x-x-xrxr",
    /*METAL*/ "C---------------"  "--------c-------",
    /*CLAP */ "----------------"  "----------------",
    /*FX   */ "----------------"  "--------------R-",
    /*SUB  */ "X-------X-------"  "X-------X-------" } },

  // ============ 9. PRISMA — 5/4. El impar "amable": cinco negras claras, el metal cada dos y
  //              la caja en el 2 y el 4.
  { "PRISMA", "5/4", 20, 2, 144, 31, 0, {
    /*BOMBO*/ "X---x---X---x---X---"  "X---x---X-x-x---X-x-",
    /*CAJA */ "----X-------X-------"  "----X-------X-----x.",
    /*TOMS */ "--------------------"  "----------------321-",
    /*HATS */ "x-x-x-x-x-x-x-x-x-x-"  "x-x-x-x-x-x-x-x-xrxr",
    /*METAL*/ "C-------------------"  "--------------------",
    /*CLAP */ "--------------------"  "----x-------x-------",
    /*FX   */ "--------------------"  "------------------R-",
    /*SUB  */ "X-------X-------X---"  "X-------X-------X---" } },

  // ============ 10. HIBRIDO — 4/4 electrónico. Acá manda la mitad de máquina: hats de trap,
  //               clap doblando la caja, tick de shaker y el sub como 808 en cada apoyo.
  { "HIBRIDO", "4/4 electronico", 16, 2, 96, 26, 0, {
    /*BOMBO*/ "X-----x-X---x-x-"  "X-----x-X-x---x-",
    /*CAJA */ "--------X-------"  "--------X-----x-",
    /*TOMS */ "----------------"  "----------------",
    /*HATS */ "xRx-xRx-xRx-xRxR"  "xRx-xRxRxRx-RRxR",
    /*METAL*/ "C---------------"  "----------------",
    /*CLAP */ "----x---X---x---"  "----x---X---x-x-",
    /*FX   */ "--------------x-"  "------x-------R-",
    /*SUB  */ "X-------X-------"  "X-------X-----X-" } },

  // ============ 11. CORAL — 12/8. El pulso de corchea con puntillo, lento y solemne. El sub
  //               sostiene cada pulso y arriba casi no pasa nada.
  { "CORAL", "12/8", 24, 2, 62, 33, 0, {
    /*BOMBO*/ "X-----------X-----------"  "X-----------X-----x-----",
    /*CAJA */ "------X-----------X-----"  "------X-----------X-----",
    /*TOMS */ "------------------------"  "---------------------321",
    /*HATS */ "x-x-x-x-x-x-x-x-x-x-x-x-"  "x-x-x-x-x-x-x-x-x-x-xrxr",
    /*METAL*/ "C-----------------------"  "------------------------",
    /*CLAP */ "------X-----------X-----"  "------X-----------X-----",
    /*FX   */ "------------------------"  "----------------------R-",
    /*SUB  */ "X-----------X-----------"  "X-----------X-----------" } },

  // ============ 12. DESPLAZADO — 4/4 con las semicorcheas agrupadas 3+3+3+3+2+2. El riff entra
  //               y sale del pulso, pero la caja se queda clavada en el 2 y el 4: eso es lo que
  //               lo hace sentir desplazado en vez de perdido. El tick articula el riff arriba
  //               y el sub lo dobla abajo, sin que nada de eso sea una nota.
  { "DESPLAZADO", "4/4 desplazado (3+3+3+3+2+2)", 16, 4, 138, 26, 0, {
    /*BOMBO*/ "X--X--X--X--X-X-"  "X--X--X--X--X-X-"  "X--X--X--X--X-X-"  "X--X--X--X--XxXx",
    /*CAJA */ "----X-------X---"  "----X-------X---"  "----X-------X---"  "----X-------X.x.",
    /*TOMS */ "----------------"  "---------2------"  "----------------"  "---------2---21-",
    /*HATS */ "x-x-x-x-x-x-x-x-"  "x-x-x-x-x-x-x-x-"  "x-x-x-x-x-x-x-x-"  "x-x-x-x-x-x-xrxr",
    /*METAL*/ "C---------------"  "----------------"  "----------------"  "----------------",
    /*CLAP */ "----------------"  "----X-------X---"  "----------------"  "----X-------X---",
    /*FX   */ "----------------"  "----------------"  "--------------x-"  "--------------R-",
    /*SUB  */ "X--X--X--X--X-X-"  "----------------"  "X--X--X--X--X-X-"  "----------------" } },
};

uint8_t groove = 0;

// ==============================================================================================
// Patrón decodificado
// ==============================================================================================
uint8_t patVel[NUM_TRACKS][MAX_STEPS];   // velocidad 0..127 (0 = no suena)
uint8_t patAux[NUM_TRACKS][MAX_STEPS];   // variante: altura del tom, hat abierto, redoble...
int     totalSteps = 32;

#define VEL_GHOST  40
#define VEL_NORM   92
#define VEL_ACC   124

// Traduce un carácter de la tabla a (velocidad, aux) según la pista.
void decodeChar(uint8_t track, char c, uint8_t *vel, uint8_t *aux) {
  *vel = 0; *aux = 0;
  switch (c) {
    case '-': return;
    case '.': *vel = VEL_GHOST; return;
    case 'x': *vel = VEL_NORM;  return;
    case 'X': *vel = VEL_ACC;   return;
  }
  switch (track) {
    case T_KICK:
      if (c == 'd') { *vel = 104; *aux = 2; }          // doble de fusas
      else if (c == 'D') { *vel = 118; *aux = 4; }     // ráfaga de 4
      break;
    case T_SNARE:
      if (c == 'f') { *vel = 110; *aux = 1; }          // flam
      else if (c == 'r') { *vel = 96;  *aux = 2; }
      else if (c == 'R') { *vel = 100; *aux = 4; }
      break;
    case T_TOM:
      if (c == '1') { *vel = VEL_NORM;  *aux = 0; }
      else if (c == '2') { *vel = VEL_NORM;  *aux = 1; }
      else if (c == '3') { *vel = VEL_NORM;  *aux = 2; }
      else if (c == 'L') { *vel = VEL_ACC;   *aux = 0; }
      else if (c == 'M') { *vel = VEL_ACC;   *aux = 1; }
      else if (c == 'H') { *vel = VEL_ACC;   *aux = 2; }
      else if (c == 'l') { *vel = VEL_GHOST; *aux = 0; }
      else if (c == 'm') { *vel = VEL_GHOST; *aux = 1; }
      else if (c == 'h') { *vel = VEL_GHOST; *aux = 2; }
      break;
    case T_HAT:
      if (c == 'o') { *vel = VEL_NORM; *aux = 1; }     // abierto
      else if (c == 'O') { *vel = VEL_ACC; *aux = 1; }
      else if (c == 'r') { *vel = 86; *aux = 2; }      // redoble de fusas
      else if (c == 'R') { *vel = 92; *aux = 4; }      // redoble de semifusas (trap)
      break;
    case T_METAL:
      if (c == 'c') { *vel = 104;     *aux = 2; }       // plato largo
      else if (c == 'C') { *vel = VEL_ACC; *aux = 2; }
      break;
    case T_FX:
      if (c == 'r') { *vel = 100;     *aux = 1; }       // riser (barrido hacia arriba, con swell)
      else if (c == 'R') { *vel = VEL_ACC; *aux = 1; }
      break;
    default: break;
  }
}

void cargarGroove(int g) {
  const Groove &gr = GROOVES[g];
  int n = (int)gr.stepsPerBar * (int)gr.bars;
  if (n > MAX_STEPS) n = MAX_STEPS;
  totalSteps = n;

  for (int t = 0; t < NUM_TRACKS; t++) {
    const char *s = gr.ln[t];
    int len = (int)strlen(s);
    for (int i = 0; i < n; i++) {
      char c = (i < len) ? s[i] : '-';
      decodeChar((uint8_t)t, c, &patVel[t][i], &patAux[t][i]);
    }
  }
}

// ==============================================================================================
// TRANSPORTE MIDI — el DIN-5 (GPIO 43, 31250 baud) NO manda notas: manda el RELOJ
// ==============================================================================================
// Este firmware no toca ninguna nota por MIDI. Manda **MIDI Clock a 24 PPQN** y **Start**, que
// es lo que un sinte externo necesita para que su secuenciador, su arpegiador o su delay corran
// enganchados a la bateria. La linea de bajo la pone el sinte: elegir el sonido y la secuencia
// alla es infinitamente mas expresivo que cualquier tabla de patrones metida aca adentro, y el
// unico trabajo real de esta maquina es dar un pulso que no se mueva.
//
// DOS DECISIONES QUE IMPORTAN:
//
// 1) EL RELOJ SE MANDA DESDE LA TAREA DE CONTROL, no desde la de audio, pero se CUENTA con el
//    contador de muestras del audio. Asi tiene lo bueno de los dos lados: no puede irse de fase
//    con la bateria (las dos cosas salen del mismo contador, no de dos relojes distintos) y la
//    resolucion es la de la tarea de control (1 ms) en vez de la del buffer de audio (2.9 ms).
//    Mandarlo desde el audio metia +-2.9 ms de fluctuacion en cada pulso, que a 120 BPM es un
//    14 % del tick — suficiente para que el arpegiado del sinte suene tembloroso.
//
// 2) SE MANDA STOP+START CADA VEZ QUE SE REUBICA EL "1" (al cambiar de groove y en el primer
//    tap de una serie de tap tempo). Sin eso el sinte sigue en el paso donde estaba y su
//    secuencia queda corrida contra la bateria para siempre. El Stop antes del Start no es
//    adorno: hay equipos que ignoran un Start si creen que ya estan corriendo.

const uint8_t MIDI_CLOCK = 0xF8;
const uint8_t MIDI_START = 0xFA;
const uint8_t MIDI_STOP  = 0xFC;

volatile bool     reqMidiStart   = false;   // la tarea de audio pide realinear el sinte externo
volatile uint32_t muestrasReloj  = 0;       // contador de muestras del audio (lo lee el control)

// ==============================================================================================
// Voces
// ==============================================================================================
Voice voices[MAX_VOICES];
Pend  pend[MAX_PEND];
int   pendN = 0;

float duckEnv = 0.0f, duckCoef = 0.0f;   // sidechain del bombo
float filtEnv = 0.0f, filtEnvCoef = 0.0f;// envolvente del filtro global: cada golpe abre el corte
float ledFlash[NUM_LEDS];

uint32_t robosDeVoz = 0;      // diagnóstico: cada robo es un golpe cortado en seco (un clic)

// ---------------------------------------------------------------------------------------------
// PEDIDOS ENTRE NUCLEOS. El audio vive en su propia tarea (core 1) y los controles en la otra
// (core 0), asi que la tarea de control NO toca el secuenciador ni las voces: deja un pedido y
// la tarea de audio lo aplica en el borde de un buffer. Sin esto habria carrera sobre
// masterStep, stepLen y el pool de voces, y una carrera ahi se oye como un golpe partido.
// Son escrituras de 32 bits alineadas: atomicas en el S3, no hace falta mutex.
// ---------------------------------------------------------------------------------------------
volatile bool  reqGroove    = false;   // BTN1 panel A: siguiente groove
volatile bool  reqKit       = false;   // BTN2 panel A: siguiente kit
volatile bool  reqUnKit     = false;   // el combo deshace el BTN2
volatile bool  reqReubicar  = false;   // primer tap: reubica el "1"
volatile float reqBPM       = 0.0f;    // > 0 = nuevo tempo pedido
volatile int   reqRepeatLen = -1;      // >= 0 = nueva division de beat repeat
volatile bool  halfHeldReq  = false;   // BTN4 mantenido
volatile bool  fillHeldReq  = false;   // BTN5 mantenido
float coefApagado = 0.0f;     // tau 1.5 ms: el fundido final de cada voz (se calcula en setup)

int allocVoice() {
  for (int i = 0; i < MAX_VOICES; i++) if (!voices[i].active) return i;
  robosDeVoz++;
  // No hay atajo por `dying`: una voz recien fastKilleada todavia esta a amplitud casi plena
  // (el fundido es de 3 ms), asi que pisarla es exactamente el clic que se quiere evitar. El
  // criterio de energia de abajo ya la elige sola cuando de verdad se apago.
  // Roba la de menor energía, pero NUNCA el bombo (es el ancla del ritmo)
  int best = -1; float lo = 1e9f;
  for (int i = 0; i < MAX_VOICES; i++) {
    if (voices[i].type == T_KICK) continue;
    float e = (voices[i].env + voices[i].nEnv + voices[i].t3Env) * voices[i].amp;
    if (e < lo) { lo = e; best = i; }
  }
  return best < 0 ? 0 : best;
}

// Ranuras monofónicas: un parche por instrumento. El CLAP queda fuera a propósito (sus dos
// réplicas agendadas SON el clap: si se cortaran entre ellas dejaría de sonar a varias
// manos) y el PERC también (una campana afinada encimándose es musical, no un error).
#define NUM_SLOTS 9
int8_t slotVoice[NUM_SLOTS];

int8_t slotDe(uint8_t t, uint8_t aux) {
  switch (t) {
    case T_KICK:  return 0;
    case T_SNARE: return 1;
    case T_TOM:   return 2 + (int8_t)(aux > 2 ? 2 : aux);   // 3 toms = 3 parches
    case T_HAT:   return 5;                                 // el choke del hi-hat
    case T_METAL: return 6 + (int8_t)(aux == 2 ? 1 : 0);    // chasquido corto · plato
    case T_SUB:   return 8;
    default:      return -1;
  }
}

// Mata una voz con un fundido de 3 ms. No se corta de golpe: un corte seco a amplitud
// alta es un clic. Es el mismo fast-kill que usa cyber_kit para los combos.
void fastKill(int i) {
  voices[i].envCoef = decCoef(0.003f);
  voices[i].nCoef   = decCoef(0.003f);
  voices[i].t3Coef  = decCoef(0.003f);
  voices[i].fCoef   = 0.0f;                 // que no siga barriendo el pitch mientras muere
  voices[i].slot    = -1;
  // Y CONGELA EL ATAQUE. Sin esto una voz golpeada durante su ataque (el plato, que abre en
  // 4 ms) seguia SUBIENDO mientras se suponia que se estaba apagando: el decaimiento vive en
  // la rama `else` del ataque y no llegaba a aplicarse. Congelado el ataque, el fundido de
  // 3 ms arranca en la misma muestra.
  voices[i].atkInc  = 0.0f;
  voices[i].dying   = true;
}

inline float midiHz(int n) { return 440.0f * powf(2.0f, (float)(n - 69) / 12.0f); }

// Frecuencia del SUB/808 y afinación de la campana FM: las dos siguen el acorde del compás,
// así que la parte "electrónica" del kit está en tonalidad igual que el bajo.
float subHz      = 41.2f;     // altura del SUB: la TÓNICA, y no se mueve nunca

// El SUB suena la TÓNICA del groove y no se mueve nunca: es percusión afinada, no una melodía.
// Esta función se llama sólo al cambiar de groove, NO en cada compás.
void afinarSub() {
  const Groove &g = GROOVES[groove];
  subHz = midiHz((int)GROOVES[groove].rootMidi) * 0.5f;
  while (subHz < 30.0f) subHz *= 2.0f;
  while (subHz > 62.0f) subHz *= 0.5f;
}

// ==============================================================================================
// Disparo de una voz
// ==============================================================================================
// Multiplicador del decay del golpe que se va a disparar. Vale 1 siempre, salvo en los
// redobles: un roll con los golpes largos del kit apila 50 toms de 0.9 s encima y eso no es
// un redoble, es barro (medido: a 220 BPM se agotaban las 12 voces). Los golpes de redoble
// salen cortos, como los toca un baterista.
float trigDecMul = 1.0f;

void trigVoice(uint8_t t, uint8_t vel, uint8_t aux) {
  const Kit &k = KITS[kit];

  // ¿esta pista tiene parche propio? Si ya hay algo sonando en él, se apaga en 3 ms.
  int8_t slot = slotDe(t, aux);
  if (slot >= 0) {
    int8_t prev = slotVoice[slot];
    if (prev >= 0 && voices[prev].active && voices[prev].slot == slot) fastKill(prev);
  }

  int idx = allocVoice();
  Voice &v = voices[idx];
  v.slot = slot;
  if (slot >= 0) slotVoice[slot] = (int8_t)idx;
  float amp = (float)vel * (1.0f / 127.0f);

  v.active = true; v.type = t; v.dying = false;
  v.amp = amp * TRACK_GAIN[t];
  v.atk = 1.0f; v.atkInc = 1.0f;
  v.env = 1.0f; v.envCoef = 0.0f;
  v.ph = 0.0f; v.f = 100.0f; v.fEnd = 100.0f; v.fCoef = 0.0f;
  v.ph2 = 0.0f; v.ratio2 = 0.0f; v.amp2 = 0.0f;
  v.ph3 = 0.0f; v.ratio3 = 0.0f; v.amp3 = 0.0f;
  v.wave = 0; v.sat = 0.0f;
  v.nEnv = 1.0f; v.nCoef = 0.0f; v.nAmt = 0.0f;
  v.t3Env = 1.0f; v.t3Coef = 0.0f; v.t3Amt = 0.0f; v.t3Ph = 0.0f; v.t3F = 1000.0f;
  v.t3IsNoise = false;
  v.useF1 = false; v.useF2 = false; v.useLane = false;
  v.swInc = 0.0f; v.swT = 0.0f;
  biqClear(v.bz1); biqClear(v.bz2); biqClear(v.laneZ);
  float pan = TRACK_PAN[t], dec = 0.2f, nDec = 0.02f, t3Dec = 0.01f;

  switch (t) {
    case T_KICK: {
      // Cuerpo: seno que cae desde kRatio x hasta la fundamental. Termina en 43–56 Hz
      // (no en 36: eso no lo reproduce ningún parlante chico) y lleva saturación propia,
      // que le genera armónicos y lo hace audible incluso sin graves reales.
      v.f = k.kFreq * k.kRatio; v.fEnd = k.kFreq;
      v.fCoef = 1.0f - expf(-1.0f / (k.kDropMs * 0.001f * SAMPLE_RATE));
      dec = k.kDec;
      v.ratio2 = 2.0f; v.amp2 = 0.10f;            // 2o armónico: cuerpo en parlante chico
      v.sat = k.kSat;
      // Capa 3 = el MAZO, y va con RUIDO PASA-BANDA, no con un seno. Un seno corto en
      // 1–2 kHz es literalmente un PITIDO, y se oía pegado a cada bombo: era el sonido agudo
      // molesto que había que sacar. Un mazo de verdad no es un tono, es un golpe de madera
      // sobre un parche — o sea una banda de ruido. Da la misma definición y no canta ninguna
      // nota, que además es la regla de este firmware.
      v.t3Amt = k.kKnock * 1.6f; t3Dec = 0.014f; v.t3IsNoise = true;
      makeBPF(v.bq2, k.kKnockF, 0.80f);
      // Capa de ruido = el click del parche, 5 ms y se va
      v.nAmt = k.kClick * 0.45f; nDec = 0.005f;
      v.useF1 = true; makeHPF(v.bq1, 3000.0f, 0.7f);
      duckEnv = 1.0f;                              // sidechain
      break;
    }
    case T_SNARE: {
      // Caja grande y AFINADA: dos parciales de cuerpo que caen un 15 % + ruido de bordonera
      // con su propia envolvente + un crack de 4 kHz de 8 ms. Las tres capas por separado
      // son lo que separa una caja con peso de un "pfff".
      v.f = k.sF1; v.fEnd = k.sF1 * 0.85f;
      v.fCoef = 1.0f - expf(-1.0f / (0.035f * SAMPLE_RATE));
      v.ratio2 = k.sF2 / k.sF1; v.amp2 = 0.60f;
      dec = k.sDec;
      v.env = (1.0f - k.sMix) * 1.7f;
      v.nAmt = k.sMix * 1.5f; nDec = k.sNDec;
      v.useF1 = true; makeBPF(v.bq1, k.sFc, k.sQ);
      v.t3Amt = k.sCrack * 0.9f; t3Dec = 0.008f; v.t3IsNoise = true;
      makeBPF(v.bq2, 3400.0f, 0.9f);
      // SIN saturación: la caja tiene DOS parciales de cuerpo (sF1 y sF2), así que saturarla
      // genera sumas y diferencias entre ellas. Medido: aparecía un tono puro en 636 Hz, 16 dB
      // sobre sus vecinos — o sea un pitido dentro de la caja. Es la misma regla que vale para
      // los metales y la FM: sólo se satura lo que tiene UNA sola parcial fuerte.
      break;
    }
    case T_TOM: {
      // Toms cantados: seno con caída de pitch + un parcial inarmónico de parche (1.58).
      // Viven en 88–224 Hz y llevan pasa-altos en 70: el sub entero queda para el bombo.
      if (aux > 2) aux = 2;
      float f0 = (aux == 0) ? k.tF1 : (aux == 1 ? k.tF2 : k.tF3);
      v.f = f0 * (1.0f + k.tDrop); v.fEnd = f0;
      v.fCoef = 1.0f - expf(-1.0f / (0.045f * SAMPLE_RATE));
      dec = k.tDec;
      v.ratio2 = 1.58f; v.amp2 = 0.30f;    // parcial inarmónico de parche
      // Sin saturación, por lo mismo que la caja: son dos parciales (1 y 1.58) y saturarlas
      // juntas deja un tono en su diferencia. Medido: 899 Hz, 13 dB sobre los vecinos.
      v.nAmt = k.tNoise; nDec = 0.012f;
      v.useF1 = true; makeHPF(v.bq1, 1800.0f, 0.7f);
      pan = (aux == 0) ? 0.38f : (aux == 1 ? 0.50f : 0.63f);   // los toms se abren
      break;
    }
    case T_HAT: {
      // RUIDO pasa-banda y nada más. Pasa-banda (no pasa-altos) porque un pasa-altos deja
      // pasar todo hasta Nyquist y esa octava de arriba es sólo filo. Q bajo: un pico estrecho
      // arriba de 6 kHz es lo más molesto que puede hacer este firmware.
      bool open = (aux == 1);
      nDec = open ? k.hDecO : k.hDecC;
      dec = 0.004f; v.env = 0.0f;                  // CERO capa tonal: un hat no tiene nota
      v.nAmt = k.hAmt * 1.5f;
      v.useF1 = true; makeBPF(v.bq1, k.hFc, k.hQ);
      pan = open ? 0.52f : 0.62f;
      break;
    }
    case T_METAL: {
      // METAL: el chasquido metálico y el plato, los dos hechos SÓLO con ruido pasa-banda.
      // Antes esto era una campana con tres parciales afinados — o sea una nota, o sea melodía,
      // y encima larga. Un plato no necesita altura: necesita banda y sobre. aux 2 = plato
      // largo (con ataque de 4 ms, que es lo que lo hace abrir), aux 0/1 = chasquido corto.
      bool plato = (aux == 2);
      dec = 0.004f; v.env = 0.0f;
      nDec = plato ? k.mDecLargo : k.mDecCorto;
      v.nAmt = k.mAmt * (plato ? 1.5f : 1.2f);
      v.useF1 = true; makeBPF(v.bq1, k.mFc * (plato ? 0.90f : 1.0f), k.mQ);
      if (plato) { v.atk = 0.0f; v.atkInc = 1.0f / (0.004f * SAMPLE_RATE); }
      pan = plato ? 0.42f : 0.66f;
      break;
    }
    case T_CLAP: {
      // Clap: palmada pasa-banda + cola corta. Las dos réplicas se agendan aparte (6.5 y 13 ms)
      // desde stepTrigger: eso es lo que lo hace sonar a varias manos.
      dec = 0.004f; v.env = 0.0f;
      v.nAmt = 1.20f; nDec = k.cDec;
      v.useF1 = true; makeBPF(v.bq1, k.cFc, k.cQ);
      if (aux == 0) {                              // sólo la primera palmada lleva cola
        v.t3Amt = 0.60f; t3Dec = k.cTail; v.t3IsNoise = true;
        makeBPF(v.bq2, k.cFc * 0.95f, k.cQ * 0.8f);
      }
      pan = 0.40f + frnd() * 0.08f;
      break;
    }
    case T_FX: {
      // EFECTOS: barridos de ruido para las transiciones, en el lugar donde antes había un tick
      // de baqueta — que se oía como percusión latina y no es lo que hace esta máquina.
      // aux 0 = CAÍDA (barrido de agudo a medio), aux 1 = RISER (al revés, con swell).
      bool sube = (aux == 1);
      dec = 0.004f; v.env = 0.0f;                  // ruido y nada más: no tiene altura
      nDec = k.fxDec;
      v.nAmt = k.fxAmt;
      v.useF1 = true;
      v.swFc    = sube ? k.fxFcFin : k.fxFcIni;
      v.swFcFin = sube ? k.fxFcIni : k.fxFcFin;
      v.swQ     = k.fxQ;
      v.swInc   = 1.0f / (k.fxDec * SAMPLE_RATE);
      makeBPF(v.bq1, v.swFc, v.swQ);
      if (sube) { v.atk = 0.0f; v.atkInc = 1.0f / (k.fxDec * 0.55f * SAMPLE_RATE); }
      pan = 0.5f;
      break;
    }
    case T_SUB: {
      // SUB: la única voz que sigue la armonía, y vive abajo de 60 Hz. Acá SÍ puede ser largo:
      // el peso del instrumento está en esta banda. Comparte el grave con el bombo, así que el
      // sidechain del bombo lo agacha — es la única forma de que dos cosas vivan en el sub.
      v.f = subHz * k.bFreqMul; v.fEnd = subHz;
      v.fCoef = 1.0f - expf(-1.0f / (0.040f * SAMPLE_RATE));
      dec = k.bDec;
      // ARMONICOS 2 y 3, y no son decoracion: la fundamental del sub vive en 33-45 Hz y un
      // parlante chico (o unos monitores sin sub) sencillamente NO la reproduce. Lo que se
      // escucha son los armonicos, y el oido reconstruye la fundamental que falta. Con el 2o
      // en 0.16 el sub se sentia "de mas" en un equipo grande y desaparecia en uno chico;
      // subirlo y agregar el 3o es lo que lo hace audible en los dos.
      v.ratio2 = 2.0f; v.amp2 = 0.34f;
      v.ratio3 = 3.0f; v.amp3 = 0.13f;
      v.sat = k.bSat;
      v.nAmt = 0.35f; nDec = k.bNoiseMs * 0.001f;
      v.useF1 = true; makeHPF(v.bq1, 1200.0f, 0.7f);
      break;
    }
  }

  // Banda de la pista (la mezcla): pasa-altos fijo para que nadie invada el sub del bombo
  if (TRACK_HP[t] > 1.0f) {
    v.useLane = true;
    makeHPF(v.lane, TRACK_HP[t], 0.707f);
  }
  v.send = TRACK_SEND[t] * k.revSend;

  // Los golpes con pegada abren el filtro global → el filtro respira con el ritmo en vez de
  // quedarse quieto. Responde a lo que suena; no es un LFO moviéndose solo.
  if (t == T_KICK || t == T_SNARE || t == T_TOM || t == T_CLAP)
    if (amp > filtEnv) filtEnv = amp;

  v.envCoef = decCoef(dec * trigDecMul);
  v.nCoef   = decCoef(nDec);
  v.t3Coef  = decCoef(t3Dec);
  v.lg = sqrtf(1.0f - pan);
  v.rg = sqrtf(pan);
}

// Agenda un golpe para dentro de N muestras (redobles, flam, doble bombo, réplicas del clap)
void agendar(int32_t samples, uint8_t track, uint8_t vel, uint8_t aux) {
  if (pendN >= MAX_PEND) return;
  pend[pendN].left = samples;
  pend[pendN].track = track;
  pend[pendN].vel = vel;
  pend[pendN].aux = aux;
  pendN++;
}

// ==============================================================================================
// Reloj / transporte
// ==============================================================================================
float bpm         = 120.0f;
float stepSamples = 0.0f;      // muestras por semicorchea
float stepAcc     = 0.0f;
int   masterStep  = 0;         // posición LIBRE del reloj
int   curPlayStep = 0;         // paso que SUENA
int   phraseRep   = 0;         // repeticiones completas de la frase (para el fill automático)

bool  halfHeld    = false;     // MEDIO TIEMPO (BTN4)
int   halfStep    = 0;
uint8_t halfTick  = 1;

bool  fillHeld    = false;     // REDOBLE (BTN5)
uint32_t fillT0   = 0;
float fillAcc     = 0.0f;
int   fillCount   = 0;

// BEAT REPEAT (POT4 del panel A): longitud del loop en pasos — 0 = OFF
int repeatLen    = 0;
int repeatZone   = 0;          // 0 OFF · 1 x2 · 2 x4 · 3 x8 · 4 x16
int repeatAnchor = 0;
int repeatPos    = 0;


float stepLen = 0.0f;          // largo del paso EN CURSO, ya con swing y medio tiempo

void setBPM(float b) {
  if (b < 50.0f)  b = 50.0f;
  if (b > 220.0f) b = 220.0f;
  bpm = b;
  stepSamples = (60.0f / bpm) * (float)SAMPLE_RATE * 0.25f;   // semicorchea
}

// El SWING alarga las semicorcheas pares y acorta las impares. Se LATCHEA al empezar cada
// paso (nunca se recalcula a mitad de camino): si el largo del paso en curso puede cambiar,
// mover el tempo dispara de nuevo la nota que ya estaba sonando — es el bug que se vio en
// espacio_modular al barrer el pot de tempo.
float duracionPaso() {
  float sw = (float)GROOVES[groove].swing * 0.01f;
  if (sw <= 0.0f) return stepSamples;
  return (masterStep & 1) ? stepSamples * (1.0f - sw) : stepSamples * (1.0f + sw);
}

inline float pasoEfectivo() { return stepSamples * (halfHeld ? 2.0f : 1.0f); }

// ==============================================================================================
// Paso del secuenciador
// ==============================================================================================
void stepTrigger(int s) {
  curPlayStep = s;
  const Groove &g = GROOVES[groove];
  float eStep = pasoEfectivo();

  bool enRedoble = FILL_AUTO && !fillHeld && (phraseRep & 3) == 3 && s >= totalSteps - 4;

  for (int t = 0; t < NUM_TRACKS; t++) {
    // Durante el REDOBLE manual el fill se queda con caja/toms/hats/ride/clap;
    // el bombo, el sub y el perc siguen debajo para no perder el piso
    if (fillHeld && t >= T_SNARE && t <= T_CLAP) continue;
    // Y durante el redoble AUTOMATICO el patron cede los toms y el metal (ver mas abajo)
    if (enRedoble && (t == T_TOM || t == T_METAL)) continue;
    uint8_t vel = patVel[t][s];
    if (vel == 0) continue;
    uint8_t aux = patAux[t][s];

    // Humanización: +-5 % de velocidad. Sin esto, 32 golpes idénticos suenan a máquina
    // (que no es lo mismo que sonar apretado).
    float hv = (float)vel * (0.95f + frnd() * 0.10f);
    if (hv > 127.0f) hv = 127.0f;
    uint8_t v8 = (uint8_t)hv;

    trigVoice((uint8_t)t, v8, aux);

    // Sub-golpes: redobles de hat, dobles de bombo, flam y redoble de caja
    if (t == T_HAT && aux >= 2) {
      int n = aux;                                  // 2 = fusas · 4 = semifusas (trap)
      for (int i = 1; i < n; i++)
        agendar((int32_t)(eStep * (float)i / (float)n), T_HAT, (uint8_t)(v8 * 0.82f), 0);
    } else if (t == T_KICK && aux >= 2) {
      int n = aux;
      for (int i = 1; i < n; i++)
        agendar((int32_t)(eStep * (float)i / (float)n), T_KICK, (uint8_t)(v8 * 0.88f), 0);
    } else if (t == T_SNARE && aux == 1) {
      agendar((int32_t)(0.018f * SAMPLE_RATE), T_SNARE, (uint8_t)(v8 * 0.72f), 0);   // flam
    } else if (t == T_SNARE && aux >= 2) {
      int n = aux;
      for (int i = 1; i < n; i++)
        agendar((int32_t)(eStep * (float)i / (float)n), T_SNARE, (uint8_t)(v8 * 0.72f), 0);
    } else if (t == T_CLAP) {
      agendar((int32_t)(0.0065f * SAMPLE_RATE), T_CLAP, (uint8_t)(v8 * 0.80f), 1);
      agendar((int32_t)(0.0130f * SAMPLE_RATE), T_CLAP, (uint8_t)(v8 * 0.68f), 1);
    }

    if (t == T_KICK)                       ledFlash[0] = 1.0f;
    else if (t == T_SNARE || t == T_CLAP)  ledFlash[1] = 1.0f;
    else if (t == T_HAT || t == T_METAL)   ledFlash[2] = 1.0f;
    else if (t == T_TOM)                   ledFlash[3] = 1.0f;
    else                                   ledFlash[4] = 1.0f;
  }

  // Redoble AUTOMÁTICO: los últimos 4 pasos de cada 4a repetición de la frase. Es la
  // estructura mínima que hace que 8 compases no sean el mismo compás ocho veces.
  // OJO CON `enRedoble`: el redoble REEMPLAZA a los toms escritos del patron, no se suma a
  // ellos. Varios grooves tienen toms justo en los ultimos pasos (CADENA, PRISMA, CORAL...) y
  // el tom del patron y el del redoble caian en la MISMA ranura monofonica en la misma
  // muestra: el primero se disparaba a amplitud plena y el segundo lo mataba en el acto. Un
  // redoble tampoco se toca asi: el baterista deja de tocar el patron.
  if (FILL_AUTO && !fillHeld && (phraseRep & 3) == 3 && s >= totalSteps - 4) {
    int i = s - (totalSteps - 4);
    const uint8_t TOMS[4] = { 2, 1, 0, 0 };
    trigDecMul = 0.55f;
    trigVoice(T_TOM, (uint8_t)(100 + i * 6), TOMS[i]);
    if (i == 3) trigVoice(T_SNARE, 118, 0);
    trigDecMul = 1.0f;
    ledFlash[3] = 1.0f;
  }
  // Plato al volver del redoble — pero solo si el propio patron no puso uno en el "1": dos
  // platos en la misma ranura y en la misma muestra es el segundo matando al primero.
  if (FILL_AUTO && s == 0 && (phraseRep & 3) == 0 && phraseRep > 0 && patVel[T_METAL][0] == 0)
    trigVoice(T_METAL, 118, 2);

}

// ¿Hay algo que suene en este bloque? Si el beat repeat engancha un tramo vacío, el break se
// convierte en silencio — se busca hacia atrás un bloque que tenga golpes.
bool blockHasHits(int anchor, int len) {
  for (int i = 0; i < len; i++) {
    int s = (anchor + i) % totalSteps;
    for (int t = 0; t < NUM_TRACKS; t++) if (patVel[t][s]) return true;
  }
  return false;
}

void armRepeat(int len) {
  int anchor = masterStep - (masterStep % len);
  for (int tries = 0; tries < 4 && !blockHasHits(anchor, len); tries++)
    anchor = (anchor - len + totalSteps) % totalSteps;
  repeatAnchor = ((anchor % totalSteps) + totalSteps) % totalSteps;
  repeatPos    = 0;
}

void advanceStep() {
  masterStep++;
  if (masterStep >= totalSteps) { masterStep = 0; phraseRep++; }
  if (masterStep % 4 == 0) ledFlash[5] = 1.0f;

  if (repeatLen > 0) {
    // El reloj maestro NO se detiene mientras se loopea: al volver a OFF la máquina retoma
    // en tiempo, nunca desfasada.
    stepTrigger((repeatAnchor + (repeatPos % repeatLen)) % totalSteps);
    repeatPos++;
  } else if (halfHeld) {
    // Cada paso del patrón ocupa dos pasos del maestro: el segundo no dispara nada. El
    // maestro no se detiene, así que al soltar se retoma exactamente donde tocaba.
    halfTick ^= 1;
    if (halfTick) return;
    stepTrigger(halfStep);
    halfStep++; if (halfStep >= totalSteps) halfStep = 0;
  } else {
    stepTrigger(masterStep);
  }
}

// Redoble manual (BTN5): acelera de fusas a semifusas y baja por los toms
void fillHit() {
  fillCount++;
  float prog = (float)fillCount / 20.0f; if (prog > 1.0f) prog = 1.0f;
  uint8_t vel = (uint8_t)(70.0f + prog * 55.0f);
  trigDecMul = 0.40f;                        // golpes cortos: el redoble tiene que ser tenso
  if ((fillCount & 3) == 0) {
    trigVoice(T_SNARE, vel, 0);
    ledFlash[1] = 1.0f;
  } else {
    const uint8_t TOMS[3] = { 2, 1, 0 };
    trigVoice(T_TOM, vel, TOMS[fillCount % 3]);
    ledFlash[3] = 1.0f;
  }
  trigDecMul = 1.0f;
}

// ==============================================================================================
// MASTERIZACIÓN — la cadena de bus, en este orden y por una razón cada eslabón
// ==============================================================================================
// 1) REVERB DE PLACA con send fijo por pista (el bombo va en 0: mojarlo es la forma más
//    rápida de perder el golpe y llenar de barro el sub). Es una placa Schroeder: 4 peines
//    en paralelo con amortiguación en el lazo (cola oscura, no un "shhh" brillante) + 2
//    pasa-todo en serie + un pasa-todo distinto por canal, que es lo que la abre en estéreo.
#define RV_C1  911
#define RV_C2 1063
#define RV_C3 1237
#define RV_C4 1381
#define RV_A1  331
#define RV_A2  191
#define RV_AL   97
#define RV_AR  127
#define RV_PRE 780                       // pre-delay ~18 ms: separa el golpe de la sala

static float rvC1[RV_C1], rvC2[RV_C2], rvC3[RV_C3], rvC4[RV_C4];
static float rvA1[RV_A1], rvA2[RV_A2], rvAL[RV_AL], rvAR[RV_AR];
static float rvPre[RV_PRE];
static int   iC1 = 0, iC2 = 0, iC3 = 0, iC4 = 0, iA1 = 0, iA2 = 0, iAL = 0, iAR = 0, iPre = 0;
static float dC1 = 0, dC2 = 0, dC3 = 0, dC4 = 0;

inline float rvComb(float *buf, int len, int &idx, float &damp, float in, float fb) {
  float y = buf[idx];
  // Amortiguación de la cola. El coeficiente es el que decide dónde deja de brillar la
  // sala: 0.38 daba un pasa-bajos de ~6.8 kHz y la cola se oía como "shhh" encima de los
  // hats (medido: en el kit ABISMO la banda 3.5-8 kHz de la reverb dominaba el timbre que
  // la estaba alimentando). 0.68 lo baja a ~2.7 kHz: sala grande y OSCURA, que es la que
  // suena a disco y no a ruido.
  damp = y * 0.32f + damp * 0.68f;
  buf[idx] = in + damp * fb;
  idx++; if (idx >= len) idx = 0;
  return y;
}

// Pasa-todo de Schroeder. Las DOS ganancias tienen que ser la misma g: con la ida en 1 y la
// realimentación en 0.5 deja de ser un pasa-todo y se vuelve un peine que colorea la cola
// (medido: con las ganancias desparejas la reverb del kit ABISMO sacaba un pico en 3.5-8 kHz
// que no estaba en la señal que la alimentaba).
inline float rvAllpass(float *buf, int len, int &idx, float in) {
  const float g = 0.5f;
  float y = buf[idx];
  float out = y - g * in;
  buf[idx] = in + g * y;
  idx++; if (idx >= len) idx = 0;
  return out;
}

// 2) FILTRO PASA-BAJOS RESONANTE (POT2) — el mismo control que en drum_ruido: 200 Hz hasta
//    abierto del todo, con la resonancia subiendo al cerrarlo y una ENVOLVENTE que abre el
//    corte en cada golpe (el filtro respira con el ritmo; no es un LFO que se mueva solo).
//    Va después de la reverb, así que también filtra la cola: es el filtro del master, y es
//    la forma más directa de oscurecer todo si los agudos molestan.
Biq  gCur, gTgt, gStp;
BiqZ gzL, gzR;

// 3) REALCE FIJO DEL GRAVE: un low-shelf de +3.5 dB en 85 Hz. Es la única EQ del bus y está
//    clavada: levanta el bombo y el sub JUNTOS, que es donde vive el peso de este instrumento,
//    sin tocar nada más. Un shelf de 3.5 dB no tiene resonancia ni pico, así que no puede sonar
//    mal en ninguna posición — y no hay pot que lo mueva.
Biq  bassShelf;
BiqZ bsL, bsR;
const float SHELF_GRAVE_DB = 3.5f;
const float SHELF_GRAVE_HZ = 85.0f;

// 4) COMPRESOR DE BUS / glue, también FIJO. Detector de picos con ataque 8 ms y suelta 140 ms,
//    razón 1:2.2 y makeup calculado. Es lo que "pega" la batería: sin él suenan ocho
//    sintetizadores, con él suena una banda. Medido, deja el factor de cresta en 3.0-3.5.
float compEnv = 0.0f, compGain = 1.0f, compTarget = 1.0f;
float compAtt = 0.0f, compRel = 0.0f;
const float COMP_UMBRAL_DB = -17.0f;
const float COMP_RAZON     =  2.8f;
//    (sin drive: la saturación de este firmware va toda antes del filtro, ver el render)

// 4) PASA-ALTOS DE 30 Hz del master: saca el retumbe inaudible que sólo se come headroom, y
//    PASA-BAJOS DE 13 kHz, que es el techo del instrumento. Nada musical de este kit vive
//    arriba de 13 kHz — el hat más brillante está centrado en 8 — así que todo lo que hay ahí
//    es salpicadura de transitorios (el click del bombo, el snap del clap, los armónicos que
//    genera el saturador del compresor). Es exactamente el filo que molesta, y acá se corta
//    una sola vez para todos en vez de perseguirlo instrumento por instrumento.
Biq  mHP; BiqZ mhL, mhR;
Biq  mLP; BiqZ mlL, mlR;
const float MASTER_LP_HZ = 13000.0f;

// Divisor de bandas del saturador de CUERPO (POT3): los graves y medios aguantan drive y
// engordan, pero saturar los agudos con la misma fuerza es lo que produce el chirrido.
Biq  splitCoef;
BiqZ splitZL, splitZR;

// 5) LIMITADOR de pico (techo 0.92) + saturación suave de seguridad, y el bloqueador de DC:
//    los barridos de pitch dejan offset y el offset se come el rango del limitador.
// El limitador mira 64 muestras HACIA ADELANTE (1.45 ms). Sin lookahead, un limitador siempre
// llega tarde al transitorio: por rápido que sea su ataque, el pico ya pasó, y lo que lo termina
// agarrando es el clipper del final — o sea distorsión en cada golpe fuerte. Con la señal
// retrasada y el detector adelantado, la ganancia ya está abajo cuando el golpe llega: el techo
// se respeta sin recortar nada y la ganancia puede moverse lento (menos modulación).
#define LIM_LOOK 64
float limDlyL[LIM_LOOK], limDlyR[LIM_LOOK];
int   limDlyIdx = 0;
float limEnv = 0.0f, limRel = 0.0f, limGain = 1.0f;
float dbgCut = 0.0f, dbgQ = 0.0f;   // sólo para el simulador
float dcX1L = 0.0f, dcY1L = 0.0f, dcX1R = 0.0f, dcY1R = 0.0f;

// ==============================================================================================
// LOS POTS — uno solo, sin paneles: los mismos cuatro de drum_ruido
// ==============================================================================================
// VOLUMEN · FILTRO · CUERPO · BEAT REPEAT, y la posicion fisica de la perilla ES el valor. No
// hay panel B ni pots congelados: existieron mientras el firmware generaba las notas del bajo,
// y al pasar el bajo al sinte externo se fueron con el.
float potVal[4];

#define pVol   potVal[0]
#define pFilt  potVal[1]
#define pBody  potVal[2]

// ==============================================================================================
// LEDs — 6 SMD de la placa, sólo indicadores
// ==============================================================================================
CRGB leds[NUM_LEDS];
uint32_t lastShow = 0;
float    msgTimer = 0.0f;
uint8_t  msgCount = 0;

void renderLeds() {
  const Kit &k = KITS[kit];
  FastLED.clear();

  if (msgTimer > 0.0f) {
    for (int i = 0; i < msgCount && i < NUM_LEDS; i++)
      leds[i] = CHSV(k.hue, 220, (uint8_t)(255 * msgTimer));
    msgTimer -= 0.05f;
  } else if (repeatZone > 0) {
    // BEAT REPEAT: N LEDs rojos según la división (x2 ... x16)
    for (int i = 0; i < repeatZone && i < NUM_LEDS; i++) leds[i] = CHSV(HUE_RED, 230, 220);
    leds[5] = CHSV(HUE_RED, 180, (uint8_t)(255.0f * ledFlash[5]));
  } else {
    for (int i = 0; i < 5; i++)
      leds[i] = CHSV(k.hue + i * 7, 230, (uint8_t)(255.0f * ledFlash[i]));
    leds[5] = CHSV(k.hue + 40, 170, (uint8_t)(255.0f * ledFlash[5]));
  }
  if (halfHeld) {
    for (int i = 0; i < 5; i++) leds[i].nscale8(120);
    leds[5] = CHSV(HUE_YELLOW, 220, 200);
  }
  if (fillHeld)
    for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB(255, 255, 255).nscale8(180);

  for (int i = 0; i < NUM_LEDS; i++) ledFlash[i] *= 0.72f;
  FastLED.show();
}

// ==============================================================================================
// Botones
// ==============================================================================================
Btn btn[5];
#define DEBOUNCE_MS 25
#define COMBO_MS    50            // ventana para reconocer BTN2+BTN4 como combo
uint32_t lastCombo = 0;

void initButtons() {
  for (int i = 0; i < 5; i++) {
    btn[i].pin = BTN_PIN[i];
    pinMode(btn[i].pin, INPUT_PULLUP);
    btn[i].last = true; btn[i].tDown = 0;
  }
}

uint32_t lastTap = 0;
float    tapAvg  = 0.0f;
uint8_t  tapN    = 0;

// ==============================================================================================
// Audio
// ==============================================================================================
static i2s_chan_handle_t tx_chan;
int16_t audioBuf[BUF_SAMPLES * 2];

void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear    = true;
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = BUF_SAMPLES;
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

// 4 muestras, no 8: cada `analogRead` del S3 cuesta decenas de microsegundos y esto se llama
// una vez por vuelta de la tarea de control. Con las zonas con histeresis de mas abajo, 4
// alcanzan de sobra para que el ruido del ADC no haga saltar nada.
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) sum += analogRead(pin);
  return (float)(sum >> 2) / 4095.0f;
}

// Zonas con histéresis: el ruido del ADC no debe hacer saltar de zona por sí solo.
int zonaPot(float v, int zonaActual, int nZonas) {
  float ancho = 1.0f / (float)nZonas;
  float hyst  = ancho * 0.22f;
  int z = zonaActual;
  while (z < nZonas - 1 && v > (float)(z + 1) * ancho + hyst) z++;
  while (z > 0         && v < (float)z * ancho - hyst)        z--;
  return z;
}

// PANEL A · POT4 -> BEAT REPEAT (igual que en drum_ruido)
void updateRepeat(float v) {
  const float TH[4] = { 0.14f, 0.34f, 0.54f, 0.74f };
  const float HYST  = 0.04f;
  int z = repeatZone;
  while (z < 4 && v > TH[z] + HYST)     z++;
  while (z > 0 && v < TH[z - 1] - HYST) z--;
  if (z == repeatZone) return;

  repeatZone = z;
  const int LEN[5] = { 0, 8, 4, 2, 1 };
  reqRepeatLen = LEN[z];       // lo aplica la tarea de audio: armRepeat() lee masterStep
}

// ==============================================================================================
// setup
// ==============================================================================================
void pasoControl();
void renderBuffer();
#ifndef SIMULADOR
void audioTask(void *);
void controlTask(void *);
#endif

void setup() {
  for (int i = 0; i <= 256; i++) sineLUT[i] = sinf(2.0f * (float)M_PI * (float)i / 256.0f);
  for (int i = 0; i < SEMI_N; i++) semiLUT[i] = powf(2.0f, (float)(i + SEMI_MIN) / 12.0f);

  initButtons();
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i < MAX_VOICES; i++) { voices[i].active = false; voices[i].type = 0; voices[i].slot = -1; voices[i].dying = false; }
  for (int i = 0; i < NUM_SLOTS; i++) slotVoice[i] = -1;
  for (int i = 0; i < NUM_LEDS; i++) ledFlash[i] = 0.0f;

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHT);
  FastLED.clear();
  FastLED.show();

  // Semilla de la humanización: el ruido del ADC alcanza y sobra
  uint32_t seed = 0;
  for (int i = 0; i < 16; i++) seed = seed * 31u + (uint32_t)analogRead(POT4);
  prng ^= seed | 1u;

  // MIDI DIN-5: sólo TX. El RX iría en el 44, que es el BTN1, así que se deja sin abrir.
  MIDIOUT.begin(MIDI_BAUD, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);
  reqMidiStart = true;                               // arranca el secuenciador del sinte externo

  cargarGroove(groove);
  setBPM((float)GROOVES[groove].bpm);
  stepLen = duracionPaso();
  afinarSub();

  coefApagado = expf(-1.0f / (0.0015f * SAMPLE_RATE));   // fundido final de voz: tau 1.5 ms
  duckCoef = decCoef(0.090f);                        // sidechain: ~90 ms de recuperación
  compAtt  = expf(-1.0f / (0.008f * SAMPLE_RATE));   // ataque 8 ms
  compRel  = expf(-1.0f / (0.140f * SAMPLE_RATE));   // suelta 140 ms
  limRel   = expf(-1.0f / (0.080f * SAMPLE_RATE));
  for (int i = 0; i < LIM_LOOK; i++) { limDlyL[i] = 0.0f; limDlyR[i] = 0.0f; }
  makeHPF(mHP, 30.0f, 0.707f);
  makeLowShelf(bassShelf, SHELF_GRAVE_HZ, SHELF_GRAVE_DB, 0.8f);
  biqClear(bsL); biqClear(bsR);
  makeLPF(mLP, MASTER_LP_HZ, 0.707f);
  biqClear(mlL); biqClear(mlR);
  makeLPF(splitCoef, 2200.0f, 0.707f);
  makeLPF(gCur, 19000.0f, 0.71f);
  gTgt = gCur;
  gStp.b0 = gStp.b1 = gStp.b2 = gStp.a1 = gStp.a2 = 0.0f;
  biqClear(gzL); biqClear(gzR); biqClear(splitZL); biqClear(splitZR);
  filtEnvCoef = expf(-(float)BUF_SAMPLES / (0.085f * SAMPLE_RATE));   // por buffer

  // Los pots arrancan con su POSICIÓN FÍSICA: la perilla es el valor (como drum_ruido)
  for (int i = 0; i < 4; i++) potVal[i] = readPot(POT_PIN[i]);
  updateRepeat(potVal[3]);
  biqClear(mhL);   biqClear(mhR);

  for (int i = 0; i < RV_C1; i++) rvC1[i] = 0.0f;
  for (int i = 0; i < RV_C2; i++) rvC2[i] = 0.0f;
  for (int i = 0; i < RV_C3; i++) rvC3[i] = 0.0f;
  for (int i = 0; i < RV_C4; i++) rvC4[i] = 0.0f;
  for (int i = 0; i < RV_A1; i++) rvA1[i] = 0.0f;
  for (int i = 0; i < RV_A2; i++) rvA2[i] = 0.0f;
  for (int i = 0; i < RV_AL; i++) rvAL[i] = 0.0f;
  for (int i = 0; i < RV_AR; i++) rvAR[i] = 0.0f;
  for (int i = 0; i < RV_PRE; i++) rvPre[i] = 0.0f;

  i2s_init();

#ifndef SIMULADOR
  // ============================================================================================
  // LAS DOS TAREAS, y esta es LA correccion del chasquido.
  //
  // Antes todo vivia en `loop()`: render de audio, 8 lecturas de ADC y `FastLED.show()`. El
  // render tiene que entregar 128 muestras cada 2.9 ms y la cola del DMA son 4 descriptores
  // (~12 ms); cuando el patron es denso (OFRENDA tiene ~25 golpes por compas) o hay un redoble,
  // el render sube y la vuelta entera se pasa del presupuesto. El DMA se queda sin datos, el
  // driver esta en `auto_clear` y saca CEROS: eso es el chasquido, y por eso aparecia "a veces",
  // en los patrones rapidos y en los fills. No era la envolvente ni el robo de voz (medidos en
  // simulacion: 0 robos y 0 cortes secos incluso con el redoble sostenido en los 12 grooves).
  //
  // Ahora el audio tiene el core 1 para el solo y los controles corren en el core 0. El ADC y
  // los LEDs ya no pueden robarle tiempo al DAC.
  xTaskCreatePinnedToCore(audioTask,   "audio",   8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask, "control", 4096, NULL,  3, NULL, 0);
#endif
}

// ==============================================================================================
// La tarea de CONTROL — botones, pots y LEDs. Corre en el core 0 a 1 kHz y NO toca el
// secuenciador ni las voces: para eso deja pedidos que la tarea de audio aplica.
// ==============================================================================================
void pasoControl() {
  uint32_t now = millis();

  // -- BOTONES (flanco de presion: accion inmediata) ------------------------
  // Un boton, una funcion. Los cinco son exactamente los de drum_ruido y no hay combos: el
  // BTN2+BTN4 que abria el panel del bajo se fue junto con el bajo.
  for (int i = 0; i < 5; i++) {
    bool lvl = digitalRead(btn[i].pin);
    if (lvl == LOW && btn[i].last == HIGH && (now - btn[i].tDown) > DEBOUNCE_MS) {
      btn[i].tDown = now;
      switch (i) {
        case 0:                                     // BTN1 — GROOVE
          reqGroove = true;
          msgCount = ((groove + 1) % NUM_LEDS) + 1; msgTimer = 1.0f;
          break;
        case 1:                                     // BTN2 — KIT
          reqKit = true;
          msgCount = ((kit + 1) % NUM_KITS) + 1; msgTimer = 1.0f;
          break;
        case 2: {                                   // BTN3 — TAP TEMPO
          uint32_t dt = now - lastTap;
          if (lastTap == 0 || dt > 2000) {          // primer tap de la serie: reubica el "1"
            tapN = 0; tapAvg = 0.0f;
            reqReubicar = true;
          } else if (dt > 150) {
            tapAvg = (tapN == 0) ? (float)dt : (tapAvg * 0.6f + (float)dt * 0.4f);
            tapN++;
            reqBPM = 60000.0f / tapAvg;
          }
          lastTap = now;
          break;
        }
        case 3: halfHeldReq = true; break;          // BTN4 — MEDIO TIEMPO (mantener)
        case 4: fillHeldReq = true; break;          // BTN5 — REDOBLE (mantener)
      }
    }
    if (lvl == HIGH && btn[i].last == LOW) {
      if (i == 3) halfHeldReq = false;              // al soltar retoma el maestro: sin desfase
      if (i == 4) fillHeldReq = false;              // el plato de cierre lo dispara el audio
    }
    btn[i].last = lvl;
  }

  // -- POTS (uno por vuelta, suavizado) -------------------------------------
  static uint8_t potIdx = 0;
  potIdx = (potIdx + 1) & 3;
  {
    float v = readPot(POT_PIN[potIdx]);
    potVal[potIdx] += (v - potVal[potIdx]) * 0.25f;
    if (potIdx == 3) updateRepeat(potVal[3]);       // POT4 = BEAT REPEAT
  }

  // -- TRANSPORTE MIDI ------------------------------------------------------
  // El reloj se CUENTA con el contador de muestras del audio (asi no puede irse de fase con la
  // bateria) pero se MANDA desde aca, a 1 kHz, que es tres veces mejor resolucion que el buffer
  // de audio: mandarlo desde el render metia +-2.9 ms de fluctuacion en cada pulso.
  if (ENVIAR_MIDI_CLOCK) {
    static uint32_t mAnt = 0;
    static float    clkAcc = 0.0f;
    if (reqMidiStart) {                             // se reubico el "1": realinear el sinte
      reqMidiStart = false;
      MIDIOUT.write(MIDI_STOP);
      MIDIOUT.write(MIDI_START);
      clkAcc = 0.0f;
      mAnt = muestrasReloj;
    }
    uint32_t m = muestrasReloj;
    clkAcc += (float)(uint32_t)(m - mAnt);
    mAnt = m;
    float per = stepSamples / 6.0f;                 // 24 PPQN = 6 pulsos por semicorchea
    int guarda = 0;                                 // por si el tempo salta: nunca inundar el UART
    while (clkAcc >= per && guarda++ < 8) { clkAcc -= per; MIDIOUT.write(MIDI_CLOCK); }
    if (clkAcc > per * 8.0f) clkAcc = 0.0f;
  }

  // -- LEDs (20 FPS) --------------------------------------------------------
  if (now - lastShow >= 50) { lastShow = now; renderLeds(); }
}

// ==============================================================================================
// La tarea de AUDIO — un buffer de 128 muestras por vuelta (~2.9 ms). Acá NO hay ADC, ni LEDs,
// ni Serial: sólo el secuenciador, las voces y la cadena de master.
// ==============================================================================================
void renderBuffer() {
  // -- PEDIDOS del otro núcleo, aplicados en el borde del buffer ------------
  if (reqGroove) {
    reqGroove = false;
    groove = (groove + 1) % NUM_GROOVES;
    cargarGroove(groove);
    setBPM((float)GROOVES[groove].bpm);
    masterStep = totalSteps - 1;                     // el próximo paso es el "1"
    stepLen = duracionPaso(); stepAcc = stepLen;
    halfStep = 0; halfTick = 1; phraseRep = 0;
    afinarSub();
    reqMidiStart = true;             // que el secuenciador del sinte arranque en el nuevo "1"
  }
  if (reqKit)   { reqKit = false;   kit = (kit + 1) % NUM_KITS; }
  if (reqUnKit) { reqUnKit = false; kit = (uint8_t)((kit + NUM_KITS - 1) % NUM_KITS); }
  if (reqBPM > 0.0f) { setBPM(reqBPM); reqBPM = 0.0f; }
  if (reqReubicar) {
    reqReubicar = false;
    masterStep = totalSteps - 1;
    stepLen = duracionPaso(); stepAcc = stepLen;
    reqMidiStart = true;
  }
  if (reqRepeatLen >= 0) {
    repeatLen = reqRepeatLen; reqRepeatLen = -1;
    if (repeatLen > 0) armRepeat(repeatLen);
  }

  // MEDIO TIEMPO: el flanco lo detecta acá, no la tarea de control, porque toca masterStep
  if (halfHeldReq != halfHeld) {
    halfHeld = halfHeldReq;
    if (halfHeld) { halfStep = masterStep; halfTick = 1; }
  }
  // REDOBLE: idem, y al soltar cierra con el plato — que es un disparo de voz
  if (fillHeldReq != fillHeld) {
    fillHeld = fillHeldReq;
    if (fillHeld) { fillT0 = millis(); fillCount = 0; fillAcc = 0.0f; }
    else { trigVoice(T_METAL, 122, 2); trigVoice(T_SUB, 110, 0); }
  }

  uint32_t now = millis();

  // -- RELOJ (libre: la máquina siempre suena; para silenciar está el POT1) -
  stepAcc += BUF_SAMPLES;
  while (stepAcc >= stepLen) { stepAcc -= stepLen; advanceStep(); stepLen = duracionPaso(); }

  muestrasReloj += BUF_SAMPLES;      // el contador con el que la otra tarea manda el MIDI Clock

  // Redoble manual: acelera de fusas a semifusas
  if (fillHeld) {
    float prog = (float)(now - fillT0) / 900.0f; if (prog > 1.0f) prog = 1.0f;
    float per  = stepSamples * (0.5f - prog * 0.25f);
    const float PISO = 0.030f * SAMPLE_RATE;     // 33 golpes/s: más rápido ya es un zumbido
    if (per < PISO) per = PISO;
    fillAcc += BUF_SAMPLES;
    while (fillAcc >= per) { fillAcc -= per; fillHit(); }
  }

  // -- FILTRO GLOBAL (POT2), igual que en drum_ruido ------------------------
  // Curva 200 Hz -> 19 kHz con el exponente comprimido (pFilt^0.55), para que a mitad del
  // recorrido el corte ya esté sobre los 3 kHz. Con una exponencial pura el punto medio caía
  // en 1.4 kHz y los hats desaparecían. Cada golpe con pegada abre el corte, y el efecto es
  // más profundo cuanto más cerrado está el filtro (arriba ya no hay nada que abrir).
  // La envolvente abre el corte en cada golpe, pero MUCHO menos que en drum_ruido (1.2 en vez
  // de 3.2): con 3.2, cada golpe disparaba el corte hasta 3 veces más arriba, o sea un pico de
  // brillo por golpe — que es exactamente lo que se sentía como filo. Ahora respira, no muerde.
  float envOpen = 1.0f + filtEnv * 1.2f * (1.0f - pFilt);
  float cut = 200.0f * powf(95.0f, powf(pFilt, 0.55f)) * envOpen;
  // Tope en 15 kHz, no en 19: un biquad RBJ diseñado tan cerca de Nyquist (19 de 22.05 kHz)
  // deja de ser transparente y hunde la banda de 8-16 kHz. Medido: con el tope en 19 kHz, el
  // pot ARRIBA sonaba más oscuro que a 3/4 de recorrido — el control se leía al revés. Y de
  // todas formas el master ya tiene su pasa-bajos en 13 kHz: más arriba no hay nada que abrir.
  if (cut > 15000.0f) cut = 15000.0f;
  // LA RESONANCIA DEPENDE DEL CORTE, NO DEL POT. En drum_ruido la resonancia sube apenas se
  // baja el pot, y con esta curva el corte a mitad de recorrido está en 5 kHz y a 3/4 en
  // 10 kHz: o sea que se metía un pico resonante justo en la zona que molesta (medido: cerrar
  // el pot de 1.00 a 0.75 SUBÍA 3 dB la banda de 8-20 kHz en vez de bajarla). Acá la
  // resonancia sólo entra cuando el corte ya bajó de 2.5 kHz, que es donde un barrido canta
  // en vez de doler. Arriba de eso el filtro es transparente.
  float qg = 0.71f;
  if (cut < 2500.0f) {
    float t = 1.0f - cut / 2500.0f;
    qg = 0.71f + t * t * 2.50f;                      // hasta ~3.2 con el filtro abajo
  }
  if (pFilt > 0.97f) { cut = 15000.0f; qg = 0.71f; } // tope = abierto del todo

  // El CORTE se suaviza (no sólo los coeficientes, que además se interpolan muestra a muestra
  // más abajo). Un corte que salta cada 2.9 ms modula el filtro a 345 Hz, y un filtro modulado
  // a escalones genera productos propios. Con este suavizado (tau ~8 ms) la envolvente sigue
  // abriendo el filtro golpe a golpe, pero por una rampa.
  static float cutSmooth = 15000.0f;
  cutSmooth += (cut - cutSmooth) * 0.30f;
  dbgCut = cutSmooth; dbgQ = qg;
  makeLPF(gTgt, cutSmooth, qg);
  float qComp = 1.0f / powf(qg, 0.35f);              // el pico resonante no debe clipear
  filtEnv *= filtEnvCoef;
  const float invBuf = 1.0f / (float)BUF_SAMPLES;
  gStp.b0 = (gTgt.b0 - gCur.b0) * invBuf; gStp.b1 = (gTgt.b1 - gCur.b1) * invBuf;
  gStp.b2 = (gTgt.b2 - gCur.b2) * invBuf; gStp.a1 = (gTgt.a1 - gCur.a1) * invBuf;
  gStp.a2 = (gTgt.a2 - gCur.a2) * invBuf;

  // -- CUERPO (POT3): saturación en DOS BANDAS -----------------------------
  // Los graves y medios aguantan drive y ENGORDAN; los agudos casi no se tocan, porque
  // saturarlos con la misma fuerza es exactamente lo que produce el chirrido. Acá el tope
  // del grave es x3.6 (más bajo que en drum_ruido) y el del agudo x1.2: esto suma peso,
  // no suciedad.
  float driveLo = 0.75f + pBody * pBody * 2.85f;     // 0.75 -> 3.6
  float driveHi = 0.75f + pBody * pBody * 0.45f;     // 0.75 -> 1.2
  float dComp   = 1.0f / (1.0f + pBody * 0.45f);

  // -- COMPRESOR DE BUS: fijo (el POT3 volvió a ser el cuerpo) --------------
  const float thr    = powf(10.0f, COMP_UMBRAL_DB / 20.0f);
  const float expo   = 1.0f - 1.0f / COMP_RAZON;
  const float makeup = powf(1.0f / thr, expo * 0.72f);
  float outGain = pVol * pVol * 1.05f;   // el limitador en 0.92 es el que fija el nivel de salida
  const float revFb = KITS[kit].revFb;

  // -- Render del buffer ----------------------------------------------------
  for (int n = 0; n < BUF_SAMPLES; n++) {
    // Golpes agendados (redobles, flam, réplicas del clap): con precisión de muestra
    for (int i = 0; i < pendN; i++) {
      if (--pend[i].left <= 0) {
        trigVoice(pend[i].track, pend[i].vel, pend[i].aux);
        pend[i] = pend[pendN - 1]; pendN--; i--;
      }
    }

    float kickL = 0.0f, kickR = 0.0f;      // el bus del bombo no se agacha a sí mismo
    float restL = 0.0f, restR = 0.0f;
    float sendBus = 0.0f;

    for (int i = 0; i < MAX_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;

      // -- Capa TONAL (con modulación de fase para la campana FM) --
      float tone = 0.0f;
      if (v.env > 0.00004f) {
        v.f += (v.fEnd - v.f) * v.fCoef;
        float dt = v.f * (1.0f / SAMPLE_RATE);
        v.ph += dt; if (v.ph >= 1.0f) v.ph -= 1.0f;
        tone = oscWave(v.wave, v.ph, dt);
        if (v.ratio2 > 0.0f) {
          v.ph2 += dt * v.ratio2; if (v.ph2 >= 1.0f) v.ph2 -= 1.0f;
          tone += sineAt(v.ph2) * v.amp2;
        }
        if (v.ratio3 > 0.0f) {
          v.ph3 += dt * v.ratio3; if (v.ph3 >= 1.0f) v.ph3 -= 1.0f;
          tone += sineAt(v.ph3) * v.amp3;
        }
        if (v.sat > 0.0f) tone = softClip(tone * v.sat) * 0.9f;
        tone *= v.env;
      }

      // -- Capa de RUIDO (envolvente propia: la pegada) --
      float nz = 0.0f;
      if (v.nAmt > 0.0f && v.nEnv > 0.00004f) {
        nz = noiseF();
        if (v.useF1) nz = biqProc(v.bq1, v.bz1, nz);
        if (v.useF2) nz = biqProc(v.bq2, v.bz2, nz);
        nz *= v.nEnv * v.nAmt;
      }

      // -- Capa TRANSITORIA (mazo del bombo / crack de la caja / cola del clap) --
      float t3 = 0.0f;
      if (v.t3Amt > 0.0f && v.t3Env > 0.00004f) {
        if (v.t3IsNoise) t3 = biqProc(v.bq2, v.bz2, noiseF());
        else {
          v.t3Ph += v.t3F * (1.0f / SAMPLE_RATE);
          if (v.t3Ph >= 1.0f) v.t3Ph -= 1.0f;
          t3 = sineAt(v.t3Ph);
        }
        t3 *= v.t3Env * v.t3Amt;
      }

      // Envolventes: ataque (apertura del platillo) y después decaimiento. Una voz `dying`
      // salta el ataque: se le congelo el atkInc y hay que dejarla decaer YA.
      if (v.atk < 1.0f && !v.dying) { v.atk += v.atkInc; if (v.atk > 1.0f) v.atk = 1.0f; }
      else { v.env *= v.envCoef; v.nEnv *= v.nCoef; v.t3Env *= v.t3Coef; }

      float s = tone + nz + t3;
      if (v.useLane) s = biqProc(v.lane, v.laneZ, s);   // la banda de la pista
      s *= v.amp * v.atk;

      sendBus += s * v.send;
      // El BOMBO Y EL SUB van al mismo bus, y ese bus NO se agacha a sí mismo. Son un solo
      // instrumento: si el sidechain del bombo agachara al sub justo cuando suenan juntos (que
      // es en cada apoyo), se estaría comiendo el peso que el sub existe para dar.
      if (v.type == T_KICK || v.type == T_SUB) { kickL += s * v.lg; kickR += s * v.rg; }
      else                                     { restL += s * v.lg; restR += s * v.rg; }

      // APAGADO DE LA VOZ, EN DOS TIEMPOS — y el segundo es la correccion del chasquido.
      //
      // El umbral de -48 dB (0.004) es el bueno para LIBERAR la voz: con -64 dB una cola de
      // tau 1.5 s deja la voz ocupada 11 s despues de dejar de oirse y con 8 pistas se agota
      // la polifonia tocando normal. Pero soltar la voz ahi mismo es CORTAR LA ONDA EN SECO a
      // -48 dB, y un escalon es un clic de banda ancha: medido, TODAS las voces terminaban con
      // un salto de entre -62 y -46 dBFS, y en el cierre de la frase (donde el redoble
      // automatico, el plato y las colas del patron se apagan casi juntos) varios de esos
      // escalones caen encima. Eso era el chasquido intermitente al final del patron.
      //
      // Ahora, al cruzar el umbral, la voz no se libera: se le acelera la envolvente a 1.5 ms
      // de tau y se la deja morir hasta -82 dB (0.00008), que son ~6 ms mas de ocupacion —
      // nada al lado de una cola — y deja el escalon 34 dB mas abajo, o sea inaudible.
      if (!v.dying && v.atk >= 1.0f && v.env < 0.004f && v.nEnv < 0.004f && v.t3Env < 0.004f) {
        v.dying = true;
        if (coefApagado < v.envCoef) v.envCoef = coefApagado;   // solo acelera: nunca alarga
        if (coefApagado < v.nCoef)   v.nCoef   = coefApagado;
        if (coefApagado < v.t3Coef)  v.t3Coef  = coefApagado;
        v.fCoef = 0.0f;
      } else if (v.dying && v.env < 0.00008f && v.nEnv < 0.00008f && v.t3Env < 0.00008f) {
        v.active = false;
        if (v.slot >= 0 && slotVoice[v.slot] == (int8_t)i) slotVoice[v.slot] = -1;
        v.slot = -1;
      }
    }

    // SIDECHAIN: el bombo agacha el resto ~2.5 dB. Es lo que hace que el bombo se SIENTA
    // en vez de competir, y lo que le hace hueco al 808 y al bajo del MIDI.
    duckEnv *= duckCoef;
    float duck = 1.0f - 0.25f * duckEnv;
    float l = kickL + restL * duck;
    float r = kickR + restR * duck;

    // -- REVERB de placa (mono a la entrada, estéreo a la salida) --
    float rin = rvPre[iPre];
    rvPre[iPre] = sendBus;
    iPre++; if (iPre >= RV_PRE) iPre = 0;
    float rv = rvComb(rvC1, RV_C1, iC1, dC1, rin, revFb)
             + rvComb(rvC2, RV_C2, iC2, dC2, rin, revFb)
             + rvComb(rvC3, RV_C3, iC3, dC3, rin, revFb)
             + rvComb(rvC4, RV_C4, iC4, dC4, rin, revFb);
    // Normalizada por la ganancia de los peines: cada peine con realimentación fb tiene ganancia
  // 1/(1-fb) en sus picos, así que sin esto la sala CRECE al subir revFb y en un patrón denso
  // florece hasta comerse el limitador — se oye como si todo se ensuciara de golpe.
  rv *= 0.25f * (1.0f - revFb);
    rv = rvAllpass(rvA1, RV_A1, iA1, rv);
    rv = rvAllpass(rvA2, RV_A2, iA2, rv);
    l += rvAllpass(rvAL, RV_AL, iAL, rv);
    r += rvAllpass(rvAR, RV_AR, iAR, rv);

    // -- Realce fijo del grave (+3.5 dB en 85 Hz): va ANTES de la saturación y del filtro, para
    //    que el peso que se agrega también pase por el saturador y engorde con él --
    l = biqProc(bassShelf, bsL, l);
    r = biqProc(bassShelf, bsR, r);

    // -- CUERPO: saturación en dos bandas (POT3) --
    float loL = biqProc(splitCoef, splitZL, l);
    float loR = biqProc(splitCoef, splitZR, r);
    l = (softClip(loL * driveLo) + softClip((l - loL) * driveHi) * 0.9f) * dComp;
    r = (softClip(loR * driveLo) + softClip((r - loR) * driveHi) * 0.9f) * dComp;

    // -- FILTRO GLOBAL (POT2): coeficientes interpolados MUESTRA A MUESTRA. Recalcularlos una
    //    vez por buffer hace que el corte salte cada 128 muestras y eso se oye como chasquidos
    //    al mover el pot.
    gCur.b0 += gStp.b0; gCur.b1 += gStp.b1; gCur.b2 += gStp.b2;
    gCur.a1 += gStp.a1; gCur.a2 += gStp.a2;
    l = biqProc(gCur, gzL, l) * qComp;
    r = biqProc(gCur, gzR, r) * qComp;

    // -- COMPRESOR DE BUS (glue) + drive suave --
    float pk = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
    if (pk > compEnv) compEnv = pk + (compEnv - pk) * compAtt;
    else              compEnv = pk + (compEnv - pk) * compRel;
    // La ganancia objetivo se recalcula cada 16 muestras (un powf por muestra sería carísimo),
    // pero NO se aplica en escalones: se suaviza con un polo de ~1 ms. Un escalón cada 16
    // muestras es una modulación a 2.75 kHz, y una modulación inyecta bandas laterales en toda
    // la zona aguda — medido, era eso lo que hacía que cerrar el filtro SUBIERA los agudos.
    if ((n & 15) == 0) {
      compTarget = 1.0f;
      if (compEnv > thr && expo > 0.001f) compTarget = powf(thr / compEnv, expo);
    }
    compGain += (compTarget - compGain) * 0.022f;
    // OJO: acá NO hay saturación, y es a propósito. TODA la distorsión de este firmware vive
    // ANTES del filtro global (la saturación por voz y la de dos bandas del POT3), porque una
    // no linealidad DESPUÉS del filtro vuelve a inyectar agudos que el filtro ya no puede
    // quitar: el pot dejaría de poder oscurecer del todo. El compresor acá sólo aplica
    // ganancia.
    l = l * compGain * makeup;
    r = r * compGain * makeup;

    // -- Pasa-altos de 30 Hz del master --
    l = biqProc(mHP, mhL, l);
    r = biqProc(mHP, mhR, r);

    // -- Bloqueador de DC (un polo a ~20 Hz) --
    float yl = l - dcX1L + 0.9985f * dcY1L; dcX1L = l; dcY1L = yl; l = yl;
    float yr = r - dcX1R + 0.9985f * dcY1R; dcX1R = r; dcY1R = yr; r = yr;

    // -- LIMITADOR de pico con lookahead (techo 0.92) --
    float pk2 = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
    if (pk2 > limEnv) limEnv = pk2;
    else              limEnv = pk2 + (limEnv - pk2) * limRel;
    float limObj = (limEnv > 0.92f) ? (0.92f / limEnv) : 1.0f;
    limGain += (limObj - limGain) * 0.020f;          // ~1.1 ms, más lento que el lookahead
    float dl = limDlyL[limDlyIdx], dr = limDlyR[limDlyIdx];
    limDlyL[limDlyIdx] = l; limDlyR[limDlyIdx] = r;
    limDlyIdx++; if (limDlyIdx >= LIM_LOOK) limDlyIdx = 0;
    l = dl * limGain; r = dr * limGain;

    // -- Pasa-bajos de 13 kHz del master: EL ÚLTIMO ESLABÓN, y va acá por la misma razón por la
    //    que la saturación va antes del filtro. Un limitador multiplica por una ganancia que se
    //    mueve, y multiplicar es modular: genera bandas laterales, y algunas caen arriba de
    //    13 kHz. Si el pasa-bajos va ANTES del limitador, esa basura ya no tiene quién la filtre
    //    y se oye como crujido en los transitorios. Medido a 15.5 kHz respecto del grave:
    //    -50 dB con el filtro antes del limitador, -78 dB con el filtro después.
    l = biqProc(mLP, mlL, l);
    r = biqProc(mLP, mlR, r);

    // -- Volumen + techo final (lineal salvo en el último 5 %) --
    l = techo(l * outGain);
    r = techo(r * outGain);

    int32_t li = (int32_t)(l * 30000.0f);
    int32_t ri = (int32_t)(r * 30000.0f);
    if (li >  32767) li =  32767; if (li < -32768) li = -32768;
    if (ri >  32767) ri =  32767; if (ri < -32768) ri = -32768;
    audioBuf[n * 2]     = (int16_t)li;
    audioBuf[n * 2 + 1] = (int16_t)ri;
  }

  gCur = gTgt;                       // cierra la interpolación del filtro en el borde exacto

  size_t written;
  i2s_channel_write(tx_chan, audioBuf, sizeof(audioBuf), &written, portMAX_DELAY);
}

// ==============================================================================================
// Las dos tareas (y el `loop()` de Arduino, que acá no hace nada)
// ==============================================================================================
#ifdef SIMULADOR
void loop() { pasoControl(); renderBuffer(); }
#else
void audioTask(void *)   { for (;;) renderBuffer(); }               // core 1, prioridad 10
void controlTask(void *) { for (;;) { pasoControl(); vTaskDelay(1); } }   // core 0, 1 kHz
void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }
#endif
