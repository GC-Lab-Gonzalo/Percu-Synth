// ==============================================================================================================================================
// PERCU-SYNTH — LA PARTIDA (Víctor Jara) · reproductor de 3 pistas MIDI con síntesis propia — GC Lab Chile
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
// - 6 LEDs WS2812 SMD internos de la placa |DATA -> 46| (indicadores de estado)
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
// - FastLED (gestor de librerías de Arduino) — para los 6 LEDs de placa
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Toca "La partida" de Víctor Jara desde las 3 pistas MIDI del arreglo (en cancion.h), cada
// una con su propio motor de síntesis:
//
//   · MELODÍA  → LEAD: dos sierras PolyBLEP desafinadas + cuadrada, filtro pasa-bajos
//                resonante (SVF) con envolvente de filtro, glide corto y vibrato con
//                entrada retardada (el vibrato aparece después del ataque, como un cantante).
//   · BAJO     → una octava abajo del archivo, y las notas que igual quedan sobre La2 bajan
//                otra octava (toda la línea en La1..La2). Sierra + cuadrada con un leve desafine
//                entre ellas (±4 cents): el batido lento de las dos llena de armónicos.
//                Saturación suave → SVF pasa-bajos.
//   · ARPEGIO  → PLUCK ELECTRÓNICO: sierra + pulso 25 % levemente desafinados, cada nota con
//                su propio filtro pasa-bajos que se abre de golpe y se cierra con la cola
//                (el arpegio clásico de sinte). El filtro sigue a la altura de la nota, así que
//                las colas quedan oscuras y afinadas: con decay largo se suman sin ensuciar.
//
// Los tiempos salen del archivo tal cual: 480 ticks por negra, 80 BPM, y la posición de la
// canción se cuenta con un acumulador de 64 bits (32.32) que avanza una vez por MUESTRA.
// Cada nota cae en la muestra exacta que le corresponde, sin deriva a lo largo del tema.
//
// SIN CLICKS, por diseño:
//   · Ninguna fase se reinicia nunca y ninguna envolvente salta: los ataques suben desde el
//     nivel en que esté la envolvente (una nota encima de otra no tiene escalón).
//   · El arpegio tiene UNA voz por altura: volver a tocar la misma nota re-dispara esa voz
//     desde donde está (amplitud y filtro suben desde su nivel actual). Nunca se roba una
//     voz, y una voz se apaga en dos etapas (a −60 dB pasa a un cierre de 1.5 ms y recién
//     se libera a −82 dB), nunca cortando la onda.
//   · Parámetros suavizados por muestra (12 ms), coeficientes de filtro cada 8 muestras con
//     un SVF de Zavalishin (estable aunque el corte se mueva rápido).
//   · La resonancia va atada al corte (no hay pico resonante en los agudos).
//   · Stop = todas las voces se cierran con un tau de 40 ms, nunca en seco.
//   · Bloqueador de continua + limitador con ganancia suavizada + saturación suave final.
//   · Audio en su propia tarea en el CORE 1; botones, pots y LEDs en el CORE 0 a 1 kHz.
//     Sin Serial (un print por USB CDC bloquea y vacía el DMA → chasquido).
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// - BTN1 → PLAY / STOP (Play parte siempre desde el comienzo) y PANEL GENERAL
//          POT1 volumen general · POT2 velocidad de la canción (×0.5 … ×2; al centro
//          hay una zona muerta que deja EXACTAMENTE los 80 BPM originales)
// - BTN2 → PANEL MELODÍA : POT1 volumen · POT2 vibrato · POT3 filtro LPF · POT4 resonancia
// - BTN3 → PANEL BAJO    : POT1 volumen · POT2 filtro LPF · POT3 resonancia
// - BTN4 → PANEL ARPEGIO : POT1 volumen · POT2 ataque (3–300 ms) · POT3 decay (0.08–1.2 s)
// - BTN5 → sin función
//
// Pots por panel: al entrar a un panel ningún pot cambia nada hasta que lo MUEVES (~5 %).
// Desde ese momento la posición física del pot es el valor. Así cambiar de panel nunca
// pisa lo que dejaste en otro.
//
// Al terminar la canción se detiene sola y deja sonar las colas (REPETIR_CANCION = true
// para que vuelva a empezar).
//
// LEDs de la placa:
//   0 = nota de la melodía · 1 = nota del bajo · 2 = nota del arpegio (destellos al sonar)
//   3 = pulso de cada negra (más fuerte en el 1 del compás)
//   4 = panel activo (general blanco · melodía cian · bajo violeta · arpegio naranjo)
//   5 = verde tocando · rojo tenue detenido · MAGENTA = sobrecarga de CPU (un buffer tardó
//       más del 80 % de su tiempo: si aparece junto con ruido, el problema es de tiempo)
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <math.h>

// ─── Tipos (arriba del todo para que el IDE de Arduino genere bien los prototipos) ───
struct NotaMidi { uint32_t inicio, fin; uint8_t nota; };
struct EstadoSVF { float ic1, ic2; };
struct CoefSVF { float a1, a2, a3, comp; };
struct VozArp {
  bool  activa;
  bool  atacando;
  bool  cerrando;        // pasó el umbral de −60 dB: cierre de 1.5 ms antes de liberarse
  float fase[2];
  float dt[2];           // incremento de fase: sierra y pulso
  float env, pasoAtk;    // amplitud y cuánto sube por muestra en el ataque
  float envF, pasoAtkF;  // envolvente del filtro (sube con el mismo ataque)
  int   muestrasAtaque;  // muestras de ataque que faltan
  float corteBase;       // piso del filtro: sigue a la altura de la nota
  EstadoSVF svf;
  CoefSVF   c;
  float panL, panR;
};

#include "cancion.h"

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
const float INV_SR = 1.0f / (float)SAMPLE_RATE;

// ─── LEDs WS2812 (sólo los 6 SMD de la placa) ──────────────
#define LED_PIN      46
#define NUM_LEDS      6
#define LED_BRIGHT  120
#define LED_TYPE     WS2812
#define COLOR_ORDER  GRB
const unsigned long LED_REFRESH_MS = 20;
CRGB leds[NUM_LEDS];

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
#define BTN1_PIN   44
#define BTN2_PIN   42
#define BTN3_PIN    0
#define BTN4_PIN   45
#define BTN5_PIN   47
const unsigned long DEBOUNCE_MS = 150;
const uint8_t BTN_PIN[5] = { BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN, BTN5_PIN };

// ─── Potenciómetros ────────────────────────────────────────
#define POT1   1    // ADC1
#define POT2   2    // ADC2
#define POT3   8    // ADC8
#define POT4  10    // ADC10
const uint8_t POT_PIN[4] = { POT1, POT2, POT3, POT4 };

// ==============================================================================================================================================
// AJUSTES FIJOS — lo que no está en un pot se ajusta acá
// ==============================================================================================================================================
const bool REPETIR_CANCION = false;   // true = al terminar vuelve a empezar

// Transposición por pista (semitonos). 0 = las notas exactas del archivo.
const int TRANSP_MELODIA = 0;
const int TRANSP_BAJO    = -12;   // una octava abajo del archivo
const int BAJO_NOTA_MAX  = 45;    // La2: una nota del bajo que (ya transpuesta) quede más arriba
                                  // baja otra octava. Así toda la línea vive en La1..La2
const int TRANSP_ARPEGIO = 0;

// Mezcla (antes de los pots de volumen)
const float NIVEL_MELODIA = 0.30f;
const float NIVEL_BAJO    = 0.36f;
const float NIVEL_ARPEGIO = 0.30f;
const float MASTER        = 1.50f;

// ─── Lead (melodía) ───
const float LEAD_DESAFINE_CENTS = 7.0f;    // segunda sierra
const float LEAD_NIVEL_CUADRADA = 0.35f;
const float LEAD_ATAQUE_S       = 0.006f;
const float LEAD_SOSTEN         = 0.80f;   // nivel al que baja después del ataque
const float LEAD_TAU_SOSTEN     = 0.35f;
const float LEAD_TAU_SOLTAR     = 0.09f;
const float LEAD_GLIDE_S        = 0.018f;  // portamento corto entre notas ligadas
const float LEAD_ENV_FILTRO_OCT = 1.6f;    // cuánto abre el filtro cada nota
const float LEAD_TAU_FILTRO     = 0.28f;
const float LEAD_Q_MAX          = 6.0f;
const float VIB_HZ              = 5.3f;
const float VIB_MAX_CENTS       = 60.0f;
const float VIB_RETARDO_S       = 0.18f;   // el vibrato entra después del ataque
const float VIB_TAU_ENTRADA     = 0.30f;

// ─── Bajo ───
const float BAJO_DESAFINE_CENTS = 4.0f;    // sierra −4, cuadrada +4 → 8 cents entre ellas
const float BAJO_NIVEL_SIERRA   = 0.55f;
const float BAJO_NIVEL_CUADRADA = 0.45f;
const float BAJO_DRIVE          = 1.30f;
const float BAJO_ATAQUE_S       = 0.004f;
const float BAJO_TAU_SOLTAR     = 0.07f;
const float BAJO_ENV_FILTRO_OCT = 1.0f;    // un poco de "pluck" en cada nota
const float BAJO_TAU_FILTRO     = 0.25f;
const float BAJO_Q_MAX          = 5.0f;

// ─── Pluck electrónico (arpegio) ───
const float ARP_DESAFINE_CENTS = 6.0f;     // pulso levemente arriba de la sierra
const float ARP_NIVEL_SIERRA   = 0.60f;
const float ARP_NIVEL_PULSO    = 0.45f;    // pulso 25 %: el color "chip"/electrónico
const float ARP_CORTE_PISO     = 1.8f;     // filtro cerrado = 1.8 × la fundamental
const float ARP_ENV_OCT        = 3.8f;     // cuánto abre el filtro cada nota (octavas)
const float ARP_Q              = 1.6f;     // resonancia moderada y fija
const float ARP_TAU_FILTRO_REL = 0.25f;    // el filtro cierra 4× más rápido que el volumen
const float ARP_UMBRAL_CIERRE  = 1.0e-3f;  // −60 dB → cierre rápido
const float ARP_UMBRAL_LIBRE   = 8.0e-5f;  // −82 dB → voz libre
#define ARP_NOTA_BASE  36                  // voz 0 = Do2
#define ARP_VOCES      60                  // Do2 .. Si6: una voz por altura

// ─── Generales ───
const float TAU_STOP    = 0.040f;   // cierre de todas las voces al hacer Stop
const float FILTRO_TECHO = 12000.0f;
const float SUAVIZADO   = 1.0f - expf(-1.0f / (0.012f * SAMPLE_RATE));   // pots → 12 ms
const float LIM_UMBRAL  = 0.85f;

// ==============================================================================================================================================
// PEDIDOS ENTRE NÚCLEOS
// ==============================================================================================================================================
// control → audio : parámetros 0..1 y tocando
// audio → control : contadores para los LEDs (y tocando = false al terminar la canción)
// Son escrituras de 32 bits alineadas, atómicas en el S3: no hace falta mutex.
volatile bool  tocando = false;

volatile float pVolGeneral = 0.80f;
volatile float pVelocidad  = 0.50f;   // 0.5 = velocidad original

volatile float pVolMel  = 0.75f;
volatile float pVibrato = 0.30f;
volatile float pCorteMel = 0.55f;
volatile float pResMel  = 0.25f;

volatile float pVolBajo  = 0.75f;
volatile float pCorteBajo = 0.40f;
volatile float pResBajo  = 0.25f;

volatile float pVolArp    = 0.70f;
volatile float pAtaqueArp = 0.10f;
volatile float pDecayArp  = 0.45f;

volatile uint32_t contMel = 0, contBajo = 0, contArp = 0, contNegra = 0;
volatile int      notaMelLed = 0, notaBajoLed = 0, notaArpLed = 0;
volatile bool     negraFuerte = false;
volatile uint32_t contSobrecarga = 0;   // buffers que tardaron > 80 % del tiempo disponible

// ─── Estado de la tarea de AUDIO ───────────────────────────
bool     aTocando = false;
bool     finNatural = false;  // la canción terminó sola: las colas suenan completas
uint64_t posicion = 0;        // posición en ticks, formato 32.32
int      idxMel = 0, idxBajo = 0, idxArp = 0;
uint32_t ultimaNegra = 0xFFFFFFFF;

// Lead
bool  melSonando = false;  uint32_t melFin = 0;
float melFrec = 440.0f, melFrecObj = 440.0f;
float melFase[3] = { 0.0f, 0.31f, 0.67f };
float melEnv = 0.0f;  int melEtapa = 0;          // 0 reposo/soltar · 1 ataque · 2 sostén
float melEnvF = 0.0f;
float vibFase = 0.0f, vibNivel = 0.0f;  uint32_t melMuestrasNota = 0;
EstadoSVF svfMel = { 0, 0 };  CoefSVF cMel = { 1, 0, 0, 1 };

// Bajo
bool  bajoSonando = false;  uint32_t bajoFin = 0;
float bajoFrec = 110.0f;
float bajoFase[2] = { 0.0f, 0.43f };
float bajoEnv = 0.0f;  bool bajoAtacando = false;
float bajoEnvF = 0.0f;
EstadoSVF svfBajo = { 0, 0 };  CoefSVF cBajo = { 1, 0, 0, 1 };

// Campanitas
VozArp arp[ARP_VOCES];
float arpMulAmp = 0.9999f, arpMulFiltro = 0.999f, arpMulCierre = 0.98f;
float arpComp = 1.0f;                  // 1 / ARP_Q^0.35, fijo (el Q del arpegio no cambia)

// Parámetros suavizados
float sVolGen = 0.8f, sVolMel = 0.75f, sVolBajo = 0.75f, sVolArp = 0.7f;
float sCorteMel = 0.55f, sResMel = 0.25f, sCorteBajo = 0.4f, sResBajo = 0.25f, sVib = 0.3f;

// Salida
float dcXL = 0, dcYL = 0, dcXR = 0, dcYR = 0;
float limGan = 1.0f, limAtk = 0.0f, limRel = 0.0f;

// Constantes precalculadas en setup()
float mulMelSosten, mulMelSoltar, mulMelFiltro, mulStop, glideK;
float mulBajoSoltar, mulBajoFiltro;
float vibSubir, vibBajar;
float razonMel2, razonBajoSierra, razonBajoCuad, razonArp;

#define TABLA_SENO 1024
float tablaSeno[TABLA_SENO + 1];

// ─── Estado de la tarea de CONTROL ─────────────────────────
#define PANEL_GENERAL  0
#define PANEL_MELODIA  1
#define PANEL_BAJO     2
#define PANEL_ARPEGIO  3
int   panel = PANEL_GENERAL;
float potEntrada[4];
bool  potTomado[4];
const float UMBRAL_TOMA = 0.05f;

bool bNivel[5] = { HIGH, HIGH, HIGH, HIGH, HIGH };
unsigned long bTiempo[5] = { 0, 0, 0, 0, 0 };
float flashNivel = 0.0f;

static i2s_chan_handle_t tx_chan;

// ─── Lectura de pot con sobre-muestreo (core 0) ────────────
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) sum += analogRead(pin);
  return (float)(sum >> 2) / 4095.0f;
}

// ==============================================================================================================================================
// SÍNTESIS — bloques básicos
// ==============================================================================================================================================
inline float mtof(int nota) { return 440.0f * powf(2.0f, (float)(nota - 69) / 12.0f); }

inline float polyBlep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  else if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}
inline float sierra(float fase, float dt) { return (2.0f * fase - 1.0f) - polyBlep(fase, dt); }
inline float cuadrada(float fase, float dt) {
  float p2 = fase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
  return (sierra(fase, dt) - sierra(p2, dt)) * 0.5f;
}
inline float seno(float fase) {               // fase 0..1, tabla con interpolación lineal
  float x = fase * (float)TABLA_SENO;
  int i = (int)x;
  float f = x - (float)i;
  return tablaSeno[i] + (tablaSeno[i + 1] - tablaSeno[i]) * f;
}
inline float softClip(float x) {
  if (x >  3.0f) x =  3.0f;
  if (x < -3.0f) x = -3.0f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}
inline void avanzar(float &fase, float dt) { fase += dt; if (fase >= 1.0f) fase -= 1.0f; }

// ─── Matemática rápida para el render ─────────────────────
// tanf/powf de la libm cuestan cientos de ciclos en el S3. Llamadas por cada voz cada pocas
// muestras, con decay largo (muchas voces sonando) el render no alcanzaba a entregar el
// buffer a tiempo → el DMA se vaciaba y salían ceros = ruido. Estas versiones cuestan
// unas pocas multiplicaciones y su error (< 0.1 %) no se oye en un corte de filtro.
union FloatBits { float f; int32_t i; };

inline float exp2Rapido(float x) {            // 2^x
  if (x < -30.0f) x = -30.0f;
  if (x >  30.0f) x =  30.0f;
  int32_t e = (int32_t)x;                     // piso sin llamar a la libm
  if ((float)e > x) e--;
  float f = x - (float)e;
  float p = 1.0f + f * (0.6951786f + f * (0.2261997f + f * 0.0781163f));   // 2^f en [0,1)
  FloatBits u; u.i = (e + 127) << 23;
  return p * u.f;
}
inline float log2Rapido(float x) {            // log2(x), x > 0
  FloatBits u; u.f = x;
  float e = (float)(((u.i >> 23) & 255) - 127);
  u.i = (u.i & 0x007FFFFF) | 0x3F800000;      // mantisa en [1, 2)
  float m = u.f;
  // log2(m) en [1, 2): polinomio de grado 4 ajustado (error < 0.0003)
  return e + (-2.49684606f + (4.02854794f + (-2.08121416f + (0.62887362f - 0.07915816f * m) * m) * m) * m);
}
inline float tanRapido(float x) {             // Padé: exacto a 0.1 % hasta x = 0.9 (12.6 kHz)
  float x2 = x * x;
  return x * (15.0f - x2) / (15.0f - 6.0f * x2);
}

// SVF de Zavalishin (TPT). comp = ganancia de compensación por la resonancia.
void calcularSVF(CoefSVF &c, float fc, float q, float comp) {
  if (fc > FILTRO_TECHO) fc = FILTRO_TECHO;
  if (fc < 20.0f) fc = 20.0f;
  float g = tanRapido((float)M_PI * fc * INV_SR);
  float k = 1.0f / q;
  c.a1 = 1.0f / (1.0f + g * (g + k));
  c.a2 = g * c.a1;
  c.a3 = g * c.a2;
  c.comp = comp;
}
inline float compQ(float q) { return exp2Rapido(-0.35f * log2Rapido(q)); }   // 1 / q^0.35
inline float filtroLP(EstadoSVF &s, const CoefSVF &c, float x) {
  float v3 = x - s.ic2;
  float v1 = c.a1 * s.ic1 + c.a2 * v3;
  float v2 = s.ic2 + c.a2 * s.ic1 + c.a3 * v3;
  s.ic1 = 2.0f * v1 - s.ic1;
  s.ic2 = 2.0f * v2 - s.ic2;
  return v2;
}

// Resonancia atada al corte: el pot da el Q, pero arriba de 3 kHz el máximo se achica
// (un pico resonante en los agudos es un pitido, no un timbre).
inline float qDe(float res, float qMax, float fc) {
  float techo = qMax;
  if (fc > 3000.0f) techo = 0.7f + (qMax - 0.7f) * (3000.0f / fc);
  return 0.7f + res * res * (techo - 0.7f);
}

// ─── Mapeos de pots (tarea de audio) ───────────────────────
inline float corteMelHz(float v)  { return 180.0f * exp2Rapido(5.78136f * v); }   // 180 Hz .. 9.9 kHz (×55)
inline float corteBajoHz(float v) { return 60.0f * exp2Rapido(6.32193f * v); }    // 60 Hz .. 4.8 kHz (×80)
inline float ataqueArpS(float v)  { return 0.003f * powf(100.0f, v); }   // 3 ms .. 300 ms (menos es un clic)
inline float decayArpS(float v)   { return 0.08f * powf(15.0f, v); }     // tau 0.08 .. 1.2 s
// Velocidad: ×0.5 .. ×2 exponencial, con zona muerta al centro = velocidad original exacta
inline float factorVelocidad(float v) {
  if (v > 0.45f && v < 0.55f) return 1.0f;
  float x = (v <= 0.45f) ? (v - 0.45f) / 0.45f : (v - 0.55f) / 0.45f;   // −1..0 / 0..1
  return powf(2.0f, x);
}

// ==============================================================================================================================================
// EVENTOS DE NOTA (tarea de audio)
// ==============================================================================================================================================
void melNotaOn(const NotaMidi &n) {
  melFrecObj = mtof(n.nota + TRANSP_MELODIA);
  if (!melSonando && melEnv < 0.001f) melFrec = melFrecObj;   // desde silencio: sin glide
  melSonando = true;
  melFin = n.fin;
  melEtapa = 1;                  // el ataque sube DESDE donde esté la envolvente
  melEnvF = 1.0f;                // re-articula el filtro (el filtro no produce escalones)
  melMuestrasNota = 0;
  notaMelLed = n.nota; contMel++;
}

void bajoNotaOn(const NotaMidi &n) {
  int nota = n.nota + TRANSP_BAJO;
  if (nota > BAJO_NOTA_MAX) nota -= 12;
  bajoFrec = mtof(nota);                   // fase continua: cambiar la altura no hace clic
  bajoSonando = true;
  bajoFin = n.fin;
  bajoAtacando = true;
  bajoEnvF = 1.0f;
  notaBajoLed = n.nota; contBajo++;
}

void arpGolpe(int nota) {
  int v = nota - ARP_NOTA_BASE;
  if (v < 0 || v >= ARP_VOCES) return;
  VozArp &a = arp[v];
  float f = mtof(nota);
  int nA = (int)(ataqueArpS(pAtaqueArp) * SAMPLE_RATE);
  if (nA < 1) nA = 1;
  a.dt[0] = f * INV_SR;
  a.dt[1] = a.dt[0] * razonArp;
  a.corteBase = f * ARP_CORTE_PISO;
  // Amplitud y filtro suben LINEALMENTE desde donde estén: re-disparar no tiene escalón.
  a.pasoAtk  = (1.0f - a.env)  / (float)nA;
  a.pasoAtkF = (1.0f - a.envF) / (float)nA;
  a.muestrasAtaque = nA;
  a.atacando = true;
  a.cerrando = false;
  if (!a.activa) {
    float pan = 0.5f + (float)(nota - 66) * 0.025f;   // panorama fijo por altura
    if (pan < 0.2f) pan = 0.2f;
    if (pan > 0.8f) pan = 0.8f;
    a.panL = sqrtf(1.0f - pan);
    a.panR = sqrtf(pan);
    calcularSVF(a.c, a.corteBase, ARP_Q, arpComp);
  }
  a.activa = true;
  notaArpLed = nota; contArp++;
}

void soltarTodo() {
  melSonando = false; melEtapa = 0;
  bajoSonando = false; bajoAtacando = false;
  for (int v = 0; v < ARP_VOCES; v++) arp[v].atacando = false;     // decaen desde donde estén
}

// ==============================================================================================================================================
// LEDs (tarea de control)
// ==============================================================================================================================================
const uint8_t HUE_PANEL[4] = { 0, 140, 195, 18 };

void renderLEDs() {
  static unsigned long ultimo = 0;
  static uint32_t vMel = 0, vBajo = 0, vArp = 0, vNegra = 0;
  static float fMel = 0, fBajo = 0, fArp = 0, fNegra = 0;
  unsigned long t = millis();
  if (t - ultimo < LED_REFRESH_MS) return;
  ultimo = t;

  if (contMel   != vMel)   { vMel   = contMel;   fMel   = 1.0f; }
  if (contBajo  != vBajo)  { vBajo  = contBajo;  fBajo  = 1.0f; }
  if (contArp   != vArp)   { vArp   = contArp;   fArp   = 1.0f; }
  if (contNegra != vNegra) { vNegra = contNegra; fNegra = negraFuerte ? 1.0f : 0.45f; }

  leds[0] = CHSV((uint8_t)(notaMelLed  * 21), 220, (uint8_t)(15 + fMel  * 200.0f));
  leds[1] = CHSV((uint8_t)(notaBajoLed * 21), 240, (uint8_t)(15 + fBajo * 200.0f));
  leds[2] = CHSV((uint8_t)(notaArpLed  * 21), 160, (uint8_t)(10 + fArp  * 200.0f));
  leds[3] = CHSV(64, 120, (uint8_t)(fNegra * 220.0f));
  leds[4] = (panel == PANEL_GENERAL) ? CRGB(110, 110, 110) : (CRGB)CHSV(HUE_PANEL[panel], 255, 150);
  leds[5] = tocando ? CHSV(96, 230, 120) : CHSV(0, 230, 35);
  static uint32_t vSobre = 0; static unsigned long tSobre = 0;
  if (contSobrecarga != vSobre) { vSobre = contSobrecarga; tSobre = t; }
  if (tSobre && t - tSobre < 300) leds[5] = CRGB(255, 0, 200);    // sobrecarga de CPU

  fMel *= 0.85f; fBajo *= 0.85f; fArp *= 0.75f; fNegra *= 0.70f;

  if (flashNivel > 0.02f) {
    uint8_t w = (uint8_t)(flashNivel * 140.0f);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] += CRGB(w, w, w);
    flashNivel *= 0.55f;
  }
  FastLED.show();
}

// ==============================================================================================================================================
// CONTROLES (tarea de control)
// ==============================================================================================================================================
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
    case PANEL_GENERAL:
      if      (i == 0) pVolGeneral = v;
      else if (i == 1) pVelocidad  = v;
      break;
    case PANEL_MELODIA:
      if      (i == 0) pVolMel   = v;
      else if (i == 1) pVibrato  = v;
      else if (i == 2) pCorteMel = v;
      else             pResMel   = v;
      break;
    case PANEL_BAJO:
      if      (i == 0) pVolBajo   = v;
      else if (i == 1) pCorteBajo = v;
      else if (i == 2) pResBajo   = v;
      break;
    case PANEL_ARPEGIO:
      if      (i == 0) pVolArp    = v;
      else if (i == 1) pAtaqueArp = v;
      else if (i == 2) pDecayArp  = v;
      break;
  }
}

void pasoControl() {
  unsigned long tms = millis();

  // ── Botones: flanco de presión ──
  for (int b = 0; b < 5; b++) {
    bool nivel = digitalRead(BTN_PIN[b]);
    if (nivel == LOW && bNivel[b] == HIGH && (tms - bTiempo[b]) > DEBOUNCE_MS) {
      bTiempo[b] = tms;
      switch (b) {
        case 0: tocando = !tocando; entrarPanel(PANEL_GENERAL); break;   // BTN1 play/stop + general
        case 1: entrarPanel(PANEL_MELODIA); break;                        // BTN2
        case 2: entrarPanel(PANEL_BAJO);    break;                        // BTN3
        case 3: entrarPanel(PANEL_ARPEGIO); break;                        // BTN4
        default: break;                                                   // BTN5 libre
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

  renderLEDs();
}

// ==============================================================================================================================================
// SETUP
// ==============================================================================================================================================
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;   // ≈ 12 ms de cola: Play/Stop responden en el acto
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

inline float mulTau(float tau) { return expf(-1.0f / (tau * SAMPLE_RATE)); }

void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);

  for (int b = 0; b < 5; b++) pinMode(BTN_PIN[b], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int i = 0; i <= TABLA_SENO; i++) tablaSeno[i] = sinf(2.0f * (float)M_PI * (float)i / (float)TABLA_SENO);
  for (int v = 0; v < ARP_VOCES; v++) memset(&arp[v], 0, sizeof(VozArp));
  arpMulCierre = mulTau(0.0015f);
  arpComp = 1.0f / powf(ARP_Q, 0.35f);

  mulMelSosten  = mulTau(LEAD_TAU_SOSTEN);
  mulMelSoltar  = mulTau(LEAD_TAU_SOLTAR);
  mulMelFiltro  = mulTau(LEAD_TAU_FILTRO);
  mulBajoSoltar = mulTau(BAJO_TAU_SOLTAR);
  mulBajoFiltro = mulTau(BAJO_TAU_FILTRO);
  mulStop       = mulTau(TAU_STOP);
  glideK        = 1.0f - mulTau(LEAD_GLIDE_S);
  vibSubir      = 1.0f - mulTau(VIB_TAU_ENTRADA);
  vibBajar      = 1.0f - mulTau(0.03f);
  razonMel2       = powf(2.0f, LEAD_DESAFINE_CENTS / 1200.0f);
  razonBajoSierra = powf(2.0f, -BAJO_DESAFINE_CENTS / 1200.0f);
  razonBajoCuad   = powf(2.0f,  BAJO_DESAFINE_CENTS / 1200.0f);
  razonArp        = powf(2.0f,  ARP_DESAFINE_CENTS / 1200.0f);
  limAtk = 1.0f - mulTau(0.0005f);
  limRel = 1.0f - mulTau(0.150f);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHT);
  FastLED.clear();
  FastLED.show();

  entrarPanel(PANEL_GENERAL);
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
  uint32_t t0Render = micros();
  // ── Borde del buffer: transporte ──
  bool quiere = tocando;
  if (quiere && !aTocando) {                    // Play: desde el comienzo, en la primera muestra
    posicion = 0;
    idxMel = idxBajo = idxArp = 0;
    ultimaNegra = 0xFFFFFFFF;
    finNatural = false;
  }
  if (!quiere && aTocando) soltarTodo();        // Stop
  bool parando = !quiere && !finNatural;        // Stop del usuario: cierre de 40 ms
  aTocando = quiere;

  // Avance por muestra en ticks (32.32). Se calcula en double una vez por buffer.
  double ticksPorMuestra = (double)BPM_ORIGINAL * factorVelocidad(pVelocidad) * PPQ / 60.0 / SAMPLE_RATE;
  uint64_t incPos = (uint64_t)(ticksPorMuestra * 4294967296.0 + 0.5);

  // Objetivos de los parámetros (se suavizan por muestra)
  float tVolGen = pVolGeneral * pVolGeneral;
  float tVolMel = pVolMel * pVolMel, tVolBajo = pVolBajo * pVolBajo, tVolArp = pVolArp * pVolArp;
  float tCorteMel = pCorteMel, tResMel = pResMel, tCorteBajo = pCorteBajo, tResBajo = pResBajo;
  float tVib = pVibrato * pVibrato;

  // Decay del arpegio (lo que ya suena toma el nuevo decay en el acto: es un cambio de
  // pendiente, no de nivel → sin clic)
  float tauA = decayArpS(pDecayArp);
  arpMulAmp    = parando ? mulStop : mulTau(tauA);
  arpMulFiltro = mulTau(tauA * ARP_TAU_FILTRO_REL + 0.02f);

  const float incAtkMel  = 1.0f / (LEAD_ATAQUE_S * SAMPLE_RATE);
  const float incAtkBajo = 1.0f / (BAJO_ATAQUE_S * SAMPLE_RATE);
  const uint32_t retardoVib = (uint32_t)(VIB_RETARDO_S * SAMPLE_RATE);

  int16_t buffer[BUFFER_SAMPLES * 2];

  for (int i = 0; i < BUFFER_SAMPLES; i++) {
    // ── Secuenciador: preciso a la muestra ──
    if (aTocando) {
      uint32_t tick = (uint32_t)(posicion >> 32);
      if (tick >= FIN_CANCION) {
        if (REPETIR_CANCION) {
          posicion -= (uint64_t)FIN_CANCION << 32;
          tick -= FIN_CANCION;
          idxMel = idxBajo = idxArp = 0;
        } else {
          aTocando = false;
          finNatural = true;
          tocando = false;
          soltarTodo();
        }
      }
      if (aTocando) {
        while (idxMel  < N_MELODIA && PISTA_MELODIA[idxMel].inicio  <= tick) melNotaOn(PISTA_MELODIA[idxMel++]);
        while (idxBajo < N_BAJO    && PISTA_BAJO[idxBajo].inicio    <= tick) bajoNotaOn(PISTA_BAJO[idxBajo++]);
        while (idxArp  < N_ARPEGIO && PISTA_ARPEGIO[idxArp].inicio  <= tick) arpGolpe(PISTA_ARPEGIO[idxArp++].nota + TRANSP_ARPEGIO);
        // Note-off: sólo el de la nota que está sonando (las notas ligadas se pisan)
        if (melSonando  && tick >= melFin)  { melSonando = false;  melEtapa = 0; }
        if (bajoSonando && tick >= bajoFin) { bajoSonando = false; bajoAtacando = false; }
        uint32_t negra = tick / PPQ;
        if (negra != ultimaNegra) { ultimaNegra = negra; negraFuerte = (negra % 4) == 0; contNegra++; }
        posicion += incPos;
      }
    }

    // ── Parámetros suavizados ──
    sVolGen    += (tVolGen    - sVolGen)    * SUAVIZADO;
    sVolMel    += (tVolMel    - sVolMel)    * SUAVIZADO;
    sVolBajo   += (tVolBajo   - sVolBajo)   * SUAVIZADO;
    sVolArp    += (tVolArp    - sVolArp)    * SUAVIZADO;
    sCorteMel  += (tCorteMel  - sCorteMel)  * SUAVIZADO;
    sResMel    += (tResMel    - sResMel)    * SUAVIZADO;
    sCorteBajo += (tCorteBajo - sCorteBajo) * SUAVIZADO;
    sResBajo   += (tResBajo   - sResBajo)   * SUAVIZADO;
    sVib       += (tVib       - sVib)       * SUAVIZADO;

    // ════════ MELODÍA (lead) ════════
    float melOut = 0.0f;
    if (melEtapa == 1) {
      melEnv += incAtkMel;
      if (melEnv >= 1.0f) { melEnv = 1.0f; melEtapa = 2; }
    } else if (melEtapa == 2) {
      melEnv = LEAD_SOSTEN + (melEnv - LEAD_SOSTEN) * mulMelSosten;
      if (parando) melEtapa = 0;
    } else {
      melEnv *= parando ? mulStop : mulMelSoltar;
      if (melEnv < 1.0e-6f) melEnv = 0.0f;
    }
    melEnvF *= mulMelFiltro;

    if (melEnv > 0.0f) {
      melFrec += (melFrecObj - melFrec) * glideK;

      // Vibrato con entrada retardada: nada al empezar la nota, después sube suave
      melMuestrasNota++;
      float objVib = (melSonando && melMuestrasNota > retardoVib) ? 1.0f : 0.0f;
      vibNivel += (objVib - vibNivel) * (objVib > vibNivel ? vibSubir : vibBajar);
      avanzar(vibFase, VIB_HZ * INV_SR);
      float cents = seno(vibFase) * VIB_MAX_CENTS * sVib * vibNivel;
      float f = melFrec * (1.0f + cents * 0.000577623f);   // 2^(c/1200) ≈ 1 + c·ln2/1200

      float dt1 = f * INV_SR, dt2 = dt1 * razonMel2, dt3 = dt1 / razonMel2;
      avanzar(melFase[0], dt1); avanzar(melFase[1], dt2); avanzar(melFase[2], dt3);
      float x = (sierra(melFase[0], dt1) + sierra(melFase[1], dt2)) * 0.5f
              + cuadrada(melFase[2], dt3) * LEAD_NIVEL_CUADRADA;

      if ((i & 7) == 0) {
        float fc = corteMelHz(sCorteMel) * exp2Rapido(LEAD_ENV_FILTRO_OCT * melEnvF);
        float q = qDe(sResMel, LEAD_Q_MAX, fc);
        calcularSVF(cMel, fc, q, compQ(q));
      }
      melOut = filtroLP(svfMel, cMel, x + 1.0e-18f) * cMel.comp * melEnv;
    } else {
      vibNivel = 0.0f;
    }

    // ════════ BAJO (sierra + cuadrada desafinadas) ════════
    float bajoOut = 0.0f;
    if (bajoAtacando) {
      bajoEnv += incAtkBajo;
      if (bajoEnv >= 1.0f) { bajoEnv = 1.0f; bajoAtacando = false; }
    } else if (!bajoSonando) {
      bajoEnv *= parando ? mulStop : mulBajoSoltar;
      if (bajoEnv < 1.0e-6f) bajoEnv = 0.0f;
    }
    bajoEnvF *= mulBajoFiltro;

    if (bajoEnv > 0.0f) {
      float dtS = bajoFrec * razonBajoSierra * INV_SR;
      float dtC = bajoFrec * razonBajoCuad   * INV_SR;
      avanzar(bajoFase[0], dtS); avanzar(bajoFase[1], dtC);
      float x = sierra(bajoFase[0], dtS) * BAJO_NIVEL_SIERRA + cuadrada(bajoFase[1], dtC) * BAJO_NIVEL_CUADRADA;
      x = softClip(x * BAJO_DRIVE) / BAJO_DRIVE;

      if ((i & 7) == 4) {
        float fc = corteBajoHz(sCorteBajo) * exp2Rapido(BAJO_ENV_FILTRO_OCT * bajoEnvF);
        float q = qDe(sResBajo, BAJO_Q_MAX, fc);
        calcularSVF(cBajo, fc, q, compQ(q));
      }
      bajoOut = filtroLP(svfBajo, cBajo, x + 1.0e-18f) * cBajo.comp * bajoEnv;
    }

    // ════════ ARPEGIO (pluck electrónico) ════════
    float aL = 0.0f, aR = 0.0f;
    for (int v = 0; v < ARP_VOCES; v++) {
      VozArp &a = arp[v];
      if (!a.activa) continue;
      if (a.atacando) {
        a.env  += a.pasoAtk;
        a.envF += a.pasoAtkF;
        if (--a.muestrasAtaque <= 0) a.atacando = false;
      } else {
        a.env  *= a.cerrando ? arpMulCierre : arpMulAmp;
        a.envF *= arpMulFiltro;
        if (!a.cerrando && a.env < ARP_UMBRAL_CIERRE) a.cerrando = true;
        if (a.cerrando && a.env < ARP_UMBRAL_LIBRE) {
          a.activa = false; a.env = 0.0f; a.envF = 0.0f;
          continue;
        }
      }
      if (((i + v) & 31) == 0)       // coeficientes cada 32 muestras, repartidos entre voces
        calcularSVF(a.c, a.corteBase * exp2Rapido(ARP_ENV_OCT * a.envF), ARP_Q, arpComp);

      avanzar(a.fase[0], a.dt[0]);
      avanzar(a.fase[1], a.dt[1]);
      float p2 = a.fase[1] + 0.25f; if (p2 >= 1.0f) p2 -= 1.0f;
      float pulso = (sierra(a.fase[1], a.dt[1]) - sierra(p2, a.dt[1])) * 0.5f;
      float x = sierra(a.fase[0], a.dt[0]) * ARP_NIVEL_SIERRA + pulso * ARP_NIVEL_PULSO;
      float y = filtroLP(a.svf, a.c, x + 1.0e-18f) * a.c.comp * a.env;
      aL += y * a.panL;
      aR += y * a.panR;
    }

    // ════════ MEZCLA ════════
    float centro = melOut * NIVEL_MELODIA * sVolMel + bajoOut * NIVEL_BAJO * sVolBajo;
    float L = centro + aL * NIVEL_ARPEGIO * sVolArp;
    float R = centro + aR * NIVEL_ARPEGIO * sVolArp;

    // Bloqueador de continua
    float dL = L - dcXL + 0.9995f * dcYL; dcXL = L; dcYL = dL;
    float dR = R - dcXR + 0.9995f * dcYR; dcXR = R; dcYR = dR;

    float g = sVolGen * MASTER;
    dL *= g; dR *= g;

    // Limitador con ganancia suavizada (un escalón de ganancia es un clic)
    float pico = fabsf(dL) > fabsf(dR) ? fabsf(dL) : fabsf(dR);
    float objetivo = (pico > LIM_UMBRAL) ? LIM_UMBRAL / pico : 1.0f;
    limGan += (objetivo - limGan) * (objetivo < limGan ? limAtk : limRel);
    dL *= limGan; dR *= limGan;

    float oL = softClip(dL) * 32000.0f;
    float oR = softClip(dR) * 32000.0f;
    if (oL >  32000.0f) oL =  32000.0f;
    if (oL < -32000.0f) oL = -32000.0f;
    if (oR >  32000.0f) oR =  32000.0f;
    if (oR < -32000.0f) oR = -32000.0f;
    buffer[i * 2]     = (int16_t)oL;
    buffer[i * 2 + 1] = (int16_t)oR;
  }

  // Indicador de carga: un buffer de 128 muestras dura 2.9 ms; si calcularlo se come más del
  // 80 %, el DMA queda al borde de vaciarse (ceros = ruido). El LED 5 destella magenta.
  if (micros() - t0Render > (uint32_t)(BUFFER_SAMPLES * 1000000ULL / SAMPLE_RATE * 8 / 10)) contSobrecarga++;

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
