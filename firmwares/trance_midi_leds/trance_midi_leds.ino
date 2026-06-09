// ==============================================================================================================================================
// PERCU-SYNTH — Secuenciador MIDI de Trance + Matriz 20x20 reactiva (IMU) — GC Lab Chile
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
//     · Cableado PROGRESIVO: todas las filas van de izquierda a derecha (NO zigzag)
//     · Los primeros 6 LEDs de la cadena son SMD internos del PCB → siempre apagados
// - IMU MPU6050 (acelerómetro I2C) |SDA -> 21, SCL -> 38, VCC -> 3.3V, GND -> GND|  (dirección 0x68)
// - 5 Botones con pull-up |BTN1 -> 44 (Play), BTN2 -> 42 (Escala), BTN3 -> 0 (Patrón), BTN4 -> 45 (Largo), BTN5 -> 47 (Octava)|
// - 4 Potenciómetros analógicos |POT1 -> ADC1 (Velocity), POT2 -> ADC2 (Brillo), POT3 -> ADC8 (Tempo), POT4 -> ADC10 (Gate)|
// - Salida MIDI USB nativa (canal 1)  →  el sonido lo pone tu DAW / sinte externo
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - USB Mode           : USB-OTG (TinyUSB)      ← obligatorio para MIDI USB
// - Flash Mode         : DIO
// - PSRAM              : OPI PSRAM
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - FastLED (gestor de librerías Arduino)
// - ESP32 Arduino core ≥ 3.x  (USB.h, USBMIDI.h, Wire.h)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// El mismo motor de secuenciador del firmware "trance_imu" (escalas, patrones,
// octava), pero MONOFÓNICO y MELÓDICO: cada paso envía UNA sola nota (la melodía
// del patrón) por MIDI USB a tu DAW o sinte, y cada nota nueva corta la anterior
// (true mono, sin notas solapadas). El IMU se traduce a MIDI CC (CC74 filtro /
// CC71 resonancia) para que muevas el aparato y barras el filtro en el sinte.
//
// En paralelo, la matriz 20x20 corre un show estilo FIESTA ELECTRÓNICA:
//   - SILENCIOS: la matriz queda APAGADA → estrobo natural al ritmo del beat
//   - CADA NOTA: una figura distinta (círculo, anillo, cuadrado, marco, rombo,
//               cruz, X, triángulo, estrella…) que EXPLOTA desde el centro
//               (zoom + fade), coloreada por la nota, sobre un plasma NEÓN que
//               brilla con el golpe, más un FLASH de impacto y una ONDA DE
//               CHOQUE que se expande por toda la matriz
//   - PARADO  : plasma lento y tenue (el aparato "respira")
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// CONTROLES:
// - BTN1 (44) → Play / Stop  (al detener envía All-Notes-Off)
// - BTN2 (42) → Cambiar escala   (Mayor · Menor · Armónica · Árabe · Lidia · Frigia)
// - BTN3 (0)  → Cambiar patrón    (16 patrones)
// - BTN4 (45) → Largo de secuencia (4 → 8 → 16 pasos)
// - BTN5 (47) → Octava            (-1 → 0 → +1 → +2 respecto a C3)
//
// - POT1 (ADC1)  → Velocity MIDI   (40 – 127)
// - POT2 (ADC2)  → Brillo de la matriz (8 – MAX_BRIGHT)
// - POT3 (ADC8)  → Tempo           (40 – 300 BPM, notas de 1/16)
// - POT4 (ADC10) → Gate / duración de nota (5 % – 95 % del paso → staccato a ligado)
//
// - IMU (MPU6050): muévelo / inclínalo →
//     · Aceleración eje X → MIDI CC74 (corte de filtro) + tono del plasma
//     · Aceleración eje Y → MIDI CC71 (resonancia)      + velocidad del plasma
//
// MODO DE USO:
// 1. Conecta el PercuSynth al PC → aparece como dispositivo MIDI "ESP32-S3".
// 2. En tu DAW selecciónalo como entrada MIDI (canal 1) y carga un sinte.
// 3. BTN1 arranca la secuencia. POT3 = tempo, POT4 = qué tan ligadas suenan las notas.
// 4. BTN2/BTN3/BTN5 cambian escala, patrón y octava para improvisar.
// 5. Mueve el aparato: el IMU manda CC al filtro del sinte y acelera el plasma.
// ==============================================================================================================================================

#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include "USB.h"
#include "USBMIDI.h"

USBMIDI MIDI;

// ─── Geometría de la matriz ──────────────────────────────────
#define WIDTH        20
#define HEIGHT       20
#define START_LED     6                         // LEDs 0-5 SMD internos → apagados
#define NUM_LEDS    (START_LED + WIDTH * HEIGHT) // 6 + 400 = 406
#define FLIP_X       false                       // espejar columnas si tu matriz quedara al revés
#define FLIP_Y       false                       // espejar filas

// ─── LED ───────────────────────────────────────────────────
#define LED_PIN      46
#define LED_TYPE     WS2812
#define COLOR_ORDER  GRB
#define MAX_BRIGHT   70                          // 400 LEDs: mantener moderado por consumo
#define FRAME_MS     25                          // ~40 fps de animación

CRGB leds[NUM_LEDS];
uint8_t  matBright = 40;

// ─── IMU MPU6050 (I2C) ─────────────────────────────────────
#define SDA_PIN     21
#define SCL_PIN     38
#define IMU_ADDR    0x68
const unsigned long IMU_READ_INTERVAL = 12;      // ms entre lecturas
const float IMU_FILTER_ALPHA = 0.15f;            // suavizado
float imu_x = 0, imu_y = 0, filtered_x = 0, filtered_y = 0;
unsigned long lastIMURead = 0;

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
#define BTN_PLAY     44
#define BTN_SCALE    42
#define BTN_PATTERN   0
#define BTN_LENGTH   45
#define BTN_OCTAVE   47
const uint8_t BTN_PIN[5] = {BTN_PLAY, BTN_SCALE, BTN_PATTERN, BTN_LENGTH, BTN_OCTAVE};
const unsigned long DEBOUNCE_MS = 180;

// ─── Potenciómetros ────────────────────────────────────────
#define POT_VEL      1    // ADC1  → velocity
#define POT_BRIGHT   2    // ADC2  → brillo matriz
#define POT_TEMPO    8    // ADC8  → tempo
#define POT_GATE    10    // ADC10 → gate (duración de nota)

// ─── MIDI ──────────────────────────────────────────────────
#define MIDI_CH       0                          // canal 1 (0-indexado)
#define BASE_NOTE    48                          // C3 (igual referencia que trance_imu)
#define CC_CUTOFF    74                          // IMU X → corte de filtro
#define CC_RESON     71                          // IMU Y → resonancia
uint8_t  velocity   = 100;
uint8_t  ccCutoffLast = 255, ccResonLast = 255;

// ─── Estado del secuenciador (= trance_imu) ────────────────
bool  isPlaying      = false;
int   currentStep    = 0;
int   sequenceLength = 16;
int   currentPattern = 0;
int   currentScale   = 0;
int   octaveIndex    = 1;
const int OCTAVE_SEMITONES[4] = {-12, 0, 12, 24};

uint32_t lastStepMs  = 0;
uint32_t stepMs      = 125;                        // recalculado por tempo
float    gateFrac    = 0.6f;                       // fracción del paso que dura la nota

// ─── Escalas (semitonos) ───────────────────────────────────
const int scales[][8] = {
  {0, 2, 4, 5, 7, 9, 11, 12},  // Mayor
  {0, 2, 3, 5, 7, 8, 10, 12},  // Menor natural
  {0, 2, 3, 5, 7, 8, 11, 12},  // Menor armónica
  {0, 1, 3, 4, 6, 8, 10, 12},  // Árabe
  {0, 2, 4, 6, 7, 9, 11, 12},  // Lidia
  {0, 1, 4, 5, 7, 8, 11, 12}   // Frigia
};
const int numScales = 6;

// ─── Patrones (0 = silencio, 1-7 = grado de la escala) ─────
const int patterns[][16] = {
  {1, 0, 3, 0, 5, 0, 3, 0, 1, 0, 3, 0, 5, 0, 7, 0},
  {1, 0, 0, 0, 5, 0, 0, 0, 3, 0, 0, 0, 7, 0, 0, 0},
  {1, 3, 5, 7, 5, 3, 1, 0, 1, 3, 5, 7, 5, 3, 1, 0},
  {7, 5, 3, 1, 3, 5, 7, 0, 7, 5, 3, 1, 3, 5, 7, 0},
  {1, 1, 5, 5, 3, 3, 7, 0, 1, 1, 5, 5, 3, 3, 7, 0},
  {1, 0, 5, 3, 0, 7, 1, 0, 5, 0, 3, 7, 0, 1, 5, 0},
  {1, 3, 0, 5, 7, 0, 3, 1, 5, 7, 0, 1, 3, 0, 5, 7},
  {1, 0, 0, 5, 0, 0, 1, 5, 0, 3, 0, 7, 0, 1, 0, 0},
  {7, 7, 5, 5, 3, 3, 1, 1, 7, 7, 5, 5, 3, 3, 1, 1},
  {1, 5, 1, 7, 1, 3, 1, 5, 1, 7, 1, 3, 1, 5, 1, 0},
  {0, 1, 0, 3, 0, 5, 0, 7, 0, 5, 0, 3, 0, 1, 0, 0},
  {1, 3, 5, 1, 7, 5, 3, 7, 1, 3, 5, 1, 7, 5, 3, 0},
  {5, 0, 5, 0, 1, 0, 1, 0, 3, 0, 3, 0, 7, 0, 7, 0},
  {1, 7, 1, 5, 1, 3, 1, 7, 5, 7, 5, 3, 5, 1, 5, 0},
  {3, 3, 3, 5, 5, 5, 1, 0, 7, 7, 7, 5, 5, 5, 3, 0},
  {1, 0, 7, 0, 5, 0, 3, 0, 5, 0, 7, 0, 1, 0, 0, 0}
};
const int numPatterns = 16;

// ─── Nota MIDI activa (monofónico: solo una a la vez) ──────
int       activeNote = -1;        // -1 = ninguna sonando
uint32_t  noteOffAt  = 0;         // momento de apagado (gate)

// ─── Figura por paso ───────────────────────────────────────
// En cada paso CON NOTA se dibuja una figura distinta que EXPLOTA desde el
// centro (zoom + fade) sobre plasma neón, con flash de impacto y onda de choque.
// En los SILENCIOS la matriz queda APAGADA → estrobo natural al ritmo del beat.
#define NUM_FIGS    10
uint8_t curFigure  = 0;
uint8_t figHue     = 0;
bool    stepHasNote = false;                      // ¿el paso actual tiene nota? (si no → negro)

// ─── Botones (anti-rebote por flanco) ──────────────────────
struct BtnState { uint8_t pin; bool last; uint32_t lastPress; };
BtnState btn[5];

bool buttonPressed(BtnState &b) {
  bool now = digitalRead(b.pin);
  uint32_t t = millis();
  bool fired = false;
  if (now == LOW && b.last == HIGH && (t - b.lastPress) > DEBOUNCE_MS) {
    b.lastPress = t; fired = true;
  }
  b.last = now;
  return fired;
}

float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (float)(sum >> 3) / 4095.0f;
}

// ─── Mapeo XY (cableado progresivo) ────────────────────────
uint16_t XY(int x, int y) {
  if (FLIP_X) x = WIDTH  - 1 - x;
  if (FLIP_Y) y = HEIGHT - 1 - y;
  return START_LED + (uint16_t)y * WIDTH + x;
}

// ─── Escala → semitono (con envoltura por octava) ──────────
int scaleSemitone(int degree) {
  int oct = 0;
  while (degree >= 7) { degree -= 7; oct += 12; }
  while (degree < 0)  { degree += 7; oct -= 12; }
  return scales[currentScale][degree] + oct;
}

// ─── MIDI monofónico: corta la nota anterior y dispara la nueva ──
void sendNote(uint8_t note, uint32_t now) {
  if (note > 127) return;
  if (activeNote >= 0) MIDI.noteOff((uint8_t)activeNote, 0, MIDI_CH);  // mono: mata la anterior
  uint32_t gate = (uint32_t)(stepMs * gateFrac);
  if (gate < 15) gate = 15;
  MIDI.noteOn(note, velocity, MIDI_CH);
  activeNote = note;
  noteOffAt  = now + gate;
}

void serviceNoteOffs(uint32_t now) {
  if (activeNote >= 0 && now >= noteOffAt) {
    MIDI.noteOff((uint8_t)activeNote, 0, MIDI_CH);
    activeNote = -1;
  }
}

void allNotesOff() {
  if (activeNote >= 0) { MIDI.noteOff((uint8_t)activeNote, 0, MIDI_CH); activeNote = -1; }
}

// ─── Figura que EXPLOTA desde el centro (escalable, aditiva) ──
// 10 figuras centradas en la matriz 20x20 (centro ≈ 9.5, 9.5). El parámetro
// scale las agranda (zoom): animándolo de pequeño a grande la figura "estalla".
void drawFigure(uint8_t fig, uint8_t hue, uint8_t val, float scale) {
  CRGB col = CHSV(hue, 255, val);
  float inv = 1.0f / (scale < 0.05f ? 0.05f : scale);
  const float cf = 9.5f;
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      float dx = (x - cf) * inv, dy = (y - cf) * inv;
      float ad = fabsf(dx), ay = fabsf(dy);
      float d  = sqrtf(dx * dx + dy * dy);
      float m  = ad > ay ? ad : ay;                                   // distancia "cuadrada"
      bool on = false;
      switch (fig) {
        case 0: on = (d <= 7.5f); break;                              // círculo relleno
        case 1: on = (fabsf(d - 7.5f) <= 1.4f); break;               // anillo grueso
        case 2: on = (m <= 7.0f); break;                              // cuadrado relleno
        case 3: on = (fabsf(m - 7.0f) <= 1.1f); break;               // marco cuadrado
        case 4: on = (ad + ay <= 7.5f); break;                        // rombo relleno
        case 5: on = (fabsf(ad + ay - 7.5f) <= 1.4f); break;         // rombo marco
        case 6: on = ((ad <= 1.7f || ay <= 1.7f) && d <= 9.0f); break;            // cruz
        case 7: on = ((fabsf(dx - dy) <= 1.7f || fabsf(dx + dy) <= 1.7f) && d <= 9.0f); break; // X
        case 8: { float hw = (dy + 7.0f) * 0.5f;                      // triángulo relleno
                  on = (dy >= -7.0f && dy <= 7.0f && ad <= hw); } break;
        case 9: on = (((ad <= 1.5f || ay <= 1.5f) ||                  // estrella de 8 puntas (cruz + X)
                       (fabsf(dx - dy) <= 1.5f || fabsf(dx + dy) <= 1.5f)) && d <= 8.5f); break;
      }
      if (on) leds[XY(x, y)] += col;                                  // aditivo: brilla sobre el plasma
    }
  }
}

// ─── Onda de choque: anillo casi blanco que se expande ─────
void drawShock(float r, uint8_t hue, uint8_t b) {
  CRGB col = CHSV(hue, 70, b);                                        // casi blanco con tinte de la nota
  const float cf = 9.5f;
  for (int y = 0; y < HEIGHT; y++)
    for (int x = 0; x < WIDTH; x++) {
      float dx = x - cf, dy = y - cf;
      float d  = sqrtf(dx * dx + dy * dy);
      if (fabsf(d - r) <= 1.3f) leds[XY(x, y)] += col;
    }
}

// ─── Disparar la nota del paso actual → MIDI mono + figura ──
void triggerStep(uint32_t now) {
  curFigure = (curFigure + 1) % NUM_FIGS;                 // FIGURA DISTINTA EN CADA PASO con nota

  int noteValue = patterns[currentPattern][currentStep];
  if (noteValue == 0) { stepHasNote = false; figHue += 28; return; }  // silencio → matriz apagada

  int base = noteValue - 1;                               // grado de la escala
  int semi = scaleSemitone(base) + OCTAVE_SEMITONES[octaveIndex];
  int note = BASE_NOTE + semi;
  if (note < 0 || note > 127) { stepHasNote = false; return; }
  sendNote((uint8_t)note, now);                           // una sola nota (melódico)
  figHue = (note % 12) * 21;                              // color según la nota
  stepHasNote = true;
}

// ─── IMU ───────────────────────────────────────────────────
void initIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B); Wire.write(0);                        // despertar
  Wire.endTransmission(true);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x1C); Wire.write(0x00);                     // ±2g
  Wire.endTransmission(true);
  delay(100);
}

void readIMU() {
  uint32_t t = millis();
  if (t - lastIMURead < IMU_READ_INTERVAL) return;
  lastIMURead = t;
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, 6, true);
  if (Wire.available() >= 6) {
    int16_t rx = (Wire.read() << 8) | Wire.read();
    int16_t ry = (Wire.read() << 8) | Wire.read();
    (void)((Wire.read() << 8) | Wire.read());
    imu_x = rx / 16384.0f;
    imu_y = ry / 16384.0f;
    filtered_x = filtered_x * (1.0f - IMU_FILTER_ALPHA) + imu_x * IMU_FILTER_ALPHA;
    filtered_y = filtered_y * (1.0f - IMU_FILTER_ALPHA) + imu_y * IMU_FILTER_ALPHA;
  }
}

// IMU → MIDI CC (solo si cambia, para no saturar el bus MIDI)
void sendIMU_CC() {
  float xa = fabsf(filtered_x); if (xa > 1.0f) xa = 1.0f;
  float ya = fabsf(filtered_y); if (ya > 1.0f) ya = 1.0f;
  uint8_t cc74 = (uint8_t)(xa * 127.0f);
  uint8_t cc71 = (uint8_t)(ya * 127.0f);
  if (cc74 != ccCutoffLast) { MIDI.controlChange(CC_CUTOFF, cc74, MIDI_CH); ccCutoffLast = cc74; }
  if (cc71 != ccResonLast)  { MIDI.controlChange(CC_RESON,  cc71, MIDI_CH); ccResonLast  = cc71; }
}

// ─── Render de la matriz (estilo fiesta electrónica) ───────
void renderMatrix(uint32_t now) {
  static uint16_t plasmaT = 0;

  // toda la matriz arranca en NEGRO → los silencios quedan apagados (estrobo)
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  // velocidad/tono del plasma según el movimiento del IMU
  uint8_t energia = (uint8_t)(constrain((fabsf(filtered_x) + fabsf(filtered_y)) * 90.0f, 0.0f, 90.0f));
  plasmaT += 3 + energia / 8;
  uint8_t hueShift = (uint8_t)(filtered_x * 70.0f);

  if (!isPlaying) {
    // IDLE (parado): plasma lento y tenue, el aparato "respira"
    for (int y = 0; y < HEIGHT; y++)
      for (int x = 0; x < WIDTH; x++) {
        uint8_t a = sin8(x * 14 + plasmaT);
        uint8_t b = sin8(y * 12 - (plasmaT >> 1));
        uint8_t c = sin8((x + y) * 10 + (plasmaT >> 2));
        leds[XY(x, y)] = CHSV(a + b + c + hueShift, 230, 38 + (sin8(a + b) >> 3));
      }
  } else if (stepHasNote) {
    // progreso dentro del paso (0 = recién disparado, 1 = a punto de cambiar)
    float prog = (float)(now - lastStepMs) / (float)stepMs;
    if (prog < 0) prog = 0; if (prog > 1) prog = 1;

    // — PLASMA NEÓN: saturado, brilla con el golpe y decae —
    uint8_t pval = (uint8_t)(150.0f * (1.0f - prog) + 22.0f);     // 172 → 22
    for (int y = 0; y < HEIGHT; y++)
      for (int x = 0; x < WIDTH; x++) {
        uint8_t a = sin8(x * 16 + plasmaT);
        uint8_t b = sin8(y * 14 - plasmaT);
        uint8_t c = sin8((x + y) * 11 + (plasmaT >> 1));
        leds[XY(x, y)] = CHSV(a + b + c + hueShift + figHue, 255, pval);
      }

    // — FLASH DE IMPACTO: wash casi blanco al inicio del paso (el "golpe") —
    if (prog < 0.13f) {
      uint8_t fb = (uint8_t)(230.0f * (1.0f - prog / 0.13f));
      CRGB add = CHSV(figHue, 110, fb);
      for (int i = START_LED; i < NUM_LEDS; i++) leds[i] += add;
    }

    // — FIGURA que ESTALLA desde el centro (zoom hacia afuera + fade) —
    float scale  = 0.35f + prog * 1.30f;                           // crece 0.35 → 1.65
    uint8_t fval = (uint8_t)(255.0f * (1.0f - prog * 0.75f));      // brillante al golpe, baja
    drawFigure(curFigure, figHue, fval, scale);

    // — ONDA DE CHOQUE: anillo blanco que se expande por toda la matriz —
    drawShock(prog * 13.5f, figHue, (uint8_t)(255.0f * (1.0f - prog)));
  }
  // si está tocando pero el paso es silencio → la matriz queda en negro

  // LEDs internos del PCB siempre apagados
  for (int i = 0; i < START_LED; i++) leds[i] = CRGB::Black;

  FastLED.setBrightness(matBright);
  FastLED.show();
}

// ─── Setup ─────────────────────────────────────────────────
void setup() {
  for (int i = 0; i < 5; i++) {
    pinMode(BTN_PIN[i], INPUT_PULLUP);
    btn[i] = {BTN_PIN[i], HIGH, 0};
  }
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(matBright);
  FastLED.clear(); FastLED.show();


  initIMU();
  delay(50);
  readIMU();

  MIDI.begin();
  USB.begin();
}

// ─── Loop ──────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // — Botones —
  if (buttonPressed(btn[0])) {                            // BTN1 → Play/Stop
    isPlaying = !isPlaying;
    if (isPlaying) { currentStep = 0; lastStepMs = now; triggerStep(now); }
    else           { allNotesOff(); }
  }
  if (buttonPressed(btn[1])) currentScale = (currentScale + 1) % numScales;   // BTN2 → escala
  if (buttonPressed(btn[2])) currentPattern = (currentPattern + 1) % numPatterns; // BTN3 → patrón
  if (buttonPressed(btn[3])) {                            // BTN4 → largo
    sequenceLength = (sequenceLength == 4) ? 8 : ((sequenceLength == 8) ? 16 : 4);
    if (currentStep >= sequenceLength) currentStep = 0;
  }
  if (buttonPressed(btn[4])) octaveIndex = (octaveIndex + 1) % 4;             // BTN5 → octava

  // — Pots —
  velocity  = 40 + (uint8_t)(readPot(POT_VEL) * 87.0f);                 // 40-127
  matBright = 8  + (uint8_t)(readPot(POT_BRIGHT) * (MAX_BRIGHT - 8));   // 8-MAX
  int bpm   = 40 + (int)(readPot(POT_TEMPO) * 260.0f);                  // 40-300 BPM
  stepMs    = (uint32_t)(60000.0f / (bpm * 4));                         // notas de 1/16
  gateFrac  = 0.05f + readPot(POT_GATE) * 0.90f;                        // 5%-95% del paso

  // — IMU → MIDI CC + visual —
  readIMU();
  sendIMU_CC();

  // — Secuenciador (basado en millis) —
  if (isPlaying && now - lastStepMs >= stepMs) {
    lastStepMs += stepMs;
    currentStep = (currentStep + 1) % sequenceLength;
    triggerStep(now);
  }
  serviceNoteOffs(now);

  // — Render de la matriz a ~40 fps —
  static uint32_t lastFrame = 0;
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    renderMatrix(now);
  }
}
