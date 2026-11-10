// ==============================================================================================================================================
// PERCU-SYNTH — Drum MIDI + LEDs (controlador MIDI + show de luces) — GC Lab Chile
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
// - Microcontrolador ESP32-S3 (USB nativo)
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// - 4 Piezos |PZ1 -> ADC4, PZ2 -> ADC5, PZ3 -> ADC6, PZ4 -> ADC7| (velocidad por fuerza del golpe)
// - Tira de LEDs WS2812 |DATA -> 46| (LEDs 0-5 = SMD de la placa como indicadores; 144 LEDs de tira)
// - Salida MIDI USB nativa (canal 10 GM Drums)
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - USB Mode           : USB-OTG (TinyUSB)
// - Flash Mode         : DIO
// - PSRAM              : OPI PSRAM
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - FastLED (gestor de librerías Arduino)
// - ESP32 Arduino core ≥ 3.x (USB.h, USBMIDI.h)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Controlador MIDI de batería para tocar en vivo, con un show de luces
// cinematográfico sincronizado a cada golpe. El PercuSynth se comporta como
// un controlador MIDI USB (envía NoteOn/NoteOff a tu DAW favorito): 5 botones
// y 4 piezos, y cada golpe dispara una animación en la tira:
//
//   - BOMBO  → onda de choque amarilla desde el centro de la tira
//   - CAJA   → flash eléctrico blanco + lluvia de chispas amarillo-cálidas
//   - HI-HAT → rayo cian que cruza la tira (alterna dirección a cada golpe)
//   - CRASH  → supernova blanca que se abre a arcoíris y decae 2 segundos
//   - CLAP   → dos frentes magenta que entran desde los extremos y chocan al centro
//   - PIEZOS → cada pad tiene su cuarto de tira: un anillo de color que se
//              expande desde el centro de su zona, con brillo según la fuerza
//
// Los 6 LEDs SMD de la placa son indicadores:
//   - LED 0-3 → destello de cada piezo (color de su pad, brillo = velocidad)
//   - LED 4   → destello del último botón tocado, en el color de su efecto
//   - LED 5   → actividad MIDI: blanco en cada nota enviada
//
// Los efectos se mezclan aditivamente; el pool soporta hasta 12 eventos
// simultáneos.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// CONTROLES (todo dispara en el instante del golpe / flanco de presión):
// - BTN1 (44) → Bombo  (NoteOn 36)
// - BTN2 (42) → Caja   (NoteOn 38)
// - BTN3 (0)  → Hi-Hat (NoteOn 42)
// - BTN4 (45) → Crash  (NoteOn 49)
// - BTN5 (47) → Clap   (NoteOn 39)
//
// - PIEZO1 (ADC4) → Tom 1     (NoteOn 45)
// - PIEZO2 (ADC5) → Tom 2     (NoteOn 47)
// - PIEZO3 (ADC6) → Floor Tom (NoteOn 43)
// - PIEZO4 (ADC7) → Ride      (NoteOn 51)
//   Velocidad = pico del golpe en una ventana de 12 ms (40–127). Anti-retrigger
//   de 60 ms. Si un piezo dispara solo o no responde, ajusta UMBRAL_PIEZO / PIEZO_MAX.
//
// - POT1 (ADC1)  → Brillo de la tira (15 – 80 / 255)
// - POT4 (ADC10) → Velocidad MIDI de los botones (60 – 127)
// - POT2 / POT3  → libres
//
// MIDI:
// - Salida USB nativa "ESP32-S3 MIDI" en canal 10 (GM Drums).
// - Notas: 36=Kick, 38=Snare, 42=Closed HiHat, 49=Crash, 39=Clap,
//          45=Tom 1, 47=Tom 2, 43=Floor Tom, 51=Ride.
// - NoteOff diferido 30 ms después del NoteOn (cola interna de 16 slots).
//
// MODO DE USO:
// 1. Conecta el PercuSynth al PC/Mac → aparece como dispositivo MIDI.
// 2. Abre tu DAW, selecciona el PercuSynth como entrada MIDI canal 10.
// 3. Toca los botones y los piezos: suena en el DAW y se ve en la tira.
// ==============================================================================================================================================

#include <Arduino.h>
#include <FastLED.h>
#include "USB.h"
#include "USBMIDI.h"

USBMIDI MIDI;

// ─── LED ───────────────────────────────────────────────────
#define LED_PIN      46
#define NUM_LEDS    150
#define START_LED     6         // LEDs 0-5 SMD de la placa = indicadores
#define ACTIVE_LEDS (NUM_LEDS - START_LED)  // 144 LEDs activos
#define LED_TYPE     WS2812
#define COLOR_ORDER  GRB
#define MAX_BRIGHT   80

CRGB leds[NUM_LEDS];

// ─── Pines ─────────────────────────────────────────────────
const uint8_t BTN[5] = {44, 42, 0, 45, 47};

#define POT_BRIGHT  1
#define POT_VEL     10

// ─── MIDI ──────────────────────────────────────────────────
#define MIDI_CH  9   // canal 9 = percusión General MIDI (0-indexado)
// Pistas: 0-4 = botones, 5-8 = piezos
#define NUM_BTN_DRUMS 5
#define NUM_PIEZOS    4
#define NUM_DRUMS     (NUM_BTN_DRUMS + NUM_PIEZOS)
const uint8_t NOTES[NUM_DRUMS] = {
  36, 38, 42, 49, 39,   // Kick, Snare, HiHat, Crash, Clap   (BTN1-5)
  45, 47, 43, 51        // Tom 1, Tom 2, Floor Tom, Ride     (PIEZO1-4)
};

// ─── Piezos ────────────────────────────────────────────────
const uint8_t PIEZO_PIN[NUM_PIEZOS] = {4, 5, 6, 7};
#define UMBRAL_PIEZO   300    // lectura cruda que abre la ventana de golpe
#define PIEZO_MAX     3800    // lectura que da velocidad 127
#define VENTANA_MS      12    // ventana de búsqueda del pico
#define RETRIGGER_MS    60    // anti-rebote tras cada golpe

struct PiezoState { bool sampling; int peak; uint32_t t0; uint32_t lastHit; };
PiezoState pz[NUM_PIEZOS];

// Color de cada pad (tira + LED SMD 0-3)
const uint8_t PAD_HUE[NUM_PIEZOS] = {96, 150, 185, 45};   // verde, azul, violeta, dorado
const uint8_t PAD_SAT[NUM_PIEZOS] = {255, 255, 255, 140};
float padFlash[NUM_PIEZOS] = {};   // 0-1, destello de los LEDs SMD 0-3

// Color de cada botón en el LED SMD 4 (mismo color que su efecto en la tira)
const uint8_t BTN_HUE[NUM_BTN_DRUMS] = {40, 25, 135, 0, 224};  // bombo, caja, hihat, crash, clap
float   btnFlash  = 0.0f;   // LED 4: último botón tocado
uint8_t btnHue    = 0;
float   midiFlash = 0.0f;   // LED 5: actividad MIDI (cualquier nota)

// ─── Debounce ──────────────────────────────────────────────
bool     btn_raw[5]={}, btn_cur[5]={}, btn_last[5]={}, btn_edge[5]={};
uint32_t btn_time[5]={};

void readButtons() {
  uint32_t now = millis();
  for (int i = 0; i < 5; i++) {
    bool r = (digitalRead(BTN[i]) == LOW);
    if (r != btn_raw[i]) btn_time[i] = now;
    if (now - btn_time[i] > 25) btn_cur[i] = r;
    btn_raw[i]  = r;
    btn_edge[i] = btn_cur[i] && !btn_last[i];
    btn_last[i] = btn_cur[i];
  }
}

float readPot(uint8_t pin) {
  uint32_t s = 0;
  for (int i = 0; i < 8; i++) s += analogRead(pin);
  return (float)(s >> 3) / 4095.0f;
}

// ─── MIDI NoteOff diferidos ────────────────────────────────
struct NoteOff { bool active; uint8_t note; uint32_t at; };
NoteOff noteOffs[16];

void scheduleOff(uint8_t note) {
  for (auto &n : noteOffs) {
    if (!n.active) { n = {true, note, millis() + 30}; return; }
  }
  MIDI.noteOff(note, 0, MIDI_CH);
}

void processNoteOffs() {
  uint32_t now = millis();
  for (auto &n : noteOffs) {
    if (n.active && now >= n.at) { MIDI.noteOff(n.note, 0, MIDI_CH); n.active = false; }
  }
}

// ============================================================
//  SISTEMA DE EFECTOS LED
//
//  Cada golpe crea un evento animado que recorre toda la tira.
//  Los eventos conviven simultáneamente (mezcla aditiva).
//  phase 0.0 → 1.0 representa el progreso completo del efecto.
//
//  Velocidades (avance de phase por frame a ~30 fps):
//    Bombo : 0.038 → ~26 frames = ~870 ms
//    Caja  : 0.062 → ~16 frames = ~530 ms
//    HiHat : 0.22  →  ~5 frames = ~165 ms
//    Crash : 0.015 → ~67 frames = ~2.2 s
//    Clap  : 0.075 → ~13 frames = ~440 ms
//    Toms  : 0.050 → ~20 frames = ~660 ms
//    Ride  : 0.030 → ~33 frames = ~1.1 s
// ============================================================
const float EVT_SPEED[NUM_DRUMS] = {0.038f, 0.062f, 0.22f, 0.015f, 0.075f,
                                    0.050f, 0.050f, 0.050f, 0.030f};

#define MAX_EVENTS 12
struct Event { bool active; uint8_t drum; float phase; float speed; int8_t dir; float vel; };
Event   events[MAX_EVENTS];
int8_t  hihatDir = 1;   // alterna con cada golpe de hihat

void spawnEvent(uint8_t drum, float vel) {
  // Buscar slot libre; si no hay, reemplazar el más avanzado
  int   slot = 0;
  float maxP = -1.0f;
  for (int i = 0; i < MAX_EVENTS; i++) {
    if (!events[i].active)         { slot = i; break; }
    if (events[i].phase > maxP)    { maxP = events[i].phase; slot = i; }
  }
  int8_t dir = (drum == 2) ? hihatDir : 1;
  if (drum == 2) hihatDir = -hihatDir;
  events[slot] = {true, drum, 0.0f, EVT_SPEED[drum], dir, vel};
}

// ============================================================
//  EFECTOS POR INSTRUMENTO
//  Todos escriben en leds[] de forma aditiva (+=).
// ============================================================

// ── BOMBO: onda de choque simétrica desde el centro ─────────
//
// Modelo de propagación real: el frente viaja a velocidad
// constante. Un LED se enciende cuando el frente lo alcanza
// y luego se enfría gradualmente (amarillo→naranja→rojo→negro).
//
void fxKick(float p) {
  const int   half      = ACTIVE_LEDS / 2;
  const float travelEnd = 0.72f;  // el frente llega al borde en este phase
  const float tailLen   = 0.40f;  // duración del brillo tras ser alcanzado

  for (int i = 0; i < ACTIVE_LEDS; i++) {
    int   dist  = abs(i - half);
    float p_hit = (float)dist / half * travelEnd;  // cuándo llega el frente a este LED
    float dt    = p - p_hit;                        // tiempo transcurrido desde el impacto

    if (dt < 0.0f || dt >= tailLen) continue;

    float decay   = 1.0f - dt / tailLen;          // brillo decae después del impacto
    float distAtt = 1.0f - (float)dist / half * 0.55f;  // leds exteriores algo menos brillantes
    float bright  = decay * distAtt;

    // Color: amarillo puro al impacto, enfría a rojo mientras decae
    uint8_t hue = (uint8_t)(45.0f * (1.0f - dt / tailLen));
    CRGB c; hsv2rgb_rainbow(CHSV(hue, 255, (uint8_t)(bright * 255)), c);
    leds[START_LED + i] += c;
  }
}

// ── CAJA: flash eléctrico + lluvia de chispas ───────────────
//
// Fase 0–0.18: destello blanco que se contrae de afuera hacia el centro
// Fase 0.0–1.0: chispas amarillo-blanco (densidad cae cuadráticamente)
//               hash determinista → parpadeo estable por frame
//
void fxSnare(float p) {
  float inv_p = 1.0f - p;

  // Flash inicial
  if (p < 0.18f) {
    float t      = p / 0.18f;
    int   margin = (int)(t * ACTIVE_LEDS * 0.18f);
    float decay  = 1.0f - t;
    uint8_t v    = (uint8_t)(decay * 255);
    for (int i = margin; i < ACTIVE_LEDS - margin; i++) {
      leds[START_LED + i] += CRGB(v, v, (uint8_t)(v * 0.92f));  // ligeramente cálido
    }
  }

  // Chispas: LCG con seed discreto → parpadean frame a frame
  float sparkDens = inv_p * inv_p;
  uint32_t seed   = (uint32_t)(p * 28.0f);  // cambia ~cada frame
  for (int i = 0; i < ACTIVE_LEDS; i++) {
    uint32_t r = (uint32_t)(i * 2654435761u + seed * 1013904223u) ^ ((uint32_t)i << 13);
    float    n = (float)(r & 0xFFFF) / 65535.0f;
    if (n < sparkDens * 0.30f) {
      float    norm = n / (sparkDens * 0.30f);
      uint8_t  h    = 25 + (uint8_t)((r >> 8) & 0x1F);   // amarillo-naranja
      uint8_t  v    = (uint8_t)(norm * inv_p * 210);
      CRGB c; hsv2rgb_rainbow(CHSV(h, 160, v), c);
      leds[START_LED + i] += c;
    }
  }
}

// ── HIHAT: rayo cian que cruza la tira completa ─────────────
//
// Un punto brillante avanza de extremo a extremo.
// Deja una cola de ~14 LEDs que va de blanco a azul-cian.
// La dirección alterna en cada golpe.
//
void fxHihat(float p, int8_t dir) {
  float rawPos = p * (ACTIVE_LEDS + 16) - 8.0f;  // incluye márgenes fuera de tira
  float pos    = (dir > 0) ? rawPos : (ACTIVE_LEDS - 1.0f - rawPos);
  float inv_p  = 1.0f - p;

  for (int i = 0; i < ACTIVE_LEDS; i++) {
    float d = fabsf((float)i - pos);
    if (d >= 14.0f) continue;
    float bright = (1.0f - d / 14.0f) * inv_p;
    // Frente blanco-cian → cola azul profundo
    uint8_t h = (uint8_t)(128 + d * 5);   // cian(128) → azul(198) en la cola
    uint8_t s = (uint8_t)(80  + d * 12);  // más saturado (azul vivo) en cola
    CRGB c; hsv2rgb_rainbow(CHSV(h, s, (uint8_t)(bright * 255)), c);
    leds[START_LED + i] += c;
  }
}

// ── CRASH: supernova — blanco → arcoíris → desvanecimiento ──
//
// Fase 0–0.07 : ataque — la tira se llena de blanco puro
// Fase 0.07–0.25: transición — el blanco gana color (saturación sube)
// Fase 0.25–1.0 : decay largo — arcoíris rotante que se apaga suave
//
void fxCrash(float p) {
  uint8_t hueShift = (uint8_t)(p * 210);  // el arcoíris rota mientras apaga
  float   env, sat_f;

  if (p < 0.07f) {
    env   = p / 0.07f;               // ataque rápido
    sat_f = 0.0f;                    // blanco puro
  } else if (p < 0.25f) {
    env   = 1.0f;
    sat_f = (p - 0.07f) / 0.18f;    // gana color gradualmente
  } else {
    env   = 1.0f - (p - 0.25f) / 0.75f;  // decay largo
    sat_f = 1.0f;
  }

  for (int i = 0; i < ACTIVE_LEDS; i++) {
    uint8_t h = hueShift + (uint8_t)((float)i / ACTIVE_LEDS * 192);
    uint8_t s = (uint8_t)(sat_f * 255);
    uint8_t v = (uint8_t)(env * 230);
    CRGB c; hsv2rgb_rainbow(CHSV(h, s, v), c);
    leds[START_LED + i] += c;
  }
}

// ── CLAP: dos frentes magenta que chocan en el centro ──────
//
// Cada frente entra desde un extremo con una cola corta; al
// llegar al centro (phase 1) se apagan. Brillo cae con phase.
//
void fxClap(float p) {
  const float half  = ACTIVE_LEDS * 0.5f;
  const float front = p * half;
  const float inv_p = 1.0f - p;
  for (int i = 0; i < ACTIVE_LEDS; i++) {
    float dEdge = (float)min(i, ACTIVE_LEDS - 1 - i);   // distancia al extremo más cercano
    float d     = front - dEdge;                       // > 0 = detrás del frente
    if (d < -1.0f || d > 10.0f) continue;
    float bright = (d < 0.0f) ? (1.0f + d) : (1.0f - d / 10.0f);
    bright *= inv_p;
    uint8_t s = (uint8_t)(120 + fminf(d, 10.0f) * 13);  // frente rosado → cola magenta
    CRGB c; hsv2rgb_rainbow(CHSV(224, s, (uint8_t)(bright * 255)), c);
    leds[START_LED + i] += c;
  }
}

// ── PAD (piezos): anillo que se expande en el cuarto de tira del pad ──
//
// Núcleo brillante que se apaga + anillo que viaja hasta el borde
// de la zona. Brillo escalado por la velocidad del golpe.
//
void fxPad(float p, uint8_t k, float vel) {
  const float zone  = ACTIVE_LEDS / (float)NUM_PIEZOS;
  const float ctr   = zone * (k + 0.5f);
  const float R     = zone * 0.5f;
  const float front = p * R * 1.1f;
  const float inv_p = 1.0f - p;
  int i0 = max(0, (int)(ctr - R - 2));
  int i1 = min(ACTIVE_LEDS - 1, (int)(ctr + R + 2));
  for (int i = i0; i <= i1; i++) {
    float d    = fabsf((float)i - ctr);
    float ring = 1.0f - fabsf(d - front) / 3.0f;
    ring = (ring > 0.0f) ? ring * inv_p : 0.0f;
    float core = (d < R) ? (1.0f - d / R) * inv_p * inv_p : 0.0f;
    float bright = fmaxf(ring, core) * vel;
    if (bright <= 0.0f) continue;
    CRGB c; hsv2rgb_rainbow(CHSV(PAD_HUE[k] + (uint8_t)(d * 1.5f), PAD_SAT[k], (uint8_t)(bright * 255)), c);
    leds[START_LED + i] += c;
  }
}

// ============================================================
//  LEDs SMD DE LA PLACA (0-5) — indicadores
// ============================================================
void renderBoardLeds() {
  // LED 0-3: destello de cada piezo (brillo tenue de reposo = "vivo")
  for (int k = 0; k < NUM_PIEZOS; k++) {
    uint8_t v = (uint8_t)(10 + padFlash[k] * 245);
    hsv2rgb_rainbow(CHSV(PAD_HUE[k], PAD_SAT[k], v), leds[k]);
    padFlash[k] *= 0.80f;
  }
  // LED 4: último botón tocado, en el color de su efecto
  hsv2rgb_rainbow(CHSV(btnHue, 230, (uint8_t)(btnFlash * 255)), leds[4]);
  btnFlash *= 0.80f;
  // LED 5: actividad MIDI — blanco en cada nota enviada (brillo = velocidad)
  uint8_t b = (uint8_t)(midiFlash * 255);
  leds[5] = CRGB(b, b, b);
  midiFlash *= 0.70f;
}

// ─── Frame principal ───────────────────────────────────────
void renderFrame() {
  // Negro total como base: los LEDs solo brillan si hay un evento activo
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  for (auto &e : events) {
    if (!e.active) continue;
    switch (e.drum) {
      case 0: fxKick (e.phase);        break;
      case 1: fxSnare(e.phase);        break;
      case 2: fxHihat(e.phase, e.dir); break;
      case 3: fxCrash(e.phase);        break;
      case 4: fxClap (e.phase);        break;
      default: fxPad(e.phase, e.drum - NUM_BTN_DRUMS, e.vel); break;
    }
    e.phase += e.speed;
    if (e.phase >= 1.0f) e.active = false;
  }

  renderBoardLeds();
  FastLED.show();
}

// ─── Disparar drum: MIDI + LED ─────────────────────────────
void hitDrum(int drum, uint8_t vel) {
  MIDI.noteOn(NOTES[drum], vel, MIDI_CH);
  scheduleOff(NOTES[drum]);
  if (drum >= NUM_BTN_DRUMS) {
    float v = vel / 127.0f;
    padFlash[drum - NUM_BTN_DRUMS] = v;
    spawnEvent(drum, 0.35f + 0.65f * v);   // golpe suave = luz tenue, nunca invisible
  } else {
    spawnEvent(drum, 1.0f);
    btnFlash = 1.0f;
    btnHue   = BTN_HUE[drum];
  }
  midiFlash = vel / 127.0f;
  Serial.printf("HIT %d  note=%d  vel=%d\n", drum, NOTES[drum], vel);
}

// ─── Piezos: detección de golpe no bloqueante ──────────────
// Supera el umbral → abre una ventana de VENTANA_MS buscando el pico →
// velocidad 40-127 según el pico.
void readPiezos() {
  uint32_t now = millis();
  for (int k = 0; k < NUM_PIEZOS; k++) {
    PiezoState &s = pz[k];
    if (!s.sampling) {
      if (now - s.lastHit < RETRIGGER_MS) continue;
      int v = analogRead(PIEZO_PIN[k]);
      if (v > UMBRAL_PIEZO) { s.sampling = true; s.peak = v; s.t0 = now; }
    } else {
      int v = analogRead(PIEZO_PIN[k]);
      if (v > s.peak) s.peak = v;
      if (now - s.t0 >= VENTANA_MS) {
        long vel = map(constrain(s.peak, UMBRAL_PIEZO, PIEZO_MAX),
                       UMBRAL_PIEZO, PIEZO_MAX, 40, 127);
        s.sampling = false;
        s.lastHit  = now;
        hitDrum(NUM_BTN_DRUMS + k, (uint8_t)vel);
      }
    }
  }
}

// ─── Setup ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  MIDI.begin();
  USB.begin();

  for (int i = 0; i < 5; i++) pinMode(BTN[i], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  memset(noteOffs, 0, sizeof(noteOffs));
  memset(events,   0, sizeof(events));
  memset(pz,       0, sizeof(pz));

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(MAX_BRIGHT);
  FastLED.clear();
  FastLED.show();

  Serial.println("=== PercuSynth v2 — Drum MIDI + LEDs ===");
  Serial.println("BTN1-5: Bombo/Caja/HiHat/Crash/Clap");
  Serial.println("PIEZO1-4: Tom1/Tom2/FloorTom/Ride (velocidad por fuerza)");
  Serial.println("POT1: Brillo  POT4: Velocidad de los botones");
}

// ─── Loop ──────────────────────────────────────────────────
void loop() {
  static uint32_t lastFrame = 0;
  uint32_t now = millis();

  // Leer pots cada 20 ms (son 16 analogRead por pasada: leídos en cada
  // vuelta, los piezos se muestrean muy de vez en cuando y se pierden picos)
  static uint32_t lastPots = 0;
  static float bright_f = 0.5f, vel_f = 1.0f;
  if (now - lastPots >= 20) {
    lastPots = now;
    bright_f = readPot(POT_BRIGHT);
    vel_f    = readPot(POT_VEL);
  }

  uint8_t vel = (uint8_t)(60 + vel_f * 67.0f);
  FastLED.setBrightness((uint8_t)(15 + bright_f * (MAX_BRIGHT - 15)));

  // BTN 1-5: disparo en el flanco de presión
  readButtons();
  for (int i = 0; i < NUM_BTN_DRUMS; i++) {
    if (btn_edge[i]) hitDrum(i, vel);
  }

  // Piezos: disparo con velocidad por fuerza
  readPiezos();

  processNoteOffs();

  // Renderizar a ~30 fps
  if (now - lastFrame >= 33) {
    lastFrame = now;
    renderFrame();
  }
}
