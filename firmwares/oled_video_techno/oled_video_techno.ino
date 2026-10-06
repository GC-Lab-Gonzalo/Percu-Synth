// ==============================================================================================================================================
// PERCU-SYNTH — OLED VIDEO TECHNO (video vertical en la OLED + secuenciador techno con bombo) — GC Lab Chile
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
// - Pantalla OLED 0.96" SSD1306 128×64 I2C en el conector OLED de la V2.0 |SDA -> 21, SCL -> 38| (dir. 0x3C)
//   USADA DE LADO: 64 de ancho × 128 de alto, como un short
// - 5 Botones con pull-up |BTN1 -> 44, BTN2 -> 42, BTN3 -> 0, BTN4 -> 45, BTN5 -> 47|
// - 4 Potenciómetros analógicos |POT1 -> ADC1, POT2 -> ADC2, POT3 -> ADC8, POT4 -> ADC10|
// - Módulo microSD por SPI (el mismo cableado que grabador_campo) |SCK -> 14, MOSI -> 15, MISO -> 16, CS -> 17|
//   VCC a 5 V si el módulo trae regulador (el azul común), a 3.3 V si no. Tarjeta en FAT32.
// - LED SMD 0 de la placa |DATA -> 46| = estado de la grabación (los otros 5 quedan apagados)
// ==============================================================================================================================================
// ARDUINO IDE — settings críticos
// ==============================================================================================================================================
// - Board              : ESP32S3 Dev Module
// - USB CDC On Boot    : Enabled
// - Flash Mode         : DIO          (¡OPI rompe I2S!)
// - Partition Scheme   : la de siempre alcanza para el video de ejemplo (258 cuadros → el sketch pesa 646 KB).
//                        Para videos de más de ~550 cuadros: "Huge APP (3MB No OTA/1MB SPIFFS)" (1 KB por cuadro)
// - PSRAM              : OPI PSRAM  (el colchón de grabación de 8 s vive ahí; sin PSRAM baja a 1 s en RAM
//                                    interna, que alcanza con una tarjeta decente)
// ==============================================================================================================================================
// LIBRERÍAS REQUERIDAS
// ==============================================================================================================================================
// - ESP32 Arduino core ≥ 3.x (incluye driver/i2s_std.h, Wire, SPI y SD)
// - FastLED (sólo para el LED de estado de la grabación)
// - La SSD1306 se maneja directo por I2C (sin Adafruit ni U8g2), así cada cuadro es una copia de 1024
//   bytes y la pantalla nunca le cuesta tiempo al audio
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Un video vertical corre en la OLED puesta de lado, AMARRADO AL TEMPO, mientras la placa toca techno:
//
//   · BOMBO de tres bandas (el de trance_pistas / drum_poder): fundamental que aterriza en 46 Hz, 2.º
//     armónico para el pecho, mazo de ruido pasa-banda y click. Cuatro en el piso.
//   · LÍNEA ÁCIDA de 16 pasos: dos sierras PolyBLEP (±6 cents) → SVF pasa-bajos resonante con envolvente
//     de filtro, acentos y ligados (slide) a la 303. Las notas salen de La menor natural; BTN2 sortea
//     un patrón nuevo (el paso 1 siempre es la tónica).
//   · HATS de ruido pasa-banda: abierto en el contratiempo y cerrados suaves entre medio.
//   · El bombo «bombea» la línea (sidechain).
//
// El video NO corre a sus fps: avanza con el reloj del secuenciador. El loop entero dura VIDEO_PULSOS
// negras (lo calcula convertir_video.py, en compases enteros), así que al subir el tempo el baile se
// acelera, al hacer Stop se congela y cada Play lo arranca desde el primer cuadro junto con el paso 1.
// En cada bombo el CONTRASTE de la pantalla salta y vuelve a bajar: la imagen late con el pulso.
//
// El video se genera con convertir_video.py (en esta carpeta) → video.h. Ver README.md.
//
// GRABACIÓN (BTN5): se graba a la microSD EXACTAMENTE lo que sale por el DAC — el mismo buffer de
// 128 muestras que va a los audífonos, estéreo 16 bit 44.1 kHz — en TECHNO_0001.WAV, TECHNO_0002.WAV…
// (la numeración sigue lo que ya haya en la tarjeta). Sin micrófono. La tarjeta NUNCA toca el audio:
// la tarea de audio deja cada bloque en un colchón (8 s en PSRAM) y otra tarea en el core 0 lo vacía
// a la tarjeta en escrituras de ~16 KB, así un atasco de la tarjeta de 100–300 ms no se oye. La
// cabecera del WAV se reescribe cada 5 s: si se corta la luz a mitad de una toma, el archivo se abre.
//
// ARQUITECTURA: el audio corre en su propia tarea fijada al CORE 1. En el CORE 0 van tres tareas: la de
// control (botones, pots y LED a 1 kHz), la grabadora (SD) y la de pantalla (I2C, la de prioridad más
// baja). Ninguna toca el audio: dejan pedidos que el audio aplica al empezar cada paso. El audio publica
// dónde va el video (en cuadros) y cuántos bombos van; la pantalla sólo lee eso. Sin Serial.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
// - BTN1 → PLAY / STOP. Play arranca en el paso 1 y en el primer cuadro del video.
// - BTN2 → PATRÓN NUEVO de la línea ácida (entra en el paso siguiente)
// - BTN3 → TAP TEMPO (dos toques o más; 60–200 BPM)
// - BTN4 → BOMBO sí / no (para armar quiebres; el video sigue corriendo)
// - BTN5 → GRABAR / PARAR la grabación a la microSD (independiente de Play: puedes empezar a grabar
//          antes del Play para tener el arranque entero)
//
// LED 0 de la placa: apagado = sin grabar · ROJO = grabando · NARANJO = la tarjeta no alcanzó y se
// perdió algo de audio (cámbiala por una más rápida) · ROJO PARPADEANDO 3 s = no hay tarjeta o falló.
//
// - POT1 → CORTE del filtro (60 Hz – 7.7 kHz)
// - POT2 → RESONANCIA (atada al corte: con el filtro muy abierto se modera sola, nunca un pico en el agudo)
// - POT3 → ENVOLVENTE: cuánto abre el filtro cada nota (0 – 5 octavas)
// - POT4 → DECAY de la envolvente de filtro (40 ms – 1 s)
//
// Pantalla al revés: cambia GIRO_180 a true (gira la imagen por hardware, sin regenerar el video).
// Si la imagen sale con basura o se corta: baja I2C_HZ a 400000.
// ==============================================================================================================================================

#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s_std.h>
#include <math.h>
#ifndef SIMULADOR
  #include <SPI.h>
  #include <SD.h>
  #include <FastLED.h>
  #include <esp_heap_caps.h>
#endif
#include "video.h"

// ─── Tipos (arriba del todo para que el IDE de Arduino genere bien los prototipos) ───
struct BQ { float b0, b1, b2, a1, a2, z1, z2; };
struct VozBombo { float f, ph, env, eC, knock, kC, click, cC, amp; bool viva; BQ bp, hp; };

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128
const float SR     = (float)SAMPLE_RATE;
const float INV_SR = 1.0f / SR;
const float PI_F   = 3.14159265f;

// ─── OLED SSD1306 (mismo bus I2C que el MPU6050) ───────────
#define OLED_SDA   21
#define OLED_SCL   38
#define OLED_DIR   0x3C
const uint32_t I2C_HZ          = 800000;  // la SSD1306 aguanta 800 kHz–1 MHz; si hay basura en la imagen, 400000
const bool     GIRO_180        = false;   // true = pantalla dada vuelta (la placa girada para el otro lado)
const uint8_t  CONTRASTE_BASE  = 0x40;    // brillo entre bombos
const uint8_t  CONTRASTE_GOLPE = 0xFF;    // brillo justo en el bombo
const float    TAU_DESTELLO_MS = 70.0f;   // cuánto tarda en volver al brillo base

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
#define BTN1_PIN   44
#define BTN2_PIN   42
#define BTN3_PIN    0
#define BTN4_PIN   45
#define BTN5_PIN   47
const unsigned long DEBOUNCE_MS = 120;
const uint8_t BTN_PIN[5] = { BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN, BTN5_PIN };

// ─── Potenciómetros ────────────────────────────────────────
#define POT1   1    // ADC1
#define POT2   2    // ADC2
#define POT3   8    // ADC8
#define POT4  10    // ADC10
const uint8_t POT_PIN[4] = { POT1, POT2, POT3, POT4 };

// ==============================================================================================================================================
// MÚSICA (fija) — lo que no está en un pot se ajusta acá
// ==============================================================================================================================================
const float BPM_INICIAL = 126.0f;        // el mismo --bpm con que se corrió convertir_video.py
const float BPM_MIN = 60.0f, BPM_MAX = 200.0f;
#define NUM_PASOS 16
const int   NOTA_RAIZ = 33;               // La1 (55 Hz) — MIDI
const int8_t ESCALA[7]      = { 0, 2, 3, 5, 7, 8, 10 };   // La menor natural
const uint8_t PESO_GRADO[7] = { 7, 1, 3, 2, 3, 1, 2 };    // la tónica manda; la 2.ª y la b6 son color
const float DENSIDAD   = 0.62f;           // probabilidad de que un paso toque
const float P_OCTAVA   = 0.28f;
const float P_ACENTO   = 0.30f;
const float P_LIGADO   = 0.18f;
const char  HATS[NUM_PASOS + 1] = "-.O.-.O.-.O.-.O.";        // O abierto en el contratiempo · . cerrado suave

// Línea ácida
const float LINEA_NIVEL    = 0.42f;
const float LINEA_DESAF    = 1.0035f;     // ±6 cents entre las dos sierras
const float GATE           = 0.55f;       // fracción del paso que la nota queda apretada (sin ligado)
const float GLIDE_S        = 0.045f;      // ligado: tiempo del portamento
const float TAU_AMP        = 0.50f;       // la nota se apaga sola despacio mientras está apretada
const float TAU_SOLTAR     = 0.008f;      // al soltar el gate
const float ATAQUE_S       = 0.002f;
const float ACENTO_AMP     = 1.0f, NORMAL_AMP = 0.68f;
const float ACENTO_ENV     = 1.45f;       // el acento abre más el filtro
const float FILTRO_TECHO   = 12000.0f;
const float SIDECHAIN      = 0.45f;       // cuánto baja la línea con cada bombo

// Bombo (recetas de trance_pistas / drum_poder: tres bandas, saturación propia porque tiene UNA parcial)
const float NIVEL_BOMBO = 0.85f;
const float K_FREQ = 46.0f, K_RATIO = 4.8f, K_DROP = 0.030f, K_DEC = 0.30f, K_SAT = 1.70f;
const float K_KNOCK = 0.22f, K_KNOCK_F = 1200.0f, K_CLICK = 0.28f;
// Hats: ruido pasa-banda (nunca pasa-altos pelado) + banda que los deja fuera del grave
const float NIVEL_HAT = 0.30f;

const float MASTER = 0.80f;
const float TECHO  = 0.89f;               // −1 dBFS: techo del limitador

// ==============================================================================================================================================
// PEDIDOS ENTRE NÚCLEOS (escrituras de 32 bits alineadas: atómicas en el S3)
// ==============================================================================================================================================
volatile bool     tocando     = false;
volatile bool     bomboActivo = true;
volatile float    reqBPM      = BPM_INICIAL;
volatile uint32_t reqPatron   = 0;        // sube cada vez que se pide un patrón nuevo
volatile float    pCorte = 0.35f, pReso = 0.55f, pEnv = 0.55f, pDecay = 0.35f;   // 0..1
// audio → pantalla
volatile float    posVideo     = 0.0f;    // en cuadros
volatile uint32_t golpesBombo  = 0;

// ==============================================================================================================================================
// GRABACIÓN A microSD (BTN5) — lo mismo que sale por el DAC, sin micrófono
// ==============================================================================================================================================
#define SD_SCK    14
#define SD_MOSI   15
#define SD_MISO   16
#define SD_CS     17
const uint32_t SD_FREQ_HZ = 20000000;               // 20 MHz: lo aguanta cualquier módulo
#define BYTES_BLOQUE   (BUFFER_SAMPLES * 2 * 2)     // estéreo 16 bit = 512 bytes por bloque de audio
#define RING_SEG_PSRAM   8                          // colchón con PSRAM (1.4 MB)
#define RING_SEG_INTERNA 1                          // sin PSRAM (176 KB de RAM interna)
#define BLOQUES_POR_ESCRITURA 32                    // 16 KB por escritura a la tarjeta
const uint32_t LIMITE_DATOS = 3900000000u;          // FAT32 no pasa de 4 GB por archivo (~6 h): ahí se corta sola
const unsigned long CABECERA_CADA_MS = 5000;        // cada cuánto se reescribe el tamaño del WAV

// control → grabadora : pedidoGrabar
// grabadora → audio   : audioEmpuja (el audio llena el colchón sólo mientras esto está en true)
// audio → grabadora   : bloquesEsc (lo escribe SÓLO el audio) · grabadora → audio: bloquesLeidos
volatile bool     pedidoGrabar   = false;
volatile bool     audioEmpuja    = false;
volatile uint32_t bloquesEsc     = 0;
volatile uint32_t bloquesLeidos  = 0;
volatile uint32_t bloquesPerdidos = 0;              // bloques que no cupieron (la tarjeta no alcanzó)
volatile uint32_t contadorBloquesAudio = 0;
enum { GR_REPOSO, GR_GRABANDO, GR_PARANDO, GR_CERRANDO };
volatile int      estadoGrab = GR_REPOSO;
volatile bool     sdLista = false;
volatile unsigned long sdErrorMs = 0;               // instante del último error (el LED parpadea 3 s)
uint8_t  *ring = nullptr;
uint32_t  ringBloques = 0;

// ==============================================================================================================================================
// UTILIDADES DSP
// ==============================================================================================================================================
#define TABLA 1024
float SENO[TABLA + 1];
static inline float seno(float fase) { float x = fase * TABLA; int i = (int)x; float f = x - i; return SENO[i] + (SENO[i + 1] - SENO[i]) * f; }
uint32_t semilla = 22222;
static inline float ruido(uint32_t &s) { s = s * 1664525u + 1013904223u; return (int32_t)s * (1.0f / 2147483648.0f); }
static inline float tanRapido(float x) { float x2 = x * x; return x * (15.0f - x2) / (15.0f - 6.0f * x2); }   // Padé
static inline float exp2Rapido(float x) {        // 2^x, x ≥ 0, error < 0.2 %
  int e = (int)x; float f = x - e;
  float p = 1.0f + f * (0.6951786f + f * (0.2261487f + f * 0.0782451f));
  union { float f; int32_t i; } u; u.f = p; u.i += e << 23; return u.f;
}
static inline float decaimiento(float tau) { return expf(-1.0f / (tau * SR)); }
static inline float softClip(float x) {
  if (x > 3.0f) return 1.0f; if (x < -3.0f) return -1.0f;
  float x2 = x * x; return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
static inline float techoSuave(float x) {
  float a = fabsf(x); if (a <= 0.95f) return x;
  float y = 0.95f + 0.05f * tanhf((a - 0.95f) / 0.05f); return x < 0 ? -y : y;
}
static inline float hz(float midi) { return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f); }
static inline float pasoBQ(BQ &q, float x) { float y = q.b0 * x + q.z1; q.z1 = q.b1 * x - q.a1 * y + q.z2; q.z2 = q.b2 * x - q.a2 * y; return y; }
void bqBPF(BQ &c, float fc, float Q) {
  float w = 2.0f * PI_F * fc / SR, s = sinf(w), co = cosf(w), al = s / (2.0f * Q), a0 = 1.0f + al;
  c.b0 = al / a0; c.b1 = 0.0f; c.b2 = -al / a0; c.a1 = -2.0f * co / a0; c.a2 = (1.0f - al) / a0; c.z1 = c.z2 = 0;
}
void bqHPF(BQ &c, float fc, float Q) {
  float w = 2.0f * PI_F * fc / SR, s = sinf(w), co = cosf(w), al = s / (2.0f * Q), a0 = 1.0f + al;
  c.b0 = (1.0f + co) * 0.5f / a0; c.b1 = -(1.0f + co) / a0; c.b2 = c.b0; c.a1 = -2.0f * co / a0; c.a2 = (1.0f - al) / a0; c.z1 = c.z2 = 0;
}
void bqLPF(BQ &c, float fc, float Q) {
  float w = 2.0f * PI_F * fc / SR, s = sinf(w), co = cosf(w), al = s / (2.0f * Q), a0 = 1.0f + al;
  c.b0 = (1.0f - co) * 0.5f / a0; c.b1 = (1.0f - co) / a0; c.b2 = c.b0; c.a1 = -2.0f * co / a0; c.a2 = (1.0f - al) / a0; c.z1 = c.z2 = 0;
}
static inline float sierra(float &f, float inc, float inv) {   // PolyBLEP; inv = 1/inc
  float y = 2.0f * f - 1.0f;
  if (f < inc) { float t = f * inv; y -= t + t - t * t - 1.0f; }
  else if (f > 1.0f - inc) { float t = (f - 1.0f) * inv; y -= t * t + t + t + 1.0f; }
  f += inc; if (f >= 1.0f) f -= 1.0f;
  return y;
}

// ==============================================================================================================================================
// ESTADO DE LA TAREA DE AUDIO
// ==============================================================================================================================================
static i2s_chan_handle_t tx_chan;

// Patrón de la línea
int8_t  patNota[NUM_PASOS];               // semitonos sobre la raíz, −1 = silencio
bool    patAcento[NUM_PASOS], patLigado[NUM_PASOS];
uint32_t patronAplicado = 0;

// Secuenciador
bool     aTocando = false;
int      paso = 0;                        // próximo paso a disparar
uint32_t restantes = 0, largoPaso = 1;    // muestras que faltan para el próximo paso / largo del actual
uint32_t pasosVideo = 0;                  // pasos desde el Play, módulo el loop del video
float    bpmActual = BPM_INICIAL;

// Línea ácida
float f1 = 0.0f, f2 = 0.37f;
float frec = 55.0f, frecObj = 55.0f, glideCoef = 0.0f;
float amp = 0.0f, ampObj = 0.0f;          // envolvente de amplitud
bool  atacando = false, gateOn = false;
uint32_t gateRest = 0;
float envF = 0.0f, envFDec = 0.999f, envFAmt = 1.0f;
float mulAmp, mulSoltar, incAtaque;
struct { float ic1, ic2, a1, a2, a3, comp; } svf = { 0, 0, 1, 0, 0, 1 };
float corteS = 0.35f, resoS = 0.55f, envS = 0.55f;

// Batería
VozBombo bombos[2]; int sigBombo = 0;
struct { float env, eC, amp; BQ bp, banda; } hat;
float fCoefBombo, dRapido3;
float scObj = 0.0f, sc = 0.0f, dSc;

// Master
#define LIM_LOOK 64
float limDly[LIM_LOOK]; int limIdx = 0;
float limEnv = 0.0f, limRel, limGain = 1.0f;
float dcX1 = 0.0f, dcY1 = 0.0f;
BQ    techo13k;

// ─── patrón aleatorio en La menor ──────────────────────────
uint32_t rnd = 12345;
static inline float azar() { rnd = rnd * 1664525u + 1013904223u; return (rnd >> 8) * (1.0f / 16777216.0f); }

void nuevoPatron(uint32_t entropia) {
  rnd ^= entropia * 2654435761u;
  int suma = 0; for (int g = 0; g < 7; g++) suma += PESO_GRADO[g];
  for (int s = 0; s < NUM_PASOS; s++) {
    bool toca = (s == 0) || azar() < DENSIDAD;
    if (!toca) { patNota[s] = -1; patAcento[s] = false; patLigado[s] = false; continue; }
    int g = 0;
    if (s != 0) {                                     // el paso 1 es siempre la tónica: el ancla del patrón
      float r = azar() * suma;
      while (g < 6 && r >= PESO_GRADO[g]) { r -= PESO_GRADO[g]; g++; }
    }
    patNota[s]   = ESCALA[g] + (azar() < P_OCTAVA ? 12 : 0);
    patAcento[s] = (s % 4 == 0) ? azar() < 0.5f : azar() < P_ACENTO;
    patLigado[s] = azar() < P_LIGADO;
  }
  for (int s = 0; s < NUM_PASOS; s++)                 // un ligado sólo tiene sentido si el paso que sigue toca
    if (patLigado[s] && patNota[(s + 1) % NUM_PASOS] < 0) patLigado[s] = false;
}

// ─── disparos ──────────────────────────────────────────────
void golpeBombo() {
  VozBombo &a = bombos[sigBombo]; if (a.viva) { a.eC = a.kC = a.cC = dRapido3; }   // el parche anterior se calla en 3 ms
  sigBombo ^= 1; VozBombo &b = bombos[sigBombo];
  b.f = K_FREQ * K_RATIO; b.ph = 0.0f; b.env = 1.0f; b.knock = 1.0f; b.click = 1.0f; b.amp = 1.0f; b.viva = true;
  b.eC = decaimiento(K_DEC); b.kC = decaimiento(0.014f); b.cC = decaimiento(0.005f);
  scObj = 1.0f;
  golpesBombo++;
}
void golpeHat(bool abierto) {                         // abierto y cerrado comparten parche (el choke)
  hat.env = 1.0f; hat.eC = decaimiento(abierto ? 0.060f : 0.016f); hat.amp = abierto ? 1.0f : 0.40f;
}
void notaLinea(int s) {
  int prev = (s + NUM_PASOS - 1) % NUM_PASOS;
  bool ligadoDesdeAntes = patLigado[prev] && patNota[prev] >= 0 && gateOn;
  frecObj = hz((float)(NOTA_RAIZ + patNota[s]));
  if (ligadoDesdeAntes) {
    glideCoef = 1.0f - decaimiento(GLIDE_S / 3.0f);   // se desliza sin re-atacar: el 303
  } else {
    frec = frecObj; glideCoef = 0.0f;
    atacando = true;                                  // sube desde donde esté: sin clic, sin reiniciar la fase
    envF = 1.0f;
    envFAmt = patAcento[s] ? ACENTO_ENV : 1.0f;
  }
  ampObj = patAcento[s] ? ACENTO_AMP : NORMAL_AMP;
  gateOn = true;
  gateRest = patLigado[s] ? 0xFFFFFFFFu : (uint32_t)(GATE * largoPaso);
}

void dispararPaso(int s) {
  if (bomboActivo && (s & 3) == 0) golpeBombo();
  if (HATS[s] == 'O') golpeHat(true); else if (HATS[s] == '.') golpeHat(false);
  if (patNota[s] >= 0) notaLinea(s);
  else gateOn = false;                                // silencio: se suelta la nota anterior
}

void prepararSintesis() {
  for (int i = 0; i <= TABLA; i++) SENO[i] = sinf(2.0f * PI_F * i / TABLA);
  fCoefBombo = 1.0f - expf(-1.0f / (K_DROP * SR));
  dRapido3 = decaimiento(0.003f);
  dSc = decaimiento(0.14f);
  mulAmp = decaimiento(TAU_AMP); mulSoltar = decaimiento(TAU_SOLTAR);
  incAtaque = 1.0f / (ATAQUE_S * SR);
  for (int i = 0; i < 2; i++) { bqBPF(bombos[i].bp, K_KNOCK_F, 0.8f); bqHPF(bombos[i].hp, 3000.0f, 0.7f); bombos[i].viva = false; }
  bqBPF(hat.bp, 8000.0f, 0.55f); bqHPF(hat.banda, 2500.0f, 0.707f); hat.env = 0.0f;
  for (int i = 0; i < LIM_LOOK; i++) limDly[i] = 0.0f;
  limRel = decaimiento(0.08f);
  bqLPF(techo13k, 13000.0f, 0.7071f);                 // DESPUÉS del limitador
}

// ─── coeficientes del filtro de la línea (cada 16 muestras, sin libm) ──
void calcularFiltro() {
  float base = 60.0f * exp2Rapido(corteS * 7.0f);     // 60 Hz … 7.7 kHz
  float oct  = envS * 5.0f * envF * envFAmt;
  float fc   = base * exp2Rapido(oct);
  if (fc > FILTRO_TECHO) fc = FILTRO_TECHO; if (fc < 25.0f) fc = 25.0f;
  // resonancia atada al corte: arriba de ~5 kHz se modera sola (nada de pico en el agudo)
  float tope = 1.15f - fc / 9000.0f; tope = tope < 0.15f ? 0.15f : tope > 1.0f ? 1.0f : tope;
  float q = 0.707f + resoS * 8.0f * tope;
  float g = tanRapido(PI_F * fc * INV_SR), k = 1.0f / q;
  svf.a1 = 1.0f / (1.0f + g * (g + k)); svf.a2 = g * svf.a1; svf.a3 = g * svf.a2;
  svf.comp = 1.0f / (1.0f + 0.18f * (q - 0.707f));
}

// ==============================================================================================================================================
// TAREA DE AUDIO — un buffer de 128 muestras (core 1)
// ==============================================================================================================================================
void renderBuffer() {
  // ── borde del buffer: pedidos de la tarea de control ──
  bool quiere = tocando;
  if (quiere && !aTocando) { paso = 0; restantes = 0; pasosVideo = 0; }   // Play: paso 1 en la primera muestra
  if (!quiere && aTocando) { gateOn = false; }                            // Stop: la nota se suelta, las colas suenan
  aTocando = quiere;
  if (reqPatron != patronAplicado) { patronAplicado = reqPatron; nuevoPatron(patronAplicado); }
  float decT = 0.04f * powf(25.0f, pDecay);           // 40 ms … 1 s
  envFDec = decaimiento(decT);
  const float cT = pCorte, rT = pReso, eT = pEnv;

  int16_t buffer[BUFFER_SAMPLES * 2];
  uint32_t sem = semilla;
  for (int i = 0; i < BUFFER_SAMPLES; i++) {
    // ── secuenciador (preciso a la muestra; el largo del paso se fija al EMPEZAR el paso) ──
    if (aTocando) {
      if (restantes == 0) {
        bpmActual = reqBPM;
        largoPaso = (uint32_t)(SR * 60.0f / (bpmActual * 4.0f));
        dispararPaso(paso);
        paso = (paso + 1) % NUM_PASOS;
        restantes = largoPaso;
        pasosVideo++;
      }
      restantes--;
      if (gateOn && gateRest != 0xFFFFFFFFu) { if (gateRest == 0) gateOn = false; else gateRest--; }
    }

    // ── parámetros suavizados (~12 ms) y filtro cada 16 muestras ──
    corteS += (cT - corteS) * 0.0019f; resoS += (rT - resoS) * 0.0019f; envS += (eT - envS) * 0.0019f;
    if ((i & 15) == 0) calcularFiltro();

    // ── línea ácida ──
    frec += (frecObj - frec) * glideCoef;
    if (atacando) { amp += incAtaque * ampObj; if (amp >= ampObj) { amp = ampObj; atacando = false; } }
    else amp *= gateOn ? mulAmp : mulSoltar;
    envF *= envFDec;
    float inc1 = frec * INV_SR, inc2 = inc1 * LINEA_DESAF;
    float x = (sierra(f1, inc1, 1.0f / inc1) + sierra(f2, inc2, 1.0f / inc2)) * 0.5f + 1.0e-18f;
    float v3 = x - svf.ic2, v1 = svf.a1 * svf.ic1 + svf.a2 * v3, v2 = svf.ic2 + svf.a2 * svf.ic1 + svf.a3 * v3;
    svf.ic1 = 2.0f * v1 - svf.ic1; svf.ic2 = 2.0f * v2 - svf.ic2;

    // sidechain: baja en ~2 ms al llegar el bombo y vuelve en ~140 ms
    scObj *= dSc; sc += (scObj - sc) * 0.012f;
    float linea = v2 * svf.comp * amp * LINEA_NIVEL * (1.0f - SIDECHAIN * sc);

    // ── bombo ──
    float bat = 0.0f;
    for (int k = 0; k < 2; k++) {
      VozBombo &v = bombos[k]; if (!v.viva) continue;
      v.f += (K_FREQ - v.f) * fCoefBombo;
      v.ph += v.f * INV_SR; if (v.ph >= 1.0f) v.ph -= 1.0f;
      float p2 = v.ph + v.ph; if (p2 >= 1.0f) p2 -= 1.0f;
      float y = softClip((seno(v.ph) + 0.10f * seno(p2)) * K_SAT) * 0.9f * v.env;
      if (v.knock > 1e-3f) y += pasoBQ(v.bp, ruido(sem)) * v.knock * (K_KNOCK * 1.6f);   // mazo: RUIDO, nunca un seno
      if (v.click > 1e-3f) y += pasoBQ(v.hp, ruido(sem)) * v.click * (K_CLICK * 0.45f);
      bat += y * v.amp * NIVEL_BOMBO;
      v.env *= v.eC; v.knock *= v.kC; v.click *= v.cC;
      if (v.env < 3e-4f && v.knock < 1e-3f) v.viva = false;   // −70 dB: el escalón ya es inaudible
    }
    // ── hats ──
    if (hat.env > 1e-4f) {
      bat += pasoBQ(hat.banda, pasoBQ(hat.bp, ruido(sem)) * hat.env) * hat.amp * NIVEL_HAT;
      hat.env *= hat.eC;
    }

    // ── master: DC → limitador con lookahead → 13 kHz → techo suave ──
    float m = (linea + bat) * MASTER;
    float dc = m - dcX1 + 0.9985f * dcY1; dcX1 = m; dcY1 = dc;
    float pk = fabsf(dc);
    if (pk > limEnv) limEnv = pk; else limEnv = pk + (limEnv - pk) * limRel;
    float limObj = (limEnv > TECHO) ? (TECHO / limEnv) : 1.0f;
    limGain += (limObj - limGain) * 0.020f;           // ganancia suavizada (~0.5 ms): sin escalones
    float d = limDly[limIdx]; limDly[limIdx] = dc; if (++limIdx >= LIM_LOOK) limIdx = 0;
    float out = techoSuave(pasoBQ(techo13k, d * limGain));
    int32_t s = (int32_t)(out * 32000.0f);
    if (s > 32767) s = 32767; if (s < -32768) s = -32768;
    buffer[i * 2] = (int16_t)s; buffer[i * 2 + 1] = (int16_t)s;
  }
  semilla = sem;

  // ── dónde va el video: pasos enteros + fracción del paso actual, en cuadros ──
  if (aTocando) {
    const uint32_t pasosLoop = (uint32_t)VIDEO_PULSOS * 4u;
    pasosVideo %= pasosLoop;
    float fr = 1.0f - (float)restantes / (float)largoPaso;     // cuánto va del paso en curso
    float pasosHechos = (float)pasosVideo - 1.0f + fr;          // pasosVideo ya contó el paso en curso
    if (pasosHechos < 0.0f) pasosHechos += (float)pasosLoop;
    posVideo = pasosHechos * ((float)VIDEO_CUADROS / (float)pasosLoop);
  }

  // ── grabación: el MISMO bloque que va al DAC, al colchón (la tarjeta la escribe otra tarea) ──
  if (audioEmpuja && ring) {
    uint32_t esc = bloquesEsc;
    if (esc - bloquesLeidos < ringBloques) {
      memcpy(ring + (size_t)(esc % ringBloques) * BYTES_BLOQUE, buffer, BYTES_BLOQUE);
      __sync_synchronize();                           // los datos antes que el contador
      bloquesEsc = esc + 1;
    } else {
      bloquesPerdidos = bloquesPerdidos + 1;          // colchón lleno: la tarjeta no alcanzó
    }
  }
  contadorBloquesAudio = contadorBloquesAudio + 1;

  size_t escritos;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &escritos, portMAX_DELAY);
}

// ==============================================================================================================================================
// PANTALLA (core 0, prioridad baja)
// ==============================================================================================================================================
void oledComandos(const uint8_t *c, int n) {
  Wire.beginTransmission(OLED_DIR);
  Wire.write((uint8_t)0x00);                          // Co = 0, D/C = 0: lo que sigue son comandos
  Wire.write(c, n);
  Wire.endTransmission();
}

void oledIniciar() {
  Wire.begin(OLED_SDA, OLED_SCL, I2C_HZ);
  const uint8_t init[] = {
    0xAE,                     // apagada mientras se configura
    0xD5, 0x80,               // reloj interno
    0xA8, 0x3F,               // multiplex 64
    0xD3, 0x00,               // sin desplazamiento
    0x40,                     // línea de inicio 0
    0x8D, 0x14,               // bomba de carga interna
    0x20, 0x00,               // direccionamiento HORIZONTAL: 1024 bytes de corrido llenan la pantalla
    (uint8_t)(GIRO_180 ? 0xA0 : 0xA1),   // columnas normales / espejadas
    (uint8_t)(GIRO_180 ? 0xC0 : 0xC8),   // filas normales / espejadas  → las dos juntas = 180°
    0xDA, 0x12,               // pines COM
    0x81, CONTRASTE_BASE,
    0xD9, 0xF1,               // precarga
    0xDB, 0x40,               // VCOMH
    0xA4,                     // muestra la RAM
    0xA6,                     // sin invertir
    0x2E,                     // sin scroll
    0xAF                      // encendida
  };
  oledComandos(init, sizeof(init));
}

void oledCuadro(const uint8_t *datos) {
  const uint8_t ventana[] = { 0x21, 0, 127, 0x22, 0, 7 };    // toda la pantalla
  oledComandos(ventana, sizeof(ventana));
  for (int i = 0; i < 1024; i += 64) {                       // el buffer de Wire es de 128: trozos de 64
    Wire.beginTransmission(OLED_DIR);
    Wire.write((uint8_t)0x40);                               // D/C = 1: datos
    Wire.write(datos + i, 64);
    Wire.endTransmission();
  }
}

void tareaPantalla(void *) {
  oledIniciar();
  int ultimo = -1;
  uint32_t golpesVistos = golpesBombo;
  float destello = 0.0f;
  uint8_t contrasteEnviado = CONTRASTE_BASE;
  unsigned long tPrev = millis();
  const float rangoContraste = (float)(CONTRASTE_GOLPE - CONTRASTE_BASE);
  for (;;) {
    unsigned long t = millis();
    float dt = (float)(t - tPrev); tPrev = t;

    // destello de contraste en cada bombo
    uint32_t g = golpesBombo;
    if (g != golpesVistos) { golpesVistos = g; destello = 1.0f; }
    else destello *= expf(-dt / TAU_DESTELLO_MS);
    uint8_t c = (uint8_t)(CONTRASTE_BASE + rangoContraste * destello);
    if (c != contrasteEnviado) {
      const uint8_t cmd[] = { 0x81, c };
      oledComandos(cmd, 2);
      contrasteEnviado = c;
    }

    // cuadro del video (se saltan los que no alcancen a dibujarse: el tiempo lo manda el audio)
    int idx = (int)posVideo;
    if (idx < 0) idx = 0; if (idx >= VIDEO_CUADROS) idx = VIDEO_CUADROS - 1;
    if (idx != ultimo) { oledCuadro(VIDEO[idx]); ultimo = idx; }
    vTaskDelay(1);
  }
}

// ==============================================================================================================================================
// GRABADORA (core 0) — vacía el colchón a la tarjeta; el audio nunca espera a la tarjeta
// ==============================================================================================================================================
#ifndef SIMULADOR
SPIClass spiSD(FSPI);
File     archivo;
int      siguienteToma = 1;
uint32_t bytesDatos = 0;
unsigned long ultimaCabecera = 0;
uint32_t marcaParada = 0;
uint32_t perdidosAlAbrir = 0;
volatile bool hayPerdidas = false;                  // en esta toma se perdió audio (LED naranjo)

void cabeceraWav(uint8_t *h, uint32_t datos) {      // PCM estéreo 16 bit 44.1 kHz
  const uint16_t canales = 2, bits = 16, blockAlign = canales * bits / 8;
  const uint32_t sr = SAMPLE_RATE, byteRate = sr * blockAlign;
  uint32_t v; uint16_t s;
  memcpy(h, "RIFF", 4);  v = 36 + datos; memcpy(h + 4, &v, 4);
  memcpy(h + 8, "WAVEfmt ", 8);
  v = 16; memcpy(h + 16, &v, 4);
  s = 1;  memcpy(h + 20, &s, 2);                    // PCM
  memcpy(h + 22, &canales, 2);
  memcpy(h + 24, &sr, 4);
  memcpy(h + 28, &byteRate, 4);
  memcpy(h + 32, &blockAlign, 2);
  memcpy(h + 34, &bits, 2);
  memcpy(h + 36, "data", 4);
  memcpy(h + 40, &datos, 4);
}

// Reescribe el tamaño y vuelve al final: una toma cortada a la mitad se abre igual
bool actualizarCabecera() {
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
    if (sscanf(f.name(), "TECHNO_%d", &num) == 1 && num >= siguienteToma) siguienteToma = num + 1;
    f.close();
  }
  raiz.close();
  return true;
}

bool abrirToma() {
  char nombre[32];
  snprintf(nombre, sizeof(nombre), "/TECHNO_%04d.WAV", siguienteToma);
  archivo = SD.open(nombre, FILE_WRITE);
  if (!archivo) return false;
  uint8_t h[44];
  cabeceraWav(h, 0);
  if (archivo.write(h, 44) != 44) { archivo.close(); return false; }
  bytesDatos = 0;
  siguienteToma++;
  return true;
}

void cerrarToma() {
  actualizarCabecera();
  archivo.close();
}

// Vacía hasta BLOQUES_POR_ESCRITURA bloques. Devuelve cuántos escribió (−1 = error de tarjeta).
int vaciarColchon() {
  uint32_t disp = bloquesEsc - bloquesLeidos;
  if (disp == 0) return 0;
  uint32_t pos = bloquesLeidos % ringBloques;
  uint32_t n = disp;
  if (n > BLOQUES_POR_ESCRITURA) n = BLOQUES_POR_ESCRITURA;
  if (n > ringBloques - pos) n = ringBloques - pos;   // una escritura no cruza el final del anillo
  __sync_synchronize();
  size_t b = (size_t)n * BYTES_BLOQUE;
  if (archivo.write(ring + (size_t)pos * BYTES_BLOQUE, b) != b) return -1;
  bytesDatos += b;
  bloquesLeidos = bloquesLeidos + n;
  return (int)n;
}

// Un paso de la grabadora. Devuelve true si hizo trabajo (para no dormir mientras haya cola).
bool pasoGrabadora() {
  if (pedidoGrabar) {
    pedidoGrabar = false;
    if (estadoGrab == GR_REPOSO) {
      if (!sdLista) sdLista = iniciarSD();           // se puede meter la tarjeta después de encender
      if (sdLista && ring && abrirToma()) {
        bloquesLeidos   = bloquesEsc;                // el audio no está empujando: se parte de cero
        perdidosAlAbrir = bloquesPerdidos;
        hayPerdidas     = false;
        ultimaCabecera  = millis();
        estadoGrab  = GR_GRABANDO;
        audioEmpuja = true;
      } else {
        sdLista = false;                             // la próxima vez se vuelve a montar
        sdErrorMs = millis() | 1;
      }
    } else if (estadoGrab == GR_GRABANDO) {
      audioEmpuja = false;
      marcaParada = contadorBloquesAudio;
      estadoGrab  = GR_PARANDO;
    }
  }
  if (estadoGrab == GR_REPOSO) return false;

  int n = vaciarColchon();
  if (n < 0) {                                       // tarjeta llena o sacada: se cierra lo que haya
    audioEmpuja = false;
    cerrarToma();
    sdLista = false;
    sdErrorMs = millis() | 1;
    estadoGrab = GR_REPOSO;
    return false;
  }
  if (bloquesPerdidos != perdidosAlAbrir) hayPerdidas = true;

  if (estadoGrab == GR_GRABANDO) {
    if (bytesDatos > LIMITE_DATOS) {                 // tope de FAT32: se cierra sola
      audioEmpuja = false;
      marcaParada = contadorBloquesAudio;
      estadoGrab  = GR_PARANDO;
    } else if (millis() - ultimaCabecera >= CABECERA_CADA_MS) {
      ultimaCabecera = millis();
      actualizarCabecera();
    }
  }
  // Dos bloques de audio después de bajar audioEmpuja, ya nadie escribe en el colchón
  if (estadoGrab == GR_PARANDO && contadorBloquesAudio - marcaParada >= 2) estadoGrab = GR_CERRANDO;
  if (estadoGrab == GR_CERRANDO && bloquesEsc == bloquesLeidos) {
    cerrarToma();
    estadoGrab = GR_REPOSO;
  }
  return n > 0;
}

void tareaGrabadora(void *) {
  sdLista = iniciarSD();
  for (;;) { if (!pasoGrabadora()) vTaskDelay(2); }
}

// ─── LED 0 de la placa = estado de la grabación ────────────
#define LED_PIN   46
#define NUM_LEDS   6
CRGB leds[NUM_LEDS];

void actualizarLED() {
  static unsigned long ultimo = 0;
  unsigned long t = millis();
  if (t - ultimo < 30) return;
  ultimo = t;
  CRGB c = CRGB::Black;
  int e = estadoGrab;
  if (e != GR_REPOSO)  c = hayPerdidas ? CRGB(255, 70, 0) : CRGB(255, 0, 0);   // naranjo = se perdió audio
  else if (sdErrorMs && t - sdErrorMs < 3000) c = ((t / 150) & 1) ? CRGB(255, 0, 0) : CRGB::Black;
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Black;
  leds[0] = c;
  FastLED.show();
}
#endif

// ==============================================================================================================================================
// CONTROLES (core 0, 1 kHz)
// ==============================================================================================================================================
bool bNivel[5] = { HIGH, HIGH, HIGH, HIGH, HIGH };
unsigned long bTiempo[5] = { 0, 0, 0, 0, 0 };
unsigned long ultimoTap = 0;
float tapMedia = 0.0f; int tapN = 0;

float readPot(uint8_t pin) {
  uint32_t s = 0;
  for (int i = 0; i < 4; i++) s += analogRead(pin);  // cada analogRead cuesta decenas de µs: 4 alcanzan
  return (float)(s >> 2) / 4095.0f;
}

void tap(unsigned long t) {
  float ms = (float)(t - ultimoTap);
  ultimoTap = t;
  // fuera de 60–200 BPM: es el primer toque de una serie (o una pausa larga entre series)
  if (ms < 60000.0f / BPM_MAX || ms > 60000.0f / BPM_MIN) { tapN = 0; return; }
  // a mitad de serie, un intervalo 35 % distinto empieza otra serie SIN tocar el tempo:
  // así la pausa entre dos series no arrastra el promedio
  if (tapN > 0 && fabsf(ms - tapMedia) > 0.35f * tapMedia) { tapN = 1; tapMedia = ms; return; }
  tapN = tapN < 4 ? tapN + 1 : 4;
  tapMedia = (tapN == 1) ? ms : tapMedia + (ms - tapMedia) / (float)tapN;
  reqBPM = 60000.0f / tapMedia;
}

void pasoControl() {
  unsigned long t = millis();
  for (int b = 0; b < 5; b++) {
    bool nivel = digitalRead(BTN_PIN[b]);
    if (nivel == LOW && bNivel[b] == HIGH && (t - bTiempo[b]) > DEBOUNCE_MS) {
      bTiempo[b] = t;
      switch (b) {
        case 0: tocando = !tocando; break;                         // BTN1 play / stop
        case 1: reqPatron = reqPatron + 1 + (uint32_t)micros(); break;  // BTN2 patrón nuevo (el instante es la entropía)
        case 2: tap(t); break;                                     // BTN3 tap tempo
        case 3: bomboActivo = !bomboActivo; break;                 // BTN4 bombo sí / no
        case 4: pedidoGrabar = true; break;                        // BTN5 grabar / parar
      }
    }
    bNivel[b] = nivel;
  }
#ifndef SIMULADOR
  actualizarLED();
#endif
  static uint8_t scan = 0;
  int i = scan; scan = (scan + 1) & 3;
  float v = readPot(POT_PIN[i]);
  if      (i == 0) pCorte = v;
  else if (i == 1) pReso  = v;
  else if (i == 2) pEnv   = v;
  else             pDecay = v;
}

// ==============================================================================================================================================
// SETUP
// ==============================================================================================================================================
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  chan_cfg.dma_desc_num  = 4;
  chan_cfg.dma_frame_num = BUFFER_SAMPLES;   // ≈ 12 ms de cola: los botones suenan en el acto
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
void audioTask(void *)   { for (;;) renderBuffer(); }                     // core 1, prioridad 10
void controlTask(void *) { for (;;) { pasoControl(); vTaskDelay(1); } }   // core 0, 1 kHz
#endif

void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);
  for (int b = 0; b < 5; b++) pinMode(BTN_PIN[b], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  prepararSintesis();
  nuevoPatron((uint32_t)micros());
  calcularFiltro();
  i2s_init();

#ifndef SIMULADOR
  // Colchón de grabación: 8 s en PSRAM si hay; si no, 1 s en RAM interna
  ringBloques = (uint32_t)(RING_SEG_PSRAM * SAMPLE_RATE / BUFFER_SAMPLES);
  ring = (uint8_t *)heap_caps_malloc((size_t)ringBloques * BYTES_BLOQUE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ring) {
    ringBloques = (uint32_t)(RING_SEG_INTERNA * SAMPLE_RATE / BUFFER_SAMPLES);
    ring = (uint8_t *)heap_caps_malloc((size_t)ringBloques * BYTES_BLOQUE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(60);
  FastLED.clear(true);

  // El audio tiene el core 1 para él solo; control, grabadora y pantalla van al core 0.
  xTaskCreatePinnedToCore(audioTask,      "audio",     8192, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(controlTask,    "control",   4096, NULL,  3, NULL, 0);
  xTaskCreatePinnedToCore(tareaGrabadora, "grabadora", 6144, NULL,  2, NULL, 0);
  xTaskCreatePinnedToCore(tareaPantalla,  "pantalla",  4096, NULL,  1, NULL, 0);
#endif
}

#ifdef SIMULADOR
void loop() { pasoControl(); renderBuffer(); }
#else
void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }
#endif
