// ==============================================================================================================================================
// PERCU-SYNTH — INDUSTRIAL MATRIZ (base de bombo + bajo, secuenciador industrial en paralelo, visuales en matriz 32×8) — GC Lab Chile
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
// - 6 LEDs WS2812 SMD internos de la placa |DATA -> 46| (índices 0..5, indicadores de estado)
// - Matriz WS2812 de 32×8 (256 LEDs) encadenada DESPUÉS de los 6 SMD, mismo pin |DATA -> 46|
//   (índices 6..261 · fuente de 5 V propia recomendada, GND común con la placa)
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
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
// - FastLED (gestor de librerías de Arduino)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Dos máquinas corriendo al mismo reloj:
//
//   BASE      bombo a negras (el de tres bandas de trance_pistas/drum_poder: fundamental que
//             aterriza en 46 Hz, 2.º armónico para el pecho, mazo de RUIDO pasa-banda) + un BAJO
//             potente (el de bajo_8_pasos: dos sierras PolyBLEP + cuadrada una octava abajo →
//             saturación → SVF con envolvente de filtro). Las líneas de bajo están ESCRITAS
//             para el bombo: ninguna cae en una negra, el bajo vive entre los golpes.
//
//   ARMONÍA  4 BANCOS de dos acordes que se eligen en el menú (Em–C · Cm–B♭ · Bm–G · E–F): el
//             principal dura 6 de cada 8 compases y el segundo 2, para que no se vuelva monótono.
//             El BAJO sigue la fundamental. Un KEYS (cuadrada filtrada, caída larga) toca uno de 8
//             patrones escritos, hipnóticos y oscuros (ritmo y notas juntos, relativos al acorde;
//             BTN5 del menú los recorre), y un ARPEGIO toca el acorde con el ritmo de los hats.
//
//   INDUSTRIAL 6 pistas de percusión con su propio largo: HAT · MARTILLO (golpe sobre plancha) ·
//             PISTÓN (golpe hidráulico grave + escape) · CLAP · HAT ABIERTO · TOM. El ciclo dura
//             16 o 32 pasos, siempre en fase con la base: cambiar de patrón nunca la corre de tiempo
//             (antes había ciclos de 10..24 que se cortaban cada 4 compases y no calzaban). La DENSIDAD (POT4) decide cuántos de sus golpes suenan — subir el
//             pot agrega golpes sin cambiar los que ya estaban. Descartado por sonar mal: parciales
//             metálicos afinados (olla, balón de gas), ráfagas de clics de ruido (clipeo — por eso
//             el clap es UN solo golpe y no las tres palmadas clásicas) y ráfagas de ruido que
//             crecen (ola).
//
// El bombo agacha el bajo y la capa industrial (sidechain). Master: suma → bloqueador DC →
// limitador con lookahead → pasa-bajos 13 kHz → DAC.
//
// MATRIZ 32×8: ocho escenas psicodélicas que se sortean solas cada 4 compases (y en cada Play),
// nunca la misma dos veces seguidas, cada una con paleta, sentido y forma sorteados:
//   TÚNEL · PLASMA · CALEIDOSCOPIO · ESPIRAL · FIGURAS · MOIRÉ · ONDAS · DAMERO
// Todas se mueven en tiempo musical (amarradas a la negra) y reaccionan al bombo, a la
// envolvente del bajo y a cada golpe industrial.
// Al encender, 3 s de PRUEBA DE ORIENTACIÓN (ver README): rojo arriba-izquierda, verde
// arriba-derecha, azul abajo-izquierda y una columna blanca que barre de izquierda a derecha.
//
// ARQUITECTURA: audio en su propia tarea en el CORE 1; botones y pots en una tarea a 1 kHz y
// la matriz en otra de menor prioridad, las dos en el CORE 0. Los controles sólo dejan
// PEDIDOS que el audio aplica en el borde de un buffer. Sin Serial.
// ==============================================================================================================================================
// FUNCIONAMIENTO (un botón = una función, todo en el flanco de presión)
// ==============================================================================================================================================
// - BTN1 (44) → PLAY / STOP. Play arranca en el "1" en la primera muestra.
// - BTN2 (42) → NUEVO PATRÓN INDUSTRIAL (golpes, largo del ciclo y afinación de los metales).
// - BTN3 (0)  → línea de bajo y figura del arpegio nuevas (los acordes y el keys van en el menú).
// - BTN4 (45) → TEMPO (mantener): con BTN4 apretado, el POT1 sube o baja el tempo (70–170 BPM)
//               desde donde estaba — no salta. Al soltar, el volumen queda donde estaba hasta que
//               el POT1 vuelva a pasar por esa posición: soltar el botón nunca cambia el volumen.
// - BTN5 (47) → BREAK: mientras lo mantienes, bombo y bajo se callan y lo demás sigue; al soltar
//               vuelven en tiempo (el reloj nunca se detuvo). Si lo mantienes MÁS DE 2 s empieza la
//               SUBIDA: 16 compases sin bombo ni bajo en los que crecen un ruido blanco y un
//               oscilador agresivo que parte grave y sube 4 octavas; desde el compás 9 un trémolo
//               rítmico en semicorcheas y una caja que crece (negras → corcheas → semicorcheas). Termina
//               en el "1" de un compás con un GOLPE (boom + crash) que deja una cola de reverb corta
//               sobre el silencio; todo lo demás se corta de golpe y espera el próximo toque de BTN5:
//               el DROP, desde el "1" con bombo y bajo (también si lo tocas antes de que termine).
//               LEDs 0 y 1: rojo parpadeando en la subida, rojo fijo esperando el drop.
// - BTN2 + BTN4 a la vez → entra y sale del MENÚ. Adentro: BTN1 Em–C · BTN2 Cm–B♭ · BTN3 Bm–G ·
//               BTN4 E–F · BTN5 el siguiente patrón del keys. Y los POTS cambian de función:
//               POT1 ataque del bajo (1–150 ms) · POT2 caída del bajo (40 ms–1.2 s) · POT3 la
//               variación del patrón de acordes (8: cuánto dura cada acorde y acordes nuevos de la
//               escala) · POT4 OSCILADOR CONTINUO: dos sierras con detune leve + reverb; el pot
//               barre la altura sin escalones a lo largo de dos octavas que empiezan y terminan en la
//               tónica del banco (abajo del todo calla) — para tocar melodías encima. Lo que dejes ahí se queda al salir. Al entrar o salir, cada pot
//               no cambia nada hasta que lo muevas un poco (sin saltos). Como todo botón actúa al
//               apretarlo, el combo DESHACE lo que hizo el primero de los dos (el patrón nuevo de
//               BTN2, o lo que se eligió al salir).
//
// - POT1 (ADC1)  → VOLUMEN master
// - POT2 (ADC2)  → MEZCLA base ↔ industrial (al centro las dos a pleno)
// - POT3 (ADC8)  → CORTE DEL BAJO (55 Hz – 1.6 kHz; la resonancia va atada al corte)
// - POT4 (ADC10) → DENSIDAD industrial (abajo del todo la fábrica calla)
//
// LEDs de la placa: 0 bombo · 1 bajo · 2 hats · 3 martillo/pistón/clap/tom · 4 keys/arpegio ·
// 5 verde = tocando, con el pulso de cada negra. Con BREAK, 0 y 1 en rojo fijo. El LED 5 se
// pone MAGENTA si un bloque de audio usa más del 80 % de su tiempo.
// ==============================================================================================================================================

// El core de Arduino compila con -Os (tamaño). Este sketch es casi todo bucles de audio: con O2 (velocidad)
// el mismo código usa bastante menos CPU por bloque (medido con MEDIR).
#pragma GCC optimize ("O2")
#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <math.h>
#include <string.h>

// ─── Tipos (arriba del todo para que el IDE de Arduino genere bien los prototipos) ───
struct Filtro { float ic1, ic2, a1, a2, a3, k; };              // SVF de Zavalishin (TPT)
struct BQ     { float b0, b1, b2, a1, a2, z1, z2; };           // biquad RBJ (DF2 transpuesta)
struct VozBombo { bool viva; float f, ph, env, knock, click, eC, kC, cC, amp; BQ bp, hp; };
struct VozInd {
  bool  viva;
  float amp, gL, gR;
  float ph[2], inc[2], a[2];         // capa TONAL: hasta dos parciales (cuerpo del martillo)
  float env, eC;
  float caida, caidaC, cuanto;       // caída de pitch del impacto (pocos %, nunca un barrido largo)
  float nz, nC, nAmt;                // capa de RUIDO con su propia envolvente
  float fc0, fc1, barre, barreInc, q;
  Filtro s;
  BQ    banda;                       // pasa-altos de carril: nadie le pisa el grave al bombo
  bool  conBanda;                    // los de puro ruido no lo necesitan: su pasa-banda ya saca el grave
};
struct VozPluck { bool viva, atk, cuad; int nota; float f1, inc1, inv1, amp, fenv, base, oct, q, eC, gL, gR; Filtro s; };   // keys y arpegio
struct Figura { bool viva; float cx, cy, r, vr, brillo; uint8_t tipo, hue; };   // figuras que se abren (escena FIGURAS)

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
const float SR = (float)SAMPLE_RATE;
const float PI_F = 3.14159265f;

// ─── LEDs: 6 SMD de la placa + matriz 32×8 en el mismo pin ──
#define LED_PIN        46
#define STATUS_LEDS     6
#define MATRIZ_ANCHO   32
#define MATRIZ_ALTO     8
#define MATRIZ_LEDS   (MATRIZ_ANCHO * MATRIZ_ALTO)
#define NUM_LEDS      (STATUS_LEDS + MATRIZ_LEDS)
#define LED_BRIGHT    160
#define LED_TYPE      WS2812
#define COLOR_ORDER   GRB
#define LED_MAX_MA   2000                     // tope duro de FastLED (baja el brillo global si se pasa)

// ══════════════════════════════════════════════════════════════════════════════════════════════
// CÓMO ESTÁ CABLEADA LA MATRIZ  ← si la prueba de orientación del arranque sale mal, se toca acá
// ══════════════════════════════════════════════════════════════════════════════════════════════
// Las matrices flexibles de 8×32 vienen casi siempre por COLUMNAS de 8 LEDs en zigzag: el dato
// baja por la primera columna, sube por la segunda, etc. Si al encender la columna blanca no
// barre de izquierda a derecha o los colores de las esquinas no calzan, ajusta estos cuatro.
const bool MATRIZ_POR_COLUMNAS = true;    // false = el dato recorre filas de 32
const bool MATRIZ_SERPENTINA   = true;    // false = todas las columnas (o filas) en el mismo sentido
const bool MATRIZ_ESPEJO_X     = false;   // invierte izquierda ↔ derecha
const bool MATRIZ_ESPEJO_Y     = false;   // invierte arriba ↔ abajo

// ══════════════════════════════════════════════════════════════════════════════════════════════
// PRESUPUESTO DE CORRIENTE DE LA MATRIZ  ← se toca si cambias la alimentación
// ══════════════════════════════════════════════════════════════════════════════════════════════
// 256 LEDs en blanco pleno piden más de 10 A. Colgada del 5 V de la placa o del USB, un frame
// muy encendido hunde el riel y CORTA EL AUDIO (lección de bajo_8_pasos_fsr_clock). El
// limitador baja al instante y sube despacio, porque lo que hunde el riel es el ESCALÓN de
// corriente. Con fuente propia de 5 V / 4 A (GND común + 1000 µF en la entrada de la matriz)
// sube MATRIZ_MAX_MA a 2500.
const float MATRIZ_MAX_MA = 450.0f;
const float MA_POR_LED    = 60.0f;        // un WS2812 en blanco pleno
const float RECUPERA_LED  = 0.06f;        // subida lenta de la ganancia por frame
const float CARGA_MAXIMA  = MATRIZ_MAX_MA / ((float)MATRIZ_LEDS * MA_POR_LED * (LED_BRIGHT / 255.0f));
const unsigned long LED_REFRESH_MS = 20;  // 50 fps (262 LEDs tardan ~8 ms en salir por el cable)
const unsigned long PRUEBA_MS      = 3000;
CRGB leds[NUM_LEDS];

// ─── Botones y pots ────────────────────────────────────────
const uint8_t BTN_PIN[5] = { 44, 42, 0, 45, 47 };   // BTN1..BTN5 (orden correcto de la placa)
const uint8_t POT_PIN[4] = { 1, 2, 8, 10 };
const unsigned long DEBOUNCE_MS = 25;

// ==============================================================================================================================================
// SONIDO — lo fijo se ajusta acá
// ==============================================================================================================================================
// MEDIR 1 = diagnóstico: arranca tocando solo y cada 2 s imprime por Serial0 (el conector UART de
// la placa, el del chip CH343) cuánto CPU usa el audio y cuánto trabaja el limitador. Sirve para
// separar un corte por TIEMPO (el DMA se queda sin datos) de una saturación de NIVEL. Uso normal: 0.
#ifndef MEDIR
#define MEDIR 0
#endif

const float BPM_INICIAL = 128.0f, BPM_MIN = 70.0f, BPM_MAX = 170.0f;
const float MASTER = 0.85f;
const float TECHO  = 0.89f;               // −1 dBFS: techo del limitador
#define LIM_LOOK 64

// BOMBO — tres bandas (trance_pistas / drum_poder): fundamental que aterriza en 46 Hz (el peso),
// 2.º armónico (el pecho) con saturación propia (tiene UNA parcial fuerte), mazo de RUIDO
// pasa-banda de 14 ms y click de 5 ms. Nunca un seno como mazo: eso es un pitido pegado al golpe.
const float NIVEL_BOMBO = 0.78f;
const float K_FREQ = 46.0f, K_RATIO = 4.8f, K_DROP = 0.030f, K_DEC = 0.32f, K_SAT = 1.80f;
const float K_KNOCK = 0.22f, K_KNOCK_F = 1200.0f, K_CLICK = 0.26f;

// BAJO — el de bajo_8_pasos, un poco más de drive para que empuje.
const int   TONICA_BAJO = 33;             // La1 (55 Hz)
const float NIVEL_BAJO = 0.62f, BAJO_DEC = 0.16f, BAJO_SUB = 0.34f, BAJO_DRIVE = 1.55f;
const float BAJO_ENV_OCT = 3.0f;          // cuánto abre el filtro cada nota

// SIDECHAIN del bombo: cuánto agacha a cada capa
const float SC_BAJO = 0.35f, SC_IND = 0.40f;
const float NIVEL_IND = 1.25f;

// ══ ARMONÍA ══ La menor (natural, o el modo que se elija en el menú). Todo se escribe en GRADOS de la escala, así ninguna nota puede
// salir de tono. Una progresión de 4 acordes, uno por compás, sólo con tríadas consonantes
// (0 Am · 2 C · 3 Dm · 4 Em · 5 F · 6 G; el 1 es Si disminuido y no se usa).
#define NUM_MODOS 5
const int ESCALAS[NUM_MODOS][7] = {
  { 0, 2, 3, 5, 7, 8, 10 },   // 1 eólico (menor natural)
  { 0, 2, 3, 5, 7, 9, 10 },   // 2 dórico (sexta mayor)
  { 0, 1, 3, 5, 7, 8, 10 },   // 3 frigio (segunda menor)
  { 0, 2, 3, 5, 7, 8, 11 },   // 4 menor armónica (sensible)
  { 0, 1, 4, 5, 7, 8, 10 },   // 5 frigio dominante (árabe)
};
// Las progresiones van por CARÁCTER, tomado de la armonía de las referencias de Gonzalo
// (Chemical Brothers, Depeche Mode): menor siempre, ritmo armónico lento, y el V MAYOR (Mi con
// sol#) — la dominante de la menor armónica — donde hace falta drama. V_MAYOR se dibuja con su
// tercera mayor y su quinta justa en cualquier modo del menú.
#define V_MAYOR 7
// BANCOS DE ACORDES (los eligió Gonzalo): el menú (BTN2+BTN4) elige uno con BTN1..BTN4. Cada
// banco son DOS acordes: el principal manda y el segundo aparece para que no se vuelva monótono (cuánto
// dura cada uno, y qué acordes nuevos de la escala entran, lo elige la variación: POT3 del menú). La tonalidad
// cambia con el banco (`transp` = semitonos sobre La) y la escala da las notas de paso:
//   BTN1 Em–C  (Mi eólico)     BTN2 Cm–B♭ (Do eólico)
//   BTN3 Bm–G  (Si eólico)     BTN4 E–F   (Mi frigio dominante: el F es la segunda menor)
struct Banco { const char *nombre; int8_t transp; uint8_t escala; int8_t acordes[2]; };
#define NUM_BANCOS 4
const Banco BANCOS[NUM_BANCOS] = {
  { "Em - C",  -5, 0, { 0, 5 } },     // i – VI
  { "Cm - Bb",  3, 0, { 0, 6 } },     // i – VII
  { "Bm - G",   2, 0, { 0, 5 } },     // i – VI
  { "E - F",   -5, 4, { 0, 1 } },     // I – II del frigio dominante
};
// VARIACIONES DEL PATRÓN DE ACORDES (POT3 del menú, 8 zonas): misma escala y mismo estilo — el
// acorde principal manda y los cambios son lentos —, pero cambia cuánto dura cada acorde y entran
// acordes nuevos de la escala. Grado: 0 = principal del banco · S = el segundo acorde del banco ·
// 2 III · 3 iv · 6 VII (en grados de la escala del banco; si en esa escala no da un acorde limpio,
// acordeDelModo lo cambia por el consonante más cercano). La variación 1 es la de siempre.
#define S_BANCO -1
struct SegAc { int8_t grado; uint8_t compases; };
#define NUM_VAR_AC 8
const SegAc VAR_ACORDES[NUM_VAR_AC][5] = {
  { { 0, 6 }, { S_BANCO, 2 } },                                   // 1  P·6 S·2            (la de siempre)
  { { 0, 4 }, { S_BANCO, 4 } },                                   // 2  P·4 S·4
  { { 0, 3 }, { S_BANCO, 1 } },                                   // 3  P·3 S·1            (ciclo de 4)
  { { 0, 6 }, { 3, 1 }, { S_BANCO, 1 } },                         // 4  P·6 iv·1 S·1
  { { 0, 4 }, { S_BANCO, 2 }, { 6, 2 } },                         // 5  P·4 S·2 VII·2
  { { 0, 2 }, { S_BANCO, 2 }, { 0, 2 }, { 3, 2 } },               // 6  P·2 S·2 P·2 iv·2
  { { 0, 5 }, { 2, 1 }, { S_BANCO, 2 } },                         // 7  P·5 III·1 S·2
  { { 0, 8 }, { S_BANCO, 4 }, { 3, 2 }, { S_BANCO, 2 } },         // 8  P·8 S·4 iv·2 S·2   (ciclo de 16)
};
// Panel del menú: rangos del ataque y la caída del bajo
const float BAJO_ATK_MIN = 0.001f, BAJO_ATK_RANGO = 150.0f;   // 1 ms … 150 ms
const float BAJO_DEC_MIN = 0.040f, BAJO_DEC_RANGO = 30.0f;    // 40 ms … 1.2 s
// SUBIDA (BTN5 mantenido más de 2 s): 16 compases sin bombo ni bajo en los que sube un ruido blanco y
// un oscilador agresivo que parte grave; desde el compás 9 se le suma un trémolo rítmico que en los
// compases sigue en semicorcheas y entra una caja que crece; termina en el "1" con un golpe y su cola
// de reverb, se corta TODO lo demás y espera el próximo toque de BTN5, que vuelve desde el "1" con
// bombo y bajo (el drop).
const int   SUBIDA_PASOS      = 256;     // 16 compases de semicorcheas
const float SUBIDA_OCTAVAS    = 4.0f;    // cuánto sube el oscilador
const float NIVEL_SUBIDA_OSC  = 0.22f;
const float NIVEL_SUBIDA_RUIDO = 0.20f;
const float SUBIDA_DRIVE      = 2.5f;    // dos sierras desafinadas y saturadas: agresivo
const float NIVEL_CAJA_SUBIDA = 0.50f;   // la caja que crece desde la mitad (negras → corcheas → semicorcheas)
const float NIVEL_GOLPE       = 0.85f;   // el golpe final (boom grave + crash de ruido)…
const float GOLPE_REVERB      = 0.55f;   // …y cuánto de él va a la reverb
const float REVERB_FB         = 0.84f;   // realimentación de los peines: cola de ~1 s
// OSCILADOR CONTINUO (POT4 del menú): dos sierras PolyBLEP con un detune leve → pasa-bajos fijo
// que sigue a la nota → reverb (la misma del golpe final). La posición del pot ES la altura, SIN
// cuantizar (lo pidió Gonzalo: primero fue cuantizado a la escala): barre dos octavas en semitonos
// continuos, y los dos extremos son la tónica del banco — la nota más grave y la más aguda caen en
// la escala. Abajo del todo calla.
// Fuera del menú sigue sonando la última nota que dejaste (como el resto del panel del menú).
const int   TONICA_OSC    = 57;          // La3 + transp del banco: la nota más grave del pot
const float OSC_RANGO     = 24.0f;       // semitonos del recorrido: dos octavas, tónica a tónica
const float OSC_APAGADO   = 0.05f;       // el primer 5 % del recorrido: silencio
const float NIVEL_OSC     = 0.18f;       // 0.28 era demasiado fuerte y 0.10 quedaba escondido (probado en la placa)
const float OSC_DETUNE    = 1.0035f;     // ±6 cents entre las dos sierras: leve, sólo ancho
const float OSC_GLIDE     = 0.015f;      // suaviza el ruido del ADC y los saltos del pot (s)
const float OSC_ATAQUE    = 0.010f, OSC_SUELTA = 0.30f;   // al salir y entrar a la zona de silencio
// FILTRO MODULADO (lo pidió Gonzalo): pasa-bajos resonante cuyo corte sube y baja con un LFO
// LIBRE de velocidad ALEATORIA: cada pocos segundos sortea una velocidad nueva entre muy lenta y
// muy rápida (repartida en escala logarítmica) y se desliza suave hacia ella — nunca salta. Primero
// fue amarrado al tempo y lo prefirió así, suelto. La resonancia va atada al corte: alta cuando
// está cerrado (el "cuac"), baja cuando abre, para no dejar un pico chillón arriba.
const float OSC_CORTE_MIN = 1.2f;        // cerrado: el corte en 1.2·f (oscuro, la resonancia canta sobre la nota)
const float OSC_LFO_OCT   = 3.5f;        // cuánto abre el LFO: 3.5 octavas sobre el mínimo
const float OSC_LFO_MIN_HZ = 0.08f, OSC_LFO_MAX_HZ = 9.0f;   // de un barrido de 12 s a un trino rápido
const float OSC_LFO_CAMBIO_MIN = 1.5f, OSC_LFO_CAMBIO_MAX = 6.0f; // cada cuánto sortea otra velocidad (s)
const float OSC_LFO_DESLIZ = 1.2f;       // cuánto tarda en llegar a la velocidad nueva (tau, s)
const float OSC_Q_CERRADO = 4.5f, OSC_Q_ABIERTO = 1.2f;
// Espacio del oscilador (Gonzalo lo pidió "más espacial"): cada sierra cargada a un lado, así el
// batido del detune se abre en estéreo; un CHORUS estéreo propio; y un envío al DELAY ping-pong del
// keys (corchea con puntillo, sigue al tempo), que lo repite cruzando de lado. La reverb la cambió
// por delay + chorus: quedó en 0, pero el envío sigue ahí por si se quiere volver a probar.
const float OSC_REVERB    = 0.0f;        // envío a la reverb (0.42 la dejaba ~1 dB bajo el seco)
const float OSC_ECO       = 0.75f;       // envío al delay ping-pong (era 0.40)
const float OSC_CHORUS_MS = 12.0f, OSC_CHORUS_PROF_MS = 4.0f;   // retardo base y cuánto lo mueve el LFO
const float OSC_CHORUS_HZ = 0.35f;       // LFO lento: ondula sin desafinar a la vista
const float OSC_CHORUS_MEZCLA = 0.70f;   // cuánto de las dos voces del chorus se suma al seco
const float OSC_ANCHO     = 0.80f;       // 0 = mono · 1 = cada sierra sólo en su lado
const float OSC_SC        = 0.15f;       // el bombo lo agacha apenas (con 0.25 se escondía)

// Líneas de bajo: 16 caracteres = un compás de semicorcheas. Compás A tres veces, B en el 4.º.
// Ninguna nota cae en una negra (pasos 0, 4, 8, 12): el bajo vive entre los bombos. Los
// caracteres son relativos a la FUNDAMENTAL DEL ACORDE del compás, así el bajo sigue la armonía:
//   x fundamental · 3 tercera · 5 quinta · b séptima MENOR (si en ese acorde la séptima de la
//   escala es mayor — Fa, Do — toca la quinta: una séptima mayor en el bajo queda medio tono bajo
//   la fundamental que tocan el arpegio y el pluck, y eso se oía desafinado) · o octava · - silencio
#define NUM_LINEAS 6
const char *LINEAS_BAJO[NUM_LINEAS][2] = {
  { "-xxx-xxx-xxx-xxx", "-xxx-xxx-xxo-x5b" },   // rodante
  { "--x---x---x---x-", "--x---x---o---5-" },   // contratiempo
  { "-x-x-x-x-x-x-x-x", "-x-x-x-x-o-o-5-b" },   // semicorcheas sueltas
  { "-xx--xx--xx--xx-", "-xx--xx--oo--55-" },   // galope
  { "--xx--xo--xx--x5", "--xx--xo--xb--3-" },   // riff
  { "-xxo-xx5-xxo-xxb", "-xxo-xx5-xbo-x53" },   // rodante con color
};

// Patrones del KEYS (teclado de sintetizador: cuadrada filtrada con caída larga). 32 caracteres =
// 2 compases que se repiten; BTN5 del menú pasa al siguiente. RITMO y NOTAS van escritos juntos:
//   1..9 una nota, en grados contados desde la fundamental del acorde que suena
//        (1 fundamental · 3 tercera · 5 quinta · 6 sexta menor en los acordes menores · 8 octava)
//   -    silencio (la nota anterior sigue con su caída)
// NUNCA dos notas a la vez (lo pidió Gonzalo): sin acordes en los patrones y el keys es MONOFÓNICO
// — cada nota toma el lugar de la anterior, así ni las colas se enciman. Siempre en secuencia.
// Hipnóticos y oscuros, y se acomodan solos a cada acorde.
// Gonzalo los probó en la placa y se quedó con estos 5; descartó el arpegio a contratiempo, la
// síncopa 3-3-4-3-3 y el dub (los tres que antes eran stabs de acorde).
#define NUM_PAT_KEYS 5
const char *PAT_KEYS[NUM_PAT_KEYS] = {
  "1-1-5-1-1-1-6-5-" "1-1-5-1-8-7-6-5-",   // pulso con la sexta que cae a la quinta
  "3--3--3---3--3--" "3--3--3---2--1--",   // la tercera insistente
  "8--7--6---5--5--" "8--7--6---5--4-3",   // descenso lento
  "1-51-51-1-51-58-" "1-51-51-1-51-36-",   // semicorcheas hipnóticas
  "1-----------3-1-" "5-------5---3-1-",   // una nota larga y una respuesta
};
inline int gradoDe(char c) { return c - '1'; }
const int   TONICA_KEYS = 57;              // La3 + transp del banco, subida de octava si queda bajo KEYS_DESDE
const int   KEYS_DESDE  = 60;              // Do4: en Mi3 (bancos Em–C y E–F) el keys quedaba demasiado grave
const float NIVEL_KEYS = 0.13f, NIVEL_ARP = 0.13f;
// Sonido del KEYS: un oscilador de PULSO con el ancho modulado por un LFO lento (PWM: el timbre
// respira; un segundo oscilador desafinado costaba CPU y el chorus ya da el ancho) → pasa-bajos con envolvente → chorus estéreo → eco ping-pong a
// corchea con puntillo. Sigue siendo una sola nota a la vez: el eco la repite en el tiempo.
const float KEYS_DEC      = 0.20f;   // caída de la nota (tau, s)
const float KEYS_PWM_HZ   = 0.35f;   // velocidad del PWM
const float KEYS_PWM_PROF = 0.20f;   // el ancho va de 0.50 a 0.30
const float KEYS_CORTE    = 4.5f;    // el filtro reposa en 4.5·f… (con 3·f le faltaba brillo)
const float KEYS_ENV_OCT  = 2.5f;    // …y cada nota lo abre 2.5 octavas
const float KEYS_Q        = 1.1f;
const float CHORUS_MS = 7.0f, CHORUS_PROF_MS = 2.5f, CHORUS_HZ = 0.6f, CHORUS_MEZCLA = 0.55f;
const float ECO_FEEDBACK = 0.52f, ECO_MEZCLA = 0.55f;   // eco ping-pong a corchea con puntillo (más presente: lo pidió Gonzalo)
const int TONICA_ARP   = 57;              // La3 + transp del banco

// Figuras del ARPEGIADOR. Cada dígito es una nota del acorde:
// 0 fundamental · 1 tercera · 2 quinta · 3 octava · 4 décima · 5 duodécima
// RITMO: el arpegio toca donde suenan los hats (cerrado y abierto); el texto da el orden de las
// notas, no el ritmo.
#define NUM_ARPS 6
const char *ARPS[NUM_ARPS] = {
  "0123012301230123",   // sube
  "0303030303030303",   // bombeo de octavas
  "0-3-2-3-0-3-2-3-",   // fundamental · octava · quinta en corcheas
  "0213021302130213",   // quebrado
  "3210321032103210",   // baja
  "00-300-200-300-1",   // síncopa sobre la fundamental
};
const int8_t TONOS_ARP[6] = { 0, 2, 4, 7, 9, 11 };   // en grados de la escala sobre la fundamental

// Pistas industriales (todas percusión, con el largo polirrítmico) y los eventos de la parte
// musical, para los LEDs
#define NUM_IND     6
#define I_HAT       0
#define I_MARTILLO  1
#define I_PISTON    2
#define I_CLAP      3
#define I_ABIERTO   4
#define I_TOM       5
#define EV_KEYS    6
#define EV_BOMBO    7
#define EV_BAJO     8
#define EV_ARP      9
#define NUM_EV      10
#define MAX_PASOS_IND 32
// Largo del ciclo industrial: SÓLO 16 o 32 pasos, que caben exactos en los 4 compases de la base.
// Antes había 10, 12, 14, 20 y 24 (polirritmo): como 64 no es múltiplo de ellos, el ciclo se cortaba
// a la mitad cada 4 compases y, al sortear un patrón nuevo, arrancaba en una fase cualquiera — se
// oía como bajo y batería que no calzaban. Ahora la posición se calcula desde el paso de la base.
#define NUM_LARGOS 2
const int   LARGOS_IND[NUM_LARGOS] = { 16, 32 };
const float DENS_PISTA[NUM_IND] = { 0.95f, 0.85f, 0.75f, 0.80f, 0.70f, 0.50f };

// ==============================================================================================================================================
// PEDIDOS ENTRE NÚCLEOS
// ==============================================================================================================================================
// control → audio
volatile bool     reqTocar = false, reqBreak = false;
volatile uint32_t reqPatron = 0, reqLinea = 0, reqDeshacer = 0, reqSemilla = 0x1234567u;
volatile uint32_t reqSubida = 0, reqDrop = 0;
volatile int      reqBanco = 0, reqPatKeys = 0;  // banco de acordes y patrón del keys (menú)
volatile int      reqVarAc = 0;                 // variación del patrón de acordes (POT3 del menú)
volatile float    reqOscPos = -1.0f;            // altura del oscilador (POT4 del menú): 0..1 sobre dos octavas, −1 = calla
volatile float    reqBPM = BPM_INICIAL;
volatile float    pVol = 0.7f, pMez = 0.5f, pCorte = 0.35f, pDens = 0.6f;
// Pots del PANEL DEL MENÚ (se quedan donde los dejaste al salir):
volatile float    pAtkBajo = 0.138f, pDecBajo = 0.408f;   // arrancan en el bajo de siempre (2 ms · 0.16 s)
// audio → LEDs
volatile uint32_t golpes[NUM_EV];
volatile float    velGolpe[NUM_EV];
volatile int      pasoBasePub = 0, pasoIndPub = 0, largoIndPub = 16, lineaPub = 0;
volatile uint32_t contadorPasos = 0;
volatile float    envBajoPub = 0.0f, densPub = 0.6f, bpmPub = BPM_INICIAL;
volatile int      notaBajoPub = 0, notaKeysPub = 45;
volatile bool     tocandoPub = false, breakPub = false, menuPub = false, breakFijoPub = false;
volatile int      estadoSubPub = 0;
volatile int      bancoPub = 0, patKeysPub = 0, varAcPub = 0;
volatile uint32_t cargaAltaHasta = 0;
volatile uint32_t medMax = 0, medSuma = 0, medN = 0, medTecho = 0;   // MEDIR
volatile float    medLimMin = 1.0f, medPico = 0.0f;
float prio[NUM_IND][MAX_PASOS_IND];        // patrón industrial: suena si prio < densidad

// ==============================================================================================================================================
// DSP — utilidades (sin libm por muestra)
// ==============================================================================================================================================
#define TABLA 1024
float SENO[TABLA + 1];
static inline float seno(float fase) { float x = fase * TABLA; int i = (int)x; float f = x - i; return SENO[i] + (SENO[i + 1] - SENO[i]) * f; }
uint32_t semillaRuido = 22222;
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
static inline float cuadrada(float &f, float inc, float inv) { // PolyBLEP = sierra − sierra desfasada
  float p2 = f + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
  float a = 2.0f * f - 1.0f, b = 2.0f * p2 - 1.0f;
  if (f < inc) { float t = f * inv; a -= t + t - t * t - 1.0f; } else if (f > 1.0f - inc) { float t = (f - 1.0f) * inv; a -= t * t + t + t + 1.0f; }
  if (p2 < inc) { float t = p2 * inv; b -= t + t - t * t - 1.0f; } else if (p2 > 1.0f - inc) { float t = (p2 - 1.0f) * inv; b -= t * t + t + t + 1.0f; }
  f += inc; if (f >= 1.0f) f -= 1.0f;
  return (a - b) * 0.5f;
}
static inline void coefFiltro(Filtro &s, float fc, float q) {
  if (fc > 12500.0f) fc = 12500.0f; if (fc < 20.0f) fc = 20.0f;
  float g = tanRapido(PI_F * fc / SR); s.k = 1.0f / q;
  s.a1 = 1.0f / (1.0f + g * (g + s.k)); s.a2 = g * s.a1; s.a3 = g * s.a2;
}
static inline void pasoFiltro(Filtro &s, float x, float &lp, float &bp) {
  float v3 = x - s.ic2, v1 = s.a1 * s.ic1 + s.a2 * v3, v2 = s.ic2 + s.a2 * s.ic1 + s.a3 * v3;
  s.ic1 = 2.0f * v1 - s.ic1; s.ic2 = 2.0f * v2 - s.ic2;
  lp = v2; bp = v1 * s.k;                          // pasa-banda con ganancia 1 en el centro
}
static inline float pasoBQ(BQ &q, float x) { float y = q.b0 * x + q.z1; q.z1 = q.b1 * x - q.a1 * y + q.z2; q.z2 = q.b2 * x - q.a2 * y; return y; }
void bqBPF(BQ &c, float fc, float Q) {
  float w = 2.0f * PI_F * fc / SR, s = sinf(w), co = cosf(w), al = s / (2.0f * Q), a0 = 1.0f + al;
  c.b0 = al / a0; c.b1 = 0.0f; c.b2 = -al / a0; c.a1 = -2.0f * co / a0; c.a2 = (1.0f - al) / a0; c.z1 = c.z2 = 0.0f;
}
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

// Generador de composición (patrones) — separado del ruido de audio
uint32_t rngComp = 0x9E3779B9u;
static inline float frnd() { rngComp ^= rngComp << 13; rngComp ^= rngComp >> 17; rngComp ^= rngComp << 5; return (rngComp >> 8) * (1.0f / 16777216.0f); }
static inline float entre(float a, float b) { return a + (b - a) * frnd(); }

// ==============================================================================================================================================
// ESTADO DE LA SÍNTESIS (lo toca sólo la tarea de audio)
// ==============================================================================================================================================
VozBombo bombos[2]; int sigBombo = 0;              // dos instancias: la que sonaba se calla en 3 ms
struct { float f1, f2, fs, inc1, inc2, incS, inv1, inv2, invS, env, envF; bool atk, atkF; Filtro s; } bajo;
float dBajo, dBajoF, fCoefBombo, dRapido3, dRapido, dSc;
float incAtkBajo = 1.0f / 88.0f, atkBajoS = 0.138f, decBajoS = 0.408f;
uint32_t semRuidoL = 12345u, semRuidoR = 67891u;
// Estado de la subida
#define E_NORMAL 0
#define E_SUBIDA 1
#define E_ESPERA 2
int      estadoSub = E_NORMAL, subPasos = 0;
uint32_t vistoSubida = 0, vistoDrop = 0;
float    subF1 = 0.0f, subF2 = 0.31f, subLp = 0.0f, subTrem = 1.0f, faltanTramo = 0.0f, corteGate = 1.0f;
// Caja de la subida: un solo parche (cuerpo de 2 parciales sin saturar + bordonera de ruido)
struct { float env, eC, nz, nC, ph1, ph2, amp; Filtro s; } cajaSub;
// Golpe final: boom (seno que cae) + crash de ruido + reverb de peines normalizada. Va por un bus
// propio que NO pasa por el corte, así la cola suena sobre el silencio.
struct { bool viva; float f, ph, env, eC, nz, nC; Filtro s; } golpeFin;
#define RV_N1 1116
#define RV_N2 1188
#define RV_N3 1277
#define RV_N4 1356
#define RV_A1 556
#define RV_A2 441
#define RV_A3 579
#define RV_A4 467
float rvC1[RV_N1], rvC2[RV_N2], rvC3[RV_N3], rvC4[RV_N4], rvA1[RV_A1], rvA2[RV_A2], rvA3[RV_A3], rvA4[RV_A4];
int   rvI1 = 0, rvI2 = 0, rvI3 = 0, rvI4 = 0, rvJ1 = 0, rvJ2 = 0, rvJ3 = 0, rvJ4 = 0;
float rvLp1 = 0, rvLp2 = 0, rvLp3 = 0, rvLp4 = 0;
int   reverbViva = 0;                              // muestras que le quedan a la cola (no se calcula de balde)
float golpeL[BUFFER_SAMPLES], golpeR[BUFFER_SAMPLES];
// Oscilador continuo: dos sierras, un filtro, y su envío a la reverb
struct { bool vivo; float f1, f2, inc, incObj, amp, obj, comp; Filtro s, s2; } osc;
// LFO del filtro: fase, velocidad actual y destino (en octavas sobre OSC_LFO_MIN_HZ) y el reloj del próximo sorteo
float lfoFase = 0.0f, lfoVel = 2.0f, lfoVelObj = 2.0f, lfoFalta = 0.0f; uint32_t semLfo = 0x51F15EEDu;
// Chorus del oscilador: un retardo corto leído en dos puntos que se mueven en cuadratura (uno por lado)
#define OSC_CHORUS_N 2048                          // 46 ms: alcanza para 12 ± 4 ms
float oscChorusBuf[OSC_CHORUS_N]; int oscChorusIdx = 0; float oscChorusLfo = 0.0f;
const float octLfo = log2f(OSC_LFO_MAX_HZ / OSC_LFO_MIN_HZ);                      // rango de velocidades, en octavas
const float kDeslizLfo = 1.0f - expf(-16.0f / (OSC_LFO_DESLIZ * SR));   // un filtro por sierra (una por lado)
float oscPosVista = -2.0f; int oscTranspVisto = 99;
float dOscGlide, kOscAtk, kOscSuelta;
float oscL[BUFFER_SAMPLES], oscR[BUFFER_SAMPLES], oscEnvio[BUFFER_SAMPLES], oscEco[BUFFER_SAMPLES];
VozInd ind[NUM_IND][2]; int sigInd[NUM_IND];       // un "parche" por pista con fast-kill al retriggear
struct { float f, r2, r3, pan; } timbre[NUM_IND];
float scObj = 0, sc = 0;

// Secuenciador
bool     aTocando = false, aBreak = false;
float    fundido = 0.0f;                           // play/stop sin clic
float    sps = 0.0f, faltan = 0.0f;                // muestras por semicorchea · hasta el próximo paso
float    bpm = BPM_INICIAL;
int      pasoBase = -1, pasoInd = 0, largoInd = 16, linea = 0;
uint32_t vistoPatron = 0, vistoLinea = 0, vistoDeshacer = 0;
uint32_t muestraGlobal = 0;
// KEYS y ARPEGIO: el mismo motor (cuadrada o sierra → pasa-bajos con envolvente), cada uno con su
// grupo de voces. Voz por altura: re-disparar la misma nota sube desde donde está, nunca un corte.
#define VOCES_ARP   2
VozPluck arps[VOCES_ARP];
// KEYS: MONOFÓNICO — nunca dos notas a la vez; la nueva sube desde donde estaba la anterior (sin clic)
struct { bool viva, atk; float f1, inc1, inv1, amp, fenv, base, eC, lfo; Filtro s; } kv;
#define CHORUS_N 1024
float    chorusBuf[CHORUS_N]; int chorusIdx = 0; float chorusLfo = 0.0f;
#define ECO_MAX 32768                              // 0.74 s: corchea con puntillo hasta 60 BPM
int16_t  ecoL[ECO_MAX], ecoR[ECO_MAX];
int      ecoIdx = 0, ecoN = 15000, ecoN2 = 15000; float ecoXf = 1.0f, ecoLpL = 0, ecoLpR = 0, ecoHpL = 0, ecoHpR = 0;
float    keysBuf[BUFFER_SAMPLES];
float    dPluckFenv;
int      banco = 0, patKeys = 0, varAc = 0, transp = -5, arp = 0, acordeAct = -1, ladoArp = 0;
int      posArp = 0;                               // por dónde va la figura del arpegio (avanza con cada golpe)
uint32_t compasTotal = 0;                          // compases desde el Play (ciclo de los acordes)
float    padL[BUFFER_SAMPLES], padR[BUFFER_SAMPLES];

// Master
float corteS = 0.35f, gBaseS = 1.0f, gIndS = 1.0f, volS = 0.5f, densS = 0.6f;
float dcX1L = 0, dcY1L = 0, dcX1R = 0, dcY1R = 0;
float limDlyL[LIM_LOOK], limDlyR[LIM_LOOK]; int limDlyIdx = 0;
float limEnv = 0.0f, limGain = 1.0f, limRel = 0.0f;
BQ techoL, techoR;

void setBPM(float b) {
  if (b < BPM_MIN) b = BPM_MIN; if (b > BPM_MAX) b = BPM_MAX;
  bpm = b; sps = SR * 60.0f / bpm / 4.0f; bpmPub = b;
}

void prepararSintesis() {
  for (int i = 0; i <= TABLA; i++) SENO[i] = sinf(2.0f * PI_F * i / TABLA);
  fCoefBombo = 1.0f - expf(-1.0f / (K_DROP * SR));
  dRapido3 = decaimiento(0.003f); dRapido = decaimiento(0.0015f); dSc = decaimiento(0.14f);
  dBajo = decaimiento(BAJO_DEC); dPluckFenv = decaimiento(0.07f);
  dBajoF = decaimiento(BAJO_DEC * 0.55f);
  dOscGlide = 1.0f - decaimiento(OSC_GLIDE); kOscAtk = 1.0f - decaimiento(OSC_ATAQUE); kOscSuelta = 1.0f - decaimiento(OSC_SUELTA);
  for (int i = 0; i < 2; i++) { bqBPF(bombos[i].bp, K_KNOCK_F, 0.8f); bqHPF(bombos[i].hp, 3000.0f, 0.7f); }
  for (int i = 0; i < LIM_LOOK; i++) { limDlyL[i] = 0; limDlyR[i] = 0; }
  limRel = expf(-1.0f / (SR * 0.08f));
  bqLPF(techoL, 13000.0f, 0.7071f); bqLPF(techoR, 13000.0f, 0.7071f);   // DESPUÉS del limitador
  setBPM(BPM_INICIAL);
}

// ─── Patrón industrial: sorteo ───────────────────────────────
// Peso de cada posición dentro de un compás de 16 para cada pista. El golpe del paso s suena
// si prio < densidad, con prio = azar / (peso · densidad de la pista): así subir el POT4
// agrega golpes en orden de importancia y nunca cambia los que ya sonaban.
float pesoPaso(int t, int s, int largo) {
  int b = s & 15;
  switch (t) {
    case I_HAT:       return (b & 1) ? 0.85f : ((b & 3) == 2 ? 0.60f : 0.25f);
    case I_MARTILLO:  return (b == 4 || b == 12) ? 1.00f : ((b == 7 || b == 15) ? 0.15f : 0.04f);
    case I_PISTON:     return ((b & 3) == 2) ? 0.70f : ((b & 1) ? 0.25f : 0.08f);
    case I_CLAP:      return (b == 4 || b == 12) ? 0.95f : (b == 15 ? 0.20f : (b == 7 ? 0.12f : 0.03f));
    case I_ABIERTO:   return ((b & 3) == 2) ? 0.85f : 0.04f;
    case I_TOM:       return (b == 10 || b == 13) ? 0.55f : ((b & 3) == 3 ? 0.30f : 0.06f);
  }
  return 0.0f;
}

uint32_t acentoInd[NUM_IND];
// Respaldo del patrón anterior: el combo BTN2+BTN4 deshace el patrón que BTN2 acaba de sortear
float    prioAntes[NUM_IND][MAX_PASOS_IND];
uint32_t acentoAntes[NUM_IND];
int      largoAntes = 16;
void guardarPatron()   { memcpy(prioAntes, prio, sizeof(prio)); memcpy(acentoAntes, acentoInd, sizeof(acentoInd)); largoAntes = largoInd; }
void restaurarPatron() { memcpy(prio, prioAntes, sizeof(prio)); memcpy(acentoInd, acentoAntes, sizeof(acentoInd)); largoInd = largoAntes; largoIndPub = largoInd; }
void nuevoPatronInd(uint32_t semilla) {
  rngComp ^= semilla | 1u;
  int nuevo = LARGOS_IND[(int)(frnd() * NUM_LARGOS) % NUM_LARGOS];
  largoInd = nuevo;
  for (int t = 0; t < NUM_IND; t++) {
    acentoInd[t] = 0;
    for (int s = 0; s < MAX_PASOS_IND; s++) {
      float w = (s < largoInd) ? pesoPaso(t, s, largoInd) * DENS_PISTA[t] : 0.0f;
      float r = frnd();
      prio[t][s] = (w > 0.0f) ? (0.02f + r) / (w * 1.3f) : 9.0f;
      if (frnd() < 0.3f) acentoInd[t] |= (1u << s);
    }
  }
  // Afinación de los metales: cada patrón trae su propia fábrica
  timbre[I_HAT]       = { entre(7200.0f, 9000.0f), 0, 0, 0.62f };
  timbre[I_MARTILLO]  = { entre(170.0f, 215.0f), 1.56f, entre(3.6f, 4.6f), 0.50f };
  timbre[I_PISTON]     = { entre(80.0f, 110.0f), 0, 0, 0.50f };
  timbre[I_CLAP]      = { entre(1000.0f, 1400.0f), 0, 0, 0.50f };
  timbre[I_ABIERTO]   = { entre(6000.0f, 7500.0f), 0, 0, 0.38f };
  timbre[I_TOM]       = { entre(95.0f, 140.0f), 0, 0, 0.50f };
  largoIndPub = largoInd;
}

// ─── Disparos ──────────────────────────────────────────────
void publicarGolpe(int ev, float v) { velGolpe[ev] = v; golpes[ev] = golpes[ev] + 1; }

void golpeBombo(float v) {
  VozBombo &a = bombos[sigBombo]; if (a.viva) { a.eC = a.kC = a.cC = dRapido3; }   // el parche anterior se calla en 3 ms
  sigBombo ^= 1; VozBombo &b = bombos[sigBombo];
  b.f = K_FREQ * K_RATIO; b.ph = 0.0f; b.env = 1.0f; b.knock = 1.0f; b.click = 1.0f; b.amp = v; b.viva = true;
  b.eC = decaimiento(K_DEC); b.kC = decaimiento(0.014f); b.cC = decaimiento(0.005f);
  scObj = 1.0f;
  publicarGolpe(EV_BOMBO, v);
}

void golpeBajo(int nota) {
  float f = hz((float)nota);
  bajo.inc1 = f / SR; bajo.inc2 = bajo.inc1 * 1.00521f; bajo.incS = bajo.inc1 * 0.5f;   // +9 cents · una octava abajo
  bajo.inv1 = 1.0f / bajo.inc1; bajo.inv2 = 1.0f / bajo.inc2; bajo.invS = 1.0f / bajo.incS;
  bajo.atk = true; bajo.atkF = true;               // sube desde donde estaba: sin clic, sin reiniciar la fase
  notaBajoPub = nota - TONICA_BAJO - transp;
  publicarGolpe(EV_BAJO, 1.0f);
}

// Grado de la escala (puede ser negativo) → nota MIDI sobre una tónica
int modo = 0;                                       // modo en uso (lo cambia el menú)
int notaGrado(int tonica, int g) {
  int o = (g >= 0) ? g / 7 : -((-g + 6) / 7);
  return tonica + 12 * o + ESCALAS[modo][g - 7 * o];
}
// ¿La tríada sobre el grado g es consonante en el modo actual? (quinta justa: mayor o menor)
bool triadaConsonante(int g) {
  int f = notaGrado(0, g), t = notaGrado(0, g + 2) - f, q = notaGrado(0, g + 4) - f;
  return q == 7 && (t == 3 || t == 4);
}
// Un acorde que en este modo quedó disminuido o aumentado se cambia por el consonante más
// cercano: primero una tercera abajo (comparte dos notas), después una arriba, después vecinos.
int acordeDelModo(int g) {
  if (g == V_MAYOR) return V_MAYOR;                // la dominante mayor se construye aparte, siempre consonante
  const int8_t PRUEBA[5] = { 0, -2, 2, -1, 1 };
  for (int k = 0; k < 5; k++) { int c = ((g + PRUEBA[k]) % 7 + 7) % 7; if (triadaConsonante(c)) return c; }
  return 0;
}
// Los acordes de Mi, Fa y Sol se toman desde abajo: así las voces se mueven poco entre acordes
inline int raizCercana(int g) { return g >= 4 ? g - 7 : g; }
// Nota del acorde: grado `off` contado desde la fundamental del acorde `raiz`. En el V MAYOR la
// tercera y la quinta se fuerzan a mayor y justa (sol# y si sobre mi), sea cual sea el modo.
int notaAcorde(int tonica, int raiz, int off, bool cercana) {
  bool dom = (raiz == V_MAYOR);
  int g = dom ? 4 : raiz;
  if (cercana) g = raizCercana(g);
  int n = notaGrado(tonica, g + off);
  if (dom) {
    int o = ((off % 7) + 7) % 7, oct = (off - o) / 7, f = notaGrado(tonica, g);
    if (o == 2) n = f + 4 + 12 * oct;
    else if (o == 4) n = f + 7 + 12 * oct;
  }
  return n;
}

void golpeVoz(VozPluck *grupo, int n, int nota, float dec, float baseMul, float oct, float q, float pan, bool cuad) {
  int k = -1;
  for (int i = 0; i < n; i++) if (grupo[i].viva && grupo[i].nota == nota) k = i;          // la misma altura
  if (k < 0) for (int i = 0; i < n; i++) if (!grupo[i].viva) { k = i; break; }              // una libre
  if (k < 0) { float menor = 9.0f; for (int i = 0; i < n; i++) if (grupo[i].amp < menor) { menor = grupo[i].amp; k = i; } }
  VozPluck &v = grupo[k];
  float f = hz((float)nota);
  if (!v.viva) { v.f1 = 0.13f; v.amp = 0.0f; v.s.ic1 = v.s.ic2 = 0.0f; }
  v.inc1 = f / SR; v.inv1 = 1.0f / v.inc1;
  v.nota = nota; v.base = f * baseMul; v.oct = oct; v.q = q; v.fenv = 1.0f; v.atk = true; v.viva = true;
  v.eC = decaimiento(dec); v.cuad = cuad;
  v.gL = sqrtf(1.0f - pan); v.gR = sqrtf(pan);
}


// KEYS: una sola voz. La fase no se reinicia y la amplitud sube desde donde estaba: sin clic.
void golpeKeys(int nota, float vel) {
  float f = hz((float)nota);
  if (!kv.viva) { kv.f1 = 0.13f; kv.amp = 0.0f; kv.s.ic1 = kv.s.ic2 = 0.0f; }
  kv.inc1 = f / SR; kv.inv1 = 1.0f / kv.inc1;
  kv.base = f * KEYS_CORTE; kv.fenv = 1.0f; kv.atk = true; kv.viva = true;
  kv.eC = decaimiento(KEYS_DEC * (0.8f + 0.4f * vel));
  notaKeysPub = nota;
  publicarGolpe(EV_KEYS, vel);
}

// ARPEGIO: notas cortas del acorde, alternando izquierda y derecha
void golpeArp(int nota) {
  ladoArp ^= 1;
  golpeVoz(arps, VOCES_ARP, nota, 0.09f, 1.5f, 2.0f, 0.9f, ladoArp ? 0.28f : 0.72f, false);
  publicarGolpe(EV_ARP, 0.7f);
}


void golpeInd(int t, float vel, bool acento) {
  VozInd &a = ind[t][sigInd[t]];
  if (a.viva) { a.eC = a.nC = dRapido3; }   // fast-kill: el parche anterior se calla en 3 ms
  sigInd[t] ^= 1;
  VozInd &v = ind[t][sigInd[t]];
  memset(&v, 0, sizeof(v));
  v.viva = true;
  float pan = timbre[t].pan;
  const float F = timbre[t].f;
  switch (t) {
    case I_HAT:        // aire comprimido corto: sólo ruido pasa-banda (nunca pasa-altos pelado)
      v.amp = 0.30f * vel;
      v.nz = 1.0f; v.nAmt = 1.0f; v.nC = decaimiento(acento ? 0.070f : 0.020f);
      v.fc0 = v.fc1 = F; v.q = 0.55f;
      bqHPF(v.banda, 2500.0f, 0.707f);
      break;
    case I_MARTILLO:   // golpe sobre plancha: cuerpo corto de 2 parciales + mucho ruido. SIN saturar y SIN
                       // anillo: un parcial agudo que suena es lo que lo hacía sonar a olla
      v.amp = 0.55f * vel;
      v.inc[0] = F / SR; v.inc[1] = F * timbre[t].r2 / SR;
      v.a[0] = 0.40f; v.a[1] = 0.20f;
      v.env = 1.0f; v.eC = decaimiento(0.045f);
      v.cuanto = 0.18f; v.caida = 1.0f; v.caidaC = decaimiento(0.035f);
      v.nz = 1.0f; v.nAmt = 1.25f; v.nC = decaimiento(acento ? 0.12f : 0.09f);
      v.fc0 = 2100.0f; v.fc1 = 1300.0f; v.barreInc = 1.0f / (0.09f * SR); v.q = 1.2f;
      bqHPF(v.banda, 150.0f, 0.707f); v.conBanda = true;
      break;
    case I_PISTON:     // pistón hidráulico: un golpe grave corto (UNA parcial que cae) + escape de aire
                       // que baja. Nada de parciales metálicos afinados: eso sonaba a olla.
      v.amp = 0.50f * vel;
      v.inc[0] = F / SR; v.a[0] = 0.85f;
      v.env = 1.0f; v.eC = decaimiento(0.050f);
      v.cuanto = 0.50f; v.caida = 1.0f; v.caidaC = decaimiento(0.020f);
      v.nz = 1.0f; v.nAmt = 0.95f; v.nC = decaimiento(acento ? 0.10f : 0.07f);
      v.fc0 = 2000.0f; v.fc1 = 350.0f; v.barreInc = 1.0f / (0.07f * SR); v.q = 0.8f;
      bqHPF(v.banda, 90.0f, 0.707f); v.conBanda = true;
      pan = 0.30f + 0.4f * frnd();
      break;
    case I_CLAP:       // clap de UN golpe de ruido pasa-banda: las tres palmadas seguidas sonaban a clipeo
      v.amp = 0.34f * vel;
      v.nz = 1.0f; v.nAmt = 1.1f; v.nC = decaimiento(acento ? 0.050f : 0.035f);
      v.fc0 = v.fc1 = F; v.q = 1.2f;
      bqHPF(v.banda, 700.0f, 0.707f);
      pan = 0.42f + 0.16f * frnd();
      break;
    case I_ABIERTO:    // hat abierto: ruido pasa-banda más largo, del otro lado que el cerrado
      v.amp = 0.22f * vel;
      v.nz = 1.0f; v.nAmt = 1.0f; v.nC = decaimiento(acento ? 0.11f : 0.08f);
      v.fc0 = v.fc1 = F; v.q = 0.5f;
      bqHPF(v.banda, 3000.0f, 0.707f);
      break;
    case I_TOM:        // tom: UNA parcial que cae 30 % (afinada, sin saturar) + mazo de ruido
      v.amp = 0.45f * vel;
      v.inc[0] = F / SR; v.a[0] = 0.80f;
      v.env = 1.0f; v.eC = decaimiento(0.16f);
      v.cuanto = 0.30f; v.caida = 1.0f; v.caidaC = decaimiento(0.030f);
      v.nz = 1.0f; v.nAmt = 0.35f; v.nC = decaimiento(0.012f);
      v.fc0 = v.fc1 = 900.0f; v.q = 0.8f;
      bqHPF(v.banda, 60.0f, 0.707f); v.conBanda = true;
      pan = 0.30f + 0.4f * frnd();
      break;
  }
  coefFiltro(v.s, v.fc0, v.q);
  v.gL = sqrtf(1.0f - pan); v.gR = sqrtf(pan);
  publicarGolpe(t, vel);
}

// ─── Un paso del secuenciador (semicorchea) ─────────────────
int notaBajoDe(char c, int raiz) {
  int g = raiz;
  switch (c) {
    case 'o': g += 7; break;
    case '5': g += 4; break;
    case 'b': g += 6; break;
    case '3': g += 2; break;
  }
  const int t = TONICA_BAJO + transp;
  int n = notaAcorde(t, raiz, g - raiz, false);
  if (c == 'b') {                                  // séptima: sólo si es menor; si no, la quinta
    int f = notaAcorde(t, raiz, 0, false);
    if (((n - f) % 12 + 12) % 12 != 10) n = notaAcorde(t, raiz, 4, false);
  }
  return n;
}

// La próxima nota (no guion) de una figura, desde pos; pos queda después de ella
char siguienteNota(const char *pat, int largo, int &pos) {
  for (int k = 0; k < largo; k++) {
    char c = pat[(pos + k) % largo];
    if (c != '-') { pos = (pos + k + 1) % largo; return c; }
  }
  return 0;
}

void golpeCajaSubida(float vel, float largo) {
  cajaSub.env = 1.0f; cajaSub.nz = 1.0f; cajaSub.amp = vel;
  cajaSub.eC = decaimiento(0.05f * largo); cajaSub.nC = decaimiento(0.11f * largo);   // más corta cuanto más rápido el redoble
  coefFiltro(cajaSub.s, 2200.0f, 1.0f);
}
void golpeFinal() {
  golpeFin.viva = true; golpeFin.f = 130.0f; golpeFin.env = 1.0f; golpeFin.nz = 1.0f;
  golpeFin.eC = decaimiento(0.45f); golpeFin.nC = decaimiento(0.30f);
  coefFiltro(golpeFin.s, 3200.0f, 0.6f);
  reverbViva = (int)(3.0f * SR);
}

// Grado del acorde que toca en este compás según la variación elegida
int gradoVariacion(int v, uint32_t compas) {
  int total = 0;
  for (int k = 0; k < 5 && VAR_ACORDES[v][k].compases; k++) total += VAR_ACORDES[v][k].compases;
  int pos = (int)(compas % (uint32_t)total);
  for (int k = 0; k < 5 && VAR_ACORDES[v][k].compases; k++) {
    if (pos < VAR_ACORDES[v][k].compases) {
      int g = VAR_ACORDES[v][k].grado;
      if (g == S_BANCO) return BANCOS[banco].acordes[1];
      if (g == 0) return BANCOS[banco].acordes[0];
      // un acorde "nuevo" que en este banco repetiría al segundo (VII en Cm–B♭) o caería en el
      // principal (el III en el frigio dominante) no aportaría nada: se usa el vecino
      if (acordeDelModo(g) == acordeDelModo(BANCOS[banco].acordes[1])) return g == 6 ? 5 : 3;
      if (acordeDelModo(g) == acordeDelModo(BANCOS[banco].acordes[0])) return 3;
      return g;
    }
    pos -= VAR_ACORDES[v][k].compases;
  }
  return BANCOS[banco].acordes[0];
}

void avanzarPaso() {
  pasoBase = (pasoBase + 1) & 63;                  // 4 compases
  pasoInd = pasoBase % largoInd;                   // la fábrica SIEMPRE en fase con la base (16 o 32 dividen a 64)
  int s = pasoBase & 15;
  if (s == 0) compasTotal++;
  if (estadoSub == E_SUBIDA) {
    subPasos++;
    if (subPasos >= SUBIDA_PASOS && s == 0) {      // fin en el "1" de un compás: el golpe, y todo lo demás se corta
      estadoSub = E_ESPERA; golpeFinal();
    } else if (subPasos > SUBIDA_PASOS / 2) {      // caja desde la mitad: negras → corcheas → semicorcheas, creciendo
      int k = subPasos - SUBIDA_PASOS / 2, cada = k <= 32 ? 4 : (k <= 64 ? 2 : 1);
      if ((s % cada) == 0) golpeCajaSubida(0.30f + 0.70f * fminf(1.0f, (float)k / (SUBIDA_PASOS / 2)), cada == 4 ? 1.0f : cada == 2 ? 0.7f : 0.45f);
    }
  }
  const int cc = compasTotal % 8;                  // (la línea de bajo varía en los compases 4 y 8)
  int raiz = acordeDelModo(gradoVariacion(varAc, compasTotal));
  acordeAct = raiz;

  // El RITMO del arpegio sale de los hats que suenan en este paso; la figura sólo pone el orden
  // de las notas y vuelve a empezar cada 4 compases.
  if (pasoBase == 0) posArp = 0;
  const bool ritmoArp = prio[I_HAT][pasoInd] < densS || prio[I_ABIERTO][pasoInd] < densS;
  if (ritmoArp) {
    char a = siguienteNota(ARPS[arp], 16, posArp);
    if (a) golpeArp(notaAcorde(TONICA_ARP + transp, raiz, TONOS_ARP[a - '0'], true));
  }

  if (!aBreak) {
    if ((s & 3) == 0) golpeBombo(s == 0 ? 1.0f : 0.92f);
    char c = LINEAS_BAJO[linea][(cc & 3) == 3 ? 1 : 0][s];   // la variación en los compases 4 y 8
    if (c != '-') golpeBajo(notaBajoDe(c, raiz));
  }

  char k = PAT_KEYS[patKeys][(compasTotal & 1) * 16 + s];   // el KEYS: su propio ritmo, escrito
  int tk = TONICA_KEYS + transp; while (tk < KEYS_DESDE) tk += 12;   // la tónica del keys siempre entre Do4 y Si4
  if (k != '-') golpeKeys(notaAcorde(tk, raiz, gradoDe(k), true), (s & 3) == 0 ? 1.0f : 0.8f);

  for (int t = 0; t < NUM_IND; t++) {
    if (prio[t][pasoInd] >= densS) continue;
    bool acc = (acentoInd[t] >> pasoInd) & 1u;
    golpeInd(t, acc ? 1.0f : entre(0.62f, 0.85f), acc);
  }

  pasoBasePub = pasoBase; pasoIndPub = pasoInd; contadorPasos = contadorPasos + 1;
}

// ==============================================================================================================================================
// RENDER POR BLOQUES — cada voz de corrido, con su estado en variables locales
// ==============================================================================================================================================
float bufBase[BUFFER_SAMPLES], bufIndL[BUFFER_SAMPLES], bufIndR[BUFFER_SAMPLES], bufSc[BUFFER_SAMPLES];

IRAM_ATTR void renderSidechain(float *o, int m) {  // baja en ~2 ms al llegar el bombo y vuelve en ~140 ms
  float obj = scObj, s = sc;
  for (int i = 0; i < m; i++) { obj *= dSc; s += (obj - s) * 0.012f; o[i] = s; }
  scObj = obj; sc = s;
}

IRAM_ATTR void renderBombo(float *o, int m) {
  uint32_t sem = semillaRuido;
  for (int k = 0; k < 2; k++) {
    VozBombo &v = bombos[k]; if (!v.viva) continue;
    float f = v.f, ph = v.ph, env = v.env, kn = v.knock, cl = v.click; const float eC = v.eC, kC = v.kC, cC = v.cC, amp = v.amp * NIVEL_BOMBO;
    for (int i = 0; i < m; i++) {
      f += (K_FREQ - f) * fCoefBombo;
      ph += f * (1.0f / SR); if (ph >= 1.0f) ph -= 1.0f;
      float p2 = ph + ph; if (p2 >= 1.0f) p2 -= 1.0f;
      float y = softClip((seno(ph) + 0.10f * seno(p2)) * K_SAT) * 0.9f * env;
      if (kn > 1e-4f) y += pasoBQ(v.bp, ruido(sem)) * kn * (K_KNOCK * 1.6f);
      if (cl > 1e-4f) y += pasoBQ(v.hp, ruido(sem)) * cl * (K_CLICK * 0.45f);
      o[i] += y * amp;
      env *= eC; kn *= kC; cl *= cC;
    }
    v.f = f; v.ph = ph; v.env = env; v.knock = kn; v.click = cl;
    if (env < 1e-3f) v.eC = dRapido;               // −60 dB: cola de 1.5 ms y recién a −82 dB se libera
    if (env < 8e-5f && kn < 1e-4f) v.viva = false;
  }
  semillaRuido = sem;
}

IRAM_ATTR void renderBajo(float *o, const float *scb, int m, float corte, float q, float comp) {
  if (!bajo.atk && bajo.env < 1e-5f) return;
  float f1 = bajo.f1, f2 = bajo.f2, fs = bajo.fs, env = bajo.env, envF = bajo.envF; bool atk = bajo.atk, atkF = bajo.atkF;
  const float i1 = bajo.inc1, i2 = bajo.inc2, iS = bajo.incS, v1 = bajo.inv1, v2 = bajo.inv2, vS = bajo.invS;
  const float nivel = NIVEL_BAJO * comp, dComp = 1.0f / (0.75f + 0.25f * BAJO_DRIVE);
  Filtro s = bajo.s; uint32_t c = muestraGlobal;
  for (int i = 0; i < m; i++) {
    if (atk) { env += incAtkBajo; if (env >= 1.0f) { env = 1.0f; atk = false; } } else env *= dBajo;   // ataque y caída: panel del menú
    if (atkF) { envF += 1.0f / 66.0f; if (envF >= 1.0f) { envF = 1.0f; atkF = false; } } else envF *= dBajoF;
    if (((c + i) & 15) == 0) coefFiltro(s, corte * exp2Rapido(BAJO_ENV_OCT * envF), q);
    float mez = (sierra(f1, i1, v1) + sierra(f2, i2, v2)) * 0.5f + cuadrada(fs, iS, vS) * BAJO_SUB;
    float lp, bp; pasoFiltro(s, softClip(mez * BAJO_DRIVE) * dComp + 1e-18f, lp, bp);
    o[i] += lp * nivel * env * (1.0f - SC_BAJO * scb[i]);
  }
  bajo.f1 = f1; bajo.f2 = f2; bajo.fs = fs; bajo.env = env; bajo.envF = envF; bajo.atk = atk; bajo.atkF = atkF; bajo.s = s;
}

IRAM_ATTR void renderInd(float *oL, float *oR, const float *scb, int m) {
  uint32_t sem = semillaRuido;
  const uint32_t c = muestraGlobal;
  for (int t = 0; t < NUM_IND; t++) for (int k = 0; k < 2; k++) {
    VozInd &v = ind[t][k]; if (!v.viva) continue;
    float p0 = v.ph[0], p1 = v.ph[1];
    const float n0 = v.inc[0], n1 = v.inc[1], a0 = v.a[0], a1 = v.a[1];
    const bool dosParciales = a1 != 0.0f, conBanda = v.conBanda;
    float env = v.env, caida = v.caida, nz = v.nz, barre = v.barre;
    const float eC = v.eC, caidaC = v.caidaC, cuanto = v.cuanto, nC = v.nC, nAmt = v.nAmt, amp = v.amp * NIVEL_IND;
    const float gL = v.gL, gR = v.gR, barreInc = v.barreInc;
    Filtro s = v.s; BQ banda = v.banda;
    for (int i = 0; i < m; i++) {
      float y = 0.0f;
      if (env > 1e-5f) {                           // capa tonal
        float mul = 1.0f + cuanto * caida; caida *= caidaC;
        p0 += n0 * mul; if (p0 >= 1.0f) p0 -= 1.0f;
        float t = seno(p0) * a0;
        if (dosParciales) { p1 += n1 * mul; if (p1 >= 1.0f) p1 -= 1.0f; t += seno(p1) * a1; }   // el pistón y el tom usan uno solo
        y = t * env;
        env *= eC;
      }
      if (barreInc > 0.0f && ((c + i) & 15) == 0) {   // barrido del filtro de ruido
        barre += barreInc * 16.0f; if (barre > 1.0f) barre = 1.0f;
        coefFiltro(s, v.fc0 + (v.fc1 - v.fc0) * barre, v.q);
      }
      if (nz > 1e-5f) {                            // capa de ruido
        float lp, bp; pasoFiltro(s, ruido(sem), lp, bp);
        y += bp * nz * nAmt; nz *= nC;
      }
      if (conBanda) y = pasoBQ(banda, y);
      y *= amp * (1.0f - SC_IND * scb[i]);
      oL[i] += y * gL; oR[i] += y * gR;
    }
    v.ph[0] = p0; v.ph[1] = p1; v.env = env; v.caida = caida; v.nz = nz; v.barre = barre;
    v.s = s; v.banda = banda;
    if (env < 1e-3f) v.eC = dRapido;               // −60 dB → 1.5 ms → se libera a −82 dB
    if (nz < 1e-3f) v.nC = dRapido;
    if (env < 8e-5f && nz < 8e-5f) v.viva = false;
  }
  semillaRuido = sem;
}

// KEYS y ARPEGIO: UNA onda PolyBLEP (cuadrada el keys, sierra el arpegio) → pasa-bajos con envolvente (sube `oct` octavas sobre
// base y se cierra en ~70 ms) → VCA. Se apagan en dos etapas: −60 dB → 1.5 ms → libre a −82 dB.
// El pluck quedó en un punto medio medido de oído: La3 brillante y a 0.34 era demasiado agudo y
// presente; 1.2·f + 2.4 oct a 0.15 ya no se escuchaba. Una sola sierra por voz y el filtro cada 32
// muestras: con dos sierras y 5+3 voces, pluck y arpegio solos se comían el 30 % del presupuesto
// (medido en la placa con MEDIR) y el audio pasaba del 100 % en los bloques densos.
IRAM_ATTR void renderGrupo(VozPluck *grupo, int n, float nivel, float *oL, float *oR, const float *scb, int m) {
  const uint32_t c = muestraGlobal;
  for (int k = 0; k < n; k++) {
    VozPluck &v = grupo[k]; if (!v.viva) continue;
    float f1 = v.f1, amp = v.amp, fenv = v.fenv; bool atk = v.atk; Filtro s = v.s;
    const float i1 = v.inc1, w1 = v.inv1, eC = v.eC, base = v.base, oct = v.oct, q = v.q;
    const float g = nivel * NIVEL_IND, gL = v.gL * g, gR = v.gR * g;
    const bool cuad = v.cuad;
    for (int i = 0; i < m; i++) {
      if (atk) { amp += 1.0f / 132.0f; if (amp >= 1.0f) { amp = 1.0f; atk = false; } }   // 3 ms, desde donde estaba
      else { amp *= amp < 1e-3f ? dRapido : eC; if (amp < 8e-5f) { v.viva = false; break; } }
      fenv *= dPluckFenv;
      if (((c + i + 7 * k) & 31) == 0) coefFiltro(s, base * exp2Rapido(oct * fenv), q);
      float x = cuad ? cuadrada(f1, i1, w1) : sierra(f1, i1, w1), lp, bp;
      pasoFiltro(s, x, lp, bp);
      float y = lp * amp * (1.0f - SC_IND * scb[i]);
      oL[i] += y * gL; oR[i] += y * gR;
    }
    v.f1 = f1; v.amp = amp; v.fenv = fenv; v.atk = atk; v.s = s;
  }
}

// Pulso PolyBLEP de ancho w: sierra menos sierra desplazada en w
static inline float pulso(float &f, float inc, float inv, float w) {
  float p2 = f + w; if (p2 >= 1.0f) p2 -= 1.0f;
  float a = 2.0f * f - 1.0f, b = 2.0f * p2 - 1.0f;
  if (f < inc) { float t = f * inv; a -= t + t - t * t - 1.0f; } else if (f > 1.0f - inc) { float t = (f - 1.0f) * inv; a -= t * t + t + t + 1.0f; }
  if (p2 < inc) { float t = p2 * inv; b -= t + t - t * t - 1.0f; } else if (p2 > 1.0f - inc) { float t = (p2 - 1.0f) * inv; b -= t * t + t + t + 1.0f; }
  f += inc; if (f >= 1.0f) f -= 1.0f;
  return (a - b) * 0.5f;
}

// KEYS: voz (mono) → chorus estéreo → eco ping-pong. Los efectos corren siempre (las colas siguen
// sonando aunque la nota ya terminó). El eco a corchea con puntillo sigue al tempo: si cambia,
// pasa del cabezal viejo al nuevo con un cruce de 50 ms — mover un cabezal de a poco desafina las
// repeticiones (lección de oscilador_escalas).
IRAM_ATTR void renderKeys(float *oL, float *oR, const float *scb, const float *ecoExtra, int m) {
  // ── la voz ──
  if (kv.viva) {
    float f1 = kv.f1, amp = kv.amp, fenv = kv.fenv, lfo = kv.lfo; bool atk = kv.atk; Filtro s = kv.s;
    const float i1 = kv.inc1, w1 = kv.inv1, eC = kv.eC, base = kv.base;
    const float g = NIVEL_KEYS * NIVEL_IND, dLfo = KEYS_PWM_HZ / SR;
    const uint32_t c = muestraGlobal;
    float ancho1 = 0.5f - KEYS_PWM_PROF * (0.5f + 0.5f * seno(lfo));
    int i = 0;
    for (; i < m; i++) {
      if (atk) { amp += 1.0f / 132.0f; if (amp >= 1.0f) { amp = 1.0f; atk = false; } }
      else { amp *= amp < 1e-3f ? dRapido : eC; if (amp < 8e-5f) { kv.viva = false; break; } }
      fenv *= dPluckFenv;
      lfo += dLfo; if (lfo >= 1.0f) lfo -= 1.0f;
      if (((c + i) & 31) == 0) {
        coefFiltro(s, base * exp2Rapido(KEYS_ENV_OCT * fenv), KEYS_Q);
        ancho1 = 0.5f - KEYS_PWM_PROF * (0.5f + 0.5f * seno(lfo));
      }
      float x = pulso(f1, i1, w1, ancho1), lp, bp;
      pasoFiltro(s, x, lp, bp);
      keysBuf[i] = lp * amp * g * (1.0f - SC_IND * scb[i]);
    }
    for (; i < m; i++) keysBuf[i] = 0.0f;
    kv.f1 = f1; kv.amp = amp; kv.fenv = fenv; kv.lfo = lfo; kv.atk = atk; kv.s = s;
  } else for (int i = 0; i < m; i++) keysBuf[i] = 0.0f;

  // ── chorus estéreo: dos lecturas moduladas de un retardo corto, en cuadratura ──
  const float dChorus = CHORUS_HZ / SR, base = CHORUS_MS * 0.001f * SR, prof = CHORUS_PROF_MS * 0.001f * SR;
  // ── eco ping-pong: largo = corchea con puntillo (3 semicorcheas) ──
  int objetivo = (int)(3.0f * sps); if (objetivo > ECO_MAX - 2) objetivo = ECO_MAX - 2;
  if (ecoXf >= 1.0f && abs(objetivo - ecoN) > 32) { ecoN2 = objetivo; ecoXf = 0.0f; }
  float lfoC = chorusLfo, xf = ecoXf, lpL = ecoLpL, lpR = ecoLpR, hpL = ecoHpL, hpR = ecoHpR;
  int ci = chorusIdx, ei = ecoIdx;
  float d1 = base + prof * seno(lfoC), d2 = base + prof * seno(lfoC + 0.25f >= 1.0f ? lfoC - 0.75f : lfoC + 0.25f);
  const uint32_t c0 = muestraGlobal;
  for (int i = 0; i < m; i++) {
    const float x = keysBuf[i];
    chorusBuf[ci] = x;
    lfoC += dChorus; if (lfoC >= 1.0f) lfoC -= 1.0f;
    if (((c0 + i) & 15) == 0) {                     // el LFO del chorus es lento: basta cada 16 muestras
      d1 = base + prof * seno(lfoC); d2 = base + prof * seno(lfoC + 0.25f >= 1.0f ? lfoC - 0.75f : lfoC + 0.25f);
    }
    float p1 = (float)ci - d1; if (p1 < 0.0f) p1 += CHORUS_N;
    float p2 = (float)ci - d2; if (p2 < 0.0f) p2 += CHORUS_N;
    int a1 = (int)p1, a2 = (int)p2; float t1 = p1 - a1, t2 = p2 - a2;
    a1 &= CHORUS_N - 1; a2 &= CHORUS_N - 1;           // ci − d + N puede redondear a N justo: sin esto se leía
                                                      // fuera del buffer y salía un valor gigante (medido en simulación)
    float c1 = chorusBuf[a1] + (chorusBuf[(a1 + 1) & (CHORUS_N - 1)] - chorusBuf[a1]) * t1;
    float c2 = chorusBuf[a2] + (chorusBuf[(a2 + 1) & (CHORUS_N - 1)] - chorusBuf[a2]) * t2;
    ci = (ci + 1) & (CHORUS_N - 1);
    float L = x * (1.0f - CHORUS_MEZCLA * 0.5f) + c1 * CHORUS_MEZCLA, R = x * (1.0f - CHORUS_MEZCLA * 0.5f) + c2 * CHORUS_MEZCLA;

    int r1 = ei - ecoN; if (r1 < 0) r1 += ECO_MAX;
    float dl = ecoL[r1] * (1.0f / 32768.0f), dr = ecoR[r1] * (1.0f / 32768.0f);
    if (xf < 1.0f) {                                  // cruce hacia el largo nuevo
      int r2 = ei - ecoN2; if (r2 < 0) r2 += ECO_MAX;
      float nl = ecoL[r2] * (1.0f / 32768.0f), nr = ecoR[r2] * (1.0f / 32768.0f);
      dl += (nl - dl) * xf; dr += (nr - dr) * xf;
      xf += 1.0f / 2205.0f; if (xf >= 1.0f) { xf = 1.0f; ecoN = ecoN2; }
    }
    lpL += (dl - lpL) * 0.45f; lpR += (dr - lpR) * 0.45f;      // cada vuelta más oscura (~4 kHz)
    hpL += (lpL - hpL) * 0.010f; hpR += (lpR - hpR) * 0.010f;  // y sin barro abajo (~70 Hz)
    float fl = lpL - hpL, fr = lpR - hpR;
    float inL = x + ecoExtra[i] + ECO_FEEDBACK * fr, inR = ECO_FEEDBACK * fl;   // ping-pong: lo que vuelve cruza de lado (+ el oscilador)
    if (inL > 0.99f) inL = 0.99f; if (inL < -0.99f) inL = -0.99f;
    if (inR > 0.99f) inR = 0.99f; if (inR < -0.99f) inR = -0.99f;
    ecoL[ei] = (int16_t)(inL * 32767.0f); ecoR[ei] = (int16_t)(inR * 32767.0f);
    ei = (ei + 1) & (ECO_MAX - 1);
    oL[i] += L + dl * ECO_MEZCLA; oR[i] += R + dr * ECO_MEZCLA;
  }
  chorusLfo = lfoC; chorusIdx = ci; ecoIdx = ei; ecoXf = xf; ecoLpL = lpL; ecoLpR = lpR; ecoHpL = hpL; ecoHpR = hpR;
}

// SUBIDA: oscilador agresivo que sube SUBIDA_OCTAVAS desde la tónica grave + ruido blanco
// estéreo, los dos creciendo con el avance; trémolo rítmico desde la mitad, cada vez más lento.
IRAM_ATTR void renderSubida(float *oL, float *oR, int m) {
  if (estadoSub != E_SUBIDA) return;
  const float porPaso = 1.0f / sps;
  const float fBaja = hz((float)(TONICA_BAJO + transp));
  float f1 = subF1, f2 = subF2, lp = subLp, tg = subTrem;
  float i1 = 0.001f, i2 = 0.001f, w1 = 1000.0f, w2 = 1000.0f;
  uint32_t a = semRuidoL, b = semRuidoR;
  const uint32_t c = muestraGlobal;
  for (int i = 0; i < m; i++) {
    float pos = (float)subPasos - (faltanTramo - (float)i) * porPaso;      // semicorcheas desde que empezó
    if (pos < 0.0f) pos = 0.0f;
    float p = pos / (float)SUBIDA_PASOS; if (p > 1.0f) p = 1.0f;
    if (i == 0 || ((c + i) & 15) == 0) {
      float f = fBaja * exp2Rapido(SUBIDA_OCTAVAS * p * (0.55f + 0.45f * p));   // sube cada vez más rápido
      i1 = f * 0.9914f / SR; i2 = f * 1.0087f / SR;                            // ±15 cents: áspero
      w1 = 1.0f / i1; w2 = 1.0f / i2;
    }
    // trémolo: desde la mitad, en semicorcheas hasta el final (frenarlo le quitaba el efecto),
    // amarrado a la grilla de la música: abierto en la primera mitad de cada semicorchea
    float objT = 1.0f;
    if (pos >= SUBIDA_PASOS / 2) {
      float ph = 1.0f - (faltanTramo - (float)i) * porPaso;
      float prof = fminf(1.0f, (pos - SUBIDA_PASOS / 2) / 16.0f) * 0.95f;   // entra en un compás
      objT = ph < 0.5f ? 1.0f : 1.0f - prof;
    }
    tg += (objT - tg) * 0.02f;                                               // ~1 ms: corta sin clic
    float x = softClip((sierra(f1, i1, w1) + sierra(f2, i2, w2)) * 0.5f * SUBIDA_DRIVE);
    lp += (x - lp) * 0.55f;                                                  // le quita el filo de arriba
    float o = lp * NIVEL_SUBIDA_OSC * (0.35f + 0.65f * p) * NIVEL_IND;
    float rn = NIVEL_SUBIDA_RUIDO * p * p * NIVEL_IND;
    oL[i] += (o + ruido(a) * rn) * tg;
    oR[i] += (o + ruido(b) * rn) * tg;
    if (cajaSub.env > 1e-4f || cajaSub.nz > 1e-4f) {                       // la caja: cuerpo + bordonera
      cajaSub.ph1 += 190.0f / SR; if (cajaSub.ph1 >= 1.0f) cajaSub.ph1 -= 1.0f;
      cajaSub.ph2 += 297.0f / SR; if (cajaSub.ph2 >= 1.0f) cajaSub.ph2 -= 1.0f;
      float lpN, bpN; pasoFiltro(cajaSub.s, ruido(a), lpN, bpN);
      float y = ((seno(cajaSub.ph1) + 0.6f * seno(cajaSub.ph2)) * 0.45f * cajaSub.env + bpN * 1.3f * cajaSub.nz) * cajaSub.amp * NIVEL_CAJA_SUBIDA * NIVEL_IND;
      oL[i] += y; oR[i] += y;
      cajaSub.env *= cajaSub.eC; cajaSub.nz *= cajaSub.nC;
    }
  }
  subF1 = f1; subF2 = f2; subLp = lp; subTrem = tg; semRuidoL = a; semRuidoR = b;
}

// OSCILADOR CONTINUO: dos sierras desafinadas ±6 cents, cada una con su SVF pasa-bajos RESONANTE
// que sigue a la nota y que barre un LFO libre de velocidad aleatoria, cargada a un lado → VCA. Seco a oscL/oscR (pasa por el corte del final de la subida
// como todo), un envío a la reverb y otro al eco ping-pong del keys. La altura cambia con un
// portamento corto; los coeficientes se recalculan cada 16 muestras.
IRAM_ATTR void renderOsc(float *oL, float *oR, float *envio, float *eco, const float *scb, int m) {
  if (!osc.vivo) {                                       // callado: el retardo del chorus sigue corriendo vacío (sin restos viejos al volver)
    for (int i = 0; i < m; i++) { oL[i] = 0.0f; oR[i] = 0.0f; envio[i] = 0.0f; eco[i] = 0.0f; oscChorusBuf[oscChorusIdx] = 0.0f; oscChorusIdx = (oscChorusIdx + 1) & (OSC_CHORUS_N - 1); }
    return;
  }
  float f1 = osc.f1, f2 = osc.f2, inc = osc.inc, amp = osc.amp; Filtro s = osc.s, s2 = osc.s2;
  const float lado = 0.5f + 0.5f * OSC_ANCHO, otro = 1.0f - lado;
  const float incObj = osc.incObj, obj = osc.obj, k = obj > amp ? kOscAtk : kOscSuelta, g = NIVEL_OSC * NIVEL_IND;
  float i1 = inc / OSC_DETUNE, i2 = inc * OSC_DETUNE, w1 = 1.0f / i1, w2 = 1.0f / i2;
  const uint32_t c = muestraGlobal;
  float comp = osc.comp;
  int ci = oscChorusIdx; float lfoC = oscChorusLfo;
  const float baseC = OSC_CHORUS_MS * 0.001f * SR, profC = OSC_CHORUS_PROF_MS * 0.001f * SR;
  float dC1 = baseC + profC * seno(lfoC), dC2 = baseC + profC * seno(lfoC + 0.25f >= 1.0f ? lfoC - 0.75f : lfoC + 0.25f);
  for (int i = 0; i < m; i++) {
    amp += (obj - amp) * k;
    inc += (incObj - inc) * dOscGlide;
    if (i == 0 || ((c + i) & 15) == 0) {
      i1 = inc / OSC_DETUNE; i2 = inc * OSC_DETUNE; w1 = 1.0f / i1; w2 = 1.0f / i2;
      // LFO libre: cada tanto sortea otra velocidad y se desliza hacia ella (en octavas: suave a cualquier velocidad)
      if (((c + i) & 15) == 0) {                                    // avanza sólo en la grilla de 16 (no al empezar cada tramo)
        lfoFalta -= 16.0f / SR;
        if (lfoFalta <= 0.0f) {
          lfoVelObj = (0.5f + 0.5f * ruido(semLfo)) * octLfo;
          lfoFalta = OSC_LFO_CAMBIO_MIN + (OSC_LFO_CAMBIO_MAX - OSC_LFO_CAMBIO_MIN) * (0.5f + 0.5f * ruido(semLfo));
        }
        lfoVel += (lfoVelObj - lfoVel) * kDeslizLfo;
        lfoFase += OSC_LFO_MIN_HZ * exp2Rapido(lfoVel) * (16.0f / SR); if (lfoFase >= 1.0f) lfoFase -= 1.0f;
      }
      float lfo = 0.5f + 0.5f * seno(lfoFase);                      // 0 cerrado · 1 abierto
      float q = OSC_Q_CERRADO + (OSC_Q_ABIERTO - OSC_Q_CERRADO) * lfo;
      coefFiltro(s, inc * SR * OSC_CORTE_MIN * exp2Rapido(OSC_LFO_OCT * lfo), q);
      s2.a1 = s.a1; s2.a2 = s.a2; s2.a3 = s.a3; s2.k = s.k;      // mismo filtro, estado propio
      comp = 1.0f / sqrtf(sqrtf(q));                                // la resonancia no sube el volumen
    }
    float lp1, lp2, bp;
    pasoFiltro(s,  sierra(f1, i1, w1) + 1e-18f, lp1, bp);
    pasoFiltro(s2, sierra(f2, i2, w2) + 1e-18f, lp2, bp);
    const float a = amp * g * comp * 0.5f * (1.0f - OSC_SC * scb[i]);
    lp1 *= a; lp2 *= a;
    const float mono = lp1 + lp2;
    // chorus: dos lecturas moduladas del retardo, una a cada lado
    oscChorusBuf[ci] = mono;
    if (((c + i) & 15) == 0) {
      lfoC += OSC_CHORUS_HZ * 16.0f / SR; if (lfoC >= 1.0f) lfoC -= 1.0f;
      dC1 = baseC + profC * seno(lfoC); dC2 = baseC + profC * seno(lfoC + 0.25f >= 1.0f ? lfoC - 0.75f : lfoC + 0.25f);
    }
    float p1 = (float)ci - dC1; if (p1 < 0.0f) p1 += OSC_CHORUS_N;
    float p2 = (float)ci - dC2; if (p2 < 0.0f) p2 += OSC_CHORUS_N;
    int a1 = (int)p1, a2 = (int)p2; float t1 = p1 - a1, t2 = p2 - a2;
    a1 &= OSC_CHORUS_N - 1; a2 &= OSC_CHORUS_N - 1;
    float c1 = oscChorusBuf[a1] + (oscChorusBuf[(a1 + 1) & (OSC_CHORUS_N - 1)] - oscChorusBuf[a1]) * t1;
    float c2 = oscChorusBuf[a2] + (oscChorusBuf[(a2 + 1) & (OSC_CHORUS_N - 1)] - oscChorusBuf[a2]) * t2;
    ci = (ci + 1) & (OSC_CHORUS_N - 1);
    const float mz = OSC_CHORUS_MEZCLA * 0.5f, seco = 1.0f - 0.5f * OSC_CHORUS_MEZCLA;
    oL[i] = (lp1 * lado + lp2 * otro) * seco + c1 * mz;
    oR[i] = (lp2 * lado + lp1 * otro) * seco + c2 * mz;
    envio[i] = mono * OSC_REVERB; eco[i] = mono * OSC_ECO;
  }
  oscChorusIdx = ci; oscChorusLfo = lfoC;
  osc.f1 = f1; osc.f2 = f2; osc.inc = inc; osc.amp = amp; osc.s = s; osc.s2 = s2; osc.comp = comp;
  if (obj == 0.0f && amp < 1e-5f) osc.vivo = false;     // ya a −100 dB: se libera sin escalón
  if (OSC_REVERB > 0.0f) reverbViva = (int)(3.0f * SR);  // mientras suena, la reverb trabaja (si tiene envío)
}

// GOLPE FINAL + REVERB: bus propio (golpeL/R) que el corte del final no toca.
// Reverb de 4 peines con amortiguación + 2 pasa-todo por lado, con la entrada normalizada por
// la realimentación (si no, la cola florece y se come el limitador — regla de drum_poder).
static inline float peine(float *b, int n, int &i, float &lp, float x) {
  float y = b[i]; lp += (y - lp) * 0.45f;          // cola oscura: arriba de ~4 kHz se apaga rápido
  b[i] = x + lp * REVERB_FB; if (++i >= n) i = 0; return y;
}
static inline float pasaTodo(float *b, int n, int &i, float x) {
  float d = b[i], y = d - 0.5f * x; b[i] = x + 0.5f * d; if (++i >= n) i = 0; return y;
}
IRAM_ATTR void renderGolpe(float *oL, float *oR, const float *envio, int m) {
  if (!golpeFin.viva && reverbViva <= 0) { for (int i = 0; i < m; i++) { oL[i] = 0.0f; oR[i] = 0.0f; } return; }
  uint32_t sem = semRuidoR;
  const float kIn = 0.28f * (1.0f - REVERB_FB);
  for (int i = 0; i < m; i++) {
    float g = 0.0f;
    if (golpeFin.viva) {
      golpeFin.f += (40.0f - golpeFin.f) * 0.0012f;                         // boom: cae de 130 a 40 Hz
      golpeFin.ph += golpeFin.f / SR; if (golpeFin.ph >= 1.0f) golpeFin.ph -= 1.0f;
      float lpN, bpN; pasoFiltro(golpeFin.s, ruido(sem), lpN, bpN);
      g = (softClip(seno(golpeFin.ph) * 1.6f) * golpeFin.env + bpN * 0.9f * golpeFin.nz) * NIVEL_GOLPE * NIVEL_IND;
      golpeFin.env *= golpeFin.eC; golpeFin.nz *= golpeFin.nC;
      if (golpeFin.env < 1e-4f && golpeFin.nz < 1e-4f) golpeFin.viva = false;
    }
    float x = (g * GOLPE_REVERB + envio[i]) * kIn / 0.28f;   // el golpe y el oscilador comparten la reverb
    float c = peine(rvC1, RV_N1, rvI1, rvLp1, x) + peine(rvC2, RV_N2, rvI2, rvLp2, x)
            + peine(rvC3, RV_N3, rvI3, rvLp3, x) + peine(rvC4, RV_N4, rvI4, rvLp4, x);
    float wl = pasaTodo(rvA2, RV_A2, rvJ2, pasaTodo(rvA1, RV_A1, rvJ1, c));
    float wr = pasaTodo(rvA4, RV_A4, rvJ4, pasaTodo(rvA3, RV_A3, rvJ3, c));
    oL[i] = g + wl * 2.2f; oR[i] = g + wr * 2.2f;
  }
  semRuidoR = sem;
  reverbViva -= m;
}

// Un tramo del bloque: [off, off+m)
volatile uint32_t medSec[6];                        // MEDIR: ciclos por sección (bombo+bajo · perc · keys · arp · master · pasos)
#if MEDIR && !defined(SIMULADOR)
#define CICLOS() ESP.getCycleCount()
#else
#define CICLOS() 0u
#endif
void renderTramo(int off, int m, float corte, float q, float comp) {
  uint32_t c0 = CICLOS();
  renderSidechain(bufSc + off, m);
  renderBombo(bufBase + off, m);
  renderBajo(bufBase + off, bufSc + off, m, corte, q, comp);
  uint32_t c1 = CICLOS();
  renderInd(bufIndL + off, bufIndR + off, bufSc + off, m);
  uint32_t c2 = CICLOS();
  renderOsc(oscL + off, oscR + off, oscEnvio + off, oscEco + off, bufSc + off, m);   // antes del keys: le manda su envío al eco
  renderKeys(bufIndL + off, bufIndR + off, bufSc + off, oscEco + off, m);
  renderSubida(bufIndL + off, bufIndR + off, m);
  renderGolpe(golpeL + off, golpeR + off, oscEnvio + off, m);
  uint32_t c3 = CICLOS();
  renderGrupo(arps,   VOCES_ARP,   NIVEL_ARP,   bufIndL + off, bufIndR + off, bufSc + off, m);
  uint32_t c4 = CICLOS();
  medSec[0] += (c1 - c0) >> 6; medSec[1] += (c2 - c1) >> 6; medSec[2] += (c3 - c2) >> 6; medSec[3] += (c4 - c3) >> 6;
  muestraGlobal += m;
}

// ==============================================================================================================================================
// UN BLOQUE DE AUDIO (128 muestras ≈ 2.9 ms)
// ==============================================================================================================================================
static i2s_chan_handle_t tx_chan;
int16_t salida[BUFFER_SAMPLES * 2];

IRAM_ATTR void calcularBloque() {
  // ── Borde del buffer: aplicar los pedidos de la tarea de control ──
  bool quiere = reqTocar;
  if (quiere && !aTocando) { pasoBase = -1; faltan = 0.0f; compasTotal = 0xFFFFFFFFu; }   // Play: el "1" en la primera muestra
  aTocando = quiere; tocandoPub = quiere;
  if (!quiere) estadoSub = E_NORMAL;
  if (reqSubida != vistoSubida) {                  // empieza la subida
    vistoSubida = reqSubida;
    if (estadoSub == E_NORMAL) { estadoSub = E_SUBIDA; subPasos = 0; subTrem = 1.0f; subLp = 0.0f; }
  }
  if (reqDrop != vistoDrop) {                      // el DROP: todo desde el "1", con bombo y bajo
    vistoDrop = reqDrop;
    if (estadoSub != E_NORMAL) {
      estadoSub = E_NORMAL;
      pasoBase = -1; faltan = 0.0f; compasTotal = 0xFFFFFFFFu;
      memset(ecoL, 0, sizeof(ecoL)); memset(ecoR, 0, sizeof(ecoR)); memset(chorusBuf, 0, sizeof(chorusBuf));
      kv.viva = false; kv.amp = 0.0f;
      for (int k = 0; k < VOCES_ARP; k++) arps[k].viva = false;
      for (int t = 0; t < NUM_IND; t++) for (int k = 0; k < 2; k++) ind[t][k].viva = false;
    }
  }
  estadoSubPub = estadoSub;
  aBreak = reqBreak || estadoSub != E_NORMAL; breakPub = aBreak;
  if (reqPatron != vistoPatron) { vistoPatron = reqPatron; guardarPatron(); nuevoPatronInd(reqSemilla); }
  if (reqDeshacer != vistoDeshacer) { vistoDeshacer = reqDeshacer; restaurarPatron(); }
  banco = reqBanco; patKeys = reqPatKeys; bancoPub = banco; patKeysPub = patKeys;
  varAc = reqVarAc; varAcPub = varAc;
  modo = BANCOS[banco].escala; transp = BANCOS[banco].transp;
  if (reqLinea != vistoLinea) {                    // BTN3: línea de bajo, figura del arpegio y frase del pluck (la armonía la elige el menú)
    vistoLinea = reqLinea; rngComp ^= reqSemilla | 1u;
    linea = (linea + 1 + (int)(frnd() * (NUM_LINEAS - 1))) % NUM_LINEAS;
    arp   = (arp   + 1 + (int)(frnd() * (NUM_ARPS   - 1))) % NUM_ARPS;
    lineaPub = linea;
  }
  float b = reqBPM; if (b != bpm) setBPM(b);
  {                                                // oscilador: posición → altura continua sobre la tónica del banco
    float pos = reqOscPos;
    if (estadoSub == E_ESPERA) pos = -1.0f;        // esperando el drop: todo calla, también él (su reverb se apaga sola)
    if (pos != oscPosVista || transp != oscTranspVisto) {
      oscPosVista = pos; oscTranspVisto = transp;
      if (pos >= 0.0f) {
        float inc = hz((float)(TONICA_OSC + transp) + pos * OSC_RANGO) / SR;
        if (!osc.vivo || osc.amp < 1e-3f) { osc.inc = inc; osc.vivo = true; }   // desde el silencio: sin portamento
        osc.incObj = inc; osc.obj = 1.0f;
      } else osc.obj = 0.0f;
    }
  }

  // ── Pots suavizados (por bloque; las ganancias se interpolan por muestra) ──
  corteS += (pCorte - corteS) * 0.15f;
#if MEDIR
  pDens = 1.0f;                                    // al medir, siempre el peor caso: densidad al máximo
#endif
  densS  += (pDens * 1.05f - densS) * 0.25f; densPub = densS;
  float gB0 = gBaseS, gI0 = gIndS, v0 = volS;
  float m = pMez;
  gBaseS += ((m < 0.5f ? 1.0f : 2.0f * (1.0f - m)) - gBaseS) * 0.15f;
  gIndS  += ((m > 0.5f ? 1.0f : 2.0f * m) - gIndS) * 0.15f;
  volS   += (pVol * pVol - volS) * 0.15f;
  atkBajoS += (pAtkBajo - atkBajoS) * 0.15f; decBajoS += (pDecBajo - decBajoS) * 0.15f;
  incAtkBajo = 1.0f / (BAJO_ATK_MIN * powf(BAJO_ATK_RANGO, atkBajoS) * SR);
  { float tau = BAJO_DEC_MIN * powf(BAJO_DEC_RANGO, decBajoS); dBajo = expf(-1.0f / (tau * SR)); dBajoF = expf(-1.0f / (tau * 0.55f * SR)); }
  const float corte = 55.0f * powf(30.0f, corteS);         // 55 Hz – 1.6 kHz
  const float q = 2.8f - 1.8f * corteS;                    // resonancia atada al corte
  const float comp = 1.0f / powf(q, 0.30f);

  for (int i = 0; i < BUFFER_SAMPLES; i++) { bufBase[i] = 0.0f; bufIndL[i] = 0.0f; bufIndR[i] = 0.0f; }

  // ── Síntesis por tramos: los pasos caen en la muestra exacta ──
  bool corre = aTocando || fundido > 1e-4f;
  int hecho = 0;
  while (hecho < BUFFER_SAMPLES) {
    int m2 = BUFFER_SAMPLES - hecho;
    if (corre && aTocando && estadoSub != E_ESPERA) {   // en la espera el reloj se queda quieto
      if (faltan <= 0.0f) { uint32_t cp0 = CICLOS(); avanzarPaso(); faltan += sps; medSec[5] += (CICLOS() - cp0) >> 6; }
      int hasta = (int)ceilf(faltan); if (hasta < 1) hasta = 1;
      if (hasta < m2) m2 = hasta;
      faltan -= (float)m2;
    }
    faltanTramo = faltan + (float)m2;              // muestras hasta el próximo paso, al comienzo del tramo
    renderTramo(hecho, m2, corte, q, comp);
    hecho += m2;
  }
  envBajoPub = bajo.env;

  // ── Master: mezcla → DC → limitador con lookahead → 13 kHz → techo suave ──
  uint32_t cm0 = CICLOS();
  const float objF = aTocando ? 1.0f : 0.0f;
  const float inv = 1.0f / BUFFER_SAMPLES;
  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    float x = (float)n * inv;
    float gB = gB0 + (gBaseS - gB0) * x, gI = gI0 + (gIndS - gI0) * x, vv = v0 + (volS - v0) * x;
    fundido += (objF - fundido) * 0.003f;         // ~8 ms
    corteGate += ((estadoSub == E_ESPERA ? 0.0f : 1.0f) - corteGate) * 0.02f;   // el fin de la subida: de golpe (~1 ms, sin clic)
    float base = bufBase[n] * gB;
    float g = vv * fundido * corteGate * MASTER * 1.6f;
    const float gG = vv * fundido * MASTER * 1.6f;          // el golpe y su cola: fuera del corte
    float l = (base + bufIndL[n] * gI + oscL[n]) * g + golpeL[n] * gG, r = (base + bufIndR[n] * gI + oscR[n]) * g + golpeR[n] * gG;
    float yl = l - dcX1L + 0.9985f * dcY1L; dcX1L = l; dcY1L = yl; l = yl;
    float yr = r - dcX1R + 0.9985f * dcY1R; dcX1R = r; dcY1R = yr; r = yr;
    float pk = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
    if (pk > medPico) medPico = pk;
    if (pk > limEnv) limEnv = pk; else limEnv = pk + (limEnv - pk) * limRel;
    float limObj = (limEnv > TECHO) ? (TECHO / limEnv) : 1.0f;
    limGain += (limObj - limGain) * 0.020f;        // ganancia suavizada: un escalón por muestra es un clic
    float dl = limDlyL[limDlyIdx], dr = limDlyR[limDlyIdx];
    limDlyL[limDlyIdx] = l; limDlyR[limDlyIdx] = r;
    limDlyIdx++; if (limDlyIdx >= LIM_LOOK) limDlyIdx = 0;
    if (limGain < medLimMin) medLimMin = limGain;
    l = pasoBQ(techoL, dl * limGain); r = pasoBQ(techoR, dr * limGain);
    if (fabsf(l) > 0.95f || fabsf(r) > 0.95f) medTecho = medTecho + 1;
    l = techoSuave(l); r = techoSuave(r);
    int32_t li = (int32_t)(l * 32000.0f), ri = (int32_t)(r * 32000.0f);
    salida[2 * n]     = (int16_t)(li > 32767 ? 32767 : li < -32768 ? -32768 : li);
    salida[2 * n + 1] = (int16_t)(ri > 32767 ? 32767 : ri < -32768 ? -32768 : ri);
  }
  medSec[4] += (CICLOS() - cm0) >> 6;
}

// ==============================================================================================================================================
// MATRIZ 32×8 (tarea de LEDs, core 0) — 8 escenas psicodélicas amarradas al tempo
// ==============================================================================================================================================
// Todo el movimiento corre en TIEMPO MUSICAL (negras), no en segundos: el túnel avanza un anillo
// por negra, la espiral gira con el pulso, el damero se desliza al tempo. Encima reaccionan a lo
// que suena: el BOMBO ensancha las líneas y agranda las figuras, la ENVOLVENTE real del bajo
// abre los contornos del plasma, y cada golpe industrial dispara su figura o su onda.
// Color de cada pista (para las figuras y los LEDs de la placa)
const uint8_t HUE_EV[NUM_EV] = { 130, 45, 160, 0, 110, 20, 200, 24, 4, 180 };   // HAT MARTILLO PISTÓN CLAP ABIERTO TOM KEYS BOMBO BAJO ARPEGIO

inline int xy(int x, int y) {
  if (x < 0 || x >= MATRIZ_ANCHO || y < 0 || y >= MATRIZ_ALTO) return -1;
  if (MATRIZ_ESPEJO_X) x = MATRIZ_ANCHO - 1 - x;
  if (MATRIZ_ESPEJO_Y) y = MATRIZ_ALTO - 1 - y;
  int i;
  if (MATRIZ_POR_COLUMNAS) { int yy = (MATRIZ_SERPENTINA && (x & 1)) ? MATRIZ_ALTO - 1 - y : y; i = x * MATRIZ_ALTO + yy; }
  else                     { int xx = (MATRIZ_SERPENTINA && (y & 1)) ? MATRIZ_ANCHO - 1 - x : x; i = y * MATRIZ_ANCHO + xx; }
  return STATUS_LEDS + i;
}
inline void pix(int x, int y, const CRGB &c) { int i = xy(x, y); if (i >= 0) leds[i] += c; }
// Punto con sub-píxel en los dos ejes: lo que se mueve se desliza en vez de saltar de LED en LED
void puntoSuave(float x, float y, const CRGB &c) {
  int x0 = (int)floorf(x), y0 = (int)floorf(y);
  float fx = x - x0, fy = y - y0;
  pix(x0,     y0,     c.scale8((uint8_t)((1 - fx) * (1 - fy) * 255)));
  pix(x0 + 1, y0,     c.scale8((uint8_t)(fx * (1 - fy) * 255)));
  pix(x0,     y0 + 1, c.scale8((uint8_t)((1 - fx) * fy * 255)));
  pix(x0 + 1, y0 + 1, c.scale8((uint8_t)(fx * fy * 255)));
}
inline void borrarMatriz() { for (int i = STATUS_LEDS; i < NUM_LEDS; i++) leds[i] = CRGB(0, 0, 0); }
inline void fundirMatriz(uint8_t c) { fadeToBlackBy(leds + STATUS_LEDS, MATRIZ_LEDS, c); }

float gananciaMatriz = 1.0f;
void limitarCorrienteMatriz() {
  uint32_t suma = 0;
  for (int i = STATUS_LEDS; i < NUM_LEDS; i++) suma += leds[i].r + leds[i].g + leds[i].b;
  float carga = (float)suma / ((float)MATRIZ_LEDS * 3.0f * 255.0f);
  float objetivo = (carga > CARGA_MAXIMA) ? (CARGA_MAXIMA / carga) : 1.0f;
  if (objetivo < gananciaMatriz) gananciaMatriz = objetivo;                 // ataque: al tiro
  else gananciaMatriz += (objetivo - gananciaMatriz) * RECUPERA_LED;        // recuperación: lenta
  if (gananciaMatriz < 0.999f) {
    uint8_t g = (uint8_t)(gananciaMatriz * 255.0f);
    for (int i = STATUS_LEDS; i < NUM_LEDS; i++) leds[i].nscale8(g);
  }
}

// ─── Estado de los visuales ─────────────────────────────────
#define NUM_ESCENAS 8
#define E_TUNEL     0
#define E_PLASMA    1
#define E_CALEIDO   2
#define E_ESPIRAL   3
#define E_FIGURAS   4
#define E_MOIRE     5
#define E_ONDAS     6
#define E_DAMERO    7
int      escena = E_TUNEL;
uint32_t vistoGolpe[NUM_EV];
float    flash[NUM_EV];               // destello de cada pista (0..1)
bool     nuevo[NUM_EV];               // golpe en este frame
#define MAX_FIGURAS 24
Figura   figuras[MAX_FIGURAS];
uint32_t rngLed = 0xA5A5A5A5u;
static inline float lrnd() { rngLed ^= rngLed << 13; rngLed ^= rngLed >> 17; rngLed ^= rngLed << 5; return (rngLed >> 8) * (1.0f / 16777216.0f); }

float    T = 0.0f;                    // tiempo musical, en negras
float    fk = 0.0f, eb = 0.0f;        // golpe del bombo · envolvente del bajo
float    brilloEsc = 1.0f;
uint32_t bombosVistos = 0;
// Paleta y carácter de la escena: se sortean junto con ella, así la misma escena nunca se ve igual
uint8_t hueBase = 0; float hueSpan = 120.0f, dirE = 1.0f, paramE = 0.5f;

static inline float fs(float x) { float p = x * 0.15915494f; p -= floorf(p); return seno(p); }   // sen(x) con la tabla del audio
static inline float fc(float x) { return fs(x + 1.5707963f); }
static inline float frac(float x) { return x - floorf(x); }
inline void pon(int x, int y, float hue, float v, uint8_t sat = 255) {
  if (v <= 0.004f) return;
  if (v > 1.0f) v = 1.0f;
  pix(x, y, CHSV((uint8_t)(hueBase + (int)hue), sat, (uint8_t)(v * 255.0f * brilloEsc)));
}

// ── TÚNEL: anillos que salen del centro, uno por negra; el bombo los ensancha ──
void escenaTunel() {
  borrarMatriz();
  const float k = 0.30f + 0.25f * paramE, w = 0.28f + 0.35f * fk;
  for (int x = 0; x < MATRIZ_ANCHO; x++) for (int y = 0; y < MATRIZ_ALTO; y++) {
    float dx = x - 15.5f, dy = y - 3.5f, d = sqrtf(dx * dx + dy * dy);
    float f = d * k - T * dirE, s = frac(f);
    float v = s < w ? 1.0f - s / w : 0.0f;
    pon(x, y, floorf(f) * hueSpan * 0.25f + d * 3.0f, v);
  }
}

// ── PLASMA: cuatro ondas que interfieren, dibujadas como curvas de nivel ──
void escenaPlasma() {
  borrarMatriz();
  const float t = T * 0.8f, esc = 0.9f + 0.7f * eb + 0.4f * paramE;
  for (int x = 0; x < MATRIZ_ANCHO; x++) for (int y = 0; y < MATRIZ_ALTO; y++) {
    float dx = x - 15.5f, dy = y - 3.5f, d = sqrtf(dx * dx + dy * dy);
    float val = fs(x * 0.32f + t) + fs(y * 0.9f - t * 1.3f) + fs((x + y) * 0.21f + t * 0.6f) + fs(d * 0.55f - t * 1.7f);
    float c = val * esc + t * 0.5f * dirE, s = frac(c);
    float v = 1.0f - fabsf(s - 0.5f) * 2.0f; v = v * v * v;
    pon(x, y, floorf(c) * hueSpan * 0.3f + val * 20.0f, v * (1.0f + fk));
  }
}

// ── CALEIDOSCOPIO: un dibujo que gira en un triángulo, espejado ──
void escenaCaleido() {
  borrarMatriz();
  const float a = T * 0.5f * dirE, cr = fc(a), sr = fs(a), umbral = 0.25f - 0.35f * fk;
  for (int x = 0; x < MATRIZ_ANCHO; x++) for (int y = 0; y < MATRIZ_ALTO; y++) {
    float ax = fabsf(x - 15.5f), ay = fabsf(y - 3.5f);
    float u = fmodf(ax, 8.0f); if (u > 4.0f) u = 8.0f - u;     // espejo cada 8 columnas
    float px = u * cr - ay * sr, py = u * sr + ay * cr;
    float val = fs(px * (1.1f + paramE) + T) * fs(py * 1.3f - T * 0.7f) + 0.5f * fs((px + py) * 0.9f + T * 1.6f);
    float v = (val - umbral) * 2.0f;
    if (v > 0.0f) pon(x, y, ax * 6.0f + val * 80.0f + T * 12.0f, v);
  }
}

// ── ESPIRAL: brazos que giran con el pulso ──
void escenaEspiral() {
  borrarMatriz();
  const float brazos = 2.0f + floorf(paramE * 3.99f), w = 0.35f + 0.30f * fk;
  for (int x = 0; x < MATRIZ_ANCHO; x++) for (int y = 0; y < MATRIZ_ALTO; y++) {
    float dx = (x - 15.5f) * 0.35f, dy = y - 3.5f;
    float a = atan2f(dy, dx) * 0.15915494f, r = sqrtf(dx * dx + dy * dy);
    float s = frac(a * brazos + r * (0.5f + paramE * 0.6f) - T * dirE * 0.5f);
    float v = s < w ? 1.0f - fabsf(s / w * 2.0f - 1.0f) : 0.0f;
    pon(x, y, r * 25.0f + a * hueSpan + T * 10.0f, v);
  }
}

// ── FIGURAS: cada golpe suelta su figura (círculo, cuadrado, rombo, triángulo) que se abre ──
void soltarFigura(float cx, float cy, float vr, uint8_t tipo, uint8_t hue, float brillo) {
  int k = 0; float menor = 9.0f;
  for (int i = 0; i < MAX_FIGURAS; i++) { if (!figuras[i].viva) { k = i; break; } if (figuras[i].brillo < menor) { menor = figuras[i].brillo; k = i; } }
  Figura &f = figuras[k];
  f.viva = true; f.cx = cx; f.cy = cy; f.r = 0.0f; f.vr = vr; f.tipo = tipo; f.hue = hue; f.brillo = brillo;
}
void escenaFiguras() {
  fundirMatriz(70);
  const uint8_t g = hueBase;              // la paleta sorteada corre los colores de todas las figuras
  if (nuevo[EV_BOMBO])    soltarFigura(15.5f, 3.5f, 0.9f, 0, HUE_EV[EV_BOMBO] + g, 1.0f);
  if (nuevo[I_MARTILLO])  soltarFigura(4.0f + lrnd() * 24.0f, 3.5f, 0.6f, 1, HUE_EV[I_MARTILLO] + g, 0.9f);
  if (nuevo[I_HAT])       soltarFigura(lrnd() * 31.0f, lrnd() * 7.0f, 0.45f, 2, HUE_EV[I_HAT] + g, 0.6f);
  if (nuevo[I_PISTON])    soltarFigura(4.0f + lrnd() * 24.0f, 7.0f, 0.7f, 3, HUE_EV[I_PISTON] + g, 0.9f);
  if (nuevo[I_CLAP])      soltarFigura(15.5f, 3.5f, 0.75f, 2, HUE_EV[I_CLAP] + g, 0.8f);
  if (nuevo[I_ABIERTO])   soltarFigura(lrnd() * 31.0f, 0.0f, 0.5f, 2, HUE_EV[I_ABIERTO] + g, 0.5f);
  if (nuevo[I_TOM])       soltarFigura(4.0f + lrnd() * 24.0f, 7.0f, 0.55f, 0, HUE_EV[I_TOM] + g, 0.8f);
  if (nuevo[EV_ARP])      soltarFigura(lrnd() * 31.0f, lrnd() * 7.0f, 0.35f, 2, HUE_EV[EV_ARP] + g, 0.35f);
  if (nuevo[EV_KEYS]) {                                         // la melodía se ve: la altura de la nota es la posición
    float x = (float)(notaKeysPub - (KEYS_DESDE - 4)) * (31.0f / 24.0f);
    soltarFigura(x, 3.5f, 0.45f, 0, HUE_EV[EV_KEYS] + g, 0.9f);
  }
  for (int i = 0; i < MAX_FIGURAS; i++) {
    Figura &f = figuras[i]; if (!f.viva) continue;
    for (int x = 0; x < MATRIZ_ANCHO; x++) for (int y = 0; y < MATRIZ_ALTO; y++) {
      float dx = x - f.cx, dy = y - f.cy, d;
      switch (f.tipo) {
        case 0:  d = sqrtf(dx * dx + dy * dy); break;                              // círculo
        case 1:  d = fmaxf(fabsf(dx), fabsf(dy)); break;                           // cuadrado
        case 2:  d = fabsf(dx) + fabsf(dy); break;                                 // rombo
        default: d = fmaxf(fabsf(dx) * 0.866f + dy * 0.5f, -dy); break;            // triángulo
      }
      float v = 1.0f - fabsf(d - f.r) * 1.2f;
      if (v > 0.0f) pix(x, y, CHSV(f.hue + (uint8_t)(f.r * 6.0f), 240, (uint8_t)(v * f.brillo * 255.0f * brilloEsc)));
    }
    f.r += f.vr * (1.0f + 0.6f * fk); f.brillo *= 0.95f;
    if (f.r > 34.0f || f.brillo < 0.04f) f.viva = false;
  }
}

// ── MOIRÉ: dos sistemas de anillos que se cruzan; el patrón aparece en la interferencia ──
void escenaMoire() {
  borrarMatriz();
  const float c1x = 15.5f + 12.0f * fs(T * 0.37f), c1y = 3.5f + 3.0f * fs(T * 0.53f + 1.0f);
  const float c2x = 15.5f + 12.0f * fs(T * 0.29f + 2.5f), c2y = 3.5f + 3.0f * fs(T * 0.41f + 4.0f);
  const float k = 0.6f + 0.5f * paramE;
  for (int x = 0; x < MATRIZ_ANCHO; x++) for (int y = 0; y < MATRIZ_ALTO; y++) {
    float d1 = sqrtf((x - c1x) * (x - c1x) + (y - c1y) * (y - c1y));
    float d2 = sqrtf((x - c2x) * (x - c2x) + (y - c2y) * (y - c2y));
    int i1 = (int)floorf(d1 * k - T * dirE), i2 = (int)floorf(d2 * k + T * dirE * 0.5f);
    if ((i1 + i2) & 1) pon(x, y, i1 * hueSpan * 0.2f + i2 * 12.0f, 0.75f + 0.25f * fk);
  }
}

// ── ONDAS: cuatro trazos de osciloscopio; cada uno lo mueve una parte de la música ──
void escenaOndas() {
  fundirMatriz(110);
  const float act[4] = { eb, fmaxf(fmaxf(flash[I_MARTILLO], flash[I_PISTON]), flash[I_CLAP]), fmaxf(flash[I_HAT], flash[I_ABIERTO]), fmaxf(flash[EV_KEYS], flash[I_TOM]) };
  for (int k = 0; k < 4; k++) {
    float A = 0.8f + 2.6f * act[k] + 0.8f * fk;
    float fr = 0.22f + 0.11f * k + 0.10f * paramE, vel = (1.0f + 0.5f * k) * dirE * 1.5f;
    for (int j = 0; j < MATRIZ_ANCHO * 2; j++) {
      float x = j * 0.5f, y = 3.5f + A * fs(x * fr + T * vel + k * 1.7f);
      if (y < 0.0f) y = 0.0f; if (y > 7.0f) y = 7.0f;
      puntoSuave(x, y, CHSV((uint8_t)(hueBase + (int)(k * hueSpan * 0.25f + x * 2.0f)), 255, (uint8_t)(130 * brilloEsc)));
    }
  }
}

// ── DAMERO: un tablero que se deforma y se desliza al tempo; cada bombo lo invierte ──
void escenaDamero() {
  borrarMatriz();
  const float lado = 2.0f + floorf(paramE * 2.99f);
  const int inv = bombosVistos & 1;
  for (int x = 0; x < MATRIZ_ANCHO; x++) for (int y = 0; y < MATRIZ_ALTO; y++) {
    float u = x + 2.2f * fs(y * 0.9f + T) + T * dirE * 2.0f, v = y + 1.4f * fs(x * 0.35f - T * 0.8f);
    int cu = (int)floorf(u / lado), cv = (int)floorf(v / lado);
    if (((cu + cv) & 1) ^ inv) pon(x, y, cu * hueSpan * 0.1f + cv * 20.0f, 0.8f + 0.2f * fk);
  }
}

// ── Prueba de orientación del arranque ──
void pruebaOrientacion(unsigned long t) {
  borrarMatriz();
  pix(0, 0, CRGB(120, 0, 0));                                  // arriba-izquierda: ROJO
  pix(MATRIZ_ANCHO - 1, 0, CRGB(0, 120, 0));                   // arriba-derecha: VERDE
  pix(0, MATRIZ_ALTO - 1, CRGB(0, 0, 120));                    // abajo-izquierda: AZUL
  int x = (int)((t % 1500) * MATRIZ_ANCHO / 1500);             // columna que barre de izquierda a derecha
  for (int y = 0; y < MATRIZ_ALTO; y++) pix(x, y, CRGB(50, 50, 50));
}

// ── LEDs de la placa ──
float pulsoNegra = 0.0f;
const uint8_t HUE_BANCO[NUM_BANCOS] = { 160, 0, 96, 32 };   // Em–C azul · Cm–B♭ rojo · Bm–G verde · E–F ámbar
void ledsEstado(bool corre) {
  const float f[5] = { flash[EV_BOMBO], flash[EV_BAJO], fmaxf(flash[I_HAT], flash[I_ABIERTO]),
                       fmaxf(fmaxf(flash[I_MARTILLO], flash[I_PISTON]), fmaxf(flash[I_CLAP], flash[I_TOM])), fmaxf(flash[EV_KEYS], flash[EV_ARP]) };
  const uint8_t hue[5] = { HUE_EV[EV_BOMBO], HUE_EV[EV_BAJO], HUE_EV[I_HAT], HUE_EV[I_PISTON], HUE_EV[EV_KEYS] };
  for (int i = 0; i < 5; i++) leds[i] = CHSV(hue[i], 220, (uint8_t)(8 + 140 * f[i]));
  if (breakPub) { leds[0] = CRGB(90, 0, 0); leds[1] = CRGB(90, 0, 0); }
  if (estadoSubPub == E_SUBIDA && ((millis() / 300) & 1)) { leds[0] = CRGB(0, 0, 0); leds[1] = CRGB(0, 0, 0); }   // subida: rojo parpadeando
  if (estadoSubPub == E_ESPERA) { leds[0] = CRGB(200, 0, 0); leds[1] = CRGB(200, 0, 0); }                        // espera del drop: rojo fijo
  if (menuPub) {                                   // menú: LED 0..3 = banco elegido · LED 4 = patrón del keys (un color cada uno)
    for (int i = 0; i < 4; i++) leds[i] = (i == bancoPub) ? CRGB(CHSV(HUE_BANCO[i], 200, 200)) : CRGB(CHSV(HUE_BANCO[i], 255, 12));
    leds[4] = CHSV((uint8_t)(patKeysPub * 50), 220, 160);   // un color por patrón del keys
    leds[5] = CHSV(32, 255, (millis() / 250) & 1 ? 160 : 20);   // ámbar parpadeando: estás en el menú
    if (millis() < cargaAltaHasta) leds[5] = CRGB(140, 0, 140);
    return;
  }
  leds[5] = corre ? CRGB(CHSV(96, 230, (uint8_t)(30 + 150 * pulsoNegra))) : CRGB(CHSV(0, 230, 25));
  if (millis() < cargaAltaHasta) leds[5] = CRGB(140, 0, 140);
}

uint32_t vistoContador = 0, bloqueEscena = 0;
bool estabaTocando = false;
void elegirEscena() {
  int n = (int)(lrnd() * (NUM_ESCENAS - 1)); escena = (escena + 1 + n) % NUM_ESCENAS;   // nunca la misma
  hueBase = (uint8_t)(lrnd() * 255.0f);
  hueSpan = 40.0f + lrnd() * 215.0f;
  dirE    = lrnd() < 0.5f ? -1.0f : 1.0f;
  paramE  = lrnd();
  for (int i = 0; i < MAX_FIGURAS; i++) figuras[i].viva = false;
}

void renderLEDs() {
  static unsigned long ultimo = 0;
  unsigned long t = millis();
  if (t - ultimo < LED_REFRESH_MS) return;
  float dt = (ultimo == 0) ? 0.0f : (float)(t - ultimo) * 0.001f;
  if (dt > 0.1f) dt = 0.1f;
  ultimo = t;

  for (int ev = 0; ev < NUM_EV; ev++) {
    uint32_t g = golpes[ev];
    nuevo[ev] = (g != vistoGolpe[ev]); vistoGolpe[ev] = g;
    if (nuevo[ev]) flash[ev] = 0.5f + 0.5f * velGolpe[ev];
  }
  bool corre = tocandoPub;
  uint32_t cp = contadorPasos;
  if (cp != vistoContador) {
    vistoContador = cp;
    if ((pasoBasePub & 3) == 0) pulsoNegra = 1.0f;
    if ((cp >> 6) != bloqueEscena) { bloqueEscena = cp >> 6; elegirEscena(); }   // cada 4 compases
  }
  if (corre && !estabaTocando) elegirEscena();                                   // y en cada Play
  estabaTocando = corre;

  // Tiempo musical: avanza al tempo y cada bombo lo amarra a la negra (nunca se desfasa)
  T += dt * (corre ? bpmPub / 60.0f : 0.25f);
  if (nuevo[EV_BOMBO]) { T += (roundf(T) - T) * 0.6f; bombosVistos++; }
  fk = flash[EV_BOMBO]; eb = envBajoPub;
  brilloEsc = corre ? 1.0f : 0.35f;                                               // detenida: la escena sigue, tenue y lenta

  int e = escena;
  if (!corre && (e == E_FIGURAS || e == E_ONDAS)) e = E_TUNEL;                    // sin golpes no tendrían qué mostrar
  if (t < PRUEBA_MS) pruebaOrientacion(t);
  else switch (e) {
    case E_TUNEL:   escenaTunel();   break;
    case E_PLASMA:  escenaPlasma();  break;
    case E_CALEIDO: escenaCaleido(); break;
    case E_ESPIRAL: escenaEspiral(); break;
    case E_FIGURAS: escenaFiguras(); break;
    case E_MOIRE:   escenaMoire();   break;
    case E_ONDAS:   escenaOndas();   break;
    default:        escenaDamero();  break;
  }
  ledsEstado(corre);
  limitarCorrienteMatriz();
  FastLED.show();

  for (int ev = 0; ev < NUM_EV; ev++) flash[ev] *= 0.80f;
  pulsoNegra *= 0.80f;
}

// ==============================================================================================================================================
// CONTROLES (tarea de control, core 0, 1 kHz)
// ==============================================================================================================================================
bool bNivel[5] = { HIGH, HIGH, HIGH, HIGH, HIGH };
unsigned long bTiempo[5];

// Menú de modos (BTN2 + BTN4)
bool  enMenu = false;
int   bancoAntes = 0, patAntes = 0;     // lo que había antes del último botón: el combo de salida lo repone
bool  btn2Sorteo = false;               // BTN2 acaba de sortear un patrón (el combo de entrada lo deshace)
bool  ignorarHastaSoltar = false;       // tras el combo, nada actúa hasta soltar los dos botones

// Tempo con BTN4 + POT1, y el volumen que no se mueve al soltar
bool  tempoHold = false, tempoTocado = false;
float bpmInicio = BPM_INICIAL, potInicio = 0.0f, potTempo = 0.0f;
bool  volCongelado = false, volArriba = false;
const float BPM_POR_RECORRIDO = 120.0f;   // girar el POT1 de punta a punta = 120 BPM

float readPot(uint8_t pin) {
  uint32_t s = 0;
  for (int i = 0; i < 4; i++) s += analogRead(pin);   // 4 y no 8: cada analogRead del S3 cuesta decenas de µs
  return (float)(s >> 2) / 4095.0f;
}

// Pots con dos funciones (normal / panel del menú): al cambiar de panel cada pot queda "suelto" y
// no cambia nada hasta que lo muevas un poco; desde ahí su posición es el valor. Así entrar o salir
// del menú nunca hace saltar el volumen ni el bajo.
float potEntrada[4];
bool  potTomado[4] = { true, true, true, true };
const float UMBRAL_TOMA = 0.04f;
void soltarPots() { for (int i = 0; i < 4; i++) { potEntrada[i] = readPot(POT_PIN[i]); potTomado[i] = false; } }

// BTN5: mantener = break; mantener más de 2 s = queda silenciado hasta el próximo toque
bool breakFijo = false, ignorar5 = false;
unsigned long t5 = 0;
const unsigned long BREAK_FIJO_MS = 2000;

void pasoControl() {
  unsigned long ahora = millis();
  for (int b = 0; b < 5; b++) {
    bool nivel = digitalRead(BTN_PIN[b]);
    if (nivel == LOW && bNivel[b] == HIGH && (ahora - bTiempo[b]) > DEBOUNCE_MS) {
      bTiempo[b] = ahora;
      reqSemilla = reqSemilla * 31u + (uint32_t)micros();   // el instante exacto del dedo: la mejor entropía
      bool otro = (b == 1) ? (bNivel[3] == LOW) : (b == 3) ? (bNivel[1] == LOW) : false;
      if (otro) {                                            // ── combo BTN2 + BTN4: entrar o salir del menú
        if (!enMenu) { if (btn2Sorteo) reqDeshacer = reqDeshacer + 1; tempoHold = false; tempoTocado = false; }
        else { reqBanco = bancoAntes; reqPatKeys = patAntes; }   // lo que eligió el primer botón no vale
        enMenu = !enMenu; menuPub = enMenu;
        ignorarHastaSoltar = true;
        soltarPots();
      } else if (ignorarHastaSoltar) {
        // un botón más mientras se sueltan los del combo: no hace nada
      } else if (enMenu) {                                   // ── adentro del menú: cada botón es un modo
        bancoAntes = reqBanco; patAntes = reqPatKeys;
        if (b < 4) reqBanco = b;                             // BTN1..BTN4: banco de acordes
        else reqPatKeys = (reqPatKeys + 1) % NUM_PAT_KEYS;   // BTN5: el siguiente patrón del keys
      } else {
        switch (b) {
          case 0: reqTocar = !reqTocar; breakFijo = false; breakFijoPub = false; reqBreak = false; break;
          case 1: reqPatron = reqPatron + 1; btn2Sorteo = true; break;
          case 2: reqLinea = reqLinea + 1; break;
          case 3:                                            // tempo: desde donde está, sin saltos
            tempoHold = true; tempoTocado = false;
            bpmInicio = bpmPub; potInicio = readPot(POT_PIN[0]); potTempo = potInicio;
            break;
          case 4:
            if (breakFijo) {                               // durante la subida o la espera: el DROP
              breakFijo = false; breakFijoPub = false; reqBreak = false; ignorar5 = true; reqDrop = reqDrop + 1;
            } else { reqBreak = true; t5 = ahora; }
            break;
        }
      }
    }
    if (nivel == HIGH && bNivel[b] == LOW) {                 // soltar
      if (b == 4) { if (ignorar5) ignorar5 = false; else if (!breakFijo) reqBreak = false; }
      if (b == 1) btn2Sorteo = false;
      if (b == 3 && tempoHold) {
        tempoHold = false;
        if (tempoTocado) { volCongelado = true; volArriba = readPot(POT_PIN[0]) > pVol; }
      }
    }
    bNivel[b] = nivel;
  }
  if (ignorarHastaSoltar && bNivel[1] == HIGH && bNivel[3] == HIGH) ignorarHastaSoltar = false;
  if (!enMenu && reqBreak && !breakFijo && !ignorar5 && bNivel[4] == LOW && ahora - t5 > BREAK_FIJO_MS) {
    breakFijo = true; breakFijoPub = true;                   // 2 s apretado: empieza la SUBIDA
    reqSubida = reqSubida + 1;
  }

  static uint8_t scan = 0;
  int i = scan; scan = (scan + 1) & 3;
  float v = readPot(POT_PIN[i]);
  bool manda = potTomado[i];
  if (!manda && fabsf(v - potEntrada[i]) > UMBRAL_TOMA) { potTomado[i] = true; manda = true; }
  if (enMenu) {                                              // ── panel del menú
    if (manda) switch (i) {
      case 0: pAtkBajo += (v - pAtkBajo) * 0.3f; break;     // ataque del bajo
      case 1: pDecBajo += (v - pDecBajo) * 0.3f; break;     // caída del bajo
      case 2: {                                              // variación del patrón de acordes, con histéresis
        static float vAc = 0.0f; vAc += (v - vAc) * 0.3f;
        float f = vAc * NUM_VAR_AC; int z = reqVarAc;
        if (f < z - 0.15f || f > z + 1.15f) { z = (int)f; if (z > NUM_VAR_AC - 1) z = NUM_VAR_AC - 1; if (z < 0) z = 0; reqVarAc = z; }
        break;
      }
      case 3: {                                              // OSCILADOR: la posición es la altura, continua
        static float vOsc = 0.0f;
        float pos = reqOscPos;
        if (pos < 0.0f) vOsc = v;                           // saliendo del silencio parte donde está el pot, sin barrido
        else vOsc += (v - vOsc) * 0.10f;                    // suavizado: el ruido del ADC no hace vibrar la nota
        if (vOsc < OSC_APAGADO) { if (pos >= 0.0f && vOsc < OSC_APAGADO - 0.012f) pos = -1.0f; }
        else {
          // un 2 % de tope en cada punta: ahí la nota es la tónica EXACTA, fácil de encontrar con el dedo
          float p = (vOsc - OSC_APAGADO - 0.02f) / (1.0f - OSC_APAGADO - 0.04f);
          if (p < 0.0f) p = 0.0f; if (p > 1.0f) p = 1.0f;
          if (pos < 0.0f || fabsf(p - pos) > 0.0025f || ((p == 0.0f || p == 1.0f) && p != pos)) pos = p;   // zona muerta ~6 cents
        }
        reqOscPos = pos;
        break;
      }
    }
    return;
  }
  if (!manda && !(i == 0 && tempoHold)) return;
  switch (i) {
    case 0:
      if (tempoHold) {                                       // POT1 = tempo mientras BTN4 está apretado
        potTempo += (v - potTempo) * 0.3f;
        if (fabsf(potTempo - potInicio) > 0.015f) tempoTocado = true;
        if (tempoTocado) {
          float b = bpmInicio + (potTempo - potInicio) * BPM_POR_RECORRIDO;
          reqBPM = b < BPM_MIN ? BPM_MIN : (b > BPM_MAX ? BPM_MAX : b);
        }
      } else if (volCongelado) {                             // el volumen espera a que el pot lo alcance
        if (fabsf(v - pVol) < 0.02f || (v > pVol) != volArriba) volCongelado = false;
      } else pVol += (v - pVol) * 0.3f;
      break;
    case 1: pMez   += (v - pMez)   * 0.3f; break;
    case 2: pCorte += (v - pCorte) * 0.3f; break;
    case 3: pDens  += (v - pDens)  * 0.3f; break;
  }
}

// ==============================================================================================================================================
// SETUP
// ==============================================================================================================================================
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear    = true;
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;   // ≈ 12 ms de cola: los botones suenan en el acto
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
  FastLED.setBrightness(LED_BRIGHT);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_MAX_MA);
  FastLED.clear();
  FastLED.show();
}

void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);
  for (int b = 0; b < 5; b++) pinMode(BTN_PIN[b], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  prepararSintesis();
  uint32_t semilla = 0;
  for (int i = 0; i < 16; i++) semilla = semilla * 31u + (uint32_t)analogRead(POT_PIN[3]);
  nuevoPatronInd(semilla ^ (uint32_t)micros());
  for (int p = 0; p < 4; p++) {                              // los pots ya en su lugar al arrancar
    float v = readPot(POT_PIN[p]);
    if (p == 0) pVol = v; else if (p == 1) pMez = v; else if (p == 2) pCorte = v; else pDens = v;
  }
  corteS = pCorte; densS = pDens;

#ifdef SIMULADOR
  iniciarLEDs();
#endif

  i2s_init();
#if MEDIR
  Serial0.begin(115200);
  reqTocar = true;                                 // arranca tocando para medir sin tocar nada
#endif

#ifndef SIMULADOR
  // El audio tiene el core 1 para él solo; controles y matriz van al core 0.
  xTaskCreatePinnedToCore(audioTask,   "audio",   8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask, "control", 4096, NULL,  3, NULL, 0);
  xTaskCreatePinnedToCore(ledTask,     "leds",    4096, NULL,  2, NULL, 0);
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
// Presupuesto de un bloque: 128 muestras a 240 MHz = 696 000 ciclos. Si un bloque usa más del
// 80 %, el LED 5 se pone magenta medio segundo: así se distingue un problema de tiempo de uno de señal.
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
  for (;;) {
    pasoControl();
#if MEDIR
    static unsigned long tMed = 0;
    if (millis() - tMed >= 2000) {
      tMed = millis();
      uint32_t n = medN ? medN : 1;
      Serial0.printf("CPU audio: prom %.0f %%  max %.0f %%  | pico antes del limitador %.2f  ganancia min %.2f  muestras en techo %lu | BPM %.0f dens %.2f vol %.2f\n",
        100.0 * (double)medSuma * 64.0 / n / CICLOS_BLOQUE, 100.0 * (double)medMax / CICLOS_BLOQUE,
        (double)medPico, (double)medLimMin, (unsigned long)medTecho, (double)bpmPub, (double)pDens, (double)pVol);
      Serial0.printf("   por sección (%% del presupuesto): bombo+bajo %.0f  percusión %.0f  keys %.0f  arpegio %.0f  master %.0f  pasos %.0f\n",
        100.0*64*medSec[0]/n/CICLOS_BLOQUE, 100.0*64*medSec[1]/n/CICLOS_BLOQUE, 100.0*64*medSec[2]/n/CICLOS_BLOQUE,
        100.0*64*medSec[3]/n/CICLOS_BLOQUE, 100.0*64*medSec[4]/n/CICLOS_BLOQUE, 100.0*64*medSec[5]/n/CICLOS_BLOQUE);
      for (int k = 0; k < 6; k++) medSec[k] = 0;
      medMax = 0; medSuma = 0; medN = 0; medTecho = 0; medLimMin = 1.0f; medPico = 0.0f;
    }
#endif
    vTaskDelay(1);
  }
}
// Los LEDs se inicializan DENTRO de su tarea, en el core 0. FastLED manda el dato con el RMT y
// sus interrupciones corren en el núcleo que lo inicializó: hecho en setup() (que corre en el
// core 1) le robaba al audio hasta 30 % de un bloque mientras salían los 262 LEDs — medido con
// MEDIR: picos de 110–129 % del presupuesto, el DMA vacío, y eso se oía como clipeo.
void ledTask(void *)     { iniciarLEDs(); for (;;) { renderLEDs(); vTaskDelay(2); } }
void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }
#endif
