// ============================================================
//  PercuSynth — drum_midi_leds.ino  v2
//  GC Lab Chile
//
//  Secuenciador 16 pasos + USB MIDI + LEDs WS2812
//  con efectos full-strip cinematográficos.
//
//  Librerías requeridas:
//    - FastLED  (gestor de librerías Arduino)
//    - USB.h / USBMIDI.h  (ESP32 Arduino core)
//
//  Controles:
//    BTN1 (pin 44) → Bombo  — trigger + grabar
//    BTN2 (pin 42) → Caja   — trigger + grabar
//    BTN3 (pin  0) → HiHat  — trigger + grabar
//    BTN4 (pin 45) → Crash  — trigger + grabar
//    BTN5 (pin 47) → Toggle GRAB / PLAYBACK
//
//    POT1 (ADC  1) → Brillo
//    POT2 (ADC  2) → Tempo (60–240 BPM)
//    POT3 (ADC  8) → Color base del fondo
//    POT4 (ADC 10) → Velocidad MIDI (60–127)
// ============================================================

#include <Arduino.h>
#include <FastLED.h>
#include "USB.h"
#include "USBMIDI.h"

USBMIDI MIDI;

// ─── LED ───────────────────────────────────────────────────
#define LED_PIN      46
#define NUM_LEDS    150
#define START_LED     6         // LEDs 0-5 SMD internos, siempre apagados
#define ACTIVE_LEDS (NUM_LEDS - START_LED)  // 144 LEDs activos
#define LED_TYPE     WS2812
#define COLOR_ORDER  GRB
#define MAX_BRIGHT   80

CRGB leds[NUM_LEDS];

// ─── Pines ─────────────────────────────────────────────────
const uint8_t BTN[5] = {44, 42, 0, 45, 47};

#define POT_BRIGHT  1
#define POT_TEMPO   2
#define POT_COLOR   8
#define POT_VEL     10

// ─── MIDI ──────────────────────────────────────────────────
#define MIDI_CH  9   // canal 9 = percusión General MIDI (0-indexado)
const uint8_t NOTES[4] = {36, 38, 42, 49};  // Kick, Snare, HiHat, Crash

// ─── Secuenciador ──────────────────────────────────────────
#define NUM_DRUMS  4
#define NUM_STEPS  16

bool     pattern[NUM_DRUMS][NUM_STEPS];
int      cur_step  = 0;
bool     is_grab   = false;
float    bpm       = 120.0f;
uint32_t last_step = 0;

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
NoteOff noteOffs[8];

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
// ============================================================
const float EVT_SPEED[4] = {0.038f, 0.062f, 0.22f, 0.015f};

#define MAX_EVENTS 8
struct Event { bool active; uint8_t drum; float phase; float speed; int8_t dir; };
Event   events[MAX_EVENTS];
int8_t  hihatDir = 1;   // alterna con cada golpe de hihat

void spawnEvent(uint8_t drum) {
  // Buscar slot libre; si no hay, reemplazar el más avanzado
  int   slot = 0;
  float maxP = -1.0f;
  for (int i = 0; i < MAX_EVENTS; i++) {
    if (!events[i].active)         { slot = i; break; }
    if (events[i].phase > maxP)    { maxP = events[i].phase; slot = i; }
  }
  int8_t dir = (drum == 2) ? hihatDir : 1;
  if (drum == 2) hihatDir = -hihatDir;
  events[slot] = {true, drum, 0.0f, EVT_SPEED[drum], dir};
}

// ─── Pulso de beat ─────────────────────────────────────────
float beatPulse = 0.0f;   // 0–1, se inyecta en los tiempos fuertes

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

// ============================================================
//  FONDO AMBIENTE
//
//  Ola sinusoidal de color que ondula lentamente.
//  En los tiempos fuertes del compás, un pulso naranja-cálido
//  ilumina toda la tira con un pico muy corto.
// ============================================================
void renderBackground(uint8_t hueBase) {
  float t = millis() * 0.00085f;  // velocidad de la ola

  uint8_t beatFlash = (uint8_t)(beatPulse * beatPulse * 50);  // cuadrático = más punch

  for (int i = 0; i < ACTIVE_LEDS; i++) {
    float wave = sinf((float)i * 0.062f + t) * 0.5f + 0.5f;   // 0→1
    uint8_t h  = hueBase + (uint8_t)(wave * 35);
    uint8_t v  = (uint8_t)(6 + wave * 12);
    CRGB c; hsv2rgb_rainbow(CHSV(h, 220, v), c);
    // Beat pulse: naranja cálido sobre la ola de fondo
    c += CRGB(beatFlash, (uint8_t)(beatFlash * 0.55f), 0);
    leds[START_LED + i] = c;  // set (capa base, no aditiva)
  }
}

// ─── Indicador de paso: cometa con cola ────────────────────
void renderStepComet() {
  for (int t = 0; t < 5; t++) {
    int     s   = (cur_step - t + NUM_STEPS) % NUM_STEPS;
    int     pos = START_LED + (s * ACTIVE_LEDS) / NUM_STEPS;
    uint8_t v   = (t == 0) ? 100 : (100 >> (t * 2 - 1));  // 100, 50, 12, 3, 0
    leds[pos] += CRGB(v, v, v);
  }
}

// ─── Frame principal ───────────────────────────────────────
void renderFrame(uint8_t hueBase) {
  // Negro total como base: los LEDs solo brillan si hay un evento activo
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  for (auto &e : events) {
    if (!e.active) continue;
    switch (e.drum) {
      case 0: fxKick (e.phase);        break;
      case 1: fxSnare(e.phase);        break;
      case 2: fxHihat(e.phase, e.dir); break;
      case 3: fxCrash(e.phase);        break;
    }
    e.phase += e.speed;
    if (e.phase >= 1.0f) e.active = false;
  }

  FastLED.show();
}

// ─── Disparar drum: MIDI + LED ─────────────────────────────
void hitDrum(int drum, uint8_t vel) {
  MIDI.noteOn(NOTES[drum], vel, MIDI_CH);
  scheduleOff(NOTES[drum]);
  spawnEvent(drum);
  Serial.printf("HIT %d  note=%d  vel=%d\n", drum, NOTES[drum], vel);
}

// ─── Setup ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  MIDI.begin();
  USB.begin();

  for (int i = 0; i < 5; i++) pinMode(BTN[i], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  memset(pattern,  0, sizeof(pattern));
  memset(noteOffs, 0, sizeof(noteOffs));
  memset(events,   0, sizeof(events));

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(MAX_BRIGHT);
  FastLED.clear();
  FastLED.show();

  last_step = millis();

  Serial.println("=== PercuSynth v2 — Drum MIDI + LEDs ===");
  Serial.println("BTN1-4: Bombo/Caja/HiHat/Crash  |  BTN5: GRAB/PLAY");
  Serial.println("POT1: Brillo  POT2: Tempo  POT3: Color  POT4: Velocidad");
}

// ─── Loop ──────────────────────────────────────────────────
void loop() {
  static uint32_t lastFrame = 0;
  uint32_t now = millis();

  // Leer pots
  float bright_f = readPot(POT_BRIGHT);
  float tempo_f  = readPot(POT_TEMPO);
  float color_f  = readPot(POT_COLOR);
  float vel_f    = readPot(POT_VEL);

  bpm = 60.0f + tempo_f * 180.0f;
  uint32_t stepMs  = (uint32_t)(60000.0f / bpm / 4.0f);
  uint8_t  hueBase = (uint8_t)(color_f * 255.0f);
  uint8_t  vel     = (uint8_t)(60 + vel_f * 67.0f);

  FastLED.setBrightness((uint8_t)(15 + bright_f * (MAX_BRIGHT - 15)));

  readButtons();

  // BTN5: toggle GRAB / PLAYBACK
  if (btn_edge[4]) {
    is_grab = !is_grab;
    if (is_grab) {
      memset(pattern, 0, sizeof(pattern));
      cur_step  = 0;
      last_step = now;
      fill_solid(leds + START_LED, ACTIVE_LEDS, CRGB::White);
      FastLED.show();
      delay(120);
      FastLED.clear();
      Serial.println("► GRAB — patrón borrado");
    } else {
      Serial.printf("► PLAYBACK  BPM=%.0f\n", bpm);
    }
  }

  // BTN 1-4: trigger + grabar
  for (int i = 0; i < NUM_DRUMS; i++) {
    if (btn_edge[i]) {
      hitDrum(i, vel);
      if (is_grab) pattern[i][cur_step] = true;
    }
  }

  // Secuenciador
  if (now - last_step >= stepMs) {
    last_step += stepMs;
    cur_step = (cur_step + 1) % NUM_STEPS;

    if (!is_grab) {
      for (int d = 0; d < NUM_DRUMS; d++) {
        if (pattern[d][cur_step]) hitDrum(d, vel);
      }
    }
  }

  processNoteOffs();

  // Renderizar a ~30 fps
  if (now - lastFrame >= 33) {
    lastFrame = now;
    renderFrame(hueBase);
  }
}
