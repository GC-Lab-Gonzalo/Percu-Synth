// ==============================================================================================================================================
// PERCU-SYNTH — INDUSTRIAL MATRIZ v2 TRIP HOP (5 estilos, el 2.º es trip hop · un panel por instrumento · matriz 32×8 · datos por USB) — GC Lab Chile
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
// - USB (el mismo del Arduino IDE): datos de cada instrumento para las proyecciones (Web Serial)
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled      (las proyecciones leen por este puerto)
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
// Una banda electrónica completa en la placa: batería, bajo, melodías y un oscilador para tocar
// encima, en 5 estilos (industrial, trip hop, techno, synthwave y futurismo). Cada instrumento tiene
// su panel para callarlo, cambiarle el patrón y el timbre, y todo suena siempre en tono y a tiempo.
// La matriz de LEDs de 32×8 se mueve con la música.
//
// COPIA de industrial_matriz_v2 con UN cambio: el segundo estilo, que era TRANCE, ahora es TRIP HOP
// (los otros cuatro estilos están idénticos). Lo trae de triphop_viaje, la máquina dedicada al género:
//   - TEMPO y SWING propios: con el trip hop el POT2 del General va de 58 a 110 BPM (en vez de
//     70–170) y el POT3 de 54 a 72 % de swing (en vez de recto–70 %). Al pasar de estilo el tempo
//     salta a su rango con el pot donde está: el mismo pot que daba 128 en techno da 88 acá.
//   - ARRASTRE por pista: la caja entra ~22 ms tarde, el clap ~26, el rim ~10, el tom ~12, con una
//     pizca de variación humana; el bombo NUNCA se atrasa (es el tiempo). No cabe en la rejilla de
//     semicorcheas, así que es un planificador aparte que dispara el golpe en la muestra exacta.
//   - ARMONÍA: Fa# MENOR ARMÓNICA, i · VI · iv · V (F#m D Bm C#): el V mayor con la sensible.
//   - Trae el kit POLVO (nuevo, el de triphop_viaje: bombo blando, caja grande y oscura, hats
//     gastados; el BTN3 de la batería ahora recorre 6 kits), el bajo SUB y la melodía VIDRIO.
//   Lo que NO vino de triphop_viaje, a propósito: la capa de vinilo y la de voz (rechazadas allá).
//
// La segunda versión de industrial_matriz. El motor de sonido es el mismo (bombo de tres bandas,
// bajo de bajo_8_pasos, percusión de 6 pistas con DENSIDAD, keys + arpegio con chorus y eco
// ping-pong, oscilador con LFO libre), pero ordenado en CUATRO INSTRUMENTOS que se
// tocan por separado, cada uno con su panel:
//
//   BATERÍA    bombo + 6 pistas (hat · caja · perc · clap · hat abierto · tom)
//   BAJO       una voz monofónica que sigue la fundamental del acorde
//   MELODÍAS   keys (una nota a la vez) + arpegio con las notas del acorde
//   OSCILADOR  dos osciladores que suenan continuo, con la nota cuantizada a la escala del estilo
//
// Y 5 ESTILOS: INDUSTRIAL · TRIP HOP · TECHNO · SYNTHWAVE · FUTURISMO. Cada estilo es una tabla:
// tonalidad, escala, progresión de acordes, 4 patrones de batería, 4 de bajo, 4 de melodía, un
// fill, y el kit y los timbres que trae por defecto. Todo se escribe en GRADOS de la escala y
// relativo al acorde que suena: ningún botón puede sacar una nota de tono ni un golpe de tiempo.
// Las secuencias corren SIEMPRE; el play/stop de cada panel sólo calla o deja sonar a ese
// instrumento (por eso es "falso": al volver entra en tiempo, nunca se corrió).
//
// PROYECCIONES (Resonancia): 60 veces por segundo sale por el USB la línea `R` de
// mezclador_pistas/trance_pistas, así Resonancia la trata como una placa más, con 4 pistas:
// batería · bajo · melodías · oscilador. Los golpes los cuenta el secuenciador (exactos, no
// detectados), incluidos bombo, caja y hats por separado. Con LINEA_PROYECCION 1 sale además una
// línea `P` con el detalle de cada instrumento (cada pista de la batería, notas, estilo, panel…).
// Serial.setTxTimeoutMs(0): si nadie escucha se descarta sin bloquear, y se manda desde la tarea
// de control (core 0), nunca desde el audio.
//
// MATRIZ 32×8: las ocho escenas del v1 (TÚNEL · PLASMA · CALEIDOSCOPIO · ESPIRAL · FIGURAS ·
// MOIRÉ · ONDAS · DAMERO), sorteadas cada 4 compases, en cada Play y en cada cambio de estilo.
// Al encender, 3 s de PRUEBA DE ORIENTACIÓN (ver README).
//
// ARQUITECTURA: audio en su propia tarea en el CORE 1; botones, pots y USB en una tarea a 1 kHz y
// la matriz en otra de menor prioridad, las dos en el CORE 0. Los controles sólo dejan PEDIDOS
// que el audio aplica en el borde de un buffer.
// ==============================================================================================================================================
// FUNCIONAMIENTO — 5 PANELES. Un botón = una función, en el flanco de presión.
// ==============================================================================================================================================
// EN TODOS LOS PANELES:  BTN4 → siguiente panel (General → Bajo → Batería → Melodías → Oscilador)
//                        BTN5 → break (calla bombo y bajo) / vuelta del break. MANTENIDO más de 2 s:
//                               SUBIDA de 16 compases que termina en un golpe y espera en silencio;
//                               el próximo toque del BTN5 es el DROP (todo entra desde el "1")
//
//   Al cambiar de panel cada pot queda "suelto" y no cambia nada hasta que lo muevas un poco (sin
//   saltos). El LED 5 dice el panel: verde General · rojo Bajo · ámbar Batería · cian Melodías ·
//   azul Oscilador. Las secuencias corren siempre: el BTN1 de cada instrumento es un play/stop "falso".
//
// GENERAL     BTN1 play/stop (arranca en el "1") · BTN2 estilo (siguiente de los 5) · BTN3 fill al final del compás
//             POT1 volumen · POT2 tempo (70–170; con TRIP HOP 58–110) · POT3 swing (recto → 70 %;
//             con TRIP HOP 54 → 72 %) · POT4 pasa-bajos general
// BAJO        BTN1 suena/calla · BTN2 patrón (4 por estilo) · BTN3 tipo de bajo (SIERRAS · SUB · ÁCIDO · REESE)
//             POT1 volumen · POT2 filtro · POT3 ataque (1–150 ms) · POT4 caída (40 ms – 1.2 s)
// BATERÍA     BTN1 suena/calla · BTN2 patrón (4 por estilo) · BTN3 kit (INDUSTRIAL · NEXO · 909 · 808 · GLITCH · POLVO)
//             POT1 volumen · POT2 filtro · POT3 largo de los golpes · POT4 DENSIDAD: abajo del todo
//             la fábrica calla. El bombo no depende de ella
// MELODÍAS    BTN1 suena/calla · BTN2 patrón (4 por estilo) · BTN3 instrumento (KEYS · PLUCK · LEAD · VIDRIO)
//             POT1 volumen · POT2 filtro · POT3 ataque · POT4 caída
// OSCILADOR   BTN1 encender/apagar · BTN2 timbre (SIERRAS · CUADRADAS · SUAVE) · BTN3 octava (normal · +1 · −1)
//             POT1 NOTA de la escala del estilo. Son 15 grados, de una octava abajo a una arriba de la
//             tónica (al centro) · POT2 filtro · POT3 profundidad del LFO del filtro · POT4 envío al delay.
//             Sin volumen: un compresor propio lo deja parejo en la mezcla
//
// LEDs de la placa: 0 bombo · 1 bajo · 2 hats · 3 caja/perc/clap/tom · 4 melodías — un instrumento
// callado se ve en rojo tenue. LED 5 = color del panel con el pulso de cada negra; MAGENTA si un
// bloque de audio usa más del 80 % de su tiempo. Con BREAK, 0 y 1 en rojo fijo.
//
// MODO DE USO:
// 1. En el panel General (LED 5 verde) aprieta BTN1: suenan batería, bajo y melodías. POT1 es
//    el volumen y POT2 el tempo.
// 2. BTN2 cambia de estilo. BTN3 mete un fill al final del compás.
// 3. BTN4 pasa al panel siguiente (Bajo, Batería, Melodías, Oscilador). En cada uno, BTN1 calla
//    o deja sonar ese instrumento, BTN2 cambia el patrón y BTN3 el timbre o el kit.
// 4. El oscilador parte apagado: en su panel, BTN1 lo enciende y el POT1 elige la nota.
// 5. BTN5 hace un break. Mantenido más de 2 s viene la subida; el próximo toque es el drop.
// ==============================================================================================================================================

// El core de Arduino compila con -Os (tamaño). Este sketch es casi todo bucles de audio: con O2
// (velocidad) el mismo código usa bastante menos CPU por bloque (medido en industrial_matriz).
#pragma GCC optimize ("O2")
#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <math.h>
#include <string.h>

// ─── Tipos (arriba del todo para que el IDE de Arduino genere bien los prototipos) ───
struct Filtro { float ic1, ic2, a1, a2, a3, k; };              // SVF de Zavalishin (TPT)
struct BQ     { float b0, b1, b2, a1, a2, z1, z2; };           // biquad RBJ (DF2 transpuesta)
struct VozBombo { bool viva; float f, fObj, fCoef, ph, env, knock, click, eC, kC, cC, amp, punch, kAmt, cAmt, sat; BQ bp, hp; };
struct VozInd {
  bool  viva;
  float amp, gL, gR;
  float ph[2], inc[2], a[2];         // capa TONAL: hasta dos parciales
  float env, eC;
  float caida, caidaC, cuanto;       // caída de pitch del impacto
  float nz, nC, nAmt;                // capa de RUIDO con su propia envolvente
  float fc0, fc1, barre, barreInc, q;
  Filtro s;
  BQ    banda;                       // pasa-altos de carril: nadie le pisa el grave al bombo
  bool  conBanda;                    // los de puro ruido no lo necesitan: su pasa-banda ya saca el grave
};
struct VozPluck { bool viva, atk; int nota; float f1, inc1, inv1, amp, fenv, base, oct, q, eC, gL, gR; Filtro s; };   // arpegio
struct Figura { bool viva; float cx, cy, r, vr, brillo; uint8_t tipo, hue; };   // escena FIGURAS
struct SegAc { int8_t grado; uint8_t compases; };              // un acorde de la progresión y cuántos compases dura

// Un golpe de percusión: TODO lo que define su sonido. El motor (renderInd) es uno solo; un kit es
// una tabla de 6 de éstos + los números del bombo.
struct Golpe {
  float amp;
  float f, r2, a0, a1;               // capa tonal: frecuencia, razón de la 2.ª parcial (0 = sin), niveles (a0 = 0: sin capa tonal)
  float eDec, cuanto, caidaDec;      // caída del tono (tau), cuánto cae el pitch y en qué tau
  float nAmt, nDec, nAcc;            // ruido: nivel, tau normal y tau con acento
  float fc0, fc1, barre, q;          // pasa-banda del ruido: de fc0 a fc1 en `barre` s
  float hp;                          // pasa-altos de carril (0 = sin)
  float pan, panRnd;
};
struct Kit {
  const char *nombre;
  // BOMBO en tres bandas (drum_poder): dónde aterriza la fundamental, desde cuánto más arriba
  // parte y en cuánto cae, caída, nivel del 2.º armónico (el PECHO), mazo de RUIDO pasa-banda,
  // click, y saturación propia (el bombo tiene UNA parcial fuerte: acá sí se puede saturar).
  float kFreq, kRatio, kDrop, kDec, kPunch, kKnock, kKnockF, kClick, kSat;
  Golpe g[6];
};
struct TimbreBajo { const char *nombre; float sierras, detune, sub, seno, drive, envOct, qMul, fenvRel, nivel; };
struct TimbreMel  { const char *nombre; uint8_t onda; float corte, envOct, q, fenvTau, nivel; };
#define NUM_SEG 4
struct Estilo {
  const char *nombre;
  uint8_t hue;
  int8_t  transp;                    // semitonos sobre La
  uint8_t escala;
  SegAc   prog[NUM_SEG];
  uint8_t kit, tBajo, tMel, tOsc;    // timbres que trae el estilo (después se cambian en cada panel)
  const char *bajo[4][2];            // 4 patrones: compás A (tres veces) y B (el 4.º)
  const char *bateria[4][7];         // 4 patrones: bombo + las 6 pistas, un compás de 16
  const char *fill[7];               // los 4 últimos pasos del compás, bombo + 6 pistas
  const char *keys[4];               // 4 patrones de melodía: 2 compases (32)…
  const char *arp[4];                // …y la figura del arpegio que va con cada uno (16)
  // Lo propio del TRIP HOP (triphop_viaje). Van AL FINAL para que los estilos que no lo escriben
  // queden en cero = como en industrial_matriz_v2: tempo 70–170, swing recto–70 %, sin arrastre.
  uint8_t bpmMin, bpmMax;            // rango del POT2 del General con este estilo (0 = el de siempre)
  float   swingMin, swingMax;        // rango del POT3 del General (0 = 50 % … 70 %)
  float   arrastre[6];               // ms que cada pista (HAT CAJA PERC CLAP ABIERTO TOM) entra TARDE. El bombo, nunca
};
// Un golpe pendiente: el ARRASTRE hace que la caja entre unos ms tarde, y eso no cabe en la rejilla
// de semicorcheas. Se dispara con precisión de muestra desde el bucle de bloques.
struct Pendiente { bool viva; int pista; float vel, largo, faltan; bool acento; };
// La parte de los controles que el audio lee entera al borde de cada buffer (el combo la repone)
struct Ctl {
  bool tocar, repeat, brk;
  int  estilo;
  bool drumOn, bajoOn, melOn, oscOn;
  int  patDrum, kit; bool kick, mitad;
  int  patBajo, tBajo, octBajo; bool glide;
  int  patMel, tMel, octMel; bool arp;
  int  tOsc, octOsc;                 // octOsc: 0 normal · 1 +1 octava · 2 −1 octava
};

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
const bool MATRIZ_POR_COLUMNAS = true;    // false = el dato recorre filas de 32
const bool MATRIZ_SERPENTINA   = true;    // false = todas las columnas (o filas) en el mismo sentido
const bool MATRIZ_ESPEJO_X     = false;   // invierte izquierda ↔ derecha
const bool MATRIZ_ESPEJO_Y     = false;   // invierte arriba ↔ abajo

// ══════════════════════════════════════════════════════════════════════════════════════════════
// PRESUPUESTO DE CORRIENTE DE LA MATRIZ  ← se toca si cambias la alimentación
// ══════════════════════════════════════════════════════════════════════════════════════════════
// Colgada del 5 V de la placa o del USB, un frame muy encendido hunde el riel y CORTA EL AUDIO
// (lección de bajo_8_pasos_fsr_clock). Con fuente propia de 5 V / 4 A sube MATRIZ_MAX_MA a 2500.
const float MATRIZ_MAX_MA = 450.0f;
const float MA_POR_LED    = 60.0f;
const float RECUPERA_LED  = 0.06f;
const float CARGA_MAXIMA  = MATRIZ_MAX_MA / ((float)MATRIZ_LEDS * MA_POR_LED * (LED_BRIGHT / 255.0f));
const unsigned long LED_REFRESH_MS = 20;
const unsigned long PRUEBA_MS      = 3000;
CRGB leds[NUM_LEDS];

// ─── Botones y pots ────────────────────────────────────────
const uint8_t BTN_PIN[5] = { 44, 42, 0, 45, 47 };   // BTN1..BTN5 (orden correcto de la placa)
const uint8_t POT_PIN[4] = { 1, 2, 8, 10 };
const unsigned long DEBOUNCE_MS = 25;

// ─── USB: datos para las proyecciones ───────────────────────
#ifndef LINEA_PROYECCION
#define LINEA_PROYECCION 0                  // línea `P`: detalle de cada pista (notas, timbres, estados), ver README
#endif
#define LINEA_RESONANCIA 1                  // líneas `I` y `R` de mezclador_pistas: las lee Resonancia
const unsigned long LINEA_MS = 16;          // ~60 por segundo

// ==============================================================================================================================================
// SONIDO — lo fijo se ajusta acá
// ==============================================================================================================================================
// MEDIR 1 = diagnóstico: arranca tocando y cada 2 s imprime por Serial0 (el conector UART de la
// placa) cuánto CPU usa el audio y cuánto trabaja el limitador. Uso normal: 0.
#ifndef MEDIR
#define MEDIR 0
#endif

const float BPM_MIN = 70.0f, BPM_RANGO = 100.0f;   // POT2 del General: 70 … 170 (el TRIP HOP trae el suyo: ver ESTILOS)
// Ganancia general con POT1 del General al máximo. Medido en la placa (MEDIR): con ×1.36 la mezcla
// llegaba a 1.6 antes del limitador, que bajaba hasta −4.7 dB con muestras en el techo = el clipeo.
// Con 0.70 ese mismo momento queda en ~0.82: el limitador sólo atrapa picos sueltos.
const float MASTER = 0.70f;
const float TECHO  = 0.89f;               // −1 dBFS: techo del limitador
#define LIM_LOOK 64
const float NIVEL_BOMBO = 0.78f;

// BAJO
const int   TONICA_BAJO = 33;             // La1 (55 Hz) + transp del estilo; abajo de Mi1 sube una octava
const float NIVEL_BAJO = 0.62f;
const float BAJO_GLIDE = 0.060f;          // portamento del BTN5 del panel del bajo (tau, s)
const float BAJO_ATK_MIN = 0.001f, BAJO_ATK_RANGO = 150.0f;   // 1 ms … 150 ms
const float BAJO_DEC_MIN = 0.040f, BAJO_DEC_RANGO = 30.0f;    // 40 ms … 1.2 s

// SIDECHAIN del bombo: cuánto agacha a cada capa
const float SC_BAJO = 0.35f, SC_IND = 0.40f, OSC_SC = 0.15f;
const float NIVEL_IND = 1.25f;

// ══ ARMONÍA ══ Todo se escribe en GRADOS de la escala: ninguna nota puede salir de tono.
#define NUM_MODOS 6
const int ESCALAS[NUM_MODOS][7] = {
  { 0, 2, 3, 5, 7, 8, 10 },   // 0 eólico (menor natural)
  { 0, 2, 3, 5, 7, 9, 10 },   // 1 dórico (sexta mayor)
  { 0, 1, 3, 5, 7, 8, 10 },   // 2 frigio (segunda menor)
  { 0, 2, 4, 6, 7, 9, 11 },   // 3 lidio (cuarta aumentada: el color "ciencia ficción")
  { 0, 1, 4, 5, 7, 8, 10 },   // 4 frigio dominante (árabe)
  { 0, 2, 3, 5, 7, 8, 11 },   // 5 menor armónica (la sensible y la 2.ª aumentada 6→7: el cine negro del trip hop)
};

// ══ TIMBRES DEL BAJO (BTN3 del panel del bajo) ══
//   sierras: nivel de las dos sierras (detune = razón entre ellas; 1 = una sola) · sub: cuadrada una
//   octava abajo · seno: fundamental redonda + su octava (UNA parcial fuerte: se puede saturar) ·
//   drive · envOct: cuánto abre el filtro cada nota · qMul: resonancia · fenvRel: el filtro se
//   cierra en esta fracción de la caída (más chico = más "cuac")
#define NUM_TBAJO 4
const TimbreBajo TBAJO[NUM_TBAJO] = {
  { "SIERRAS", 1.0f, 1.00521f, 0.34f, 0.0f, 1.55f, 3.0f, 1.0f, 0.55f, 1.00f },   // el de bajo_8_pasos
  // SUB: un seno puro a 41–55 Hz no lo reproduce un parlante chico (sonaba a silencio). El seno
  // tiene UNA parcial fuerte, así que se satura fuerte para sacarle 3.º y 5.º armónico audibles, y
  // una sierra suave por debajo le da definición; el peso sigue siendo la fundamental.
  { "SUB",     0.20f, 1.0f,    0.0f,  1.0f, 3.00f, 1.2f, 0.6f, 0.80f, 1.10f },
  { "ACIDO",   1.0f, 1.0f,     0.0f,  0.0f, 1.90f, 4.5f, 1.9f, 0.30f, 0.85f },   // una sierra, resonancia y envolvente a la 303
  { "REESE",   1.0f, 1.0116f,  0.20f, 0.0f, 1.30f, 1.4f, 0.8f, 0.80f, 0.95f },   // dos sierras a ±10 cents que baten
};

// ══ INSTRUMENTOS DE LAS MELODÍAS (BTN3 del panel de melodías) ══
//   onda: 0 pulso con PWM · 1 sierra · 2 dos sierras desafinadas · 3 seno + octava + 3.er armónico
//   corte: el filtro reposa en corte·f · envOct: cuánto lo abre cada nota · fenvTau: en cuánto se cierra
#define NUM_TMEL 4
const TimbreMel TMEL[NUM_TMEL] = {
  { "KEYS",   0, 4.5f, 2.5f, 1.1f, 0.070f, 1.00f },   // el keys del v1: pulso con el ancho respirando
  { "PLUCK",  1, 1.8f, 3.8f, 0.9f, 0.045f, 1.10f },   // cierra rápido: pulsado y oscuro
  { "LEAD",   2, 3.0f, 1.5f, 1.4f, 0.250f, 0.80f },   // dos sierras ±7 cents, el filtro se queda abierto
  { "VIDRIO", 3, 8.0f, 1.0f, 0.7f, 0.100f, 1.20f },   // casi un seno: limpio y frío (sólo armónicos enteros)
};
const float KEYS_PWM_HZ = 0.35f, KEYS_PWM_PROF = 0.20f;
const float CHORUS_MS = 7.0f, CHORUS_PROF_MS = 2.5f, CHORUS_HZ = 0.6f, CHORUS_MEZCLA = 0.55f;
const float ECO_FEEDBACK = 0.52f, ECO_MEZCLA = 0.55f;   // eco ping-pong a corchea con puntillo
const int   TONICA_KEYS = 57, KEYS_DESDE = 60;          // La3 + transp, subida de octava si queda bajo Do4
const int   TONICA_ARP  = 57;
const float NIVEL_KEYS = 0.13f, NIVEL_ARP = 0.13f;
const float MEL_ATK_MIN = 0.003f, MEL_ATK_RANGO = 100.0f;   // 3 ms … 300 ms
const float MEL_DEC_MIN = 0.060f, MEL_DEC_RANGO = 25.0f;    // 60 ms … 1.5 s
const int8_t TONOS_ARP[6] = { 0, 2, 4, 7, 9, 11 };   // dígitos del arpegio, en grados sobre la fundamental del acorde

// ══ OSCILADOR (panel 5) ══ dos osciladores ±6 cents, cada uno con su SVF resonante que sigue a la
// nota y que barre un LFO LIBRE de velocidad aleatoria (cada 1.5–6 s sortea una velocidad entre
// 0.08 y 9 Hz y se desliza hacia ella). La altura es un GRADO de la escala del estilo (BTN4/BTN5):
// siempre en tono, con un portamento corto entre grados.
#define NUM_TOSC 3
const char *NOMBRE_TOSC[NUM_TOSC] = { "SIERRAS", "CUADRADAS", "SUAVE" };
// Ajuste fino de volumen por timbre DESPUÉS del compresor: si uno queda más atrás o más adelante
// que los otros en la mezcla, se corrige acá (1.0 = como lo deja el compresor).
const float NIVEL_TOSC[NUM_TOSC] = { 1.00f, 1.00f, 1.00f };
const int   TONICA_OSC    = 57;          // La3 + transp del estilo: el grado 0
const int   GRADO_MIN = -7, GRADO_MAX = 14;
const float NIVEL_OSC     = 0.18f;
const float OSC_DETUNE    = 1.0035f;     // ±6 cents
const float OSC_GLIDE     = 0.030f;
const float OSC_ATAQUE    = 0.010f, OSC_SUELTA = 0.30f;
const float OSC_LFO_OCT   = 3.5f;        // profundidad máxima del LFO (POT3)
const float OSC_LFO_MIN_HZ = 0.08f, OSC_LFO_MAX_HZ = 9.0f;
const float OSC_LFO_CAMBIO_MIN = 1.5f, OSC_LFO_CAMBIO_MAX = 6.0f;
const float OSC_LFO_DESLIZ = 1.2f;
const float OSC_Q_CERRADO = 4.5f, OSC_Q_ABIERTO = 1.2f;
const float OSC_ECO_MAX   = 0.90f;       // envío al delay con el POT4 al máximo
// POT1 del oscilador = la nota: OSC_NOTAS grados de la escala del estilo, desde OSC_GRADO_BAJO
// (−7 = una octava bajo la tónica) hasta una octava arriba; el centro del pot es la tónica. Cada
// zona tiene histéresis para que el ruido del ADC no haga temblar la nota.
const int   OSC_GRADO_BAJO = -7, OSC_NOTAS = 15;
const float OSC_HISTERESIS = 0.15f;      // fracción de zona que hay que pasarse para cambiar de nota
// COMPRESOR propio del oscilador: el volumen no se toca, se mantiene parejo en la mezcla aunque
// el filtro, el LFO, la resonancia, el timbre o la altura cambien cuánto suena. La salida tiende a
// OSC_COMP_NIVEL y nunca pasa de él; lo muy suave sube como mucho ×1/OSC_COMP_RODILLA. Mide el
// nivel PROMEDIO (valor absoluto suavizado), no el pico: con el pico una cuadrada y un seno quedaban
// igualados en la punta pero la cuadrada se oía mucho más fuerte.
const float OSC_COMP_NIVEL = 0.08f, OSC_COMP_RODILLA = 0.35f;
const float OSC_COMP_ATK = 0.030f, OSC_COMP_REL = 0.250f;
const float OSC_CHORUS_MS = 12.0f, OSC_CHORUS_PROF_MS = 4.0f, OSC_CHORUS_HZ = 0.35f, OSC_CHORUS_MEZCLA = 0.70f;
const float OSC_ANCHO     = 0.80f;

// SUBIDA (BTN5 mantenido más de 2 s, en cualquier panel) y DROP (el siguiente toque del BTN5) — igual que en el v1
const int   SUBIDA_PASOS      = 256;
const float SUBIDA_OCTAVAS    = 4.0f;
const float NIVEL_SUBIDA_OSC  = 0.22f;
const float NIVEL_SUBIDA_RUIDO = 0.20f;
const float SUBIDA_DRIVE      = 2.5f;
const float NIVEL_CAJA_SUBIDA = 0.50f;
const float NIVEL_GOLPE       = 0.85f;
const float GOLPE_REVERB      = 0.55f;
const float REVERB_FB         = 0.84f;

// ══ KITS DE BATERÍA (BTN3 del panel de batería) ══
// Pistas (0..5): HAT · CAJA · PERC · CLAP · ABIERTO · TOM. Reglas que vienen de drum_poder y del v1:
// arriba de 1 kHz todo es RUIDO filtrado (pasa-banda, nunca un pasa-altos pelado); nada de parciales
// metálicos afinados (sonaban a olla); el clap es UN golpe de ruido (tres palmadas sonaban a clipeo);
// nada de ráfagas de clics; las dos parciales de una caja NO se saturan; caídas por banda (arriba de
// 1 kHz hasta 0.14 s; 250 Hz–1 kHz hasta 0.10; 100–250 hasta 0.22).
#define NUM_IND     6
#define I_HAT       0
#define I_CAJA      1
#define I_PERC      2
#define I_CLAP      3
#define I_ABIERTO   4
#define I_TOM       5
#define NUM_KITS 6
//                amp   f       r2     a0    a1    eDec   cuanto caidaD  nAmt  nDec   nAcc   fc0     fc1     barre  q     hp     pan   panRnd
const Kit KITS[NUM_KITS] = {
  { "INDUSTRIAL", 46.0f, 4.8f, 0.030f, 0.32f, 0.10f, 0.22f, 1200.0f, 0.26f, 1.80f, {     // el del v1
    { 0.30f, 8000.0f, 0,     0,     0,    0.01f, 0,     0.01f,  1.00f, 0.020f, 0.070f, 8000.0f, 8000.0f, 0,     0.55f, 0,     0.62f, 0     },   // aire comprimido
    { 0.55f, 190.0f, 1.56f, 0.40f, 0.20f, 0.045f, 0.18f, 0.035f, 1.25f, 0.090f, 0.120f, 2100.0f, 1300.0f, 0.09f, 1.2f,  150.0f, 0.50f, 0     },   // martillo sobre plancha
    { 0.50f, 95.0f,  0,     0.85f, 0,    0.050f, 0.50f, 0.020f, 0.95f, 0.070f, 0.100f, 2000.0f, 350.0f,  0.07f, 0.8f,  90.0f,  0.50f, 0.20f },   // pistón + escape
    { 0.34f, 1200.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.10f, 0.035f, 0.050f, 1200.0f, 1200.0f, 0,     1.2f,  0,     0.50f, 0.08f },   // clap de un golpe
    { 0.22f, 6800.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.00f, 0.080f, 0.110f, 6800.0f, 6800.0f, 0,     0.5f,  0,     0.38f, 0     },   // hat abierto
    { 0.45f, 118.0f, 0,     0.80f, 0,    0.160f, 0.30f, 0.030f, 0.35f, 0.012f, 0.012f, 900.0f,  900.0f,  0,     0.8f,  60.0f,  0.50f, 0.20f } } }, // tom
  { "NEXO", 39.0f, 4.8f, 0.034f, 0.30f, 0.42f, 0.20f, 1100.0f, 0.28f, 1.70f, {           // drum_poder: bombo 808 con pecho, arriba todo corto
    { 0.28f, 8000.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.00f, 0.018f, 0.075f, 8000.0f, 8000.0f, 0,     0.55f, 0,     0.62f, 0     },
    { 0.55f, 195.0f, 1.564f, 0.45f, 0.28f, 0.085f, 0.06f, 0.020f, 1.00f, 0.085f, 0.110f, 2300.0f, 2300.0f, 0,    1.5f,  150.0f, 0.50f, 0     },   // caja afinada + bordonera
    { 0.42f, 140.0f, 0,     0.70f, 0,    0.060f, 0.25f, 0.020f, 0.30f, 0.010f, 0.012f, 1500.0f, 1500.0f, 0,     0.8f,  80.0f,  0.50f, 0.20f },   // golpe de parche corto
    { 0.36f, 1200.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.10f, 0.060f, 0.080f, 1200.0f, 1200.0f, 0,     1.6f,  0,     0.50f, 0.08f },
    { 0.22f, 7200.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.00f, 0.075f, 0.100f, 7200.0f, 7200.0f, 0,     0.55f, 0,     0.38f, 0     },
    { 0.48f, 100.0f, 0,     0.80f, 0,    0.120f, 0.14f, 0.030f, 0.10f, 0.010f, 0.010f, 900.0f,  900.0f,  0,     0.8f,  60.0f,  0.50f, 0.20f } } },
  { "909", 50.0f, 6.0f, 0.018f, 0.24f, 0.30f, 0.45f, 1700.0f, 0.36f, 1.50f, {            // techno: bombo con pegada y click
    { 0.32f, 9000.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.00f, 0.015f, 0.050f, 9000.0f, 9000.0f, 0,     0.60f, 0,     0.62f, 0     },
    { 0.50f, 180.0f, 1.83f, 0.35f, 0.15f, 0.050f, 0.12f, 0.015f, 1.30f, 0.070f, 0.100f, 4500.0f, 2500.0f, 0.06f, 0.8f,  160.0f, 0.50f, 0     },
    { 0.45f, 85.0f,  0,     0.80f, 0,    0.040f, 0.60f, 0.015f, 0.50f, 0.030f, 0.040f, 1200.0f, 400.0f,  0.04f, 0.8f,  70.0f,  0.50f, 0.20f },
    { 0.38f, 1100.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.10f, 0.045f, 0.070f, 1100.0f, 1100.0f, 0,     1.2f,  0,     0.50f, 0.08f },
    { 0.24f, 7600.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.00f, 0.090f, 0.130f, 7600.0f, 7600.0f, 0,     0.55f, 0,     0.38f, 0     },
    { 0.45f, 130.0f, 0,     0.80f, 0,    0.100f, 0.35f, 0.020f, 0.25f, 0.010f, 0.010f, 1000.0f, 1000.0f, 0,     0.8f,  60.0f,  0.50f, 0.20f } } },
  { "808", 44.0f, 3.5f, 0.045f, 0.34f, 0.25f, 0.10f, 900.0f, 0.10f, 1.40f, {             // synthwave: bombo largo y redondo, caja grande
    { 0.26f, 7000.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.00f, 0.025f, 0.060f, 7000.0f, 7000.0f, 0,     0.50f, 0,     0.62f, 0     },
    { 0.55f, 180.0f, 1.556f, 0.40f, 0.25f, 0.100f, 0.08f, 0.020f, 1.30f, 0.140f, 0.140f, 3200.0f, 1800.0f, 0.12f, 0.7f, 140.0f, 0.50f, 0     },   // la caja grande del synthwave
    { 0.16f, 5500.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.00f, 0.018f, 0.030f, 5500.0f, 5500.0f, 0,     0.7f,  0,     0.40f, 0.10f },   // shaker
    { 0.40f, 1300.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.10f, 0.070f, 0.100f, 1300.0f, 1300.0f, 0,     1.0f,  0,     0.50f, 0.08f },
    { 0.20f, 6500.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.00f, 0.110f, 0.140f, 6500.0f, 6500.0f, 0,     0.5f,  0,     0.38f, 0     },
    { 0.48f, 110.0f, 0,     0.85f, 0,    0.180f, 0.45f, 0.060f, 0.10f, 0.010f, 0.010f, 800.0f,  800.0f,  0,     0.8f,  60.0f,  0.50f, 0.25f } } },  // syndrum
  { "GLITCH", 54.0f, 7.0f, 0.012f, 0.16f, 0.20f, 0.50f, 2200.0f, 0.40f, 1.60f, {         // futurismo: todo cortísimo y apretado
    { 0.30f, 10000.0f, 0,   0,     0,    0.01f, 0,     0.01f,  1.00f, 0.010f, 0.030f, 10000.0f, 10000.0f, 0,    0.70f, 0,     0.62f, 0.15f },
    { 0.50f, 240.0f, 1.55f, 0.25f, 0.12f, 0.030f, 0.30f, 0.010f, 1.40f, 0.040f, 0.060f, 6000.0f, 2500.0f, 0.03f, 1.0f,  200.0f, 0.50f, 0.10f },
    { 0.45f, 80.0f,  0,     0.80f, 0,    0.045f, 1.20f, 0.010f, 0.40f, 0.015f, 0.020f, 3000.0f, 800.0f,  0.03f, 0.8f,  60.0f,  0.50f, 0.30f },
    { 0.34f, 1500.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.10f, 0.025f, 0.040f, 1500.0f, 1500.0f, 0,     1.4f,  0,     0.50f, 0.20f },
    { 0.20f, 8500.0f, 0,    0,     0,    0.01f, 0,     0.01f,  1.00f, 0.050f, 0.080f, 8500.0f, 8500.0f, 0,     0.7f,  0,     0.38f, 0.15f },
    { 0.42f, 160.0f, 0,     0.70f, 0,    0.070f, 0.50f, 0.015f, 0.20f, 0.008f, 0.008f, 1200.0f, 1200.0f, 0,     0.8f,  60.0f,  0.50f, 0.30f } } },
  { "POLVO", 50.0f, 3.2f, 0.040f, 0.30f, 0.34f, 0.20f, 900.0f, 0.06f, 1.50f, {           // trip hop (el POLVO de triphop_viaje): bombo blando, caja oscura, hats gastados
    { 0.22f, 6500.0f, 0,    0,     0,    0.010f, 0,     0.010f, 1.00f, 0.022f, 0.045f, 6500.0f, 6500.0f, 0,     0.55f, 0,     0.60f, 0     },
    { 0.52f, 178.0f, 1.520f, 0.38f, 0.22f, 0.085f, 0.07f, 0.020f, 1.15f, 0.120f, 0.150f, 2400.0f, 1500.0f, 0.10f, 0.90f, 140.0f, 0.50f, 0  },   // caja grande y oscura
    { 0.34f, 320.0f, 0,     0.55f, 0,    0.045f, 0.35f, 0.015f, 0.55f, 0.030f, 0.040f, 2600.0f, 1400.0f, 0.03f, 1.10f, 300.0f, 0.38f, 0.18f },   // rim
    { 0.32f, 1400.0f, 0,    0,     0,    0.010f, 0,     0.010f, 1.10f, 0.085f, 0.110f, 1500.0f, 1100.0f, 0.05f, 1.10f, 0,     0.56f, 0.08f },
    { 0.20f, 6200.0f, 0,    0,     0,    0.010f, 0,     0.010f, 1.00f, 0.110f, 0.140f, 6200.0f, 5200.0f, 0.08f, 0.50f, 0,     0.40f, 0     },
    { 0.40f, 105.0f, 0,     0.82f, 0,    0.190f, 0.28f, 0.035f, 0.22f, 0.012f, 0.012f, 850.0f,  850.0f,  0,     0.80f, 60.0f,  0.50f, 0.20f } } },
};

// ══ ESTILOS (BTN2 del General) ══════════════════════════════════════════════════════════════════
// BATERÍA — 7 líneas de 16 (un compás): BOMBO · HAT · CAJA · PERC · CLAP · ABIERTO · TOM
//   X golpe fuerte · x golpe normal · . golpe fantasma · - nada
//   En las 6 pistas la letra es también la PRIORIDAD: con la DENSIDAD (POT4 del panel) baja, sólo
//   suenan las X; al subir entran las x y arriba del todo los fantasmas. Abajo del todo, nada. El
//   bombo no depende de la densidad.
// BAJO — 16 caracteres relativos a la FUNDAMENTAL DEL ACORDE del compás:
//   x fundamental · 3 tercera · 5 quinta · b séptima MENOR (si en ese acorde es mayor, la quinta) ·
//   o octava · - silencio. El patrón A suena tres compases y el B en el cuarto.
// MELODÍAS — keys: 32 caracteres (2 compases), 1..9 = grado contado desde la fundamental del acorde
//   (1 fundamental · 3 tercera · 5 quinta · 8 octava; 2, 4, 6 y 7 son las notas de paso que le dan
//   el color a cada modo), - silencio. Monofónico: cada nota toma el lugar de la anterior.
//   arpegio: 16 caracteres, 0..5 = nota del acorde (0 fund · 1 3.ª · 2 5.ª · 3 8.ª · 4 10.ª · 5 12.ª), - silencio.
// FILL — 7 líneas de 4: los pasos 13–16 del compás. REEMPLAZA a la batería (nunca se suma: dos golpes
//   del mismo tambor en la misma muestra se matan) y el "1" siguiente trae un plato.
// La progresión: cada acorde es un grado de la escala con su largo en compases; si en esa escala no
// da una tríada consonante se usa la más cercana (acordeDelModo).
#define NUM_ESTILOS 5
#define NUM_PAT 4
const Estilo ESTILOS[NUM_ESTILOS] = {
  // ── INDUSTRIAL ── Mi eólico, i·6 VI·2 (Em … C). Bombo a negras, el bajo vive entre los bombos.
  { "INDUSTRIAL", 0, -5, 0, { { 0, 6 }, { 5, 2 } }, 0, 0, 0, 0,
    { { "-xxx-xxx-xxx-xxx", "-xxx-xxx-xxo-x5b" },     // rodante
      { "--x---x---x---x-", "--x---x---o---5-" },     // contratiempo
      { "-xx--xx--xx--xx-", "-xx--xx--oo--55-" },     // galope
      { "--xx--xo--xx--x5", "--xx--xo--xb--3-" } },   // riff
    { { "X---X---X---X---", ".xXx.xXx.xXx.xXx", "----X-------X--.", "--x---.x--x---x.", "----x-------x---", "--X---.---X---.-", "----------x--x--" },   // fábrica
      { "X---X---X---X-x-", "x.x.x.x.x.x.x.x.", "----X--.-.--X---", "x--x--x---x--x--", "-------x----x---", "--x---x---x---x-", "---.------x---.x" },   // prensa
      { "X---X--xX---X---", "XxxX.xxXXxxX.xxX", "----X-------X-x-", "-x.--x.--x.--x.-", "----X-------X---", "------x-------x-", "-------------xx." },   // martillo
      { "X-----x---X-----", ".x.x.x.x.x.x.x.x", "----X-------X---", "x-----x.--x---x-", "-------X------x-", "--x-------x-----", "-----x------x.x-" } },  // ruptura
    { "X---", "----", "xxXX", "----", "----", "----", "x.x-" },
    { "1-1-5-1-1-1-6-5-1-1-5-1-8-7-6-5-",   // pulso con la sexta que cae a la quinta
      "3--3--3---3--3--3--3--3---2--1--",   // la tercera insistente
      "8--7--6---5--5--8--7--6---5--4-3",   // descenso lento
      "1-51-51-1-51-58-1-51-51-1-51-36-" }, // semicorcheas hipnóticas
    { "0-3-2-3-0-3-2-3-", "0123012301230123", "0303030303030303", "00-300-200-300-1" } },

  // ── TRIP HOP ── Fa# menor armónica, i·2 VI·2 iv·2 V·2 (F#m D Bm C#): el V MAYOR con la sensible
  //    es el cine negro. Viene de triphop_viaje (dialecto BRISTOL): bombo pesado y quebrado que
  //    nunca se atrasa, caja grande en 2 y 4 que entra TARDE (el arrastre de abajo), bajo SUB con
  //    notas largas, una melodía con mucho espacio. Tempo 58–110 y swing 54–72 % en sus pots.
  { "TRIP HOP", 192, -3, 5, { { 0, 2 }, { 5, 2 }, { 3, 2 }, { 4, 2 } }, 5, 1, 3, 2,
    { { "x-------x--5----", "x-------x--5-b--" },     // notas largas
      { "x------x--x-----", "x------x--x--5-3" },     // empujado
      { "x--x---5x--x--b-", "x--x---5x--o--5-" },     // quebrado
      { "x--5--b--x--5---", "x--5--b--o--5-3-" } },   // caminando
    { { "X------x--X-----", "x.x.x.x.x.x.x.x.", "----X-------X---", "--.---.---.-x-.-", "----------------", "--------------x-", "----------------" },   // pesado
      { "X--x------X--x--", "x-x-x-x-x-x-x-xx", "----X-------X--.", "-x---x---x---x--", "----x-------x---", "----------------", "-------------x-." },   // quebrado
      { "X-------x-x-----", "x..x..x..x..x..x", "--------X-------", "---.---x---.---x", "--------x-------", "------x-------x-", "----------------" },   // medio tiempo
      { "X--x--x---X-----", "xxx.xxx.xxx.xxx.", "----X--.----X-.-", ".-.-.-.-.-.-.-.-", "----x-------x---", "--------------x-", "----------x.----" } },   // boom bap
    { "X---", "x.x.", "..xX", "----", "---X", "----", "x-x-" },
    { "5-------3-------4---3---2-------",   // el suspiro que baja
      "1---3---5-------7-------5---3---",   // el acorde abierto, lento
      "8-----7-5-------6-----5-3-------",   // descenso con la sexta
      "--3-5-------2-3-----1-----------" }, // frase a contratiempo
    { "0---1---2---1---", "2---1---0-------", "0--2--1---3--1--", "0-------3-------" },
    58, 110, 0.54f, 0.72f,
    { 0.0f, 22.0f, 10.0f, 26.0f, 0.0f, 12.0f } },     // el arrastre del kit POLVO de triphop_viaje

  // ── TECHNO ── Sol frigio, i·6 II·2 (Gm … A♭). Hipnótico: un acorde casi siempre, la segunda
  //    menor como única sombra. Bajo ácido.
  { "TECHNO", 160, -2, 2, { { 0, 6 }, { 1, 2 } }, 2, 2, 0, 1,
    { { "-x-x-x-x-x-x-x-x", "-x-x-x-x-x-o-x-b" },     // rumble en semicorcheas sueltas
      { "x-xo-x5-x-bx-o-x", "x-xo-x5-x-bx-o-5" },     // ácido
      { "--x-x--x--x-x--x", "--x-x--x--x-o--5" },     // síncopa
      { "-xxx-xxx-xxx-xxx", "-xxx-xxb-xxx-xo5" } },   // rodante
    { { "X---X---X---X---", "x.xXx.xXx.xXx.xX", "----------------", "---x--x----x--x-", "----X-------X---", "--x---x---x---x-", "----------------" },
      { "X---X---X---X---", "XxXxXxXxXxXxXxXx", "----x-------x---", "-x--.x---x--.x--", "--------------x-", "--X---X---X---X-", "------------.---" },
      { "X---X---X---X--x", ".x.x.x.x.x.x.x.x", "-------x-------.", "x--x--x--x--x-x-", "----x-------x---", "--x---x---x---x-", "---------x------" },
      { "X---X---X---X---", "--x---x---x---x-", "----X-------X---", "-.x--.x--.x--.x-", "----x-------x---", "--X---X---X---X-", "-----.-------.--" } },
    { "X--x", "xxxx", "--xX", "x-x-", "----", "----", "----" },
    { "1-------1-----1-1-------1--5----",
      "1--1--1---1--1--1--1--1---2--1--",
      "5-------3-------1-------3-------",
      "1-5-8-5-1-5-8-5-1-5-8-5-1-5-2-1-" },
    { "0--0--0---0--0--", "0-0-3-0-0-0-2-0-", "00300020003000-1", "0-3-0-3-0-3-0-31" } },

  // ── SYNTHWAVE ── Re dórico, i·2 IV·2 i·2 VII·2 (Dm G Dm C): la sexta mayor del dórico es el IV
  //    mayor, el color de los 80. Bajo en corcheas, caja grande en 2 y 4.
  { "SYNTHWAVE", 213, -7, 1, { { 0, 2 }, { 3, 2 }, { 0, 2 }, { 6, 2 } }, 3, 3, 2, 2,
    { { "x-x-x-x-x-x-x-x-", "x-x-x-x-x-x-o-5-" },     // corcheas
      { "x-o-x-o-x-o-x-o-", "x-o-x-o-x-o-5-o-" },     // octavas
      { "x--x--x-x--x--x-", "x--x--x-x--x-o5-" },     // 3-3-2
      { "xxxxxxxxxxxxxxxx", "xxxxxxxxxxxxooo5" } },   // semicorcheas que empujan
    { { "X-------X-------", "x-x-x-x-x-x-x-x-", "----X-------X---", ".x.x.x.x.x.x.x.x", "----x-------x---", "--------------x-", "----------------" },
      { "X---X---X---X---", "x-x-x-x-x-x-x-x-", "----X-------X---", "-x-x-x-x-x-x-x-x", "----x-------x---", "------x-------x-", "-------------xx-" },
      { "X-----X-X-------", "xxxxxxxxxxxxxxxx", "----X-------X---", "-.-.-.-.-.-.-.-.", "------------x---", "--x-------x-----", "----------x-x-x-" },
      { "X-------X-x-----", "x-x-x-x-x-x-x-x-", "----X-------X--.", "...............x", "----X-------X---", "--------------X-", "-----------xx-x-" } },
    { "X---", "----", "x-x-", "----", "x-x-", "----", "xXxX" },
    { "8---7-5-----3-5-8---7-5---9-8---",
      "1---3---5---8---7---5---3---2---",
      "5-5-3-5---8-7-5-5-5-3-5---3-2-1-",
      "3-------5-------8---7---5-------" },
    { "0123012301230123", "0-1-2-3-2-1-0-1-", "0213021302130213", "0303030303030303" } },

  // ── FUTURISMO ── Do lidio, I·2 II·2 (C … D): la cuarta aumentada y el II mayor, el color
  //    ciencia ficción. Batería quebrada, bajo sub.
  { "FUTURISMO", 128, 3, 3, { { 0, 2 }, { 1, 2 } }, 4, 1, 3, 1,
    { { "x--x--x---x--x--", "x--x--x---x--o--" },     // quebrado
      { "-x5--x-b--x--o5-", "-x5--x-b--x--o53" },     // saltos
      { "x---o--5x---3-o-", "x---o--5x---3-55" },     // vaivén
      { "--xo--xo--5b--xo", "--xo--xo--5b--o5" } },   // síncopa
    { { "X--x--X---x-X---", "x.xxx.x.xx.xx.x.", "----X--x-.--X-.-", "-x--.--x-x--.-x-", "-------X------x-", "---.---x---.--x-", "--x--x--x---.---" },
      { "X-----X---X-----", "xxx.xxx.xxx.xxx.", "----X-------X---", "x--x--x--x--x-x-", "----x-------x---", "--.---.---.---x-", "------x------x--" },
      { "X--X--X---X--X--", ".x.x.x.x.x.x.x.x", "----X--.----X---", "-x.-x.-x.-x.-x.-", "-------x-------x", "---x-------x----", "-x-------x------" },
      { "X-x---X-x---X---", "x.x.xxx.x.x.xxx.", "----X-------X-xx", "--x---x---x---x-", "----X---------x-", "-x-----x-x-----x", "------x-------x." } },
    { "X-x-", "x.x.", "-x-X", "xxxx", "----", "----", "--xx" },
    { "1--5--8--9--5--1--5--8--9--7----",
      "8-5-2-8-5-2-8-5-7-4-2-7-4-2-7-4-",
      "1-2-3-5---4-3-2-1-2-3-5---8-----",
      "5--4--3--2--1-------5--6--5--4--" },
    { "0-2-5-2-0-3-1-4-", "0213021302130213", "3-1-2-0-3-1-2-4-", "0--30--20--30--1" } },
};

// Eventos (para los LEDs, la matriz y las proyecciones)
#define EV_KEYS     6
#define EV_BOMBO    7
#define EV_BAJO     8
#define EV_ARP      9
#define EV_OSC     10
#define NUM_EV     11

// Paneles
#define P_GENERAL   0
#define P_BAJO      1
#define P_DRUM      2
#define P_MEL       3
#define P_OSC       4
#define NUM_PANELES 5
const char *NOMBRE_PANEL[NUM_PANELES] = { "GENERAL", "BAJO", "BATERIA", "MELODIAS", "OSCILADOR" };

// ==============================================================================================================================================
// PEDIDOS ENTRE NÚCLEOS
// ==============================================================================================================================================
// control → audio
volatile Ctl      reqC;                            // se copia entera (memcpy) al borde de cada buffer
volatile uint32_t reqFill = 0, reqFillNo = 0, reqSubida = 0, reqDrop = 0;
// Pots de cada panel: la tarea de control los escribe ya suavizados; el audio los lee.
//   [P_GENERAL] vol · tempo · swing · pasa-bajos     [P_BAJO] vol · filtro · ataque · caída
//   [P_DRUM]    vol · filtro · largo · densidad      [P_MEL]  vol · filtro · ataque · caída
//   [P_OSC]     vol · filtro · LFO · delay
volatile float    pPot[NUM_PANELES][4] = {
  { 0.70f, 0.58f, 0.00f, 1.00f },
  { 0.88f, 0.35f, 0.138f, 0.408f },                // arrancan en el bajo de siempre (2 ms · 0.16 s)
  { 0.88f, 1.00f, 0.50f, 0.60f },
  { 0.88f, 0.50f, 0.00f, 0.43f },
  { 0.50f, 0.30f, 0.60f, 0.60f },                // POT1 = nota: al centro, la tónica
};

// audio → LEDs y USB
volatile uint32_t golpes[NUM_EV];
volatile float    velGolpe[NUM_EV];
volatile uint32_t pasoTotalPub = 0, contadorPasos = 0;
volatile float    envBajoPub = 0.0f, bpmPub = 128.0f, fasePub = 0.0f;
volatile float    nivelPub[5];                     // pico por bloque: batería · bajo · melodías · oscilador · master
volatile int      notaBajoPub = 0, notaKeysPub = 64, notaArpPub = 64, notaOscPub = -1, acordePub = 0, tonicaPub = 57;
volatile bool     tocandoPub = false, breakPub = false, fillPub = false, repeatPub = false;
volatile int      estadoSubPub = 0;
volatile uint32_t muestraPub = 0;
volatile uint32_t cargaAltaHasta = 0;
volatile uint32_t medMax = 0, medSuma = 0, medN = 0, medTecho = 0;   // MEDIR
volatile float    medLimMin = 1.0f, medPico = 0.0f;
volatile int      panelPub = 0;                    // lo escribe el control, lo leen los LEDs

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
static inline float pulso(float &f, float inc, float inv, float w) {   // PolyBLEP de ancho w: sierra − sierra desplazada
  float p2 = f + w; if (p2 >= 1.0f) p2 -= 1.0f;
  float a = 2.0f * f - 1.0f, b = 2.0f * p2 - 1.0f;
  if (f < inc) { float t = f * inv; a -= t + t - t * t - 1.0f; } else if (f > 1.0f - inc) { float t = (f - 1.0f) * inv; a -= t * t + t + t + 1.0f; }
  if (p2 < inc) { float t = p2 * inv; b -= t + t - t * t - 1.0f; } else if (p2 > 1.0f - inc) { float t = (p2 - 1.0f) * inv; b -= t * t + t + t + 1.0f; }
  f += inc; if (f >= 1.0f) f -= 1.0f;
  return (a - b) * 0.5f;
}
static inline float cuadrada(float &f, float inc, float inv) { return pulso(f, inc, inv, 0.5f); }
static inline float suave(float &f, float inc) {                // seno + octava: casi un seno, con algo de cuerpo
  f += inc; if (f >= 1.0f) f -= 1.0f;
  float p2 = f + f; if (p2 >= 1.0f) p2 -= 1.0f;
  return (seno(f) + 0.25f * seno(p2)) * 0.8f;
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
static inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
// Pot de volumen → ganancia: cuadrática, 1.3 arriba del todo (≈ +2 dB sobre el nivel del v1, que
// cae en el 88 %). Con 1.56 y los cuatro instrumentos al máximo el limitador bajaba 9 dB (simulado).
static inline float ganVol(float v) { return 1.3f * v * v; }
// Corte de un pasa-bajos de bus (batería y general): 150 Hz … 19 kHz; arriba del 92 % se abre del todo
static inline float corteBus(float p) { return 150.0f * exp2Rapido(7.0f * clamp01(p)); }
static inline float abiertoBus(float p) { return clamp01((p - 0.90f) / 0.08f); }

// Generador de composición — separado del ruido de audio
uint32_t rngComp = 0x9E3779B9u;
static inline float frnd() { rngComp ^= rngComp << 13; rngComp ^= rngComp >> 17; rngComp ^= rngComp << 5; return (rngComp >> 8) * (1.0f / 16777216.0f); }

// ==============================================================================================================================================
// ESTADO DE LA SÍNTESIS (lo toca sólo la tarea de audio)
// ==============================================================================================================================================
Ctl C;                                              // la copia de los controles de este bloque
int estilo = 0, kit = 0, tBajo = 0, tMel = 0, tOsc = 0;
VozBombo bombos[2]; int sigBombo = 0;              // dos instancias: la que sonaba se calla en 3 ms
struct { float f1, f2, fs, fsn, inc1, incObj, env, envF; bool atk, atkF; Filtro s; } bajo;
float dBajo, dBajoF, dRapido3, dRapido, dSc, kGlideBajo, kGlide16;
float incAtkBajo = 1.0f / 88.0f, incAtkMel = 1.0f / 132.0f, decMel = 0.20f, largoDrum = 1.0f;
float filtMel = 1.0f;
uint32_t semRuidoL = 12345u, semRuidoR = 67891u;
// Subida (como en el v1)
#define E_NORMAL 0
#define E_SUBIDA 1
#define E_ESPERA 2
int      estadoSub = E_NORMAL, subPasos = 0;
uint32_t vistoSubida = 0, vistoDrop = 0;
float    subF1 = 0.0f, subF2 = 0.31f, subLp = 0.0f, subTrem = 1.0f, faltanTramo = 0.0f, corteGate = 1.0f;
struct { float env, eC, nz, nC, ph1, ph2, amp; Filtro s; } cajaSub;
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
int   reverbViva = 0;
float golpeL[BUFFER_SAMPLES], golpeR[BUFFER_SAMPLES];
// Oscilador
struct { bool vivo; float f1, f2, inc, incObj, amp, obj, comp; Filtro s, s2; } osc;
float lfoFase = 0.0f, lfoVel = 2.0f, lfoVelObj = 2.0f, lfoFalta = 0.0f; uint32_t semLfo = 0x51F15EEDu;
#define OSC_CHORUS_N 2048
float oscChorusBuf[OSC_CHORUS_N]; int oscChorusIdx = 0; float oscChorusLfo = 0.0f;
const float octLfo = log2f(OSC_LFO_MAX_HZ / OSC_LFO_MIN_HZ);
const float kDeslizLfo = 1.0f - expf(-16.0f / (OSC_LFO_DESLIZ * SR));
float dOscGlide, kOscAtk, kOscSuelta;
float oscFiltS = 0.3f, oscLfoS = 0.6f, oscEcoS = 0.6f;
int   notaOscVista = -999, gradoPot = 7; bool oscOnVisto = false;
float oscCompEnv = 0.0f, oscCompG = 1.0f, kOscCompAtk, kOscCompRel;
float oscL[BUFFER_SAMPLES], oscR[BUFFER_SAMPLES], oscEco[BUFFER_SAMPLES];
VozInd ind[NUM_IND][2]; int sigInd[NUM_IND];       // un "parche" por pista con fast-kill al retriggear
float scObj = 0, sc = 0;
float prio[NUM_IND][16];                           // prioridad de cada golpe del patrón: suena si prio < densidad
#define MAX_PEND 12
Pendiente pend[MAX_PEND];                          // el ARRASTRE del trip hop: golpes que entran unos ms tarde

// Secuenciador
bool     aTocando = false, aBreak = false;
float    fundido = 0.0f;
float    sps = 0.0f, faltan = 0.0f;                // muestras por semicorchea · hasta el próximo paso
float    bpm = 128.0f, swingS = 0.5f;
uint32_t pasoTotal = 0xFFFFFFFFu;                  // semicorcheas desde el Play (el reloj maestro, nunca se corre)
uint32_t muestraGlobal = 0;
// Beat repeat, media velocidad y fill
bool     repAntes = false; uint32_t repAncla = 0;
bool     mitadAntes = false; int32_t mitadAncla = 0;
uint32_t vistoFill = 0, vistoFillNo = 0; bool fillPend = false, crashPend = false; uint32_t fillCompas = 0;
// Melodías
#define VOCES_ARP   2
VozPluck arps[VOCES_ARP];
struct { bool viva, atk; float f1, f2, inc1, inv1, inc2, inv2, amp, fenv, base, eC, lfo; Filtro s; } kv;   // KEYS: monofónico
#define CHORUS_N 1024
float    chorusBuf[CHORUS_N]; int chorusIdx = 0; float chorusLfo = 0.0f;
#define ECO_MAX 32768                              // 0.74 s: corchea con puntillo hasta 60 BPM
int16_t  ecoL[ECO_MAX], ecoR[ECO_MAX];
int      ecoIdx = 0, ecoN = 15000, ecoN2 = 15000; float ecoXf = 1.0f, ecoLpL = 0, ecoLpR = 0, ecoHpL = 0, ecoHpR = 0;
float    keysBuf[BUFFER_SAMPLES], ecoBufL[BUFFER_SAMPLES], ecoBufR[BUFFER_SAMPLES];
float    dKeysFenv, dArpFenv;
int      modo = 0, transp = -5, tonicaBajo = 28, ladoArp = 0;
int      estiloPrio = -1, patPrio = -1;

// Master y buses
float bufKick[BUFFER_SAMPLES], bufBajo[BUFFER_SAMPLES], bufIndL[BUFFER_SAMPLES], bufIndR[BUFFER_SAMPLES];
float bufMelL[BUFFER_SAMPLES], bufMelR[BUFFER_SAMPLES], bufFxL[BUFFER_SAMPLES], bufFxR[BUFFER_SAMPLES], bufSc[BUFFER_SAMPLES];
float corteBajoS = 0.35f, volS = 0.5f, densS = 0.6f, lpfS = 1.0f, filtDrumS = 1.0f;
float gDrumS = 1, gKickS = 1, gBajoS = 1, gMelS = 1, gOscS = 1;
Filtro fDrumL, fDrumR, fMasL, fMasR;
float dcX1L = 0, dcY1L = 0, dcX1R = 0, dcY1R = 0;
float limDlyL[LIM_LOOK], limDlyR[LIM_LOOK]; int limDlyIdx = 0;
float limEnv = 0.0f, limGain = 1.0f, limRel = 0.0f;
BQ techoL, techoR;
float nivAcum[5];

// Rango del POT2 del General con el estilo que suena: el de siempre, o el propio (TRIP HOP 58–110)
static inline float bpmMinDe(int e)   { return ESTILOS[e].bpmMax ? (float)ESTILOS[e].bpmMin : BPM_MIN; }
static inline float bpmRangoDe(int e) { return ESTILOS[e].bpmMax ? (float)(ESTILOS[e].bpmMax - ESTILOS[e].bpmMin) : BPM_RANGO; }

void setBPM(float b) {
  const float bMin = bpmMinDe(estilo), bMax = bMin + bpmRangoDe(estilo);
  if (b < bMin) b = bMin; if (b > bMax) b = bMax;
  bpm = b; sps = SR * 60.0f / bpm / 4.0f; bpmPub = b;
}

void prepararSintesis() {
  for (int i = 0; i <= TABLA; i++) SENO[i] = sinf(2.0f * PI_F * i / TABLA);
  dRapido3 = decaimiento(0.003f); dRapido = decaimiento(0.0015f); dSc = decaimiento(0.14f);
  dBajo = decaimiento(0.16f); dBajoF = decaimiento(0.16f * 0.55f);
  dArpFenv = decaimiento(0.07f); dKeysFenv = decaimiento(0.07f);
  kGlideBajo = 1.0f - decaimiento(BAJO_GLIDE); kGlide16 = 1.0f - powf(1.0f - kGlideBajo, 16.0f);   // el glide avanza cada 16 muestras
  dOscGlide = 1.0f - decaimiento(OSC_GLIDE); kOscAtk = 1.0f - decaimiento(OSC_ATAQUE); kOscSuelta = 1.0f - decaimiento(OSC_SUELTA);
  kOscCompAtk = 1.0f - decaimiento(OSC_COMP_ATK); kOscCompRel = 1.0f - decaimiento(OSC_COMP_REL);
  for (int i = 0; i < LIM_LOOK; i++) { limDlyL[i] = 0; limDlyR[i] = 0; }
  limRel = expf(-1.0f / (SR * 0.08f));
  bqLPF(techoL, 13000.0f, 0.7071f); bqLPF(techoR, 13000.0f, 0.7071f);   // DESPUÉS del limitador
  coefFiltro(fDrumL, 12000.0f, 0.75f); fDrumR = fDrumL; fMasL = fDrumL; fMasR = fDrumL;
  setBPM(128.0f);
}

// ─── Armonía ───────────────────────────────────────────────
int notaGrado(int tonica, int g) {                  // grado de la escala (puede ser negativo) → nota MIDI
  int o = (g >= 0) ? g / 7 : -((-g + 6) / 7);
  return tonica + 12 * o + ESCALAS[modo][g - 7 * o];
}
bool triadaConsonante(int g) {                      // ¿quinta justa con tercera mayor o menor?
  int f = notaGrado(0, g), t = notaGrado(0, g + 2) - f, q = notaGrado(0, g + 4) - f;
  return q == 7 && (t == 3 || t == 4);
}
int acordeDelModo(int g) {                          // disminuido o aumentado → el consonante más cercano
  const int8_t PRUEBA[5] = { 0, -2, 2, -1, 1 };
  for (int k = 0; k < 5; k++) { int c = ((g + PRUEBA[k]) % 7 + 7) % 7; if (triadaConsonante(c)) return c; }
  return 0;
}
inline int raizCercana(int g) { return g >= 4 ? g - 7 : g; }   // los acordes altos se toman desde abajo: las voces se mueven poco
int notaAcorde(int tonica, int raiz, int off, bool cercana) {
  int g = cercana ? raizCercana(raiz) : raiz;
  return notaGrado(tonica, g + off);
}
int acordeDe(uint32_t compas) {                     // grado del acorde que suena en ese compás
  const Estilo &E = ESTILOS[estilo];
  int total = 0;
  for (int k = 0; k < NUM_SEG && E.prog[k].compases; k++) total += E.prog[k].compases;
  int pos = (int)(compas % (uint32_t)total);
  for (int k = 0; k < NUM_SEG && E.prog[k].compases; k++) {
    if (pos < E.prog[k].compases) return acordeDelModo(E.prog[k].grado);
    pos -= E.prog[k].compases;
  }
  return 0;
}

// Prioridad de cada golpe del patrón de batería, desde la letra: X entra primero, x después, los
// fantasmas al final. El azar dentro de cada letra es fijo por pista y paso (siempre el mismo
// patrón), así subir la densidad agrega golpes de a poco y nunca cambia los que ya sonaban.
void armarPrioridades() {
  const Estilo &E = ESTILOS[estilo];
  for (int t = 0; t < NUM_IND; t++) {
    const char *lin = E.bateria[C.patDrum][t + 1];
    for (int s = 0; s < 16; s++) {
      uint32_t h = (uint32_t)(t * 16 + s + 1) * 2654435761u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
      float r = (h & 0xFFFF) / 65535.0f;
      char c = lin[s];
      prio[t][s] = c == 'X' ? 0.02f + 0.10f * r : c == 'x' ? 0.20f + 0.30f * r : c == '.' ? 0.55f + 0.40f * r : 9.0f;
    }
  }
  estiloPrio = estilo; patPrio = C.patDrum;
}

// ─── Disparos ──────────────────────────────────────────────
void publicarGolpe(int ev, float v) { velGolpe[ev] = v; golpes[ev] = golpes[ev] + 1; }
static inline float velDe(char c) { return c == 'X' ? 1.0f : (c == 'x' ? 0.80f : 0.55f); }

void golpeBombo(float v) {
  const Kit &K = KITS[kit];
  VozBombo &a = bombos[sigBombo]; if (a.viva) { a.eC = a.kC = a.cC = dRapido3; }   // el parche anterior se calla en 3 ms
  sigBombo ^= 1; VozBombo &b = bombos[sigBombo];
  b.fObj = K.kFreq; b.f = K.kFreq * K.kRatio; b.fCoef = 1.0f - expf(-1.0f / (K.kDrop * SR));
  b.ph = 0.0f; b.env = 1.0f; b.knock = 1.0f; b.click = 1.0f; b.amp = v; b.viva = true;
  b.punch = K.kPunch; b.kAmt = K.kKnock; b.cAmt = K.kClick; b.sat = K.kSat;
  b.eC = decaimiento(K.kDec * (0.75f + 0.50f * (largoDrum - 0.5f))); b.kC = decaimiento(0.014f); b.cC = decaimiento(0.005f);
  bqBPF(b.bp, K.kKnockF, 0.8f); bqHPF(b.hp, 3000.0f, 0.7f);
  scObj = 1.0f;
  publicarGolpe(EV_BOMBO, v);
}

void golpeBajo(int nota) {
  float inc = hz((float)nota) / SR;
  bajo.incObj = inc;
  if (!C.glide || bajo.env < 1e-3f) bajo.inc1 = inc;   // con glide, la altura se desliza desde la nota anterior
  bajo.atk = true; bajo.atkF = true;               // sube desde donde estaba: sin clic, sin reiniciar la fase
  notaBajoPub = nota;
  publicarGolpe(EV_BAJO, 1.0f);
}

void golpeKeys(int nota, float vel) {               // KEYS: una sola voz; la fase no se reinicia, la amplitud sube desde donde estaba
  const TimbreMel &T = TMEL[tMel];
  float f = hz((float)nota);
  if (!kv.viva) { kv.f1 = 0.13f; kv.f2 = 0.61f; kv.amp = 0.0f; kv.s.ic1 = kv.s.ic2 = 0.0f; }
  kv.inc1 = f / SR; kv.inv1 = 1.0f / kv.inc1;
  kv.inc2 = kv.inc1 * 1.00405f; kv.inv2 = 1.0f / kv.inc2;   // +7 cents (el LEAD)
  kv.base = f * T.corte * filtMel; kv.fenv = 1.0f; kv.atk = true; kv.viva = true;
  kv.eC = decaimiento(decMel * (0.8f + 0.4f * vel));
  dKeysFenv = decaimiento(T.fenvTau);
  notaKeysPub = nota;
  publicarGolpe(EV_KEYS, vel);
}

void golpeArp(int nota) {                           // ARPEGIO: notas cortas del acorde, una voz por altura, alternando lados
  ladoArp ^= 1;
  float pan = ladoArp ? 0.28f : 0.72f;
  int k = -1;
  for (int i = 0; i < VOCES_ARP; i++) if (arps[i].viva && arps[i].nota == nota) k = i;
  if (k < 0) for (int i = 0; i < VOCES_ARP; i++) if (!arps[i].viva) { k = i; break; }
  if (k < 0) { float menor = 9.0f; for (int i = 0; i < VOCES_ARP; i++) if (arps[i].amp < menor) { menor = arps[i].amp; k = i; } }
  VozPluck &v = arps[k];
  float f = hz((float)nota);
  if (!v.viva) { v.f1 = 0.13f; v.amp = 0.0f; v.s.ic1 = v.s.ic2 = 0.0f; }
  v.inc1 = f / SR; v.inv1 = 1.0f / v.inc1;
  v.nota = nota; v.base = f * 1.5f * filtMel; v.oct = 2.0f; v.q = 0.9f; v.fenv = 1.0f; v.atk = true; v.viva = true;
  float dec = 0.45f * decMel; if (dec < 0.04f) dec = 0.04f; if (dec > 0.40f) dec = 0.40f;
  v.eC = decaimiento(dec);
  v.gL = sqrtf(1.0f - pan); v.gR = sqrtf(pan);
  notaArpPub = nota;
  publicarGolpe(EV_ARP, 0.7f);
}

void golpeInd(int t, float vel, bool acento, float largoExtra) {
  VozInd &a = ind[t][sigInd[t]];
  if (a.viva) { a.eC = a.nC = dRapido3; }          // fast-kill: el parche anterior se calla en 3 ms
  sigInd[t] ^= 1;
  VozInd &v = ind[t][sigInd[t]];
  memset(&v, 0, sizeof(v));
  const Golpe &g = KITS[kit].g[t];
  const float L = largoDrum * largoExtra;
  v.viva = true;
  v.amp = g.amp * vel;
  if (g.a0 > 0.0f) {                               // capa tonal (la caja, el perc grave, el tom)
    v.inc[0] = g.f / SR; v.a[0] = g.a0;
    if (g.r2 > 0.0f) { v.inc[1] = g.f * g.r2 / SR; v.a[1] = g.a1; }
    v.env = 1.0f; v.eC = decaimiento(g.eDec * L);
    v.cuanto = g.cuanto; v.caida = 1.0f; v.caidaC = decaimiento(g.caidaDec);
  }
  v.nz = 1.0f; v.nAmt = g.nAmt; v.nC = decaimiento((acento ? g.nAcc : g.nDec) * L);
  v.fc0 = g.fc0; v.fc1 = g.fc1; v.q = g.q;
  if (g.barre > 0.0f) v.barreInc = 1.0f / (g.barre * SR);
  if (g.hp > 0.0f) { bqHPF(v.banda, g.hp, 0.707f); v.conBanda = true; }
  coefFiltro(v.s, v.fc0, v.q);
  float pan = g.pan + g.panRnd * (2.0f * frnd() - 1.0f);
  pan = clamp01(pan);
  v.gL = sqrtf(1.0f - pan); v.gR = sqrtf(pan);
  publicarGolpe(t, vel);
}

// EL ARRASTRE (de triphop_viaje). Lo que hace que el trip hop suene tocado y no programado: la caja
// no entra en el tiempo, entra 15–30 ms después. En la rejilla de semicorcheas no cabe (a 86 BPM
// una semicorchea son 174 ms), así que el golpe se deja PENDIENTE y el bucle de bloques lo dispara
// en la muestra exacta. Sin pendiente libre suena al tiempo: más vale a tiempo que no sonar.
void golpeIndTarde(int t, float vel, bool acento, float largoExtra, float ms) {
  if (ms <= 0.05f) { golpeInd(t, vel, acento, largoExtra); return; }
  for (int i = 0; i < MAX_PEND; i++) if (!pend[i].viva) {
    pend[i].viva = true; pend[i].pista = t; pend[i].vel = vel; pend[i].acento = acento;
    pend[i].largo = largoExtra; pend[i].faltan = ms * 0.001f * SR;
    return;
  }
  golpeInd(t, vel, acento, largoExtra);
}

// ─── Un paso del secuenciador (semicorchea) ─────────────────
int notaBajoDe(char c, int raiz) {
  int off = 0;
  switch (c) { case 'o': off = 7; break; case '5': off = 4; break; case 'b': off = 6; break; case '3': off = 2; break; }
  const int t = tonicaBajo + (C.octBajo ? 12 : 0);
  int n = notaAcorde(t, raiz, off, false);
  if (c == 'b') {                                  // séptima: sólo si es menor; si no, la quinta
    int f = notaAcorde(t, raiz, 0, false);
    if (((n - f) % 12 + 12) % 12 != 10) n = notaAcorde(t, raiz, 4, false);
  }
  return n;
}

void golpeCajaSubida(float vel, float largo) {
  cajaSub.env = 1.0f; cajaSub.nz = 1.0f; cajaSub.amp = vel;
  cajaSub.eC = decaimiento(0.05f * largo); cajaSub.nC = decaimiento(0.11f * largo);
  coefFiltro(cajaSub.s, 2200.0f, 1.0f);
}
void golpeFinal() {
  golpeFin.viva = true; golpeFin.f = 130.0f; golpeFin.env = 1.0f; golpeFin.nz = 1.0f;
  golpeFin.eC = decaimiento(0.45f); golpeFin.nC = decaimiento(0.30f);
  coefFiltro(golpeFin.s, 3200.0f, 0.6f);
  reverbViva = (int)(3.0f * SR);
}

// Posición VIRTUAL que leen todos los patrones. Normalmente es el reloj maestro; con el BEAT
// REPEAT apretado, repite el pulso donde lo apretaste: 2 vueltas de 4 semicorcheas, 4 de 2, y
// después una sola semicorchea — un redoble que se cierra. El reloj maestro nunca se detiene, así
// que al soltar todo vuelve exactamente a tiempo.
uint32_t posicionVirtual() {
  if (!C.repeat) return pasoTotal;
  uint32_t n = pasoTotal - repAncla;
  if (n < 8)  return repAncla + (n & 3);
  if (n < 16) return repAncla + (n & 1);
  return repAncla;
}

void avanzarPaso() {
  pasoTotal++;
  const uint32_t pr = pasoTotal;                   // el reloj maestro
  const int sr = pr & 15;
  if (C.repeat && !repAntes) repAncla = pr & ~3u;  // el pulso que estaba sonando
  repAntes = C.repeat;
  const uint32_t vp = posicionVirtual();
  const int s = vp & 15;
  const uint32_t compas = vp >> 4;

  if (estadoSub == E_SUBIDA) {
    subPasos++;
    if (subPasos >= SUBIDA_PASOS && sr == 0) {     // fin en el "1": el golpe, y todo lo demás se corta
      estadoSub = E_ESPERA; golpeFinal();
    } else if (subPasos > SUBIDA_PASOS / 2) {      // caja desde la mitad: negras → corcheas → semicorcheas
      int k = subPasos - SUBIDA_PASOS / 2, cada = k <= 32 ? 4 : (k <= 64 ? 2 : 1);
      if ((sr % cada) == 0) golpeCajaSubida(0.30f + 0.70f * fminf(1.0f, (float)k / (SUBIDA_PASOS / 2)), cada == 4 ? 1.0f : cada == 2 ? 0.7f : 0.45f);
    }
  }
  const Estilo &E = ESTILOS[estilo];
  const int raiz = acordeDe(compas);
  acordePub = raiz;

  // ── MELODÍAS ──
  if (C.melOn) {
    char k = E.keys[C.patMel][(compas & 1) * 16 + s];
    int tk = TONICA_KEYS + transp; while (tk < KEYS_DESDE) tk += 12;
    tk += C.octMel == 1 ? 12 : (C.octMel == 2 ? -12 : 0);
    if (k >= '1' && k <= '9') golpeKeys(notaAcorde(tk, raiz, k - '1', true), (s & 3) == 0 ? 1.0f : 0.8f);
    if (C.arp) {
      char a = E.arp[C.patMel][s];
      int ta = TONICA_ARP + transp + (C.octMel == 1 ? 12 : (C.octMel == 2 ? -12 : 0));
      if (a >= '0' && a <= '5') golpeArp(notaAcorde(ta, raiz, TONOS_ARP[a - '0'], true));
    }
  }

  // ── BAJO ── (el break lo calla, como al bombo)
  if (C.bajoOn && !aBreak) {
    char c = E.bajo[C.patBajo][(compas & 3) == 3 ? 1 : 0][s];
    if (c != '-') golpeBajo(notaBajoDe(c, raiz));
  }

  // ── BATERÍA ──
  // Media velocidad (BTN5 del panel, mantenido): cada paso del patrón dura dos del reloj. El ancla
  // es donde lo apretaste, así al soltar el patrón sigue desde el reloj maestro, en tiempo.
  if (C.mitad && !mitadAntes) mitadAncla = (int32_t)vp;
  mitadAntes = C.mitad;
  int32_t dp = (int32_t)vp; bool suena = true;
  if (C.mitad) { int32_t d = (int32_t)vp - mitadAncla; if (d >= 0) { dp = mitadAncla + d / 2; suena = (d & 1) == 0; } }
  // FILL: los pasos 13–16 del compás pedido, en lugar del patrón (con el beat repeat no hay fill)
  bool enFill = fillPend && !C.repeat && (pr >> 4) == fillCompas && sr >= 12;
  if (fillPend && (pr >> 4) > fillCompas) fillPend = false;
  fillPub = enFill;
  if (sr == 15 && enFill) { fillPend = false; crashPend = true; }
  if (C.drumOn && suena) {
    const int ds = dp & 15;
    if (estiloPrio != estilo || patPrio != C.patDrum) armarPrioridades();
    const float *arr = E.arrastre;                 // todo en cero salvo en el TRIP HOP
    char kc = enFill ? E.fill[0][sr - 12] : E.bateria[C.patDrum][0][ds];
    if (kc != '-' && C.kick && !aBreak) golpeBombo(velDe(kc));   // el bombo nunca se arrastra: es el tiempo
    for (int t = 0; t < NUM_IND; t++) {
      if (enFill) {                                // el fill suena entero, sin mirar la densidad
        char c = E.fill[t + 1][sr - 12];
        if (c != '-') golpeIndTarde(t, velDe(c), c == 'X', 0.6f, arr[t] * 0.4f);   // golpes cortos y casi sin arrastre: un redoble de golpes largos o arrastrados es barro
        continue;
      }
      if (prio[t][ds] >= densS) continue;
      char c = E.bateria[C.patDrum][t + 1][ds];
      // con arrastre, una pizca de variación humana (±12 %). frnd() se llama igual que en el v2:
      // sin arrastre los golpes quedan idénticos a los del original
      float vel = velDe(c) * (0.92f + 0.08f * frnd());
      if (arr[t] > 0.0f) golpeIndTarde(t, vel, c == 'X', 1.0f, arr[t] * (0.88f + 0.24f * frnd()));
      else               golpeInd(t, vel, c == 'X', 1.0f);
    }
    if (crashPend && sr == 0 && !enFill) { golpeInd(I_ABIERTO, 1.0f, true, 2.5f); crashPend = false; }   // el plato del "1"
  } else if (crashPend && sr == 0) crashPend = false;

  pasoTotalPub = pr; contadorPasos = contadorPasos + 1;
}

// ==============================================================================================================================================
// RENDER POR BLOQUES — cada voz de corrido, con su estado en variables locales
// ==============================================================================================================================================
IRAM_ATTR void renderSidechain(float *o, int m) {  // baja en ~2 ms al llegar el bombo y vuelve en ~140 ms
  float obj = scObj, s = sc;
  for (int i = 0; i < m; i++) { obj *= dSc; s += (obj - s) * 0.012f; o[i] = s; }
  scObj = obj; sc = s;
}

IRAM_ATTR void renderBombo(float *o, int m) {
  uint32_t sem = semillaRuido;
  for (int k = 0; k < 2; k++) {
    VozBombo &v = bombos[k]; if (!v.viva) continue;
    float f = v.f, ph = v.ph, env = v.env, kn = v.knock, cl = v.click;
    const float eC = v.eC, kC = v.kC, cC = v.cC, amp = v.amp * NIVEL_BOMBO, fo = v.fObj, fk = v.fCoef;
    const float punch = v.punch, sat = v.sat, kA = v.kAmt * 1.6f, cA = v.cAmt * 0.45f, norm = 0.9f / (1.0f + 0.4f * punch);
    for (int i = 0; i < m; i++) {
      f += (fo - f) * fk;
      ph += f * (1.0f / SR); if (ph >= 1.0f) ph -= 1.0f;
      float p2 = ph + ph; if (p2 >= 1.0f) p2 -= 1.0f;
      float y = softClip((seno(ph) + punch * seno(p2)) * sat) * norm * env;
      if (kn > 1e-4f) y += pasoBQ(v.bp, ruido(sem)) * kn * kA;
      if (cl > 1e-4f) y += pasoBQ(v.hp, ruido(sem)) * cl * cA;
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
  const TimbreBajo &T = TBAJO[tBajo];
  float f1 = bajo.f1, f2 = bajo.f2, fs = bajo.fs, fsn = bajo.fsn, env = bajo.env, envF = bajo.envF, inc = bajo.inc1;
  bool atk = bajo.atk, atkF = bajo.atkF;
  const float incObj = bajo.incObj, kg = C.glide ? kGlide16 : 1.0f;
  const bool dos = T.detune != 1.0f, conSierra = T.sierras > 0.0f, conSub = T.sub > 0.0f, conSeno = T.seno > 0.0f;
  const float nivel = NIVEL_BAJO * T.nivel * comp, dComp = 1.0f / (0.75f + 0.25f * T.drive);
  float i1 = inc, i2 = inc * T.detune, iS = inc * 0.5f, v1 = 1.0f / i1, v2 = 1.0f / i2, vS = 1.0f / iS;
  Filtro s = bajo.s; const uint32_t c = muestraGlobal;
  for (int i = 0; i < m; i++) {
    if (atk) { env += incAtkBajo; if (env >= 1.0f) { env = 1.0f; atk = false; } } else env *= dBajo;
    if (atkF) { envF += 1.0f / 66.0f; if (envF >= 1.0f) { envF = 1.0f; atkF = false; } } else envF *= dBajoF;
    if (((c + i) & 15) == 0) {
      inc += (incObj - inc) * kg;                   // glide: se desliza hacia la nota nueva (sin glide, salta)
      i1 = inc; i2 = inc * T.detune; iS = inc * 0.5f; v1 = 1.0f / i1; v2 = 1.0f / i2; vS = 1.0f / iS;
      coefFiltro(s, corte * exp2Rapido(T.envOct * envF), q);
    }
    float mez = 0.0f;
    if (conSierra) { float a = sierra(f1, i1, v1); if (dos) a = (a + sierra(f2, i2, v2)) * 0.5f; mez += a * T.sierras; }
    if (conSub) mez += cuadrada(fs, iS, vS) * T.sub;
    if (conSeno) mez += suave(fsn, i1) * T.seno * 1.2f;
    float lp, bp; pasoFiltro(s, softClip(mez * T.drive) * dComp + 1e-18f, lp, bp);
    o[i] += lp * nivel * env * (1.0f - SC_BAJO * scb[i]);
  }
  bajo.f1 = f1; bajo.f2 = f2; bajo.fs = fs; bajo.fsn = fsn; bajo.env = env; bajo.envF = envF; bajo.atk = atk; bajo.atkF = atkF; bajo.s = s; bajo.inc1 = inc;
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
        float tt = seno(p0) * a0;
        if (dosParciales) { p1 += n1 * mul; if (p1 >= 1.0f) p1 -= 1.0f; tt += seno(p1) * a1; }
        y = tt * env;
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

// ARPEGIO: una sierra PolyBLEP → pasa-bajos con envolvente (sube `oct` octavas y se cierra en
// ~70 ms) → VCA. Se apaga en dos etapas: −60 dB → 1.5 ms → libre a −82 dB.
IRAM_ATTR void renderArp(float *oL, float *oR, const float *scb, int m) {
  const uint32_t c = muestraGlobal;
  for (int k = 0; k < VOCES_ARP; k++) {
    VozPluck &v = arps[k]; if (!v.viva) continue;
    float f1 = v.f1, amp = v.amp, fenv = v.fenv; bool atk = v.atk; Filtro s = v.s;
    const float i1 = v.inc1, w1 = v.inv1, eC = v.eC, base = v.base, oct = v.oct, q = v.q;
    const float g = NIVEL_ARP * NIVEL_IND, gL = v.gL * g, gR = v.gR * g;
    for (int i = 0; i < m; i++) {
      if (atk) { amp += 1.0f / 132.0f; if (amp >= 1.0f) { amp = 1.0f; atk = false; } }   // 3 ms, desde donde estaba
      else { amp *= amp < 1e-3f ? dRapido : eC; if (amp < 8e-5f) { v.viva = false; break; } }
      fenv *= dArpFenv;
      if (((c + i + 7 * k) & 31) == 0) coefFiltro(s, base * exp2Rapido(oct * fenv), q);
      float lp, bp; pasoFiltro(s, sierra(f1, i1, w1), lp, bp);
      float y = lp * amp * (1.0f - SC_IND * scb[i]);
      oL[i] += y * gL; oR[i] += y * gR;
    }
    v.f1 = f1; v.amp = amp; v.fenv = fenv; v.atk = atk; v.s = s;
  }
}

// KEYS: voz (mono) → chorus estéreo → al bus de melodías; y un envío al eco ping-pong (a corchea con
// puntillo, sigue al tempo cruzando de cabezal en 50 ms). El eco devuelve a su propio bus, que NO
// pasa por el volumen ni el silencio de las melodías: así las colas suenan después de callarlas y
// el oscilador (que también le manda) no depende del panel de melodías.
IRAM_ATTR void renderKeys(float *oL, float *oR, float *eL, float *eR, const float *scb, const float *ecoExtra, float envioMel, int m) {
  if (kv.viva) {
    const TimbreMel &T = TMEL[tMel];
    float f1 = kv.f1, f2 = kv.f2, amp = kv.amp, fenv = kv.fenv, lfo = kv.lfo; bool atk = kv.atk; Filtro s = kv.s;
    const float i1 = kv.inc1, w1 = kv.inv1, i2 = kv.inc2, w2 = kv.inv2, eC = kv.eC, base = kv.base, dF = dKeysFenv, ia = incAtkMel;
    const float g = NIVEL_KEYS * NIVEL_IND * T.nivel, dLfo = KEYS_PWM_HZ / SR, envOct = T.envOct, q = T.q;
    const uint8_t onda = T.onda;
    const uint32_t c = muestraGlobal;
    float ancho1 = 0.5f - KEYS_PWM_PROF * (0.5f + 0.5f * seno(lfo));
    int i = 0;
    for (; i < m; i++) {
      if (atk) { amp += ia; if (amp >= 1.0f) { amp = 1.0f; atk = false; } }
      else { amp *= amp < 1e-3f ? dRapido : eC; if (amp < 8e-5f) { kv.viva = false; break; } }
      fenv *= dF;
      lfo += dLfo; if (lfo >= 1.0f) lfo -= 1.0f;
      if (((c + i) & 31) == 0) {
        coefFiltro(s, base * exp2Rapido(envOct * fenv), q);
        ancho1 = 0.5f - KEYS_PWM_PROF * (0.5f + 0.5f * seno(lfo));
      }
      float x;
      switch (onda) {
        case 0:  x = pulso(f1, i1, w1, ancho1); break;
        case 1:  x = sierra(f1, i1, w1); break;
        case 2:  x = (sierra(f1, i1, w1) + sierra(f2, i2, w2)) * 0.6f; break;
        default: { f1 += i1; if (f1 >= 1.0f) f1 -= 1.0f;
                   float p2 = f1 + f1; if (p2 >= 1.0f) p2 -= 1.0f; float p3 = f1 * 3.0f; p3 -= (int)p3;
                   x = seno(f1) + 0.40f * seno(p2) + 0.12f * seno(p3); } break;
      }
      float lp, bp; pasoFiltro(s, x, lp, bp);
      keysBuf[i] = lp * amp * g * (1.0f - SC_IND * scb[i]);
    }
    for (; i < m; i++) keysBuf[i] = 0.0f;
    kv.f1 = f1; kv.f2 = f2; kv.amp = amp; kv.fenv = fenv; kv.lfo = lfo; kv.atk = atk; kv.s = s;
  } else for (int i = 0; i < m; i++) keysBuf[i] = 0.0f;

  const float dChorus = CHORUS_HZ / SR, base = CHORUS_MS * 0.001f * SR, prof = CHORUS_PROF_MS * 0.001f * SR;
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
    if (((c0 + i) & 15) == 0) {
      d1 = base + prof * seno(lfoC); d2 = base + prof * seno(lfoC + 0.25f >= 1.0f ? lfoC - 0.75f : lfoC + 0.25f);
    }
    float p1 = (float)ci - d1; if (p1 < 0.0f) p1 += CHORUS_N;
    float p2 = (float)ci - d2; if (p2 < 0.0f) p2 += CHORUS_N;
    int a1 = (int)p1, a2 = (int)p2; float t1 = p1 - a1, t2 = p2 - a2;
    a1 &= CHORUS_N - 1; a2 &= CHORUS_N - 1;         // ci − d + N puede redondear a N justo
    float c1 = chorusBuf[a1] + (chorusBuf[(a1 + 1) & (CHORUS_N - 1)] - chorusBuf[a1]) * t1;
    float c2 = chorusBuf[a2] + (chorusBuf[(a2 + 1) & (CHORUS_N - 1)] - chorusBuf[a2]) * t2;
    ci = (ci + 1) & (CHORUS_N - 1);
    float L = x * (1.0f - CHORUS_MEZCLA * 0.5f) + c1 * CHORUS_MEZCLA, R = x * (1.0f - CHORUS_MEZCLA * 0.5f) + c2 * CHORUS_MEZCLA;

    int r1 = ei - ecoN; if (r1 < 0) r1 += ECO_MAX;
    float dl = ecoL[r1] * (1.0f / 32768.0f), dr = ecoR[r1] * (1.0f / 32768.0f);
    if (xf < 1.0f) {
      int r2 = ei - ecoN2; if (r2 < 0) r2 += ECO_MAX;
      float nl = ecoL[r2] * (1.0f / 32768.0f), nr = ecoR[r2] * (1.0f / 32768.0f);
      dl += (nl - dl) * xf; dr += (nr - dr) * xf;
      xf += 1.0f / 2205.0f; if (xf >= 1.0f) { xf = 1.0f; ecoN = ecoN2; }
    }
    lpL += (dl - lpL) * 0.45f; lpR += (dr - lpR) * 0.45f;      // cada vuelta más oscura (~4 kHz)
    hpL += (lpL - hpL) * 0.010f; hpR += (lpR - hpR) * 0.010f;  // y sin barro abajo (~70 Hz)
    float fl = lpL - hpL, fr = lpR - hpR;
    float inL = x * envioMel + ecoExtra[i] + ECO_FEEDBACK * fr, inR = ECO_FEEDBACK * fl;   // ping-pong: lo que vuelve cruza de lado
    if (inL > 0.99f) inL = 0.99f; if (inL < -0.99f) inL = -0.99f;
    if (inR > 0.99f) inR = 0.99f; if (inR < -0.99f) inR = -0.99f;
    ecoL[ei] = (int16_t)(inL * 32767.0f); ecoR[ei] = (int16_t)(inR * 32767.0f);
    ei = (ei + 1) & (ECO_MAX - 1);
    oL[i] += L; oR[i] += R;
    eL[i] = dl * ECO_MEZCLA; eR[i] = dr * ECO_MEZCLA;
  }
  chorusLfo = lfoC; chorusIdx = ci; ecoIdx = ei; ecoXf = xf; ecoLpL = lpL; ecoLpR = lpR; ecoHpL = hpL; ecoHpR = hpR;
}

// SUBIDA: oscilador agresivo que sube SUBIDA_OCTAVAS + ruido blanco, con trémolo desde la mitad
IRAM_ATTR void renderSubida(float *oL, float *oR, int m) {
  if (estadoSub != E_SUBIDA) return;
  const float porPaso = 1.0f / sps;
  const float fBaja = hz((float)tonicaBajo);
  float f1 = subF1, f2 = subF2, lp = subLp, tg = subTrem;
  float i1 = 0.001f, i2 = 0.001f, w1 = 1000.0f, w2 = 1000.0f;
  uint32_t a = semRuidoL, b = semRuidoR;
  const uint32_t c = muestraGlobal;
  for (int i = 0; i < m; i++) {
    float pos = (float)subPasos - (faltanTramo - (float)i) * porPaso;
    if (pos < 0.0f) pos = 0.0f;
    float p = pos / (float)SUBIDA_PASOS; if (p > 1.0f) p = 1.0f;
    if (i == 0 || ((c + i) & 15) == 0) {
      float f = fBaja * exp2Rapido(SUBIDA_OCTAVAS * p * (0.55f + 0.45f * p));
      i1 = f * 0.9914f / SR; i2 = f * 1.0087f / SR;
      w1 = 1.0f / i1; w2 = 1.0f / i2;
    }
    float objT = 1.0f;
    if (pos >= SUBIDA_PASOS / 2) {
      float ph = 1.0f - (faltanTramo - (float)i) * porPaso;
      float prof = fminf(1.0f, (pos - SUBIDA_PASOS / 2) / 16.0f) * 0.95f;
      objT = ph < 0.5f ? 1.0f : 1.0f - prof;
    }
    tg += (objT - tg) * 0.02f;
    float x = softClip((sierra(f1, i1, w1) + sierra(f2, i2, w2)) * 0.5f * SUBIDA_DRIVE);
    lp += (x - lp) * 0.55f;
    float o = lp * NIVEL_SUBIDA_OSC * (0.35f + 0.65f * p) * NIVEL_IND;
    float rn = NIVEL_SUBIDA_RUIDO * p * p * NIVEL_IND;
    oL[i] += (o + ruido(a) * rn) * tg;
    oR[i] += (o + ruido(b) * rn) * tg;
    if (cajaSub.env > 1e-4f || cajaSub.nz > 1e-4f) {
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

// OSCILADOR: dos osciladores ±6 cents, cada uno con su SVF que sigue a la nota y que barre el LFO
// libre; chorus estéreo propio y envío al eco del keys. POT2 = dónde reposa el filtro (1·f … 16·f),
// POT3 = cuántas octavas lo mueve el LFO (0 = quieto), POT4 = envío al delay. La resonancia va atada
// a la apertura: alta cerrado, baja abierto (nunca un pico chillón arriba).
IRAM_ATTR void renderOsc(float *oL, float *oR, float *eco, const float *scb, int m) {
  if (!osc.vivo) {                                 // callado: el retardo del chorus sigue corriendo vacío
    for (int i = 0; i < m; i++) { oL[i] = 0.0f; oR[i] = 0.0f; eco[i] = 0.0f; oscChorusBuf[oscChorusIdx] = 0.0f; oscChorusIdx = (oscChorusIdx + 1) & (OSC_CHORUS_N - 1); }
    return;
  }
  float f1 = osc.f1, f2 = osc.f2, inc = osc.inc, amp = osc.amp; Filtro s = osc.s, s2 = osc.s2;
  const float lado = 0.5f + 0.5f * OSC_ANCHO, otro = 1.0f - lado;
  const float incObj = osc.incObj, obj = osc.obj, k = obj > amp ? kOscAtk : kOscSuelta, g = NIVEL_OSC * NIVEL_IND;
  float cEnv = oscCompEnv, cG = oscCompG; const float trim = NIVEL_TOSC[tOsc];
  const float octBase = 4.0f * oscFiltS, octLfoP = OSC_LFO_OCT * oscLfoS, envio = OSC_ECO_MAX * oscEcoS;
  const int onda = tOsc;
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
      if (((c + i) & 15) == 0) {
        lfoFalta -= 16.0f / SR;
        if (lfoFalta <= 0.0f) {
          lfoVelObj = (0.5f + 0.5f * ruido(semLfo)) * octLfo;
          lfoFalta = OSC_LFO_CAMBIO_MIN + (OSC_LFO_CAMBIO_MAX - OSC_LFO_CAMBIO_MIN) * (0.5f + 0.5f * ruido(semLfo));
        }
        lfoVel += (lfoVelObj - lfoVel) * kDeslizLfo;
        lfoFase += OSC_LFO_MIN_HZ * exp2Rapido(lfoVel) * (16.0f / SR); if (lfoFase >= 1.0f) lfoFase -= 1.0f;
      }
      float lfo = 0.5f + 0.5f * seno(lfoFase);
      float oct = octBase + octLfoP * lfo;                        // octavas sobre la nota
      float ab = oct * (1.0f / 7.5f); if (ab > 1.0f) ab = 1.0f;    // 0 cerrado · 1 abierto
      float q = OSC_Q_CERRADO + (OSC_Q_ABIERTO - OSC_Q_CERRADO) * ab;
      coefFiltro(s, inc * SR * exp2Rapido(oct), q);
      s2.a1 = s.a1; s2.a2 = s.a2; s2.a3 = s.a3; s2.k = s.k;
      comp = 1.0f / sqrtf(sqrtf(q));
    }
    float x1, x2;
    if (onda == 0)      { x1 = sierra(f1, i1, w1); x2 = sierra(f2, i2, w2); }
    else if (onda == 1) { x1 = cuadrada(f1, i1, w1); x2 = cuadrada(f2, i2, w2); }
    else                { x1 = suave(f1, i1) * 1.3f; x2 = suave(f2, i2) * 1.3f; }
    float lp1, lp2, bp;
    pasoFiltro(s,  x1 + 1e-18f, lp1, bp);
    pasoFiltro(s2, x2 + 1e-18f, lp2, bp);
    // compresor: detecta ANTES del fundido de encendido y del sidechain, para no deshacerlos
    const float a0 = g * comp * 0.5f;
    lp1 *= a0; lp2 *= a0;
    const float pk = fabsf(lp1 + lp2);
    cEnv += (pk - cEnv) * (pk > cEnv ? kOscCompAtk : kOscCompRel);
    cG += (OSC_COMP_NIVEL / (cEnv + OSC_COMP_NIVEL * OSC_COMP_RODILLA) - cG) * 0.003f;
    const float a = cG * trim * amp * (1.0f - OSC_SC * scb[i]);
    lp1 *= a; lp2 *= a;
    const float mono = lp1 + lp2;
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
    eco[i] = mono * envio;
  }
  oscChorusIdx = ci; oscChorusLfo = lfoC;
  oscCompEnv = cEnv; oscCompG = cG;
  osc.f1 = f1; osc.f2 = f2; osc.inc = inc; osc.amp = amp; osc.s = s; osc.s2 = s2; osc.comp = comp;
  if (obj == 0.0f && amp < 1e-5f) osc.vivo = false;   // ya a −100 dB: se libera sin escalón
}

// GOLPE FINAL + REVERB: bus propio que el corte del final de la subida no toca
static inline float peine(float *b, int n, int &i, float &lp, float x) {
  float y = b[i]; lp += (y - lp) * 0.45f;
  b[i] = x + lp * REVERB_FB; if (++i >= n) i = 0; return y;
}
static inline float pasaTodo(float *b, int n, int &i, float x) {
  float d = b[i], y = d - 0.5f * x; b[i] = x + 0.5f * d; if (++i >= n) i = 0; return y;
}
IRAM_ATTR void renderGolpe(float *oL, float *oR, int m) {
  if (!golpeFin.viva && reverbViva <= 0) { for (int i = 0; i < m; i++) { oL[i] = 0.0f; oR[i] = 0.0f; } return; }
  uint32_t sem = semRuidoR;
  const float kIn = 0.28f * (1.0f - REVERB_FB);
  for (int i = 0; i < m; i++) {
    float g = 0.0f;
    if (golpeFin.viva) {
      golpeFin.f += (40.0f - golpeFin.f) * 0.0012f;
      golpeFin.ph += golpeFin.f / SR; if (golpeFin.ph >= 1.0f) golpeFin.ph -= 1.0f;
      float lpN, bpN; pasoFiltro(golpeFin.s, ruido(sem), lpN, bpN);
      g = (softClip(seno(golpeFin.ph) * 1.6f) * golpeFin.env + bpN * 0.9f * golpeFin.nz) * NIVEL_GOLPE * NIVEL_IND;
      golpeFin.env *= golpeFin.eC; golpeFin.nz *= golpeFin.nC;
      if (golpeFin.env < 1e-4f && golpeFin.nz < 1e-4f) golpeFin.viva = false;
    }
    float x = g * GOLPE_REVERB * kIn / 0.28f;
    float cc = peine(rvC1, RV_N1, rvI1, rvLp1, x) + peine(rvC2, RV_N2, rvI2, rvLp2, x)
             + peine(rvC3, RV_N3, rvI3, rvLp3, x) + peine(rvC4, RV_N4, rvI4, rvLp4, x);
    float wl = pasaTodo(rvA2, RV_A2, rvJ2, pasaTodo(rvA1, RV_A1, rvJ1, cc));
    float wr = pasaTodo(rvA4, RV_A4, rvJ4, pasaTodo(rvA3, RV_A3, rvJ3, cc));
    oL[i] = g + wl * 2.2f; oR[i] = g + wr * 2.2f;
  }
  semRuidoR = sem;
  reverbViva -= m;
}

volatile uint32_t medSec[6];                        // MEDIR: ciclos por sección (batería · bajo · melodías · oscilador · master · pasos)
#if MEDIR && !defined(SIMULADOR)
#define CICLOS() ESP.getCycleCount()
#else
#define CICLOS() 0u
#endif
float envioMelBloque = 1.0f;
void renderTramo(int off, int m, float corte, float q, float comp) {
  uint32_t c0 = CICLOS();
  renderSidechain(bufSc + off, m);
  renderBombo(bufKick + off, m);
  renderInd(bufIndL + off, bufIndR + off, bufSc + off, m);
  uint32_t c1 = CICLOS();
  renderBajo(bufBajo + off, bufSc + off, m, corte, q, comp);
  uint32_t c2 = CICLOS();
  renderOsc(oscL + off, oscR + off, oscEco + off, bufSc + off, m);   // antes del keys: le manda su envío al eco
  uint32_t c3 = CICLOS();
  renderKeys(bufMelL + off, bufMelR + off, ecoBufL + off, ecoBufR + off, bufSc + off, oscEco + off, envioMelBloque, m);
  renderArp(bufMelL + off, bufMelR + off, bufSc + off, m);
  renderSubida(bufFxL + off, bufFxR + off, m);
  renderGolpe(golpeL + off, golpeR + off, m);
  uint32_t c4 = CICLOS();
  medSec[0] += (c1 - c0) >> 6; medSec[1] += (c2 - c1) >> 6; medSec[3] += (c3 - c2) >> 6; medSec[2] += (c4 - c3) >> 6;
  muestraGlobal += m;
}

// ==============================================================================================================================================
// UN BLOQUE DE AUDIO (128 muestras ≈ 2.9 ms)
// ==============================================================================================================================================
static i2s_chan_handle_t tx_chan;
int16_t salida[BUFFER_SAMPLES * 2];

IRAM_ATTR void calcularBloque() {
  // ── Borde del buffer: aplicar los pedidos de la tarea de control ──
  memcpy(&C, (const void *)&reqC, sizeof(C));
  if (C.tocar && !aTocando) {                      // Play: el "1" en la primera muestra
    pasoTotal = 0xFFFFFFFFu; faltan = 0.0f; repAntes = false; mitadAntes = false; fillPend = false; crashPend = false;
    for (int k = 0; k < MAX_PEND; k++) pend[k].viva = false;
  }
  aTocando = C.tocar; tocandoPub = C.tocar;
  if (!C.tocar) estadoSub = E_NORMAL;
  if (reqSubida != vistoSubida) {
    vistoSubida = reqSubida;
    if (estadoSub == E_NORMAL && aTocando) { estadoSub = E_SUBIDA; subPasos = 0; subTrem = 1.0f; subLp = 0.0f; }
  }
  if (reqDrop != vistoDrop) {                      // el DROP: todo desde el "1", con bombo y bajo
    vistoDrop = reqDrop;
    if (estadoSub != E_NORMAL) {
      estadoSub = E_NORMAL;
      pasoTotal = 0xFFFFFFFFu; faltan = 0.0f;
      memset(ecoL, 0, sizeof(ecoL)); memset(ecoR, 0, sizeof(ecoR)); memset(chorusBuf, 0, sizeof(chorusBuf));
      kv.viva = false; kv.amp = 0.0f;
      for (int k = 0; k < VOCES_ARP; k++) arps[k].viva = false;
      for (int t = 0; t < NUM_IND; t++) for (int k = 0; k < 2; k++) ind[t][k].viva = false;
      for (int k = 0; k < MAX_PEND; k++) pend[k].viva = false;
    }
  }
  estadoSubPub = estadoSub;
  aBreak = C.brk || estadoSub != E_NORMAL; breakPub = aBreak; repeatPub = C.repeat;
  if (reqFill != vistoFill) {                      // FILL: en el compás que suena si aún falta para el 13, si no en el siguiente
    vistoFill = reqFill;
    uint32_t p = pasoTotal + 1;                    // el próximo paso
    fillCompas = (p >> 4) + ((p & 15) > 11 ? 1 : 0); fillPend = true;
  }
  if (reqFillNo != vistoFillNo) { vistoFillNo = reqFillNo; fillPend = false; }

  estilo = C.estilo; kit = C.kit; tBajo = C.tBajo; tMel = C.tMel; tOsc = C.tOsc;
  modo = ESTILOS[estilo].escala; transp = ESTILOS[estilo].transp;
  tonicaBajo = TONICA_BAJO + transp; if (tonicaBajo < 28) tonicaBajo += 12;   // nunca abajo de Mi1
  tonicaPub = TONICA_OSC + transp;

  // ── Pots suavizados (por bloque; las ganancias se interpolan por muestra) ──
  // Tempo y swing: el rango lo pone el estilo (el TRIP HOP trae 58–110 BPM y swing 54–72 %; los
  // demás, los de siempre). Al cambiar de estilo el tempo salta a su rango con el pot donde está.
  float b = bpmMinDe(estilo) + bpmRangoDe(estilo) * pPot[P_GENERAL][1];
  if (fabsf(b - bpm) > 0.6f) setBPM(roundf(b));   // BPM entero con histéresis: el ruido del ADC no hace bailar el tempo
  {
    const Estilo &Es = ESTILOS[estilo];
    const float sMin = Es.swingMax > 0.0f ? Es.swingMin : 0.5f, sMax = Es.swingMax > 0.0f ? Es.swingMax : 0.70f;
    swingS += ((sMin + (sMax - sMin) * pPot[P_GENERAL][2]) - swingS) * 0.2f;
  }
  lpfS += (pPot[P_GENERAL][3] - lpfS) * 0.15f;
  corteBajoS += (pPot[P_BAJO][1] - corteBajoS) * 0.15f;
  filtDrumS += (pPot[P_DRUM][1] - filtDrumS) * 0.15f;
#if MEDIR
  densS = 1.05f;                                   // al medir, siempre el peor caso
#else
  densS += (pPot[P_DRUM][3] * 1.05f - densS) * 0.25f;
#endif
  largoDrum = 0.5f + pPot[P_DRUM][2];              // ×0.5 … ×1.5 en las caídas de la batería
  incAtkBajo = 1.0f / (BAJO_ATK_MIN * powf(BAJO_ATK_RANGO, pPot[P_BAJO][2]) * SR);
  { float tau = BAJO_DEC_MIN * powf(BAJO_DEC_RANGO, pPot[P_BAJO][3]); dBajo = expf(-1.0f / (tau * SR)); dBajoF = expf(-1.0f / (tau * TBAJO[tBajo].fenvRel * SR)); }
  incAtkMel = 1.0f / (MEL_ATK_MIN * powf(MEL_ATK_RANGO, pPot[P_MEL][2]) * SR);
  decMel = MEL_DEC_MIN * powf(MEL_DEC_RANGO, pPot[P_MEL][3]);
  filtMel = exp2f((pPot[P_MEL][1] - 0.5f) * 4.0f);   // ×0.25 … ×4 sobre el reposo del filtro de cada nota
  oscFiltS += (pPot[P_OSC][1] - oscFiltS) * 0.15f; oscLfoS += (pPot[P_OSC][2] - oscLfoS) * 0.15f; oscEcoS += (pPot[P_OSC][3] - oscEcoS) * 0.15f;
  const float corte = 55.0f * powf(30.0f, corteBajoS);   // 55 Hz – 1.6 kHz
  const float q = fminf(5.0f, (2.8f - 1.8f * corteBajoS) * TBAJO[tBajo].qMul);   // resonancia atada al corte
  const float comp = 1.0f / powf(q, 0.30f);

  // Ganancias de cada instrumento: volumen del panel × su play/stop "falso" (con fundido)
  const float g0[5] = { gDrumS, gKickS, gBajoS, gMelS, gOscS };
  const float gDrumObj = C.drumOn ? ganVol(pPot[P_DRUM][0]) : 0.0f;
  const float gKickObj = C.kick ? 1.0f : 0.0f;
  const float gBajoObj = C.bajoOn ? ganVol(pPot[P_BAJO][0]) : 0.0f;
  const float gMelObj  = C.melOn  ? ganVol(pPot[P_MEL][0])  : 0.0f;
  gDrumS += (gDrumObj - gDrumS) * 0.25f; gKickS += (gKickObj - gKickS) * 0.25f;
  gBajoS += (gBajoObj - gBajoS) * 0.25f; gMelS += (gMelObj - gMelS) * 0.25f;
  gOscS = 1.0f;
  envioMelBloque = gMelS;
  float v0 = volS; volS += (pPot[P_GENERAL][0] * pPot[P_GENERAL][0] - volS) * 0.15f;

  // Oscilador: POT1 → grado de la escala del estilo (siempre en tono) + octava del BTN3
  {
    const float x = pPot[P_OSC][0] * OSC_NOTAS;
    if (x < gradoPot - OSC_HISTERESIS || x > gradoPot + 1.0f + OSC_HISTERESIS) {
      gradoPot = (int)x; if (gradoPot > OSC_NOTAS - 1) gradoPot = OSC_NOTAS - 1; if (gradoPot < 0) gradoPot = 0;
    }
    const int oct = C.octOsc == 1 ? 12 : (C.octOsc == 2 ? -12 : 0);
    const int nota = notaGrado(TONICA_OSC + transp + oct, OSC_GRADO_BAJO + gradoPot);
    bool on = C.oscOn && estadoSub != E_ESPERA;
    if (nota != notaOscVista || on != oscOnVisto) {
      bool cambioNota = nota != notaOscVista;
      notaOscVista = nota; oscOnVisto = on;
      float inc = hz((float)nota) / SR;
      if (on) {
        if (!osc.vivo || osc.amp < 1e-3f) { osc.inc = inc; osc.vivo = true; }   // desde el silencio: sin portamento
        osc.incObj = inc; osc.obj = 1.0f; notaOscPub = nota;
        if (cambioNota || osc.amp < 0.5f) publicarGolpe(EV_OSC, 0.8f);
      } else { osc.obj = 0.0f; osc.incObj = inc; notaOscPub = -1; }
    }
  }

  // Filtros de bus (coeficientes una vez por bloque; el valor ya viene suavizado)
  coefFiltro(fDrumL, corteBus(filtDrumS), 0.75f); fDrumR.a1 = fDrumL.a1; fDrumR.a2 = fDrumL.a2; fDrumR.a3 = fDrumL.a3; fDrumR.k = fDrumL.k;
  coefFiltro(fMasL, corteBus(lpfS), 0.75f);       fMasR.a1 = fMasL.a1; fMasR.a2 = fMasL.a2; fMasR.a3 = fMasL.a3; fMasR.k = fMasL.k;
  const float abDrum = abiertoBus(filtDrumS), abMas = abiertoBus(lpfS);

  for (int i = 0; i < BUFFER_SAMPLES; i++) {
    bufKick[i] = 0.0f; bufBajo[i] = 0.0f; bufIndL[i] = 0.0f; bufIndR[i] = 0.0f;
    bufMelL[i] = 0.0f; bufMelR[i] = 0.0f; bufFxL[i] = 0.0f; bufFxR[i] = 0.0f;
  }

  // ── Síntesis por tramos: los pasos caen en la muestra exacta, y lo mismo los golpes ATRASADOS del
  //    arrastre: el tramo se corta en el próximo paso O en el próximo golpe pendiente, el que venga
  //    primero. SWING: la semicorchea par dura 2·r y la impar 2·(1−r) (r = 50 % recto … 70 %, en el
  //    trip hop 54 … 72 %); el par siempre suma dos, el pulso no se mueve.
  int hecho = 0;
  while (hecho < BUFFER_SAMPLES) {
    for (int k = 0; k < MAX_PEND; k++) if (pend[k].viva && pend[k].faltan <= 0.0f) {
      pend[k].viva = false;
      golpeInd(pend[k].pista, pend[k].vel, pend[k].acento, pend[k].largo);
    }
    int m2 = BUFFER_SAMPLES - hecho;
    if (aTocando && estadoSub != E_ESPERA) {
      if (faltan <= 0.0f) {
        uint32_t cp0 = CICLOS(); avanzarPaso();
        faltan += sps * ((pasoTotal & 1) ? 2.0f * (1.0f - swingS) : 2.0f * swingS);
        medSec[5] += (CICLOS() - cp0) >> 6;
      }
      int hasta = (int)ceilf(faltan); if (hasta < 1) hasta = 1;
      if (hasta < m2) m2 = hasta;
    }
    for (int k = 0; k < MAX_PEND; k++) if (pend[k].viva) {
      int h = (int)ceilf(pend[k].faltan); if (h < 1) h = 1; if (h < m2) m2 = h;
    }
    if (aTocando && estadoSub != E_ESPERA) faltan -= (float)m2;
    for (int k = 0; k < MAX_PEND; k++) if (pend[k].viva) pend[k].faltan -= (float)m2;
    faltanTramo = faltan + (float)m2;
    renderTramo(hecho, m2, corte, q, comp);
    hecho += m2;
  }
  envBajoPub = bajo.env;
  { float pulso = (float)(pasoTotal & 3) + 1.0f - faltan / sps; fasePub = pulso * 0.25f; }

  // ── Master ──
  //   batería = (bombo + 6 pistas) → su pasa-bajos · bajo · melodías · eco · oscilador · subida
  //   → pasa-bajos general → DC → limitador con lookahead → 13 kHz → techo suave
  uint32_t cm0 = CICLOS();
  const float objF = aTocando ? 1.0f : 0.0f;
  const float inv = 1.0f / BUFFER_SAMPLES;
  float pkD = 0, pkB = 0, pkM = 0, pkO = 0, pkT = 0;
  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    float x = (float)n * inv;
    float gD = g0[0] + (gDrumS - g0[0]) * x, gK = g0[1] + (gKickS - g0[1]) * x, gB = g0[2] + (gBajoS - g0[2]) * x, gM = g0[3] + (gMelS - g0[3]) * x;
    float vv = v0 + (volS - v0) * x;
    fundido += (objF - fundido) * 0.003f;          // ~8 ms
    corteGate += ((estadoSub == E_ESPERA ? 0.0f : 1.0f) - corteGate) * 0.02f;
    // batería con su filtro
    float k = bufKick[n] * gK;
    float dl = k + bufIndL[n], dr = k + bufIndR[n], lp, bp;
    pasoFiltro(fDrumL, dl + 1e-18f, lp, bp); dl += (lp - dl) * (1.0f - abDrum);
    pasoFiltro(fDrumR, dr + 1e-18f, lp, bp); dr += (lp - dr) * (1.0f - abDrum);
    dl *= gD; dr *= gD;
    float ba = bufBajo[n] * gB, ml = bufMelL[n] * gM, mr = bufMelR[n] * gM;
    pkD = fmaxf(pkD, fabsf(dl)); pkB = fmaxf(pkB, fabsf(ba)); pkM = fmaxf(pkM, fabsf(ml) + fabsf(mr)); pkO = fmaxf(pkO, fabsf(oscL[n]));
    float g = vv * fundido * corteGate * MASTER;
    const float gG = vv * fundido * MASTER;          // el golpe final y su cola: fuera del corte
    float l = (dl + ba + ml + ecoBufL[n] + oscL[n] + bufFxL[n]) * g;
    float r = (dr + ba + mr + ecoBufR[n] + oscR[n] + bufFxR[n]) * g;
    pasoFiltro(fMasL, l + 1e-18f, lp, bp); l += (lp - l) * (1.0f - abMas);   // pasa-bajos general
    pasoFiltro(fMasR, r + 1e-18f, lp, bp); r += (lp - r) * (1.0f - abMas);
    l += golpeL[n] * gG; r += golpeR[n] * gG;
    float yl = l - dcX1L + 0.9985f * dcY1L; dcX1L = l; dcY1L = yl; l = yl;
    float yr = r - dcX1R + 0.9985f * dcY1R; dcX1R = r; dcY1R = yr; r = yr;
    float pk = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
    if (pk > medPico) medPico = pk;
    if (pk > pkT) pkT = pk;
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
  nivelPub[0] = pkD; nivelPub[1] = pkB; nivelPub[2] = pkM * 0.5f; nivelPub[3] = pkO; nivelPub[4] = pkT;
  muestraPub = muestraGlobal;
  medSec[4] += (CICLOS() - cm0) >> 6;
}

// ==============================================================================================================================================
// MATRIZ 32×8 (tarea de LEDs, core 0) — 8 escenas psicodélicas amarradas al tempo (las del v1)
// ==============================================================================================================================================
const uint8_t HUE_EV[NUM_EV] = { 130, 45, 160, 0, 110, 20, 200, 24, 4, 180, 230 };   // HAT CAJA PERC CLAP ABIERTO TOM KEYS BOMBO BAJO ARP OSC

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
float    flash[NUM_EV];
bool     nuevo[NUM_EV];
#define MAX_FIGURAS 24
Figura   figuras[MAX_FIGURAS];
uint32_t rngLed = 0xA5A5A5A5u;
static inline float lrnd() { rngLed ^= rngLed << 13; rngLed ^= rngLed >> 17; rngLed ^= rngLed << 5; return (rngLed >> 8) * (1.0f / 16777216.0f); }

float    T = 0.0f;                    // tiempo musical, en negras
float    fk = 0.0f, eb = 0.0f;        // golpe del bombo · envolvente del bajo
float    brilloEsc = 1.0f;
uint32_t bombosVistos = 0;
uint8_t hueBase = 0; float hueSpan = 120.0f, dirE = 1.0f, paramE = 0.5f;

static inline float fs(float x) { float p = x * 0.15915494f; p -= floorf(p); return seno(p); }
static inline float fc(float x) { return fs(x + 1.5707963f); }
static inline float frac(float x) { return x - floorf(x); }
inline void pon(int x, int y, float hue, float v, uint8_t sat = 255) {
  if (v <= 0.004f) return;
  if (v > 1.0f) v = 1.0f;
  pix(x, y, CHSV((uint8_t)(hueBase + (int)hue), sat, (uint8_t)(v * 255.0f * brilloEsc)));
}

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
void escenaCaleido() {
  borrarMatriz();
  const float a = T * 0.5f * dirE, cr = fc(a), sr = fs(a), umbral = 0.25f - 0.35f * fk;
  for (int x = 0; x < MATRIZ_ANCHO; x++) for (int y = 0; y < MATRIZ_ALTO; y++) {
    float ax = fabsf(x - 15.5f), ay = fabsf(y - 3.5f);
    float u = fmodf(ax, 8.0f); if (u > 4.0f) u = 8.0f - u;
    float px = u * cr - ay * sr, py = u * sr + ay * cr;
    float val = fs(px * (1.1f + paramE) + T) * fs(py * 1.3f - T * 0.7f) + 0.5f * fs((px + py) * 0.9f + T * 1.6f);
    float v = (val - umbral) * 2.0f;
    if (v > 0.0f) pon(x, y, ax * 6.0f + val * 80.0f + T * 12.0f, v);
  }
}
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
void soltarFigura(float cx, float cy, float vr, uint8_t tipo, uint8_t hue, float brillo) {
  int k = 0; float menor = 9.0f;
  for (int i = 0; i < MAX_FIGURAS; i++) { if (!figuras[i].viva) { k = i; break; } if (figuras[i].brillo < menor) { menor = figuras[i].brillo; k = i; } }
  Figura &f = figuras[k];
  f.viva = true; f.cx = cx; f.cy = cy; f.r = 0.0f; f.vr = vr; f.tipo = tipo; f.hue = hue; f.brillo = brillo;
}
void escenaFiguras() {
  fundirMatriz(70);
  const uint8_t g = hueBase;
  if (nuevo[EV_BOMBO])  soltarFigura(15.5f, 3.5f, 0.9f, 0, HUE_EV[EV_BOMBO] + g, 1.0f);
  if (nuevo[I_CAJA])    soltarFigura(4.0f + lrnd() * 24.0f, 3.5f, 0.6f, 1, HUE_EV[I_CAJA] + g, 0.9f);
  if (nuevo[I_HAT])     soltarFigura(lrnd() * 31.0f, lrnd() * 7.0f, 0.45f, 2, HUE_EV[I_HAT] + g, 0.6f);
  if (nuevo[I_PERC])    soltarFigura(4.0f + lrnd() * 24.0f, 7.0f, 0.7f, 3, HUE_EV[I_PERC] + g, 0.9f);
  if (nuevo[I_CLAP])    soltarFigura(15.5f, 3.5f, 0.75f, 2, HUE_EV[I_CLAP] + g, 0.8f);
  if (nuevo[I_ABIERTO]) soltarFigura(lrnd() * 31.0f, 0.0f, 0.5f, 2, HUE_EV[I_ABIERTO] + g, 0.5f);
  if (nuevo[I_TOM])     soltarFigura(4.0f + lrnd() * 24.0f, 7.0f, 0.55f, 0, HUE_EV[I_TOM] + g, 0.8f);
  if (nuevo[EV_ARP])    soltarFigura(lrnd() * 31.0f, lrnd() * 7.0f, 0.35f, 2, HUE_EV[EV_ARP] + g, 0.35f);
  if (nuevo[EV_OSC])    soltarFigura((float)(notaOscPub - tonicaPub + 12) * (31.0f / 36.0f), 3.5f, 0.3f, 1, HUE_EV[EV_OSC] + g, 0.7f);
  if (nuevo[EV_KEYS]) {                            // la melodía se ve: la altura de la nota es la posición
    float x = (float)(notaKeysPub - (KEYS_DESDE - 16)) * (31.0f / 40.0f);
    soltarFigura(x, 3.5f, 0.45f, 0, HUE_EV[EV_KEYS] + g, 0.9f);
  }
  for (int i = 0; i < MAX_FIGURAS; i++) {
    Figura &f = figuras[i]; if (!f.viva) continue;
    for (int x = 0; x < MATRIZ_ANCHO; x++) for (int y = 0; y < MATRIZ_ALTO; y++) {
      float dx = x - f.cx, dy = y - f.cy, d;
      switch (f.tipo) {
        case 0:  d = sqrtf(dx * dx + dy * dy); break;
        case 1:  d = fmaxf(fabsf(dx), fabsf(dy)); break;
        case 2:  d = fabsf(dx) + fabsf(dy); break;
        default: d = fmaxf(fabsf(dx) * 0.866f + dy * 0.5f, -dy); break;
      }
      float v = 1.0f - fabsf(d - f.r) * 1.2f;
      if (v > 0.0f) pix(x, y, CHSV(f.hue + (uint8_t)(f.r * 6.0f), 240, (uint8_t)(v * f.brillo * 255.0f * brilloEsc)));
    }
    f.r += f.vr * (1.0f + 0.6f * fk); f.brillo *= 0.95f;
    if (f.r > 34.0f || f.brillo < 0.04f) f.viva = false;
  }
}
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
void escenaOndas() {
  fundirMatriz(110);
  const float act[4] = { eb, fmaxf(fmaxf(flash[I_CAJA], flash[I_PERC]), flash[I_CLAP]), fmaxf(flash[I_HAT], flash[I_ABIERTO]), fmaxf(fmaxf(flash[EV_KEYS], flash[I_TOM]), flash[EV_OSC]) };
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
const uint8_t HUE_PANEL[NUM_PANELES] = { 96, 0, 32, 128, 160 };   // verde · rojo · ámbar · cian · azul
volatile uint32_t panelCambioMs = 0;
void ledsEstado(bool corre) {
  const float f[5] = { flash[EV_BOMBO], flash[EV_BAJO], fmaxf(flash[I_HAT], flash[I_ABIERTO]),
                       fmaxf(fmaxf(flash[I_CAJA], flash[I_PERC]), fmaxf(flash[I_CLAP], flash[I_TOM])), fmaxf(flash[EV_KEYS], fmaxf(flash[EV_ARP], flash[EV_OSC])) };
  const uint8_t hue[5] = { HUE_EV[EV_BOMBO], HUE_EV[EV_BAJO], HUE_EV[I_HAT], HUE_EV[I_PERC], HUE_EV[EV_KEYS] };
  for (int i = 0; i < 5; i++) leds[i] = CHSV(hue[i], 220, (uint8_t)(8 + 140 * f[i]));
  const bool mute[5] = { !reqC.drumOn || !reqC.kick, !reqC.bajoOn, !reqC.drumOn, !reqC.drumOn, !reqC.melOn };   // callado: rojo tenue
  for (int i = 0; i < 5; i++) if (mute[i]) leds[i] = CRGB(28, 0, 0);
  if (breakPub) { leds[0] = CRGB(90, 0, 0); leds[1] = CRGB(90, 0, 0); }
  if (estadoSubPub == E_SUBIDA && ((millis() / 300) & 1)) { leds[0] = CRGB(0, 0, 0); leds[1] = CRGB(0, 0, 0); }
  if (estadoSubPub == E_ESPERA) { leds[0] = CRGB(200, 0, 0); leds[1] = CRGB(200, 0, 0); }
  const int p = panelPub;
  leds[5] = CHSV(HUE_PANEL[p], 230, corre ? (uint8_t)(40 + 150 * pulsoNegra) : 40);
  if (millis() - panelCambioMs < 350) for (int i = 0; i < 6; i++) leds[i] = CHSV(HUE_PANEL[p], 230, 170);   // recién cambiado: todos del color del panel
  if (millis() < cargaAltaHasta) leds[5] = CRGB(140, 0, 140);
}

uint32_t vistoContador = 0, bloqueEscena = 0;
bool estabaTocando = false; int estiloVisto = -1;
void elegirEscena() {
  int n = (int)(lrnd() * (NUM_ESCENAS - 1)); escena = (escena + 1 + n) % NUM_ESCENAS;   // nunca la misma
  hueBase = (uint8_t)(ESTILOS[reqC.estilo].hue + lrnd() * 60.0f);                      // la paleta parte del color del estilo
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
    if ((pasoTotalPub & 3) == 0) pulsoNegra = 1.0f;
    if ((cp >> 6) != bloqueEscena) { bloqueEscena = cp >> 6; elegirEscena(); }   // cada 4 compases
  }
  if (corre && !estabaTocando) elegirEscena();                                   // en cada Play
  if (reqC.estilo != estiloVisto) { estiloVisto = reqC.estilo; elegirEscena(); } // y en cada estilo
  estabaTocando = corre;

  T += dt * (corre ? bpmPub / 60.0f : 0.25f);
  if (nuevo[EV_BOMBO]) { T += (roundf(T) - T) * 0.6f; bombosVistos++; }
  fk = flash[EV_BOMBO]; eb = envBajoPub;
  brilloEsc = corre ? 1.0f : 0.35f;

  int e = escena;
  if (!corre && (e == E_FIGURAS || e == E_ONDAS)) e = E_TUNEL;
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
Ctl  ctl;                           // lo que mandan los botones
int  panel = P_GENERAL;
// BTN5: un toque entra o sale del break. Mantenido BREAK_LARGO_MS arranca la SUBIDA del v1 (16
// compases que terminan en un golpe y esperan en silencio); el próximo toque es el DROP.
const unsigned long BREAK_LARGO_MS = 2000;
unsigned long t5 = 0;
bool sosteniendo5 = false, enSubida = false;

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

void aplicarEstilo(int e) {                        // el estilo trae sus patrones y sus timbres; después se cambian en cada panel
  const Estilo &E = ESTILOS[e];
  ctl.estilo = e;
  ctl.patDrum = 0; ctl.patBajo = 0; ctl.patMel = 0;
  ctl.kit = E.kit; ctl.tBajo = E.tBajo; ctl.tMel = E.tMel; ctl.tOsc = E.tOsc;
}

void cambiarPanel(int p) {
  panel = p; panelPub = p; panelCambioMs = millis();
  soltarPots();
}

// Un botón = una función, en el flanco de presión. En TODOS los paneles BTN4 pasa al siguiente
// panel y BTN5 entra o sale del break; BTN1–BTN3 son del panel.
void accionBoton(int b) {
  if (b == 3) {
    cambiarPanel((panel + 1) % NUM_PANELES);
  } else if (b == 4) {
    if (enSubida) { enSubida = false; ctl.brk = false; reqDrop = reqDrop + 1; }   // el DROP: todo desde el "1"
    else { ctl.brk = !ctl.brk; t5 = millis(); sosteniendo5 = true; }
  } else switch (panel) {
    case P_GENERAL:
      if (b == 0)      { ctl.tocar = !ctl.tocar; ctl.brk = false; enSubida = false; }
      else if (b == 1) aplicarEstilo((ctl.estilo + 1) % NUM_ESTILOS);
      else             reqFill = reqFill + 1;
      break;
    case P_BAJO:
      if (b == 0)      ctl.bajoOn = !ctl.bajoOn;
      else if (b == 1) ctl.patBajo = (ctl.patBajo + 1) % NUM_PAT;
      else             ctl.tBajo = (ctl.tBajo + 1) % NUM_TBAJO;
      break;
    case P_DRUM:
      if (b == 0)      ctl.drumOn = !ctl.drumOn;
      else if (b == 1) ctl.patDrum = (ctl.patDrum + 1) % NUM_PAT;
      else             ctl.kit = (ctl.kit + 1) % NUM_KITS;
      break;
    case P_MEL:
      if (b == 0)      ctl.melOn = !ctl.melOn;
      else if (b == 1) ctl.patMel = (ctl.patMel + 1) % NUM_PAT;
      else             ctl.tMel = (ctl.tMel + 1) % NUM_TMEL;
      break;
    case P_OSC:
      if (b == 0)      ctl.oscOn = !ctl.oscOn;
      else if (b == 1) ctl.tOsc = (ctl.tOsc + 1) % NUM_TOSC;
      else             ctl.octOsc = (ctl.octOsc + 1) % 3;   // normal → +1 → −1
      break;
  }
  publicarCtl();
}

void pasoControl() {
  unsigned long ahora = millis();
  for (int b = 0; b < 5; b++) {
    bool nivel = digitalRead(BTN_PIN[b]);
    if (nivel == LOW && bNivel[b] == HIGH && (ahora - bTiempo[b]) > DEBOUNCE_MS) {
      bTiempo[b] = ahora;
      accionBoton(b);
    }
    bNivel[b] = nivel;
  }
  if (sosteniendo5 && bNivel[4] == HIGH) sosteniendo5 = false;
  if (sosteniendo5 && ahora - t5 > BREAK_LARGO_MS) {   // mantenido: empieza la SUBIDA
    sosteniendo5 = false;
    if (ctl.tocar) { enSubida = true; ctl.brk = true; reqSubida = reqSubida + 1; publicarCtl(); }
  }

  static uint8_t scan = 0;
  int i = scan; scan = (scan + 1) & 3;
  float v = readPot(POT_PIN[i]);
  if (!potTomado[i] && fabsf(v - potEntrada[i]) > UMBRAL_TOMA) potTomado[i] = true;
  if (potTomado[i]) pPot[panel][i] += (v - pPot[panel][i]) * 0.3f;
}

// ─── USB: líneas para las proyecciones ───────────────────────
uint32_t enviados[NUM_EV];
void enviarLineas(unsigned long t) {
  char b[400];
#if LINEA_RESONANCIA
  static unsigned long ultInfo = 0; static int estiloInfo = -1;
  if (t - ultInfo > 5000 || estiloInfo != reqC.estilo) {
    ultInfo = t; estiloInfo = reqC.estilo;
    snprintf(b, sizeof(b), "I,%s,PercuSynth Industrial Matriz,%.2f,0,%d,%d,batería|bajo|melodías|oscilador\n",
             ESTILOS[reqC.estilo].nombre, (double)bpmPub, reqC.estilo + 1, NUM_ESTILOS);
    Serial.print(b);
  }
#endif
  const uint32_t pt = pasoTotalPub, ms = muestraPub / (SAMPLE_RATE / 1000);
  const float fase = fasePub;
  const uint32_t cDrum = golpes[I_HAT] + golpes[I_CAJA] + golpes[I_PERC] + golpes[I_CLAP] + golpes[I_ABIERTO] + golpes[I_TOM] + golpes[EV_BOMBO];
  const uint32_t cMel = golpes[EV_KEYS] + golpes[EV_ARP];
  const bool on[4] = { reqC.drumOn, reqC.bajoOn, reqC.melOn, reqC.oscOn };
  const int volI[4] = { (int)(pPot[P_DRUM][0] * 1000), (int)(pPot[P_BAJO][0] * 1000), (int)(pPot[P_MEL][0] * 1000), 1000 };   // el oscilador no tiene volumen: va comprimido
  const int filI[4] = { (int)(pPot[P_DRUM][1] * 500), (int)(pPot[P_BAJO][1] * 500), (int)(pPot[P_MEL][1] * 500), (int)(pPot[P_OSC][1] * 500) };   // Resonancia: 0.5 = abierto
  int niv[5]; for (int k = 0; k < 5; k++) { float n = nivelPub[k] * 2.0f; niv[k] = (int)((n > 1 ? 1 : n) * 1000); }
  int btn = 0; for (int k = 0; k < 5; k++) if (bNivel[k] == LOW) btn |= 1 << k;
  int tom = 0; for (int k = 0; k < 4; k++) if (potTomado[k]) tom |= 1 << k;
#if LINEA_RESONANCIA
  // R: el formato de 43 campos de mezclador_pistas / trance_pistas. Pistas: 0 batería · 1 bajo · 2 melodías · 3 oscilador
  snprintf(b, sizeof(b), "R,%lu,%d,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
    (unsigned long)ms, (int)(fase * 1000), (unsigned long)(pt >> 2), tocandoPub ? 1 : 0,
    on[0] ? volI[0] : 0, on[1] ? volI[1] : 0, on[2] ? volI[2] : 0, on[3] ? volI[3] : 0, filI[0], filI[1], filI[2], filI[3],
    niv[0], niv[1], niv[2], niv[3],
    (unsigned long)cDrum, (unsigned long)golpes[EV_BAJO], (unsigned long)cMel, (unsigned long)golpes[EV_OSC],
    (unsigned long)golpes[EV_BOMBO], (unsigned long)(golpes[I_CAJA] + golpes[I_CLAP]), (unsigned long)(golpes[I_HAT] + golpes[I_ABIERTO]),
    0UL, 0, 0, 0, btn, panel, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, tom);
  Serial.print(b);
#endif
#if LINEA_PROYECCION
  // P: todo lo de cada instrumento (ver README). Los contadores sólo suben: una línea perdida no pierde golpes.
  int mute = (reqC.drumOn ? 0 : 1) | (reqC.kick ? 0 : 2) | (reqC.bajoOn ? 0 : 4) | (reqC.melOn ? 0 : 8) | (reqC.oscOn ? 0 : 16);
  snprintf(b, sizeof(b), "P,%lu,%d,%d,%d,%lu,%d,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
    (unsigned long)ms, reqC.estilo, panel, tocandoPub ? 1 : 0, (unsigned long)pt, (int)(bpmPub * 10), acordePub, tonicaPub,
    (unsigned long)golpes[EV_BOMBO], (unsigned long)golpes[I_HAT], (unsigned long)golpes[I_CAJA], (unsigned long)golpes[I_PERC],
    (unsigned long)golpes[I_CLAP], (unsigned long)golpes[I_ABIERTO], (unsigned long)golpes[I_TOM],
    (unsigned long)golpes[EV_BAJO], (unsigned long)golpes[EV_KEYS], (unsigned long)golpes[EV_ARP], (unsigned long)golpes[EV_OSC],
    notaBajoPub, notaKeysPub, notaArpPub, notaOscPub,
    niv[0], niv[1], niv[2], niv[3], niv[4], (int)(envBajoPub * 1000),
    mute, breakPub ? (estadoSubPub == E_SUBIDA ? 2 : (estadoSubPub == E_ESPERA ? 3 : 1)) : 0, repeatPub ? 1 : 0, fillPub ? 1 : 0,
    (int)(pPot[P_GENERAL][2] * 1000), (int)(pPot[P_GENERAL][3] * 1000), (int)(pPot[P_DRUM][3] * 1000),
    reqC.kit, reqC.tBajo, reqC.tMel);
  Serial.print(b);
#endif
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
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // OBLIGATORIO con audio: un print por USB CDC bloquea hasta que el PC lea
  for (int b = 0; b < 5; b++) pinMode(BTN_PIN[b], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  prepararSintesis();
  uint32_t semilla = 0;
  for (int i = 0; i < 16; i++) semilla = semilla * 31u + (uint32_t)analogRead(POT_PIN[3]);
  rngComp ^= semilla ^ (uint32_t)micros();
  memset(&ctl, 0, sizeof(ctl));
  ctl.drumOn = ctl.bajoOn = ctl.melOn = true; ctl.kick = true; ctl.arp = true;
  aplicarEstilo(0);
  publicarCtl();
  for (int p = 0; p < 4; p++) pPot[P_GENERAL][p] = readPot(POT_PIN[p]);   // el General ya en su lugar al arrancar
  lpfS = pPot[P_GENERAL][3];

#ifdef SIMULADOR
  iniciarLEDs();
#endif

  i2s_init();
#if MEDIR
  Serial0.begin(115200);
  ctl.tocar = true; ctl.oscOn = true; publicarCtl();   // arranca tocando (y con el oscilador) para medir sin tocar nada
#endif

#ifndef SIMULADOR
  // El audio tiene el core 1 para él solo; controles, USB y matriz van al core 0.
  xTaskCreatePinnedToCore(audioTask,   "audio",   8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask, "control", 6144, NULL,  3, NULL, 0);
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
  unsigned long ultLinea = 0;
  for (;;) {
    pasoControl();
    unsigned long t = millis();
    if (t - ultLinea >= LINEA_MS) { ultLinea = t; enviarLineas(t); }
#if MEDIR
    static unsigned long tMed = 0;
    if (millis() - tMed >= 2000) {
      tMed = millis();
      uint32_t n = medN ? medN : 1;
      Serial0.printf("CPU audio: prom %.0f %%  max %.0f %%  | pico antes del limitador %.2f  ganancia min %.2f  muestras en techo %lu | BPM %.0f estilo %s\n",
        100.0 * (double)medSuma * 64.0 / n / CICLOS_BLOQUE, 100.0 * (double)medMax / CICLOS_BLOQUE,
        (double)medPico, (double)medLimMin, (unsigned long)medTecho, (double)bpmPub, ESTILOS[reqC.estilo].nombre);
      Serial0.printf("   por sección (%% del presupuesto): batería %.0f  bajo %.0f  melodías %.0f  oscilador %.0f  master %.0f  pasos %.0f\n",
        100.0*64*medSec[0]/n/CICLOS_BLOQUE, 100.0*64*medSec[1]/n/CICLOS_BLOQUE, 100.0*64*medSec[2]/n/CICLOS_BLOQUE,
        100.0*64*medSec[3]/n/CICLOS_BLOQUE, 100.0*64*medSec[4]/n/CICLOS_BLOQUE, 100.0*64*medSec[5]/n/CICLOS_BLOQUE);
      for (int k = 0; k < 6; k++) medSec[k] = 0;
      medMax = 0; medSuma = 0; medN = 0; medTecho = 0; medLimMin = 1.0f; medPico = 0.0f;
    }
#endif
    vTaskDelay(1);
  }
}
// Los LEDs se inicializan DENTRO de su tarea, en el core 0: las interrupciones del RMT corren en el
// núcleo que lo inicializó (hecho en setup(), que corre en el core 1, le robaba al audio hasta 30 %
// de un bloque — medido en industrial_matriz).
void ledTask(void *)     { iniciarLEDs(); for (;;) { renderLEDs(); vTaskDelay(2); } }
void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }
#endif
