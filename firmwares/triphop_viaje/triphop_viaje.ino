// ==============================================================================================================================================
// PERCU-SYNTH — TRIP HOP VIAJE (máquina de trip hop · OLED: monitor de controles · matriz: efectos matemáticos) — GC Lab Chile
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
// - Pantalla OLED SSD1306 128x64 I2C en el conector OLED de la V2.0 |SDA -> 21, SCL -> 38| (0x3C)
// - 6 LEDs WS2812 SMD internos de la placa |DATA -> 46| (índices 0..5, indicadores de estado)
// - Matriz WS2812 de 32x8 (256 LEDs) encadenada DESPUÉS de los 6 SMD, mismo pin |DATA -> 46|
//   (índices 6..261 · fuente de 5 V propia recomendada, GND común con la placa)
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// - USB (el mismo del Arduino IDE): datos de cada instrumento para las proyecciones (Web Serial)
// - Módulo microSD por SPI (el cableado de grabador_campo) |SCK -> 14, MOSI -> 15, MISO -> 16, CS -> 17|
//   Opcional: sin tarjeta todo funciona igual, sólo no graba.
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled      (las proyecciones leen por este puerto)
// - Flash Mode         : DIO          (¡OPI rompe I2S!)
// - PSRAM              : OPI PSRAM  (el colchón de grabación de 8 s vive ahí; sin PSRAM baja a 0.4 s)
// - Partition Scheme   : el de siempre (el sketch son 470 KB: 35 % de la partición por omisión)
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core >= 3.x (incluye driver/i2s_std.h)
// - FastLED (gestor de librerías de Arduino)
// - Wire, SPI y SD (vienen con el core) — la OLED se maneja a mano, sin librería de pantalla
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Máquina de trip hop: batería arrastrada, bajo, armonía (rhodes, vibráfono, piano…) y un pad que
// siguen una progresión oscura y lenta, con sonido de cinta gastada. Cada instrumento tiene su
// panel para callarlo y cambiarle patrón y timbre, la OLED te muestra qué hace cada perilla, y si
// pones una microSD cada Play graba lo que tocas.
//
// Una máquina para hacer TRIP HOP, y nada más que trip hop. Nace del industrial_matriz_v2, donde el
// trip hop era uno de cinco estilos: acá los seis "dialectos" son seis variantes del mismo género y
// todo el resto del instrumento está puesto al servicio de ese sonido.
//
// Lo que define al género, y por lo tanto a este firmware:
//
//   LENTO Y ARRASTRADO. El tempo va de 58 a 110 BPM (no de 70 a 170) y arranca en 86. El swing
//   llega al 72 %. Y sobre todo: cada kit tiene un ARRASTRE por pista, en milisegundos — la caja
//   entra 15 a 30 ms DESPUÉS del tiempo, que es lo que hace que un groove suene tocado por una
//   persona y no por una máquina. Eso no se puede escribir en la rejilla de semicorcheas: a 86 BPM
//   una semicorchea son 174 ms, así que el arrastre es un planificador de golpes aparte, con
//   precisión de muestra.
//
//   LA CINTA. Queda el WOW y FLUTTER (la altura derivando porque la cinta no corre perfecta) y el
//   DESGASTE (saturación suave + ancho de banda recortado), los dos como CONSTANTES FIJAS, sin
//   panel: son parte del timbre de la máquina. Hubo además una capa de ruido de disco —crepitar,
//   siseo, retumbe, el golpe de la aguja— y se QUITÓ: el usuario la oía como chasquidos aleatorios
//   y, silenciando instrumento por instrumento, seguía sonando porque su panel no era un
//   instrumento. Si alguna vez vuelve: una capa cuyo trabajo es hacer chasquidos no puede ir
//   encendida y audible por omisión, y su control tiene que poder llevarla a inaudible.
//
//   LA ARMONÍA. Sólo dos escalas, elegidas por el usuario en un test de diez: MENOR ARMÓNICA y
//   FRIGIO DOMINANTE (el 5.º modo de la menor armónica: la 2.ª menor y la 3.ª mayor). Cada cambio de dialecto sortea una de 20
//   progresiones (10 por escala), nunca la que ya sonaba, con los acordes apilados en terceras de
//   la escala y con séptima: Am(maj7), Bø7, Cmaj7#5, E7(b9), G#°7… — los disminuidos y aumentados
//   van a propósito, son el color de estas escalas. El PAD toca el acorde completo (fundamental,
//   tercera, quinta y séptima) siguiendo la progresión, y las melodías son monofónicas y con mucho
//   espacio: en este género lo que no suena importa más.
//
// Y CANTIDAD, que es lo otro que hacía falta: 8 kits de batería · 6 timbres de bajo · 6
// instrumentos de armonía · 4 timbres de pad · 6 dialectos x 12 patrones de cada cosa · 20
// progresiones. Todo son
// TABLAS, escritas en grados de la escala y relativo al acorde que suena: ningún botón puede
// sacar una nota de tono ni un golpe de tiempo.
//
// LO QUE SE VE. La OLED es un MONITOR DE CONTROLES: el panel en que estás y su estado, los 4 pots
// con su valor en la unidad que mueven (y si están sueltos, dónde está la perilla), lo que hacen
// BTN2 y BTN3 en ese panel, los 16 pasos del compás, y el valor nuevo de cada botón en grande
// durante un segundo. La matriz 32x8 son EFECTOS MATEMÁTICOS que mutan cada 2 compases en el
// "1" (interferencia, anillos, espiral, rayos, rayas, rombos, cuadros, damero, con paleta y
// simetría sorteadas): bandas de color plano con negro entre medio, que avanzan a saltos con la
// rejilla; el bombo las engorda y la caja gira la paleta. (Antes las dos pantallas eran un vuelo
// sobre un planeta: el usuario no quiso nada figurativo ni calmo; quiso información en la OLED y
// en la matriz algo rítmico, definido y que cambie.)
//
// PROYECCIONES (Resonancia): 60 veces por segundo sale por el USB la línea `R` de
// mezclador_pistas/trance_pistas, con 4 pistas: batería · bajo · armonía · pad.
// Serial.setTxTimeoutMs(0): si nadie escucha se descarta sin bloquear, y se manda desde la tarea
// de control (core 0), nunca desde el audio.
//
// ARQUITECTURA: audio en su propia tarea en el CORE 1; en el CORE 0, tres tareas por orden de
// prioridad: los botones/pots/USB a 1 kHz, la matriz y la OLED. Los controles sólo dejan PEDIDOS
// que el audio aplica en el borde de un buffer, nunca tocan el secuenciador ni las voces.
// ==============================================================================================================================================
// FUNCIONAMIENTO — 5 PANELES. Un botón = una función, en el flanco de presión.
// ==============================================================================================================================================
// EN TODOS LOS PANELES:  BTN4 → siguiente panel (General → Batería → Bajo → Armonía → Pad)
//                        BTN5 → break (calla bombo y bajo) / vuelta del break
//
//   Al cambiar de panel cada pot queda "suelto" y no cambia nada hasta que lo muevas un poco (sin
//   saltos). El LED 5 dice el panel. Las secuencias corren siempre: el BTN1 de cada instrumento
//   es un play/stop "falso", así al volver entra en tiempo.
//
// GENERAL   BTN1 play/stop (arranca en el "1"; Stop y Play vuelven a lo mismo) · BTN2 dialecto (6) + progresión nueva
//           BTN3 fill al final del compás.
//           GRABA: cada Play abre una toma en la microSD (TRIPHOP_0001.WAV + un .TXT con dialecto,
//           escala, progresión y tempo) y su Stop la cierra. Mientras graba, un punto late en la
//           cabecera de la OLED; si sale hueco, la tarjeta no alcanzó y se perdió un trozo.
//           POT1 volumen · POT2 tempo (58-110) · POT3 swing (recto -> 72 %) · POT4 pasa-bajos general
// BATERÍA   BTN1 suena/calla · BTN2 patrón (12) · BTN3 siguiente kit (de 8). Son POLVO · BOOM BAP · 808 DUB · ESCOBILLAS ·
//           CINTA · PESADO · SECO · QUEBRADO · POT1 volumen · POT2 filtro · POT3 largo de los golpes
//           POT4 DENSIDAD: abajo del todo sólo el bombo. Arriba entran hasta los fantasmas
// BAJO      BTN1 suena/calla · BTN2 patrón (12) · BTN3 timbre (6: SUB · DUB · CONTRABAJO · PÚA · REESE · ÁCIDO)
//           POT1 volumen · POT2 filtro · POT3 ataque · POT4 caída
// ARMONÍA   BTN1 suena/calla · BTN2 patrón (12) · BTN3 siguiente instrumento (de 6). Son RHODES · VIBRÁFONO · PIANO POLVO ·
//           CLAVE · GUITARRA · CUERDAS · POT1 volumen · POT2 filtro · POT3 ataque · POT4 caída
// PAD       BTN1 suena/calla · BTN2 timbre (4: CUERDAS · MELOTRÓN · CORO · VIDRIO) · BTN3 registro (normal · +1 · -1).
//           El pad toca el ACORDE (fundamental · 3.ª · 5.ª · 7.ª) siguiendo la progresión.
//           POT1 volumen · POT2 filtro · POT3 ataque y suelta (corto ↔ muy largo) · POT4 envío al eco
//
// LEDs de la placa: 0 bombo · 1 bajo · 2 hats · 3 caja/perc/clap/tom · 4 armonía+pad — un
// instrumento callado se ve en rojo tenue. LED 5 = color del panel con el pulso de cada negra;
// MAGENTA si un bloque de audio usa más del 80 % de su tiempo. Con BREAK, 0 y 1 en rojo fijo.
//
// MODO DE USO:
// 1. En el panel General aprieta BTN1: arranca el trip hop (y si hay microSD, empieza a grabar).
//    POT1 es el volumen y POT2 el tempo.
// 2. BTN2 cambia de dialecto y sortea una progresión nueva. BTN3 mete un fill.
// 3. BTN4 recorre los paneles Batería, Bajo, Armonía y Pad. En cada uno BTN1 calla o deja sonar
//    ese instrumento, y BTN2 y BTN3 cambian patrón, kit o timbre (la OLED dice cuál).
// 4. BTN5 calla bombo y bajo (break); otro toque los trae de vuelta.
// 5. Para cerrar la grabación, vuelve al General y aprieta BTN1 (Stop).
// ==============================================================================================================================================

// El core de Arduino compila con -Os (tamaño). Este sketch es casi todo bucles de audio: con O2
// (velocidad) el mismo código usa bastante menos CPU por bloque (medido en industrial_matriz).
#pragma GCC optimize ("O2")
#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

#define MEDIR             0    // 1 = arranca tocando al peor caso e imprime CPU por Serial0 cada 2 s
#define LINEA_RESONANCIA  1    // línea `R` de 43 campos para web/public_html/resonancia/
#define LINEA_PROYECCION  0    // línea `P` con el detalle de cada instrumento
#define CON_OLED          1    // 0 = sin pantalla (no se inicia Wire ni su tarea)

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
// Una nota del acorde del pad. `cambiando` = su nota cambió con el acorde: se apaga, se reafina en
// el silencio (reafinar sonando sería un portamento) y vuelve a entrar con el ataque.
struct VozPad   { bool viva, cambiando; int nota, notaObj; float f1, f2, fs, inc1, inv1, inc2, inv2, amp, gL, gR; Filtro s; };
struct SegAc { int8_t grado; uint8_t compases; };              // un acorde de la progresión y cuántos compases dura
// Un golpe pendiente: el ARRASTRE del kit hace que la caja entre unos ms tarde, y eso no cabe en la
// rejilla de semicorcheas. Se dispara con precisión de muestra desde el bucle de bloques.
struct Pendiente { bool viva; int pista; float vel, largo, faltan; bool acento; };

// Un golpe de percusión: TODO lo que define su sonido. El motor (renderInd) es uno solo; un kit es
// una tabla de 6 de éstos + los números del bombo + el arrastre de cada pista.
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
  float arrastre[6];                 // ms que cada pista entra TARDE (0 = en el tiempo). El alma del género.
};
// Bajo: sierras (detune = razón entre las dos; 1 = una sola) · sub cuadrada una octava abajo ·
// seno redondo + octava (UNA parcial fuerte: se puede saturar) · drive · envOct cuánto abre el
// filtro cada nota · qMul resonancia · fenvRel el filtro se cierra en esta fracción de la caída ·
// nzAmt/nzTau/nzF el golpe de dedo o de púa (ruido pasa-banda en nzF)
struct TimbreBajo { const char *nombre; float sierras, detune, sub, seno, drive, envOct, qMul, fenvRel, nivel, nzAmt, nzTau, nzF; };
// Armonía: onda 0 pulso con PWM · 1 sierra · 2 dos sierras · 3 seno+octava+3.er armónico ·
// 4 RHODES (seno + campana x4.2 que muere con el filtro) · 5 VIBRÁFONO (seno + x4 con trémolo)
struct TimbreMel  { const char *nombre; uint8_t onda; float corte, envOct, q, fenvTau, nivel, trem, campana; };
// Pad: onda 0 dos sierras · 1 pulso + sub (melotrón) · 2 seno + octava (coro) · 3 casi seno (vidrio)
struct TimbrePad  { const char *nombre; uint8_t onda; float corte, q, nivel, detune; };
#define NUM_PAT 12
struct Estilo {
  const char *nombre;
  uint8_t hue;
  int8_t  transp;                    // semitonos sobre La: la tónica de la escala que sortee el Play
  uint8_t bpm;                       // el tempo que le queda (informativo: el POT2 manda)
  uint8_t kit, tBajo, tMel, tPad;    // timbres que trae el dialecto (después se cambian en cada panel)
  const char *bajo[NUM_PAT][2];      // 12 patrones: compás A (tres veces) y B (el 4.º)
  const char *bateria[NUM_PAT][7];   // 12 patrones: bombo + las 6 pistas, un compás de 16
  const char *fill[7];               // los 4 últimos pasos del compás, bombo + 6 pistas
  const char *mel[NUM_PAT];          // 12 patrones de armonía: 2 compases (32)…
  const char *arp[NUM_PAT];          // …y la figura del arpegio que va con cada uno (16)
};
// La parte de los controles que el audio lee entera al borde de cada buffer
struct Ctl {
  bool tocar, brk;
  int  estilo;
  int  prog;                         // la progresión que sorteó el último Play
  bool drumOn, bajoOn, melOn, padOn;
  int  patDrum, kit; bool kick;
  int  patBajo, tBajo;
  int  patMel, tMel; bool arp;
  int  tPad, octPad;                 // octPad: 0 normal · 1 +1 octava · 2 -1 octava
};

// ─── I2S PCM5102 (salida) ──────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
const float SR = (float)SAMPLE_RATE;
const float PI_F = 3.14159265f;

// ─── OLED SSD1306 ──────────────────────────────────────────
#define OLED_SDA   21
#define OLED_SCL   38
#define OLED_DIR   0x3C
const uint32_t I2C_HZ   = 800000;        // a 800 kHz un cuadro entero tarda ~12 ms; 400000 es el respaldo
const bool     OLED_GIRO_180 = false;    // si la pantalla quedó al revés en el soporte

// ─── LEDs: 6 SMD de la placa + matriz 32x8 en el mismo pin ──
#define LED_PIN        46
#define STATUS_LEDS     6
#define MATRIZ_ANCHO   32
#define MATRIZ_ALTO     8
#define MATRIZ_LEDS   (MATRIZ_ANCHO * MATRIZ_ALTO)
#define NUM_LEDS      (STATUS_LEDS + MATRIZ_LEDS)
#define LED_BRIGHT    150
#define LED_TYPE      WS2812
#define COLOR_ORDER   GRB
#define LED_MAX_MA   2000                     // tope duro de FastLED (baja el brillo global si se pasa)

// ══════════════════════════════════════════════════════════════════════════════════════════════
// CÓMO ESTÁ CABLEADA LA MATRIZ  <- si la prueba de orientación del arranque sale mal, se toca acá
// ══════════════════════════════════════════════════════════════════════════════════════════════
const bool MATRIZ_POR_COLUMNAS = true;    // false = el dato recorre filas de 32
const bool MATRIZ_SERPENTINA   = true;    // false = todas las columnas (o filas) en el mismo sentido
const bool MATRIZ_ESPEJO_X     = false;
const bool MATRIZ_ESPEJO_Y     = false;
const unsigned long PRUEBA_MS  = 3000;    // prueba de orientación al encender (0 = sin prueba)
// Limitador de corriente de la matriz: una nebulosa que prende los 256 LEDs hunde el riel de 5 V de
// la placa y eso CORTA EL AUDIO (medido en bajo_8_pasos_fsr_clock). Ataque instantáneo, vuelta lenta.
const float CARGA_MAXIMA = 0.090f;        // fracción del máximo teórico (~450 mA con una fuente propia)
const float RECUPERA_LED = 0.02f;

#define LED_REFRESH_MS 16
#define OLED_REFRESH_MS 33
#define LINEA_MS       16

// ─── Botones y pots ────────────────────────────────────────
const uint8_t BTN_PIN[5] = { 44, 42, 0, 45, 47 };
const uint8_t POT_PIN[4] = { 1, 2, 8, 10 };
#define DEBOUNCE_MS 25

CRGB leds[NUM_LEDS];

// ══════════════════════════════════════════════════════════════════════════════════════════════
// NIVELES Y AJUSTES GLOBALES
// ══════════════════════════════════════════════════════════════════════════════════════════════
const float MASTER        = 0.70f;        // medido en el v2: con 1.36 la mezcla llegaba a 1.6 antes del limitador
const float TECHO         = 0.80f;
#define LIM_LOOK 64                       // lookahead del limitador (1.5 ms)
const float NIVEL_BOMBO   = 1.00f;
const float NIVEL_BAJO    = 0.52f;
const float NIVEL_IND     = 1.20f;
const float NIVEL_MEL     = 0.13f;
const float NIVEL_ARP     = 0.11f;
const float NIVEL_PAD     = 0.085f;       // son cuatro voces a la vez: cada una suena bajo
const float SC_BAJO = 0.35f, SC_IND = 0.38f, SC_PAD = 0.22f;   // cuánto los agacha el bombo
const float BPM_MIN = 58.0f, BPM_RANGO = 52.0f;                // 58 a 110: el rango del género
const float SWING_MAX = 0.72f;                                 // POT3 del General: recto a 72 %
const float BAJO_GLIDE = 0.030f;
const float BAJO_ATK_MIN = 0.001f, BAJO_ATK_RANGO = 150.0f;    // 1 ms a 150 ms
const float BAJO_DEC_MIN = 0.060f, BAJO_DEC_RANGO = 30.0f;     // 60 ms a 1.8 s
const float MEL_ATK_MIN = 0.003f, MEL_ATK_RANGO = 120.0f;      // 3 ms a 360 ms
const float MEL_DEC_MIN = 0.080f, MEL_DEC_RANGO = 30.0f;       // 80 ms a 2.4 s
// El grado 0 de cada instrumento, en nota MIDI, antes del transporte del dialecto. Las cuatro son
// la misma nota en distintas octavas (La1 · La2 · La3): así el bajo, el pad y la armonía hablan del
// mismo acorde y no de uno a la quinta del otro.
const int   TONICA_BAJO = 33, TONICA_PAD = 45, TONICA_MEL = 57, TONICA_ARP = 57;
const int   MEL_DESDE = 60;                                    // si la tónica queda bajo Do4, sube una octava
const int   BAJO_MINIMO = 28;                                  // nunca abajo de Mi1
const float ECO_FEEDBACK = 0.56f, ECO_MEZCLA = 0.60f;          // eco ping-pong a corchea con puntillo
const float CHORUS_MS = 8.0f, CHORUS_PROF_MS = 2.8f, CHORUS_HZ = 0.5f, CHORUS_MEZCLA = 0.50f;
const float MEL_PWM_HZ = 0.30f, MEL_PWM_PROF = 0.18f;
const float PAD_GLIDE = 0.0025f;
const float PAD_ATK_MIN = 0.040f, PAD_ATK_RANGO = 60.0f;       // 40 ms a 2.4 s
const float PAD_ECO_MAX = 0.55f;
const float REVERB_FB = 0.80f;

// ══ ARMONÍA ══ Todo se escribe en GRADOS de la escala: ninguna nota puede salir de tono.
#define NUM_MODOS 2
#define M_ARM 0
#define F_DOM 1
const int ESCALAS[NUM_MODOS][7] = {
  { 0, 2, 3, 5, 7, 8, 11 },   // 0 menor armónica: la séptima mayor (el cine negro) y la 2.ª aumentada 6->7
  { 0, 1, 4, 5, 7, 8, 10 },   // 1 frigio dominante (hijaz): la 2.ª menor y la 2.ª aumentada 2->3
};
const char *NOMBRE_ESCALA[NUM_MODOS] = { "MENOR ARMONICA", "FRIGIO DOMINANTE" };

// ══ PROGRESIONES ══ Cada cambio de dialecto sortea una, nunca la que ya sonaba (Play/Stop no la
// toca). La progresión no está escrita en el dialecto (el dialecto trae el groove y los timbres). Cada acorde es un grado de la escala con su
// largo en compases; el acorde se arma apilando terceras DE LA ESCALA, con séptima, así que en la
// menor armónica los grados 0..6 son Am(maj7) · Bø7 · Cmaj7#5 · Dm7 · E7 · Fmaj7 · G#°7 (y la
// novena del arpegio sobre el V es la b9: E7(b9)). El frigio dominante tiene las MISMAS siete notas
// que la menor armónica una quinta abajo, pero descansa en el acorde MAYOR del grado 0:
// E7 · Fmaj7 · G#°7 · Am(maj7) · Bø7 · Cmaj7#5 · Dm7 (sobre la tónica del dialecto).
// Disminuidos y aumentados van a propósito: son el color de estas dos escalas.
#define NUM_SEG  6
struct Progresion { const char *nombre; uint8_t escala; SegAc seg[NUM_SEG]; };
#define NUM_PROG 20
const Progresion PROGS[NUM_PROG] = {
  //  menor armónica:  0 i · 1 ii° · 2 III+ · 3 iv · 4 V · 5 VI · 6 vii°
  { "NOIR",       M_ARM, { { 0, 2 }, { 3, 2 }, { 1, 2 }, { 4, 2 } } },   // la del test: Am(maj7) Dm7 Bø7 E7
  { "LAMENTO",    M_ARM, { { 0, 2 }, { 5, 2 }, { 3, 2 }, { 4, 2 } } },
  { "PEDAL",      M_ARM, { { 0, 4 }, { 3, 2 }, { 4, 2 } } },
  { "DESCENSO",   M_ARM, { { 0, 2 }, { 5, 2 }, { 1, 2 }, { 4, 2 } } },
  { "AGENTE",     M_ARM, { { 0, 2 }, { 2, 2 }, { 5, 2 }, { 4, 2 } } },   // el aumentado del cine de espías
  { "DISMINUIDO", M_ARM, { { 0, 2 }, { 6, 2 }, { 0, 2 }, { 4, 2 } } },
  { "SOMBRA",     M_ARM, { { 3, 2 }, { 4, 2 }, { 0, 4 } } },
  { "VALS NEGRO", M_ARM, { { 0, 2 }, { 3, 2 }, { 5, 2 }, { 6, 2 } } },
  { "SUSPENSO",   M_ARM, { { 5, 2 }, { 4, 2 }, { 0, 4 } } },
  { "QUIETA",     M_ARM, { { 0, 6 }, { 4, 2 } } },
  //  frigio dominante:  0 I · 1 II · 2 iii° · 3 iv · 4 v° · 5 VI+ · 6 vii
  { "HIJAZ",      F_DOM, { { 0, 2 }, { 1, 2 }, { 6, 2 }, { 0, 2 } } },   // la del test: E Fmaj7 Dm7 E7
  { "DESIERTO",   F_DOM, { { 0, 2 }, { 3, 2 }, { 1, 2 }, { 0, 2 } } },
  { "ESPEJISMO",  F_DOM, { { 0, 2 }, { 5, 2 }, { 1, 2 }, { 0, 2 } } },
  { "MEDIALUNA",  F_DOM, { { 0, 2 }, { 6, 2 }, { 1, 2 }, { 0, 2 } } },
  { "OSCURO",     F_DOM, { { 0, 2 }, { 4, 2 }, { 1, 2 }, { 0, 2 } } },
  { "CARAVANA",   F_DOM, { { 1, 2 }, { 0, 2 }, { 6, 2 }, { 0, 2 } } },
  { "ZOCO",       F_DOM, { { 0, 2 }, { 1, 1 }, { 0, 1 }, { 3, 2 }, { 1, 2 } } },
  { "LETARGO",    F_DOM, { { 0, 4 }, { 1, 2 }, { 0, 2 } } },
  { "ORACULO",    F_DOM, { { 3, 2 }, { 1, 2 }, { 0, 4 } } },
  { "RITUAL",     F_DOM, { { 0, 1 }, { 1, 1 }, { 0, 1 }, { 1, 1 }, { 6, 2 }, { 0, 2 } } },
};

// Grados del arpegio y del pad, en grados de escala sobre la fundamental del acorde:
// fundamental · 3.ª · 5.ª · SÉPTIMA · octava · NOVENA. Las dos últimas son las que hacen al género.
const int8_t TONOS_ARP[6] = { 0, 2, 4, 6, 7, 9 };
const int8_t TONOS_PAD[4] = { 0, 2, 4, 6 };     // el acorde del pad: con séptima siempre

// ══ TIMBRES DEL BAJO (BTN3 del panel del bajo) ══
#define NUM_TBAJO 6
//                        nombre      sierras detune    sub   seno  drive envOct qMul fenvRel nivel  nzAmt nzTau   nzF
const TimbreBajo TBAJO[NUM_TBAJO] = {
  // Un seno puro a 41-55 Hz no lo reproduce un parlante chico (sonaba a silencio). El seno tiene UNA
  // parcial fuerte, así que se satura para sacarle 3.er y 5.º armónico audibles, y una sierra suave
  // por debajo le da definición; el peso sigue estando en la fundamental.
  { "SUB",        0.20f, 1.00000f, 0.00f, 1.00f, 3.00f, 1.2f, 0.6f, 0.80f, 1.10f, 0.00f, 0.010f, 1000.0f },
  { "DUB",        0.10f, 1.00000f, 0.00f, 1.00f, 3.60f, 0.7f, 0.5f, 0.90f, 1.15f, 0.00f, 0.010f, 1000.0f },   // más redondo y oscuro: el de las notas largas
  // CONTRABAJO: cuerpo de seno + un golpe de ruido corto (el DEDO). Sin ese golpe un bajo sin
  // trastes suena a sinte; con él, a madera.
  { "CONTRABAJO", 0.30f, 1.00000f, 0.00f, 0.95f, 1.60f, 1.6f, 0.7f, 0.45f, 1.05f, 0.55f, 0.012f, 1400.0f },
  { "PUA",        1.00f, 1.00521f, 0.25f, 0.00f, 1.70f, 3.2f, 1.0f, 0.45f, 1.00f, 0.35f, 0.006f, 2600.0f },   // dos sierras + el ruido de la púa
  { "REESE",      1.00f, 1.01160f, 0.20f, 0.00f, 1.30f, 1.4f, 0.8f, 0.80f, 0.95f, 0.00f, 0.010f, 1000.0f },   // dos sierras a 10 cents que baten
  { "ACIDO",      1.00f, 1.00000f, 0.00f, 0.00f, 1.90f, 4.5f, 1.9f, 0.30f, 0.85f, 0.00f, 0.010f, 1000.0f },   // una sierra, resonancia y envolvente a la 303
};

// ══ INSTRUMENTOS DE LA ARMONÍA (BTN3 del panel de armonía) ══
//   corte: el filtro reposa en corte·f · envOct: cuánto lo abre cada nota · fenvTau: en cuánto se cierra
//   trem: trémolo propio (el ventilador del vibráfono, el motor del Rhodes) · campana: parcial x4.2
//   que muere con el filtro (la lámina del Rhodes, el metal del vibráfono)
#define NUM_TMEL 6
//                     nombre         onda  corte envOct   q  fenvTau nivel  trem  campana
const TimbreMel TMEL[NUM_TMEL] = {
  { "RHODES",       4, 3.0f, 1.2f, 0.8f, 0.250f, 1.00f, 0.10f, 0.55f },   // el piano eléctrico del género
  { "VIBRAFONO",    5, 6.0f, 0.6f, 0.7f, 0.500f, 1.05f, 0.35f, 0.30f },   // casi senoidal, con su trémolo
  { "PIANO POLVO",  0, 2.2f, 2.6f, 1.0f, 0.060f, 1.05f, 0.00f, 0.00f },   // pulso oscuro que cierra rápido
  { "CLAVE",        2, 5.0f, 3.0f, 1.0f, 0.035f, 0.95f, 0.00f, 0.00f },   // clavecín: brillante y pulsado
  { "GUITARRA",     1, 1.7f, 3.6f, 0.9f, 0.040f, 1.10f, 0.00f, 0.00f },   // sierra apagada con la palma
  { "CUERDAS",      2, 2.4f, 1.0f, 0.7f, 0.400f, 0.85f, 0.06f, 0.00f },   // dos sierras, el filtro se queda abierto
};

// ══ PAD (panel 5) ══ cuatro voces tocando el acorde (fundamental · 3.ª · 5.ª · 7.ª) de la progresión
#define NUM_TPAD 4
//                    nombre       onda corte   q   nivel  detune
const TimbrePad TPAD[NUM_TPAD] = {
  { "CUERDAS",  0, 3.0f, 0.8f, 1.00f, 1.00350f },   // dos sierras por voz
  { "MELOTRON", 1, 2.2f, 0.9f, 0.95f, 1.00220f },   // pulso + sub: la cinta del melotrón
  { "CORO",     2, 5.0f, 0.7f, 1.15f, 1.00450f },   // seno + octava: voces lejanas
  { "VIDRIO",   3, 8.0f, 0.6f, 1.20f, 1.00150f },   // casi un seno: limpio y frío
};

// ══ KITS DE BATERÍA (BTN3 del panel de batería) ══
// Pistas (0..5): HAT · CAJA · PERC · CLAP · ABIERTO · TOM. Reglas que vienen de drum_poder:
// arriba de 1 kHz todo es RUIDO filtrado (pasa-banda, nunca un pasa-altos pelado: ése pasa todo
// hasta Nyquist y esa octava de arriba es filo); nada de parciales metálicos afinados (suenan a
// olla); el clap es UN golpe de ruido (tres palmadas suenan a clipeo); las dos parciales de una
// caja NO se saturan (dejan un tono puro en su diferencia); caídas por banda (arriba de 1 kHz
// hasta 0.14 s, sólo el plato llega a 0.50; 250 Hz-1 kHz hasta 0.10; 100-250 hasta 0.22; abajo de
// 100 hasta 0.70). Y el ARRASTRE: los ms que cada pista entra tarde.
#define NUM_IND     6
#define I_HAT       0
#define I_CAJA      1
#define I_PERC      2
#define I_CLAP      3
#define I_ABIERTO   4
#define I_TOM       5
#define NUM_KITS 8
//               amp    f       r2     a0     a1     eDec    cuanto caidaD  nAmt   nDec    nAcc    fc0      fc1      barre  q      hp      pan    panRnd
const Kit KITS[NUM_KITS] = {
  // ── POLVO ── el break polvoriento: bombo blando, caja oscura, hats como escobillas gastadas
  { "POLVO", 50.0f, 3.2f, 0.040f, 0.30f, 0.34f, 0.20f, 900.0f, 0.06f, 1.50f, {
    { 0.22f, 6500.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.022f, 0.045f, 6500.0f, 6500.0f, 0,     0.55f, 0,      0.60f, 0     },
    { 0.52f, 178.0f, 1.520f, 0.38f, 0.22f, 0.085f, 0.07f, 0.020f, 1.15f, 0.120f, 0.150f, 2400.0f, 1500.0f, 0.10f, 0.90f, 140.0f, 0.50f, 0     },
    { 0.34f, 320.0f, 0,      0.55f, 0,     0.045f, 0.35f, 0.015f, 0.55f, 0.030f, 0.040f, 2600.0f, 1400.0f, 0.03f, 1.10f, 300.0f, 0.38f, 0.18f },
    { 0.32f, 1400.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.10f, 0.085f, 0.110f, 1500.0f, 1100.0f, 0.05f, 1.10f, 0,      0.56f, 0.08f },
    { 0.20f, 6200.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.110f, 0.140f, 6200.0f, 5200.0f, 0.08f, 0.50f, 0,      0.40f, 0     },
    { 0.40f, 105.0f, 0,      0.82f, 0,     0.190f, 0.28f, 0.035f, 0.22f, 0.012f, 0.012f, 850.0f,  850.0f,  0,     0.80f, 60.0f,  0.50f, 0.20f } },
    { 0.0f, 22.0f, 10.0f, 26.0f, 0.0f, 12.0f } },
  // ── BOOM BAP ── el de los 90: pegada, caja brillante, todo más apretado
  { "BOOM BAP", 54.0f, 4.2f, 0.030f, 0.22f, 0.30f, 0.40f, 1300.0f, 0.22f, 1.70f, {
    { 0.26f, 8200.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.016f, 0.040f, 8200.0f, 8200.0f, 0,     0.60f, 0,      0.62f, 0     },
    { 0.58f, 196.0f, 1.560f, 0.42f, 0.26f, 0.070f, 0.10f, 0.018f, 1.30f, 0.105f, 0.135f, 3400.0f, 2200.0f, 0.07f, 1.00f, 170.0f, 0.50f, 0     },
    { 0.36f, 420.0f, 0,      0.50f, 0,     0.030f, 0.50f, 0.012f, 0.70f, 0.025f, 0.035f, 3200.0f, 1800.0f, 0.03f, 1.20f, 350.0f, 0.34f, 0.20f },
    { 0.40f, 1200.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.15f, 0.060f, 0.085f, 1600.0f, 1150.0f, 0.04f, 1.20f, 0,      0.58f, 0.08f },
    { 0.22f, 7400.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.120f, 0.150f, 7400.0f, 6200.0f, 0.09f, 0.55f, 0,      0.40f, 0     },
    { 0.44f, 128.0f, 0,      0.80f, 0,     0.130f, 0.35f, 0.025f, 0.25f, 0.010f, 0.010f, 1000.0f, 1000.0f, 0,     0.80f, 65.0f,  0.50f, 0.20f } },
    { 0.0f, 16.0f, 6.0f, 18.0f, 0.0f, 8.0f } },
  // ── 808 DUB ── bombo largo que se queda sonando, clap grande, todo con aire
  { "808 DUB", 42.0f, 3.0f, 0.055f, 0.55f, 0.22f, 0.10f, 750.0f, 0.05f, 1.35f, {
    { 0.20f, 7000.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.020f, 0.045f, 7000.0f, 7000.0f, 0,     0.50f, 0,      0.60f, 0     },
    { 0.50f, 185.0f, 1.550f, 0.40f, 0.24f, 0.100f, 0.06f, 0.020f, 1.20f, 0.140f, 0.140f, 3000.0f, 1700.0f, 0.12f, 0.70f, 150.0f, 0.50f, 0     },
    { 0.30f, 0,      0,      0,     0,     0.010f, 0,     0.010f, 1.00f, 0.035f, 0.045f, 2800.0f, 2800.0f, 0,     1.40f, 0,      0.30f, 0.20f },
    { 0.44f, 1300.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.20f, 0.100f, 0.130f, 1500.0f, 1000.0f, 0.07f, 1.00f, 0,      0.60f, 0.10f },
    { 0.22f, 6600.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.130f, 0.140f, 6600.0f, 5600.0f, 0.10f, 0.50f, 0,      0.38f, 0     },
    { 0.46f, 98.0f,  0,      0.85f, 0,     0.200f, 0.45f, 0.050f, 0.08f, 0.010f, 0.010f, 800.0f,  800.0f,  0,     0.80f, 55.0f,  0.50f, 0.22f } },
    { 0.0f, 20.0f, 8.0f, 24.0f, 0.0f, 10.0f } },
  // ── ESCOBILLAS ── jazz: casi todo es ruido, nada golpea, y es el kit que más se arrastra
  { "ESCOBILLAS", 46.0f, 2.6f, 0.050f, 0.26f, 0.20f, 0.14f, 700.0f, 0.03f, 1.25f, {
    { 0.20f, 5200.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.035f, 0.060f, 5200.0f, 4200.0f, 0.03f, 0.45f, 0,      0.60f, 0     },
    { 0.44f, 170.0f, 1.500f, 0.20f, 0.10f, 0.060f, 0.05f, 0.020f, 1.30f, 0.130f, 0.160f, 1900.0f, 1200.0f, 0.12f, 0.60f, 130.0f, 0.50f, 0     },
    { 0.26f, 0,      0,      0,     0,     0.010f, 0,     0.010f, 1.00f, 0.140f, 0.140f, 1500.0f, 900.0f,  0.13f, 0.40f, 0,      0.32f, 0.25f },   // el barrido de la escobilla
    { 0.26f, 1200.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.090f, 0.110f, 1300.0f, 900.0f,  0.06f, 0.80f, 0,      0.58f, 0.10f },
    { 0.18f, 5600.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.130f, 0.140f, 5600.0f, 4600.0f, 0.10f, 0.45f, 0,      0.40f, 0     },
    { 0.36f, 112.0f, 0,      0.75f, 0,     0.160f, 0.25f, 0.035f, 0.35f, 0.014f, 0.014f, 780.0f,  780.0f,  0,     0.80f, 60.0f,  0.50f, 0.20f } },
    { 4.0f, 26.0f, 14.0f, 28.0f, 4.0f, 14.0f } },
  // ── CINTA ── todo con el brillo comido, como un cassette que ya dio muchas vueltas
  { "CINTA", 48.0f, 2.8f, 0.048f, 0.34f, 0.28f, 0.16f, 800.0f, 0.04f, 1.45f, {
    { 0.18f, 4800.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.025f, 0.050f, 4800.0f, 4000.0f, 0.02f, 0.50f, 0,      0.58f, 0     },
    { 0.48f, 182.0f, 1.540f, 0.36f, 0.20f, 0.090f, 0.07f, 0.020f, 1.10f, 0.115f, 0.140f, 2000.0f, 1300.0f, 0.10f, 0.80f, 140.0f, 0.50f, 0     },
    { 0.30f, 300.0f, 0,      0.50f, 0,     0.040f, 0.30f, 0.015f, 0.50f, 0.028f, 0.038f, 2000.0f, 1200.0f, 0.03f, 1.00f, 280.0f, 0.36f, 0.18f },
    { 0.30f, 1100.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.05f, 0.080f, 0.105f, 1200.0f, 900.0f,  0.05f, 1.00f, 0,      0.56f, 0.08f },
    { 0.18f, 5000.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.115f, 0.140f, 5000.0f, 4200.0f, 0.09f, 0.50f, 0,      0.40f, 0     },
    { 0.38f, 100.0f, 0,      0.80f, 0,     0.180f, 0.30f, 0.040f, 0.20f, 0.012f, 0.012f, 760.0f,  760.0f,  0,     0.80f, 58.0f,  0.50f, 0.20f } },
    { 2.0f, 24.0f, 12.0f, 26.0f, 2.0f, 12.0f } },
  // ── PESADO ── el lado oscuro: bombo enorme y lento, tom profundo, plato que se queda
  { "PESADO", 38.0f, 3.6f, 0.060f, 0.62f, 0.40f, 0.26f, 1000.0f, 0.10f, 1.90f, {
    { 0.22f, 7600.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.018f, 0.040f, 7600.0f, 7600.0f, 0,     0.60f, 0,      0.60f, 0     },
    { 0.56f, 160.0f, 1.580f, 0.44f, 0.28f, 0.095f, 0.08f, 0.022f, 1.25f, 0.125f, 0.150f, 2600.0f, 1600.0f, 0.11f, 0.90f, 145.0f, 0.50f, 0     },
    { 0.42f, 150.0f, 0,      0.78f, 0,     0.070f, 0.30f, 0.020f, 0.30f, 0.018f, 0.024f, 1200.0f, 700.0f,  0.04f, 0.90f, 80.0f,  0.44f, 0.15f },
    { 0.34f, 1000.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.15f, 0.095f, 0.120f, 1100.0f, 800.0f,  0.06f, 1.00f, 0,      0.60f, 0.08f },
    { 0.24f, 6000.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.300f, 0.420f, 6000.0f, 4500.0f, 0.25f, 0.45f, 0,      0.40f, 0     },
    { 0.50f, 88.0f,  0,      0.85f, 0,     0.220f, 0.35f, 0.045f, 0.18f, 0.012f, 0.012f, 700.0f,  700.0f,  0,     0.80f, 50.0f,  0.50f, 0.20f } },
    { 0.0f, 18.0f, 8.0f, 20.0f, 0.0f, 10.0f } },
  // ── SECO ── el break de funk de los 70: cortísimo, sin colas, casi sin arrastre
  { "SECO", 56.0f, 4.6f, 0.026f, 0.16f, 0.26f, 0.45f, 1500.0f, 0.26f, 1.60f, {
    { 0.26f, 8600.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.012f, 0.028f, 8600.0f, 8600.0f, 0,     0.65f, 0,      0.62f, 0     },
    { 0.55f, 205.0f, 1.570f, 0.40f, 0.24f, 0.050f, 0.12f, 0.014f, 1.25f, 0.075f, 0.095f, 3600.0f, 2400.0f, 0.05f, 1.10f, 180.0f, 0.50f, 0     },
    { 0.34f, 450.0f, 0,      0.48f, 0,     0.022f, 0.55f, 0.010f, 0.65f, 0.018f, 0.026f, 3400.0f, 2000.0f, 0.02f, 1.30f, 380.0f, 0.33f, 0.20f },
    { 0.38f, 1250.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.10f, 0.045f, 0.065f, 1700.0f, 1200.0f, 0.03f, 1.30f, 0,      0.58f, 0.08f },
    { 0.20f, 7800.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.080f, 0.105f, 7800.0f, 6800.0f, 0.06f, 0.55f, 0,      0.40f, 0     },
    { 0.42f, 140.0f, 0,      0.76f, 0,     0.090f, 0.40f, 0.020f, 0.26f, 0.009f, 0.009f, 1100.0f, 1100.0f, 0,     0.80f, 70.0f,  0.50f, 0.20f } },
    { 0.0f, 10.0f, 4.0f, 12.0f, 0.0f, 6.0f } },
  // ── QUEBRADO ── el sampler mal usado a propósito: golpes muy cortos y bien abiertos de lado a lado
  { "QUEBRADO", 58.0f, 5.5f, 0.018f, 0.14f, 0.18f, 0.50f, 1900.0f, 0.34f, 1.55f, {
    { 0.24f, 9500.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.009f, 0.022f, 9500.0f, 9500.0f, 0,     0.70f, 0,      0.62f, 0.15f },
    { 0.50f, 235.0f, 1.550f, 0.26f, 0.13f, 0.032f, 0.28f, 0.010f, 1.35f, 0.040f, 0.058f, 5500.0f, 2600.0f, 0.03f, 1.10f, 200.0f, 0.50f, 0.10f },
    { 0.40f, 520.0f, 0,      0.45f, 0,     0.018f, 0.80f, 0.008f, 0.55f, 0.013f, 0.018f, 4000.0f, 1600.0f, 0.02f, 1.20f, 420.0f, 0.30f, 0.30f },
    { 0.34f, 1600.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.10f, 0.028f, 0.042f, 1900.0f, 1400.0f, 0.02f, 1.40f, 0,      0.60f, 0.20f },
    { 0.20f, 8800.0f, 0,     0,     0,     0.010f, 0,     0.010f, 1.00f, 0.050f, 0.075f, 8800.0f, 7600.0f, 0.04f, 0.70f, 0,      0.40f, 0.15f },
    { 0.40f, 165.0f, 0,      0.70f, 0,     0.060f, 0.55f, 0.014f, 0.20f, 0.008f, 0.008f, 1250.0f, 1250.0f, 0,     0.80f, 75.0f,  0.50f, 0.30f } },
    { 0.0f, 8.0f, 4.0f, 10.0f, 0.0f, 4.0f } },
};

// ══ CINTA FIJA ══ Lo que queda del soporte después de sacar el ruido del disco: la altura
// derivando porque la cinta no corre perfecta (WOW a la vuelta lenta, FLUTTER rápido) y el
// DESGASTE —saturación suave más la banda recortada arriba y abajo—. No son ruido, no tienen
// panel y no se tocan: son parte del timbre de la máquina, como el bloque TIMBRE FIJO de
// espacio_modular. Si alguna vez hay que moverlos, están acá y en un solo sitio.
const float CINTA_WOW      = 0.30f;   // 0 = la altura no deriva · 1 = mareo
const float CINTA_DESGASTE = 0.22f;   // 0 = limpio · 1 = cinta muy gastada

// ══ DISCOS (ya no hay panel; sólo queda el desgaste de cada uno como referencia) ══ el carácter del soporte. Los pots escalan estos números.
//   crepita: golpes por segundo del crepitar fino · pops: por segundo de los graves y gordos
//   siseo · retumbe · banda: hasta dónde llega el crepitar (más bajo = disco más sordo)
struct Disco { const char *nombre; float crepita, pops, siseo, retumbe, banda, desgaste; };
#define NUM_DISCOS 6
// Los "pops" son los tics gordos, y son LOS QUE SE OYEN COMO CHASQUIDOS: medido con todo lo demás
// callado, a 0.8 por segundo y −48 dBFS eran exactamente «chasquidos aleatorios». Van a ritmo de
// textura; el que quiera un disco destrozado lo sube con el POT1 del panel o pone RAYADO.
const Disco DISCOS[NUM_DISCOS] = {
  { "LIMPIO",  4.0f, 0.08f, 0.25f, 0.30f, 1800.0f, 0.10f },   // un disco nuevo igual tiene algo
  { "POLVO",  22.0f, 0.25f, 0.55f, 0.55f, 1300.0f, 0.25f },
  { "VIEJO",  60.0f, 0.70f, 0.90f, 0.85f, 1200.0f, 0.45f },
  { "RAYADO", 16.0f, 2.00f, 0.50f, 0.70f,  950.0f, 0.35f },   // pocos crepitares finos, muchos golpes
  { "CINTA",   6.0f, 0.10f, 1.00f, 0.40f, 1700.0f, 0.75f },   // casi no crepita: lo suyo es el siseo y el desgaste
  { "RADIO",  30.0f, 0.50f, 0.95f, 0.25f, 1100.0f, 0.60f },   // AM: sin graves y con la banda cerrada
};

// ══ DIALECTOS (BTN2 del General) ════════════════════════════════════════════════════════════════
// Seis maneras de hacer trip hop. Cada uno es una tabla: tonalidad, 12 patrones de batería, 12 de
// bajo, 12 de armonía con su arpegio, un fill, y los timbres con los que entra. La ESCALA y la
// PROGRESIÓN no están escritas en el dialecto: se sortean al cambiar de dialecto (ver PROGRESIONES).
//
// BATERÍA — 7 líneas de 16 (un compás de semicorcheas): BOMBO · HAT · CAJA · PERC · CLAP · ABIERTO · TOM
//   X golpe fuerte · x golpe normal · . golpe fantasma · - nada
//   En las 6 pistas la letra es también la PRIORIDAD: con la DENSIDAD (POT4 del panel) baja sólo
//   suenan las X; al subir entran las x y arriba del todo los fantasmas. Abajo del todo, nada. El
//   bombo no depende de la densidad. El ARRASTRE del kit es lo que mete la caja unos ms tarde.
// BAJO — 16 caracteres relativos a la FUNDAMENTAL DEL ACORDE del compás:
//   x fundamental · 3 tercera · 5 quinta · b SÉPTIMA menor (si en ese acorde es mayor, la quinta) ·
//   9 NOVENA · o octava · - silencio. El patrón A suena tres compases y el B en el cuarto.
// ARMONÍA — 32 caracteres (2 compases), 1..9 = grado contado desde la fundamental del acorde
//   (1 fundamental · 3 tercera · 5 quinta · 7 SÉPTIMA · 8 octava · 9 NOVENA; 2, 4 y 6 son las notas
//   de paso que le dan el color al modo), - silencio. Monofónico: cada nota toma el lugar de la
//   anterior, así que el espacio entre dos notas ES la duración de la primera.
//   arpegio: 16 caracteres, 0..5 = nota del acorde (0 fund · 1 3.ª · 2 5.ª · 3 7.ª · 4 8.ª · 5 9.ª), - silencio.
// FILL — 7 líneas de 4: los pasos 13-16 del compás. REEMPLAZA a la batería (nunca se suma: dos
//   golpes del mismo tambor en la misma muestra se matan) y el "1" siguiente trae un plato.
#define NUM_ESTILOS 6
const Estilo ESTILOS[NUM_ESTILOS] = {

  // ── BRISTOL ─────────────────────────────────────────────────────────────────────────────────
  // El sonido de Bristol: bombo pesado y quebrado, caja grande en 2 y 4 que entra tarde, bajo sub
  // larguísimo y un Rhodes con tres notas por compás.
  { "BRISTOL", 192, 2, 88, 0, 0, 0, 0,
    { { "x-------x--5----", "x-------x--5-b--" },     // notas largas
      { "x------x--x-----", "x------x--x--5-3" },     // empujado
      { "x---x---x---x---", "x---x---o---5---" },     // negras
      { "x--x---5x--x--b-", "x--x---5x--o--5-" },     // quebrado
      { "x-----------5---", "x-----------b---" },     // dub: dos notas por compás
      { "x--5--b--x--5---", "x--5--b--o--9---" },   // caminando
      { "x-------x-------", "x-------x---b-o-" },     // dos golpes, cierra arriba
      { "x--x-------x----", "x--x-------9----" },     // empujado con la novena
      { "x---------5--b--", "x---------5--9--" },     // cae tarde
      { "x-x-----x-x-----", "x-x-----o-o-----" },     // en pares
      { "x-------------x-", "x-----------5-b-" },     // anticipa el "1"
      { "x------5x-------", "x------5o---b---" } },     // quinta de paso
    { { "X------x--X-----", "x.x.x.x.x.x.x.x.", "----X-------X---", "--.---.---.-x-.-", "----------------", "--------------x-", "----------------" },   // pesado
      { "X--x------X--x--", "x-x-x-x-x-x-x-xx", "----X-------X--.", "-x---x---x---x--", "----x-------x---", "----------------", "-------------x-." },   // quebrado
      { "X-------x-x-----", "x..x..x..x..x..x", "--------X-------", "---.---x---.---x", "--------x-------", "------x-------x-", "----------------" },   // medio tiempo
      { "X--x--x---X-----", "xxx.xxx.xxx.xxx.", "----X--.----X-.-", ".-.-.-.-.-.-.-.-", "----x-------x---", "--------------x-", "----------x.----" },   // boom bap
      { "X----------x----", "x---x---x---x---", "----X-------X---", "----------------", "----------------", "----------x-----", "-------------.x-" },   // arrastrado
      { "X-xX------X-x---", "x.x.x.xxx.x.x.x.", "----X-------X---", "--x---.---x---.-", "----------------", "------x-------x-", "------------x---" },  // doble
      { "X---------X-----", "x-x-x-x-x-x-x-x-", "----X-------X---", "------.-------.-", "----------------", "--------------x-", "----------------" },   // abierto
      { "X--X------X-----", "x.x.x.x.x.x.x.x.", "----X-------X--.", "--.---.---.---.-", "------------x---", "----------------", "----------------" },   // bombo doble
      { "X-------X--x----", "x..x..x..x..x..x", "----X-------X---", "-x-------x------", "----------------", "------x---------", "-------------xx." },   // hats en 3
      { "X-x-------X-----", "xxxxxxxxxxxxxxxx", "----X---.---X---", "................", "----------------", "----------------", "----------------" },   // semicorcheas
      { "X------XX-------", "x---x---x---x---", "----X-------X-x-", "--x-------x-----", "----x-------x---", "----------------", "----------x-----" },   // tropezado
      { "X-----x---X--x--", "x.xx.xx.xx.xx.xx", "----X-------X---", "---.---.---.---.", "--------x-------", "--------------x-", "------------.x-x" } },   // lleno
    { "X---", "x.x.", "..xX", "----", "---X", "----", "x-x-" },
    { "5-------3-------4---3---2-------",   // el suspiro que baja
      "1---3---5-------7-------5---3---",   // el acorde abierto, lento
      "8-----7-5-------6-----5-3-------",   // descenso con la sexta
      "--3-5-------2-3-----1-----------",   // frase a contratiempo
      "1-------------7-----------------",   // una nota y silencio: el género es esto
      "5---4---3-------7-------9-------", // sube a la novena
      "1-------7---8-------------------",   // la sensible que vuelve
      "5---6---7-------8-------5-------",   // sube por la 2.ª aumentada
      "3-----2-1-------7-------6-------",   // baja hasta la sensible
      "8-------6-------5---------------",   // octava, sexta, quinta
      "--5---5-6-5-----3---2---1-------",   // suspiro a contratiempo
      "1---------------2-----3---------" },   // dos notas que se abren
    { "0---1---2---1---", "0-2-1---0-2-3---", "2---1---0-------", "0--2--1---3--1--", "0-------3-------", "0-1-2-3-2-1-0---",
      "0---3---4---3---", "0-1-2-3-4-3-2-1-", "4-3-2-1-0-------", "0--3--5--3--2---", "0-----5-----3---", "2-3-4-5-4-3-2---" } },

  // ── PORTIS ──────────────────────────────────────────────────────────────────────────────────
  // El cine negro: cinta gastada, contrabajo, clavecín y un vals roto de 16.
  { "PORTIS", 20, -3, 80, 4, 2, 3, 1,
    { { "x-----5-x-------", "x-----5-x---b---" },     // de dos en dos
      { "x---x-----x-----", "x---x-----9-----" },     // tres apoyos
      { "x--x--x---x--x--", "x--x--x---b--5--" },     // caminando con el 3-3-2
      { "x-3-5-b-x-3-5-b-", "x-3-5-b-o-b-5-3-" },     // la escala del acorde (contrabajo de jazz)
      { "x---------------", "x-----------b---" },     // una nota por compás
      { "x----5--x----b--", "x----5--o----9--" },   // apoyos raros
      { "x-------5-------", "x-------b---5---" },     // raíz y quinta
      { "x--x----x-------", "x--x----9-------" },     // rebote
      { "x-----x-----x---", "x-----x-----o---" },     // en tres
      { "x---5---b---5---", "x---5---b---9---" },     // el acorde en negras
      { "x-------------5-", "x-------------b-" },     // una y la anacrusa
      { "x--5-----x--5---", "x--5-----o--b---" } },     // cojo
    { { "X-------X-x-----", "x.x.x.x.x.x.x.x.", "----X-------X---", "--x---.---x---.-", "----------------", "----------------", "----------------" },   // pesado y recto
      { "X-----x-X-------", "x..x..x..x..x..x", "----X-------X-..", "-x-.-x-.-x-.-x-.", "----------------", "------x---------", "------------x-x-" },   // vals roto
      { "X--------x------", "x-x-x-x-x-x-x-x-", "--------X-------", "---x---x---x---x", "--------x-------", "--------------x-", "----------------" },   // medio tiempo de cine
      { "X---x---X---x--x", "xxxxxxxxxxxxxxxx", "----X--x----X--x", "................", "----------------", "----------------", "-----------x---." },   // apretado
      { "X-----------x---", "x---x---x---x---", "----X-------X---", "----------------", "----------------", "--------x-------", "---------------." },   // despojado
      { "X--x----X--x----", "x.xxx.xxx.xxx.xx", "----X-.-----X-.-", "--.-x---.-x---.-", "----x-----------", "------------x---", "--------x-------" },  // arrastrando los pies
      { "X-------X-------", "x.x.x.x.x.x.x.x.", "----X-------X---", "-x-.-x-.-x-.-x-.", "----------------", "----------------", "----------------" },   // recto con clave
      { "X-----X---------", "x--x--x--x--x---", "--------X-------", "---.--.---.--.--", "--------x-------", "----------x-----", "----------------" },   // medio tiempo en 3
      { "X--x----X-------", "x-x-x-x-x-x-x-x-", "----X-------X-.-", "--.---x---.---x-", "----------------", "------x---------", "------------x---" },   // empujado
      { "X---------x-----", "x...x...x...x...", "----X-------X---", "----------------", "----x-----------", "--------------x-", "-------------.-." },   // seco
      { "X-x-----X-x-----", "xx.xxx.xxx.xxx.x", "----X-------X---", "----.-------.---", "----------------", "----------------", "----------x-x---" },   // galope
      { "X------x--X-----", "x..x..x..x..x.x.", "----X--.----X---", ".-x-.-x-.-x-.-x-", "------------x---", "------x---------", "----------------" } },   // arrastrado
    { "X-x-", "x.x.", "-xxX", "x---", "----", "----", "-x-x" },
    { "1---1-7-8-------5---4---3-------",   // la séptima mayor: el color noir
      "5-------7-8-----7-------5-------",
      "8-7-5---4-------3---2---1-------",
      "1-2-3---5-------4-3-2---1-------",
      "3-------------------------------",   // una nota en dos compases
      "5---7---8---7---9-------8-------",
      "7-8-------------5-------6-------",   // la sensible resuelve
      "1---2---3-------7---------------",   // sube y salta a la séptima
      "6-------7-------8-------5-------",   // la 2.ª aumentada, lenta
      "5-4-3-------2-------1-----------",   // cae al centro
      "8---7---6-----5-----------------",   // el descenso armónico
      "3-------4---5---6---5-----------" },   // se asoma a la sexta
    { "0-1-2-3-2-1-0---", "0---3---2---1---", "3-2-1-0---------", "0-2-4-2-0-3-1-0-", "0-------0-------", "0--1--2--3--4--3",
      "0-3-4-3-0-3-4-3-", "4---3---2---1---", "0--1--2--3--4---", "3-------0-------", "0-5-4-3-2-1-0---", "1-2-3-2-1-0-----" } },

  // ── JAZZ HOP ────────────────────────────────────────────────────────────────────────────────
  // Escobillas o boom bap con mucho swing, contrabajo caminando y vibráfono. El dialecto más suelto
  // de los seis.
  { "JAZZ HOP", 64, 0, 92, 1, 2, 1, 2,
    { { "x--5--b--9--5---", "x--5--b--9--5-3-" },     // caminando
      { "x-x---5---b---x-", "x-x---5---b---9-" },     // con el rebote
      { "x-----x-----x---", "x-----x-----b---" },     // tres apoyos
      { "x-3-5-b-9-b-5-3-", "o-b-9-b-5-3-x---" },     // sube y baja la escala
      { "x---------5-----", "x---------9-----" },     // espacio
      { "x--x--5--x--b---", "x--x--5--o--9---" },   // 3-3-2 con séptima
      { "x--3--5--b--5---", "x--3--5--b--o---" },     // caminando en 3
      { "x-3-5-b-o-b-5-3-", "x-3-5-b-9-b-5-3-" },     // arpegio de jazz
      { "x-----5---x-----", "x-----5---b-----" },     // suelto
      { "x--x--5-----b---", "x--x--5-----9---" },     // 3-3-2
      { "x-------5---3---", "x-------b---5---" },     // baja por el acorde
      { "x-x-5---b---5---", "x-x-5---o---9---" } },     // rebote y sube
    { { "X-----x-X-x-----", "x.xx.xx.xx.xx.x.", "----X---.---X---", "--x-.-x-.-x-.-x-", "----------------", "--------------x-", "----------------" },   // swing clásico
      { "X--x--x---X-----", "xxx.xxx.xxx.xxx.", "----X--.----X-.-", ".-x-.-x-.-x-.-x-", "----x-------x---", "----------------", "----------x.----" },   // boom bap
      { "X-------x---x---", "x..x..x..x..x..x", "----X-------X---", "---x--.x---x--.x", "----------------", "------x-------x-", "------------.-x-" },   // suelto
      { "X--x-x----X--x--", "x.x.x.x.x.x.x.x.", "----X-.-----X---", "-x-.-x-.-x-.-x-.", "--------x-------", "----------------", "--------x------." },   // empujado
      { "X-----------x---", "x--x--x--x--x--x", "----X-------X---", "----------------", "----------------", "----------x-----", "----------------" },   // mínimo
      { "X-x---x-X---x-x-", "xxxxxx.xxxxxx.xx", "----X--x----X--x", "...-...-...-...-", "----x-------x---", "----------------", "-------x------x-" },  // relleno constante
      { "X-----x---X-----", "x.xx.xx.xx.xx.x.", "----X-------X---", "-.x--.x--.x--.x-", "----------------", "--------------x-", "----------------" },   // swing
      { "X--x------x-----", "x..x..x..x..x..x", "----X--.----X--.", "--x---x---x---x-", "----x-------x---", "----------------", "----------------" },   // tresillo
      { "X-x-----X-------", "xxx.xxx.xxx.xxx.", "----X-------X-x-", "...-...-...-...-", "----------------", "------x---------", "--------------x-" },   // boom bap suelto
      { "X-------X-x-x---", "x-xxx-xxx-xxx-xx", "----X-.-----X---", "-x---x---x---x--", "--------x-------", "----------------", "-------------x.x" },   // quebrado
      { "X---------X--x--", "x--x--x--x--x--x", "----X-------X---", "----------------", "----------------", "----------x-----", "--------x---x---" },   // tom en el aire
      { "X--x--x---X--x--", "x.x.x.x.x.x.x.x.", "----X--x----X---", ".-.-.-.-.-.-.-.-", "----x-------x---", "----------------", "----------x.----" } },   // apretado
    { "X-x-", "x.xx", "-.xX", "x-x-", "----", "----", "x.x-" },
    { "1---3---5---6-------5---3---2---",   // la sexta mayor del dórico
      "5-6-5---3-------1---2---3-------",
      "8---7---6---5---3-------1-------",
      "1-3-5-6-8-------7-6-5---3-------",
      "6-------------------5-----------",
      "3---5---7---9-------8---6-------",
      "1-3-5-7-8-------7-6-5---3-------",   // corre por el acorde
      "5---3---2-1-----7---------------",   // cae y salta
      "3-4-5---6---7---8-------9-------",   // escala hasta la novena
      "--8-7---6---5-------3---4---3---",   // baja a contratiempo
      "1-------5-6-7-8-------7---------",   // el remate
      "9-------8---7-------6---5-------" },   // desde la novena
    { "0-1-2-3-4-3-2-1-", "0--2--1--3--2--1", "0-2-1-3-2-4-3-5-", "0---2---4---2---", "0-----3-----2---", "0-1-2-3-4-5-4-3-",
      "0-1-2-3-4-5-4-3-", "0--2--4--1--3--5", "5-4-3-2-1-0-----", "0-2-4-2-1-3-5-3-", "0---1---3---5---", "3-2-1-0-3-2-1-0-" } },

  // ── DUB ─────────────────────────────────────────────────────────────────────────────────────
  // Bombo 808 larguísimo, bajo redondo y oscuro, guitarra apagada en el contratiempo y todo con
  // mucho eco. Lo que no suena manda.
  { "DUB", 108, -5, 72, 2, 1, 4, 1,
    { { "x---------------", "x-----------b---" },     // una nota, y que suene
      { "x-------b-------", "x-------b---5---" },     // dos
      { "x-----x-----x---", "x-----x-----b---" },     // tres apoyos
      { "x---x-------x---", "x---x-------9---" },     // adelantado
      { "x----------5----", "x----------b----" },
      { "x--x--------b---", "x--x--------9---" },
      { "x-------o-------", "x-------o---b---" },     // raíz y octava
      { "x-----------5---", "x-----------9---" },     // tarde
      { "x--x------------", "x--x--------b---" },     // doble y silencio
      { "x-------5-------", "x-------5---o---" },     // la quinta en el 3
      { "x---------x-----", "x---------b-----" },     // el contratiempo
      { "x-----x---------", "x-----x-----9---" } },     // dos y espacio
    { { "X---------------", "----x-------x---", "--------X-------", "----------------", "----------------", "----x-------x---", "----------------" },   // one drop
      { "X-------X-------", "----x-------x---", "--------X-------", "--------.-------", "----------------", "------------x---", "----------------" },   // dos bombos
      { "X-----------x---", "x---x---x---x---", "--------X-------", "----.-------.---", "--------x-------", "----------------", "----------------" },   // rockers
      { "X-------x-------", "--x---x---x---x-", "----X-------X---", "----------------", "----------------", "--x-------x-----", "------------x---" },   // steppers
      { "X---------------", "----x-------x---", "--------X-----x-", "----------------", "----------------", "----------------", "-----------x----" },   // vacío
      { "X--x------X-----", "x-x-x-x-x-x-x-x-", "--------X-------", "--x---.---x---.-", "--------x-------", "------------x---", "----------------" },  // con pulso
      { "X---------------", "--x---x---x---x-", "--------X-------", "----------------", "----------------", "----x-----------", "--------------x-" },   // one drop seco
      { "X-------x-------", "----x-------x---", "--------X-------", "------.-------.-", "--------x-------", "----------------", "----------------" },   // con clap
      { "X---X---X---X---", "--x---x---x---x-", "--------X-------", "----------------", "----------------", "------------x---", "----------------" },   // steppers
      { "X-----x---------", "x-x-x-x-x-x-x-x-", "--------X-------", "--.-------.-----", "----------------", "----------------", "------------x-x-" },   // con pulso
      { "X---------x-----", "----x-------x---", "--------X-----.-", "----.-------.---", "--------x-------", "--------------x-", "----------------" },   // rockers roto
      { "X-------------x-", "x---x---x---x---", "--------X-------", "--x-------x-----", "----------------", "------x---------", "----------x-----" } },   // anticipado
    { "X---", "----", "--xX", "----", "----", "---x", "x---" },
    { "1---------------5---------------",   // dos notas en dos compases
      "5-------7-------1---------------",
      "1-------3-------5-------3-------",
      "8---------------7---------------",
      "3-----------5-------------------",
      "1---5---8-------5---------------",
      "7---------------8---------------",   // sensible y tónica
      "5---------------6---------------",   // quinta y sexta
      "3-------2-------1---------------",   // tres notas que bajan
      "8-----------------------7-------",   // octava larga
      "1-----5---------------------6---",   // la sexta al final
      "2---------------1---------------" },   // la segunda y vuelve
    { "0-------2-------", "0---------------", "0---2---1-------", "3-------0-------", "0-----2-----1---", "0---1---2---3---",
      "0-------3-------", "4-------2-------", "0---------5-----", "2-------1-------", "0-----3-----5---", "3---------------" } },

  // ── NOCTURNO ────────────────────────────────────────────────────────────────────────────────
  // Medio tiempo pesado, bombo enorme, piano con el brillo comido. El dialecto más oscuro: casi no
  // se mueve y da miedo.
  { "NOCTURNO", 224, 1, 76, 5, 0, 2, 0,
    { { "x---------------", "x-------b-------" },
      { "x-------x-------", "x-------x---b---" },
      { "x--x------------", "x--x--------5---" },
      { "x-------5-------", "x-------5---b---" },
      { "x-x-------------", "x-x---------9---" },
      { "x-----x---x-----", "x-----x---b-----" },
      { "x---------------", "x-----------5---" },     // una nota
      { "x-------x---x---", "x-------x---b---" },     // tres apoyos
      { "x--x--x---------", "x--x--x-----9---" },     // 3-3 y silencio
      { "x-------------x-", "x-------------5-" },     // anticipa
      { "x---5-----------", "x---5-------b---" },     // quinta temprana
      { "x-x---x---------", "x-x---x---o-----" } },     // golpeado
    { { "X---------------", "x---x---x---x---", "--------X-------", "----------------", "----------------", "----------------", "----------------" },   // medio tiempo desnudo
      { "X-------x-------", "x..x..x..x..x..x", "--------X-------", "------.-------.-", "----------------", "--------------x-", "----------------" },   // medio tiempo con aire
      { "X--x------------", "x-x-x-x-x-x-x-x-", "--------X-------", "----.---x---.---", "--------x-------", "----------------", "-----------x----" },   // bombo doble
      { "X-------X-x-----", "x---x---x---x---", "----X-------X---", "--x-------x-----", "----------------", "------------x---", "----------------" },   // a tiempo entero
      { "X---------------", "----------------", "--------X-------", "----------------", "----------------", "x---------------", "------------x---" },   // el plato y nada
      { "X-x-----X-------", "x.x.x.x.x.x.x.x.", "--------X-----x-", "--.---x---.---x-", "--------x-------", "----------------", "----------x-----" },  // arrastrado y pesado
      { "X---------------", "x-x-x-x-x-x-x-x-", "--------X-------", "--------------.-", "----------------", "----------------", "------------x---" },   // desnudo
      { "X--x----X-------", "x---x---x---x---", "--------X-------", "----.-------.---", "----------------", "--------------x-", "----------------" },   // pesado
      { "X-------------x-", "x..x..x..x..x..x", "--------X-------", "--.---.---.---.-", "--------x-------", "----------------", "----------------" },   // en 3
      { "X-----X---------", "x-xxx-xxx-xxx-xx", "--------X-------", "----------x-----", "----------------", "----------------", "----------x-x---" },   // nervioso
      { "X---------X-----", "----x-------x---", "--------X-------", "----------------", "--------x-------", "x---------------", "----------------" },   // plato y nada
      { "X-x-x-----------", "x.x.x.x.x.x.x.x.", "--------X-----x-", "---x-------x----", "----------------", "------x---------", "--------------x-" } },   // tres bombos
    { "X-X-", "----", "--xX", "----", "----", "----", "xx--" },
    { "1---------------2---------------",   // la segunda menor, sola
      "1-------2-------1---------------",
      "5-------4-------3-------2-------",
      "8-------------------------------",
      "1-2-1-----------8---------------",
      "3-------2-------1-------7-------",
      "2-------1-----------------------",   // la segunda que cae
      "1---------------7-------8-------",   // la sensible
      "6-------5-------6-------5-------",   // sexta y quinta
      "5-----6---------5---------------",   // se asoma y vuelve
      "3---2---1-------7---------------",   // baja al fondo
      "1-2-3-----------2-1-------------" },   // la 2.ª aumentada sola
    { "0-------1-------", "0---------------", "0-1-0-----------", "2-------1-------", "0---------------", "0---1---2---1---",
      "0-------1-------", "0---1---0-------", "4-----3-----2---", "0-1-------------", "2-------0-------", "0---0---1-------" } },

  // ── CELESTE ─────────────────────────────────────────────────────────────────────────────────
  // Escobillas, bajo redondo, vibráfono y coro: el dialecto que flota.
  { "CELESTE", 150, 3, 84, 3, 1, 1, 2,
    { { "x---------------", "x-----------9---" },
      { "x-------5-------", "x-------5---9---" },
      { "x-----x-----5---", "x-----x-----9---" },
      { "x---------5-----", "x---------b-----" },
      { "x-------------5-", "x-------------9-" },
      { "x--5--x--5--9---", "x--5--x--9--5---" },
      { "x---------------", "x-----------o---" },     // una nota
      { "x-------9-------", "x-------9---5---" },     // la novena
      { "x-----5---------", "x-----5-----b---" },     // quinta suelta
      { "x---x-----------", "x---x-------9---" },     // dos
      { "x-----------x---", "x-----------5---" },     // respira
      { "x--5------x-----", "x--5------9-----" } },     // flotando
    { { "X-----------x---", "x..x..x..x..x..x", "--------X-------", "----.-------.---", "----------------", "------x---------", "----------------" },   // flotando
      { "X-------x-------", "x-x-x-x-x-x-x-x-", "----X-------X---", "--.---.---.---.-", "----------------", "----------------", "------------.-x-" },   // con pulso
      { "X---------------", "----x-------x---", "--------X-------", "----------------", "----------------", "x-------x-------", "----------------" },   // casi nada
      { "X--x------X-----", "x.xx.xx.xx.xx.x.", "----X-------X---", "--x-.-x-.-x-.-x-", "----x-------x---", "----------------", "----------x.----" },   // con swing
      { "X-------------x-", "x---x---x---x---", "--------X-------", "------.-------.-", "----------------", "--------------x-", "----------------" },   // respirando
      { "X-x-------X-x---", "x.x.x.x.x.x.x.x.", "----X-.-----X---", "---x---x---x---x", "--------x-------", "------x-------x-", "--------------x-" },  // movido
      { "X---------------", "x..x..x..x..x..x", "--------X-------", "----------------", "----------------", "----------x-----", "----------------" },   // flotando en 3
      { "X-------X-------", "x-x-x-x-x-x-x-x-", "----X-------X---", "--.---.---.---.-", "----------------", "----------------", "--------------x-" },   // con pulso
      { "X-----x---------", "----x-------x---", "--------X-------", "------.-------.-", "----------------", "x---------------", "----------------" },   // casi nada
      { "X--x------X-----", "x.x.x.x.x.x.x.x.", "----X-------X---", "-x---x---x---x--", "--------x-------", "----------------", "------------x.x-" },   // andando
      { "X-------------x-", "x--x--x--x--x--x", "--------X-------", "---.---.---.---.", "----------------", "------------x---", "----------------" },   // respirando
      { "X-x-------X-----", "xxx.xxx.xxx.xxx.", "----X-.-----X-.-", "--x-------x-----", "----x-------x---", "----------------", "----------x-----" } },   // movido
    { "X---", "x.x.", "-x-X", "----", "----", "---x", "-x-x" },
    { "1-------5-------9-------5-------",   // la novena, flotando
      "5---9---8-------7-------5-------",
      "1---2---3-------5-------4-------",   // el 4 en lidio es la cuarta aumentada
      "9-------------------8-----------",
      "3-------5-------9---------------",
      "5-4-3---5-------9-------8-------",
      "8-------9-------7---------------",   // arriba, flotando
      "5---7---9-------8-------7-------",   // sube a la novena
      "3-------5-------7-------9-------",   // el acorde abierto
      "9---------------8-------6-------",   // baja desde la novena
      "1---5---8---9-------------------",   // la escalera
      "7-------6-------5-------3-------" },   // desciende
    { "0---2---5---2---", "0-2-5-2-0-2-5-2-", "0-------5-------", "5---3---2---0---", "0-----2-----5---", "0-2-4-5-4-2-0---",
      "0-2-4-5-4-2-0-2-", "5---4---3---2---", "0-4-0-4-2-5-2-5-", "1-3-5-3-1-3-5-3-", "0-------4-------", "2-4-5-----4-2---" } },
};

// Eventos (para los LEDs, la matriz y las proyecciones)
#define EV_MEL      6
#define EV_BOMBO    7
#define EV_BAJO     8
#define EV_ARP      9
#define EV_PAD     10
#define NUM_EV     11

// Paneles
#define P_GENERAL   0
#define P_DRUM      1
#define P_BAJO      2
#define P_MEL       3
#define P_PAD       4
#define NUM_PANELES 5
const char *NOMBRE_PANEL[NUM_PANELES] = { "GENERAL", "BATERIA", "BAJO", "ARMONIA", "PAD" };
const char *REGISTRO_PAD[3] = { "NORMAL", "+1 OCT", "-1 OCT" };   // BTN3 del PAD

// ==============================================================================================================================================
// PEDIDOS ENTRE NÚCLEOS
// ==============================================================================================================================================
// control -> audio
volatile Ctl      reqC;                            // se copia entera (memcpy) al borde de cada buffer
volatile uint32_t reqFill = 0;
// Pots de cada panel: la tarea de control los escribe ya suavizados; el audio los lee.
//   [P_GENERAL] vol · tempo · swing · pasa-bajos      [P_DRUM] vol · filtro · largo · densidad
//   [P_BAJO]    vol · filtro · ataque · caída         [P_MEL]  vol · filtro · ataque · caída
//   [P_PAD]     vol · filtro · ataque-suelta · eco
volatile float    pPot[NUM_PANELES][4] = {
  { 0.70f, 0.538f, 0.45f, 1.00f },                 // 0.538 del rango 58-110 = 86 BPM
  { 0.88f, 1.00f,  0.50f, 0.55f },
  { 0.90f, 0.30f,  0.10f, 0.45f },
  { 0.85f, 0.50f,  0.05f, 0.40f },
  { 0.70f, 0.35f,  0.45f, 0.40f },
};

// audio -> LEDs, OLED y USB
volatile uint32_t golpes[NUM_EV];
volatile float    velGolpe[NUM_EV];
volatile uint32_t pasoTotalPub = 0, contadorPasos = 0;
volatile float    envBajoPub = 0.0f, bpmPub = 86.0f, fasePub = 0.0f;
volatile float    nivelPub[5];                     // pico por bloque: batería · bajo · armonía · pad · master
volatile int      notaBajoPub = 0, notaMelPub = 64, notaArpPub = 64, acordePub = 0, tonicaPub = 57;
volatile bool     tocandoPub = false, breakPub = false, fillPub = false;
volatile uint32_t muestraPub = 0;
volatile uint32_t cargaAltaHasta = 0;
volatile uint32_t medMax = 0, medSuma = 0, medN = 0, medTecho = 0;   // MEDIR
volatile float    medLimMin = 1.0f, medPico = 0.0f;
volatile int      panelPub = 0;                    // lo escribe el control, lo leen los LEDs
// control -> monitor de la OLED (las dos tareas viven en el core 0)
volatile uint8_t  tomadoPub = 0x0F;                // bit i = el pot i ya manda (si no, está suelto)
volatile float    potFisicoPub[4];                 // dónde está cada perilla, mande o no
volatile uint32_t potMovidoMs[4];                  // cuándo se movió por última vez (para marcarlo)
char              avisoTitulo[24] = "", avisoValor[24] = "";
volatile uint32_t avisoHasta = 0;                  // el aviso grande de un botón se ve hasta acá

// ── GRABACIÓN: cada Play del General abre una toma en la microSD y su Stop la cierra ──
// El audio copia a un COLCHÓN el mismo bloque que manda al DAC y marca dónde empieza y dónde termina
// cada toma; la tarea grabadora (core 0) vacía el colchón a la tarjeta. El audio nunca espera a la
// tarjeta: una microSD se puede quedar 100-300 ms pensando, y eso con el colchón no se oye.
#define SD_SCK    14                               // el mismo cableado que grabador_campo y oled_video_techno
#define SD_MOSI   15
#define SD_MISO   16
#define SD_CS     17
const uint32_t SD_FREQ_HZ = 20000000;
#define BYTES_BLOQUE   (BUFFER_SAMPLES * 2 * 2)     // estéreo 16 bit = 512 bytes por bloque
#define RING_SEG_PSRAM   8.0f                       // colchón con PSRAM: 8 s (1.4 MB)
#define RING_SEG_INTERNA 0.4f                       // sin PSRAM: 0.4 s (70 KB de RAM interna)
#define BLOQUES_POR_ESCRITURA 32                    // 16 KB por escritura
#define BLOQUES_COLA 25                             // tras el Stop se graban 72 ms más: lo que tarda el
                                                    // fundido de salida (tau 8 ms) en llegar a −78 dB
const uint32_t LIMITE_DATOS = 3900000000u;          // FAT32 no pasa de 4 GB por archivo (~6 h)
const unsigned long CABECERA_CADA_MS = 5000;        // el WAV se puede abrir aunque se corte la luz
struct Marca { uint32_t idx; bool inicio; };        // en qué bloque del colchón empieza / termina una toma
#define NUM_MARCAS 8
volatile Marca    marcas[NUM_MARCAS];
volatile uint32_t marcasEsc = 0, marcasLeidas = 0;  // marcas: las escribe el audio, las lee la grabadora
volatile uint32_t bloquesEsc = 0, bloquesLeidos = 0, bloquesPerdidos = 0;
volatile bool     sdLista = false;                  // la tarjeta respondió la última vez que se probó
volatile bool     grabandoPub = false, perdidasPub = false;   // para el monitor
uint8_t  *ring = nullptr;
uint32_t  ringBloques = 0;

// ==============================================================================================================================================
// DSP — utilidades (sin libm por muestra)
// ==============================================================================================================================================
#define TABLA 1024
float SENO[TABLA + 1];
static inline float seno(float fase) { float x = fase * TABLA; int i = (int)x; float f = x - i; return SENO[i] + (SENO[i + 1] - SENO[i]) * f; }
uint32_t semillaRuido = 22222;
static inline float ruido(uint32_t &s) { s = s * 1664525u + 1013904223u; return (int32_t)s * (1.0f / 2147483648.0f); }
static inline float aleatorio01(uint32_t &s) { s = s * 1664525u + 1013904223u; return (s >> 8) * (1.0f / 16777216.0f); }
static inline float tanRapido(float x) { float x2 = x * x; return x * (15.0f - x2) / (15.0f - 6.0f * x2); }   // Padé, < 0.2 % hasta 13 kHz
static inline float exp2Rapido(float x) {          // 2^x para x >= 0, error < 0.2 %
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
// ANTI-SUBNORMALES. Un filtro al que le entra silencio decae hacia cero, y a mitad de camino sus
// estados caen en el rango subnormal: ahí cada multiplicación cuesta del orden de cien veces más
// (en x86, y también en el WASM del simulador, que no pone flush-to-zero). Medido en este firmware:
// el vinilo costaba 179 us por bloque en vez de 5, y el vocoder 3137 en vez de 16 — el motor corría
// a la mitad del tiempo real y se oía arrastrado. Un `1e-18f` constante a la entrada deja los
// estados en 1e-18, que es un float NORMAL (el subnormal empieza en 1.2e-38), y a la salida son
// −360 dB: no se oye. Va dentro de las primitivas para que ningún filtro nuevo se quede sin ella.
#define DITHER 1e-18f
static inline float pasoBQ(BQ &q, float x) { x += DITHER; float y = q.b0 * x + q.z1; q.z1 = q.b1 * x - q.a1 * y + q.z2; q.z2 = q.b2 * x - q.a2 * y; return y; }
// Y para los suavizadores de un polo, que no tienen entrada donde inyectar: se van a cero exacto
// (el cero NO es subnormal y la aritmética con cero va a velocidad normal).
static inline float sinSub(float x) { return (x > -1e-20f && x < 1e-20f) ? 0.0f : x; }
static inline void coefFiltro(Filtro &s, float fc, float q) {
  if (fc > 12500.0f) fc = 12500.0f; if (fc < 20.0f) fc = 20.0f;
  float g = tanRapido(PI_F * fc / SR); s.k = 1.0f / q;
  s.a1 = 1.0f / (1.0f + g * (g + s.k)); s.a2 = g * s.a1; s.a3 = g * s.a2;
}
static inline void pasoFiltro(Filtro &s, float x, float &lp, float &bp) {
  x += DITHER;                                     // ver DITHER: sin esto, un SVF en silencio se queda en subnormales
  float v3 = x - s.ic2, v1 = s.a1 * s.ic1 + s.a2 * v3, v2 = s.ic2 + s.a2 * s.ic1 + s.a3 * v3;
  s.ic1 = 2.0f * v1 - s.ic1; s.ic2 = 2.0f * v2 - s.ic2;
  lp = v2; bp = v1 * s.k;                          // pasa-banda con ganancia 1 en el centro
}
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
// Igual que bqLPF/bqHPF pero SIN borrar el estado: para barrer un filtro con un pot sin que suene un clic
void bqLPFvivo(BQ &c, float fc, float Q) { float z1 = c.z1, z2 = c.z2; bqLPF(c, fc, Q); c.z1 = z1; c.z2 = z2; }
void bqHPFvivo(BQ &c, float fc, float Q) { float z1 = c.z1, z2 = c.z2; bqHPF(c, fc, Q); c.z1 = z1; c.z2 = z2; }
void bqBPFvivo(BQ &c, float fc, float Q) { float z1 = c.z1, z2 = c.z2; bqBPF(c, fc, Q); c.z1 = z1; c.z2 = z2; }
static inline float techoSuave(float x) {
  float a = fabsf(x); if (a <= 0.95f) return x;
  float y = 0.95f + 0.05f * tanhf((a - 0.95f) / 0.05f); return x < 0 ? -y : y;
}
static inline float hz(float midi) { return 440.0f * exp2f((midi - 69.0f) / 12.0f); }
static inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
// Pot de volumen -> ganancia: cuadrática, 1.3 arriba del todo
static inline float ganVol(float v) { return 1.3f * v * v; }
// Corte de un pasa-bajos de bus: 150 Hz … 19 kHz; arriba del 92 % se abre del todo
static inline float corteBus(float p) { return 150.0f * exp2Rapido(7.0f * clamp01(p)); }
static inline float abiertoBus(float p) { return clamp01((p - 0.90f) / 0.08f); }

// Generador de composición y de sorteos — separado del ruido de audio
uint32_t rngComp = 0x9E3779B9u;
static inline float frnd() { rngComp ^= rngComp << 13; rngComp ^= rngComp >> 17; rngComp ^= rngComp << 5; return (rngComp >> 8) * (1.0f / 16777216.0f); }

// ==============================================================================================================================================
// ESTADO DE LA SÍNTESIS (lo toca sólo la tarea de audio)
// ==============================================================================================================================================
Ctl C;                                              // la copia de los controles de este bloque
int estilo = 0, kit = 0, tBajo = 0, tMel = 0, tPad = 0;
VozBombo bombos[2]; int sigBombo = 0;              // dos instancias: la que sonaba se calla en 3 ms
struct { float f1, f2, fs, fsn, inc1, incObj, env, envF, nz, nzC; bool atk, atkF; Filtro s; BQ nzBp; } bajo;
float dBajo, dBajoF, dRapido3, dRapido, dSc, kGlideBajo, kGlide16;
float incAtkBajo = 1.0f / 88.0f, incAtkMel = 1.0f / 132.0f, decMel = 0.30f, largoDrum = 1.0f;
float filtMel = 1.0f;
uint32_t semRuidoL = 12345u, semRuidoR = 67891u, semVin = 0xBEEF1234u;

// Batería
VozInd ind[NUM_IND][2]; int sigInd[NUM_IND];       // un "parche" por pista con fast-kill al retriggear
float scObj = 0, sc = 0;
float prio[NUM_IND][16];                           // prioridad de cada golpe del patrón: suena si prio < densidad
#define MAX_PEND 12
Pendiente pend[MAX_PEND];                          // el ARRASTRE: golpes que entran unos ms tarde

// Secuenciador
bool     aTocando = false, aBreak = false;
float    fundido = 0.0f;
float    sps = 0.0f, faltan = 0.0f;                // muestras por semicorchea · hasta el próximo paso
float    bpm = 86.0f, swingS = 0.55f;
uint32_t pasoTotal = 0xFFFFFFFFu;                  // semicorcheas desde el Play (el reloj maestro, nunca se corre)
uint32_t muestraGlobal = 0;
uint32_t vistoFill = 0; bool fillPend = false, crashPend = false; uint32_t fillCompas = 0;

// Armonía
#define VOCES_ARP   2
VozPluck arps[VOCES_ARP];
struct { bool viva, atk; float f1, f2, f3, inc1, inv1, inc2, inv2, amp, fenv, base, eC, lfo; Filtro s; } mv;   // monofónico
#define CHORUS_N 1024
float    chorusBuf[CHORUS_N]; int chorusIdx = 0; float chorusLfo = 0.0f;
#define ECO_MAX 32768                              // 0.74 s: corchea con puntillo hasta 60 BPM
int16_t  ecoL[ECO_MAX], ecoR[ECO_MAX];
int      ecoIdx = 0, ecoN = 15000, ecoN2 = 15000; float ecoXf = 1.0f, ecoLpL = 0, ecoLpR = 0, ecoHpL = 0, ecoHpR = 0;
float    melBuf[BUFFER_SAMPLES], ecoBufL[BUFFER_SAMPLES], ecoBufR[BUFFER_SAMPLES], envioEco[BUFFER_SAMPLES];
float    dMelFenv, dArpFenv;
int      modo = 0, transp = 2, tonicaBajo = 35, ladoArp = 0;
int      estiloPrio = -1, patPrio = -1;

// Pad
#define VOCES_PAD 4
VozPad   pads[VOCES_PAD];
int      acordePad = -99, octPadVisto = -99, estiloPadVisto = -1;
float    padAtk = 0.001f, padRel = 0.0005f, padFiltS = 0.35f, padEcoS = 0.40f, kPadCorte = 0.003f;
float    padL[BUFFER_SAMPLES], padR[BUFFER_SAMPLES];
float    padCompEnv = 0.0f, padCompG = 1.0f, kPadCompAtk, kPadCompRel;
const float PAD_COMP_NIVEL = 0.16f, PAD_COMP_RODILLA = 0.35f, PAD_COMP_ATK = 0.010f, PAD_COMP_REL = 0.240f;

// Vinilo
struct {
  float vuelta;                                    // la vuelta lenta que mueve el wow (0.556 Hz)
  float wow, desgaste;                             // fijos: ver CINTA_WOW / CINTA_DESGASTE
} vin;
#define WOW_N 1024
float wowL[WOW_N], wowR[WOW_N]; int wowIdx = 0;
float wowFase = 0.0f, flutFase = 0.0f, derivaFase = 0.0f;
BQ desgLpL, desgLpR, desgHpL, desgHpR;
float desgVisto = -1.0f;

// Master y buses
float bufKick[BUFFER_SAMPLES], bufBajo[BUFFER_SAMPLES], bufIndL[BUFFER_SAMPLES], bufIndR[BUFFER_SAMPLES];
float bufMelL[BUFFER_SAMPLES], bufMelR[BUFFER_SAMPLES], bufSc[BUFFER_SAMPLES];
float corteBajoS = 0.30f, volS = 0.5f, densS = 0.55f, lpfS = 1.0f, filtDrumS = 1.0f;
float gDrumS = 1, gKickS = 1, gBajoS = 1, gMelS = 1, gPadS = 1;
Filtro fDrumL, fDrumR, fMasL, fMasR;
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
  dRapido3 = decaimiento(0.003f); dRapido = decaimiento(0.0015f); dSc = decaimiento(0.14f);
  dBajo = decaimiento(0.30f); dBajoF = decaimiento(0.30f * 0.55f);
  dArpFenv = decaimiento(0.07f); dMelFenv = decaimiento(0.25f);
  kGlideBajo = 1.0f - decaimiento(BAJO_GLIDE); kGlide16 = 1.0f - powf(1.0f - kGlideBajo, 16.0f);
  kPadCompAtk = 1.0f - decaimiento(PAD_COMP_ATK); kPadCompRel = 1.0f - decaimiento(PAD_COMP_REL);
  kPadCorte = 1.0f - decaimiento(0.012f);   // el corte de una voz del pad que cambia de nota
  for (int i = 0; i < LIM_LOOK; i++) { limDlyL[i] = 0; limDlyR[i] = 0; }
  limRel = expf(-1.0f / (SR * 0.10f));
  bqLPF(techoL, 13000.0f, 0.7071f); bqLPF(techoR, 13000.0f, 0.7071f);   // DESPUÉS del limitador
  coefFiltro(fDrumL, 12000.0f, 0.75f); fDrumR = fDrumL; fMasL = fDrumL; fMasR = fDrumL;
  bqLPF(desgLpL, 16000.0f, 0.707f); desgLpR = desgLpL;
  bqHPF(desgHpL,     25.0f, 0.707f); desgHpR = desgHpL;
  setBPM(86.0f);
}

// ─── Armonía ───────────────────────────────────────────────
int notaGrado(int tonica, int g) {                  // grado de la escala (puede ser negativo) -> nota MIDI
  int o = (g >= 0) ? g / 7 : -((-g + 6) / 7);
  return tonica + 12 * o + ESCALAS[modo][g - 7 * o];
}
inline int raizCercana(int g) { return g >= 4 ? g - 7 : g; }   // los acordes altos se toman desde abajo: las voces se mueven poco
int notaAcorde(int tonica, int raiz, int off, bool cercana) {
  int g = cercana ? raizCercana(raiz) : raiz;
  return notaGrado(tonica, g + off);
}
int acordeDe(uint32_t compas) {                     // grado del acorde que suena en ese compás
  const Progresion &Pr = PROGS[C.prog];
  int total = 0;
  for (int k = 0; k < NUM_SEG && Pr.seg[k].compases; k++) total += Pr.seg[k].compases;
  if (total <= 0) return 0;
  int pos = (int)(compas % (uint32_t)total);
  for (int k = 0; k < NUM_SEG && Pr.seg[k].compases; k++) {
    if (pos < Pr.seg[k].compases) return Pr.seg[k].grado;
    pos -= Pr.seg[k].compases;
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
  const TimbreBajo &T = TBAJO[tBajo];
  float inc = hz((float)nota) / SR;
  bajo.incObj = inc;
  if (bajo.env < 1e-3f) bajo.inc1 = inc;           // desde el silencio no hay portamento
  bajo.atk = true; bajo.atkF = true;               // sube desde donde estaba: sin clic, sin reiniciar la fase
  if (T.nzAmt > 0.0f) { bajo.nz = 1.0f; bajo.nzC = decaimiento(T.nzTau); bqBPFvivo(bajo.nzBp, T.nzF, 1.2f); }
  notaBajoPub = nota;
  publicarGolpe(EV_BAJO, 1.0f);
}

void golpeMel(int nota, float vel) {               // una sola voz; la fase no se reinicia, la amplitud sube desde donde estaba
  const TimbreMel &T = TMEL[tMel];
  float f = hz((float)nota);
  if (!mv.viva) { mv.f1 = 0.13f; mv.f2 = 0.61f; mv.f3 = 0.29f; mv.amp = 0.0f; mv.s.ic1 = mv.s.ic2 = 0.0f; }
  mv.inc1 = f / SR; mv.inv1 = 1.0f / mv.inc1;
  mv.inc2 = mv.inc1 * 1.00405f; mv.inv2 = 1.0f / mv.inc2;   // +7 cents (las dos sierras)
  mv.base = f * T.corte * filtMel; mv.fenv = 1.0f; mv.atk = true; mv.viva = true;
  mv.eC = decaimiento(decMel * (0.8f + 0.4f * vel));
  dMelFenv = decaimiento(T.fenvTau);
  notaMelPub = nota;
  publicarGolpe(EV_MEL, vel);
}

void golpeArp(int nota) {                           // notas cortas del acorde, una voz por altura, alternando lados
  ladoArp ^= 1;
  float pan = ladoArp ? 0.26f : 0.74f;
  int k = -1;
  for (int i = 0; i < VOCES_ARP; i++) if (arps[i].viva && arps[i].nota == nota) k = i;
  if (k < 0) for (int i = 0; i < VOCES_ARP; i++) if (!arps[i].viva) { k = i; break; }
  if (k < 0) { float menor = 9.0f; for (int i = 0; i < VOCES_ARP; i++) if (arps[i].amp < menor) { menor = arps[i].amp; k = i; } }
  VozPluck &v = arps[k];
  float f = hz((float)nota);
  if (!v.viva) { v.f1 = 0.13f; v.amp = 0.0f; v.s.ic1 = v.s.ic2 = 0.0f; }
  v.inc1 = f / SR; v.inv1 = 1.0f / v.inc1;
  v.nota = nota; v.base = f * 1.5f * filtMel; v.oct = 2.0f; v.q = 0.9f; v.fenv = 1.0f; v.atk = true; v.viva = true;
  float dec = 0.45f * decMel; if (dec < 0.04f) dec = 0.04f; if (dec > 0.45f) dec = 0.45f;
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
  float pan = clamp01(g.pan + g.panRnd * (2.0f * frnd() - 1.0f));
  v.gL = sqrtf(1.0f - pan); v.gR = sqrtf(pan);
  publicarGolpe(t, vel);
}

// EL ARRASTRE. Lo que hace que esto suene a trip hop y no a drum machine: la caja no entra en el
// tiempo, entra 15-30 ms después. En la rejilla de semicorcheas no cabe (a 86 BPM una semicorchea
// son 174 ms), así que el golpe se deja PENDIENTE y el bucle de bloques lo dispara en la muestra
// exacta. Sin pendiente libre, suena al tiempo: más vale a tiempo que no sonar.
void golpeIndTarde(int t, float vel, bool acento, float largoExtra, float ms) {
  if (ms <= 0.05f) { golpeInd(t, vel, acento, largoExtra); return; }
  for (int i = 0; i < MAX_PEND; i++) if (!pend[i].viva) {
    pend[i].viva = true; pend[i].pista = t; pend[i].vel = vel; pend[i].acento = acento;
    pend[i].largo = largoExtra; pend[i].faltan = ms * 0.001f * SR;
    return;
  }
  golpeInd(t, vel, acento, largoExtra);
}

// ─── El PAD toca el acorde ─────────────────────────────────
// Cuatro voces: fundamental · 3.ª · 5.ª · 7.ª del acorde que manda la progresión. Una voz cuya nota
// NO cambia entre dos acordes se queda sonando (es lo que hace que un pad suene continuo); la que
// cambia se apaga en ~25 ms, se reafina en el silencio y vuelve a entrar con el ataque. Reafinar
// una voz sonando sería un portamento, y un pad que se desliza entre acordes suena a sirena.
void acomodarPad(int raiz) {
  const int t = TONICA_PAD + transp + (C.octPad == 1 ? 12 : (C.octPad == 2 ? -12 : 0));
  const float PANES[VOCES_PAD] = { 0.20f, 0.42f, 0.58f, 0.80f };
  for (int k = 0; k < VOCES_PAD; k++) {
    VozPad &v = pads[k];
    int n = notaAcorde(t, raiz, TONOS_PAD[k], true);
    v.gL = sqrtf(1.0f - PANES[k]); v.gR = sqrtf(PANES[k]);
    v.notaObj = n;
    if (!v.viva) { v.nota = n; v.f1 = 0.11f * (k + 1); v.f2 = 0.53f; v.fs = 0.17f; v.amp = 0.0f; v.s.ic1 = v.s.ic2 = 0.0f; v.viva = true; v.cambiando = false; }
    else if (n != v.nota) v.cambiando = true;      // se apaga, se reafina y vuelve
  }
  acordePad = raiz;
}

// ─── Un paso del secuenciador (semicorchea) ─────────────────
int notaBajoDe(char c, int raiz) {
  int off = 0;
  switch (c) { case 'o': off = 7; break; case '5': off = 4; break; case 'b': off = 6; break; case '3': off = 2; break; case '9': off = 8; break; }
  const int t = tonicaBajo;
  int n = notaAcorde(t, raiz, off, false);
  if (c == 'b') {                                  // séptima: sólo si es menor; si no, la quinta
    int f = notaAcorde(t, raiz, 0, false);
    if (((n - f) % 12 + 12) % 12 != 10) n = notaAcorde(t, raiz, 4, false);
  }
  return n;
}

void avanzarPaso() {
  pasoTotal++;
  const uint32_t pr = pasoTotal;                   // el reloj maestro
  const int s = pr & 15;
  const uint32_t compas = pr >> 4;

  const Estilo &E = ESTILOS[estilo];
  const int raiz = acordeDe(compas);
  acordePub = raiz;
  if (s == 0 && (raiz != acordePad || C.octPad != octPadVisto)) { octPadVisto = C.octPad; acomodarPad(raiz); }

  // ── ARMONÍA ──
  if (C.melOn) {
    char k = E.mel[C.patMel][(compas & 1) * 16 + s];
    int tk = TONICA_MEL + transp; while (tk < MEL_DESDE) tk += 12;
    if (k >= '1' && k <= '9') golpeMel(notaAcorde(tk, raiz, k - '1', true), (s & 3) == 0 ? 1.0f : 0.8f);
    if (C.arp) {
      char a = E.arp[C.patMel][s];
      int ta = TONICA_ARP + transp;
      if (a >= '0' && a <= '5') golpeArp(notaAcorde(ta, raiz, TONOS_ARP[a - '0'], true));
    }
  }

  // ── BAJO ── (el break lo calla, como al bombo)
  if (C.bajoOn && !aBreak) {
    char c = E.bajo[C.patBajo][(compas & 3) == 3 ? 1 : 0][s];
    if (c != '-') golpeBajo(notaBajoDe(c, raiz));
  }

  // ── BATERÍA ──
  // FILL: los pasos 13-16 del compás pedido, en lugar del patrón
  bool enFill = fillPend && (pr >> 4) == fillCompas && s >= 12;
  if (fillPend && (pr >> 4) > fillCompas) fillPend = false;
  fillPub = enFill;
  if (s == 15 && enFill) { fillPend = false; crashPend = true; }
  if (C.drumOn) {
    if (estiloPrio != estilo || patPrio != C.patDrum) armarPrioridades();
    const float *arr = KITS[kit].arrastre;
    char kc = enFill ? E.fill[0][s - 12] : E.bateria[C.patDrum][0][s];
    if (kc != '-' && C.kick && !aBreak) golpeBombo(velDe(kc));   // el bombo nunca se arrastra: es el tiempo
    for (int t = 0; t < NUM_IND; t++) {
      if (enFill) {                                // el fill suena entero, sin mirar la densidad
        char c = E.fill[t + 1][s - 12];
        if (c != '-') golpeIndTarde(t, velDe(c), c == 'X', 0.6f, arr[t] * 0.4f);   // golpes cortos y casi sin arrastre: un redoble arrastrado es barro
        continue;
      }
      if (prio[t][s] >= densS) continue;
      char c = E.bateria[C.patDrum][t + 1][s];
      // El arrastre de la pista, con una pizca de variación humana (±12 %)
      golpeIndTarde(t, velDe(c) * (0.92f + 0.08f * frnd()), c == 'X', 1.0f, arr[t] * (0.88f + 0.24f * frnd()));
    }
    if (crashPend && s == 0 && !enFill) { golpeInd(I_ABIERTO, 1.0f, true, 2.5f); crashPend = false; }   // el plato del "1"
  } else if (crashPend && s == 0) crashPend = false;

  pasoTotalPub = pr; contadorPasos = contadorPasos + 1;
}

// ==============================================================================================================================================
// RENDER POR BLOQUES — cada voz de corrido, con su estado en variables locales
// ==============================================================================================================================================
IRAM_ATTR void renderSidechain(float *o, int m) {  // baja en ~2 ms al llegar el bombo y vuelve en ~140 ms
  float obj = scObj, s = sc;
  for (int i = 0; i < m; i++) { obj = sinSub(obj * dSc); s += (obj - s) * 0.012f; s = sinSub(s); o[i] = s; }
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

// BAJO: sierras/sub/seno -> saturación -> SVF con envolvente, más el golpe de RUIDO del dedo o de la
// púa (pasa-banda propio). Sin ese golpe un contrabajo suena a sinte; con él, a madera.
IRAM_ATTR void renderBajo(float *o, const float *scb, int m, float corte, float q, float comp) {
  if (!bajo.atk && bajo.env < 1e-5f && bajo.nz < 1e-5f) return;
  const TimbreBajo &T = TBAJO[tBajo];
  float f1 = bajo.f1, f2 = bajo.f2, fs = bajo.fs, fsn = bajo.fsn, env = bajo.env, envF = bajo.envF, inc = bajo.inc1;
  float nz = bajo.nz; const float nzC = bajo.nzC, nzAmt = T.nzAmt;
  bool atk = bajo.atk, atkF = bajo.atkF;
  const float incObj = bajo.incObj;
  const bool dos = T.detune != 1.0f, conSierra = T.sierras > 0.0f, conSub = T.sub > 0.0f, conSeno = T.seno > 0.0f, conRuido = nzAmt > 0.0f;
  const float nivel = NIVEL_BAJO * T.nivel * comp, dComp = 1.0f / (0.75f + 0.25f * T.drive);
  float i1 = inc, i2 = inc * T.detune, iS = inc * 0.5f, v1 = 1.0f / i1, v2 = 1.0f / i2, vS = 1.0f / iS;
  Filtro s = bajo.s; BQ nb = bajo.nzBp; const uint32_t c = muestraGlobal;
  uint32_t sem = semillaRuido;
  for (int i = 0; i < m; i++) {
    if (atk) { env += incAtkBajo; if (env >= 1.0f) { env = 1.0f; atk = false; } } else env *= dBajo;
    if (atkF) { envF += 1.0f / 66.0f; if (envF >= 1.0f) { envF = 1.0f; atkF = false; } } else envF *= dBajoF;
    if (((c + i) & 15) == 0) {
      inc += (incObj - inc) * kGlide16;             // se desliza hacia la nota nueva
      i1 = inc; i2 = inc * T.detune; iS = inc * 0.5f; v1 = 1.0f / i1; v2 = 1.0f / i2; vS = 1.0f / iS;
      coefFiltro(s, corte * exp2Rapido(T.envOct * envF), q);
    }
    float mez = 0.0f;
    if (conSierra) { float a = sierra(f1, i1, v1); if (dos) a = (a + sierra(f2, i2, v2)) * 0.5f; mez += a * T.sierras; }
    if (conSub) mez += cuadrada(fs, iS, vS) * T.sub;
    if (conSeno) mez += suave(fsn, i1) * T.seno * 1.2f;
    float lp, bp; pasoFiltro(s, softClip(mez * T.drive) * dComp + 1e-18f, lp, bp);
    float y = lp * env;
    if (conRuido && nz > 1e-4f) { y += pasoBQ(nb, ruido(sem)) * nz * nzAmt * 0.8f; nz *= nzC; }
    o[i] += y * nivel * (1.0f - SC_BAJO * scb[i]);
  }
  bajo.f1 = f1; bajo.f2 = f2; bajo.fs = fs; bajo.fsn = fsn; bajo.env = env; bajo.envF = envF;
  bajo.atk = atk; bajo.atkF = atkF; bajo.s = s; bajo.inc1 = inc; bajo.nz = nz; bajo.nzBp = nb;
  semillaRuido = sem;
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
    if (env < 1e-3f) v.eC = dRapido;               // −60 dB -> 1.5 ms -> se libera a −82 dB
    if (nz < 1e-3f) v.nC = dRapido;
    if (env < 8e-5f && nz < 8e-5f) v.viva = false;
  }
  semillaRuido = sem;
}

// ARPEGIO: una sierra PolyBLEP -> pasa-bajos con envolvente -> VCA. Se apaga en dos etapas.
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

// PAD: cuatro voces tocando el acorde. Cada una lleva sus dos osciladores y su SVF; la suma pasa por
// un compresor propio (como el oscilador del v2) para que el pad no se mueva de nivel cuando cambia
// el timbre, el filtro o el acorde. Manda una copia al eco de cinta.
IRAM_ATTR void renderPad(float *oL, float *oR, float *envio, const float *scb, int m) {
  for (int i = 0; i < m; i++) { oL[i] = 0.0f; oR[i] = 0.0f; }
  const TimbrePad &T = TPAD[tPad];
  bool algo = false;
  for (int k = 0; k < VOCES_PAD; k++) {
    VozPad &v = pads[k];
    if (!v.viva) continue;
    // Una voz que cambia de nota baja con un corte FIJO de 12 ms, no con la suelta del POT3: con la
    // suelta larga tardaba segundos en llegar al silencio y el pad se quedaba en el acorde anterior
    // — medido: el pad andaba fuera de la armonía la mayor parte del tiempo. Son ~80 ms de bajada en
    // UNA de las cuatro voces, y los acordes duran 6 a 13 s: se oye como un cambio de arco.
    if (v.cambiando && v.amp < 2e-3f) { v.nota = v.notaObj; v.cambiando = false; }
    const float obj = (C.padOn && !v.cambiando) ? 1.0f : 0.0f;
    if (obj == 0.0f && v.amp < 1e-5f) continue;    // callada del todo: no gasta nada
    algo = true;
    float f = hz((float)v.nota);
    v.inc1 = f / SR; v.inv1 = 1.0f / v.inc1;
    v.inc2 = v.inc1 * T.detune; v.inv2 = 1.0f / v.inc2;
    coefFiltro(v.s, fminf(12000.0f, f * T.corte * (0.35f + 3.6f * padFiltS)), T.q);
    float f1 = v.f1, f2 = v.f2, fsub = v.fs, amp = v.amp;
    const float i1 = v.inc1, w1 = v.inv1, i2 = v.inc2, w2 = v.inv2, iS = i1 * 0.5f, wS = 1.0f / iS;
    const float gL = v.gL, gR = v.gR, k1 = v.cambiando ? kPadCorte : (obj > amp ? padAtk : padRel);
    Filtro s = v.s;
    const uint8_t onda = T.onda;
    for (int i = 0; i < m; i++) {
      amp += (obj - amp) * k1;
      float x;
      switch (onda) {
        case 0:  x = (sierra(f1, i1, w1) + sierra(f2, i2, w2)) * 0.45f; break;
        case 1:  { fsub += iS; if (fsub >= 1.0f) fsub -= 1.0f;   // el sub en seno, no en cuadrada:
                   x = pulso(f1, i1, w1, 0.42f) * 0.45f + seno(fsub) * 0.26f; } break;   // una cuadrada son dos PolyBLEP más por voz
        case 2:  x = (suave(f1, i1) + suave(f2, i2)) * 0.45f; break;
        default: { f1 += i1; if (f1 >= 1.0f) f1 -= 1.0f;
                   float p2 = f1 + f1; if (p2 >= 1.0f) p2 -= 1.0f;
                   x = seno(f1) * 0.80f + 0.14f * seno(p2); } break;
      }
      float lp, bp; pasoFiltro(s, x + 1e-18f, lp, bp);
      float y = lp * amp;
      oL[i] += y * gL; oR[i] += y * gR;
    }
    v.f1 = f1; v.f2 = f2; v.fs = fsub; v.amp = amp; v.s = s;
  }
  // Compresor del pad + sidechain del bombo + envío al eco
  float cEnv = padCompEnv, cG = padCompG;
  const float g = NIVEL_PAD * NIVEL_IND * T.nivel, envio0 = PAD_ECO_MAX * padEcoS;
  if (!algo && cEnv < 1e-6f) return;               // callado: ni compresor ni envío
  for (int i = 0; i < m; i++) {
    float l = oL[i] * g, r = oR[i] * g;
    const float pk = fabsf(l + r);
    cEnv = sinSub(cEnv + (pk - cEnv) * (pk > cEnv ? kPadCompAtk : kPadCompRel));
    cG += (PAD_COMP_NIVEL / (cEnv + PAD_COMP_NIVEL * PAD_COMP_RODILLA) - cG) * 0.003f;
    const float a = cG * (1.0f - SC_PAD * scb[i]);
    l *= a; r *= a;
    oL[i] = l; oR[i] = r;
    envio[i] += (l + r) * envio0;
  }
  padCompEnv = cEnv; padCompG = cG;
}

// ARMONÍA: voz monofónica -> chorus estéreo -> bus de armonía; y un envío al eco ping-pong de cinta
// (corchea con puntillo, o corchea si el tempo es tan lento que no cabe en el buffer; sigue al tempo
// cruzando de cabezal en 50 ms, porque un cabezal que se desliza desafina los ecos). El eco vuelve
// por su propio bus, que NO pasa por el volumen ni el silencio de la armonía: así las colas suenan
// después de callarla, y el pad y la voz (que también le mandan) no dependen de este panel.
IRAM_ATTR void renderMel(float *oL, float *oR, float *eL, float *eR, const float *scb, const float *extra, float envioMel, float fbExtra, int m) {
  if (mv.viva) {
    const TimbreMel &T = TMEL[tMel];
    float f1 = mv.f1, f2 = mv.f2, f3 = mv.f3, amp = mv.amp, fenv = mv.fenv, lfo = mv.lfo; bool atk = mv.atk; Filtro s = mv.s;
    const float i1 = mv.inc1, w1 = mv.inv1, i2 = mv.inc2, w2 = mv.inv2, eC = mv.eC, base = mv.base, dF = dMelFenv, ia = incAtkMel;
    const float g = NIVEL_MEL * NIVEL_IND * T.nivel, envOct = T.envOct, q = T.q, trem = T.trem, camp = T.campana;
    const uint8_t onda = T.onda;
    const float dLfo = (onda == 5 ? 5.0f : (onda == 4 ? 5.5f : MEL_PWM_HZ)) / SR;
    const float i3 = i1 * (onda == 4 ? 4.2f : 4.0f);     // la lámina del Rhodes / el metal del vibráfono
    const uint32_t c = muestraGlobal;
    float ancho = 0.5f - MEL_PWM_PROF * (0.5f + 0.5f * seno(lfo));
    float tg = 1.0f - trem * (0.5f + 0.5f * seno(lfo));
    int i = 0;
    for (; i < m; i++) {
      if (atk) { amp += ia; if (amp >= 1.0f) { amp = 1.0f; atk = false; } }
      else { amp *= amp < 1e-3f ? dRapido : eC; if (amp < 8e-5f) { mv.viva = false; break; } }
      fenv *= dF;
      lfo += dLfo; if (lfo >= 1.0f) lfo -= 1.0f;
      if (((c + i) & 31) == 0) {
        coefFiltro(s, base * exp2Rapido(envOct * fenv), q);
        ancho = 0.5f - MEL_PWM_PROF * (0.5f + 0.5f * seno(lfo));
        tg = 1.0f - trem * (0.5f + 0.5f * seno(lfo));
      }
      float x;
      switch (onda) {
        case 0:  x = pulso(f1, i1, w1, ancho); break;
        case 1:  x = sierra(f1, i1, w1); break;
        case 2:  x = (sierra(f1, i1, w1) + sierra(f2, i2, w2)) * 0.6f; break;
        case 3:  { f1 += i1; if (f1 >= 1.0f) f1 -= 1.0f;
                   float p2 = f1 + f1; if (p2 >= 1.0f) p2 -= 1.0f; float p3 = f1 * 3.0f; p3 -= (int)p3;
                   x = seno(f1) + 0.40f * seno(p2) + 0.12f * seno(p3); } break;
        default: { f1 += i1; if (f1 >= 1.0f) f1 -= 1.0f;
                   f3 += i3; if (f3 >= 1.0f) f3 -= 1.0f;
                   float p2 = f1 + f1; if (p2 >= 1.0f) p2 -= 1.0f;
                   // seno + 2.º armónico suave + la campana, que en el Rhodes muere con el filtro
                   x = seno(f1) * 0.95f + 0.16f * seno(p2) + camp * (onda == 4 ? fenv : 0.45f) * seno(f3);
                 } break;
      }
      float lp, bp; pasoFiltro(s, x, lp, bp);
      melBuf[i] = lp * amp * g * tg * (1.0f - SC_IND * scb[i]);
    }
    for (; i < m; i++) melBuf[i] = 0.0f;
    mv.f1 = f1; mv.f2 = f2; mv.f3 = f3; mv.amp = amp; mv.fenv = fenv; mv.lfo = lfo; mv.atk = atk; mv.s = s;
  } else for (int i = 0; i < m; i++) melBuf[i] = 0.0f;

  const float dChorus = CHORUS_HZ / SR, base = CHORUS_MS * 0.001f * SR, prof = CHORUS_PROF_MS * 0.001f * SR;
  // A tempos muy lentos la corchea con puntillo no cabe en el buffer: ahí el eco pasa a corchea. El
  // cambio se oye suave porque el delay ya cruza entre dos cabezales en 50 ms.
  int objetivo = (int)(3.0f * sps);
  if (objetivo > ECO_MAX - 2) objetivo = (int)(2.0f * sps);
  if (objetivo > ECO_MAX - 2) objetivo = ECO_MAX - 2;
  if (ecoXf >= 1.0f && abs(objetivo - ecoN) > 32) { ecoN2 = objetivo; ecoXf = 0.0f; }
  float fb = ECO_FEEDBACK + fbExtra; if (fb > 0.86f) fb = 0.86f;
  float lfoC = chorusLfo, xf = ecoXf, lpL = ecoLpL, lpR = ecoLpR, hpL = ecoHpL, hpR = ecoHpR;
  int ci = chorusIdx, ei = ecoIdx;
  float d1 = base + prof * seno(lfoC), d2 = base + prof * seno(lfoC + 0.25f >= 1.0f ? lfoC - 0.75f : lfoC + 0.25f);
  const uint32_t c0 = muestraGlobal;
  for (int i = 0; i < m; i++) {
    const float x = melBuf[i];
    chorusBuf[ci] = x;
    lfoC += dChorus; if (lfoC >= 1.0f) lfoC -= 1.0f;
    if (((c0 + i) & 15) == 0) {
      d1 = base + prof * seno(lfoC); d2 = base + prof * seno(lfoC + 0.25f >= 1.0f ? lfoC - 0.75f : lfoC + 0.25f);
    }
    float p1 = (float)ci - d1; if (p1 < 0.0f) p1 += CHORUS_N;
    float p2 = (float)ci - d2; if (p2 < 0.0f) p2 += CHORUS_N;
    int a1 = (int)p1, a2 = (int)p2; float t1 = p1 - a1, t2 = p2 - a2;
    a1 &= CHORUS_N - 1; a2 &= CHORUS_N - 1;
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
    lpL = sinSub(lpL + (dl - lpL) * 0.38f); lpR = sinSub(lpR + (dr - lpR) * 0.38f);      // cada vuelta más oscura (cinta)
    hpL = sinSub(hpL + (lpL - hpL) * 0.010f); hpR = sinSub(hpR + (lpR - hpR) * 0.010f);  // y sin barro abajo (~70 Hz)
    float fl = lpL - hpL, fr = lpR - hpR;
    float inL = x * envioMel + extra[i] + fb * fr, inR = fb * fl;   // ping-pong: lo que vuelve cruza de lado
    if (inL > 0.99f) inL = 0.99f; if (inL < -0.99f) inL = -0.99f;
    if (inR > 0.99f) inR = 0.99f; if (inR < -0.99f) inR = -0.99f;
    ecoL[ei] = (int16_t)(inL * 32767.0f); ecoR[ei] = (int16_t)(inR * 32767.0f);
    ei = (ei + 1) & (ECO_MAX - 1);
    oL[i] += L; oR[i] += R;
    eL[i] = dl * ECO_MEZCLA; eR[i] = dr * ECO_MEZCLA;
  }
  chorusLfo = lfoC; chorusIdx = ci; ecoIdx = ei; ecoXf = xf; ecoLpL = lpL; ecoLpR = lpR; ecoHpL = hpL; ecoHpR = hpR;
}

volatile uint32_t medSec[6];                        // MEDIR: ciclos por sección (batería · bajo · armonía · pad · master · pasos)
#if MEDIR && !defined(SIMULADOR)
#define CICLOS() ESP.getCycleCount()
#else
#define CICLOS() 0u
#endif
float envioMelBloque = 1.0f;
void renderTramo(int off, int m, float corte, float q, float comp, float fbExtra) {
  uint32_t c0 = CICLOS();
  renderSidechain(bufSc + off, m);
  renderBombo(bufKick + off, m);
  renderInd(bufIndL + off, bufIndR + off, bufSc + off, m);
  uint32_t c1 = CICLOS();
  renderBajo(bufBajo + off, bufSc + off, m, corte, q, comp);
  uint32_t c2 = CICLOS();
  renderPad(padL + off, padR + off, envioEco + off, bufSc + off, m);   // antes de la armonía: le deja su envío al eco
  uint32_t c3 = CICLOS();
  renderMel(bufMelL + off, bufMelR + off, ecoBufL + off, ecoBufR + off, bufSc + off, envioEco + off, envioMelBloque, fbExtra, m);
  renderArp(bufMelL + off, bufMelR + off, bufSc + off, m);
  uint32_t c4 = CICLOS();
  medSec[0] += (c1 - c0) >> 6; medSec[1] += (c2 - c1) >> 6; medSec[3] += (c3 - c2) >> 6; medSec[2] += (c4 - c3) >> 6;
  muestraGlobal += m;
}

// ==============================================================================================================================================
// UN BLOQUE DE AUDIO (128 muestras ~ 2.9 ms)
// ==============================================================================================================================================
static i2s_chan_handle_t tx_chan;
int16_t salida[BUFFER_SAMPLES * 2];


IRAM_ATTR void calcularBloque() {
  // ── Borde del buffer: aplicar los pedidos de la tarea de control ──
  memcpy(&C, (const void *)&reqC, sizeof(C));
  if (C.tocar && !aTocando) { pasoTotal = 0xFFFFFFFFu; faltan = 0.0f; fillPend = false; crashPend = false; }   // Play: el "1" en la primera muestra
  aTocando = C.tocar; tocandoPub = C.tocar;
  aBreak = C.brk; breakPub = aBreak;
  if (reqFill != vistoFill) {                      // FILL: en el compás que suena si aún falta para el 13, si no en el siguiente
    vistoFill = reqFill;
    uint32_t p = pasoTotal + 1;
    fillCompas = (p >> 4) + ((p & 15) > 11 ? 1 : 0); fillPend = true;
  }

  estilo = C.estilo; kit = C.kit; tBajo = C.tBajo; tMel = C.tMel; tPad = C.tPad;
  modo = PROGS[C.prog].escala; transp = ESTILOS[estilo].transp;
  tonicaBajo = TONICA_BAJO + transp; if (tonicaBajo < BAJO_MINIMO) tonicaBajo += 12;
  tonicaPub = TONICA_MEL + transp;
  // Al cambiar de dialecto, el pad se reacomoda EN EL ACTO. Si esperara el "1" del próximo compás
  // se quedaría hasta 2.8 s tocando el acorde de la tonalidad anterior, mientras el bajo y la
  // melodía ya cambiaron en su próxima nota — medido: el pad quedaba fuera de tono todo ese rato.
  if (estilo != estiloPadVisto) {
    estiloPadVisto = estilo;
    acomodarPad(acordeDe(pasoTotal == 0xFFFFFFFFu ? 0 : (pasoTotal >> 4)));
  }

  // ── Pots suavizados (por bloque; las ganancias se interpolan por muestra) ──
  float b = BPM_MIN + BPM_RANGO * pPot[P_GENERAL][1];
  if (fabsf(b - bpm) > 0.6f) setBPM(roundf(b));    // BPM entero con histéresis: el ruido del ADC no hace bailar el tempo
  swingS += ((0.5f + (SWING_MAX - 0.5f) * pPot[P_GENERAL][2]) - swingS) * 0.2f;
  lpfS += (pPot[P_GENERAL][3] - lpfS) * 0.15f;
  corteBajoS += (pPot[P_BAJO][1] - corteBajoS) * 0.15f;
  filtDrumS += (pPot[P_DRUM][1] - filtDrumS) * 0.15f;
#if MEDIR
  densS = 1.05f;                                   // al medir, siempre el peor caso
#else
  densS += (pPot[P_DRUM][3] * 1.05f - densS) * 0.25f;
#endif
  largoDrum = 0.5f + pPot[P_DRUM][2];              // x0.5 … x1.5 en las caídas de la batería
  incAtkBajo = 1.0f / (BAJO_ATK_MIN * powf(BAJO_ATK_RANGO, pPot[P_BAJO][2]) * SR);
  { float tau = BAJO_DEC_MIN * powf(BAJO_DEC_RANGO, pPot[P_BAJO][3]); dBajo = expf(-1.0f / (tau * SR)); dBajoF = expf(-1.0f / (tau * TBAJO[tBajo].fenvRel * SR)); }
  incAtkMel = 1.0f / (MEL_ATK_MIN * powf(MEL_ATK_RANGO, pPot[P_MEL][2]) * SR);
  decMel = MEL_DEC_MIN * powf(MEL_DEC_RANGO, pPot[P_MEL][3]);
  filtMel = exp2f((pPot[P_MEL][1] - 0.5f) * 4.0f); // x0.25 … x4 sobre el reposo del filtro de cada nota
  padFiltS += (pPot[P_PAD][1] - padFiltS) * 0.15f;
  padEcoS  += (pPot[P_PAD][3] - padEcoS)  * 0.15f;
  { float tau = PAD_ATK_MIN * powf(PAD_ATK_RANGO, pPot[P_PAD][2]);   // el mismo pot para el ataque y la suelta
    padAtk = 1.0f - expf(-1.0f / (tau * SR)); padRel = 1.0f - expf(-1.0f / (tau * 1.6f * SR)); }
  vin.wow = CINTA_WOW; vin.desgaste = CINTA_DESGASTE;
  const float corte = 55.0f * powf(30.0f, corteBajoS);   // 55 Hz … 1.6 kHz
  const float q = fminf(5.0f, (2.8f - 1.8f * corteBajoS) * TBAJO[tBajo].qMul);   // resonancia atada al corte
  const float comp = 1.0f / powf(q, 0.30f);
  const float fbExtra = 0.0f;

  // Ganancias de cada instrumento: volumen del panel x su play/stop "falso" (con fundido)
  const float g0[5] = { gDrumS, gKickS, gBajoS, gMelS, gPadS };
  const float gDrumObj = C.drumOn ? ganVol(pPot[P_DRUM][0]) : 0.0f;
  const float gBajoObj = C.bajoOn ? ganVol(pPot[P_BAJO][0]) : 0.0f;
  const float gMelObj  = C.melOn  ? ganVol(pPot[P_MEL][0])  : 0.0f;
  const float gPadObj  = C.padOn  ? ganVol(pPot[P_PAD][0])  : 0.0f;
  gDrumS += (gDrumObj - gDrumS) * 0.25f; gKickS += ((C.kick ? 1.0f : 0.0f) - gKickS) * 0.25f;
  gBajoS += (gBajoObj - gBajoS) * 0.25f; gMelS += (gMelObj - gMelS) * 0.25f;
  gPadS  += (gPadObj - gPadS) * 0.25f;
  envioMelBloque = gMelS;
  float v0 = volS; volS += (pPot[P_GENERAL][0] * pPot[P_GENERAL][0] - volS) * 0.15f;

  // Filtros de bus (coeficientes una vez por bloque; el valor ya viene suavizado)
  coefFiltro(fDrumL, corteBus(filtDrumS), 0.75f); fDrumR.a1 = fDrumL.a1; fDrumR.a2 = fDrumL.a2; fDrumR.a3 = fDrumL.a3; fDrumR.k = fDrumL.k;
  coefFiltro(fMasL, corteBus(lpfS), 0.75f);       fMasR.a1 = fMasL.a1; fMasR.a2 = fMasL.a2; fMasR.a3 = fMasL.a3; fMasR.k = fMasL.k;
  const float abDrum = abiertoBus(filtDrumS), abMas = abiertoBus(lpfS);
  // DESGASTE de cinta: el disco pone el carácter y el POT4 lo escala. Saturación suave + la banda
  // recortada por arriba y por abajo. Va ANTES del pasa-bajos general (una no linealidad después de
  // un filtro le devuelve los agudos que el filtro ya quitó).
  const float desg = clamp01(vin.desgaste);
  if (fabsf(desg - desgVisto) > 0.004f) {
    desgVisto = desg;
    bqLPFvivo(desgLpL, 16000.0f * exp2Rapido(1.83f * (1.0f - desg)) * 0.282f, 0.707f);   // 16 kHz … 4.5 kHz
    bqLPFvivo(desgLpR, 16000.0f * exp2Rapido(1.83f * (1.0f - desg)) * 0.282f, 0.707f);
    bqHPFvivo(desgHpL, 25.0f * exp2Rapido(2.26f * desg), 0.707f);                        // 25 Hz … 120 Hz
    bqHPFvivo(desgHpR, 25.0f * exp2Rapido(2.26f * desg), 0.707f);
  }
  const float satDrive = 1.0f + 2.2f * desg, satComp = 1.0f / (1.0f + 0.9f * desg);
  // WOW y FLUTTER: la altura deriva porque el disco no gira perfecto. El wow va EXACTAMENTE a la
  // vuelta del disco (33 1/3 rpm = 0.556 Hz, la misma fase que el golpe de la aguja) y el flutter a
  // 7.3 Hz. Se hace con un retardo corto leído en una posición que se mueve: mover el retardo ES
  // mover la altura.
  const float aWow = 120.0f * vin.wow, aFlut = 2.5f * vin.wow;
  const float dFlut = 7.3f / SR;
  // la "vuelta" (0.556 Hz) ya no la lleva el disco: la lleva el wow, que es lo que queda de él
  vin.vuelta += (float)BUFFER_SAMPLES / (1.8f * SR);
  if (vin.vuelta >= 1.0f) vin.vuelta -= 1.0f;

  for (int i = 0; i < BUFFER_SAMPLES; i++) envioEco[i] = 0.0f;

  for (int i = 0; i < BUFFER_SAMPLES; i++) {
    bufKick[i] = 0.0f; bufBajo[i] = 0.0f; bufIndL[i] = 0.0f; bufIndR[i] = 0.0f;
    bufMelL[i] = 0.0f; bufMelR[i] = 0.0f;
  }

  // ── Síntesis por tramos ──
  // Los pasos caen en la muestra exacta, y lo mismo los golpes ATRASADOS del arrastre: el bloque se
  // corta en el borde del próximo paso O en el del próximo golpe pendiente, el que venga primero.
  // SWING: la semicorchea par dura 2·r y la impar 2·(1−r) (r = 50 % recto … 72 %); el par siempre
  // suma dos, así el pulso no se mueve de lugar.
  int hecho = 0;
  while (hecho < BUFFER_SAMPLES) {
    for (int k = 0; k < MAX_PEND; k++) if (pend[k].viva && pend[k].faltan <= 0.0f) {
      pend[k].viva = false;
      golpeInd(pend[k].pista, pend[k].vel, pend[k].acento, pend[k].largo);
    }
    if (aTocando && faltan <= 0.0f) {
      uint32_t cp0 = CICLOS(); avanzarPaso();
      faltan += sps * ((pasoTotal & 1) ? 2.0f * (1.0f - swingS) : 2.0f * swingS);
      medSec[5] += (CICLOS() - cp0) >> 6;
    }
    int m2 = BUFFER_SAMPLES - hecho;
    if (aTocando) { int hasta = (int)ceilf(faltan); if (hasta < 1) hasta = 1; if (hasta < m2) m2 = hasta; }
    for (int k = 0; k < MAX_PEND; k++) if (pend[k].viva) {
      int h = (int)ceilf(pend[k].faltan); if (h < 1) h = 1; if (h < m2) m2 = h;
    }
    if (aTocando) faltan -= (float)m2;
    for (int k = 0; k < MAX_PEND; k++) if (pend[k].viva) pend[k].faltan -= (float)m2;
    renderTramo(hecho, m2, corte, q, comp, fbExtra);
    hecho += m2;
  }
  envBajoPub = bajo.env;
  { float pulso = (float)(pasoTotal & 3) + 1.0f - faltan / sps; fasePub = pulso * 0.25f; }

  // ── Master ──
  //   batería (bombo + 6 pistas) -> su pasa-bajos · bajo · armonía · eco · pad
  //   -> wow y flutter -> desgaste de cinta -> pasa-bajos general
  //   -> DC -> limitador con lookahead -> 13 kHz -> techo suave
  uint32_t cm0 = CICLOS();
  const float objF = aTocando ? 1.0f : 0.0f;
  const float inv = 1.0f / BUFFER_SAMPLES;
  float pkD = 0, pkB = 0, pkM = 0, pkP = 0, pkT = 0;
  int wi = wowIdx; float flut = flutFase, dWow = 256.0f;
  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    float x = (float)n * inv;
    float gD = g0[0] + (gDrumS - g0[0]) * x, gK = g0[1] + (gKickS - g0[1]) * x, gB = g0[2] + (gBajoS - g0[2]) * x;
    float gM = g0[3] + (gMelS - g0[3]) * x, gP = g0[4] + (gPadS - g0[4]) * x;
    float vv = v0 + (volS - v0) * x;
    fundido = sinSub(fundido + (objF - fundido) * 0.003f);   // ~8 ms
    // batería con su filtro
    float k = bufKick[n] * gK;
    float dl = k + bufIndL[n], dr = k + bufIndR[n], lp, bp;
    pasoFiltro(fDrumL, dl + 1e-18f, lp, bp); dl += (lp - dl) * (1.0f - abDrum);
    pasoFiltro(fDrumR, dr + 1e-18f, lp, bp); dr += (lp - dr) * (1.0f - abDrum);
    dl *= gD; dr *= gD;
    float ba = bufBajo[n] * gB, ml = bufMelL[n] * gM, mr = bufMelR[n] * gM;
    float pl = padL[n] * gP, pr = padR[n] * gP;
    pkD = fmaxf(pkD, fabsf(dl)); pkB = fmaxf(pkB, fabsf(ba));
    pkM = fmaxf(pkM, fabsf(ml) + fabsf(mr)); pkP = fmaxf(pkP, fabsf(pl) + fabsf(pr));
    const float g = vv * fundido * MASTER;
    float l = (dl + ba + ml + ecoBufL[n] + pl) * g;
    float r = (dr + ba + mr + ecoBufR[n] + pr) * g;
    // wow y flutter
    wowL[wi] = l; wowR[wi] = r;
    flut += dFlut; if (flut >= 1.0f) flut -= 1.0f;
    if ((n & 7) == 0) dWow = 256.0f + aWow * seno(vin.vuelta) + aFlut * seno(flut);
    float pp = (float)wi - dWow; if (pp < 0.0f) pp += WOW_N;
    int a0 = (int)pp; float tf = pp - a0; a0 &= WOW_N - 1;
    int a1 = (a0 + 1) & (WOW_N - 1);
    l = wowL[a0] + (wowL[a1] - wowL[a0]) * tf;
    r = wowR[a0] + (wowR[a1] - wowR[a0]) * tf;
    wi = (wi + 1) & (WOW_N - 1);
    // desgaste de cinta: saturación + banda recortada (antes del pasa-bajos general)
    l = pasoBQ(desgHpL, pasoBQ(desgLpL, softClip(l * satDrive) * satComp));
    r = pasoBQ(desgHpR, pasoBQ(desgLpR, softClip(r * satDrive) * satComp));
    // El DISCO entra ANTES del pasa-bajos general. Lo tenía después, "porque el ruido del disco está
    // encima de la música", y eso estaba mal: con el POT4 a medio camino la música sale cortada en
    // ~1.7 kHz y el disco se quedaba con todo el agudo para él solo. Medido: el vinilo va 36 dB bajo
    // la música en RMS, pero era lo ÚNICO en la banda de 4–13 kHz, y eso se oye como "el ruido blanco
    // está más fuerte que la música". Pasando por el filtro, cerrar el POT4 apaga el disco con todo
    // lo demás, que es además lo que hace un filtro sobre un disco de verdad.
    pasoFiltro(fMasL, l + 1e-18f, lp, bp); l += (lp - l) * (1.0f - abMas);   // pasa-bajos general
    pasoFiltro(fMasR, r + 1e-18f, lp, bp); r += (lp - r) * (1.0f - abMas);
    float yl = sinSub(l - dcX1L + 0.9985f * dcY1L); dcX1L = l; dcY1L = yl; l = yl;
    float yr = sinSub(r - dcX1R + 0.9985f * dcY1R); dcX1R = r; dcY1R = yr; r = yr;
    float pk = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
    if (pk > medPico) medPico = pk;
    if (pk > pkT) pkT = pk;
    if (pk > limEnv) limEnv = pk; else limEnv = sinSub(pk + (limEnv - pk) * limRel);
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
  wowIdx = wi; flutFase = flut;
  nivelPub[0] = pkD; nivelPub[1] = pkB; nivelPub[2] = pkM * 0.5f; nivelPub[3] = pkP * 0.5f; nivelPub[4] = pkT;
  muestraPub = muestraGlobal;
  medSec[4] += (CICLOS() - cm0) >> 6;
  grabarBloque();
}

// Al colchón, el MISMO bloque que va al DAC. Play abre una toma, Stop la cierra BLOQUES_COLA después
// (para que entre el fundido y no termine en un clic); un Play durante esa cola cierra la anterior
// ahí mismo y abre la siguiente sin perder ni un bloque.
static inline void marcar(uint32_t idx, bool inicio) {
  const uint32_t k = marcasEsc;
  if (k - marcasLeidas >= NUM_MARCAS) return;       // la grabadora está muy atrás: no debería pasar nunca
  marcas[k % NUM_MARCAS].idx = idx; marcas[k % NUM_MARCAS].inicio = inicio;
  __sync_synchronize();
  marcasEsc = k + 1;
}
void grabarBloque() {
  static bool tocabaG = false, enToma = false;
  static int  cola = 0;
  if (!ring) return;
  if (aTocando && !tocabaG) {                       // Play
    if (enToma) marcar(bloquesEsc, false);          // la cola de la toma anterior se corta acá
    marcar(bloquesEsc, true);
    enToma = true; cola = 0;
  } else if (!aTocando && tocabaG && enToma) cola = BLOQUES_COLA;   // Stop
  tocabaG = aTocando;
  if (!enToma) return;
  const uint32_t esc = bloquesEsc;
  if (esc - bloquesLeidos < ringBloques) {
    memcpy(ring + (size_t)(esc % ringBloques) * BYTES_BLOQUE, salida, BYTES_BLOQUE);
    __sync_synchronize();                           // los datos antes que el contador
    bloquesEsc = esc + 1;
  } else bloquesPerdidos = bloquesPerdidos + 1;     // colchón lleno: la tarjeta no alcanzó
  if (cola > 0 && --cola == 0) { marcar(bloquesEsc, false); enToma = false; }
}

// ==============================================================================================================================================
// MATRIZ 32x8 (tarea de LEDs, core 0) — EFECTOS MATEMÁTICOS QUE MUTAN
// ==============================================================================================================================================
// Ocho campos matemáticos (interferencia de dos fuentes, anillos, espiral, rayos, rayas, rombos,
// cuadros, damero). Cada LED evalúa el campo en su posición y sus NIVELES ENTEROS son las bandas:
// cada banda es de un color plano y entre banda y banda hay negro. Por eso se ve definido: no hay
// degradados, sólo bordes.
//
// Muta: cada 2 compases, siempre en el "1", se sortea otro efecto (nunca el mismo), otra paleta,
// otra simetría (ninguna · espejo · espejo doble) y otros parámetros.
//
// Es rítmico: el patrón NO se desliza, avanza A SALTOS en la rejilla (por negra, corchea o
// semicorchea, según lo que salió en el sorteo) y cada salto se asienta en ~35 ms. El bombo engorda
// las bandas, la caja gira la paleta un lugar y las destiñe un instante, y el acorde que suena pone
// el tono de la paleta. Parado, sigue andando con un reloj propio al tempo del POT2 del General.
//
// La energía se normaliza en cada cuadro (un efecto con bandas anchas no prende más que uno con
// bandas finas) y deja que el bombo la suba un poco: así el limitador de corriente casi nunca actúa y
// el pulso del bombo se sigue viendo.
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
inline void borrarMatriz() { for (int i = STATUS_LEDS; i < NUM_LEDS; i++) leds[i] = CRGB(0, 0, 0); }

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

uint32_t rngLed = 0xA5A5A5A5u;
static inline float lrnd() { rngLed ^= rngLed << 13; rngLed ^= rngLed >> 17; rngLed ^= rngLed << 5; return (rngLed >> 8) * (1.0f / 16777216.0f); }

uint32_t vistoGolpe[NUM_EV];
float    flash[NUM_EV];                            // el destello de cada evento, para los LEDs de la placa
float    pulsoNegra = 0.0f;

const float ENERGIA_MATRIZ = 0.055f;               // fracción del máximo que prende un cuadro normal…
const float ENERGIA_BOMBO  = 0.030f;               // …y lo que le suma el bombo (todo bajo CARGA_MAXIMA)
const float ASIENTA_SALTO  = 0.035f;               // s que tarda un salto del patrón en asentarse

#define NUM_EFECTOS 8
#define EF_INTERFERENCIA 0
#define EF_ANILLOS       1
#define EF_ESPIRAL       2
#define EF_RAYOS         3
#define EF_RAYAS         4
#define EF_ROMBOS        5
#define EF_CUADROS       6
#define EF_DAMERO        7
struct {
  int tipo, sim, sub, n, lado;
  float k, sep, cosA, senA, salto, duty;
  bool dif;
  uint8_t pal[3];
} ef;
int      efAnterior = -1;
float    faseEf = 0.0f, faseEfObj = 0.0f, bombM = 0.0f, cajaM = 0.0f;
uint8_t  giroPal = 0;
uint32_t pasosVistosM = 0, bomboVisto = 0, cajaVista = 0;
uint32_t pasoLibre = 0; float relojLibre = 0.0f;   // parado: los pasos los cuenta un reloj propio

void sortearEfecto() {
  rngLed ^= (uint32_t)micros() * 2654435761u;
  ef.tipo = (efAnterior + 1 + (int)(lrnd() * (NUM_EFECTOS - 1))) % NUM_EFECTOS;   // nunca el mismo
  efAnterior = ef.tipo;
  ef.sim = (int)(lrnd() * 3.0f);
  const uint8_t h = (uint8_t)(ESTILOS[reqC.estilo].hue + acordePub * 37);
  switch ((int)(lrnd() * 4.0f)) {
    case 0:  ef.pal[0] = h; ef.pal[1] = h + 85;  ef.pal[2] = h + 170; break;   // tríada
    case 1:  ef.pal[0] = h; ef.pal[1] = h + 128; ef.pal[2] = h;       break;   // complemento
    case 2:  ef.pal[0] = h; ef.pal[1] = h + 18;  ef.pal[2] = h + 36;  break;   // vecinos
    default: ef.pal[0] = 16; ef.pal[1] = 140;    ef.pal[2] = 28;      break;   // fuego y agua
  }
  ef.k = 0.25f + 0.30f * lrnd();
  // espiral y rayos dan la vuelta entera: con 3 o 6 brazos los colores empalman en la costura
  ef.n = lrnd() < 0.5f ? 3 : 6;
  ef.sep = 5.0f + 6.0f * lrnd();
  ef.dif = lrnd() < 0.5f;                          // interferencia: elipses (suma) o hipérbolas (resta)
  const float ANG[5] = { 0.0f, 0.5f, 1.0f, 1.5f, 2.36f };
  const float a = ANG[(int)(lrnd() * 5.0f)];
  ef.cosA = cosf(a); ef.senA = sinf(a);
  const int SUB[4] = { 1, 2, 2, 4 };               // saltos por negra
  ef.sub = SUB[(int)(lrnd() * 4.0f)];
  const float SALTO[4] = { 0.25f, 0.5f, -0.25f, 0.5f };
  ef.salto = SALTO[(int)(lrnd() * 4.0f)];
  ef.duty = 0.38f + 0.14f * lrnd();                // fracción de cada banda que se prende
  ef.lado = 2 + (int)(lrnd() * 3.0f);              // damero: lado del cuadro
}

float campoEfecto(float cx, float cy) {
  switch (ef.tipo) {
    case EF_INTERFERENCIA: {
      const float d1 = sqrtf((cx - ef.sep) * (cx - ef.sep) + cy * cy), d2 = sqrtf((cx + ef.sep) * (cx + ef.sep) + cy * cy);
      return (ef.dif ? d1 - d2 : d1 + d2) * ef.k * 0.8f;
    }
    case EF_ANILLOS: return sqrtf(cx * cx + cy * cy) * ef.k;
    case EF_ESPIRAL: return atan2f(cy, cx) * 0.15915494f * ef.n + sqrtf(cx * cx + cy * cy) * ef.k;
    case EF_RAYOS:   return atan2f(cy, cx) * 0.15915494f * ef.n * 2;
    case EF_RAYAS:   return (cx * ef.cosA + cy * 3.0f * ef.senA) * ef.k * 0.6f;
    case EF_ROMBOS:  return (fabsf(cx) + fabsf(cy) * 2.0f) * ef.k * 0.7f;
    default:         return fmaxf(fabsf(cx) * 0.25f, fabsf(cy)) * ef.k * 2.4f;   // EF_CUADROS
  }
}

// Un paso de la rejilla (semicorchea): el salto del patrón y, cada 2 compases en el "1", la mutación
void pasoMatriz(uint32_t p) {
  if ((p & 31) == 0) sortearEfecto();
  if ((p & 15) % (4 / ef.sub) == 0) faseEfObj += ef.salto;
}

void escenaEfectos(float dt) {
  // ── los pasos que pasaron desde el cuadro anterior ──
  if (tocandoPub) {
    const uint32_t c = contadorPasos;
    uint32_t n = c - pasosVistosM; pasosVistosM = c;
    if (n > 8) n = 1;                              // se perdió la cuenta (arranque, Play): sólo el último
    const uint32_t pt = pasoTotalPub;
    for (uint32_t k = 0; k < n; k++) pasoMatriz(pt - (n - 1 - k));
    relojLibre = 0.0f;
  } else {
    pasosVistosM = contadorPasos;
    relojLibre += dt * bpmPub * (4.0f / 60.0f);    // semicorcheas al tempo del POT2
    while (relojLibre >= 1.0f) { relojLibre -= 1.0f; pasoMatriz(++pasoLibre); }
  }
  // ── los golpes ──
  const uint32_t gb = golpes[EV_BOMBO], gc = golpes[I_CAJA] + golpes[I_CLAP];
  if (gb != bomboVisto) { bomboVisto = gb; bombM = 1.0f; }
  if (gc != cajaVista)  { cajaVista = gc; cajaM = 1.0f; giroPal++; }

  faseEf += (faseEfObj - faseEf) * (1.0f - expf(-dt / ASIENTA_SALTO));
  if (fabsf(faseEf) > 4096.0f) { const float r = floorf(faseEf); faseEf -= r; faseEfObj -= r; }   // sin perder precisión
  const float duty = fminf(0.92f, ef.duty + 0.32f * bombM);
  const uint8_t sat = (uint8_t)(255.0f - 120.0f * cajaM);

  uint32_t suma = 0;
  for (int y = 0; y < MATRIZ_ALTO; y++)
    for (int x = 0; x < MATRIZ_ANCHO; x++) {
      float cx = (float)x - 15.5f, cy = (float)y - 3.5f;
      if (ef.sim >= 1) cx = fabsf(cx);
      if (ef.sim == 2) cy = fabsf(cy);
      CRGB c = CRGB(0, 0, 0);
      if (ef.tipo == EF_DAMERO) {                  // los cuadros se corren de a uno con el ritmo
        const int b = (int)floorf((cx + roundf(faseEf * 4.0f)) / ef.lado) + (int)floorf(cy / ef.lado);
        if ((b & 1) == 0) c = CHSV(ef.pal[(((b >> 1) + giroPal) % 3 + 3) % 3], sat, 255);
      } else {
        const float b = campoEfecto(cx, cy) + faseEf;
        const int i = (int)floorf(b);
        if (b - (float)i < duty) c = CHSV(ef.pal[((i + giroPal) % 3 + 3) % 3], sat, 255);
      }
      const int j = xy(x, y);
      if (j >= 0) { leds[j] = c; suma += c.r + c.g + c.b; }
    }
  // ── la misma energía en todos los efectos; el bombo la sube un poco ──
  const float carga = (float)suma / ((float)MATRIZ_LEDS * 3.0f * 255.0f);
  const float objetivo = ENERGIA_MATRIZ + ENERGIA_BOMBO * bombM;
  if (carga > objetivo) {
    const uint8_t g = (uint8_t)(255.0f * objetivo / carga);
    for (int i = STATUS_LEDS; i < NUM_LEDS; i++) leds[i].nscale8(g);
  }
  bombM *= expf(-dt / 0.12f); cajaM *= expf(-dt / 0.08f);
  limitarCorrienteMatriz();
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
const uint8_t HUE_EV[NUM_EV] = { 130, 45, 160, 0, 110, 20, 200, 24, 4, 180, 150 };   // HAT CAJA PERC CLAP ABIERTO TOM MEL BOMBO BAJO ARP PAD
const uint8_t HUE_PANEL[NUM_PANELES] = { 96, 32, 0, 128, 160 };   // verde · ámbar · rojo · cian · azul
volatile uint32_t panelCambioMs = 0;
void ledsEstado(bool corre) {
  const float f[5] = { flash[EV_BOMBO], flash[EV_BAJO], fmaxf(flash[I_HAT], flash[I_ABIERTO]),
                       fmaxf(fmaxf(flash[I_CAJA], flash[I_PERC]), fmaxf(flash[I_CLAP], flash[I_TOM])),
                       fmaxf(fmaxf(flash[EV_MEL], flash[EV_ARP]), flash[EV_PAD]) };
  const uint8_t hue[5] = { HUE_EV[EV_BOMBO], HUE_EV[EV_BAJO], HUE_EV[I_HAT], HUE_EV[I_PERC], HUE_EV[EV_MEL] };
  for (int i = 0; i < 5; i++) leds[i] = CHSV(hue[i], 220, (uint8_t)(8 + 140 * fminf(1.0f, f[i])));
  const bool mute[5] = { !reqC.drumOn || !reqC.kick, !reqC.bajoOn, !reqC.drumOn, !reqC.drumOn, !reqC.melOn && !reqC.padOn };
  for (int i = 0; i < 5; i++) if (mute[i]) leds[i] = CRGB(28, 0, 0);
  if (breakPub) { leds[0] = CRGB(90, 0, 0); leds[1] = CRGB(90, 0, 0); }
  const int p = panelPub;
  leds[5] = CHSV(HUE_PANEL[p], 230, corre ? (uint8_t)(40 + 150 * pulsoNegra) : 40);
  if (millis() - panelCambioMs < 350) for (int i = 0; i < 6; i++) leds[i] = CHSV(HUE_PANEL[p], 230, 170);   // recién cambiado: todos del color del panel
  if (millis() < cargaAltaHasta) leds[5] = CRGB(140, 0, 140);
}

uint32_t vistoContador = 0;
bool estabaTocando = false;
void renderLEDs() {
  static unsigned long ultimo = 0;
  unsigned long t = millis();
  if (t - ultimo < LED_REFRESH_MS) return;
  float dt = (ultimo == 0) ? 0.0f : (float)(t - ultimo) * 0.001f;
  if (dt > 0.1f) dt = 0.1f;
  ultimo = t;

  for (int ev = 0; ev < NUM_EV; ev++) {
    uint32_t g = golpes[ev];
    if (g != vistoGolpe[ev]) { vistoGolpe[ev] = g; flash[ev] = 0.5f + 0.5f * velGolpe[ev]; }
  }
  bool corre = tocandoPub;
  uint32_t cp = contadorPasos;
  if (cp != vistoContador) { vistoContador = cp; if ((pasoTotalPub & 3) == 0) pulsoNegra = 1.0f; }
  estabaTocando = corre;

  if (PRUEBA_MS && t < PRUEBA_MS) pruebaOrientacion(t);
  else escenaEfectos(dt);
  ledsEstado(corre);

  for (int ev = 0; ev < NUM_EV; ev++) flash[ev] = sinSub(flash[ev] * 0.80f);
  pulsoNegra = sinSub(pulsoNegra * 0.82f);
  FastLED.show();
}

// ==============================================================================================================================================
// OLED 128x64 — MONITOR DE CONTROLES (tarea propia, core 0, la de menor prioridad)
// ==============================================================================================================================================
// La pantalla no adorna: dice qué está haciendo cada control. Arriba, invertida, el panel en que
// estás, los cinco paneles (el actual es el cuadrito lleno) y su estado (PLAY/STOP en el General,
// ON/OFF en los instrumentos, BRK parpadeando en el break). Debajo, los 4 pots con su nombre, su
// barra y su valor en la unidad que de verdad mueven (BPM, %, ms, x). Un pot SUELTO —recién
// cambiado de panel, todavía no lo tomaste— se ve punteado y con una rayita donde está la perilla
// física: hay que pasar por ahí para que mande. El pot que estás moviendo se marca. Abajo, qué hacen
// BTN2 y BTN3 en ese panel y qué tienen puesto, y los 16 pasos del compás. Cada botón, al apretarlo,
// muestra su valor nuevo en grande durante un segundo.
#if CON_OLED
uint8_t fb[1024];
bool    oledOk = false;

void oledComandos(const uint8_t *c, int n) {
  Wire.beginTransmission(OLED_DIR);
  Wire.write((uint8_t)0x00);                          // Co = 0, D/C = 0: lo que sigue son comandos
  Wire.write(c, n);
  Wire.endTransmission();
}
bool oledIniciar() {
  Wire.begin(OLED_SDA, OLED_SCL, I2C_HZ);
  Wire.beginTransmission(OLED_DIR);
  if (Wire.endTransmission() != 0) {                  // nadie contesta en 0x3C: seguimos sin pantalla
    Wire.begin(OLED_SDA, OLED_SCL, 400000);
    Wire.beginTransmission(OLED_DIR);
    if (Wire.endTransmission() != 0) return false;
  }
  const uint8_t init[] = {
    0xAE,                     // apagada mientras se configura
    0xD5, 0x80,               // reloj interno
    0xA8, 0x3F,               // multiplex 64
    0xD3, 0x00,               // sin desplazamiento
    0x40,                     // línea de inicio 0
    0x8D, 0x14,               // bomba de carga interna
    0x20, 0x00,               // direccionamiento HORIZONTAL: 1024 bytes de corrido llenan la pantalla
    0xDA, 0x12,               // pines COM
    0x81, 0xB0,               // contraste
    0xD9, 0xF1,               // precarga
    0xDB, 0x40,               // VCOMH
    0xA4,                     // muestra la RAM
    0xA6,                     // sin invertir
    0x2E,                     // sin scroll
    0xAF                      // encendida
  };
  oledComandos(init, sizeof(init));
  const uint8_t giro[] = { (uint8_t)(OLED_GIRO_180 ? 0xA0 : 0xA1), (uint8_t)(OLED_GIRO_180 ? 0xC0 : 0xC8) };
  oledComandos(giro, sizeof(giro));
  return true;
}
void oledVolcar() {
  const uint8_t ventana[] = { 0x21, 0, 127, 0x22, 0, 7 };    // toda la pantalla
  oledComandos(ventana, sizeof(ventana));
  for (int i = 0; i < 1024; i += 64) {                       // el buffer de Wire es de 128: trozos de 64
    Wire.beginTransmission(OLED_DIR);
    Wire.write((uint8_t)0x40);                               // D/C = 1: datos
    Wire.write(fb + i, 64);
    Wire.endTransmission();
  }
}
void oledContraste(uint8_t v) { const uint8_t c[] = { 0x81, v }; oledComandos(c, sizeof(c)); }

// ─── dibujo en el buffer ───────────────────────────────────
static inline void pon(int x, int y) {
  if ((unsigned)x >= 128u || (unsigned)y >= 64u) return;
  fb[(y >> 3) * 128 + x] |= (uint8_t)(1 << (y & 7));
}
static inline void borra(int x, int y) {
  if ((unsigned)x >= 128u || (unsigned)y >= 64u) return;
  fb[(y >> 3) * 128 + x] &= (uint8_t)~(1 << (y & 7));
}
static inline void pixO(int x, int y, bool v) { if (v) pon(x, y); else borra(x, y); }
void rectO(int x, int y, int w, int h, bool v) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) pixO(i, j, v); }
void marcoO(int x, int y, int w, int h) { rectO(x, y, w, 1, true); rectO(x, y + h - 1, w, 1, true); rectO(x, y, 1, h, true); rectO(x + w - 1, y, 1, h, true); }

// Fuente 5x7 de siempre (columnas, bit 0 arriba), de ' ' a 'Z'. Sólo mayúsculas: lo que no está se
// dibuja como espacio, así que los nombres van sin tildes.
const uint8_t FUENTE[59][5] = {
  {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},   //   ! " #
  {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x56,0x20,0x50}, {0x00,0x08,0x07,0x03,0x00},   // $ % & '
  {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x2A,0x1C,0x7F,0x1C,0x2A}, {0x08,0x08,0x3E,0x08,0x08},   // ( ) * +
  {0x00,0x80,0x70,0x30,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x00,0x60,0x60,0x00}, {0x20,0x10,0x08,0x04,0x02},   // , - . /
  {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x72,0x49,0x49,0x49,0x46}, {0x21,0x41,0x49,0x4D,0x33},   // 0 1 2 3
  {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x31}, {0x41,0x21,0x11,0x09,0x07},   // 4 5 6 7
  {0x36,0x49,0x49,0x49,0x36}, {0x46,0x49,0x49,0x29,0x1E}, {0x00,0x00,0x14,0x00,0x00}, {0x00,0x40,0x34,0x00,0x00},   // 8 9 : ;
  {0x00,0x08,0x14,0x22,0x41}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x59,0x09,0x06},   // < = > ?
  {0x3E,0x41,0x5D,0x59,0x4E}, {0x7C,0x12,0x11,0x12,0x7C}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},   // @ A B C
  {0x7F,0x41,0x41,0x41,0x3E}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x41,0x51,0x73},   // D E F G
  {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},   // H I J K
  {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x1C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},   // L M N O
  {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x26,0x49,0x49,0x49,0x32},   // P Q R S
  {0x03,0x01,0x7F,0x01,0x03}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},   // T U V W
  {0x63,0x14,0x08,0x14,0x63}, {0x03,0x04,0x78,0x04,0x03}, {0x61,0x59,0x49,0x4D,0x43},                               // X Y Z
};
// esc 1: 6 px por letra · esc 2: 11 px por letra (letras de 10x14)
static inline int avanceLetra(int esc) { return esc > 1 ? 6 * esc - 1 : 6; }
int anchoTexto(const char *s, int esc = 1) { int n = (int)strlen(s); return n ? n * avanceLetra(esc) - esc : 0; }
void texto(int x, int y, const char *s, bool v = true, int esc = 1) {
  for (; *s; s++) {
    int c = (unsigned char)*s;
    if (c >= 'a' && c <= 'z') c -= 32;
    const uint8_t *g = (c >= 32 && c <= 'Z') ? FUENTE[c - 32] : FUENTE[0];
    for (int col = 0; col < 5; col++)
      for (int f = 0; f < 8; f++)
        if (g[col] >> f & 1) rectO(x + col * esc, y + f * esc, esc, esc, v);
    x += avanceLetra(esc);
  }
}
inline void textoDerecha(int xDer, int y, const char *s, bool v = true) { texto(xDer - anchoTexto(s) + 1, y, s, v); }

// ─── EL MONITOR ─────────────────────────────────────────────
const char *ETIQUETA_POT[NUM_PANELES][4] = {
  { "VOL", "TEMPO",  "SWING",  "FILTRO" },
  { "VOL", "FILTRO", "LARGO",  "DENS"   },
  { "VOL", "FILTRO", "ATAQUE", "CAIDA"  },
  { "VOL", "FILTRO", "ATAQUE", "CAIDA"  },
  { "VOL", "FILTRO", "ENVOLV", "ECO"    },
};
void formatoTiempo(char *b, float s) {
  if (s < 1.0f) snprintf(b, 12, "%dMS", (int)(s * 1000.0f + 0.5f));
  else snprintf(b, 12, "%.1fS", (double)s);
}
// El valor de cada pot en la unidad que de verdad mueve (las mismas fórmulas que lee el audio)
void valorPot(char *b, int p, int i, float v) {
  const int pc = (int)(v * 100.0f + 0.5f);
  switch (p) {
    case P_GENERAL:
      if (i == 1)      snprintf(b, 12, "%d BPM", (int)(BPM_MIN + BPM_RANGO * v + 0.5f));
      else if (i == 2) snprintf(b, 12, "%d%%", (int)(100.0f * (0.5f + (SWING_MAX - 0.5f) * v) + 0.5f));
      else             snprintf(b, 12, "%d%%", pc);
      return;
    case P_DRUM:
      if (i == 2) snprintf(b, 12, "X%.1f", (double)(0.5f + v)); else snprintf(b, 12, "%d%%", pc);
      return;
    case P_BAJO:
      if (i == 2)      formatoTiempo(b, BAJO_ATK_MIN * powf(BAJO_ATK_RANGO, v));
      else if (i == 3) formatoTiempo(b, BAJO_DEC_MIN * powf(BAJO_DEC_RANGO, v));
      else             snprintf(b, 12, "%d%%", pc);
      return;
    case P_MEL:
      if (i == 1)      snprintf(b, 12, "X%.2f", (double)exp2f((v - 0.5f) * 4.0f));
      else if (i == 2) formatoTiempo(b, MEL_ATK_MIN * powf(MEL_ATK_RANGO, v));
      else if (i == 3) formatoTiempo(b, MEL_DEC_MIN * powf(MEL_DEC_RANGO, v));
      else             snprintf(b, 12, "%d%%", pc);
      return;
    default:
      if (i == 2) formatoTiempo(b, PAD_ATK_MIN * powf(PAD_ATK_RANGO, v)); else snprintf(b, 12, "%d%%", pc);
      return;
  }
}
// Lo que dicen BTN2 y BTN3 en cada panel: qué hacen y qué hay puesto ahora
void lineasBotones(int p, const char **e2, char *v2, const char **e3, char *v3) {
  v2[0] = v3[0] = 0;
  switch (p) {
    case P_GENERAL: *e2 = "DIALECTO"; snprintf(v2, 16, "%s", ESTILOS[reqC.estilo].nombre);
                    *e3 = "FILL";     if (fillPub) snprintf(v3, 16, "SONANDO"); break;
    case P_DRUM:    *e2 = "PATRON";   snprintf(v2, 16, "%d/%d", reqC.patDrum + 1, NUM_PAT);
                    *e3 = "KIT";      snprintf(v3, 16, "%s", KITS[reqC.kit].nombre); break;
    case P_BAJO:    *e2 = "PATRON";   snprintf(v2, 16, "%d/%d", reqC.patBajo + 1, NUM_PAT);
                    *e3 = "TIMBRE";   snprintf(v3, 16, "%s", TBAJO[reqC.tBajo].nombre); break;
    case P_MEL:     *e2 = "PATRON";   snprintf(v2, 16, "%d/%d", reqC.patMel + 1, NUM_PAT);
                    *e3 = "INSTR";    snprintf(v3, 16, "%s", TMEL[reqC.tMel].nombre); break;
    default:        *e2 = "TIMBRE";   snprintf(v2, 16, "%s", TPAD[reqC.tPad].nombre);
                    *e3 = "REGISTRO"; snprintf(v3, 16, "%s", REGISTRO_PAD[reqC.octPad % 3]); break;
  }
}

void renderOled() {
  static unsigned long ultimo = 0;
  const unsigned long t = millis();
  if (t - ultimo < OLED_REFRESH_MS) return;
  ultimo = t;
  memset(fb, 0, sizeof(fb));
  const int p = panelPub;
  char b[24];

  // ── cabecera invertida: el panel · los 5 paneles · su estado ──
  rectO(0, 0, 128, 9, true);
  texto(2, 1, NOMBRE_PANEL[p], false);
  if (grabandoPub && ((t / 500) & 1) == 0) {         // REC: punto negro sobre la cabecera
    rectO(49, 2, 5, 5, false); rectO(48, 3, 7, 3, false); rectO(50, 1, 3, 7, false);
    if (perdidasPub) rectO(50, 3, 3, 3, true);       // hueco = la tarjeta no alcanzó y se perdió audio
  }
  for (int k = 0; k < NUM_PANELES; k++) {
    rectO(62 + k * 7, 2, 5, 5, false);
    if (k != p) rectO(63 + k * 7, 3, 3, 3, true);  // el panel actual es el cuadrito lleno
  }
  const bool on[NUM_PANELES] = { tocandoPub, reqC.drumOn, reqC.bajoOn, reqC.melOn, reqC.padOn };
  const char *estado = p == P_GENERAL ? (on[p] ? "PLAY" : "STOP") : (on[p] ? "ON" : "OFF");
  if (breakPub && ((t / 300) & 1) == 0) estado = "BRK";
  textoDerecha(126, 1, estado, false);

  // ── los 4 pots ──
  for (int i = 0; i < 4; i++) {
    const int y = 11 + i * 8;
    const float v = clamp01(pPot[p][i]);
    const bool tomado = (tomadoPub >> i) & 1;
    if (t - potMovidoMs[i] < 900) { rectO(0, y - 1, 37, 9, true); texto(1, y, ETIQUETA_POT[p][i], false); }   // el que se está moviendo
    else texto(1, y, ETIQUETA_POT[p][i]);
    marcoO(39, y, 50, 7);
    const int lleno = (int)(v * 48.0f + 0.5f);
    for (int x = 0; x < lleno; x++)
      for (int j = 0; j < 5; j++)
        if (tomado || ((x + j) & 1) == 0) pon(40 + x, y + 1 + j);     // suelto: punteado
    if (!tomado) rectO(40 + (int)(clamp01(potFisicoPub[i]) * 47.0f + 0.5f), y - 1, 1, 9, true);   // dónde está la perilla
    valorPot(b, p, i, v);
    textoDerecha(127, y, b);
  }

  // ── BTN2 y BTN3: qué hacen y qué hay puesto ──
  const char *e[2]; char val[2][16];
  lineasBotones(p, &e[0], val[0], &e[1], val[1]);
  for (int k = 0; k < 2; k++) {
    const int y = 45 + k * 8;
    rectO(0, y - 1, 7, 9, true);
    b[0] = (char)('2' + k); b[1] = 0;
    texto(1, y, b, false);
    texto(10, y, e[k]);
    textoDerecha(127, y, val[k]);
  }

  // ── los 16 pasos del compás ──
  const int paso = tocandoPub ? (int)(pasoTotalPub & 15) : -1;
  for (int s = 0; s < 16; s++) {
    if (s == paso) rectO(s * 8, 61, 7, 3, true);
    else if ((s & 3) == 0) rectO(s * 8 + 3, 62, 1, 2, true);
  }

  // ── el aviso grande de un botón (un segundo) ──
  if ((int32_t)(avisoHasta - (uint32_t)t) > 0) {
    rectO(6, 14, 116, 38, false); marcoO(6, 14, 116, 38);
    texto(64 - anchoTexto(avisoTitulo) / 2, 18, avisoTitulo);
    const int esc = anchoTexto(avisoValor, 2) <= 110 ? 2 : 1;
    texto(64 - anchoTexto(avisoValor, esc) / 2, esc == 2 ? 30 : 34, avisoValor, true, esc);
  }
  oledVolcar();
}
#endif  // CON_OLED

// ==============================================================================================================================================
// GRABADORA (core 0) — vacía el colchón a la microSD: TRIPHOP_0001.WAV, TRIPHOP_0002.WAV…
// ==============================================================================================================================================
// Al lado de cada WAV queda un .TXT con el dialecto, la escala, la progresión y el tempo con que
// arrancó la toma, para saber qué era la que salió bien.
SPIClass spiSD(FSPI);
File     archivo;
bool     abierto = false, descartando = false;
int      siguienteToma = 1;
uint32_t bytesDatos = 0, perdidosAlAbrir = 0;
unsigned long ultimaCabecera = 0;

void cabeceraWav(uint8_t *h, uint32_t datos) {      // PCM estéreo 16 bit 44.1 kHz
  const uint16_t canales = 2, bits = 16, blockAlign = canales * bits / 8;
  const uint32_t sr = SAMPLE_RATE, byteRate = sr * blockAlign;
  uint32_t v; uint16_t f = 1;
  memcpy(h, "RIFF", 4);  v = 36 + datos; memcpy(h + 4, &v, 4);
  memcpy(h + 8, "WAVEfmt ", 8);
  v = 16; memcpy(h + 16, &v, 4);
  memcpy(h + 20, &f, 2);
  memcpy(h + 22, &canales, 2);
  memcpy(h + 24, &sr, 4);
  memcpy(h + 28, &byteRate, 4);
  memcpy(h + 32, &blockAlign, 2);
  memcpy(h + 34, &bits, 2);
  memcpy(h + 36, "data", 4);
  memcpy(h + 40, &datos, 4);
}
bool actualizarCabecera() {                         // reescribe el tamaño y vuelve al final
  uint8_t h[44];
  cabeceraWav(h, bytesDatos);
  bool ok = archivo.seek(0) && archivo.write(h, 44) == 44 && archivo.seek(44 + bytesDatos);
  archivo.flush();
  return ok;
}
bool iniciarSD() {
  SD.end();
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD, SD_FREQ_HZ) || SD.cardType() == CARD_NONE) return false;
  File raiz = SD.open("/");                          // la numeración sigue lo que ya hay en la tarjeta
  for (File f = raiz.openNextFile(); f; f = raiz.openNextFile()) {
    int num;
    if (sscanf(f.name(), "TRIPHOP_%d", &num) == 1 && num >= siguienteToma) siguienteToma = num + 1;
    f.close();
  }
  raiz.close();
  return true;
}
bool abrirToma() {
  char nombre[32];
  snprintf(nombre, sizeof(nombre), "/TRIPHOP_%04d.TXT", siguienteToma);
  File nota = SD.open(nombre, FILE_WRITE);
  if (nota) {
    const Progresion &Pr = PROGS[reqC.prog];
    nota.printf("dialecto: %s\nescala: %s\nprogresion: %s\ntempo al empezar: %.0f BPM\n",
                ESTILOS[reqC.estilo].nombre, NOMBRE_ESCALA[Pr.escala], Pr.nombre, (double)(BPM_MIN + BPM_RANGO * pPot[P_GENERAL][1]));
    nota.close();
  }
  snprintf(nombre, sizeof(nombre), "/TRIPHOP_%04d.WAV", siguienteToma);
  archivo = SD.open(nombre, FILE_WRITE);
  if (!archivo) return false;
  uint8_t h[44];
  cabeceraWav(h, 0);
  if (archivo.write(h, 44) != 44) { archivo.close(); return false; }
  bytesDatos = 0; siguienteToma++;
  ultimaCabecera = millis();
  return true;
}
void cerrarToma() { actualizarCabecera(); archivo.close(); abierto = false; grabandoPub = false; }
void errorTarjeta() { if (abierto) { archivo.close(); abierto = false; } sdLista = false; descartando = true; grabandoPub = false; }

// Un paso de la grabadora. Devuelve true si escribió algo (así no duerme mientras haya cola).
bool pasoGrabadora() {
  // 1. las marcas, en orden: abrir una toma donde empieza, cerrarla cuando se escribió hasta su fin
  uint32_t limite = bloquesEsc;
  while (marcasLeidas != marcasEsc) {
    __sync_synchronize();
    const Marca m = { marcas[marcasLeidas % NUM_MARCAS].idx, marcas[marcasLeidas % NUM_MARCAS].inicio };
    if (m.inicio) {
      if (!sdLista) sdLista = iniciarSD();           // la tarjeta se puede meter después de encender
      bloquesLeidos = m.idx;
      abierto = sdLista && abrirToma();
      descartando = !abierto;                        // sin tarjeta, el colchón se vacía a la nada
      if (!abierto) sdLista = false;
      perdidosAlAbrir = bloquesPerdidos; perdidasPub = false; grabandoPub = abierto;
      marcasLeidas = marcasLeidas + 1;
      continue;
    }
    if ((int32_t)(m.idx - bloquesLeidos) > 0) { limite = m.idx; break; }   // falta escribir hasta su fin
    if (abierto) cerrarToma();
    descartando = false;
    marcasLeidas = marcasLeidas + 1;
  }
  // 2. vaciar el colchón hasta el límite
  uint32_t disp = limite - bloquesLeidos;
  if (disp == 0 || (int32_t)disp < 0) return false;
  if (!abierto) { bloquesLeidos = limite; return false; }   // descartando (o nada abierto): se tira
  uint32_t pos = bloquesLeidos % ringBloques, n = disp;
  if (n > BLOQUES_POR_ESCRITURA) n = BLOQUES_POR_ESCRITURA;
  if (n > ringBloques - pos) n = ringBloques - pos;          // una escritura no cruza el final del anillo
  const size_t b = (size_t)n * BYTES_BLOQUE;
  if (archivo.write(ring + (size_t)pos * BYTES_BLOQUE, b) != b) { errorTarjeta(); return false; }   // llena o sacada
  bytesDatos += b;
  bloquesLeidos = bloquesLeidos + n;
  if (bloquesPerdidos != perdidosAlAbrir) perdidasPub = true;
  if (bytesDatos > LIMITE_DATOS) { cerrarToma(); descartando = true; }   // tope de FAT32: se cierra sola
  else if (millis() - ultimaCabecera >= CABECERA_CADA_MS) { ultimaCabecera = millis(); actualizarCabecera(); }
  return true;
}

// ==============================================================================================================================================
// CONTROLES (tarea de control, core 0, 1 kHz)
// ==============================================================================================================================================
bool bNivel[5] = { HIGH, HIGH, HIGH, HIGH, HIGH };
unsigned long bTiempo[5];
Ctl  ctl;                           // lo que mandan los botones
int  panel = P_GENERAL;

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
void soltarPots() { for (int i = 0; i < 4; i++) { potEntrada[i] = readPot(POT_PIN[i]); potTomado[i] = false; } tomadoPub = 0; }
float potRef[4];                    // la última posición que contó como "movido", para marcarlo en el monitor

// El aviso grande del monitor: qué hizo el botón recién apretado
void avisar(const char *titulo, const char *valor) {
  snprintf(avisoTitulo, sizeof(avisoTitulo), "%s", titulo);
  snprintf(avisoValor, sizeof(avisoValor), "%s", valor);
  avisoHasta = millis() + 1100;
}
void avisarN(const char *titulo, int n, int total, const char *valor) {
  char t[24]; snprintf(t, sizeof(t), "%s %d/%d", titulo, n + 1, total); avisar(t, valor);
}

void publicarCtl() { memcpy((void *)&reqC, &ctl, sizeof(ctl)); }

void aplicarEstilo(int e) {         // el dialecto trae sus patrones y sus timbres; después se cambian en cada panel
  const Estilo &E = ESTILOS[e];
  ctl.estilo = e;
  ctl.patDrum = 0; ctl.patBajo = 0; ctl.patMel = 0;
  ctl.kit = E.kit; ctl.tBajo = E.tBajo; ctl.tMel = E.tMel; ctl.tPad = E.tPad;
}

// Cada cambio de dialecto (BTN2 del General) trae otra progresión, nunca la que ya sonaba. Play/Stop
// NO sortea: así un Stop y un Play vuelven a lo mismo, que es lo que hace falta para grabar una
// sesión otra vez (antes sorteaba en cada Play y el usuario no podía volver a lo que tenía). La
// entropía es el instante exacto en que se apretó el botón (la regla de espacio_modular).
void sortearProgresion() {
  uint32_t h = (uint32_t)micros() * 2654435761u; h ^= h >> 15;
  ctl.prog = (ctl.prog + 1 + (int)(h % (uint32_t)(NUM_PROG - 1))) % NUM_PROG;
}

void cambiarPanel(int p) {
  panel = p; panelPub = p; panelCambioMs = millis();
  soltarPots();
}

// Un botón = una función, en el flanco de presión. En TODOS los paneles BTN4 pasa al siguiente panel
// y BTN5 entra o sale del break; BTN1-BTN3 son del panel.
void accionBoton(int b) {
  char n[16];
  if (b == 3) {
    cambiarPanel((panel + 1) % NUM_PANELES);
    avisar("PANEL", NOMBRE_PANEL[panel]);
  } else if (b == 4) {
    ctl.brk = !ctl.brk;
    avisar("BREAK", ctl.brk ? "ADENTRO" : "AFUERA");
  } else switch (panel) {
    case P_GENERAL:
      if (b == 0) {
        ctl.tocar = !ctl.tocar; ctl.brk = false;
        if (ctl.tocar) avisar(sdLista ? NOMBRE_ESCALA[PROGS[ctl.prog].escala] : "SIN MICROSD", PROGS[ctl.prog].nombre);   // Play NO cambia nada: vuelve a lo mismo
        else avisar("TRANSPORTE", "STOP");
      }
      else if (b == 1) {                           // dialecto nuevo Y progresión nueva (sorteada, nunca la que sonaba)
        aplicarEstilo((ctl.estilo + 1) % NUM_ESTILOS); sortearProgresion();
        char t[24]; snprintf(t, sizeof(t), "%s %d/%d", ESTILOS[ctl.estilo].nombre, ctl.estilo + 1, NUM_ESTILOS);
        avisar(t, PROGS[ctl.prog].nombre);
      }
      else             { reqFill = reqFill + 1; avisar("FILL", "AL FINAL"); }
      break;
    case P_DRUM:
      if (b == 0)      { ctl.drumOn = !ctl.drumOn; avisar("BATERIA", ctl.drumOn ? "SUENA" : "CALLADA"); }
      else if (b == 1) { ctl.patDrum = (ctl.patDrum + 1) % NUM_PAT; snprintf(n, sizeof(n), "%d DE %d", ctl.patDrum + 1, NUM_PAT); avisar("PATRON", n); }
      else             { ctl.kit = (ctl.kit + 1) % NUM_KITS; avisarN("KIT", ctl.kit, NUM_KITS, KITS[ctl.kit].nombre); }
      break;
    case P_BAJO:
      if (b == 0)      { ctl.bajoOn = !ctl.bajoOn; avisar("BAJO", ctl.bajoOn ? "SUENA" : "CALLADO"); }
      else if (b == 1) { ctl.patBajo = (ctl.patBajo + 1) % NUM_PAT; snprintf(n, sizeof(n), "%d DE %d", ctl.patBajo + 1, NUM_PAT); avisar("PATRON", n); }
      else             { ctl.tBajo = (ctl.tBajo + 1) % NUM_TBAJO; avisarN("TIMBRE", ctl.tBajo, NUM_TBAJO, TBAJO[ctl.tBajo].nombre); }
      break;
    case P_MEL:
      if (b == 0)      { ctl.melOn = !ctl.melOn; avisar("ARMONIA", ctl.melOn ? "SUENA" : "CALLADA"); }
      else if (b == 1) { ctl.patMel = (ctl.patMel + 1) % NUM_PAT; snprintf(n, sizeof(n), "%d DE %d", ctl.patMel + 1, NUM_PAT); avisar("PATRON", n); }
      else             { ctl.tMel = (ctl.tMel + 1) % NUM_TMEL; avisarN("INSTRUMENTO", ctl.tMel, NUM_TMEL, TMEL[ctl.tMel].nombre); }
      break;
    case P_PAD:
      if (b == 0)      { ctl.padOn = !ctl.padOn; avisar("PAD", ctl.padOn ? "SUENA" : "CALLADO"); }
      else if (b == 1) { ctl.tPad = (ctl.tPad + 1) % NUM_TPAD; avisarN("TIMBRE", ctl.tPad, NUM_TPAD, TPAD[ctl.tPad].nombre); }
      else             { ctl.octPad = (ctl.octPad + 1) % 3; avisar("REGISTRO", REGISTRO_PAD[ctl.octPad]); }   // normal -> +1 -> -1
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
  static uint8_t scan = 0;
  int i = scan; scan = (scan + 1) & 3;
  float v = readPot(POT_PIN[i]);
  potFisicoPub[i] = v;
  if (!potTomado[i] && fabsf(v - potEntrada[i]) > UMBRAL_TOMA) { potTomado[i] = true; tomadoPub = tomadoPub | (1 << i); potRef[i] = v; potMovidoMs[i] = ahora; }
  if (potTomado[i]) {
    pPot[panel][i] += (v - pPot[panel][i]) * 0.3f;
    if (fabsf(v - potRef[i]) > 0.010f) { potRef[i] = v; potMovidoMs[i] = ahora; }   // más que el ruido del ADC
  }
}

// ─── USB: líneas para las proyecciones ───────────────────────
void enviarLineas(unsigned long t) {
  char b[400];
#if LINEA_RESONANCIA
  static unsigned long ultInfo = 0; static int estiloInfo = -1;
  if (t - ultInfo > 5000 || estiloInfo != reqC.estilo) {
    ultInfo = t; estiloInfo = reqC.estilo;
    snprintf(b, sizeof(b), "I,%s,PercuSynth Trip Hop Viaje,%.2f,0,%d,%d,bateria|bajo|armonia|pad\n",
             ESTILOS[reqC.estilo].nombre, (double)bpmPub, reqC.estilo + 1, NUM_ESTILOS);
    Serial.print(b);
  }
#endif
  const uint32_t pt = pasoTotalPub, ms = muestraPub / (SAMPLE_RATE / 1000);
  const float fase = fasePub;
  const uint32_t cDrum = golpes[I_HAT] + golpes[I_CAJA] + golpes[I_PERC] + golpes[I_CLAP] + golpes[I_ABIERTO] + golpes[I_TOM] + golpes[EV_BOMBO];
  const uint32_t cMel = golpes[EV_MEL] + golpes[EV_ARP];
  const bool on[4] = { reqC.drumOn, reqC.bajoOn, reqC.melOn, reqC.padOn };
  const int volI[4] = { (int)(pPot[P_DRUM][0] * 1000), (int)(pPot[P_BAJO][0] * 1000), (int)(pPot[P_MEL][0] * 1000), (int)(pPot[P_PAD][0] * 1000) };
  const int filI[4] = { (int)(pPot[P_DRUM][1] * 500), (int)(pPot[P_BAJO][1] * 500), (int)(pPot[P_MEL][1] * 500), (int)(pPot[P_PAD][1] * 500) };   // Resonancia: 0.5 = abierto
  int niv[5]; for (int k = 0; k < 5; k++) { float n = nivelPub[k] * 2.0f; niv[k] = (int)((n > 1 ? 1 : n) * 1000); }
  int btn = 0; for (int k = 0; k < 5; k++) if (bNivel[k] == LOW) btn |= 1 << k;
  int tom = 0; for (int k = 0; k < 4; k++) if (potTomado[k]) tom |= 1 << k;
#if LINEA_RESONANCIA
  // R: el formato de 43 campos de mezclador_pistas / trance_pistas. Pistas: 0 batería · 1 bajo · 2 armonía · 3 pad
  snprintf(b, sizeof(b), "R,%lu,%d,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
    (unsigned long)ms, (int)(fase * 1000), (unsigned long)(pt >> 2), tocandoPub ? 1 : 0,
    on[0] ? volI[0] : 0, on[1] ? volI[1] : 0, on[2] ? volI[2] : 0, on[3] ? volI[3] : 0, filI[0], filI[1], filI[2], filI[3],
    niv[0], niv[1], niv[2], niv[3],
    (unsigned long)cDrum, (unsigned long)golpes[EV_BAJO], (unsigned long)cMel, (unsigned long)golpes[EV_PAD],
    (unsigned long)golpes[EV_BOMBO], (unsigned long)(golpes[I_CAJA] + golpes[I_CLAP]), (unsigned long)(golpes[I_HAT] + golpes[I_ABIERTO]),
    0UL, 0, 0, 0, btn, panel, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, tom);
  Serial.print(b);
#endif
#if LINEA_PROYECCION
  // P: todo lo de cada instrumento. Los contadores sólo suben: una línea perdida no pierde golpes.
  int mute = (reqC.drumOn ? 0 : 1) | (reqC.kick ? 0 : 2) | (reqC.bajoOn ? 0 : 4) | (reqC.melOn ? 0 : 8) | (reqC.padOn ? 0 : 16);
  snprintf(b, sizeof(b), "P,%lu,%d,%d,%d,%lu,%d,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
    (unsigned long)ms, reqC.estilo, panel, tocandoPub ? 1 : 0, (unsigned long)pt, (int)(bpmPub * 10), acordePub, tonicaPub,
    (unsigned long)golpes[EV_BOMBO], (unsigned long)golpes[I_HAT], (unsigned long)golpes[I_CAJA], (unsigned long)golpes[I_PERC],
    (unsigned long)golpes[I_CLAP], (unsigned long)golpes[I_ABIERTO], (unsigned long)golpes[I_TOM],
    (unsigned long)golpes[EV_BAJO], (unsigned long)golpes[EV_MEL], (unsigned long)golpes[EV_ARP], (unsigned long)golpes[EV_PAD],
    notaBajoPub, notaMelPub, notaArpPub, 0,
    niv[0], niv[1], niv[2], niv[3], niv[4], (int)(envBajoPub * 1000),
    mute, breakPub ? 1 : 0, fillPub ? 1 : 0, 0,
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
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;   // ~12 ms de cola: los botones suenan en el acto
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
void tareaGrabadora(void *);
#if CON_OLED
void oledTask(void *);
#endif
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
  semilla ^= (uint32_t)micros();
  rngComp ^= semilla; rngLed ^= semilla * 2654435761u;

  memset(&ctl, 0, sizeof(ctl));
  ctl.drumOn = ctl.bajoOn = ctl.melOn = ctl.padOn = true;
  ctl.kick = true; ctl.arp = true;
  aplicarEstilo(0);
  ctl.prog = (int)(semilla % (uint32_t)NUM_PROG);   // la del arranque, al azar; después sólo la cambia el dialecto
  publicarCtl();
  for (int p = 0; p < 4; p++) pPot[P_GENERAL][p] = readPot(POT_PIN[p]);   // el General ya en su lugar al arrancar
  lpfS = pPot[P_GENERAL][3];
  sortearEfecto();

#ifdef SIMULADOR
  iniciarLEDs();
#if CON_OLED
  oledOk = oledIniciar();
#endif
#endif

  i2s_init();
  // Colchón de grabación: 8 s en PSRAM si la hay; si no, 0.4 s en RAM interna
  ringBloques = (uint32_t)(RING_SEG_PSRAM * SAMPLE_RATE / BUFFER_SAMPLES);
  ring = (uint8_t *)heap_caps_malloc((size_t)ringBloques * BYTES_BLOQUE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ring) {
    ringBloques = (uint32_t)(RING_SEG_INTERNA * SAMPLE_RATE / BUFFER_SAMPLES);
    ring = (uint8_t *)heap_caps_malloc((size_t)ringBloques * BYTES_BLOQUE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
#ifdef SIMULADOR
  sdLista = iniciarSD();
#endif
#if MEDIR
  Serial0.begin(115200);
  ctl.tocar = true; publicarCtl();   // arranca tocando, para medir el peor caso
#endif

#ifndef SIMULADOR
  // El audio tiene el core 1 para él solo; controles, USB, matriz y pantalla van al core 0.
  xTaskCreatePinnedToCore(audioTask,   "audio",   8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask, "control", 6144, NULL,  3, NULL, 0);
  xTaskCreatePinnedToCore(ledTask,     "leds",    4096, NULL,  2, NULL, 0);
  xTaskCreatePinnedToCore(tareaGrabadora, "grabadora", 6144, NULL, 2, NULL, 0);
#if CON_OLED
  xTaskCreatePinnedToCore(oledTask,    "oled",    4096, NULL,  1, NULL, 0);
#endif
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
  pasoGrabadora();
#if CON_OLED
  if (oledOk) renderOled();
#endif
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
      Serial0.printf("CPU audio: prom %.0f %%  max %.0f %%  | pico antes del limitador %.2f  ganancia min %.2f  muestras en techo %lu | BPM %.0f  %s\n",
        100.0 * (double)medSuma * 64.0 / n / CICLOS_BLOQUE, 100.0 * (double)medMax / CICLOS_BLOQUE,
        (double)medPico, (double)medLimMin, (unsigned long)medTecho, (double)bpmPub, ESTILOS[reqC.estilo].nombre);
      Serial0.printf("   por sección (%% del presupuesto): bateria %.0f  bajo %.0f  armonia %.0f  pad %.0f  master %.0f  pasos %.0f
",
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
// núcleo que lo inicializó, y hacerlo en setup() (que corre en el core 1) le robaba al audio hasta
// un 30 % de un bloque — medido en industrial_matriz.
void ledTask(void *) { iniciarLEDs(); for (;;) { renderLEDs(); vTaskDelay(2); } }
// La grabadora: la tarjeta se monta acá, en su tarea, y se vuelve a probar en cada Play
void tareaGrabadora(void *) {
  sdLista = iniciarSD();
  for (;;) { if (!pasoGrabadora()) vTaskDelay(2); }
}
#if CON_OLED
// La pantalla también se inicia en su tarea, y es la de menor prioridad: un cuadro entero son ~12 ms
// de I2C, y si llegara a atrasarse lo único que pasa es que el viaje va a menos cuadros por segundo.
void oledTask(void *) {
  oledOk = oledIniciar();
  for (;;) { if (oledOk) renderOled(); vTaskDelay(5); }
}
#endif
void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }
#endif
