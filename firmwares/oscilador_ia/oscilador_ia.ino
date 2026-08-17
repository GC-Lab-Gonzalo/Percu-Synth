// ==============================================================================================================================================
// PERCUSYNTH - OSCILADOR IA (el sample de ElevenLabs ES el oscilador: escalas, arpegios, drones y secuencia generativa) - GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo Sandoval - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
//
// Pides un SONIDO hablando ("una cuerda metalica oxidada", "un coro de vidrio") y el PercuSynth
// le pide a ElevenLabs ESE sonido, tal cual, traducido claro al ingles. El sample se convierte
// en la onda principal de un sintetizador afinado: teclado por grados de la escala, arpegiador,
// drones infinitos (el sample como oscilador continuo) y una secuencia melodica generativa.
//
// La clave que lo hace AFINADO de verdad: ElevenLabs NO sabe de notas ni de Hz (es un generador
// de sonidos, no un musico), asi que no se le pide afinacion ninguna. Al cargar el sample se
// MIDE su frecuencia fundamental real por autocorrelacion, y cada nota se calcula contra esa
// medicion. Caiga donde caiga el sonido, el La sale La: la afinacion vive en el firmware.
//
// Hereda de sampler_ia toda la cadena de red (mic INMP441 -> Whisper -> GPT -> ElevenLabs ->
// PSRAM) y de trance_imu el filtro biquad resonante controlado por el IMU.
// ==============================================================================================================================================
// HARDWARE
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3 (PercuSynth). REQUIERE modulo CON PSRAM.
//
// - DAC PCM5102 por I2S  ->  I2S_NUM_0 (SALIDA, 44.1 kHz / 16 bit / estereo):
//       I2S LCK / LRCK ... GPIO 39
//       I2S DIN / DATA ... GPIO 40
//       I2S BCK / BCLK ... GPIO 41
//
// - Microfono INMP441 por I2S  ->  I2S_NUM_1 (ENTRADA, 16 kHz mono):
//       WS  (LRCL) ....... GPIO 11
//       SCK (BCLK) ....... GPIO 12
//       SD  (DOUT) ....... GPIO 13    (L/R a GND, VDD a 3.3V)
//
// - IMU MPU6050 por I2C:  SDA -> GPIO 21, SCL -> GPIO 38  (dir. 0x68)
//
// - BTN1 (GPIO 44) ..... MANTENER = grabar el pedido de timbre (max 5 s)
//                        TOQUE CORTO = siguiente ESCALA (10 escalas)
// - BTN2..BTN5 (GPIO 42, 0, 45, 47) cambian segun el MODO (POT4):
//     MODO TECLADO:  4 notas del acorde de la escala (fundamental / 3a / 5a / octava).
//                    Toque = nota mientras presionas. MANTENER > 0.6 s = DRONE infinito
//                    (el sample queda sonando como un oscilador); otro toque lo apaga.
//     MODOS ARP:     cada boton LATCHEA el arpegio sobre un acorde de la escala
//                    (grados I / IV / V / VI). Tocar el mismo boton = parar.
//     MODO SEQ:      BTN2 = play/stop de la secuencia generativa (4 compases)
//                    BTN3 = nueva melodia (misma progresion)
//                    BTN4 = nueva progresion de acordes (+ melodia nueva)
//                    BTN5 = pedal de DRONE en la tonica (toggle)
// - POT1 (ADC 1) ....... BPM del arpegiador / secuencia (60 - 200)
// - POT2 (ADC 2) ....... volumen master
// - POT3 (ADC 8) ....... TONICA (12 zonas: C2 .. B2)
// - POT4 (ADC 10) ...... MODO (6 zonas): TECLADO | ARP subida | ARP bajada | ARP sube-baja |
//                        ARP aleatorio | SECUENCIA generativa
// - IMU  ............... eje X -> cutoff del filtro · eje Y -> resonancia (Q)
// - LEDs: solo los 6 SMD on-board (GPIO 46) como indicadores + LED RGB del modulo (GPIO 48)
// ==============================================================================================================================================
// ARDUINO IDE SETTINGS
// ==============================================================================================================================================
// - Placa:           ESP32S3 Dev Module
// - Flash Mode:      DIO            (IMPORTANTE en este hardware para que el I2S funcione bien)
// - PSRAM:           OPI PSRAM      (OBLIGATORIO: instrumento + buffers de red ~600 KB)
// - USB CDC On Boot: Enabled        (para ver el pitch detectado y el estado musical)
// - Upload/Monitor:  115200 baud
// ==============================================================================================================================================
// LIBRERIAS REQUERIDAS
// ==============================================================================================================================================
// - WiFi.h / WiFiClientSecure.h / HTTPClient.h   (core ESP32 Arduino)
// - driver/i2s_std.h                             (core ESP32 Arduino, nuevo driver I2S)
// - Wire.h                                       (core ESP32 Arduino, IMU)
// - FastLED
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
//   1) MANTEN BTN1 y describe un SONIDO: "un cello de metal liquido", "una flauta de cristal".
//   2) GPT lo traduce a una descripcion clara del sonido en ingles y ESO es todo el prompt
//      (sin coletillas de afinacion ni loop: confunden al generador). La duracion (4 s) y el
//      loop van como parametros de la API.
//   3) Al cargarlo: recorte de silencio + normalizado + DETECCION DE PITCH por autocorrelacion
//      (con guardia de error de octava y refinado parabolico = afinacion en centesimas) +
//      SUSTAIN LOOP con crossfade horneado en la zona sostenida (45%..92% del sample).
//      El Serial muestra la frecuencia medida. Suena una nota de audicion.
//   4) Desde ahi el sample ES el instrumento: toca el teclado, latchea drones, corre arpegios
//      o deja la secuencia generativa (progresion modal + melodia con snap a notas del acorde).
//   5) Cada nueva generacion REEMPLAZA el instrumento (un timbre a la vez, todo el caracter).
//
//   FORMATO DE AUDIO Y PLANES DE ELEVENLABS
//   Pide primero 'pcm_22050' (limpio, plan Pro). Si la key lo rechaza reintenta en 'ulaw_8000'
//   (todos los planes, lo-fi). Se decodifica aqui mismo, sin librerias.
//
//   LED 0 = instrumento (color del sample, flash con cada nota)
//   LED 1 = modo (cian teclado · violetas arp · naranja secuencia; respira si algo corre)
//   LED 2 = escala (color por escala, flash al cambiarla)
//   LED 3 = pulso (negra del reloj)
//   LED 4 = filtro IMU (color por cutoff, brillo por resonancia)
//   LED 5 = estado: VERDE listo · ROJO grabando · AMBAR procesando · MAGENTA error
// ==============================================================================================================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "driver/i2s_std.h"
#include <FastLED.h>

// ==================== CONFIGURACION USUARIO ====================

// --- Credenciales (WiFi + OpenAI + ElevenLabs) ---
// No viven en este archivo. Copia secretos.example.h a secretos.h (misma carpeta del
// sketch) y escribe ahi tus claves. secretos.h esta en .gitignore: nunca se sube al repo.
#if !__has_include("secretos.h")
  #error "Falta secretos.h: copia secretos.example.h a secretos.h y pon tus claves."
#endif
#include "secretos.h"

// GPT traduce tu pedido a un prompt de timbre. En false se manda la transcripcion literal.
#define USE_GPT_PROMPT   true

// ==================== PINES ====================

#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41

#define MIC_WS    11
#define MIC_SCK   12
#define MIC_SD    13

#define BTN_REC   44          // BTN1 - mantener = grabar · toque = escala
#define BTN_N1    42          // BTN2
#define BTN_N2     0          // BTN3
#define BTN_N3    45          // BTN4
#define BTN_N4    47          // BTN5

#define POT_BPM    1          // ADC1  - BPM arp/seq
#define POT_VOL    2          // ADC2  - volumen master
#define POT_ROOT   8          // ADC8  - tonica (C2..B2)
#define POT_MODE  10          // ADC10 - modo

#define SDA_PIN   21
#define SCL_PIN   38
#define IMU_ADDR  0x68

#define LED_PIN    46
#define NUM_LEDS    6         // solo los 6 SMD on-board
#define RGB_PIN    48

// ==================== AUDIO ====================

#define OUT_RATE     44100
#define BUF_SAMPLES    128

#define MIC_RATE     16000
#define RECORD_SECONDS   5
#define MAX_MIC_SAMPLES (MIC_RATE * RECORD_SECONDS)
#define MIC_GAIN         6

#define MAX_INST_SECS    5
#define INST_RATE_PCM 22050   // pcm_22050
#define INST_RATE_ULAW 8000   // ulaw_8000 (fallback para planes sin PCM)
#define MAX_INST_SAMPLES (INST_RATE_PCM * MAX_INST_SECS)

#define NUM_VOICES       8
#define GATE_INF 0xFFFFFFFFu

// Si la autocorrelacion no encuentra periodicidad (ElevenLabs devolvio algo sin tono claro,
// puro ruido/textura), se asume 110 Hz: los INTERVALOS siguen siendo correctos entre si
// aunque la afinacion absoluta quede a ciegas. Suena, y suena en escala.
#define FALLBACK_FREQ  110.0f

// ==================== TIPOS (ANTES de cualquier funcion) ====================
// El pre-procesador de Arduino genera los prototipos automaticos justo antes de la PRIMERA
// funcion del archivo. Todo tipo que aparezca en una firma tiene que estar declarado antes
// de ese punto o los prototipos no compilan ("does not name a type").

// Lo que GPT decide sobre el timbre a pedirle a ElevenLabs
struct SfxSpec { String prompt; float dur; bool loop; };

// Boton con deteccion de flanco y de pulsacion larga
struct Btn {
  uint8_t  pin;
  bool     last     = HIGH;
  unsigned long down = 0;
  bool     longDone = false;
};

// ==================== TEORIA MUSICAL ====================
// Regla del proyecto: la teoria se maneja EN LA LOGICA, no se recorta. 10 escalas (las mismas
// de cyber_kit) y por cada una un vocabulario de grados para la progresion + cadencia modal.

#define NUM_SCALES 10
const int8_t SCALE_IV[NUM_SCALES][7] = {
  { 0, 2, 4, 5, 7, 9, 11 },   // Jonico (mayor)
  { 0, 2, 3, 5, 7, 9, 10 },   // Dorico
  { 0, 1, 3, 5, 7, 8, 10 },   // Frigio
  { 0, 2, 4, 6, 7, 9, 11 },   // Lidio
  { 0, 2, 4, 5, 7, 9, 10 },   // Mixolidio
  { 0, 2, 3, 5, 7, 8, 10 },   // Eolico (menor natural)
  { 0, 1, 3, 5, 6, 8, 10 },   // Locrio
  { 0, 2, 3, 5, 7, 8, 11 },   // Menor armonica
  { 0, 1, 4, 5, 7, 8, 10 },   // Frigio dominante (flamenco)
  { 0, 1, 4, 5, 7, 8, 11 },   // Doble armonica (gitana)
};
const char* SCALE_NAME[NUM_SCALES] = {
  "Jonico", "Dorico", "Frigio", "Lidio", "Mixolidio",
  "Eolico", "Locrio", "Menor armonica", "Frigio dominante", "Doble armonica"
};

// Grados (0..6) que suenan idiomaticos en cada modo para los compases 2 y 3 de la progresion,
// y el grado de CADENCIA que cierra el compas 4 antes de volver al I.
const int8_t MODE_POOL[NUM_SCALES][4] = {
  { 3, 4, 5, 1 },   // Jonico:   IV V vi ii
  { 3, 6, 2, 0 },   // Dorico:   IV bVII bIII i
  { 1, 6, 3, 0 },   // Frigio:   bII bvii iv i
  { 1, 4, 5, 0 },   // Lidio:    II V vi I
  { 6, 3, 4, 0 },   // Mixolid.: bVII IV v I
  { 5, 6, 3, 2 },   // Eolico:   bVI bVII iv bIII
  { 1, 5, 3, 0 },   // Locrio:   bII bvi iv i
  { 3, 5, 4, 0 },   // M. armo.: iv bVI V i
  { 1, 6, 3, 0 },   // F. dom.:  bII bvii iv I
  { 1, 5, 3, 0 },   // D. armo.: bII bVI iv I
};
const int8_t MODE_CADENCE[NUM_SCALES] = { 4, 3, 1, 1, 6, 6, 1, 4, 1, 1 };

const char* NOTE_NAME[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// TECLADO: los 4 botones despliegan el acorde de la tonica (siempre consonante)
const int8_t KEY_DEG[4]  = { 0, 2, 4, 7 };     // fundamental, 3a, 5a, octava
// ARP: cada boton latchea el arpegio sobre un acorde de la escala
const int8_t ARP_ROOT[4] = { 0, 3, 4, 5 };     // grados I, IV, V, VI

// ==================== MODOS ====================

#define MODE_KEYS      0
#define MODE_ARP_UP    1
#define MODE_ARP_DOWN  2
#define MODE_ARP_UPDN  3
#define MODE_ARP_RND   4
#define MODE_SEQ       5
const char* MODE_NAME[6] = { "TECLADO", "ARP subida", "ARP bajada", "ARP sube-baja", "ARP aleatorio", "SECUENCIA" };
static inline bool isArpMode(int m) { return m >= MODE_ARP_UP && m <= MODE_ARP_RND; }

// ==================== ESTADO GLOBAL ====================

i2s_chan_handle_t tx_chan = NULL;
i2s_chan_handle_t rx_chan = NULL;

int16_t* micBuffer = nullptr;                 // grabacion de voz
uint8_t* netBuffer = nullptr;                 // descarga cruda desde ElevenLabs

// --- EL INSTRUMENTO: un solo sample que es la onda del sinte ---
int16_t* instData      = nullptr;             // PSRAM
uint32_t instLen       = 0;                   // muestras utiles (ya recortadas)
uint32_t instRate      = INST_RATE_PCM;       // frecuencia nativa del sample
float    instBaseFreq  = FALLBACK_FREQ;       // fundamental MEDIDA del sample
uint32_t instLoopStart = 0, instLoopEnd = 0;  // sustain loop (0 = sin loop)
uint32_t instLoopLen   = 0;
bool     instReady     = false;
uint8_t  instHue       = 0;

struct Voice {
  float    pos    = 0.0f;                     // posicion de lectura (decimales = resampleo)
  float    step   = 0.0f;                     // avance por muestra de salida
  float    env    = 0.0f;
  float    atkInc = 0.0f;
  float    relDec = 0.0f;
  uint32_t gate   = 0;                        // muestras hasta el release; GATE_INF = sostenida
  uint8_t  stage  = 0;                        // 0 ataque · 1 sosten · 2 release
  bool     active = false;
};
Voice voices[NUM_VOICES];

// --- estado musical ---
int   g_scale = 5;                            // arranca en Eolico (menor)
int   g_root  = 45;                           // MIDI de la tonica (45 = A2)
int   g_rootZone = 9;                         // zona del POT3 (9 = A)
int   g_mode  = MODE_KEYS;

// TECLADO: voz sostenida mientras presionas + drones latcheados por boton
int heldV[4]  = { -1, -1, -1, -1 };
int droneV[4] = { -1, -1, -1, -1 };

// ARP
int arpBtn = -1;                              // boton latcheado (-1 = parado)
int arpPos = 0;

// SEQ: 64 pasos de semicorchea = 4 compases, un acorde por compas
#define SEQ_STEPS 64
int8_t  seqDeg[SEQ_STEPS];                    // grado de escala del evento
uint8_t seqLen[SEQ_STEPS];                    // largo en pasos (0 = no hay evento aqui)
bool    seqStac[SEQ_STEPS];                   // staccato
int8_t  prog[4] = { 0, 5, 3, 6 };             // progresion (grados 0..6)
bool    seqPlaying = false;
int     seqPos = SEQ_STEPS - 1;
int     seqDroneV = -1;                       // pedal de tonica (BTN5 en modo SEQ)

// reloj de semicorcheas compartido por arp y seq
uint32_t clkAcc = 0;
int      clk16  = 0;

// Controles
float g_vol = 0.6f;
float g_bpm = 120.0f;

// Filtro biquad LPF resonante, recalculado por bloque desde el IMU
float bq_b0 = 1, bq_b1 = 0, bq_b2 = 0, bq_a1 = 0, bq_a2 = 0;
float bq_z1 = 0, bq_z2 = 0;
float g_cutoff = 8000.0f, g_q = 0.9f;
bool  imuOK = false;
float imuX = 0.0f, imuY = 0.0f;

// Visual
CRGB leds[NUM_LEDS];
CRGB onboard[1];
float ledNote = 0.0f;                         // flash con cada nota
float ledBeat = 0.0f;
float ledScale = 0.0f;                        // flash al cambiar de escala

enum State { ST_READY, ST_RECORDING, ST_PROCESSING, ST_ERROR };
State g_state = ST_READY;
unsigned long g_errUntil = 0;

// ==================== UTILIDADES ====================

float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(pin);
  return (float)(sum >> 4) / 4095.0f;
}

// mu-law (G.711) -> PCM 16 bit. Formato de respaldo para planes sin PCM.
static inline int16_t ulaw2linear(uint8_t u) {
  u = ~u;
  int t = (((u & 0x0F) << 3) + 0x84) << ((unsigned)(u & 0x70) >> 4);
  return (u & 0x80) ? (int16_t)(0x84 - t) : (int16_t)(t - 0x84);
}

// Zona de un pot con HISTERESIS: para cambiar hay que cruzar el borde con margen.
// Sin esto, un pot que queda justo en la frontera hace vibrar el modo/tonica.
int potZone(float pv, int n, int cur) {
  float zf = pv * (float)n;
  if (zf >= (float)n) zf = (float)n - 0.001f;
  int z = (int)zf;
  if (z == cur || cur < 0) return z;
  float border = (z > cur) ? (float)z : (float)(z + 1);
  return (fabsf(zf - border) > 0.15f) ? z : cur;
}

// ==================== TEORIA: grado de escala -> frecuencia ====================

int degreeToMidi(int deg) {
  int oct = (deg >= 0) ? deg / 7 : -((-deg + 6) / 7);
  int idx = deg - oct * 7;                    // 0..6 siempre
  return g_root + oct * 12 + SCALE_IV[g_scale][idx];
}

static inline float midiFreq(int m) { return 440.0f * powf(2.0f, (m - 69) / 12.0f); }
static inline float degFreq(int deg) { return midiFreq(degreeToMidi(deg)); }

// ==================== I2S ====================

void i2s_dac_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear    = true;
  chan_cfg.dma_desc_num  = 6;
  chan_cfg.dma_frame_num = 128;               // cola corta = notas al instante
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(OUT_RATE),
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

void i2s_mic_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

  // INMP441 = 24 bits dentro de slot de 32. Leemos STEREO 32-bit y usamos el canal IZQUIERDO.
  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(MIC_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                  I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)MIC_SCK,
      .ws   = (gpio_num_t)MIC_WS,
      .dout = I2S_GPIO_UNUSED,
      .din  = (gpio_num_t)MIC_SD,
      .invert_flags = { false, false, false },
    },
  };
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
}

// ==================== IMU ====================
// Nota: el WHO_AM_I es solo informativo. Lo que manda es despertar el chip y reintentar.

void imuWake() {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);          // PWR_MGMT_1 = 0 -> despierto
  Wire.endTransmission(true);
  delay(10);
}

bool imuInit() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  for (int intento = 0; intento < 3; intento++) {
    imuWake();
    Wire.beginTransmission(IMU_ADDR);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) == 0) {
      if (Wire.requestFrom(IMU_ADDR, 6, true) == 6) {
        while (Wire.available()) Wire.read();
        return true;
      }
    }
    delay(60);
  }
  return false;
}

void imuRead() {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) { imuOK = false; return; }
  if (Wire.requestFrom(IMU_ADDR, 6, true) < 6)  { imuOK = false; return; }

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  (void)((Wire.read() << 8) | Wire.read());    // az no se usa

  imuX += (constrain(ax / 16384.0f, -1.0f, 1.0f) - imuX) * 0.15f;
  imuY += (constrain(ay / 16384.0f, -1.0f, 1.0f) - imuY) * 0.15f;
  imuOK = true;
}

// ==================== FILTRO ====================

void updateFilter() {
  if (imuOK) {
    float nx = (imuX + 1.0f) * 0.5f;
    float ny = (imuY + 1.0f) * 0.5f;
    g_cutoff += (200.0f * powf(60.0f, nx) - g_cutoff) * 0.25f;   // ~200 Hz .. 12 kHz
    g_q      += ((0.7f + ny * 7.0f) - g_q) * 0.25f;              // 0.7 .. 7.7
  } else {
    g_cutoff += (12000.0f - g_cutoff) * 0.05f;
    g_q      += (0.7f - g_q) * 0.05f;
  }

  float fc = constrain(g_cutoff, 120.0f, 15000.0f);
  float w0 = 2.0f * PI * fc / (float)OUT_RATE;
  float cw = cosf(w0), sw = sinf(w0);
  float alpha = sw / (2.0f * g_q);
  float a0 = 1.0f + alpha;
  bq_b0 = ((1.0f - cw) * 0.5f) / a0;
  bq_b1 = (1.0f - cw) / a0;
  bq_b2 = bq_b0;
  bq_a1 = (-2.0f * cw) / a0;
  bq_a2 = (1.0f - alpha) / a0;
}

static inline float biquad(float x) {
  float y = bq_b0 * x + bq_z1;
  bq_z1   = bq_b1 * x - bq_a1 * y + bq_z2;
  bq_z2   = bq_b2 * x - bq_a2 * y;
  return y;
}

// ==================== VOCES ====================

// Dispara una nota a una frecuencia EXACTA. El paso de lectura sale de la fundamental
// MEDIDA del sample: freq/instBaseFreq es la transposicion, instRate/OUT_RATE el resampleo.
int triggerNote(float freq, uint32_t gate, float atkSec, float relSec) {
  if (!instReady) return -1;

  // robo de voz: primero una libre, si no la de menor envolvente
  int v = -1;
  for (int i = 0; i < NUM_VOICES; i++) if (!voices[i].active) { v = i; break; }
  if (v < 0) {
    float low = 1e9f;
    for (int i = 0; i < NUM_VOICES; i++) if (voices[i].env < low) { low = voices[i].env; v = i; }
  }

  // si robamos una voz que era un drone o una tecla sostenida, limpiar la referencia
  for (int k = 0; k < 4; k++) {
    if (heldV[k]  == v) heldV[k]  = -1;
    if (droneV[k] == v) droneV[k] = -1;
  }
  if (seqDroneV == v) seqDroneV = -1;

  Voice& vo = voices[v];
  vo.pos    = 0.0f;
  vo.step   = ((float)instRate / (float)OUT_RATE) * (freq / instBaseFreq);
  vo.env    = 0.0f;
  vo.atkInc = 1.0f / (atkSec * OUT_RATE);
  vo.relDec = 1.0f / (relSec * OUT_RATE);
  vo.gate   = gate;
  vo.stage  = 0;
  vo.active = true;

  ledNote = 1.0f;
  return v;
}

// Suelta una voz (entra al release). relSec > 0 cambia el largo de la cola al soltar.
void releaseVoice(int v, float relSec) {
  if (v < 0 || v >= NUM_VOICES || !voices[v].active) return;
  if (relSec > 0.0f) voices[v].relDec = 1.0f / (relSec * OUT_RATE);
  voices[v].gate = 0;
}

void killAllVoices() {
  for (int i = 0; i < NUM_VOICES; i++) voices[i].active = false;
  for (int k = 0; k < 4; k++) { heldV[k] = -1; droneV[k] = -1; }
  seqDroneV = -1;
  arpBtn = -1;
}

// ==================== RENDER ====================

void renderBlock(int16_t* out) {
  for (int n = 0; n < BUF_SAMPLES; n++) {
    float mix = 0.0f;

    for (int i = 0; i < NUM_VOICES; i++) {
      Voice& v = voices[i];
      if (!v.active) continue;

      // --- envolvente ---
      if (v.stage == 0) {
        v.env += v.atkInc;
        if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; }
      }
      if (v.stage != 2) {
        if (v.gate == 0) v.stage = 2;
        else if (v.gate != GATE_INF) v.gate--;
      }
      if (v.stage == 2) {
        v.env -= v.relDec;
        if (v.env <= 0.0f) { v.active = false; continue; }
      }

      // --- posicion: sustain loop = el sample como oscilador continuo ---
      // La primera pasada recorre el ataque real del sample; despues queda dando vueltas
      // en la zona sostenida (con crossfade ya horneado en la costura, ver buildSustainLoop).
      if (instLoopLen > 0) {
        if (v.pos >= (float)instLoopEnd) v.pos -= (float)instLoopLen;
      } else if (v.pos >= (float)instLen) {
        v.active = false;
        continue;
      }

      uint32_t i0 = (uint32_t)v.pos;
      uint32_t i1 = i0 + 1;
      if (i1 >= instLen) i1 = i0;
      float fr = v.pos - (float)i0;
      float s  = ((float)instData[i0] * (1.0f - fr) + (float)instData[i1] * fr) * (1.0f / 32768.0f);

      mix += s * v.env;
      v.pos += v.step;
    }

    mix *= 0.35f;                              // headroom para la polifonia
    float y = biquad(mix) * g_vol;
    if (y >  1.2f) y =  1.2f;
    if (y < -1.2f) y = -1.2f;
    y = y - (y * y * y) * 0.15f;               // saturacion suave en vez de recorte duro

    int16_t o = (int16_t)(constrain(y, -1.0f, 1.0f) * 32000.0f);
    out[n * 2]     = o;
    out[n * 2 + 1] = o;
  }
}

// ==================== ARPEGIADOR ====================

void arpFire(uint32_t spb) {
  int8_t base = ARP_ROOT[arpBtn];
  int8_t tones[4] = { base, (int8_t)(base + 2), (int8_t)(base + 4), (int8_t)(base + 7) };

  int idx;
  switch (g_mode) {
    case MODE_ARP_UP:   idx = arpPos & 3; break;
    case MODE_ARP_DOWN: idx = 3 - (arpPos & 3); break;
    case MODE_ARP_UPDN: { int p = arpPos % 6; idx = (p < 4) ? p : 6 - p; break; }
    default:            idx = random(0, 4); break;
  }
  arpPos++;

  triggerNote(degFreq(tones[idx]), (uint32_t)(spb * 0.6f), 0.003f, 0.12f);
}

// ==================== SECUENCIA GENERATIVA ====================

void genProgression() {
  prog[0] = 0;                                            // siempre arranca en la tonica
  prog[1] = MODE_POOL[g_scale][random(0, 4)];
  prog[2] = MODE_POOL[g_scale][random(0, 4)];
  if (prog[2] == prog[1]) prog[2] = MODE_POOL[g_scale][random(0, 4)];
  prog[3] = MODE_CADENCE[g_scale];                        // cadencia modal antes de volver al I

  Serial.print("Progresion (grados): ");
  for (int i = 0; i < 4; i++) { Serial.print(prog[i] + 1); Serial.print(i < 3 ? " - " : "\n"); }
}

void genMelody() {
  memset(seqLen, 0, sizeof(seqLen));

  int deg  = 7 + prog[0];                                 // registro: una octava sobre la tonica
  int step = 0;
  static const uint8_t LENS[8] = { 1, 1, 1, 2, 2, 2, 3, 4 };

  while (step < SEQ_STEPS) {
    int bar   = step >> 4;
    int chord = prog[bar];
    int len   = LENS[random(0, 8)];

    if (random(0, 100) < 16) { step += len; continue; }   // respiracion: silencio

    if ((step & 7) == 0) {
      // paso fuerte: la melodia se ANCLA a la nota del acorde mas cercana = siempre en armonia
      int best = deg, bd = 99;
      for (int oct = 0; oct <= 14; oct += 7)
        for (int k = 0; k < 3; k++) {
          int cand = chord + k * 2 + oct;
          int d = abs(cand - deg);
          if (d < bd) { bd = d; best = cand; }
        }
      deg = best;
    } else {
      deg += random(-2, 3);                               // paso debil: caminata por la escala
    }
    deg = constrain(deg, 2, 14);

    seqDeg[step]  = (int8_t)deg;
    seqLen[step]  = (uint8_t)len;
    seqStac[step] = (random(0, 100) < 20);
    step += len;
  }
}

void seqFire(uint32_t spb) {
  seqPos = (seqPos + 1) % SEQ_STEPS;
  uint8_t len = seqLen[seqPos];
  if (len == 0) return;
  float gateF = seqStac[seqPos] ? 0.35f : 0.85f;
  triggerNote(degFreq(seqDeg[seqPos]), (uint32_t)(spb * len * gateF), 0.003f, 0.15f);
}

// ==================== RELOJ (semicorcheas, compartido por arp y seq) ====================

void clockAdvance() {
  uint32_t spb = (uint32_t)((60.0f / g_bpm / 4.0f) * OUT_RATE);
  clkAcc += BUF_SAMPLES;
  if (clkAcc < spb) return;
  clkAcc -= spb;
  clk16 = (clk16 + 1) & 63;
  if ((clk16 & 3) == 0) ledBeat = 1.0f;

  if (isArpMode(g_mode) && arpBtn >= 0) arpFire(spb);
  if (g_mode == MODE_SEQ && seqPlaying)  seqFire(spb);
}

// ==================== DETECCION DE PITCH ====================
// Autocorrelacion normalizada sobre una ventana del CENTRO del sample (zona sostenida).
// - Guardia de error de octava: el fallo tipico es elegir un MULTIPLO del periodo real,
//   asi que nos quedamos con el lag mas corto que sea pico local y llegue al 90% del maximo.
// - Refinado parabolico del lag: afina en centesimas, no en muestras enteras.
// Devuelve 0 si no hay periodicidad clara (el que llama decide el fallback).

#define PITCH_W 2048

float detectBaseFreq(const int16_t* d, uint32_t len, uint32_t rate) {
  if (len < PITCH_W + 256) return 0.0f;
  const int16_t* x = d + (len - PITCH_W) / 2;

  int lagMin = rate / 660; if (lagMin < 8) lagMin = 8;      // hasta ~E5
  int lagMax = rate / 55;                                    // hasta ~A1
  if (lagMax > 500) lagMax = 500;
  if (lagMax > PITCH_W / 2) lagMax = PITCH_W / 2;
  if (lagMin >= lagMax) return 0.0f;
  int N = PITCH_W - lagMax;

  static float pref[PITCH_W + 1];
  static float corr[502];
  pref[0] = 0.0f;
  for (int i = 0; i < PITCH_W; i++) {
    float s = (float)x[i];
    pref[i + 1] = pref[i] + s * s;
  }
  float e0 = pref[N] - pref[0];
  if (e0 < 1.0f) return 0.0f;                                // ventana en silencio

  float best = 0.0f; int bestLag = 0;
  for (int lag = lagMin; lag <= lagMax; lag++) {
    float num = 0.0f;
    for (int i = 0; i < N; i++) num += (float)x[i] * (float)x[i + lag];
    float den = sqrtf(e0 * (pref[lag + N] - pref[lag])) + 1e-9f;
    float c = num / den;
    corr[lag] = c;
    if (c > best) { best = c; bestLag = lag; }
  }
  if (best < 0.35f || bestLag == 0) return 0.0f;

  int chosen = bestLag;
  for (int lag = lagMin + 1; lag < bestLag; lag++) {
    if (corr[lag] >= best * 0.90f && corr[lag] >= corr[lag - 1] && corr[lag] >= corr[lag + 1]) {
      chosen = lag;
      break;
    }
  }

  float lagF = (float)chosen;
  if (chosen > lagMin && chosen < lagMax) {
    float a = corr[chosen - 1], b = corr[chosen], c = corr[chosen + 1];
    float dnm = a - 2.0f * b + c;
    if (fabsf(dnm) > 1e-9f) lagF += 0.5f * (a - c) / dnm;
  }
  return (float)rate / lagF;
}

// ==================== SUSTAIN LOOP ====================
// Toma la zona sostenida del sample (45%..92%), ancla los bordes a cruces por cero
// ascendentes y HORNEA un crossfade en la costura: el final de la vuelta se funde con el
// material que precede al inicio, asi la vuelta es continua sin ningun costo en el render.
// Con esto una nota mantenida suena PARA SIEMPRE = el sample funciona como oscilador.

void buildSustainLoop() {
  instLoopLen = 0;
  if (instLen < instRate / 2) return;                       // < 0.5 s: no hay que sostener

  uint32_t a = (uint32_t)(instLen * 0.45f);
  uint32_t b = (uint32_t)(instLen * 0.92f);
  while (a + 1 < b       && !(instData[a] <= 0 && instData[a + 1] > 0)) a++;
  while (b + 1 < instLen && !(instData[b] <= 0 && instData[b + 1] > 0)) b++;
  if (b <= a + instRate / 10) return;                       // vuelta minima de 100 ms

  uint32_t F = (b - a) / 3;
  if (F > instRate / 20) F = instRate / 20;                 // crossfade de hasta 50 ms
  if (F > a) F = a;
  for (uint32_t i = 0; i < F; i++) {
    float t = (float)i / (float)F;
    float m = (float)instData[b - F + i] * (1.0f - t) + (float)instData[a - F + i] * t;
    instData[b - F + i] = (int16_t)m;
  }

  instLoopStart = a;
  instLoopEnd   = b;
  instLoopLen   = b - a;
}

// ==================== RED: GRABAR + WHISPER ====================

int recordVoice() {
  g_state = ST_RECORDING;
  int32_t raw[256];
  size_t bytesRead = 0;
  int n = 0;

  for (int k = 0; k < 10; k++)                 // descartar el "pop" de arranque del DMA
    i2s_channel_read(rx_chan, raw, sizeof(raw), &bytesRead, 50);

  while (digitalRead(BTN_REC) == LOW && n < MAX_MIC_SAMPLES) {
    if (i2s_channel_read(rx_chan, raw, sizeof(raw), &bytesRead, 200) != ESP_OK) continue;
    int got = bytesRead / sizeof(int32_t);
    for (int i = 0; i < got && n < MAX_MIC_SAMPLES; i += 2) {   // i += 2 -> canal IZQ
      int32_t s = (raw[i] >> 16) * MIC_GAIN;
      if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
      micBuffer[n++] = (int16_t)s;
    }
  }
  return n;
}

String transcribeAudio(int sampleCount) {
  if (sampleCount < 1000) return "";
  g_state = ST_PROCESSING;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(60);
  if (!client.connect("api.openai.com", 443)) return "";

  uint8_t wav[44];
  uint32_t dataSize = sampleCount * 2;
  uint32_t fileSize = dataSize + 36;
  uint32_t byteRate = MIC_RATE * 2;
  memcpy(wav, "RIFF", 4);
  wav[4]=fileSize&0xFF; wav[5]=(fileSize>>8)&0xFF; wav[6]=(fileSize>>16)&0xFF; wav[7]=(fileSize>>24)&0xFF;
  memcpy(wav+8, "WAVEfmt ", 8);
  wav[16]=16; wav[17]=0; wav[18]=0; wav[19]=0;
  wav[20]=1;  wav[21]=0;                                   // PCM
  wav[22]=1;  wav[23]=0;                                   // mono
  wav[24]=MIC_RATE&0xFF; wav[25]=(MIC_RATE>>8)&0xFF; wav[26]=(MIC_RATE>>16)&0xFF; wav[27]=(MIC_RATE>>24)&0xFF;
  wav[28]=byteRate&0xFF; wav[29]=(byteRate>>8)&0xFF; wav[30]=(byteRate>>16)&0xFF; wav[31]=(byteRate>>24)&0xFF;
  wav[32]=2;  wav[33]=0;
  wav[34]=16; wav[35]=0;
  memcpy(wav+36, "data", 4);
  wav[40]=dataSize&0xFF; wav[41]=(dataSize>>8)&0xFF; wav[42]=(dataSize>>16)&0xFF; wav[43]=(dataSize>>24)&0xFF;

  String boundary = "----PercuSynthBoundary";
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";
  String model = "\r\n--" + boundary + "\r\n";
  model += "Content-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1";
  String language = "\r\n--" + boundary + "\r\n";
  language += "Content-Disposition: form-data; name=\"language\"\r\n\r\nes";
  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t totalLen = head.length() + 44 + dataSize + model.length() + language.length() + tail.length();

  client.println("POST /v1/audio/transcriptions HTTP/1.1");
  client.println("Host: api.openai.com");
  client.println("Authorization: Bearer " + String(OPENAI_API_KEY));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(totalLen));
  client.println("Connection: close");
  client.println();

  client.print(head);
  client.write(wav, 44);
  uint8_t* bytes = (uint8_t*)micBuffer;
  size_t sent = 0;
  while (sent < dataSize) {
    size_t toSend = min((size_t)512, (size_t)(dataSize - sent));
    client.write(bytes + sent, toSend);
    sent += toSend;
    yield();
  }
  client.print(model); client.print(language); client.print(tail);

  unsigned long timeout = millis() + 30000;
  while (!client.available() && millis() < timeout) delay(50);

  String response = "";
  bool bodyStarted = false;
  while (client.available() || client.connected()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (!bodyStarted) { if (line == "\r") bodyStarted = true; }
      else response += line;
    } else { delay(10); if (!client.available()) break; }
  }
  client.stop();

  int t = response.indexOf("\"text\":\"");
  if (t > 0) { t += 8; int e = response.indexOf("\"", t); return response.substring(t, e); }
  return "";
}

// ==================== GPT: pedido en espanol -> prompt de SONIDO ====================
// GPT traduce el pedido a una descripcion clara del sonido en ingles y ese es TODO el prompt.
// La afinacion NO se pide (ElevenLabs no sabe de musica): se mide despues con detectBaseFreq.

SfxSpec buildSfxSpec(String pedido) {
  SfxSpec spec;
  spec.prompt = pedido;
  spec.dur    = 4.0f;                          // sostenido largo: material para el sustain loop
  spec.loop   = true;

#if USE_GPT_PROMPT
  g_state = ST_PROCESSING;

  pedido.replace("\\", "\\\\"); pedido.replace("\"", "\\\"");
  pedido.replace("\n", " ");   pedido.replace("\r", "");

  // El prompt a ElevenLabs es SOLO el sonido pedido, traducido claro al ingles. Sin coletillas
  // de afinacion, loop ni "sustained": eso confunde al generador. La duracion y el loop van
  // como PARAMETROS de la API, y la afinacion se mide despues aqui (detectBaseFreq).
  String sys = "El usuario describe (en espanol) un sonido que quiere generar. ";
  sys += "Traducelo a una descripcion CLARA y DIRECTA del sonido en ingles, max 15 palabras. ";
  sys += "Se fiel al pedido: no agregues restricciones, adornos ni instrucciones tecnicas. ";
  sys += "Responde SOLO un JSON en una linea, sin markdown, con esta forma exacta: {\\\"p\\\":\\\"...\\\"}. ";
  sys += "Ejemplo: 'una cuerda metalica oxidada' -> {\\\"p\\\":\\\"a rusty metallic string sound\\\"}.";

  String body = "{\"model\":\"gpt-4o-mini\",\"messages\":[";
  body += "{\"role\":\"system\",\"content\":\"" + sys + "\"},";
  body += "{\"role\":\"user\",\"content\":\"" + pedido + "\"}";
  body += "],\"max_tokens\":80,\"temperature\":0.6}";

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.openai.com/v1/chat/completions");
  http.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000);

  int code = http.POST(body);
  if (code == 200) {
    String r = http.getString();
    int p = r.indexOf("\\\"p\\\":\\\"");        // el JSON pedido viene escapado en "content"
    if (p > 0) {
      p += 8;
      int e = r.indexOf("\\\"", p);
      if (e > p) spec.prompt = r.substring(p, e);
    }
  }
  http.end();
#endif

  spec.prompt.replace("\\", " "); spec.prompt.replace("\"", " ");
  spec.prompt.trim();
  if (spec.prompt.length() < 2) spec.prompt = "a warm analog synthesizer sound";

  // El prompt va tal cual: el sonido y nada mas.
  return spec;
}

// ==================== ELEVENLABS: prompt -> instrumento ====================

static size_t readExact(WiFiClientSecure& c, uint8_t* dst, size_t need) {
  size_t got = 0;
  unsigned long t = millis();
  while (got < need) {
    int r = c.read(dst + got, need - got);
    if (r > 0) { got += r; t = millis(); }
    else {
      if (!c.connected() && c.available() == 0) break;
      if (millis() - t > 8000) break;
      delay(1);
    }
  }
  return got;
}

// Descarga el binario de audio a netBuffer. Devuelve bytes leidos, 0 si la API fallo.
size_t fetchSfx(const SfxSpec& spec, const char* outFormat) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20);
  if (!client.connect("api.elevenlabs.io", 443)) return 0;

  String body = "{\"text\":\"" + spec.prompt + "\"";
  body += ",\"duration_seconds\":" + String(spec.dur, 1);
  // 0.45 (herencia de sampler_ia) dejaba a ElevenLabs "interpretar libre" y el timbre pedido
  // se perdia. Alto = se apega al texto; el margen creativo ya lo pusimos en el prompt de GPT.
  body += ",\"prompt_influence\":0.80";
  body += ",\"model_id\":\"eleven_text_to_sound_v2\"";
  body += String(",\"loop\":") + (spec.loop ? "true" : "false") + "}";

  client.print(String("POST /v1/sound-generation?output_format=") + outFormat + " HTTP/1.1\r\n");
  client.print("Host: api.elevenlabs.io\r\n");
  client.print(String("xi-api-key: ") + ELEVEN_API_KEY + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Accept: audio/*\r\n");
  client.print("Content-Length: " + String(body.length()) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  // La generacion tarda: hay que esperar de verdad antes de rendirse
  unsigned long to = millis() + 60000;
  while (!client.available() && millis() < to) delay(20);

  String status = client.readStringUntil('\n');
  if (status.indexOf("200") < 0) {
    // El motivo real viene en el CUERPO, no en la linea de estado
    Serial.print("ElevenLabs "); Serial.print(outFormat); Serial.print(" -> "); Serial.println(status);
    while (client.connected() || client.available()) {          // saltar cabeceras
      String line = client.readStringUntil('\n');
      if (line == "\r" || line.length() <= 1) break;
    }
    String err = "";
    unsigned long tEnd = millis() + 3000;
    while ((client.connected() || client.available()) && err.length() < 400 && millis() < tEnd) {
      if (client.available()) err += (char)client.read(); else delay(2);
    }
    err.trim();
    if (err.length()) { Serial.print("  motivo: "); Serial.println(err); }
    client.stop();
    return 0;
  }

  bool chunked = false;
  while (true) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() <= 1) break;
    String low = line; low.toLowerCase();
    if (low.indexOf("transfer-encoding") >= 0 && low.indexOf("chunked") >= 0) chunked = true;
  }

  // Si viene chunked hay que quitar las cabeceras de tamano: esos bytes ASCII en el PCM
  // suenan como ruido fuerte.
  const size_t MAXB = MAX_INST_SAMPLES * 2;
  size_t len = 0;

  if (chunked) {
    while (true) {
      String sizeLine = client.readStringUntil('\n');
      sizeLine.trim();
      if (sizeLine.length() == 0) {
        if (!client.connected() && client.available() == 0) break;
        continue;
      }
      long chunkSize = strtol(sizeLine.c_str(), NULL, 16);
      if (chunkSize <= 0) break;

      size_t room   = MAXB - len;
      size_t toRead = min((size_t)chunkSize, room);
      len += readExact(client, netBuffer + len, toRead);

      size_t remain = (size_t)chunkSize - toRead;    // descartar lo que no cupo
      uint8_t tmp[256];
      while (remain > 0) {
        size_t rr = readExact(client, tmp, min(remain, sizeof(tmp)));
        if (rr == 0) break;
        remain -= rr;
      }
      client.readStringUntil('\n');
    }
  } else {
    while (client.connected() || client.available()) {
      if (client.available()) {
        size_t room = MAXB - len;
        if (room == 0) break;
        int r = client.read(netBuffer + len, room);
        if (r > 0) len += r;
      } else delay(2);
    }
  }

  client.stop();
  return len;
}

// Recorta el silencio inicial, normaliza, mide el pitch real y construye el sustain loop.
void finishInstrument(uint32_t rawLen, uint32_t rate) {
  instRate = rate;

  // pico
  int32_t peak = 1;
  for (uint32_t i = 0; i < rawLen; i++) {
    int32_t a = abs(instData[i]);
    if (a > peak) peak = a;
  }

  // primer cruce por encima de -40 dBFS respecto al pico, con 5 ms de pre-roll
  int32_t umbral = peak / 100;
  uint32_t start = 0;
  while (start < rawLen && abs(instData[start]) < umbral) start++;
  uint32_t pre = rate / 200;                       // 5 ms
  start = (start > pre) ? start - pre : 0;

  uint32_t len = rawLen - start;
  if (start > 0) memmove(instData, instData + start, len * sizeof(int16_t));

  // normalizar a ~0.9 FS
  float gain = 29500.0f / (float)peak;
  if (gain > 12.0f) gain = 12.0f;                  // no amplificar puro ruido de fondo
  if (gain > 1.0f) {
    for (uint32_t i = 0; i < len; i++) {
      int32_t v = (int32_t)(instData[i] * gain);
      instData[i] = (int16_t)constrain(v, -32768, 32767);
    }
  }

  // micro-fade de 2 ms en los bordes
  uint32_t fade = rate / 500;
  if (fade * 2 < len) {
    for (uint32_t i = 0; i < fade; i++) {
      float g = (float)i / (float)fade;
      instData[i]           = (int16_t)(instData[i] * g);
      instData[len - 1 - i] = (int16_t)(instData[len - 1 - i] * g);
    }
  }

  instLen = len;

  // --- AQUI esta la magia: medir la fundamental REAL del sample ---
  float f = detectBaseFreq(instData, instLen, instRate);
  if (f > 0.0f) {
    instBaseFreq = f;
    // nota real mas cercana (solo informativo: la afinacion es relativa a lo medido)
    float m = 69.0f + 12.0f * logf(f / 440.0f) / logf(2.0f);
    int   mi = (int)roundf(m);
    float cents = (m - mi) * 100.0f;
    Serial.print("Pitch medido: "); Serial.print(f, 1);
    Serial.print(" Hz  (~"); Serial.print(NOTE_NAME[((mi % 12) + 12) % 12]);
    Serial.print(mi / 12 - 1); Serial.print(" ");
    if (cents >= 0) Serial.print("+");
    Serial.print(cents, 0); Serial.println(" cents)");
  } else {
    instBaseFreq = FALLBACK_FREQ;
    Serial.println("Sin periodicidad clara: se asume A2 = 110 Hz (afinacion a ciegas)");
  }

  buildSustainLoop();
  if (instLoopLen > 0) {
    Serial.print("Sustain loop: "); Serial.print(instLoopStart); Serial.print(" .. ");
    Serial.print(instLoopEnd); Serial.println(" (crossfade horneado) -> drones infinitos OK");
  } else {
    Serial.println("Sample corto: sin sustain loop (las notas duran lo que dura el sample)");
  }

  instReady = (len > 500);
  instHue   = random(0, 255);
}

// Pide el sonido y lo deja cargado como instrumento. true si quedo listo.
bool generateInstrument(const SfxSpec& spec) {
  g_state = ST_PROCESSING;
  WiFi.setSleep(false);

  uint32_t rate = INST_RATE_PCM;
  size_t len = fetchSfx(spec, "pcm_22050");

  // pcm_22050 exige plan Pro. Si la key no lo permite, ulaw_8000 esta en todos los planes.
  if (len == 0) {
    Serial.println("Reintentando en ulaw_8000 (lo-fi, disponible en cualquier plan)...");
    rate = INST_RATE_ULAW;
    len  = fetchSfx(spec, "ulaw_8000");
  }

  WiFi.setSleep(true);
  if (len < 1000) return false;

  instReady = false;                               // por si estaba sonando

  uint32_t n = 0;
  if (rate == INST_RATE_ULAW) {
    n = min((uint32_t)len, (uint32_t)MAX_INST_SAMPLES);
    for (uint32_t i = 0; i < n; i++) instData[i] = ulaw2linear(netBuffer[i]);
  } else {
    n = min((uint32_t)(len / 2), (uint32_t)MAX_INST_SAMPLES);
    for (uint32_t i = 0; i < n; i++)               // s16 little-endian
      instData[i] = (int16_t)(netBuffer[i * 2] | (netBuffer[i * 2 + 1] << 8));
  }

  finishInstrument(n, rate);
  return instReady;
}

// ==================== CICLO COMPLETO DE GENERACION ====================

void generarInstrumento() {
  int n = recordVoice();
  if (n < 1000) { g_state = ST_READY; return; }

  String pedido = transcribeAudio(n);
  Serial.print("Pedido: "); Serial.println(pedido);
  if (pedido.length() < 2) { g_state = ST_ERROR; g_errUntil = millis() + 1500; return; }

  SfxSpec spec = buildSfxSpec(pedido);
  Serial.print("Timbre: "); Serial.println(spec.prompt);

  if (generateInstrument(spec)) {
    // audicion inmediata: la tonica una octava arriba (registro claro), 1 segundo
    triggerNote(degFreq(7), OUT_RATE, 0.005f, 0.4f);
    g_state = ST_READY;
  } else {
    g_state = ST_ERROR;
    g_errUntil = millis() + 2000;
  }
}

// ==================== MODO ====================

void setMode(int m) {
  if (m == g_mode) return;

  // transiciones limpias: lo que era de un modo no queda sonando fantasma en el otro.
  // Entre tipos de arpegio NO se limpia nada: cambiar el patron en vivo es parte del juego.
  if (g_mode == MODE_KEYS && m != MODE_KEYS) {
    for (int k = 0; k < 4; k++) {
      if (heldV[k]  >= 0) { releaseVoice(heldV[k],  0.3f); heldV[k]  = -1; }
      if (droneV[k] >= 0) { releaseVoice(droneV[k], 0.6f); droneV[k] = -1; }
    }
  }
  if (isArpMode(g_mode) && !isArpMode(m)) arpBtn = -1;
  if (g_mode == MODE_SEQ && m != MODE_SEQ) {
    seqPlaying = false;
    if (seqDroneV >= 0) { releaseVoice(seqDroneV, 0.8f); seqDroneV = -1; }
  }

  g_mode = m;
  Serial.print("Modo: "); Serial.println(MODE_NAME[m]);
}

// ==================== BOTONES ====================
// Regla del proyecto: el sonido sale en el FLANCO DE PRESION, sin ventanas de espera.
// El "mantener" solo AGREGA comportamiento (drone) sobre algo que ya sono.

Btn bN1 = { BTN_N1 }, bN2 = { BTN_N2 }, bN3 = { BTN_N3 }, bN4 = { BTN_N4 };

void handleNoteButton(Btn& b, int idx) {
  bool now = digitalRead(b.pin);

  if (now == LOW && b.last == HIGH) {              // presion
    b.down = millis();
    b.longDone = false;

    if (g_mode == MODE_KEYS) {
      if (droneV[idx] >= 0) {
        releaseVoice(droneV[idx], 0.5f);           // ya droneaba -> este toque lo apaga
        droneV[idx] = -1;
        b.longDone = true;                         // y no vuelve a engancharlo al mantener
      } else {
        heldV[idx] = triggerNote(degFreq(KEY_DEG[idx]), GATE_INF, 0.004f, 0.18f);
      }
    } else if (isArpMode(g_mode)) {
      if (arpBtn == idx) {
        arpBtn = -1;                               // mismo boton = parar
        Serial.println("ARP stop");
      } else {
        arpBtn = idx;                              // latch (o cambio de acorde al vuelo)
        arpPos = 0;
        Serial.print("ARP acorde grado "); Serial.println(ARP_ROOT[idx] + 1);
      }
    } else {                                       // MODE_SEQ
      switch (idx) {
        case 0:
          seqPlaying = !seqPlaying;
          if (seqPlaying) { seqPos = SEQ_STEPS - 1; clkAcc = 0; }
          Serial.print("SEQ "); Serial.println(seqPlaying ? "PLAY" : "STOP");
          break;
        case 1:
          genMelody();
          Serial.println("Nueva melodia");
          break;
        case 2:
          genProgression();
          genMelody();                             // la melodia vieja anclaba a acordes viejos
          break;
        case 3:
          if (seqDroneV >= 0) { releaseVoice(seqDroneV, 0.8f); seqDroneV = -1; }
          else seqDroneV = triggerNote(degFreq(0), GATE_INF, 0.25f, 0.8f);
          Serial.print("Pedal de tonica: "); Serial.println(seqDroneV >= 0 ? "ON" : "OFF");
          break;
      }
    }
  }

  if (now == LOW && !b.longDone && (millis() - b.down) > 600) {
    b.longDone = true;
    if (g_mode == MODE_KEYS && heldV[idx] >= 0) {
      droneV[idx] = heldV[idx];                    // la tecla se convierte en DRONE infinito
      heldV[idx]  = -1;
      Serial.print("DRONE grado "); Serial.print(KEY_DEG[idx] + 1);
      Serial.println(" enganchado (otro toque lo apaga)");
    }
  }

  if (now == HIGH && b.last == LOW) {              // suelta
    if (g_mode == MODE_KEYS && heldV[idx] >= 0) {
      releaseVoice(heldV[idx], 0.0f);              // nota normal: al soltar entra el release
      heldV[idx] = -1;
    }
  }

  b.last = now;
}

// ==================== LEDs ====================

const uint8_t MODE_HUE[6] = { 130, 180, 192, 204, 216, 24 };   // cian · violetas · naranja

void updateLeds() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  // LED 0 = instrumento: color propio del sample, flash con cada nota
  if (instReady) {
    uint8_t v = (uint8_t)min(255.0f, 35.0f + ledNote * 200.0f);
    leds[0] = CHSV(instHue, 205, v);
  } else {
    uint8_t br = (uint8_t)(14 + 12 * sinf(millis() * 0.005f));   // vacio: respira tenue
    leds[0] = CRGB(br, br, br);
  }

  // LED 1 = modo; respira si el motor de ese modo esta corriendo
  bool running = (isArpMode(g_mode) && arpBtn >= 0) || (g_mode == MODE_SEQ && seqPlaying);
  uint8_t mv = running ? (uint8_t)(90 + 60 * sinf(millis() * 0.004f)) : 45;
  leds[1] = CHSV(MODE_HUE[g_mode], 210, mv);

  // LED 2 = escala (flash al cambiarla)
  leds[2] = CHSV((uint8_t)(g_scale * 25), 220, (uint8_t)min(255.0f, 40.0f + ledScale * 200.0f));

  // LED 3 = pulso (negra)
  leds[3] = CHSV(MODE_HUE[g_mode], 150, (uint8_t)(8 + ledBeat * 180));

  // LED 4 = filtro IMU
  uint8_t fh = (uint8_t)(160.0f - constrain((g_cutoff - 200.0f) / 11800.0f, 0.0f, 1.0f) * 130.0f);
  leds[4] = CHSV(fh, 190, (uint8_t)(30 + constrain((g_q - 0.7f) / 7.0f, 0.0f, 1.0f) * 180));

  // LED 5 = estado de red
  switch (g_state) {
    case ST_READY:      leds[5] = CRGB(0, 40, 0);    break;
    case ST_RECORDING:  leds[5] = CRGB(130, 0, 0);   break;
    case ST_PROCESSING: leds[5] = CRGB(95, 55, 0);   break;
    case ST_ERROR:      leds[5] = CRGB(110, 0, 110); break;
  }

  // el LED del modulo resume el promedio de los 6
  uint16_t sr = 0, sg = 0, sb = 0;
  for (int i = 0; i < NUM_LEDS; i++) { sr += leds[i].r; sg += leds[i].g; sb += leds[i].b; }
  onboard[0] = CRGB(sr / NUM_LEDS, sg / NUM_LEDS, sb / NUM_LEDS);

  FastLED.show();

  ledNote  *= 0.82f;
  ledBeat  *= 0.86f;
  ledScale *= 0.90f;
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(400);
  setCpuFrequencyMhz(240);

  pinMode(BTN_REC, INPUT_PULLUP);
  pinMode(BTN_N1,  INPUT_PULLUP);
  pinMode(BTN_N2,  INPUT_PULLUP);
  pinMode(BTN_N3,  INPUT_PULLUP);
  pinMode(BTN_N4,  INPUT_PULLUP);

  analogReadResolution(12);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.addLeds<WS2812, RGB_PIN, GRB>(onboard, 1);
  FastLED.setBrightness(45);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // --- memoria en PSRAM ---
  micBuffer = (int16_t*)ps_malloc(MAX_MIC_SAMPLES * sizeof(int16_t));
  netBuffer = (uint8_t*)ps_malloc(MAX_INST_SAMPLES * 2);
  instData  = (int16_t*)ps_malloc(MAX_INST_SAMPLES * sizeof(int16_t));

  if (!micBuffer || !netBuffer || !instData) {
    Serial.println("Sin PSRAM suficiente. Activa 'PSRAM: OPI PSRAM' en el IDE.");
    while (1) {
      fill_solid(leds, NUM_LEDS, CRGB(110, 0, 110)); FastLED.show(); delay(300);
      fill_solid(leds, NUM_LEDS, CRGB::Black);       FastLED.show(); delay(300);
    }
  }

  i2s_dac_init();
  i2s_mic_init();
  imuOK = imuInit();
  Serial.println(imuOK ? "IMU listo (X=cutoff, Y=resonancia)" : "Sin IMU: filtro abierto fijo");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 40) {
    delay(500); intentos++;
    leds[5] = (intentos % 2) ? CRGB(60, 30, 0) : CRGB::Black;
    FastLED.show();
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sin WiFi: no se pueden generar instrumentos nuevos (lo demas funciona).");
    g_state = ST_ERROR;
    g_errUntil = millis() + 3000;
  } else {
    WiFi.setSleep(true);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    g_state = ST_READY;
  }

  randomSeed(esp_random());
  genProgression();
  genMelody();

  Serial.println("OSCILADOR IA listo. Manten BTN1 y pide un TIMBRE (una nota sostenida sale sola).");
  Serial.print("Escala: "); Serial.print(SCALE_NAME[g_scale]);
  Serial.print(" | Tonica: "); Serial.print(NOTE_NAME[g_root % 12]); Serial.println("2");
}

// ==================== LOOP ====================
// El ritmo lo marca el DMA: i2s_channel_write bloquea hasta que hay hueco, asi que
// todo el control se atiende una vez por bloque de 128 muestras (~2.9 ms).

void loop() {
  static int16_t out[BUF_SAMPLES * 2];
  static uint8_t ctrlDiv = 0;
  static unsigned long lastLed = 0;
  static bool recDown = false;
  static unsigned long recT = 0;

  // --- control cada 4 bloques (~12 ms): pots e IMU no necesitan mas ---
  if (++ctrlDiv >= 4) {
    ctrlDiv = 0;

    g_bpm = 60.0f + readPot(POT_BPM) * 140.0f;
    float pv = readPot(POT_VOL);
    g_vol = pv * pv;                                         // curva cuadratica

    // POT3 = tonica (12 zonas con histeresis)
    int rz = potZone(readPot(POT_ROOT), 12, g_rootZone);
    if (rz != g_rootZone) {
      g_rootZone = rz;
      g_root = 36 + rz;                                      // C2 .. B2
      Serial.print("Tonica: "); Serial.print(NOTE_NAME[rz]); Serial.println("2");
    }

    // POT4 = modo (6 zonas con histeresis)
    static int modeZone = MODE_KEYS;
    int mz = potZone(readPot(POT_MODE), 6, modeZone);
    if (mz != modeZone) { modeZone = mz; setMode(mz); }

    if (imuOK) imuRead();
    updateFilter();
  }

  // --- botones de nota (flanco de presion = sonido inmediato) ---
  handleNoteButton(bN1, 0);
  handleNoteButton(bN2, 1);
  handleNoteButton(bN3, 2);
  handleNoteButton(bN4, 3);

  // --- reloj (arp/seq) y audio ---
  clockAdvance();
  renderBlock(out);
  size_t bw;
  i2s_channel_write(tx_chan, out, sizeof(out), &bw, portMAX_DELAY);

  // --- LEDs a ~30 FPS ---
  if (millis() - lastLed > 33) {
    lastLed = millis();
    if (g_state == ST_ERROR && millis() > g_errUntil) g_state = ST_READY;
    updateLeds();
  }

  // --- BTN1: toque corto = escala · mantener = grabar pedido de timbre ---
  bool rn = (digitalRead(BTN_REC) == LOW);
  if (rn && !recDown) recT = millis();

  if (rn && (millis() - recT) > 250) {
    // mantener: grabar (bloquea el audio mientras dura la generacion)
    bool seqEstaba = seqPlaying;
    seqPlaying = false;
    killAllVoices();
    updateLeds();

    generarInstrumento();

    seqPlaying = seqEstaba;
    while (digitalRead(BTN_REC) == LOW) delay(10);   // no re-disparar al soltar
    rn = false;
  }

  if (!rn && recDown && (millis() - recT) <= 250) {
    // toque corto: siguiente escala. La melodia generada son GRADOS, no notas: la misma
    // secuencia se reinterpreta en la escala nueva al instante (cambio de color modal).
    g_scale = (g_scale + 1) % NUM_SCALES;
    ledScale = 1.0f;
    Serial.print("Escala: "); Serial.println(SCALE_NAME[g_scale]);
  }
  recDown = rn;
}
