// ==============================================================================================================================================
// PERCU-SYNTH — Matrix MIDI "ANYMA" (secuenciador electro + motor visual 20x20) — GC Lab Chile
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
// - Matriz de LEDs WS2812 20x20 (400 LEDs) |DATA -> 46|
//     · Cableado PROGRESIVO: TODAS las filas van de izquierda a derecha (NO zigzag/serpentina)
//     · Los primeros 6 LEDs de la cadena son SMD internos del PCB → siempre apagados
// - 5 Botones con pull-up |BTN1 -> 42, BTN2 -> 44, BTN3 -> 45, BTN4 -> 47, BTN5 -> 0|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// - Salida MIDI USB nativa (batería ch10 GM + bajo ch1)
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
// - ESP32 Arduino core ≥ 3.x (USB.h, USBMIDI.h, TinyUSB)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// El PercuSynth se convierte en una "máquina audiovisual" electro estilo Anyma:
//
//   1) SECUENCIADOR INTERNO de 16 pasos con 4 patrones electro/techno (kick a la
//      negra, claps, hats rodando, bajo acid sincopado). Envía la base de batería
//      por USB MIDI ch10 (GM drums) + el bajo por ch1 → en tu DAW pones el sampler
//      de batería y el sinte de bajo que quieras.
//
//   2) MIDI CLOCK MASTER: emite reloj 24 PPQ (0xF8) + Start/Stop (0xFA/0xFC) de
//      forma continua → pon tu DAW en "External / MIDI Clock Sync" y queda pegado
//      al tempo del PercuSynth.
//
//   3) MOTOR VISUAL 2D sobre la matriz 20x20: 5 escenas espectaculares (NEXUS,
//      TÚNEL, ESPECTRO, TORMENTA, REJILLA) construidas con blending aditivo,
//      estela por motion-blur, sistema de partículas, ondas de choque radiales y
//      "pump" de brillo bloqueado al beat. Reacciona TANTO al secuenciador interno
//      COMO a notas MIDI entrantes de un secuenciador externo / DAW.
//
// Más que color, el foco es MOVIMIENTO: todo respira con el kick, las notas
// escupen partículas con estela y cada escena explota distinto con cada golpe.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// CONTROLES:
// - BTN1 (42) → Escena siguiente  (NEXUS · TÚNEL · ESPECTRO · TORMENTA · REJILLA)
// - BTN2 (44) → Patrón siguiente  (DRIVE · ANYMA · PEAK · BREAK)
// - BTN3 (45) → Play / Stop  (envía 0xFA/0xFC y arranca/para la base interna)
// - BTN4 (47) → IMPACTO manual: crash + supernova visual (úsalo en los breaks)
// - BTN5 (0)  → Blackout / pánico (toggle)
//
// - POT1 (ADC1)  → Brillo global (5 – 80 / 255)
// - POT2 (ADC2)  → BPM (90 – 150)
// - POT3 (ADC8)  → Color base / paleta (hue 0 – 255)
// - POT4 (ADC10) → Intensidad FX (estela larga + más partículas + caos)
//
// MIDI:
// - Salida USB "PercuSynth Anyma": batería ch10 (36 kick, 39 clap, 42 hat,
//   46 open hat, 75 perc) + bajo ch1. Reloj 24 PPQ continuo + Start/Stop.
// - Entrada USB: cualquier NoteOn entrante dispara visuales (ch10 = drums por tipo,
//   resto = notas melódicas → columnas/partículas). Así un secuenciador externo
//   también "pinta" la matriz.
//
// MODO DE USO:
// 1. Conecta el PercuSynth al PC → aparece como dispositivo MIDI.
// 2. En el DAW: clock IN = External (MIDI Clock), y enruta ch10/ch1 a tus instrumentos.
// 3. Arranca tocando: ya viene en PLAY con un groove. BTN2 cambia el patrón,
//    BTN1 cambia el show visual, los pots moldean tempo, color y caos.
// ==============================================================================================================================================

#include <Arduino.h>
#include <FastLED.h>
#include <math.h>
#include "USB.h"
#include "USBMIDI.h"

USBMIDI MIDI;

// ─── Geometría de la matriz ──────────────────────────────────
#define WIDTH        20
#define HEIGHT       20
#define START_LED     6                          // LEDs 0-5 SMD internos → siempre apagados
#define NUM_LEDS    (START_LED + WIDTH * HEIGHT)  // 6 + 400 = 406
#define FLIP_X       false
#define FLIP_Y       false

#define LED_PIN      46
#define LED_TYPE     WS2812
#define COLOR_ORDER  GRB
#define MAX_BRIGHT   80                           // tope duro: 400 LEDs consumen mucho

CRGB leds[NUM_LEDS];

const uint8_t BTN_PINS[] = { 42, 44, 45, 47, 0 };
const uint8_t POT_PINS[] = {  1,  2,  8, 10 };
#define NUM_BTNS 5

// Centro geométrico (para ondas radiales / túnel)
static const float CX = (WIDTH  - 1) * 0.5f;
static const float CY = (HEIGHT - 1) * 0.5f;

// ─── Mapeo XY → índice + dibujo aditivo ──────────────────────
inline uint16_t XY(uint8_t x, uint8_t y) {
  if (FLIP_X) x = WIDTH  - 1 - x;
  if (FLIP_Y) y = HEIGHT - 1 - y;
  return START_LED + (uint16_t)y * WIDTH + x;
}
// Suma color en (x,y) — FastLED satura en blanco (mezcla aditiva tipo HDR)
inline void addPixel(int x, int y, const CRGB& c) {
  if ((unsigned)x >= WIDTH || (unsigned)y >= HEIGHT) return;
  leds[XY(x, y)] += c;
}
inline void setPixel(int x, int y, const CRGB& c) {
  if ((unsigned)x >= WIDTH || (unsigned)y >= HEIGHT) return;
  leds[XY(x, y)] = c;
}
void fadeMatrix(uint8_t amt) { fadeToBlackBy(leds + START_LED, WIDTH * HEIGHT, amt); }
void clearMatrix()           { fill_solid(leds + START_LED, WIDTH * HEIGHT, CRGB::Black); }
void apagarInternos()        { for (int i = 0; i < START_LED; i++) leds[i] = CRGB::Black; }

// ─── Lectura de pots / botones ───────────────────────────────
int leerPot(uint8_t pin) {
  uint32_t s = 0;
  for (int i = 0; i < 8; i++) s += analogRead(pin);
  return s >> 3;
}
bool     btnPrev[NUM_BTNS]   = {};
uint32_t btnLastMs[NUM_BTNS] = {};
#define  DEBOUNCE_MS 35
bool botonFlanco(uint8_t i, uint32_t now) {           // flanco de bajada con debounce
  bool pressed = (digitalRead(BTN_PINS[i]) == LOW);
  bool edge = false;
  if (pressed != btnPrev[i] && (now - btnLastMs[i]) > DEBOUNCE_MS) {
    btnLastMs[i] = now;
    btnPrev[i] = pressed;
    if (pressed) edge = true;
  }
  return edge;
}

// ─── Parámetros globales (pots) ──────────────────────────────
uint8_t  brilloBase = 45;
uint8_t  hueBase    = 150;   // POT3
float    fxAmount   = 0.5f;  // POT4 (0..1): estela + densidad + caos
float    bpm        = 124.0f;

// ==============================================================================================================================================
// MIDI HELPERS (OUT) — TinyUSB raw packets
// ==============================================================================================================================================
inline void midiRT(uint8_t status) {                                  // realtime (clock/start/stop)
  uint8_t pkt[4] = { 0x0F, status, 0, 0 };
  tud_midi_n_packet_write(0, pkt);
}
inline void midiNoteOn(uint8_t ch, uint8_t note, uint8_t vel) {
  uint8_t pkt[4] = { 0x09, (uint8_t)(0x90 | (ch & 0x0F)), note, vel };
  tud_midi_n_packet_write(0, pkt);
}
inline void midiNoteOff(uint8_t ch, uint8_t note) {
  uint8_t pkt[4] = { 0x08, (uint8_t)(0x80 | (ch & 0x0F)), note, 0 };
  tud_midi_n_packet_write(0, pkt);
}

// Cola de NoteOff diferidos para la batería (gate corto, no acapara canales)
#define OFF_SLOTS 12
struct OffEvt { bool on; uint8_t ch, note; uint32_t at; };
OffEvt offQ[OFF_SLOTS];
void scheduleOff(uint8_t ch, uint8_t note, uint32_t at) {
  for (int i = 0; i < OFF_SLOTS; i++)
    if (!offQ[i].on) { offQ[i] = { true, ch, note, at }; return; }
}
void serviceOffQ(uint32_t now) {
  for (int i = 0; i < OFF_SLOTS; i++)
    if (offQ[i].on && (int32_t)(now - offQ[i].at) >= 0) {
      midiNoteOff(offQ[i].ch, offQ[i].note);
      offQ[i].on = false;
    }
}

// ==============================================================================================================================================
// MOTOR VISUAL — estado compartido (envolventes + partículas + eventos)
// ==============================================================================================================================================
// Envolventes por golpe: se ponen a 1.0 al disparar y decaen cada frame.
float envKick = 0, envClap = 0, envHat = 0, envOpen = 0, envCrash = 0, envPerc = 0, envBass = 0;

// Flags de golpe del frame en curso (los consume la escena activa y se limpian)
struct Hits { bool k, c, h, oh, cr, pc; float vk, vc, vh, voh, vcr, vpc; };
Hits hit;

// Cola de notas melódicas entrantes para el frame (note + vel)
#define MEL_SLOTS 16
struct MelEvt { uint8_t note, vel; };
MelEvt melQ[MEL_SLOTS];
uint8_t melCount = 0;
void pushMel(uint8_t note, uint8_t vel) {
  if (melCount < MEL_SLOTS) melQ[melCount++] = { note, vel };
}

// Sistema de partículas (compartido por las escenas que lo usan)
#define MAX_P 70
struct Particle { float x, y, vx, vy, life, decay; uint8_t h, s; bool on; };
Particle P[MAX_P];
void spawn(float x, float y, float vx, float vy, uint8_t h, uint8_t s, float decay) {
  for (int i = 0; i < MAX_P; i++)
    if (!P[i].on) { P[i] = { x, y, vx, vy, 1.0f, decay, h, s, true }; return; }
}
// Estallido radial de n partículas desde (x,y)
void burst(float x, float y, int n, float speed, uint8_t h, uint8_t s, float decay) {
  for (int i = 0; i < n; i++) {
    float a  = (float)i / n * 6.2832f;
    float sp = speed * (0.4f + 0.6f * ((i * 53) % 17) / 17.0f);
    spawn(x, y, cosf(a) * sp, sinf(a) * sp, h + (uint8_t)(i * 3), s, decay);
  }
}
void updateParticles(float gravity) {
  for (int i = 0; i < MAX_P; i++) {
    if (!P[i].on) continue;
    uint8_t v = (uint8_t)(P[i].life * 255.0f);
    addPixel((int)(P[i].x + 0.5f), (int)(P[i].y + 0.5f), CHSV(P[i].h, P[i].s, v));
    P[i].x += P[i].vx; P[i].y += P[i].vy; P[i].vy += gravity;
    P[i].life -= P[i].decay;
    if (P[i].life <= 0) P[i].on = false;
  }
}

// ─── Mapeo nota → tipo de drum / disparo de visuales ─────────
// type: 0 kick · 1 clap/snare · 2 hat cerrado · 3 hat abierto · 4 crash/ride · 5 perc/tom · 255 = melódico
uint8_t drumType(uint8_t note) {
  switch (note) {
    case 35: case 36:                       return 0;
    case 37: case 38: case 39: case 40:     return 1;
    case 42: case 44:                       return 2;
    case 46:                                return 3;
    case 49: case 51: case 52: case 55: case 57: return 4;
    case 41: case 43: case 45: case 47: case 48: case 50:
    case 75: case 76: case 77:              return 5;
    default:                                return 255;
  }
}
void onDrumHit(uint8_t type, uint8_t vel) {
  float v = vel / 127.0f;
  switch (type) {
    case 0: hit.k  = true; hit.vk  = v; envKick  = 1.0f;            break;
    case 1: hit.c  = true; hit.vc  = v; envClap  = 1.0f;            break;
    case 2: hit.h  = true; hit.vh  = v; envHat   = max(envHat, v);  break;
    case 3: hit.oh = true; hit.voh = v; envOpen  = 1.0f;            break;
    case 4: hit.cr = true; hit.vcr = v; envCrash = 1.0f;            break;
    case 5: hit.pc = true; hit.vpc = v; envPerc  = 1.0f;            break;
  }
}
void onMelodic(uint8_t note, uint8_t vel) { envBass = max(envBass, vel / 127.0f); pushMel(note, vel); }

// ==============================================================================================================================================
// SECUENCIADOR ELECTRO — 16 pasos · 4 patrones · base de batería + bajo
// ==============================================================================================================================================
#define NUM_STEPS 16
#define NUM_PRESETS 4
#define PPQ 24
#define TICKS_PER_STEP 6   // 24 PPQ / 4 = semicorcheas

// Notas GM de batería
#define N_KICK 36
#define N_CLAP 39
#define N_HAT  42
#define N_OPEN 46
#define N_PERC 75
#define CH_DRUM 9          // ch10 (0-based)
#define CH_BASS 0          // ch1

const char* nombrePreset[NUM_PRESETS] = { "DRIVE", "ANYMA", "PEAK", "BREAK" };

// Velocidades por paso (0 = silencio). [preset][step]
const uint8_t patKick[NUM_PRESETS][NUM_STEPS] = {
  {127,0,0,0, 120,0,0,0, 127,0,0,0, 120,0,0,0},                 // DRIVE  4x4
  {127,0,0,0, 118,0,0,60, 127,0,0,0, 118,0,70,0},               // ANYMA  4x4 + ghosts
  {127,0,0,0, 120,0,0,0, 127,0,0,0, 110,0,110,110},             // PEAK   + roll final
  {127,0,0,0, 0,0,90,0, 0,0,127,0, 0,0,0,90},                   // BREAK  roto / half-time
};
const uint8_t patClap[NUM_PRESETS][NUM_STEPS] = {
  {0,0,0,0, 110,0,0,0, 0,0,0,0, 110,0,0,0},                     // DRIVE  2 y 4
  {0,0,0,0, 115,0,0,50, 0,0,0,0, 115,0,0,0},                    // ANYMA
  {0,0,0,0, 120,0,0,0, 0,0,0,0, 120,0,90,90},                   // PEAK
  {0,0,0,0, 0,0,0,0, 110,0,0,0, 0,0,0,0},                       // BREAK
};
const uint8_t patHat[NUM_PRESETS][NUM_STEPS] = {
  {50,0,80,0, 50,0,80,0, 50,0,80,0, 50,0,80,0},                 // DRIVE  16ths c/ acento offbeat
  {40,60,40,90, 40,60,40,90, 40,60,40,90, 40,60,40,95},         // ANYMA  rodando
  {70,70,80,90, 70,70,80,90, 70,70,80,95, 80,90,100,110},       // PEAK   build
  {0,0,70,0, 0,0,70,0, 0,0,70,0, 0,0,80,0},                     // BREAK
};
const uint8_t patOpen[NUM_PRESETS][NUM_STEPS] = {
  {0,0,0,0, 0,0,110,0, 0,0,0,0, 0,0,110,0},                     // DRIVE  open en el "&"
  {0,0,0,0, 0,0,100,0, 0,0,0,0, 0,0,105,0},                     // ANYMA
  {0,0,0,0, 0,0,100,0, 0,0,0,0, 0,0,0,0},                       // PEAK
  {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,110,0},                       // BREAK
};
const uint8_t patPerc[NUM_PRESETS][NUM_STEPS] = {
  {0,0,0,0, 0,0,0,90, 0,0,0,0, 0,0,0,90},                       // DRIVE
  {0,0,80,0, 0,70,0,0, 0,0,80,0, 0,0,0,75},                     // ANYMA
  {0,90,0,0, 0,0,90,0, 0,90,0,0, 0,0,90,0},                     // PEAK
  {0,0,0,80, 0,0,0,0, 0,0,0,80, 0,90,0,0},                      // BREAK
};
// Bajo melódico (número de nota MIDI, 0 = silencio). La menor: A1=33.
const uint8_t patBass[NUM_PRESETS][NUM_STEPS] = {
  {33,0,0,33, 0,0,33,0, 36,0,0,36, 0,0,40,0},                   // DRIVE
  {33,0,33,0, 36,0,33,0, 40,0,33,0, 0,40,0,45},                 // ANYMA acid
  {33,33,0,33, 0,33,0,36, 40,40,0,40, 0,45,43,40},              // PEAK
  {33,0,0,0, 0,0,33,0, 0,0,40,0, 0,0,0,0},                      // BREAK
};
const uint8_t swingPreset[NUM_PRESETS] = { 0, 1, 0, 1 };        // ticks de swing en steps impares

uint8_t  preset      = 1;          // arranca en ANYMA
uint8_t  curStep     = 0;
bool     playing     = true;
uint8_t  bassNoteOn  = 0;          // nota de bajo sonando (para NoteOff)

// Reloj
uint32_t clockIntervalUs;
uint32_t nextClockUs = 0;
uint8_t  seqTick     = 0;          // ticks dentro del step actual

void recomputeClock() {
  if (bpm < 60.0f)  bpm = 60.0f;
  if (bpm > 200.0f) bpm = 200.0f;
  clockIntervalUs = (uint32_t)(60000000.0f / (bpm * (float)PPQ));
}

void fireStep(uint8_t s) {
  uint8_t v;
  if ((v = patKick[preset][s])) { midiNoteOn(CH_DRUM, N_KICK, v); scheduleOff(CH_DRUM, N_KICK, micros() + 25000); onDrumHit(0, v); }
  if ((v = patClap[preset][s])) { midiNoteOn(CH_DRUM, N_CLAP, v); scheduleOff(CH_DRUM, N_CLAP, micros() + 30000); onDrumHit(1, v); }
  if ((v = patHat [preset][s])) { midiNoteOn(CH_DRUM, N_HAT,  v); scheduleOff(CH_DRUM, N_HAT,  micros() + 15000); onDrumHit(2, v); }
  if ((v = patOpen[preset][s])) { midiNoteOn(CH_DRUM, N_OPEN, v); scheduleOff(CH_DRUM, N_OPEN, micros() + 60000); onDrumHit(3, v); }
  if ((v = patPerc[preset][s])) { midiNoteOn(CH_DRUM, N_PERC, v); scheduleOff(CH_DRUM, N_PERC, micros() + 25000); onDrumHit(5, v); }
  // Bajo: nota sostenida hasta el próximo evento de bajo
  uint8_t bn = patBass[preset][s];
  if (bn) {
    if (bassNoteOn) midiNoteOff(CH_BASS, bassNoteOn);
    midiNoteOn(CH_BASS, bn, 100);
    bassNoteOn = bn;
    onMelodic(bn, 100);
  }
}

void advanceStep() {
  curStep = (curStep + 1) % NUM_STEPS;
  fireStep(curStep);
}
void doStart() {
  playing = true; seqTick = 0; curStep = 0;
  midiRT(0xFA);
  fireStep(0);
}
void doStop() {
  playing = false;
  if (bassNoteOn) { midiNoteOff(CH_BASS, bassNoteOn); bassNoteOn = 0; }
  midiRT(0xFC);
}

// ==============================================================================================================================================
// MIDI IN — un secuenciador externo / DAW también pinta la matriz
// ==============================================================================================================================================
void handleMidiIn(uint8_t b1, uint8_t b2, uint8_t b3) {
  uint8_t status = b1 & 0xF0;
  uint8_t chan   = b1 & 0x0F;
  if (status == 0x90 && b3 > 0) {                  // NoteOn real
    if (chan == CH_DRUM) {
      uint8_t t = drumType(b2);
      if (t != 255) onDrumHit(t, b3); else onMelodic(b2, b3);
    } else {
      uint8_t t = drumType(b2);                    // por si mandan drums en otro canal
      if (t != 255 && (b2 == 36 || b2 == 38)) onDrumHit(t, b3);
      else onMelodic(b2, b3);
    }
  }
}
void pollMidi() {
  uint8_t pkt[4];
  while (tud_midi_n_packet_read(0, pkt)) handleMidiIn(pkt[1], pkt[2], pkt[3]);
}

// ==============================================================================================================================================
// ESCENAS VISUALES
// ==============================================================================================================================================
#define NUM_SCENES 5
const char* nombreScene[NUM_SCENES] = { "NEXUS", "TUNEL", "ESPECTRO", "TORMENTA", "REJILLA" };
uint8_t scene = 0;

uint32_t frameCount = 0;
float    plasmaT = 0;

// Onda de choque radial reutilizable (anillo que se expande con env)
void drawShock(float env, uint8_t hue, float width) {
  if (env <= 0.01f) return;
  float r = (1.0f - env) * 15.0f;                  // crece hacia afuera
  for (int y = 0; y < HEIGHT; y++)
    for (int x = 0; x < WIDTH; x++) {
      float d  = hypotf(x - CX, y - CY);
      float dd = fabsf(d - r);
      if (dd < width) {
        uint8_t v = (uint8_t)((1.0f - dd / width) * env * 255.0f);
        addPixel(x, y, CHSV(hue, 220, v));
      }
    }
}

// ── Escena 0: NEXUS — plasma fluido + partículas con estela ──────────────────
void sceneNexus() {
  fadeMatrix(40 + (uint8_t)(fxAmount * 30));       // estela: POT4 la alarga
  // Plasma base (aditivo, tenue) — movimiento orgánico constante
  for (int y = 0; y < HEIGHT; y++)
    for (int x = 0; x < WIDTH; x++) {
      float v = sinf(x * 0.42f + plasmaT * 1.7f)
              + sinf(y * 0.37f - plasmaT * 1.3f)
              + sinf((x + y) * 0.22f + plasmaT)
              + sinf(hypotf(x - CX, y - CY) * 0.5f - plasmaT * 2.0f);
      uint8_t h = hueBase + (uint8_t)(v * 26.0f);
      uint8_t b = 14 + (uint8_t)((v + 4.0f) * 6.0f);     // base muy tenue
      addPixel(x, y, CHSV(h, 235, b));
    }
  // Reacciones
  if (hit.k)  { drawShock(1.0f, hueBase + 20, 2.2f); burst(CX, CY, 16, 0.9f, hueBase + 40, 255, 0.05f); }
  drawShock(envKick, hueBase + 20, 2.0f);
  if (hit.c)  burst(CX, CY, 24, 1.4f, hueBase + 128, 200, 0.06f);   // clap = anillo amplio color complementario
  if (hit.h)  spawn(random8(WIDTH), random8(4), 0, 0.5f, hueBase + 80, 180, 0.10f);
  if (hit.oh) for (int i = 0; i < 4; i++) spawn(random8(WIDTH), random8(HEIGHT), 0, 0, hueBase + 96, 160, 0.07f);
  // Notas melódicas → cohetes verticales con estela
  for (int i = 0; i < melCount; i++) {
    int x = map(melQ[i].note % 24, 0, 23, 0, WIDTH - 1);
    spawn(x, HEIGHT - 1, 0, -1.2f - melQ[i].vel / 127.0f, hueBase + 64 + melQ[i].note, 255, 0.04f);
  }
  updateParticles(0.0f);
}

// ── Escena 1: TÚNEL — espiral concéntrica que zooma con el kick ──────────────
void sceneTunnel() {
  float punch = envKick * 3.0f;                     // el kick "empuja" el túnel
  for (int y = 0; y < HEIGHT; y++)
    for (int x = 0; x < WIDTH; x++) {
      float dx = x - CX, dy = y - CY;
      float d  = hypotf(dx, dy);
      float a  = atan2f(dy, dx);
      float phase = d * (1.1f + fxAmount) - plasmaT * 4.0f + a * 3.0f + punch;
      float ring  = sinf(phase);
      ring = ring * ring * ring * ring;             // anillos finos y nítidos
      uint8_t h = hueBase + (uint8_t)(d * 10.0f + plasmaT * 12.0f);
      uint8_t b = (uint8_t)(ring * (90 + envKick * 120));
      setPixel(x, y, CHSV(h, 230, b));
    }
  // Destellos: clap = flash radial, hats = chispas en el borde
  if (hit.c)  for (int y = 0; y < HEIGHT; y++) for (int x = 0; x < WIDTH; x++) addPixel(x, y, CHSV(hueBase + 128, 120, 70));
  if (hit.h)  { float a = random8() / 40.0f; addPixel((int)(CX + cosf(a) * 9), (int)(CY + sinf(a) * 9), CHSV(hueBase + 64, 200, 220)); }
  for (int i = 0; i < melCount; i++) {              // notas = anillo de luz a su radio
    float r = map(melQ[i].note % 24, 0, 23, 2, 13);
    for (int k = 0; k < 24; k++) { float a = k / 24.0f * 6.2832f; addPixel((int)(CX + cosf(a) * r), (int)(CY + sinf(a) * r), CHSV(hueBase + melQ[i].note, 255, melQ[i].vel + 60)); }
  }
}

// ── Escena 2: ESPECTRO — columnas por nota + caps que caen + kick shock ──────
float colH[WIDTH] = {0};
float colPk[WIDTH] = {0};
void sceneSpectrum() {
  fadeMatrix(120);
  // Notas → altura de columna
  for (int i = 0; i < melCount; i++) {
    int x = map(melQ[i].note % 24, 0, 23, 0, WIDTH - 1);
    float h = (melQ[i].vel / 127.0f) * (HEIGHT - 1);
    if (h > colH[x]) colH[x] = h;
  }
  // Kick / clap llenan el espectro de golpe
  if (hit.k) for (int x = 0; x < WIDTH; x++) colH[x] = max(colH[x], (HEIGHT - 1) * (0.4f + 0.5f * (random8() / 255.0f)) * hit.vk);
  if (hit.c) for (int x = 0; x < WIDTH; x++) colH[x] = max(colH[x], (float)(HEIGHT - 1) * hit.vc);
  // Dibujo de barras + cap
  for (int x = 0; x < WIDTH; x++) {
    int top = (int)colH[x];
    for (int y = 0; y <= top; y++) {
      uint8_t h = hueBase + x * 5 + y * 3;
      uint8_t b = 60 + (y * 180) / HEIGHT;
      addPixel(x, HEIGHT - 1 - y, CHSV(h, 235, b));
    }
    if (colH[x] > colPk[x]) colPk[x] = colH[x];
    if (colPk[x] > 0.2f) addPixel(x, HEIGHT - 1 - (int)colPk[x], CHSV(hueBase + 128, 80, 255)); // cap blanco-pastel
    colH[x]  *= 0.82f;                              // caída de barra
    colPk[x] -= 0.35f + fxAmount * 0.4f;            // caída del cap
    if (colPk[x] < 0) colPk[x] = 0;
  }
  drawShock(envKick, hueBase, 1.6f);
}

// ── Escena 3: TORMENTA — física pura de partículas con gravedad ──────────────
void sceneStorm() {
  fadeMatrix(55 + (uint8_t)(fxAmount * 25));
  if (hit.k)  burst(CX, CY, 28, 1.6f, hueBase, 255, 0.035f);                         // explosión central
  if (hit.c)  for (int i = 0; i < 14; i++) spawn(random8(WIDTH), 0, 0, 0.6f + random8() / 200.0f, hueBase + 128, 200, 0.03f); // lluvia
  if (hit.h)  spawn(random8(WIDTH), random8(HEIGHT), (random8() - 128) / 90.0f, 0, hueBase + 80, 160, 0.12f);
  if (hit.oh) burst(random8(WIDTH), random8(HEIGHT), 10, 1.0f, hueBase + 96, 180, 0.05f);
  for (int i = 0; i < melCount; i++) {
    int x = map(melQ[i].note % 24, 0, 23, 0, WIDTH - 1);
    spawn(x, HEIGHT - 1, (random8() - 128) / 160.0f, -1.6f, hueBase + melQ[i].note, 255, 0.03f);
  }
  updateParticles(0.035f);                          // gravedad suave hacia abajo
}

// ── Escena 4: REJILLA — celdas geométricas que laten con el beat ─────────────
void sceneGrid() {
  fadeMatrix(90);
  const int CELL = 4;                               // 5x5 celdas de 4x4 px
  static float cellE[25] = {0};
  // Notas encienden celdas
  for (int i = 0; i < melCount; i++) cellE[melQ[i].note % 25] = 1.0f;
  if (hit.k) for (int i = 0; i < 25; i++) cellE[i] = max(cellE[i], 0.9f * hit.vk);  // kick = toda la rejilla pulsa
  if (hit.c) for (int i = 12; i < 25; i += 2) cellE[i] = 1.0f;
  if (hit.h) cellE[random8(25)] = 1.0f;
  float pump = 0.25f + envKick * 0.75f;
  for (int cy = 0; cy < 5; cy++)
    for (int cx = 0; cx < 5; cx++) {
      int idx = cy * 5 + cx;
      float e = cellE[idx];
      uint8_t h = hueBase + (cx + cy) * 14 + (uint8_t)(plasmaT * 8);
      uint8_t b = (uint8_t)((10 + e * 230) * pump);
      // marco de la celda (más nítido que rellenarla → look geométrico)
      for (int k = 0; k < CELL; k++) {
        addPixel(cx * CELL + k, cy * CELL,     CHSV(h, 230, b));
        addPixel(cx * CELL + k, cy * CELL + CELL - 1, CHSV(h, 230, b));
        addPixel(cx * CELL,     cy * CELL + k, CHSV(h, 230, b));
        addPixel(cx * CELL + CELL - 1, cy * CELL + k, CHSV(h, 230, b));
      }
      cellE[idx] *= 0.86f;
    }
  // Haz de barrido sincronizado al step
  int sweep = map(curStep, 0, NUM_STEPS - 1, 0, WIDTH - 1);
  for (int y = 0; y < HEIGHT; y++) addPixel(sweep, y, CHSV(hueBase + 128, 60, 120));
}

// ── Overlay de IMPACTO manual (BTN4) — supernova que decae sobre cualquier escena
void drawSupernova() {
  if (envCrash <= 0.02f) return;
  float r = (1.0f - envCrash) * 16.0f;
  for (int y = 0; y < HEIGHT; y++)
    for (int x = 0; x < WIDTH; x++) {
      float d = hypotf(x - CX, y - CY);
      if (d <= r) {
        uint8_t h = hueBase + (uint8_t)(d * 12);
        uint8_t b = (uint8_t)((1.0f - d / 16.0f) * envCrash * 255.0f);
        addPixel(x, y, CHSV(h, 200 - (uint8_t)(d * 8), b));
      }
    }
}

// ==============================================================================================================================================
// SETUP
// ==============================================================================================================================================
void setup() {
  Serial.begin(115200); delay(100);
  Serial.println("PercuSynth — Matrix MIDI ANYMA boot...");

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  for (int i = 0; i < NUM_BTNS; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(brilloBase);
  FastLED.clear(); FastLED.show();

  recomputeClock();
  MIDI.begin();
  USB.begin();

  doStart();   // arranca con groove + visuales al toque
  Serial.printf("Listo. Escena=%s  Patron=%s  BPM=%.0f\n", nombreScene[scene], nombrePreset[preset], bpm);
  Serial.println("BTN1=escena BTN2=patron BTN3=play/stop BTN4=impacto BTN5=blackout");
}

// ==============================================================================================================================================
// MAIN LOOP
// ==============================================================================================================================================
bool blackout = false;

void loop() {
  uint32_t now   = millis();
  uint32_t nowUs = micros();

  pollMidi();
  serviceOffQ(now);

  // ── MIDI CLOCK MASTER: 0xF8 continuo + avance del secuenciador ──────────────
  while ((int32_t)(nowUs - nextClockUs) >= 0) {
    midiRT(0xF8);
    if (playing) {
      seqTick++;
      uint8_t needed = TICKS_PER_STEP + ((curStep & 1) ? swingPreset[preset] : -swingPreset[preset]);
      if (seqTick >= needed) { seqTick = 0; advanceStep(); }
    }
    nextClockUs += clockIntervalUs;
    nowUs = micros();
    if ((int32_t)(nowUs - nextClockUs) > 100000) nextClockUs = nowUs;   // resync si nos atrasamos
  }

  // ── Pots (cada ~30 ms, no en cada frame) ────────────────────────────────────
  static uint32_t lastPot = 0;
  if (now - lastPot > 30) {
    lastPot = now;
    brilloBase = map(leerPot(POT_PINS[0]), 0, 4095, 5, MAX_BRIGHT);
    float nb   = map(leerPot(POT_PINS[1]), 0, 4095, 90, 150);
    if (fabsf(nb - bpm) > 0.6f) { bpm = nb; recomputeClock(); }
    hueBase    = map(leerPot(POT_PINS[2]), 0, 4095, 0, 255);
    fxAmount   = leerPot(POT_PINS[3]) / 4095.0f;
  }

  // ── Botones ─────────────────────────────────────────────────────────────────
  if (botonFlanco(0, now)) { scene = (scene + 1) % NUM_SCENES; clearMatrix(); Serial.printf("Escena: %s\n", nombreScene[scene]); }
  if (botonFlanco(1, now)) { preset = (preset + 1) % NUM_PRESETS; Serial.printf("Patron: %s\n", nombrePreset[preset]); }
  if (botonFlanco(2, now)) { playing ? doStop() : doStart(); Serial.println(playing ? "PLAY" : "STOP"); }
  if (botonFlanco(3, now)) {                                   // impacto manual
    midiNoteOn(CH_DRUM, 49, 120); scheduleOff(CH_DRUM, 49, micros() + 80000);
    onDrumHit(4, 120); envCrash = 1.0f;
  }
  if (botonFlanco(4, now)) { blackout = !blackout; if (blackout) { FastLED.clear(); FastLED.show(); } Serial.println(blackout ? "BLACKOUT" : "ON"); }

  // ── Render de frame (~70 fps tope) ──────────────────────────────────────────
  static uint32_t lastFrame = 0;
  if (blackout || (now - lastFrame < 14)) return;
  lastFrame = now;

  plasmaT += 0.045f + fxAmount * 0.03f;
  frameCount++;

  switch (scene) {
    case 0: sceneNexus();    break;
    case 1: sceneTunnel();   break;
    case 2: sceneSpectrum(); break;
    case 3: sceneStorm();    break;
    case 4: sceneGrid();     break;
  }
  drawSupernova();                                  // overlay del impacto manual

  // Decaimiento de envolventes (por frame)
  envKick  *= 0.86f; envClap *= 0.80f; envHat *= 0.70f;
  envOpen  *= 0.88f; envCrash *= 0.93f; envPerc *= 0.85f; envBass *= 0.88f;

  // "Pump" de brillo global bloqueado al kick → toda la matriz respira con el beat
  uint8_t bril = (uint8_t)min((float)MAX_BRIGHT, brilloBase * (0.72f + 0.55f * envKick));
  FastLED.setBrightness(bril);

  // Limpieza de eventos del frame
  hit = Hits{};
  melCount = 0;

  apagarInternos();
  FastLED.show();
}
