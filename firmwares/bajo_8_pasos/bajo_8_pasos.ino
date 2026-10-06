// ==============================================================================================================================================
// PERCU-SYNTH — BAJO 8 PASOS (secuenciador de bajo en sierra, pentatónica menor) — GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo - GC Lab Chile, en conjunto con los participantes del taller abierto en Hive Espacios
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
//
// Notas: cada pot recorre 11 notas de la pentatónica menor (2 octavas). Con el pot
// al mínimo (< 3 % del recorrido) ese paso queda en SILENCIO.
//
// LEDs de la placa:
//   0..3 = panel A/B: los 4 pasos del panel (color = nota, apagado = silencio,
//          blanco = paso sonando) · panel C: nivel de cada parámetro
//   4    = panel activo (A cian · B violeta · C naranjo)
//   5    = octava (brillo) + pulso en cada paso mientras suena (fuerte en los pasos 1 y 5)
//
// Tira externa (121 LEDs): BALAS DE LUZ. Cada paso que suena dispara un pulso desde el
// inicio de la tira que avanza hacia el final con su estela. Color = paso (arcoíris de
// 8 colores) corrido un poco por la nota; los pasos 1 y 5 salen más brillantes y largos.
// Un paso en silencio no dispara. La velocidad sigue al tempo: siempre quedan 15 LEDs
// entre bala y bala, o sea la secuencia entera de 8 pasos cabe volando en la tira.
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <math.h>

// ─── Tipos (arriba del todo para que el IDE de Arduino genere bien los prototipos) ───
struct EstadoSVF { float ic1, ic2; };
struct Bala {
  bool    viva;
  float   pos;        // posición de la punta en la tira (en LEDs, con decimales)
  uint8_t hue;        // color
  uint8_t brillo;     // brillo de la punta
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
#define LED_MAX_MA    2000                    // tope de consumo de la tira (FastLED baja el brillo si se pasa)
const unsigned long LED_REFRESH_MS = 16;      // ~60 fps: las balas se mueven suaves
CRGB leds[NUM_LEDS];

// ─── Balas de luz (tira externa) ───────────────────────────
// Cada paso que suena dispara una bala desde el LED 0 de la tira hacia el final.
// La velocidad va atada al tempo: entre bala y bala quedan siempre ESPACIO_BALAS LEDs,
// así que la tira muestra la secuencia entera volando (8 pasos × 15 = 120 LEDs).
#define MAX_BALAS      16
const float   ESPACIO_BALAS = 15.0f;          // LEDs recorridos por paso
const uint8_t HUE_PASO[8] ={ 0, 28, 56, 96, 130, 160, 190, 225 };  // arcoíris por paso
Bala balas[MAX_BALAS];

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
volatile int      pasoSonando   = -1;   // −1 = detenido
volatile uint32_t contadorPasos = 0;    // sube en cada paso (latido de los LEDs)

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

float volS = 0.7f, corteS = 0.45f;      // parámetros suavizados por muestra
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

float flashNivel = 0.0f;                // destello al cambiar panel / octava / play
float pulsoPaso  = 0.0f;

static i2s_chan_handle_t tx_chan;

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
  float oct = ENV_FILTRO_OCT * envF;
  float fc = corteHzDe(corteS) * powf(2.0f, oct);
  if (fc > FILTRO_TECHO) fc = FILTRO_TECHO;
  if (fc < 20.0f) fc = 20.0f;

  float q = RES_MAX - (RES_MAX - RES_MIN) * corteS;
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

// Dispara una bala desde el inicio de la tira. Color = paso (arcoíris) corrido por la
// nota; los pasos 1 y 5 salen más brillantes y con estela más larga.
void dispararBala(int paso, int nota) {
  int slot = 0;
  float masLejos = -1.0f;
  for (int i = 0; i < MAX_BALAS; i++) {
    if (!balas[i].viva) { slot = i; break; }
    if (balas[i].pos > masLejos) { masLejos = balas[i].pos; slot = i; }  // sin libres: la más lejana
  }
  bool fuerte = (paso == 0 || paso == 4);
  Bala &b = balas[slot];
  b.viva   = true;
  b.pos    = 0.0f;
  b.hue    = HUE_PASO[paso] + (uint8_t)(nota * 5);
  b.brillo = fuerte ? 255 : 170;
  b.largo  = fuerte ? 11 : 6;
}

// Mueve y dibuja todas las balas. La velocidad sale del tempo actual: ESPACIO_BALAS
// LEDs por paso, así la separación entre balas es la misma a cualquier velocidad.
void renderBalas(unsigned long dtMs) {
  for (int i = STATUS_LEDS; i < NUM_LEDS; i++) leds[i] = CRGB(0, 0, 0);
  if (dtMs > 60) dtMs = 60;
  float msPaso = 60000.0f / ((60.0f + pVel * 140.0f) * 4.0f);
  float avance = ESPACIO_BALAS * (float)dtMs / msPaso;

  for (int k = 0; k < MAX_BALAS; k++) {
    Bala &b = balas[k];
    if (!b.viva) continue;
    b.pos += avance;
    if (b.pos - (float)b.largo > (float)TIRA_LEDS) { b.viva = false; continue; }

    for (int j = 0; j <= b.largo; j++) {
      float x = b.pos - (float)j;
      if (x < 0.0f) break;
      float caida = 1.0f - (float)j / (float)(b.largo + 1);
      float v = (float)b.brillo * caida * caida;          // estela que se apaga cuadrática
      uint8_t sat = (j == 0) ? 90 : 255;                  // punta casi blanca, estela de color
      int   i0 = (int)x;
      float fr = x - (float)i0;                           // sub-píxel: avanza suave, sin saltos
      pixelTira(i0,     CHSV(b.hue, sat, (uint8_t)(v * (1.0f - fr))));
      pixelTira(i0 + 1, CHSV(b.hue, sat, (uint8_t)(v * fr)));
    }
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
    pulsoPaso = (ps == 0 || ps == 4) ? 1.0f : 0.45f;
    if (tocando && ps >= 0) {
      int n = notaPaso[ps];
      if (n >= 0) dispararBala(ps, n);             // paso en silencio = no hay bala
    }
  }
  renderBalas(dtMs);

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

  // LED 4 → panel activo
  leds[4] = CHSV(HUE_PANEL[panel], 255, 150);

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
      switch (b) {
        case 0: tocando = !tocando; flashNivel = 0.4f; break;     // BTN1 play / stop
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
  }
  if (!quiereTocar && aTocando) {          // Stop: se cierra la nota
    soltando = true;
    atacando = false;
    pasoSonando = -1;
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
  float tau = tauDecayDe(pDecay);
  mulDecay  = expf(-1.0f / (tau * SAMPLE_RATE));
  mulDecayF = expf(-1.0f / (tau * 0.55f * SAMPLE_RATE));   // el filtro cierra antes que el volumen
  float volT = pVol * pVol;                                // curva de volumen más natural
  float corteT = pCorte;

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

    // ── Parámetros suavizados ──
    volS   += (volT   - volS)   * SUAVIZADO;
    corteS += (corteT - corteS) * SUAVIZADO;

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
