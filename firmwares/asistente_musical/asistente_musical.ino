// ==============================================================================================================================================
// PERCUSYNTH - ASISTENTE MUSICAL (pad generativo + voz IA con pasa-altos) - GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo Sandoval - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
//
// Cruce de 'asistente_ia' (Whisper + GPT + TTS) con el motor de voces de 'pads_imu'.
// La diferencia de fondo con asistente_ia: alli el DAC estaba clavado a 24 kHz y la unica
// funcion que escribia al I2S era la reproduccion del TTS, con TODO el flujo bloqueante en
// loop(). Aqui hay un MEZCLADOR unico a 44.1 kHz corriendo en su propia tarea (core 1) y el
// asistente vive en otra tarea (core 0, junto al stack WiFi): la musica NUNCA se detiene,
// ni siquiera durante el handshake TLS ni mientras GPT piensa.
// ==============================================================================================================================================
// HARDWARE
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3 (PercuSynth). PSRAM OBLIGATORIA (~1.4 MB de buffers).
//
// - DAC PCM5102 por I2S  ->  I2S_NUM_0 (SALIDA / TX) a 44100 Hz, 16 bit, estereo:
//       I2S LCK / LRCK ... GPIO 39
//       I2S DIN / DATA ... GPIO 40   (DIN del DAC = DOUT del ESP32)
//       I2S BCK / BCLK ... GPIO 41
//
// - Microfono INMP441 por I2S  ->  I2S_NUM_1 (ENTRADA / RX) a 16000 Hz:
//       WS  (LRCL) ....... GPIO 11
//       SCK (BCLK) ....... GPIO 12
//       SD  (DOUT) ....... GPIO 13   (SD del micro = DIN del ESP32)
//       L/R .............. GND       (dato en el slot IZQUIERDO)
//       VDD .............. 3.3V      (NO 5V)
//
// - Botones (INPUT_PULLUP, presionado = LOW): 44, 42, 0, 45, 47
// - Potenciometros: ADC 1, 2, 8, 10  -> los cuatro son de la unidad ADC1, la unica que NO
//   entra en conflicto con el WiFi (ADC2 queda inutilizable con la radio encendida).
// - Indicadores: 6 LEDs SMD WS2812 on-board (data GPIO 46)
// - Sin IMU: el filtro del pad se maneja por potenciometro.
// ==============================================================================================================================================
// ARDUINO IDE SETTINGS
// ==============================================================================================================================================
// - Placa:           ESP32S3 Dev Module
// - Flash Mode:      DIO            (IMPORTANTE en este hardware para que el I2S funcione bien)
// - PSRAM:           OPI PSRAM      (obligatorio; sin PSRAM parpadea magenta y no arranca)
// - USB CDC On Boot: opcional
// - Upload/Monitor:  115200 baud
// ==============================================================================================================================================
// LIBRERIAS REQUERIDAS
// ==============================================================================================================================================
// - WiFi.h / WiFiClientSecure.h / HTTPClient.h   (core ESP32 Arduino)
// - driver/i2s_std.h                             (core ESP32 Arduino, nuevo driver I2S)
// - FastLED                                      (indicadores WS2812)
// ==============================================================================================================================================
// DESCRIPCION
// ==============================================================================================================================================
// Una cama armonica generativa suena SIEMPRE, y encima conversas con GPT por voz:
//
//   MUSICA (permanente, tarea de audio en core 1)
//     Pad polifonico estereo de 32 voces + arpegio, en modo AUTO: sortea tonalidad (12),
//     modo (mayor/menor) y una progresion diatonica funcional de 4-8 acordes que termina
//     en V (cadencia V->i al loopear), en 4/4 a 80 BPM. Filtro biquad LPF resonante con
//     cutoff y Q en potenciometros (SIN LFO: el filtro se queda donde lo dejas).
//
//   ASISTENTE (tarea en core 0, junto al WiFi)
//     1) Manten BTN1 y habla (max 5 s, INMP441 a 16 kHz mono).
//     2) Whisper (whisper-1, es) transcribe.
//     3) GPT-4o-mini responde con tu contexto personalizado embebido en FLASH.
//     4) TTS (tts-1, pcm 24 kHz) se descarga completo a PSRAM.
//     5) El MEZCLADOR lo resamplea 24k -> 44.1k, le aplica un PASA-ALTOS biquad con
//        cutoff al potenciometro, y lo suma a la musica. La voz NO pasa por el filtro
//        del pad (si no, cerrar el pad tambien apagaria a GPT).
//
//   Mientras GPT piensa, la musica sigue sonando: esa es toda la gracia.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// ── BOTONES ───────────────────────────────────────────────────────────────────
// - BTN1 (GPIO44) MANTENER  -> grabar tu voz. La musica NO se corta: solo baja a REC_DUCK
//                              (~-5 dB) con un fundido de 120 ms. El INMP441 igual escucha
//                              el parlante, asi que lo que protege la transcripcion es el
//                              PASA-ALTOS de 250 Hz sobre el micro (updateMicHP): el pad
//                              tiene casi toda su energia debajo de ese corte y la voz no,
//                              porque la inteligibilidad vive en 300 Hz - 3.4 kHz.
// - BTN2 (GPIO42)           -> CANCION NUEVA: re-sortea tonalidad, modo y progresion.
// - BTN3 (GPIO0)            -> Forma de onda del pad: Seno -> Sierra -> Cuadrada -> Triangular.
// - BTN4 (GPIO45)           -> Tipo de arpegio: UP / DOWN / UP-DOWN / DOWN-UP / RANDOM / CHORD.
// - BTN5 (GPIO47) MANTENER  -> PANEL B (mientras lo tengas apretado).
//
// ── PANEL A (pots sueltos, es donde vives) ────────────────────────────────────
// - POT1 (ADC1)  -> CUTOFF del filtro del pad   (200 Hz - 9 kHz)
// - POT2 (ADC2)  -> RESONANCIA (Q) del pad      (0.7 - 12)
// - POT3 (ADC8)  -> PASA-ALTOS de la voz de GPT (20 Hz limpio -> 2 kHz telefono/megafono)
// - POT4 (ADC10) -> VOLUMEN del pad
//
// ── PANEL B (manteniendo BTN5) ────────────────────────────────────────────────
// - POT1 (ADC1)  -> PROFUNDIDAD del ducking (0 = sin sidechain -> 1 = la musica casi mutea
//                   cuando GPT habla). Es un sidechain real: sigue la envolvente de la voz.
// - POT2 (ADC2)  -> VOLUMEN de la voz de GPT
// - POT3 (ADC8)  -> VELOCIDAD del arpegio (2 - 16 notas/s)
// - POT4 (ADC10) -> VOLUMEN del arpegio (en 0 = arpegio apagado, queda solo el pad)
//
// CONGELADO DE CONTROLES: al entrar o salir del Panel B los pots quedan CONGELADOS y cada
// uno retoma el control solo cuando lo MUEVES (>= 4 %, tres lecturas seguidas). Sin esto, al
// soltar BTN5 el cutoff pegaria un salto a donde quedo el pot del panel B.
//
// ── LEDS (6 SMD on-board) ─────────────────────────────────────────────────────
// LED 0 = ESTADO:  verde = listo · rojo = grabando · ambar = procesando (Whisper/GPT)
//                  cian = hablando · magenta parpadeante = error (sin WiFi / sin PSRAM)
// LEDs 1-5 = VU:   nivel de la MUSICA normalmente, nivel de la VOZ mientras GPT habla.
// ==============================================================================================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <driver/i2s_std.h>
#include <FastLED.h>
#include <math.h>

// ─── Tipos (arriba del todo para que el IDE de Arduino genere bien los prototipos) ───
struct BiqState { float x1, x2, y1, y2; };
struct BtnState { uint8_t pin; bool last; unsigned long lastPress; };

// ==============================================================================================
// CONFIGURACION USUARIO
// ==============================================================================================

// --- Credenciales (WiFi + OpenAI) ---
// No viven en este archivo. Copia secretos.example.h a secretos.h (misma carpeta del
// sketch) y escribe ahi tus claves. secretos.h esta en .gitignore: nunca se sube al repo.
#if !__has_include("secretos.h")
  #error "Falta secretos.h: copia secretos.example.h a secretos.h y pon tus claves."
#endif
#include "secretos.h"

// ==============================================================================================
// PINES
// ==============================================================================================

// I2S salida (DAC PCM5102) - I2S_NUM_0
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41

// I2S entrada (mic INMP441) - I2S_NUM_1
#define MIC_WS    11
#define MIC_SCK   12
#define MIC_SD    13

// Botones (INPUT_PULLUP)
#define BTN1_PIN  44        // grabar (mantener)
#define BTN2_PIN  42        // cancion nueva
#define BTN3_PIN   0        // forma de onda
#define BTN4_PIN  45        // tipo de arpegio
#define BTN5_PIN  47        // Panel B (mantener)
const unsigned long DEBOUNCE_MS = 200;

// Potenciometros (TODOS en ADC1: el WiFi deja ADC2 inservible)
#define POT1   1
#define POT2   2
#define POT3   8
#define POT4  10

#define LED_PIN   46
#define NUM_LEDS   6

// ==============================================================================================
// AUDIO
// ==============================================================================================

#define SAMPLE_RATE     44100      // salida del DAC (todo el motor musical corre aqui)
#define BUFFER_SAMPLES  128
#define MIC_RATE        16000      // grabacion + WAV para Whisper
#define TTS_RATE        24000      // el TTS de OpenAI en response_format=pcm

// Relacion de resampleo 24 kHz -> 44.1 kHz para la voz (avance de fase por muestra de salida)
const float TTS_STEP = (float)TTS_RATE / (float)SAMPLE_RATE;   // 0.544217...

#define RECORD_SECONDS 5
#define MAX_SAMPLES (MIC_RATE * RECORD_SECONDS)
#define MIC_GAIN   6               // el INMP441 es sensible pero la voz queda baja

// ~100 s de voz a 24 kHz mono 16 bit (48 KB/s). Antes eran 25 s; se amplio para que GPT
// pueda extenderse cuando la pregunta lo pide (max_tokens 500 en askGPT ≈ 370 palabras
// ≈ 150 s de habla, asi que el techo real de la respuesta lo pone ESTE buffer). Si se
// llena, la descarga descarta el resto y la frase se corta: subelo si te pasa seguido.
#define MAX_TTS_BYTES 4800000

// Nivel de la musica MIENTRAS grabas. La musica ya NO se mutea: 1.0 = sin tocarla,
// 0.55 ≈ -5 dB (sigue sonando claramente, pero le deja aire al microfono). El INMP441
// escucha el parlante, asi que este numero es el compromiso entre "no se corta la
// musica" y "Whisper entiende". Junto con el pasa-altos del micro (ver micHP) es lo
// que permite grabar sin silenciar el pad.
const float REC_DUCK = 0.55f;

// ==============================================================================================
// ESTADO COMPARTIDO ENTRE TAREAS
// ==============================================================================================
// Productor/consumidor unico en cada direccion: la tarea del asistente escribe ttsBuf y
// levanta ttsActive; la tarea de audio lo consume y lo baja. Con 'volatile' basta, no hace
// falta mutex (y un mutex en el render seria justo lo que no queremos).

i2s_chan_handle_t tx_chan = NULL;   // DAC
i2s_chan_handle_t rx_chan = NULL;   // mic

int16_t*  audioBuffer = nullptr;    // grabacion del micro (PSRAM)
uint8_t*  ttsBuf      = nullptr;    // PCM del TTS (PSRAM)

volatile size_t   ttsBytes  = 0;    // bytes validos en ttsBuf
volatile bool     ttsActive = false;// true = el mezclador lo esta reproduciendo
volatile bool     g_recording = false;   // BTN1 apretado -> musica en silencio

enum State { ST_READY, ST_RECORDING, ST_PROCESSING, ST_SPEAKING, ST_ERROR };
volatile int g_state = ST_READY;

volatile float g_musicLevel = 0.0f;  // para el VU de los LEDs
volatile float g_voiceLevel = 0.0f;

// ==============================================================================================
// CONTEXTO PERSONALIZADO (en FLASH)
// ==============================================================================================
//
// ESTO ES UNA PLANTILLA: reemplaza el texto de abajo por la informacion que quieras que el
// asistente sepa. Es lo que lo convierte en TU asistente y no en un GPT generico.
//
// Ideas de que poner: quien eres o que es tu proyecto, productos o servicios, y los datos
// concretos que quieras que responda bien (fechas, precios, lugar, requisitos, contacto).
// Mientras mas concreto el dato, menos se lo inventa. Sirve cerrar con una linea del tipo
// "no inventes datos que no aparezcan aqui: di que los consulten en <tu web>".
//
// Reglas practicas:
//   - No pongas nada privado: esto se compila dentro del firmware y viaja a la API de OpenAI
//     en cada pregunta. Si publicas tu sketch, publicas este texto.
//   - Sin comillas dobles sin escapar y sin acentos ni enes: el JSON se arma a mano mas abajo
//     y los caracteres raros lo pueden romper.
//   - El largo se mide con strlen_P, asi que puedes hacerlo crecer sin tocar nada mas.
//   - Si lo dejas vacio, el asistente igual funciona con el conocimiento general del modelo.
//
// Cambia tambien el prompt de sistema en la funcion de GPT (mas abajo) para que se presente
// como tu asistente: aqui va lo que SABE, alla va quien DICE SER.

const char CONTEXT_PERSONALIZADO[] PROGMEM = R"(
CONTEXTO:

SOBRE MI / SOBRE EL PROYECTO:
[Describe aqui quien eres o que es tu proyecto, en dos o tres frases.]

PRODUCTOS O SERVICIOS:
1. [Nombre]
   - [Que es, en una linea.]
   - [Detalle util: para quien es, que incluye.]

2. [Nombre]
   - [Que es, en una linea.]

DATOS CONCRETOS QUE DEBE RESPONDER BIEN:
- [Fechas, horarios, lugar, precio, cupos, requisitos... lo que te pregunten seguido.]

CONTACTO:
[Web, correo, redes.]

No inventes datos que no aparezcan aqui: si te preguntan algo que falta, di que lo
consulten en el contacto de arriba.
)";

// El tamano se toma del propio string en FLASH. Antes era un buffer fijo de 3072 bytes: al
// crecer el contexto lo habria desbordado y strcpy_P habria pisado memoria (un fallo
// silencioso y dificil de rastrear). Si agrandas el texto, esto sigue funcionando solo.
String getCustomContext() {
  size_t n = strlen_P(CONTEXT_PERSONALIZADO);
  char* buffer = (char*)malloc(n + 1);
  if (!buffer) return "";
  strcpy_P(buffer, CONTEXT_PERSONALIZADO);
  String context = String(buffer);
  free(buffer);
  return context;
}

// ==============================================================================================
// MOTOR DE VOCES (portado de pads_imu, sin IMU y sin LFO)
// ==============================================================================================

#define NUM_VOICES   32

struct Voice {
  bool     active;
  uint8_t  kind;     // 0 = pad (AHR sostenido) · 1 = arpegio (pluck que decae solo)
  uint8_t  layer;    // 0 core · 1 sub
  float    freq;
  float    phase;
  float    env;
  uint8_t  stage;    // 0 attack · 1 sustain(pad) · 2 release(pad) · 3 decay(arp)
  float    gain;
  float    lGain, rGain;
  uint32_t age;
};
#define LYR_CORE  0
#define LYR_SUB   1

Voice voices[NUM_VOICES];
uint32_t voiceCounter = 0;

const float BASE_FREQ = 130.81f;           // C3 (referencia de semitono 0)

// Tabla semitono -> relacion de frecuencia. Evita powf() al disparar acordes (un acorde
// dispara muchas voces de golpe y el pico de powf provocaba glitches).
#define SEMI_OFFSET 72
#define SEMI_LUT_N  145
float semiLUT[SEMI_LUT_N];

inline float semiToFreq(int semi) {
  int idx = semi + SEMI_OFFSET;
  if (idx < 0) idx = 0; else if (idx >= SEMI_LUT_N) idx = SEMI_LUT_N - 1;
  return BASE_FREQ * semiLUT[idx];
}

// Tabla de seno (oscilador barato, sin sinf() por muestra)
float sineLUT[256];
inline float oscSine(float phase) {
  float f = phase * 256.0f;
  int i0 = (int)f; float frac = f - (float)i0;
  i0 &= 255; int i1 = (i0 + 1) & 255;
  return sineLUT[i0] + (sineLUT[i1] - sineLUT[i0]) * frac;
}

// ─── Timbre del pad ────────────────────────────────────────
int   waveType    = 3;        // 0 seno · 1 sierra · 2 cuadrada · 3 triangular
bool  subOsc      = true;     // capa una octava abajo (cuerpo)
float detuneCents = 12.0f;    // ensemble (voces duplicadas con pan opuesto)
float panWidth    = 0.54f;
float g_volume    = 0.5f;     // POT4 Panel A -> volumen del PAD
float attackInc   = 1.0f / (2.0f * SAMPLE_RATE);   // ataque de 2 s (pad clasico)
float releaseCoef = 0.0f;                          // se calcula en setup

// ─── Arpegio ───────────────────────────────────────────────
#define ARP_MAX  8
float arpVol      = 0.30f;
float arpRate     = 6.0f;
int   arpRange    = 2;
float arpGate     = 0.18f;
#define ARP_UP      0
#define ARP_DOWN    1
#define ARP_UPDOWN  2
#define ARP_DOWNUP  3
#define ARP_RANDOM  4
#define ARP_CHORD   5
#define ARP_NTYPES  6
int   arpType     = ARP_RANDOM;   // en AUTO suena mas organico que UP
int   arpNotes[4];
int   arpCount    = 0;
int   arpStep     = 0;
uint32_t arpRng   = 0x12345678;
uint32_t arpSampleCount    = 0;
uint32_t arpSamplesPerStep = 7350;
float arpAtkInc   = 0.02f;
float arpDecCoef  = 0.0f;         // se calcula en setup

// ─── Filtro del pad (biquad LPF resonante, estereo) ────────
float cutoffBase  = 1200.0f;      // POT1 Panel A
float qBase       = 1.5f;         // POT2 Panel A
float f_b0, f_b1, f_b2, f_a1, f_a2;
BiqState bqL = {0, 0, 0, 0};
BiqState bqR = {0, 0, 0, 0};

// ─── Cama armonica generativa (modo AUTO permanente) ───────
#define AUTO_BPM   80
const uint32_t beatSamples = (uint32_t)SAMPLE_RATE * 60 / AUTO_BPM;
uint32_t autoSeed = 0x1234ABCDu;
int   autoKeyRoot = 0;
int   autoLen     = 4;
int   autoRoot[8];
bool  autoMin[8];
int   autoBeats = 16;
int   autoIdx = 0;
uint32_t autoSampleCount = 0, autoChordSamples = 0;
int   soundingRoot = 0;

// ==============================================================================================
// CANAL DE VOZ (TTS) : resampleo + PASA-ALTOS + ducking
// ==============================================================================================

float ttsPos   = 0.0f;        // posicion fraccionaria de lectura en ttsBuf (en muestras)
float voiceVol = 1.0f;        // POT2 Panel B

// Pasa-altos biquad RBJ sobre la voz. POT3 Panel A -> 20 Hz (limpio) a 2 kHz (megafono).
float hpCutoff = 20.0f;
float h_b0, h_b1, h_b2, h_a1, h_a2;
BiqState hpState = {0, 0, 0, 0};

// PASA-ALTOS DEL MICROFONO (biquad RBJ a 16 kHz, 250 Hz). Como la musica ya no se mutea al
// grabar, el INMP441 capta el pad — y la energia del pad esta casi toda por DEBAJO de 250 Hz
// (raiz, sub-octava, cuerpo del acorde). Quitarsela antes de mandar el WAV le saca a Whisper
// justo lo que le estorba. La voz no sufre: la telefonia corta en 300 Hz y se entiende
// perfecto, porque la inteligibilidad vive en los formantes (300 Hz - 3.4 kHz).
float m_b0, m_b1, m_b2, m_a1, m_a2;
BiqState micHP = {0, 0, 0, 0};

// Ducking (sidechain): la envolvente de la VOZ baja el bus de musica.
// Ataque rapido para que la primera silaba ya despeje, release lento para que la musica
// no "bombee" entre palabra y palabra.
float duckDepth = 0.6f;       // POT1 Panel B
float duckEnv   = 0.0f;
const float DUCK_ATT = 0.0045f;    // ~5 ms
const float DUCK_REL = 0.000065f;  // ~350 ms

// Compuerta de la musica al grabar: baja de 1.0 a REC_DUCK con un fundido de 120 ms
// (mas lento que el mute de 50 ms de antes: al no cortar del todo, un fundido brusco se
// notaria como un tiron en medio del pad).
float musicGate = 1.0f;
const float GATE_STEP = (float)BUFFER_SAMPLES / (0.12f * SAMPLE_RATE);

// ==============================================================================================
// PANELES / POTS CONGELADOS
// ==============================================================================================

bool  panelB = false;
bool  panelChanged = false;
float potAnchor[4]  = {0, 0, 0, 0};
bool  potLive[4]    = {true, true, true, true};
uint8_t potMoveCnt[4] = {0, 0, 0, 0};
const float POT_MOVE_THR = 0.04f;

CRGB leds[NUM_LEDS];

// ==============================================================================================
// UTILIDADES
// ==============================================================================================

float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(pin);   // 16x oversampling (anti-ruido)
  return (float)(sum >> 4) / 4095.0f;
}

bool buttonPressed(BtnState &b) {
  bool now = digitalRead(b.pin);
  unsigned long t = millis();
  bool fired = false;
  if (now == LOW && b.last == HIGH && (t - b.lastPress) > DEBOUNCE_MS) {
    b.lastPress = t;
    fired = true;
  }
  b.last = now;
  return fired;
}

// ─── Disparar UNA voz (libre o roba la menos audible) ──────
void spawnVoice(int semi, float gain, float pan, uint8_t kind, uint8_t layer) {
  int idx = -1;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (!voices[i].active) { idx = i; break; }
  }
  if (idx < 0) {
    // Ninguna libre -> robar la MENOS AUDIBLE (env*gain minimo), no la mas vieja: asi se
    // roban primero las colas casi apagadas y no se oye el "click" del robo.
    float quietest = 1e30f;
    idx = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
      float a = voices[i].env * voices[i].gain;
      if (a < quietest) { quietest = a; idx = i; }
    }
  }

  // detune = 2^a con a pequeno -> aprox. polinomica barata (evita exp2f por voz)
  float a = (pan * detuneCents) / 1200.0f;
  float det = 1.0f + a * (0.6931472f + a * 0.2401597f);

  Voice &v = voices[idx];
  v.active = true;
  v.kind   = kind;
  v.layer  = layer;
  v.freq   = semiToFreq(semi) * det;
  float ph = (float)voiceCounter * 0.61803f; ph -= (float)(int)ph;   // fase decorrelacionada
  v.phase  = ph;
  v.env    = 0.0f;
  v.stage  = 0;
  v.gain   = gain;
  v.age    = voiceCounter++;

  // Paneo equal-power via la tabla de seno (evita cosf/sinf por voz)
  float p = pan * panWidth;
  if (p >  1.0f) p =  1.0f;
  if (p < -1.0f) p = -1.0f;
  float phr = (p + 1.0f) * 0.125f;
  v.rGain = oscSine(phr);
  v.lGain = oscSine(phr + 0.25f);
}

void releaseAll() {
  for (int i = 0; i < NUM_VOICES; i++)
    if (voices[i].active && voices[i].kind == 0 && voices[i].stage < 2)
      voices[i].stage = 2;
}

// Acota las colas del pad: con release largo se apilaban y saturaban la CPU.
void capPad(int keep) {
  while (true) {
    int cnt = 0, vic = -1; float lo = 1e30f;
    for (int i = 0; i < NUM_VOICES; i++)
      if (voices[i].active && voices[i].kind == 0) {
        cnt++;
        float a = voices[i].env * voices[i].gain;
        if (a < lo) { lo = a; vic = i; }
      }
    if (cnt <= keep || vic < 0) break;
    voices[vic].active = false;
  }
}

void capArp(int toSpawn) {
  while (true) {
    int active = 0, oldest = -1; uint32_t oa = 0xFFFFFFFF;
    for (int i = 0; i < NUM_VOICES; i++)
      if (voices[i].active && voices[i].kind == 1) {
        active++;
        if (voices[i].age < oa) { oa = voices[i].age; oldest = i; }
      }
    if (active + toSpawn <= ARP_MAX || oldest < 0) break;
    voices[oldest].active = false;
  }
}

// ─── Disparar un acorde de la cama generativa ──────────────
void triggerAutoChord(int rootBase, bool minor) {
  releaseAll();
  capPad(8);
  int root = rootBase;
  soundingRoot = root;
  int third = minor ? 3 : 4;
  arpNotes[0] = root; arpNotes[1] = root + third; arpNotes[2] = root + 7; arpNotes[3] = root + 12;
  arpCount = 4; arpStep = 0;
  const float pans[3] = { 0.0f, -0.7f, 0.7f };
  const int   iv[3]   = { 0, third, 7 };
  for (int n = 0; n < 3; n++) {
    int semi = root + iv[n];
    spawnVoice(semi, 0.9f, pans[n] - 0.25f, 0, LYR_CORE);
    spawnVoice(semi, 0.9f, pans[n] + 0.25f, 0, LYR_CORE);
  }
  if (subOsc) spawnVoice(root - 12, 0.85f, 0.0f, 0, LYR_SUB);
}

// ─── Generar una CANCION NUEVA (tonalidad + modo + progresion) ──
// Grados diatonicos y movimientos funcionales coherentes, MAYOR y MENOR. Empieza en la
// tonica y termina en V -> al loopear cae la cadencia V->i.
void startAuto() {
  autoSeed = autoSeed * 1664525u + 1013904223u + (uint32_t)micros() + voiceCounter;
  uint32_t s = autoSeed;

  static const int8_t majOff[6] = { 0, 2, 4, 5, 7, 9 };          // I  ii iii IV V  vi
  static const bool   majMin[6] = { false, true, true, false, false, true };
  static const int8_t minOff[6] = { 0, 3, 5, 7, 8, 10 };         // i  III iv V  VI VII
  static const bool   minMin[6] = { true, false, true, false, false, false };
  static const int8_t NEXTmaj[6][4] = {
    {3,4,5,1}, {4,3,4,3}, {5,3,5,3}, {4,0,1,4}, {0,5,0,5}, {3,1,4,3}
  };
  static const int8_t NEXTmin[6][4] = {
    {2,3,4,1}, {4,2,5,4}, {3,0,5,3}, {0,4,0,4}, {2,1,3,2}, {1,0,1,0}
  };

  s = s * 1664525u + 1013904223u; bool minorKey = ((s >> 17) & 1);
  s = s * 1664525u + 1013904223u; autoKeyRoot = (int)((s >> 16) % 12);
  s = s * 1664525u + 1013904223u; autoLen = 4 + (int)((s >> 16) % 5);   // 4-8 acordes
  s = s * 1664525u + 1013904223u; autoBeats = (((s >> 16) & 1) ? 16 : 8);

  int deg = 0;
  int endDeg = minorKey ? 3 : 4;                                 // termina en V
  for (int i = 0; i < autoLen; i++) {
    if      (i == autoLen - 1) deg = endDeg;
    else if (i > 0) { s = s * 1664525u + 1013904223u;
                      deg = minorKey ? NEXTmin[deg][(s >> 16) % 4] : NEXTmaj[deg][(s >> 16) % 4]; }
    autoRoot[i] = autoKeyRoot + (minorKey ? minOff[deg] : majOff[deg]);
    autoMin[i]  = minorKey ? minMin[deg] : majMin[deg];
  }
  autoSeed = s;
  autoIdx = 0; autoSampleCount = 0;
  autoChordSamples = (uint32_t)((uint64_t)autoBeats * beatSamples);
  triggerAutoChord(autoRoot[0], autoMin[0]);
}

// ─── Un paso del arpegio (nota aleatoria del acorde que suena, o el tipo elegido) ──
void arpTrigger() {
  int total = arpCount * arpRange;
  if (total < 1) return;

  if (arpType == ARP_CHORD) {
    int oct = arpStep % arpRange;
    capArp(arpCount);
    for (int k = 0; k < arpCount; k++) {
      int semi  = arpNotes[k] + 12 * oct + 12;
      float pan = (k & 1) ? 0.4f : -0.4f;
      spawnVoice(semi, arpVol, pan, 1, 0);
    }
  } else {
    int p, idx = arpStep % total;
    if      (arpType == ARP_UP)     p = idx;
    else if (arpType == ARP_DOWN)   p = total - 1 - idx;
    else if (arpType == ARP_UPDOWN){ int period = 2 * total;
                                     int j = arpStep % period; p = (j < total) ? j : (period - 1 - j); }
    else if (arpType == ARP_DOWNUP){ int period = 2 * total;
                                     int j = arpStep % period; int pu = (j < total) ? j : (period - 1 - j);
                                     p = total - 1 - pu; }
    else if (arpType == ARP_RANDOM){ arpRng = arpRng * 1664525u + 1013904223u;
                                     p = (int)((arpRng >> 8) % (uint32_t)total); }
    else                            p = idx;
    int note  = p % arpCount;
    int oct   = p / arpCount;
    int semi  = arpNotes[note] + 12 * oct + 12;     // +12: una octava sobre el pad
    float pan = (arpStep & 1) ? 0.5f : -0.5f;
    capArp(1);
    spawnVoice(semi, arpVol, pan, 1, 0);
  }

  arpStep++;
  if (arpStep >= 1000000) arpStep = 0;
}

// ─── PolyBLEP + osciladores ────────────────────────────────
inline float polyBlep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  else if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

inline float osc(float phase, float dt) {
  if (waveType == 0) {
    return oscSine(phase);
  } else if (waveType == 1) {                 // sierra (anti-aliasing)
    return (2.0f * phase - 1.0f) - polyBlep(phase, dt);
  } else if (waveType == 2) {                 // cuadrada = diferencia de 2 sierras
    float saw1 = (2.0f * phase - 1.0f) - polyBlep(phase, dt);
    float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
    float saw2 = (2.0f * p2 - 1.0f) - polyBlep(p2, dt);
    return (saw1 - saw2) * 0.6f;
  } else {                                    // triangular
    return (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
  }
}

// ─── Coeficientes: LPF resonante del pad ───────────────────
void updatePadFilter() {
  float cutoff = cutoffBase;
  if (cutoff < 80.0f)    cutoff = 80.0f;
  if (cutoff > 12000.0f) cutoff = 12000.0f;
  float Q = qBase;
  if (Q < 0.5f) Q = 0.5f;
  if (Q > 18.0f) Q = 18.0f;

  float omega = 2.0f * (float)M_PI * cutoff / SAMPLE_RATE;
  float s = sinf(omega), c = cosf(omega);
  float alpha = s / (2.0f * Q);

  float b0 = (1.0f - c) * 0.5f;
  float b1 =  1.0f - c;
  float b2 = (1.0f - c) * 0.5f;
  float a0 =  1.0f + alpha;
  float a1 = -2.0f * c;
  float a2 =  1.0f - alpha;

  f_b0 = b0 / a0; f_b1 = b1 / a0; f_b2 = b2 / a0;
  f_a1 = a1 / a0; f_a2 = a2 / a0;
}

// ─── Coeficientes: PASA-ALTOS de la voz (RBJ high-pass, Q = 0.707) ──
// b0=(1+cos w0)/2 · b1=-(1+cos w0) · b2=(1+cos w0)/2 · a0=1+alpha · a1=-2cos w0 · a2=1-alpha
void updateVoiceHP() {
  float cutoff = hpCutoff;
  if (cutoff < 20.0f)   cutoff = 20.0f;
  if (cutoff > 4000.0f) cutoff = 4000.0f;
  const float Q = 0.7071f;

  float omega = 2.0f * (float)M_PI * cutoff / SAMPLE_RATE;
  float s = sinf(omega), c = cosf(omega);
  float alpha = s / (2.0f * Q);

  float b0 =  (1.0f + c) * 0.5f;
  float b1 = -(1.0f + c);
  float b2 =  (1.0f + c) * 0.5f;
  float a0 =   1.0f + alpha;
  float a1 =  -2.0f * c;
  float a2 =   1.0f - alpha;

  h_b0 = b0 / a0; h_b1 = b1 / a0; h_b2 = b2 / a0;
  h_a1 = a1 / a0; h_a2 = a2 / a0;
}

// ─── Coeficientes: PASA-ALTOS del microfono (RBJ, 250 Hz @ MIC_RATE) ──
// Ojo: se calcula contra MIC_RATE (16 kHz), no contra SAMPLE_RATE. Usar la tasa
// equivocada aqui correria el corte casi 3 octavas.
void updateMicHP() {
  const float cutoff = 250.0f;
  const float Q = 0.7071f;

  float omega = 2.0f * (float)M_PI * cutoff / MIC_RATE;
  float s = sinf(omega), c = cosf(omega);
  float alpha = s / (2.0f * Q);

  float b0 =  (1.0f + c) * 0.5f;
  float b1 = -(1.0f + c);
  float b2 =  (1.0f + c) * 0.5f;
  float a0 =   1.0f + alpha;
  float a1 =  -2.0f * c;
  float a2 =   1.0f - alpha;

  m_b0 = b0 / a0; m_b1 = b1 / a0; m_b2 = b2 / a0;
  m_a1 = a1 / a0; m_a2 = a2 / a0;
}

inline float applyBiquad(BiqState &st, float in, float b0, float b1, float b2, float a1, float a2) {
  float out = b0 * in + b1 * st.x1 + b2 * st.x2 - a1 * st.y1 - a2 * st.y2;
  st.x2 = st.x1; st.x1 = in;
  st.y2 = st.y1; st.y1 = out;
  return out;
}

// ==============================================================================================
// I2S
// ==============================================================================================

void i2s_dac_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  // Colchon DMA generoso (~70 ms). Aqui NO hay boton que dispare percusion, asi que la
  // latencia no importa y ese colchon es justo lo que aguanta el handshake TLS sin glitch.
  chan_cfg.dma_desc_num  = 12;
  chan_cfg.dma_frame_num = 256;
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

void i2s_mic_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));   // solo RX

  // El INMP441 entrega 24 bits dentro de un slot de 32. Usamos STEREO 32-bit (64 BCLK por
  // frame, que es lo que el micro espera) y nos quedamos con el canal IZQUIERDO.
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

// ==============================================================================================
// TAREA DE AUDIO  (core 1) — el UNICO que escribe al DAC
// ==============================================================================================

void applyPot(int i, float val) {
  if (!panelB) {
    // PANEL A — filtro del pad / pasa-altos de la voz / volumen del pad
    switch (i) {
      case 0: cutoffBase = 200.0f + val * 8800.0f;  break;   // POT1 -> cutoff 200 Hz - 9 kHz
      case 1: qBase      = 0.7f  + val * 11.3f;     break;   // POT2 -> resonancia 0.7 - 12
      case 2: hpCutoff   = 20.0f * powf(100.0f, val); break; // POT3 -> HPF voz 20 Hz - 2 kHz (log)
      case 3: g_volume   = val * val;               break;   // POT4 -> volumen del pad
    }
  } else {
    // PANEL B — ducking / voz / arpegio
    switch (i) {
      case 0: duckDepth = val;                      break;   // POT1 -> profundidad del sidechain
      case 1: voiceVol  = val * 1.5f;               break;   // POT2 -> volumen de la voz
      case 2: { arpRate = 2.0f + val * 14.0f;                // POT3 -> 2-16 notas/s
                arpSamplesPerStep = (uint32_t)(SAMPLE_RATE / arpRate); } break;
      case 3: arpVol    = val;                      break;   // POT4 -> volumen del arpegio
    }
  }
}

void audioTask(void* arg) {
  static const uint8_t POT_PIN[4] = { POT1, POT2, POT3, POT4 };
  uint8_t potScan = 0;
  int16_t buffer[BUFFER_SAMPLES * 2];

  BtnState bBtn2 = {BTN2_PIN, HIGH, 0};
  BtnState bBtn3 = {BTN3_PIN, HIGH, 0};
  BtnState bBtn4 = {BTN4_PIN, HIGH, 0};

  startAuto();

  for (;;) {
    // ── Panel B mientras BTN5 este apretado ───────────────────────────────
    bool nowB = (digitalRead(BTN5_PIN) == LOW);
    if (nowB != panelB) { panelB = nowB; panelChanged = true; }

    // ── Botones de la musica (no hacen nada en Panel B: ahi BTN5 esta tomado) ──
    if (!panelB) {
      if (buttonPressed(bBtn2)) startAuto();                       // cancion nueva
      if (buttonPressed(bBtn3)) waveType = (waveType + 1) % 4;     // forma de onda
      if (buttonPressed(bBtn4)) arpType  = (arpType + 1) % ARP_NTYPES;
    } else {
      // mantener el flanco al dia para que no dispare al soltar BTN5
      bBtn2.last = digitalRead(BTN2_PIN);
      bBtn3.last = digitalRead(BTN3_PIN);
      bBtn4.last = digitalRead(BTN4_PIN);
    }

    // ── Pots: 1 por buffer en rotacion, congelados al cambiar de panel ────
    if (panelChanged) {
      // Captura las 4 anclas de golpe y congela todo: ningun parametro del panel nuevo
      // cambia hasta que MUEVAS su pot (si no, soltar BTN5 saltaria el cutoff).
      for (int i = 0; i < 4; i++) {
        potLive[i] = false; potMoveCnt[i] = 0; potAnchor[i] = readPot(POT_PIN[i]);
      }
      panelChanged = false;
    }

    int pi = potScan; potScan = (potScan + 1) & 3;
    float pv = readPot(POT_PIN[pi]);
    if (!potLive[pi]) {
      // Despierta solo si supera el umbral en 3 lecturas SEGUIDAS (un pico de ruido no
      // dura 3 lecturas -> el pot no se despierta solo).
      if (fabsf(pv - potAnchor[pi]) > POT_MOVE_THR) { if (++potMoveCnt[pi] >= 3) potLive[pi] = true; }
      else potMoveCnt[pi] = 0;
    }
    if (potLive[pi]) applyPot(pi, pv);

    updatePadFilter();
    updateVoiceHP();

    // ── Compuerta: la musica BAJA (no se corta) mientras BTN1 graba ───────
    float gateTarget = g_recording ? REC_DUCK : 1.0f;
    if (musicGate < gateTarget) { musicGate += GATE_STEP; if (musicGate > gateTarget) musicGate = gateTarget; }
    else if (musicGate > gateTarget) { musicGate -= GATE_STEP; if (musicGate < gateTarget) musicGate = gateTarget; }

    // ── La progresion avanza por tiempo (cada acorde dura sus negras) ─────
    autoSampleCount += BUFFER_SAMPLES;
    if (autoSampleCount >= autoChordSamples) {
      autoSampleCount = 0;
      autoIdx = (autoIdx + 1) % autoLen;
      triggerAutoChord(autoRoot[autoIdx], autoMin[autoIdx]);
    }

    // ── Snapshot de la voz para este buffer ───────────────────────────────
    bool  vActive = ttsActive;
    size_t vSamples = ttsBytes / 2;          // muestras mono s16le
    float musicPeak = 0.0f, voicePeak = 0.0f;

    // ── Render ────────────────────────────────────────────────────────────
    for (int n = 0; n < BUFFER_SAMPLES; n++) {
      // Arpegiador
      if (arpVol > 0.02f && arpCount > 0) {
        if (arpSampleCount >= arpSamplesPerStep) { arpSampleCount = 0; arpTrigger(); }
        arpSampleCount++;
      } else {
        arpSampleCount = 0;
      }

      // Dos buses: PAD (lleva g_volume) y ARPEGIO (lleva su arpVol baked-in)
      float padL = 0.0f, padR = 0.0f, arpL = 0.0f, arpR = 0.0f;
      for (int i = 0; i < NUM_VOICES; i++) {
        Voice &v = voices[i];
        if (!v.active) continue;

        float dt = v.freq / SAMPLE_RATE;
        v.phase += dt;
        if (v.phase >= 1.0f) v.phase -= 1.0f;

        float wave = osc(v.phase, dt);

        if (v.stage == 0) {                         // attack
          v.env += (v.kind == 1) ? arpAtkInc : attackInc;
          if (v.env >= 1.0f) { v.env = 1.0f; v.stage = (v.kind == 1) ? 3 : 1; }
        } else if (v.stage == 2) {                  // release del pad
          v.env *= releaseCoef;
          if (v.env < 0.0008f) { v.active = false; v.env = 0.0f; continue; }
        } else if (v.stage == 3) {                  // decay del arpegio
          v.env *= arpDecCoef;
          if (v.env < 0.0008f) { v.active = false; v.env = 0.0f; continue; }
        }                                           // stage 1 = sustain del pad

        float a = wave * v.env * v.gain;
        if (v.kind == 0) { padL += a * v.lGain; padR += a * v.rGain; }
        else             { arpL += a * v.lGain; arpR += a * v.rGain; }
      }

      float mixL = (padL * g_volume + arpL) * 0.13f;
      float mixR = (padR * g_volume + arpR) * 0.13f;
      // Anti-denormal: DC infimo (inaudible) para que el biquad nunca caiga en numeros
      // denormales, que en la FPU del ESP32 son lentisimos -> glitches.
      mixL += 1.0e-18f;
      mixR -= 1.0e-18f;
      float fL = applyBiquad(bqL, mixL, f_b0, f_b1, f_b2, f_a1, f_a2);
      float fR = applyBiquad(bqR, mixR, f_b0, f_b1, f_b2, f_a1, f_a2);

      // ── Canal de VOZ: resampleo lineal 24k -> 44.1k + pasa-altos ───────
      // Importante: la voz NO pasa por el filtro del pad. Si pasara, cerrar el cutoff
      // del pad tambien apagaria a GPT.
      float voiceOut = 0.0f, voiceRaw = 0.0f;
      if (vActive) {
        size_t i0 = (size_t)ttsPos;
        if (i0 + 1 < vSamples) {
          float frac = ttsPos - (float)i0;
          int16_t s0 = (int16_t)(ttsBuf[i0 * 2]       | (ttsBuf[i0 * 2 + 1] << 8));
          int16_t s1 = (int16_t)(ttsBuf[i0 * 2 + 2]   | (ttsBuf[i0 * 2 + 3] << 8));
          voiceRaw = ((float)s0 + ((float)s1 - (float)s0) * frac) * (1.0f / 32768.0f);
          ttsPos += TTS_STEP;
        } else {
          vActive = false;
          ttsActive = false;                 // el productor puede volver a llenar el buffer
          ttsPos = 0.0f;
          hpState = {0, 0, 0, 0};            // limpiar el filtro para la proxima frase
        }
        voiceOut = applyBiquad(hpState, voiceRaw + 1.0e-18f,
                               h_b0, h_b1, h_b2, h_a1, h_a2) * voiceVol;
      }

      // ── Ducking (sidechain): la envolvente de la voz CRUDA baja la musica ──
      // Se mide antes del pasa-altos: si no, subir el cutoff (que quita graves) haria
      // que el ducking se soltara justo cuando la voz esta mas delgada y necesita mas sitio.
      float rect = fabsf(voiceRaw);
      if (rect > duckEnv) duckEnv += (rect - duckEnv) * DUCK_ATT;
      else                duckEnv += (rect - duckEnv) * DUCK_REL;
      float d = duckEnv * 5.0f; if (d > 1.0f) d = 1.0f;
      float duck = 1.0f - duckDepth * d;

      float busGain = duck * musicGate;
      float vL = fL * busGain + voiceOut;
      float vR = fR * busGain + voiceOut;

      float ml = fabsf(fL * busGain); if (ml > musicPeak) musicPeak = ml;
      float vv = fabsf(voiceOut);     if (vv > voicePeak) voicePeak = vv;

      // Soft-clip de SEGURIDAD (tanh racional): solo actua en picos extremos.
      if (vL >  3.0f) vL =  3.0f; if (vL < -3.0f) vL = -3.0f;
      if (vR >  3.0f) vR =  3.0f; if (vR < -3.0f) vR = -3.0f;
      float shL = vL * (27.0f + vL * vL) / (27.0f + 9.0f * vL * vL);
      float shR = vR * (27.0f + vR * vR) / (27.0f + 9.0f * vR * vR);

      buffer[n * 2]     = (int16_t)(shL * 30000.0f);
      buffer[n * 2 + 1] = (int16_t)(shR * 30000.0f);
    }

    // VU suavizado para los LEDs
    g_musicLevel = g_musicLevel * 0.85f + musicPeak * 0.15f;
    g_voiceLevel = g_voiceLevel * 0.85f + voicePeak * 0.15f;

    size_t written;
    i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);
  }
}

// ==============================================================================================
// RED: Whisper / GPT / TTS   (core 0, junto al stack WiFi)
// ==============================================================================================

int recordAudio() {
  g_state = ST_RECORDING;

  int32_t raw[256];          // 128 frames estereo (L,R,L,R...)
  size_t bytesRead = 0;
  int n = 0;

  micHP = {0, 0, 0, 0};                         // estado limpio en cada grabacion

  // Descartar ~80 ms iniciales para evitar el "pop" de arranque del DMA
  for (int k = 0; k < 10; k++) {
    i2s_channel_read(rx_chan, raw, sizeof(raw), &bytesRead, 50);
  }

  while (digitalRead(BTN1_PIN) == LOW && n < MAX_SAMPLES) {
    if (i2s_channel_read(rx_chan, raw, sizeof(raw), &bytesRead, 200) != ESP_OK) continue;
    int got = bytesRead / sizeof(int32_t);      // nº de int32 leidos (frames*2)
    for (int i = 0; i < got && n < MAX_SAMPLES; i += 2) {   // i += 2 -> solo canal IZQ
      float x = (float)(raw[i] >> 16);          // 32 -> 16 bits
      // Pasa-altos 250 Hz: se lleva el grave del pad que entra por el parlante. Va ANTES
      // de la ganancia para que el pad no consuma headroom y sature la voz.
      float y = applyBiquad(micHP, x + 1.0e-18f, m_b0, m_b1, m_b2, m_a1, m_a2);
      int32_t s = (int32_t)(y * MIC_GAIN);
      if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
      audioBuffer[n++] = (int16_t)s;
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

  uint8_t wavHeader[44];
  uint32_t dataSize = sampleCount * 2;
  uint32_t fileSize = dataSize + 36;
  uint32_t byteRate = MIC_RATE * 2;

  wavHeader[0]='R'; wavHeader[1]='I'; wavHeader[2]='F'; wavHeader[3]='F';
  wavHeader[4]=fileSize&0xFF; wavHeader[5]=(fileSize>>8)&0xFF;
  wavHeader[6]=(fileSize>>16)&0xFF; wavHeader[7]=(fileSize>>24)&0xFF;
  wavHeader[8]='W'; wavHeader[9]='A'; wavHeader[10]='V'; wavHeader[11]='E';
  wavHeader[12]='f'; wavHeader[13]='m'; wavHeader[14]='t'; wavHeader[15]=' ';
  wavHeader[16]=16; wavHeader[17]=0; wavHeader[18]=0; wavHeader[19]=0;
  wavHeader[20]=1; wavHeader[21]=0;                       // PCM
  wavHeader[22]=1; wavHeader[23]=0;                       // 1 canal
  wavHeader[24]=MIC_RATE&0xFF; wavHeader[25]=(MIC_RATE>>8)&0xFF;
  wavHeader[26]=(MIC_RATE>>16)&0xFF; wavHeader[27]=(MIC_RATE>>24)&0xFF;
  wavHeader[28]=byteRate&0xFF; wavHeader[29]=(byteRate>>8)&0xFF;
  wavHeader[30]=(byteRate>>16)&0xFF; wavHeader[31]=(byteRate>>24)&0xFF;
  wavHeader[32]=2; wavHeader[33]=0;                       // block align
  wavHeader[34]=16; wavHeader[35]=0;                      // 16 bits
  wavHeader[36]='d'; wavHeader[37]='a'; wavHeader[38]='t'; wavHeader[39]='a';
  wavHeader[40]=dataSize&0xFF; wavHeader[41]=(dataSize>>8)&0xFF;
  wavHeader[42]=(dataSize>>16)&0xFF; wavHeader[43]=(dataSize>>24)&0xFF;

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
  client.write(wavHeader, 44);

  uint8_t* audioBytes = (uint8_t*)audioBuffer;
  size_t sent = 0;
  while (sent < dataSize) {
    size_t toSend = min((size_t)512, (size_t)(dataSize - sent));
    client.write(audioBytes + sent, toSend);
    sent += toSend;
    vTaskDelay(1);                     // cede CPU: la musica no se entera de la subida
  }

  client.print(model);
  client.print(language);
  client.print(tail);

  unsigned long timeout = millis() + 30000;
  while (!client.available() && millis() < timeout) vTaskDelay(pdMS_TO_TICKS(50));

  String response = "";
  bool bodyStarted = false;
  while (client.available() || client.connected()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (!bodyStarted) { if (line == "\r") bodyStarted = true; }
      else response += line;
    } else {
      vTaskDelay(pdMS_TO_TICKS(10));
      if (!client.available()) break;
    }
  }
  client.stop();

  int textStart = response.indexOf("\"text\":\"");
  if (textStart > 0) {
    textStart += 8;
    int textEnd = response.indexOf("\"", textStart);
    return response.substring(textStart, textEnd);
  }
  return "";
}

String askGPT(String question) {
  g_state = ST_PROCESSING;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, "https://api.openai.com/v1/chat/completions");
  http.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000);

  question.replace("\\", "\\\\");
  question.replace("\"", "\\\"");
  question.replace("\n", " ");
  question.replace("\r", "");

  String ctx = getCustomContext();
  ctx.replace("\\", "\\\\");
  ctx.replace("\"", "\\\"");
  ctx.replace("\n", "\\n");
  ctx.replace("\r", "");

  // Presentacion del asistente. EDITA esta linea junto con CONTEXT_PERSONALIZADO:
  // aqui va quien dice ser, arriba va lo que sabe.
  String sys = "Eres un PercuSynth, un asistente con inteligencia artificial. ";
  sys += "Usa la siguiente informacion para responder:\\n\\n";
  sys += ctx;
  sys += "\\n\\nResponde SIEMPRE en espanol latino. Tu respuesta se reproduce por voz en el PercuSynth, ";
  sys += "un dispositivo fisico basado en ESP32, asi que habla en prosa continua: nada de listas, vinetas, ";
  sys += "titulos, markdown ni emojis. AJUSTA EL LARGO A LA PREGUNTA: si es simple o directa, responde en 2-3 ";
  sys += "oraciones; si te piden explicar, detallar o contar sobre un taller o un producto, extiendete lo que ";
  sys += "haga falta (hasta unas 10-12 oraciones) sin rellenar ni repetir. Nunca cortes una idea a la mitad. ";
  sys += "Si te preguntan algo que no esta en el contexto, responde con tu conocimiento general.";

  String body = "{";
  body += "\"model\":\"gpt-4o-mini\",";
  body += "\"messages\":[";
  body += "{\"role\":\"system\",\"content\":\"" + sys + "\"},";
  body += "{\"role\":\"user\",\"content\":\"" + question + "\"}";
  // 200 tokens cortaban las respuestas a media frase. 500 dan ~370 palabras ≈ 2.5 min de
  // voz; el buffer de TTS (MAX_TTS_BYTES) aguanta 75 s, asi que el limite real es ese.
  body += "],\"max_tokens\":500,\"temperature\":0.7}";

  int httpCode = http.POST(body);
  String out = "";

  if (httpCode > 0) {
    String response = http.getString();
    int contentStart = response.indexOf("\"content\":");
    if (contentStart > 0) {
      contentStart = response.indexOf("\"", contentStart + 10);
      if (contentStart > 0) {
        contentStart += 1;
        for (int i = contentStart; i < (int)response.length() - 1; i++) {
          if (response.charAt(i) == '"' && response.charAt(i - 1) != '\\') {
            out = response.substring(contentStart, i);
            break;
          }
        }
        out.replace("\\n", " ");
        out.replace("\\r", "");
        out.replace("\\\"", "\"");
        out.replace("\\\\", "\\");
      }
    }
  }

  http.end();
  return out;
}

// Lee exactamente 'need' bytes del cliente (con timeout). Devuelve cuantos leyo.
static size_t readExact(WiFiClientSecure& client, uint8_t* dst, size_t need) {
  size_t got = 0;
  unsigned long t = millis();
  while (got < need) {
    int r = client.read(dst + got, need - got);
    if (r > 0) { got += r; t = millis(); }
    else {
      if (!client.connected() && client.available() == 0) break;
      if (millis() - t > 5000) break;
      vTaskDelay(1);
    }
  }
  return got;
}

// Descarga el TTS a PSRAM y se lo entrega al mezclador. NO reproduce: solo llena el buffer
// y levanta ttsActive; el que suena es siempre la tarea de audio.
bool speakText(String text) {
  // Esperar a que el mezclador termine la frase anterior antes de pisar el buffer
  while (ttsActive) vTaskDelay(pdMS_TO_TICKS(20));

  g_state = ST_SPEAKING;
  WiFi.setSleep(false);

  text.replace("\\", "\\\\");
  text.replace("\"", "\\\"");
  text.replace("\n", " ");
  text.replace("\r", "");

  // response_format=pcm -> 24 kHz, mono, 16-bit little-endian (crudo, sin cabecera)
  String body = "{\"model\":\"tts-1\",\"input\":\"" + text +
                "\",\"voice\":\"nova\",\"response_format\":\"pcm\",\"speed\":1.0}";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);
  if (!client.connect("api.openai.com", 443)) {
    WiFi.setSleep(true);
    return false;
  }

  client.print("POST /v1/audio/speech HTTP/1.1\r\n");
  client.print("Host: api.openai.com\r\n");
  client.print("Authorization: Bearer " + String(OPENAI_API_KEY) + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: " + String(body.length()) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  unsigned long to = millis() + 30000;
  while (!client.available() && millis() < to) vTaskDelay(pdMS_TO_TICKS(10));

  String status = client.readStringUntil('\n');
  if (status.indexOf("200") < 0) {           // error de API
    client.stop();
    WiFi.setSleep(true);
    return false;
  }

  bool chunked = false;
  while (true) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() <= 1) break;         // fin de cabeceras
    String low = line; low.toLowerCase();
    if (low.indexOf("transfer-encoding") >= 0 && low.indexOf("chunked") >= 0) chunked = true;
  }

  // OpenAI entrega el audio con Transfer-Encoding: chunked. Hay que quitar la cabecera de
  // tamano de cada chunk; si no, esos bytes ASCII se meten en el PCM y suenan como ruido
  // fuerte. (getStreamPtr NO lo hace por nosotros.)
  size_t pcmLen = 0;

  if (chunked) {
    while (true) {
      String sizeLine = client.readStringUntil('\n');       // "1a2b\r"
      sizeLine.trim();
      if (sizeLine.length() == 0) {
        if (!client.connected() && client.available() == 0) break;
        continue;
      }
      long chunkSize = strtol(sizeLine.c_str(), NULL, 16);
      if (chunkSize <= 0) break;                            // chunk final (0)

      size_t room   = MAX_TTS_BYTES - pcmLen;
      size_t toRead = min((size_t)chunkSize, room);
      pcmLen += readExact(client, ttsBuf + pcmLen, toRead);

      // si el chunk no cupo, descartar el resto para no desalinear el stream
      size_t remain = (size_t)chunkSize - toRead;
      uint8_t tmp[256];
      while (remain > 0) {
        size_t rr = readExact(client, tmp, min(remain, sizeof(tmp)));
        if (rr == 0) break;
        remain -= rr;
      }
      client.readStringUntil('\n');                         // consumir CRLF tras el chunk
    }
  } else {
    while (client.connected() || client.available()) {
      if (client.available()) {
        size_t room = MAX_TTS_BYTES - pcmLen;
        if (room == 0) break;
        int r = client.read(ttsBuf + pcmLen, room);
        if (r > 0) pcmLen += r;
      } else vTaskDelay(1);
    }
  }

  client.stop();
  WiFi.setSleep(true);

  if (pcmLen < 4000) return false;

  // Entregar al mezclador (orden importante: primero los datos, al final la bandera)
  ttsBytes  = pcmLen;
  ttsPos    = 0.0f;
  ttsActive = true;

  // Quedarse aqui hasta que termine de sonar, para que el LED de estado sea honesto
  while (ttsActive) vTaskDelay(pdMS_TO_TICKS(30));
  return true;
}

// ==============================================================================================
// TAREA DEL ASISTENTE  (core 0)
// ==============================================================================================

void assistantTask(void* arg) {
  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) { vTaskDelay(pdMS_TO_TICKS(500)); attempts++; }

  if (WiFi.status() != WL_CONNECTED) {
    // Sin red no hay asistente, pero la musica ya esta sonando: se avisa por LED y se
    // reintenta en segundo plano en vez de bloquear el equipo entero.
    g_state = ST_ERROR;
    for (;;) {
      vTaskDelay(pdMS_TO_TICKS(10000));
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      int a = 0;
      while (WiFi.status() != WL_CONNECTED && a < 20) { vTaskDelay(pdMS_TO_TICKS(500)); a++; }
      if (WiFi.status() == WL_CONNECTED) { g_state = ST_READY; break; }
    }
  }

  WiFi.setSleep(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  g_state = ST_READY;

  for (;;) {
    if (digitalRead(BTN1_PIN) == LOW) {
      vTaskDelay(pdMS_TO_TICKS(50));                   // antirrebote
      if (digitalRead(BTN1_PIN) == LOW) {
        g_recording = true;                            // la musica baja (no se corta)
        vTaskDelay(pdMS_TO_TICKS(140));                // dejar que el fade de 120 ms complete
        int sampleCount = recordAudio();
        g_recording = false;                           // la musica vuelve a nivel

        if (sampleCount > 1000) {
          String question = transcribeAudio(sampleCount);
          if (question.length() > 0) {
            String answer = askGPT(question);
            if (answer.length() > 0) speakText(answer);
          }
        }
        g_state = ST_READY;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ==============================================================================================
// SETUP / LOOP
// ==============================================================================================

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  esp_log_level_set("*", ESP_LOG_NONE);

  setCpuFrequencyMhz(240);

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);
  pinMode(BTN5_PIN, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(40);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  for (int i = 0; i < SEMI_LUT_N; i++)
    semiLUT[i] = powf(2.0f, (float)(i - SEMI_OFFSET) / 12.0f);
  for (int i = 0; i < 256; i++)
    sineLUT[i] = sinf(2.0f * (float)M_PI * (float)i / 256.0f);
  for (int i = 0; i < NUM_VOICES; i++)
    voices[i] = {false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  // -6.5 ≈ caida a -60 dB en el tiempo pedido -> el numero ES el largo real de la cola.
  releaseCoef = expf(-6.5f / (3.0f * SAMPLE_RATE));      // release de 3 s
  arpDecCoef  = expf(-6.5f / (arpGate * SAMPLE_RATE));
  arpSamplesPerStep = (uint32_t)(SAMPLE_RATE / arpRate);

  // Buffers en PSRAM: grabacion (~160 KB) + TTS (~1.2 MB). Se reservan UNA vez, para no
  // pedir 1.2 MB en caliente mientras el mezclador esta corriendo.
  audioBuffer = (int16_t*)ps_malloc(MAX_SAMPLES * sizeof(int16_t));
  ttsBuf      = (uint8_t*)ps_malloc(MAX_TTS_BYTES);
  if (!audioBuffer || !ttsBuf) {
    for (;;) {                                            // magenta parpadeante = sin PSRAM
      fill_solid(leds, NUM_LEDS, CRGB(110, 0, 110)); FastLED.show(); delay(400);
      fill_solid(leds, NUM_LEDS, CRGB::Black);       FastLED.show(); delay(400);
    }
  }

  i2s_dac_init();
  i2s_mic_init();
  updatePadFilter();
  updateVoiceHP();
  updateMicHP();

  // Audio en core 1 (APP_CPU) con prioridad alta; asistente en core 0, donde ya vive el
  // stack WiFi. Separarlos es lo que permite que TLS y GPT no corten la musica.
  xTaskCreatePinnedToCore(audioTask,     "audio", 8192,  NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(assistantTask, "asis",  16384, NULL,  3, NULL, 0);
}

// loop() se queda solo con los LEDs (~30 FPS). FastLED usa RMT, no pelea con el I2S.
void loop() {
  CRGB c;
  switch (g_state) {
    case ST_READY:      c = CRGB(0, 60, 0);    break;   // verde
    case ST_RECORDING:  c = CRGB(120, 0, 0);   break;   // rojo
    case ST_PROCESSING: c = CRGB(90, 55, 0);   break;   // ambar
    case ST_SPEAKING:   c = CRGB(0, 45, 80);   break;   // cian
    default:            c = CRGB(110, 0, 110); break;   // magenta
  }

  // LED 0 = estado (parpadea si es error). LEDs 1-5 = VU.
  if (g_state == ST_ERROR && ((millis() / 400) & 1)) leds[0] = CRGB::Black;
  else                                               leds[0] = c;

  bool speaking = (g_state == ST_SPEAKING);
  float lvl = speaking ? g_voiceLevel * 2.2f : g_musicLevel * 3.0f;
  if (lvl > 1.0f) lvl = 1.0f;
  int lit = (int)(lvl * 5.0f + 0.5f);
  for (int i = 1; i < NUM_LEDS; i++) {
    if (i <= lit) leds[i] = speaking ? CRGB(0, 60, 110) : CRGB(0, 40, 55);
    else          leds[i] = CRGB::Black;
  }

  FastLED.show();
  delay(33);
}
