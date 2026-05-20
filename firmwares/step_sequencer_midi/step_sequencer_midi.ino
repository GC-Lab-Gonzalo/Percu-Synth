// ==============================================================================================================================================
// PERCU-SYNTH — Sample Sequencer + Realtime Remote — GC Lab Chile
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
// - Microcontrolador ESP32-S3 (USB MIDI nativo)
// - DAC PCM5102 vía I2S — estéreo 44.1 kHz · 16-bit |LCK -> 39, DIN -> 40, BCK -> 41|
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// - Salida MIDI USB nativa (canal 16 = control, canal 10 = trigger)
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - USB Mode           : USB-OTG (TinyUSB)
// - Flash Mode         : DIO          (¡OPI rompe I2S!)
// - PSRAM              : OPI PSRAM
// - Partition Scheme   : Huge APP (3MB No OTA/1MB SPIFFS)
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (USB.h, USBMIDI.h, driver/i2s_std.h, TinyUSB)
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Secuenciador de samples (16 pasos × 6 pistas) que funciona como
// MIDI CLOCK MASTER por USB. Los samples se embeben en flash en compile-time
// — el firmware base trae placeholders vacíos y se regenera con la webapp
// tools/step_sequencer_loader/ cada vez que quieras cargar drum kits nuevos.
//
// ARQUITECTURA:
//   PercuSynth (ESP32-S3) → motor de audio + MIDI CLOCK MASTER (sale por USB).
//   Webapp     (Chrome)   → edita patrón y FX EN VIVO vía Web MIDI sobre el
//                           mismo USB, sin necesidad de re-flashear.
//   DAW / sintes externos → se sincronizan al clock del PercuSynth.
//
// FX globales: HPF/LPF biquad · master pitch (cinta) · bitcrush · stutter ·
// reverse · soft limiter.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// CONTROLES LOCALES (en el dispositivo):
// - POT1 (ADC1)  → BPM (40 – 200)
// - POT2 (ADC2)  → LPF cutoff
// - POT3 (ADC8)  → BITCRUSH amount
// - POT4 (ADC10) → Master volume
// - BTN1 (44)    → PLAY / STOP
// - BTN2 (42)    → STUTTER momentáneo (mantener apretado para subdividir el step)
// - BTN3 (0)     → REVERSE toggle (lee los samples al revés)
// - BTN4 (45)    → trigger manual T0
// - BTN5 (47)    → trigger manual T1
//
// Los pots tienen histéresis: solo se aplican cuando se mueven, así no
// pisan los valores que cambia la webapp.
//
// PROTOCOLO MIDI OUT (sale del PercuSynth):
//   0xF8 → Clock 24 PPQ · 0xFA → Start · 0xFC → Stop
//   ch16 NoteOn note (100+step) → notifica el paso actual (la webapp resalta)
//   ch16 CC echo → cuando se mueve un pot local, la webapp actualiza sliders
//
// PROTOCOLO MIDI IN (le entra al PercuSynth) — canal 16:
//   NoteOn note 0..95   (= track*16 + step), vel 0=off / >0=on   → edita celda
//   NoteOn note 96..101 vel>0                                    → toggle MUTE T0..T5
//   NoteOn note 120 vel>0                                        → PLAY
//   NoteOn note 121 vel>0                                        → STOP
//   NoteOn note 122 vel>0                                        → RESET (step 0)
//   CC 20 → BPM         (0..127 → 40..200)
//   CC 21 → SWING       (0..0.4 de la semicorchea impar)
//   CC 22 → master PITCH (CC64 = 1.0×)
//   CC 23 → HPF cutoff
//   CC 24 → master VOL
//   CC 25 → LPF cutoff
//   CC 26 → BITCRUSH amount
//   CC 27 → STUTTER on/off  (>=64)
//   CC 28 → REVERSE on/off  (>=64)
//
// PROTOCOLO MIDI IN — trigger manual (canal 10 GM drum):
//   NoteOn note 36/38/42/46/49/51 → dispara T0..T5
//
// MODO DE USO:
// 1. Generar firmware con samples: abrir tools/step_sequencer_loader/ en Chrome,
//    cargar 6 samples cortos, descargar el .ino y flashearlo.
// 2. Conectar al PC → aparece como "ESP32-S3 MIDI".
// 3. Volver a abrir la webapp y "Conectar MIDI" → ahora editas patrón en vivo.
// 4. Mandar Clock a un DAW: configurarlo como esclavo del PercuSynth.
// ==============================================================================================================================================

#include <Arduino.h>
#include "driver/i2s_std.h"
#include "USB.h"
#include "USBMIDI.h"
#include <math.h>

USBMIDI MIDI;
// tinyusb/midi_device.h ya está incluido vía USBMIDI.h. Sólo usamos
// tud_midi_n_packet_read() y tud_midi_n_packet_write() directamente.

// ================================================================
// SAMPLE DATA (placeholder — re-generar con la webapp)
// ================================================================
const int16_t TRK_0[] PROGMEM = {0, 0};
const uint32_t TRK_0_LEN = 0UL;
const int16_t TRK_1[] PROGMEM = {0, 0};
const uint32_t TRK_1_LEN = 0UL;
const int16_t TRK_2[] PROGMEM = {0, 0};
const uint32_t TRK_2_LEN = 0UL;
const int16_t TRK_3[] PROGMEM = {0, 0};
const uint32_t TRK_3_LEN = 0UL;
const int16_t TRK_4[] PROGMEM = {0, 0};
const uint32_t TRK_4_LEN = 0UL;
const int16_t TRK_5[] PROGMEM = {0, 0};
const uint32_t TRK_5_LEN = 0UL;

// ================================================================
// CONSTANTES Y PINOUT
// ================================================================
#define SAMPLE_RATE   44100
#define BUF_SAMPLES   128
#define NUM_TRACKS    6
#define NUM_STEPS     16
#define NUM_VOICES    8
#define PPQ           24
#define TICKS_PER_STEP 6

#define PIN_I2S_LRCK  39
#define PIN_I2S_DIN   40
#define PIN_I2S_BCK   41

const uint8_t BTN_PINS[5] = {44, 42, 0, 45, 47};
const uint8_t POT_PINS[4] = {1, 2, 8, 10};

const uint8_t TRK_NOTES[NUM_TRACKS] = {36, 38, 42, 46, 49, 51};
const int16_t* const TRK_DATA[NUM_TRACKS] = {TRK_0, TRK_1, TRK_2, TRK_3, TRK_4, TRK_5};
const uint32_t       TRK_LENS[NUM_TRACKS] = {TRK_0_LEN, TRK_1_LEN, TRK_2_LEN, TRK_3_LEN, TRK_4_LEN, TRK_5_LEN};

// ================================================================
// PATTERN (en RAM, modificable por MIDI)
// ================================================================
uint8_t pattern[NUM_TRACKS][NUM_STEPS] = {
  { 1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,1,0 }, // T0 kick
  { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 }, // T1 snare
  { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0 }, // T2 hat
  { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }, // T3 perc
  { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 }, // T4
  { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 }, // T5
};
bool trackMute[NUM_TRACKS] = { false, false, false, false, false, false };

// ================================================================
// VOICE POOL
// ================================================================
struct Voice {
  const int16_t* data;
  uint32_t       length;
  float          pos;
  float          pitchRatio;
  float          velocity;
  bool           active;
  bool           reverse;
};
Voice voices[NUM_VOICES];

float g_pitchRatio  = 1.0f;
bool  g_reverseMode = false;

void triggerTrack(uint8_t trk, float velocity) {
  if (trk >= NUM_TRACKS) return;
  if (trackMute[trk]) return;
  if (TRK_LENS[trk] == 0) return;
  int v = -1;
  for (int i = 0; i < NUM_VOICES; i++) if (!voices[i].active) { v = i; break; }
  if (v < 0) {
    float mx = -1.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
      float prog = voices[i].pos / (float)voices[i].length;
      if (prog > mx) { mx = prog; v = i; }
    }
  }
  voices[v].data       = TRK_DATA[trk];
  voices[v].length     = TRK_LENS[trk];
  voices[v].reverse    = g_reverseMode;
  voices[v].pos        = g_reverseMode ? (float)(TRK_LENS[trk] - 1) : 0.0f;
  voices[v].pitchRatio = g_pitchRatio;
  voices[v].velocity   = velocity;
  voices[v].active     = true;
}

// ================================================================
// FX globales — HPF/LPF biquad + bitcrush + softlimit
// ================================================================
static float hpf_x1=0,hpf_x2=0,hpf_y1=0,hpf_y2=0;
static float hpf_b0=1,hpf_b1=0,hpf_b2=0,hpf_a1=0,hpf_a2=0;
static float lpf_x1=0,lpf_x2=0,lpf_y1=0,lpf_y2=0;
static float lpf_b0=1,lpf_b1=0,lpf_b2=0,lpf_a1=0,lpf_a2=0;

float fx_hpfFreq   = 20.0f;
float fx_lpfFreq   = 12000.0f;
float fx_lpfQ      = 0.707f;
float fx_bitcrush  = 0.0f;        // 0..1
float fx_masterVol = 0.6f;
float fx_swing     = 0.0f;        // 0..0.4
bool  fx_stutter   = false;

void computeHPF(float freq, float Q) {
  if (freq < 20.0f) freq = 20.0f;
  if (freq > SAMPLE_RATE * 0.45f) freq = SAMPLE_RATE * 0.45f;
  float w = 2.0f * (float)M_PI * freq / SAMPLE_RATE;
  float c = cosf(w), s = sinf(w);
  float a = s / (2.0f * Q);
  float n = 1.0f / (1.0f + a);
  hpf_b0 =  (1.0f + c) * 0.5f * n;
  hpf_b1 = -(1.0f + c) * n;
  hpf_b2 =  hpf_b0;
  hpf_a1 = -2.0f * c * n;
  hpf_a2 =  (1.0f - a) * n;
}
inline float applyHPF(float x) {
  float y = hpf_b0*x + hpf_b1*hpf_x1 + hpf_b2*hpf_x2 - hpf_a1*hpf_y1 - hpf_a2*hpf_y2;
  hpf_x2=hpf_x1; hpf_x1=x; hpf_y2=hpf_y1; hpf_y1=y;
  return y;
}
void computeLPF(float freq, float Q) {
  if (freq < 20.0f) freq = 20.0f;
  if (freq > SAMPLE_RATE * 0.45f) freq = SAMPLE_RATE * 0.45f;
  float w = 2.0f * (float)M_PI * freq / SAMPLE_RATE;
  float c = cosf(w), s = sinf(w);
  float a = s / (2.0f * Q);
  float n = 1.0f / (1.0f + a);
  lpf_b0 = (1.0f - c) * 0.5f * n;
  lpf_b1 = (1.0f - c) * n;
  lpf_b2 = lpf_b0;
  lpf_a1 = -2.0f * c * n;
  lpf_a2 = (1.0f - a) * n;
}
inline float applyLPF(float x) {
  float y = lpf_b0*x + lpf_b1*lpf_x1 + lpf_b2*lpf_x2 - lpf_a1*lpf_y1 - lpf_a2*lpf_y2;
  lpf_x2=lpf_x1; lpf_x1=x; lpf_y2=lpf_y1; lpf_y1=y;
  return y;
}
inline float applyBitcrush(float x, float amt) {
  if (amt < 0.02f) return x;
  int bits = 16 - (int)(amt * 12.0f);   // de 16 a 4 bits
  if (bits < 2) bits = 2;
  float levels = (float)(1 << (bits - 1));
  return roundf(x * levels) / levels;
}
inline float softLimit(float x) {
  const float knee = 0.85f;
  float a = fabsf(x);
  if (a <= knee) return x;
  float s = (x < 0.0f) ? -1.0f : 1.0f;
  return s * (knee + (1.0f - knee) * tanhf((a - knee) / (1.0f - knee)));
}

// ================================================================
// I2S (PCM5102)
// ================================================================
i2s_chan_handle_t tx_handle;
void initI2S() {
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  i2s_new_channel(&cc, &tx_handle, NULL);
  i2s_std_config_t sc = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = { .mclk = I2S_GPIO_UNUSED,
                  .bclk = (gpio_num_t)PIN_I2S_BCK,
                  .ws   = (gpio_num_t)PIN_I2S_LRCK,
                  .dout = (gpio_num_t)PIN_I2S_DIN,
                  .din  = I2S_GPIO_UNUSED }
  };
  i2s_channel_init_std_mode(tx_handle, &sc);
  i2s_channel_enable(tx_handle);
}

int readPot(uint8_t pin) {
  int s = 0;
  for (int i = 0; i < 8; i++) s += analogRead(pin);
  return s >> 3;
}

// ================================================================
// MIDI HELPERS (OUT)
// ================================================================
inline void midiSendRT(uint8_t status) {
  uint8_t pkt[4] = { 0x0F, status, 0, 0 };
  tud_midi_n_packet_write(0, pkt);
}
inline void midiSendNoteOn(uint8_t chan, uint8_t note, uint8_t vel) {
  uint8_t pkt[4] = { 0x09, (uint8_t)(0x90 | (chan & 0x0F)), note, vel };
  tud_midi_n_packet_write(0, pkt);
}
inline void midiSendCC(uint8_t chan, uint8_t cc, uint8_t v) {
  uint8_t pkt[4] = { 0x0B, (uint8_t)(0xB0 | (chan & 0x0F)), cc, v };
  tud_midi_n_packet_write(0, pkt);
}
// Poly Aftertouch — usamos esto para devolver echos a la webapp (no
// colisiona con NoteOn pattern edits ni con CC params).
inline void midiSendPolyAT(uint8_t chan, uint8_t note, uint8_t val) {
  uint8_t pkt[4] = { 0x0A, (uint8_t)(0xA0 | (chan & 0x0F)), note, val };
  tud_midi_n_packet_write(0, pkt);
}

// ================================================================
// CLOCK / TRANSPORT
// ================================================================
float    bpm             = 120.0f;
uint32_t clockIntervalUs = (uint32_t)(60000000.0f / (120.0f * (float)PPQ));
uint32_t nextClockUs     = 0;
uint16_t tickCount       = 0;
uint8_t  currentStep     = 0;
bool     playing         = false;

void recomputeClock() {
  if (bpm < 30.0f)  bpm = 30.0f;
  if (bpm > 240.0f) bpm = 240.0f;
  clockIntervalUs = (uint32_t)(60000000.0f / (bpm * (float)PPQ));
}

void fireStep(uint8_t step) {
  for (int t = 0; t < NUM_TRACKS; t++)
    if (pattern[t][step]) triggerTrack(t, 1.0f);
}
void advanceStep() {
  currentStep = (currentStep + 1) % NUM_STEPS;
  fireStep(currentStep);
  // Feedback a la webapp: NoteOn ch16 note 100+step
  midiSendNoteOn(15, (uint8_t)(100 + currentStep), 127);
}
void doStart() {
  playing = true;
  tickCount = 0;
  currentStep = 0;
  nextClockUs = micros();
  midiSendRT(0xFA);
  fireStep(0);
  midiSendNoteOn(15, 100, 127);   // notifica step 0
}
void doStop() {
  playing = false;
  midiSendRT(0xFC);
}

// ================================================================
// MIDI IN
// ================================================================
void handleCC(uint8_t cc, uint8_t v) {
  float n = v / 127.0f;
  switch (cc) {
    case 20: bpm          = 40.0f + n * 160.0f; recomputeClock(); break;
    case 21: fx_swing     = n * 0.4f;                            break;
    case 22: g_pitchRatio = powf(2.0f, (n - 0.5f) * 2.0f);       break;
    case 23: fx_hpfFreq   = 20.0f + n*n * 3980.0f; computeHPF(fx_hpfFreq, 0.707f); break;
    case 24: fx_masterVol = n;                                   break;
    case 25: fx_lpfFreq   = 200.0f + n * 19800.0f; computeLPF(fx_lpfFreq, fx_lpfQ); break;
    case 26: fx_bitcrush  = n;                                   break;
    case 27: fx_stutter   = (v >= 64);                           break;
    case 28: g_reverseMode= (v >= 64);                           break;
  }
}

void handleMidiByte(uint8_t b1, uint8_t b2, uint8_t b3) {
  uint8_t status = b1 & 0xF0;
  uint8_t chan   = b1 & 0x0F;

  // DEBUG: log todo lo que no sea clock
  if (b1 != 0xF8) Serial.printf("MIDI in: %02X %02X %02X\n", b1, b2, b3);

  if (status == 0x90) {                                  // NoteOn
    if (chan == 15) {                                    // ch16 = remote control
      if (b2 < 96) {
        uint8_t t = b2 / 16;
        uint8_t s = b2 % 16;
        pattern[t][s] = (b3 > 0) ? 1 : 0;
        Serial.printf("  -> celda t=%d s=%d = %d\n", t, s, pattern[t][s]);
        // ECHO a la webapp confirmando la celda
        midiSendPolyAT(15, b2, pattern[t][s] ? 127 : 0);
        return;
      }
      if (b2 >= 96 && b2 < 96 + NUM_TRACKS && b3 > 0) {
        trackMute[b2 - 96] = !trackMute[b2 - 96];
        Serial.printf("  -> mute T%d = %d\n", b2-96, trackMute[b2-96]);
        return;
      }
      if (b2 == 120 && b3 > 0) { doStart(); return; }
      if (b2 == 121 && b3 > 0) { doStop();  return; }
      if (b2 == 122 && b3 > 0) { currentStep = 0; tickCount = 0; return; }
      return;
    }
    if (chan == 9 && b3 > 0) {                           // ch10 GM drum
      for (int t = 0; t < NUM_TRACKS; t++)
        if (TRK_NOTES[t] == b2) { triggerTrack(t, b3 / 127.0f); return; }
    }
  }
  if (status == 0xB0 && chan == 15) handleCC(b2, b3);
}

void pollMidi() {
  uint8_t pkt[4];
  while (tud_midi_n_packet_read(0, pkt)) {
    handleMidiByte(pkt[1], pkt[2], pkt[3]);
  }
}

// ================================================================
// SETUP
// ================================================================
bool     btnPrev[5]   = {};
uint32_t btnLastMs[5] = {};
#define  DEBOUNCE_MS  40

void setup() {
  Serial.begin(115200); delay(100);
  Serial.println("PercuSynth — Sample Sequencer (MIDI master) boot...");
  analogReadResolution(12);
  for (int i = 0; i < 5; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);
  for (int i = 0; i < NUM_VOICES; i++) voices[i].active = false;
  initI2S();
  computeHPF(fx_hpfFreq, 0.707f);
  computeLPF(fx_lpfFreq, fx_lpfQ);
  recomputeClock();
  MIDI.begin();
  USB.begin();
  Serial.println("Listo. BTN0=play  POT0=BPM POT1=LPF POT2=crush POT3=vol");
  for (int t = 0; t < NUM_TRACKS; t++)
    Serial.printf("  T%d  %u samples (%.2fs)  note=%u\n",
                  t, TRK_LENS[t], TRK_LENS[t]/(float)SAMPLE_RATE, TRK_NOTES[t]);
}

// ================================================================
// MAIN LOOP
// ================================================================
void loop() {
  uint32_t now   = millis();
  uint32_t nowUs = micros();

  pollMidi();

  // ---- Pots con histeresis (no pisan al webapp si no se mueven) ----
  static int lp0=-9999, lp1=-9999, lp2=-9999, lp3=-9999;
  int p0 = readPot(POT_PINS[0]);
  int p1 = readPot(POT_PINS[1]);
  int p2 = readPot(POT_PINS[2]);
  int p3 = readPot(POT_PINS[3]);
  if (abs(p0 - lp0) > 20) { lp0 = p0; bpm = 40.0f + (p0/4095.0f) * 160.0f; recomputeClock(); midiSendCC(15, 20, (uint8_t)((bpm-40.0f)/160.0f*127.0f)); }
  if (abs(p1 - lp1) > 20) { lp1 = p1; fx_lpfFreq = 200.0f + (p1/4095.0f) * 19800.0f; computeLPF(fx_lpfFreq, fx_lpfQ); midiSendCC(15, 25, (uint8_t)((fx_lpfFreq-200.0f)/19800.0f*127.0f)); }
  if (abs(p2 - lp2) > 20) { lp2 = p2; fx_bitcrush = p2/4095.0f; midiSendCC(15, 26, (uint8_t)(fx_bitcrush*127.0f)); }
  if (abs(p3 - lp3) > 20) { lp3 = p3; fx_masterVol = 0.1f + (p3/4095.0f) * 0.9f; midiSendCC(15, 24, (uint8_t)(fx_masterVol*127.0f)); }

  // ---- Botones ----
  for (int i = 0; i < 5; i++) {
    bool pressed = (digitalRead(BTN_PINS[i]) == LOW);
    if (pressed != btnPrev[i] && now - btnLastMs[i] > DEBOUNCE_MS) {
      btnLastMs[i] = now;
      btnPrev[i] = pressed;
      if (pressed) {
        switch (i) {
          case 0: playing ? doStop() : doStart(); Serial.println(playing ? "PLAY" : "STOP"); break;
          case 1: fx_stutter = true;  midiSendCC(15, 27, 127); break;
          case 2: g_reverseMode = !g_reverseMode; midiSendCC(15, 28, g_reverseMode ? 127 : 0); break;
          case 3: triggerTrack(0, 1.0f); break;
          case 4: triggerTrack(1, 1.0f); break;
        }
      } else {
        if (i == 1) { fx_stutter = false; midiSendCC(15, 27, 0); }
      }
    }
  }

  // ---- MIDI clock OUT + advance sequencer ----
  if (playing) {
    while ((int32_t)(nowUs - nextClockUs) >= 0) {
      midiSendRT(0xF8);
      tickCount++;
      uint16_t stepEvery = fx_stutter ? (TICKS_PER_STEP / 2) : TICKS_PER_STEP;
      if (tickCount >= stepEvery) {
        tickCount = 0;
        advanceStep();
      }
      nextClockUs += clockIntervalUs;
      nowUs = micros();
      // si nos atrasamos >100 ms, resincronizamos
      if ((int32_t)(nowUs - nextClockUs) > 100000) nextClockUs = nowUs;
    }
  }

  // ---- Render audio ----
  static int16_t buf[BUF_SAMPLES * 2];
  for (int s = 0; s < BUF_SAMPLES; s++) {
    float mix = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
      if (!voices[i].active) continue;
      int32_t pos = (int32_t)voices[i].pos;
      if (voices[i].reverse) {
        if (pos < 0) { voices[i].active = false; continue; }
        float frac = voices[i].pos - (float)pos;
        float s0 = (float)voices[i].data[pos];
        float s1 = (pos - 1 >= 0) ? (float)voices[i].data[pos-1] : 0.0f;
        mix += (s0 + frac*(s1-s0)) * voices[i].velocity;
        voices[i].pos -= voices[i].pitchRatio;
        if (voices[i].pos < 0) voices[i].active = false;
      } else {
        float frac = voices[i].pos - (float)pos;
        float s0 = (float)voices[i].data[pos];
        float s1 = ((uint32_t)(pos + 1) < voices[i].length) ? (float)voices[i].data[pos+1] : 0.0f;
        mix += (s0 + frac*(s1-s0)) * voices[i].velocity;
        voices[i].pos += voices[i].pitchRatio;
        if ((uint32_t)voices[i].pos >= voices[i].length) voices[i].active = false;
      }
    }
    mix = (mix * 0.18f) / 32768.0f;
    mix *= fx_masterVol;
    mix = applyHPF(mix);
    mix = applyLPF(mix);
    mix = applyBitcrush(mix, fx_bitcrush);
    mix = softLimit(mix);
    int16_t out = (int16_t)(mix * 32767.0f);
    buf[s*2] = out; buf[s*2+1] = out;
  }
  size_t written;
  i2s_channel_write(tx_handle, buf, sizeof(buf), &written, portMAX_DELAY);
}
