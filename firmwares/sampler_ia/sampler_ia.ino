// ==============================================================================================================================================
// PERCUSYNTH - SAMPLER IA (voz -> Whisper -> GPT -> ElevenLabs SFX -> sample disparable) - GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo Sandoval - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
//
// Pides un sonido HABLANDO y el PercuSynth lo genera, lo carga en un slot y lo dispara con un boton.
// Fusion de tres piezas del proyecto:
//   · asistente_ia ......... cadena de voz (mic INMP441 -> Whisper -> GPT por WiFi) y descarga PCM
//   · sample_loader ........ motor de disparo (resampleo por pitch, envolvente, pool de voces)
//   · trance_imu ........... filtro biquad resonante controlado por el IMU
//
// La diferencia con sample_loader: los samples NO vienen precompilados en PROGMEM desde el PC.
// Se generan EN VIVO con la API de ElevenLabs (Text-to-Sound Effects) y viven en PSRAM.
// ==============================================================================================================================================
// HARDWARE
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3 (PercuSynth). REQUIERE modulo CON PSRAM (los slots son ~660 KB).
//
// - DAC PCM5102 por I2S  ->  I2S_NUM_0 (SALIDA, 44.1 kHz / 16 bit / ESTEREO REAL):
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
// - BTN1 (GPIO 44) ..... MANTENER = grabar el pedido de voz (max 5 s). EXCLUSIVO de grabacion.
// - BTN2 (GPIO 42) ..... dispara SLOT 1   (mantener > 0.6 s = LOOP sostenido / textura)
// - BTN3 (GPIO 0)  ..... dispara SLOT 2   (idem)
// - BTN4 (GPIO 45) ..... dispara SLOT 3   (idem)
//   Si un slot esta vacio toca PRESTADO el ultimo sample cargado, pero TRANSPUESTO:
//   x1.000000 (BTN2) · x1.189207 = +3 semitonos (BTN3) · x1.498307 = +7 semitonos (BTN4).
//   Con un solo sample los tres botones dan una triada menor (Do -> Do, Re#, Sol).
// - BTN5 (GPIO 47) ..... SECUENCIA: toque = play/stop (manda Start/Stop MIDI al DAW)
//                        · mantener > 1 s = borrar patron
// - POT1 (ADC 1) ....... velocidad de la secuencia (60 - 200 BPM). No cambia de funcion.
// - POT2 (ADC 2) ....... volumen master
// - POT3 (ADC 8) ....... pitch / tono (-12 a +12 semitonos, centro = original)
//   MIXER: BTN2+BTN4 JUNTOS entran y salen del modo mixer: los pots 2/3/4 pasan a ser
//   el volumen de los canales 1/2/3 (LEDs 0-2 = faders visibles). Pickup: al cambiar
//   de modo cada pot queda mudo hasta que lo mueves, asi nada pega saltos.
// - POT4 (ADC 10) ...... STUTTER GRANULAR: congela un trocito de la mezcla y lo repite.
//                        Al minimo no hace nada; hacia el medio tartamudea en trozos ritmicos;
//                        al maximo los granos son de ~2 ms y se vuelven un tono. Va antes del
//                        filtro, asi el IMU puede domar el brillo de los granos cortos.
// - IMU  ............... eje X -> cutoff del filtro · eje Y -> resonancia (Q)
// - LEDs WS2812 en GPIO 46: los 6 SMD on-board van PRIMEROS en la linea de datos y despues
//   se encadena la TIRA EXTERNA DE 80 LEDs (86 en total). Los 6 SMD siguen siendo los
//   indicadores de estado; la tira es el show. Alimenta la tira desde 5V EXTERNOS con la
//   masa unida a la placa: 80 LEDs no salen del regulador de la ESP32.
// - LED RGB del modulo (GPIO 48)
// ==============================================================================================================================================
// ARDUINO IDE SETTINGS
// ==============================================================================================================================================
// - Placa:           ESP32S3 Dev Module
// - Flash Mode:      DIO            (IMPORTANTE en este hardware para que el I2S funcione bien)
// - PSRAM:           OPI PSRAM      (OBLIGATORIO: los 3 slots + buffer de grabacion son ~1 MB)
// - USB CDC On Boot: Enabled        (para ver el log de que sonido se pidio)
// - USB Mode:        USB-OTG (TinyUSB)   (OBLIGATORIO para que salga el MIDI por USB)
// - Upload/Monitor:  115200 baud
//
// Estos cinco ajustes estan FIJADOS en `sketch.yaml`, al lado de este archivo. El Arduino IDE
// 2.3+ los ofrece como perfil en el selector de placa, y por linea de comandos basta:
//     arduino-cli compile --profile percusynth
// Asi no hay que acordarse de cinco menus cada vez que el IDE se resetea.
//
// Si no necesitas el MIDI, pon MIDI_CLOCK_OUT en false (mas abajo, junto a los #include) y
// el firmware compila con cualquier USB Mode.
// ==============================================================================================================================================
// LIBRERIAS REQUERIDAS
// ==============================================================================================================================================
// - WiFi.h / WiFiClientSecure.h / HTTPClient.h   (core ESP32 Arduino)
// - driver/i2s_std.h                             (core ESP32 Arduino, nuevo driver I2S)
// - Wire.h                                       (core ESP32 Arduino, IMU)
// - USB.h / USBMIDI.h                            (core ESP32 Arduino >= 3.x, reloj MIDI USB)
// - FastLED
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
//   1) MANTEN BTN1 y di lo que quieres: "un golpe metalico oxidado con cola larga".
//   2) Al soltar: Whisper transcribe -> GPT lo traduce a un prompt de efecto de sonido en
//      ingles (mas duracion y si conviene que loopee) -> ElevenLabs devuelve el audio crudo.
//   3) El sample se recorta (quita el silencio inicial), se normaliza y queda en el SLOT
//      apuntado por el LED que parpadea. El destino avanza solo: 1 -> 2 -> 3 -> 1.
//   4) BTN2/3/4 lo disparan al instante. Manteniendo > 0.6 s el slot queda en LOOP (textura);
//      un toque nuevo sobre ese mismo boton lo apaga.
//   5) BTN5 arranca la secuencia de 16 pasos. CON LA SECUENCIA SONANDO, cada vez que tocas
//      BTN2/3/4 grabas ese golpe en el paso mas cercano (cuantizado). Mantener BTN5 = borrar.
//
//   FORMATO DE AUDIO Y PLANES DE ELEVENLABS
//   El firmware pide primero 'pcm_22050' (limpio). Ese formato exige plan Pro o superior;
//   si tu key lo rechaza, REINTENTA SOLO en 'ulaw_8000', disponible en todos los planes
//   (incluido Starter): 8 kHz, 8 bits, lo-fi. Se decodifica aqui mismo, sin librerias.
//
//   ESTEREO
//   Los samples de ElevenLabs son MONO, asi que el ancho se SINTETIZA. Cada canal del mixer
//   (el boton que aprietas) tiene su lugar fijo en la imagen con pan de potencia constante:
//   slot 1 a la izquierda, slot 2 al centro, slot 3 a la derecha. Ademas, las texturas
//   latcheadas en LOOP leen el canal derecho 6 ms atrasado (Haas), que abre mucho un sonido
//   sostenido; en los golpes secos NO se aplica, porque ahi el retardo se oye como un flam.
//   Toda la cadena posterior es estereo: dos biquad (el IMU mueve los dos con los mismos
//   coeficientes) y anillo de stutter intercalado, para que al entrar el efecto la imagen
//   no se colapse al centro. Si el ESP32 se queda sin CPU, la primera palanca es poner
//   HAAS_MS en 0: es lo que duplica las lecturas de PSRAM en los loops.
//
//   RELOJ MIDI POR USB (MAESTRO) - PARA GRABAR EN ABLETON
//   El PercuSynth se presenta al PC como dispositivo MIDI (compuesto con el puerto serie de
//   log) y MANDA reloj: 24 PPQ (0xF8) mas Start (0xFA) / Stop (0xFC) en BTN5. El tempo es el
//   de POT1. El reloj corre SIEMPRE, tambien con la secuencia parada, para que el DAW tenga
//   tempo desde que lo enchufas. No se RECIBE nada: el PercuSynth manda, el DAW sigue.
//     · En Ableton: Preferencias > Link/Tempo/MIDI, en la entrada "ESP32S3 Dev Module"
//       activa Sync. Luego pon Ableton en EXT.
//     · OJO: esto sincroniza la LINEA DE TIEMPO, no el audio. El sonido sigue saliendo
//       analogico por el PCM5102, o sea que para grabarlo necesitas interfaz de audio igual.
//     · La cola del DMA hace que el codigo vaya ~17 ms por delante de lo que se oye, asi que
//       el reloj sale adelantado esa misma cantidad (constante). Se corrige con el "MIDI
//       Clock Sync Delay" de Ableton en esa entrada, ajustando de oido.
//     · Mientras se genera un sample el loop se bloquea varios segundos: se manda Stop antes
//       y Start despues, para no arrastrar el tempo del DAW.
//
//   LEDs 0/1/2 = slots (apagado vacio · color cargado · flash al disparar · respirando en loop)
//   LED 3      = secuencia (pulso en cada negra)
//   LED 4      = filtro IMU (color por cutoff, brillo por resonancia)
//   LED 5      = estado: VERDE listo · ROJO grabando · AMBAR procesando · MAGENTA error
// ==============================================================================================================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "driver/i2s_std.h"
#include <FastLED.h>

// ==============================================================================================
// RELOJ MIDI POR USB - INTERRUPTOR
// ==============================================================================================
// Ponlo en false y el firmware compila con CUALQUIER USB Mode, sin MIDI. Todo lo demas (audio,
// secuenciador, LEDs, generacion por voz) funciona igual; solo dejas de mandarle reloj al DAW.
// Sirve cuando quieres probar el sampler y no estas grabando en Ableton.
#define MIDI_CLOCK_OUT   true

#if MIDI_CLOCK_OUT
#include "USB.h"
#include "USBMIDI.h"

// ---- Red de seguridad para la configuracion del IDE ------------------------------
// USB.h y USBMIDI.h se AUTO-ANULAN (quedan como archivos vacios) si el chip no tiene
// USB OTG. Sin este aviso, el error que aparece es "'USBMIDI' does not name a type" a
// 130 lineas de aqui, que no dice nada util, y ademas viene acompanado de una cascada
// de errores de FastLED sobre los GPIO 46/48 que despistan todavia mas. El sintoma es
// la placa, no los LEDs.
#if !defined(SOC_USB_OTG_SUPPORTED) || !SOC_USB_OTG_SUPPORTED
  #error "PLACA INCORRECTA. Herramientas > Placa > 'ESP32S3 Dev Module'. El 'ESP32 Dev Module' normal no tiene USB OTG, y ademas no tiene los GPIO 46/48 que usan los LEDs (por eso tambien reventaria FastLED)."
#endif

// Este de aqui es el importante, y no es obvio: la clase USBMIDI EXISTE igual con
// 'Hardware CDC and JTAG' (viene del sdkconfig precompilado del core, que no cambia con
// el menu), asi que el sketch COMPILA Y ARRANCA sin quejarse... y no enumera ningun
// dispositivo MIDI, porque el USB lo esta manejando el periferico USB-Serial-JTAG y no
// la pila TinyUSB. Un fallo silencioso. Quien distingue los dos modos es ARDUINO_USB_MODE,
// que llega como -D desde el menu: 0 = USB-OTG (TinyUSB), 1 = Hardware CDC and JTAG.
#if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE
  #error "USB MODE INCORRECTO. Herramientas > USB Mode > 'USB-OTG (TinyUSB)'. Con 'Hardware CDC and JTAG' esto compila pero el PC no ve ningun dispositivo MIDI: el USB lo maneja el USB-Serial-JTAG, no TinyUSB. Si no necesitas MIDI ahora, pon MIDI_CLOCK_OUT en false arriba del sketch."
#endif

// El PercuSynth aparece en el PC como dispositivo MIDI (ademas del puerto serie de log:
// TinyUSB los expone como un compuesto CDC + MIDI). Solo se usa para SALIDA de reloj.
USBMIDI MIDI;
#endif

// OJO: los envoltorios midiBegin()/midiByte() NO pueden ir aqui, aunque sea el sitio
// natural. Arduino inyecta los prototipos autogenerados justo antes de la PRIMERA
// definicion de funcion del archivo; si esa primera funcion queda por encima de las
// structs (Slot, SfxSpec, Btn), los prototipos se insertan antes que los tipos y salta
// "'Slot' does not name a type". Es la misma trampa que ya documentan las structs mas
// abajo. Los envoltorios estan definidos junto al reloj MIDI, pasadas las structs.

// ==================== CONFIGURACION USUARIO ====================

// --- Credenciales (WiFi + OpenAI + ElevenLabs) ---
// No viven en este archivo. Copia secretos.example.h a secretos.h (misma carpeta del
// sketch) y escribe ahi tus claves. secretos.h esta en .gitignore: nunca se sube al repo.
#if !__has_include("secretos.h")
  #error "Falta secretos.h: copia secretos.example.h a secretos.h y pon tus claves."
#endif
#include "secretos.h"

// GPT traduce tu pedido a un prompt de efecto de sonido. Si lo pones en false se le manda
// a ElevenLabs la transcripcion literal (mas rapido, resultados menos finos).
#define USE_GPT_PROMPT   true

// ==================== PINES ====================

#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41

#define MIC_WS    11
#define MIC_SCK   12
#define MIC_SD    13

#define BTN_REC   44          // BTN1 - exclusivo grabacion
#define BTN_S1    42          // BTN2 - slot 1
#define BTN_S2     0          // BTN3 - slot 2
#define BTN_S3    45          // BTN4 - slot 3
#define BTN_SEQ   47          // BTN5 - secuencia

#define POT_BPM    1          // ADC1 - velocidad de secuencia
#define POT_VOL    2          // ADC2 - volumen master
#define POT_PITCH  8          // ADC8 - pitch
#define POT_STUT  10          // ADC10 - stutter granular (largo del grano)

#define SDA_PIN   21
#define SCL_PIN   38
#define IMU_ADDR  0x68

#define LED_PIN    46
// Los 6 LEDs SMD de la placa van PRIMEROS en la misma linea de datos (GPIO 46); la tira
// externa se encadena despues. Por eso el indice de la tira arranca en STATUS_LEDS.
#define STATUS_LEDS 6                    // SMD on-board: indicadores de estado
#define STRIP_LEDS  80                   // tira externa WS2812
#define NUM_LEDS    (STATUS_LEDS + STRIP_LEDS)
#define BAND        (STRIP_LEDS / NUM_SLOTS)   // 26 LEDs por canal (sobran 2 al final)
#define RGB_PIN    48

// ==================== AUDIO ====================

#define OUT_RATE     44100    // salida al DAC
#define BUF_SAMPLES    128    // frames por bloque de render

#define MIC_RATE     16000    // grabacion de voz para Whisper
#define RECORD_SECONDS   5
#define MAX_MIC_SAMPLES (MIC_RATE * RECORD_SECONDS)
#define MIC_GAIN         6

#define NUM_SLOTS        3
#define MAX_SLOT_SECS    5
#define SLOT_RATE_PCM 22050   // pcm_22050
#define SLOT_RATE_ULAW 8000   // ulaw_8000 (fallback para planes sin PCM)
// El slot se dimensiona por el peor caso (22050 Hz x 5 s) = 110250 muestras = 220 KB
#define MAX_SLOT_SAMPLES (SLOT_RATE_PCM * MAX_SLOT_SECS)

// Pool de voces. Con 8 el secuenciador a semicorcheas + retriggers a mano agotaba el pool
// y cada robo de voz era un salto de amplitud = chasquido. 12 voces son ~500 bytes de RAM.
#define NUM_VOICES      12

// Envolvente de la voz. El ataque existe SOLO como red de seguridad para el robo de voz:
// el arranque limpio de verdad lo da el anclaje a cruce por cero al cargar el sample.
// Estuvo en 1 ms y era demasiado: sumado al fade de entrada del slot, aplastaba el
// transitorio de la percusion. 0.25 ms deja pasar entero hasta un armonico de 4 kHz.
#define ATTACK_SAMPLES  11                    // ~0.25 ms a 44.1 kHz
#define RELEASE_SAMPLES 1323                  // ~30 ms: cola al soltar

// Estereo: cada canal del mixer tiene su posicion fija en la imagen. Los samples de
// ElevenLabs son MONO, asi que el ancho se sintetiza (pan + Haas en las texturas).
#define HAAS_MS         6.0f                  // retardo del canal derecho en los loops
// 32 pasos de semicorchea = DOS compases. Con 16 (un compas) el patron duraba 2 s a 120 BPM
// y las texturas de 3-5 s no alcanzaban a sonar antes de re-dispararse. A 60 BPM son 8 s.
#define SEQ_STEPS       32

// ==================== ESTADO GLOBAL ====================

i2s_chan_handle_t tx_chan = NULL;
i2s_chan_handle_t rx_chan = NULL;

int16_t* micBuffer = nullptr;                 // grabacion de voz
uint8_t* netBuffer = nullptr;                 // descarga cruda desde ElevenLabs

struct Slot {
  int16_t* data   = nullptr;
  uint32_t len    = 0;                        // muestras utiles (ya recortadas)
  uint32_t rate   = SLOT_RATE_PCM;            // frecuencia nativa del sample
  bool     ready  = false;
  uint8_t  hue    = 0;                        // color en los LEDs
};
Slot slots[NUM_SLOTS];

struct Voice {
  int      slot   = -1;
  float    pos    = 0.0f;                     // posicion de lectura (con decimales = resampleo)
  float    step   = 1.0f;                     // avance por muestra de salida
  float    amp    = 0.0f;
  uint32_t held   = 0;                        // muestras que quedan antes de soltar la cola
  uint32_t loopLen = 0;                       // largo de la vuelta, cuadrado al pulso
  float    ratio  = 1.0f;                     // transposicion fija de esta voz (slot prestado)
  uint8_t  chan   = 0;                        // canal del mixer = boton que la disparo
  bool     loop   = false;
  bool     active = false;
  float    env    = 0.0f;                     // envolvente real (rampa hacia amp)
  float    panL   = 0.707f;                   // ganancias de potencia constante
  float    panR   = 0.707f;
  float    haas   = 0.0f;                     // desfase del canal derecho, en muestras del slot
};
Voice voices[NUM_VOICES];

// Un loop por slot: guardamos que voz lo esta sosteniendo para poder apagarlo
int loopVoice[NUM_SLOTS] = { -1, -1, -1 };

// Loop pedido pero aun no arrancado: espera a la proxima negra para entrar en la grilla.
// Sin esto el loop empieza donde apretaste el boton y queda desfasado para siempre.
bool pendingLoop[NUM_SLOTS] = { false, false, false };

// OJO: estas dos structs se declaran AQUI, arriba de todo, aunque solo se usen mas abajo.
// El pre-procesador de Arduino genera los prototipos de las funciones al principio del
// archivo; si el tipo se declara despues, esos prototipos no compilan ("does not name a type").

// Lo que GPT decide sobre el sonido a pedirle a ElevenLabs
struct SfxSpec { String prompt; float dur; bool loop; };

// Boton con deteccion de flanco y de pulsacion larga
struct Btn {
  uint8_t  pin;
  bool     last     = HIGH;
  unsigned long down = 0;
  bool     longDone = false;
};

// MIXER (BTN2+BTN4 juntos): los pots 2/3/4 pasan a ser volumen de cada canal.
// El fader va por CANAL (el boton que aprietas), no por slot: si el slot 2 esta tocando
// prestado el sample del 1, su volumen sigue siendo el del canal 2.
float slotVol[NUM_SLOTS] = { 1.0f, 1.0f, 1.0f };

// Modo mixer: se entra y se sale SOLO con el combo BTN2+BTN4 (ver handleSlotButton).
// Los pots usan "pickup": al cambiar de modo quedan mudos hasta que los mueves, asi ni
// los faders ni volumen/pitch/stutter pegan saltos al entrar o salir.
bool  mixerMode  = false;
bool  potLive[3] = { true, true, true };
float potRef[3]  = { 0.0f, 0.0f, 0.0f };

// Undo del combo (patron de cyber_kit): el sonido sale igual en el flanco, y cada
// presion de BTN2/BTN4 registra lo que hizo; si el otro boton llega dentro de la
// ventana, era el combo y lo del primer boton se deshace.
#define COMBO_MS 50
struct PressInfo {
  unsigned long t = 0;                        // millis de la presion (0 = consumida)
  int8_t voice      = -1;                     // voz que disparo (para mandarla a release)
  int8_t recStep    = -1;                     // paso que grabo en el secuenciador
  bool   wasLooping = false;                  // el toque apago un loop que sonaba
  bool   wasPending = false;                  // el toque cancelo un loop en espera
};
PressInfo lastPress[NUM_SLOTS];

// Controles (leidos una vez por bloque)
float g_vol      = 0.6f;
float g_pitch    = 1.0f;                      // multiplicador de velocidad de lectura
float g_len      = 1.0f;                      // fraccion del sample que suena (fija: entero)
float g_bpm      = 120.0f;

// STUTTER GRANULAR (POT4): congela un trocito de la mezcla y lo repite. Al minimo no hace
// nada; al medio tartamudea; al maximo los granos son de milisegundos y se vuelven un tono.
#define STUT_SIZE   32768                     // potencia de 2 (~0.74 s) -> indice por mascara
#define STUT_MASK   (STUT_SIZE - 1)
#define STUT_HOLD   (OUT_RATE / 5)            // ~200 ms congelado antes de capturar de nuevo

// Anillo ESTEREO intercalado (L,R,L,R...): si el stutter capturara la mezcla ya sumada a
// mono, cada vez que entrara el efecto la imagen se colapsaria al centro.
int16_t* stutBuf   = nullptr;                 // anillo con la mezcla reciente (PSRAM)
uint32_t stutWrite = 0;                       // cabeza de escritura (libre, se enmascara)
uint32_t stutStart = 0;                       // inicio del grano capturado
uint32_t stutPhase = 0;                       // posicion dentro del grano
uint32_t stutHeld  = 0;                       // muestras repetidas desde la ultima captura
bool     stutOn    = false;                   // para detectar el momento de enganche
float    g_stutMix = 0.0f;                    // 0 = seco, 1 = solo grano
uint32_t g_stutLen = 0;                       // largo del grano en muestras

// Filtro biquad LPF resonante, recalculado por bloque desde el IMU.
// Los coeficientes son comunes a los dos canales; el ESTADO no (un solo juego de z1/z2
// para L y R mezclaria los canales dentro del propio filtro).
float bq_b0 = 1, bq_b1 = 0, bq_b2 = 0, bq_a1 = 0, bq_a2 = 0;
float bq_z1L = 0, bq_z2L = 0, bq_z1R = 0, bq_z2R = 0;
float g_cutoff = 8000.0f, g_q = 0.9f;
bool  imuOK = false;
float imuX = 0.0f, imuY = 0.0f;

// Bloqueador de continua a la salida (uno por canal). Los samples de ElevenLabs, y sobre
// todo el mu-law del plan Starter, traen offset: con continua, cada disparo y cada corte
// es un escalon = chasquido, y ademas se come margen antes del limitador.
float dcxL = 0, dcyL = 0, dcxR = 0, dcyR = 0;

// Secuenciador. Los pasos NO se cuentan por muestras sino por ticks del reloj MIDI
// (24 PPQ, una semicorchea = 6 ticks): asi lo que suena y lo que sale por USB no
// pueden desfasarse entre si, vengan del mismo contador o no.
bool     seqPat[NUM_SLOTS][SEQ_STEPS];
bool     seqPlaying = false;
int      seqStep = 0;

// Reloj MIDI (maestro). Corre SIEMPRE, tambien con la secuencia parada: asi Ableton
// sigue el tempo de POT1 aunque no estes tocando, y Start/Stop solo marcan el transporte.
float    clkAcc  = 0.0f;                      // muestras acumuladas dentro del tick actual
uint32_t clkTick = 0;                         // ticks emitidos (24 por negra)

// Visual
CRGB leds[NUM_LEDS];
CRGB onboard[1];
float ledFlash[NUM_SLOTS] = { 0, 0, 0 };
float ledBeat = 0.0f;
int   targetSlot = 0;                         // slot destino de la proxima generacion

enum State { ST_READY, ST_RECORDING, ST_PROCESSING, ST_ERROR };
State g_state = ST_READY;
unsigned long g_errUntil = 0;

// ==================== UTILIDADES ====================

float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(pin);
  return (float)(sum >> 4) / 4095.0f;
}

// mu-law (G.711) -> PCM 16 bit. Es el formato de respaldo para planes sin PCM.
static inline int16_t ulaw2linear(uint8_t u) {
  u = ~u;
  int t = (((u & 0x0F) << 3) + 0x84) << ((unsigned)(u & 0x70) >> 4);
  return (u & 0x80) ? (int16_t)(0x84 - t) : (int16_t)(t - 0x84);
}

// ==================== I2S ====================

void i2s_dac_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear    = true;
  chan_cfg.dma_desc_num  = 6;
  chan_cfg.dma_frame_num = 128;               // cola corta = disparo instantaneo (~17 ms)
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
// Nota: el WHO_AM_I es solo informativo. Lo que manda es despertar el chip y reintentar;
// una sola lectura en frio puede fallar sin que el IMU este mal.

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

  // suavizado: el filtro no debe saltar con cada temblor de mano
  imuX += (constrain(ax / 16384.0f, -1.0f, 1.0f) - imuX) * 0.15f;
  imuY += (constrain(ay / 16384.0f, -1.0f, 1.0f) - imuY) * 0.15f;
  imuOK = true;
}

// ==================== FILTRO ====================

void updateFilter() {
  if (imuOK) {
    // X inclina el cutoff (escala exponencial: se oye lineal), Y abre la resonancia
    float nx = (imuX + 1.0f) * 0.5f;                     // 0..1
    float ny = (imuY + 1.0f) * 0.5f;
    g_cutoff += (200.0f * powf(60.0f, nx) - g_cutoff) * 0.25f;   // ~200 Hz .. 12 kHz
    g_q      += ((0.7f + ny * 7.0f) - g_q) * 0.25f;              // 0.7 .. 7.7
  } else {
    g_cutoff += (12000.0f - g_cutoff) * 0.05f;           // sin IMU: filtro abierto
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

// Direct Form II Transposed. El estado va por referencia para poder tener un juego por canal.
static inline float biquad(float x, float& z1, float& z2) {
  float y = bq_b0 * x + z1;
  z1      = bq_b1 * x - bq_a1 * y + z2;
  z2      = bq_b2 * x - bq_a2 * y;
  return y;
}

// Bloqueador de continua de un polo (corte ~20 Hz). y[n] = x[n] - x[n-1] + 0.997*y[n-1]
static inline float dcBlock(float x, float& xz, float& yz) {
  float y = x - xz + 0.997f * yz;
  xz = x; yz = y;
  return y;
}

// Interpolacion CUBICA de Catmull-Rom. La lineal que habia antes es un filtro pesimo:
// con el fallback ulaw_8000 (8 kHz -> 44.1 kHz, x5.5 de sobremuestreo) deja pasar las
// imagenes del espectro casi crudas, y eso es exactamente el "ruido raro"/aspereza que
// se oia. Cuesta ~4 multiplicaciones mas por voz y muestra.
static inline float cubic(float ym1, float y0, float y1, float y2, float t) {
  float c0 = y0;
  float c1 = 0.5f * (y1 - ym1);
  float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
  float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
  return ((c3 * t + c2) * t + c1) * t + c0;
}

// ==================== VOCES ====================

// Ultimo slot que se cargo: los slots vacios lo piden prestado para que el instrumento
// nunca este mudo. Con un solo sample generado, BTN2/3/4 y el secuenciador ya suenan.
int lastLoaded = -1;

// Cuando un slot toca PRESTADO, no repite la misma nota: la transpone. Los tres botones
// forman una triada menor sobre el sample original (si es un Do -> Do, Re#, Sol).
// Son multiplicadores de frecuencia = razon de velocidad de lectura del sample:
//   BTN2  0 semitonos  2^(0/12) = 1.000000   (tono original)
//   BTN3 +3 semitonos  2^(3/12) = 1.189207   (tercera menor)
//   BTN4 +7 semitonos  2^(7/12) = 1.498307   (quinta justa)
const float SLOT_RATIO[NUM_SLOTS] = { 1.000000f, 1.189207f, 1.498307f };

int resolveSlot(int s) {
  if (s < 0 || s >= NUM_SLOTS) return -1;
  if (slots[s].ready) return s;
  if (lastLoaded >= 0 && slots[lastLoaded].ready) return lastLoaded;
  return -1;
}

// Devuelve la voz que arranco (-1 si no habia sample): el combo del mixer la necesita
// para poder deshacer el disparo.
int triggerSlot(int sPedido, bool asLoop) {
  int s = resolveSlot(sPedido);
  if (s < 0) return -1;
  Slot& sl = slots[s];

  // Robo de voz: primero una libre, si no la que MENOS se esta oyendo. El criterio es
  // `env` (la ganancia real que sale) y no `amp` (la objetivo): una voz recien disparada
  // tiene amp = 1 pero env casi 0, y robar esa es justamente lo que no cuesta nada.
  int v = -1;
  for (int i = 0; i < NUM_VOICES; i++) if (!voices[i].active) { v = i; break; }
  if (v < 0) {
    float low = 1e9f;
    for (int i = 0; i < NUM_VOICES; i++) if (voices[i].env < low) { low = voices[i].env; v = i; }
  }

  // si robamos la voz que sostenia un loop, ese slot deja de considerarse en loop
  for (int k = 0; k < NUM_SLOTS; k++) if (loopVoice[k] == v) loopVoice[k] = -1;

  // Si el slot toca prestado, suena transpuesto; si tiene su propio sample, a su tono.
  float ratio = (s == sPedido) ? 1.0f : SLOT_RATIO[sPedido];

  voices[v].slot   = s;
  voices[v].pos    = 0.0f;
  voices[v].ratio  = ratio;
  voices[v].chan   = (uint8_t)sPedido;
  voices[v].step   = ((float)slots[s].rate / (float)OUT_RATE) * g_pitch * ratio;
  voices[v].amp    = 1.0f;
  voices[v].env    = 0.0f;                   // arranca en 0: la rampa de ataque evita el click
  voices[v].loop   = asLoop;
  voices[v].active = true;

  // ---- Estereo -----------------------------------------------------------------
  // SIN pan por canal. Lo hubo (slot 1 izquierda / 2 centro / 3 derecha, potencia
  // constante) y hubo que sacarlo: en el equipo de monitoreo de este proyecto el canal
  // que quedaba AL CENTRO se volvia casi inaudible, mientras los dos laterales sonaban
  // bien. Sintoma clasico de una suma L+R con un canal en oposicion de fase, donde lo
  // que esta al centro se cancela. Los tres canales van al centro, como antes.
  voices[v].panL = 0.7071f;
  voices[v].panR = 0.7071f;

  // Haas SOLO en las texturas latcheadas: leer el canal derecho unos milisegundos
  // atrasado abre mucho la imagen de un sonido sostenido. En los golpes secos no se
  // aplica, porque ahi el retardo se oye como un flam y ensucia el transitorio.
  voices[v].haas = asLoop ? (HAAS_MS * 0.001f * (float)slots[s].rate) : 0.0f;

  float dur = (float)slots[s].len / voices[v].step;
  voices[v].held = asLoop ? 0xFFFFFFFF : (uint32_t)(dur * g_len);

  // ---- Cuadrar el loop con el reloj -------------------------------------------
  // El largo del sample es arbitrario (la API no lo respeta y ademas le recortamos el
  // silencio inicial), asi que la vuelta NO se hace al final del archivo sino al pulso
  // mas cercano. Se trunca o se deja aire, pero siempre dura un numero entero de negras.
  // No se estira el audio: estirar cambiaria el tono, y un loop afinado que no calza es
  // peor que uno que calza y pierde la cola.
  voices[v].loopLen = 0;
  if (asLoop) {
    float durSeg  = (float)sl.len / ((float)sl.rate * ratio * g_pitch);   // segundos reales
    float pulsos  = durSeg * g_bpm / 60.0f;
    int   destino = (int)roundf(pulsos);
    if (destino < 1) destino = 1;
    // muestras del slot que ocupan esos pulsos, a la velocidad a la que va a sonar
    voices[v].loopLen = (uint32_t)((destino * 60.0f / g_bpm) * (float)sl.rate * ratio * g_pitch);
    if (voices[v].loopLen < 64) voices[v].loopLen = sl.len;               // red de seguridad
  }

  // el feedback (flash y loop) va al boton que APRETASTE, no al slot prestado
  ledFlash[sPedido] = 1.0f;
  if (asLoop) loopVoice[sPedido] = v;

  // ---- DIAGNOSTICO TEMPORAL (borrar cuando este resuelto) ----
  Serial.printf("TRIG btn%d slot=%d %s | ratio=%.3f step=%.4f | len=%u lim=%u held=%u"
                " | voz=%d chan=%d vol=%.2f pan=%.2f/%.2f | pitch=%.3f\n",
                sPedido + 2, s, asLoop ? "LOOP" : "corto",
                ratio, voices[v].step, sl.len,
                (asLoop && voices[v].loopLen) ? voices[v].loopLen : sl.len,
                voices[v].held, v, voices[v].chan, slotVol[voices[v].chan],
                voices[v].panL, voices[v].panR, g_pitch);
  return v;
}

void stopLoop(int s) {
  int v = loopVoice[s];
  if (v >= 0 && v < NUM_VOICES && voices[v].active && voices[v].loop) {
    voices[v].loop = false;
    voices[v].held = 0;                        // entra en la cola de release
  }
  loopVoice[s] = -1;
}

// ==================== RENDER ====================

// Lee el slot en una posicion con decimales, con interpolacion cubica. Necesita la
// vecindad (i-1 .. i+2): en un loop esa vecindad DA LA VUELTA, si no, se clampea.
static inline float readSample(const Slot& sl, float pos, uint32_t lim, bool loop) {
  int32_t L   = (int32_t)lim;
  int32_t i0  = (int32_t)pos;
  float   fr  = pos - (float)i0;
  int32_t im1 = i0 - 1, i1 = i0 + 1, i2 = i0 + 2;

  if (loop) {
    if (im1 < 0)  im1 += L;
    if (i1  >= L) i1  -= L;
    if (i2  >= L) i2  -= L;
  } else {
    if (im1 < 0)  im1 = 0;
    if (i1  >= L) i1  = L - 1;
    if (i2  >= L) i2  = L - 1;
  }

  // Si la vuelta al pulso es MAS LARGA que el sample, lo que falta es silencio:
  // el loop respira y sigue en tiempo, en vez de acelerarse para caber.
  float ym1 = ((uint32_t)im1 < sl.len) ? (float)sl.data[im1] : 0.0f;
  float y0  = ((uint32_t)i0  < sl.len) ? (float)sl.data[i0]  : 0.0f;
  float y1  = ((uint32_t)i1  < sl.len) ? (float)sl.data[i1]  : 0.0f;
  float y2  = ((uint32_t)i2  < sl.len) ? (float)sl.data[i2]  : 0.0f;

  return cubic(ym1, y0, y1, y2, fr) * (1.0f / 32768.0f);
}

void renderBlock(int16_t* out) {
  for (int n = 0; n < BUF_SAMPLES; n++) {
    float mixL = 0.0f, mixR = 0.0f;

    for (int i = 0; i < NUM_VOICES; i++) {
      Voice& v = voices[i];
      if (!v.active) continue;
      Slot& sl = slots[v.slot];

      // Vuelta del loop. OJO con lo que habia antes: restaba sl.len cuando i0+1 llegaba al
      // final, o sea con pos ~ len-1, dejando pos NEGATIVA. Al castear a uint32_t eso daba
      // indice 0 con fraccion negativa -> extrapolaba basura en cada vuelta = el chicharreo.
      // El loop da la vuelta en loopLen (cuadrado al pulso), no al final del archivo.
      uint32_t lim = (v.loop && v.loopLen > 0) ? v.loopLen : sl.len;

      if (v.pos >= (float)lim) {
        if (!v.loop) { v.active = false; continue; }
        v.pos -= (float)lim;
        if (v.pos < 0.0f || v.pos >= (float)lim) v.pos = 0.0f;      // red de seguridad
      }

      // ---- Ventana de bordes ------------------------------------------------------
      // El fade de 8 ms va en MUESTRAS DEL SLOT, no fijo: con el fallback ulaw_8000 las
      // 256 muestras que habia antes eran 32 ms y se comian la cola de los samples cortos.
      //
      // Y lo importante: la rampa de SALIDA ahora tambien corre para los one-shot. Antes
      // la voz se mataba en seco al llegar al final del sample (`active = false`), porque
      // held = duracion entera; los 30 ms de cola NUNCA llegaban a aplicarse y lo unico
      // que suavizaba el final eran los 2 ms grabados en el sample. Ese era el chasquido
      // que se oia al final de cada golpe, y con samples cortos disparados en semicorcheas
      // eran cuatro chasquidos por negra.
      float F = (float)sl.rate * 0.008f;
      float w = 1.0f;
      if ((float)lim > F * 4.0f) {
        if      (v.loop && v.pos < F)     w = v.pos / F;               // costura del loop
        else if (v.pos > (float)lim - F)  w = ((float)lim - v.pos) / F;
      }

      float sL = readSample(sl, v.pos, lim, v.loop);
      float sR = sL;

      // Haas: el canal derecho lee unos milisegundos atras. Solo en loops, donde la
      // posicion es periodica y el desfase se puede envolver sin leer fuera del bucle.
      if (v.haas > 0.0f) {
        float pr = v.pos - v.haas;
        if (pr < 0.0f) pr += (float)lim;
        if (pr >= 0.0f && pr < (float)lim) sR = readSample(sl, pr, lim, true);
      }

      // Rampa de ataque de 1 ms hacia la amplitud objetivo. Sin esto, el sample empieza
      // en pleno transitorio (el recorte de silencio no busca un cruce por cero) y cada
      // robo de voz metia un escalon.
      if (v.env < v.amp) {
        v.env += 1.0f / (float)ATTACK_SAMPLES;
        if (v.env > v.amp) v.env = v.amp;
      } else v.env = v.amp;

      float g = v.env * w * slotVol[v.chan];
      mixL += sL * g * v.panL;
      mixR += sR * g * v.panR;

      v.pos += v.step;

      if (v.held > 0) { v.held--; }
      else {
        v.amp -= 1.0f / (float)RELEASE_SAMPLES;   // cola de 30 ms: corta sin click
        if (v.amp <= 0.0f) { v.amp = 0.0f; v.active = false; }
      }
    }

    // ---- STUTTER GRANULAR ------------------------------------------------------
    // El anillo se escribe SIEMPRE, tambien con el efecto apagado: asi al enganchar
    // ya hay material grabado y el primer grano suena de inmediato, sin un hueco.
    uint32_t wi = (stutWrite & STUT_MASK) * 2;                  // anillo intercalado L,R
    stutBuf[wi]     = (int16_t)(constrain(mixL, -1.0f, 1.0f) * 32000.0f);
    stutBuf[wi + 1] = (int16_t)(constrain(mixR, -1.0f, 1.0f) * 32000.0f);
    stutWrite++;

    if (g_stutMix > 0.001f && g_stutLen > 0) {
      if (!stutOn) {                                   // recien enganchado: capturar ya
        stutOn = true; stutPhase = 0; stutHeld = 0;
        stutStart = stutWrite - g_stutLen;
      }
      if (stutPhase >= g_stutLen) {                    // fin del grano: repetir
        stutPhase = 0;
        stutHeld += g_stutLen;
        // Se recaptura cada ~200 ms pase lo que pase. Sin esto, un grano corto se queda
        // pegado como tono fijo y el efecto deja de seguir a la musica.
        if (stutHeld >= STUT_HOLD) { stutHeld = 0; stutStart = stutWrite - g_stutLen; }
      }

      uint32_t ri = ((stutStart + stutPhase) & STUT_MASK) * 2;
      float wl = stutBuf[ri]     * (1.0f / 32768.0f);
      float wr = stutBuf[ri + 1] * (1.0f / 32768.0f);

      // Ventana en los bordes del grano: repetir un trozo cortado en seco clickea
      // en cada vuelta, y a granos chicos eso son cientos de clicks por segundo.
      uint32_t fade = g_stutLen / 8; if (fade > 64) fade = 64;
      if (fade > 0) {
        float gw = 1.0f;
        if (stutPhase < fade)                   gw = (float)stutPhase / (float)fade;
        else if (stutPhase >= g_stutLen - fade) gw = (float)(g_stutLen - stutPhase) / (float)fade;
        wl *= gw; wr *= gw;
      }

      stutPhase++;
      mixL += (wl - mixL) * g_stutMix;
      mixR += (wr - mixR) * g_stutMix;
    } else stutOn = false;

    float yL = biquad(mixL, bq_z1L, bq_z2L) * g_vol;
    float yR = biquad(mixR, bq_z1R, bq_z2R) * g_vol;

    // Continua fuera antes del limitador: si el sample trae offset (tipico en mu-law),
    // se come margen de headroom y hace que cada corte suene como un golpe seco.
    yL = dcBlock(yL, dcxL, dcyL);
    yR = dcBlock(yR, dcxR, dcyR);

    if (yL >  1.2f) yL =  1.2f;
    if (yL < -1.2f) yL = -1.2f;
    if (yR >  1.2f) yR =  1.2f;
    if (yR < -1.2f) yR = -1.2f;
    yL = yL - (yL * yL * yL) * 0.15f;          // saturacion suave en vez de recorte duro
    yR = yR - (yR * yR * yR) * 0.15f;

    out[n * 2]     = (int16_t)(constrain(yL, -1.0f, 1.0f) * 32000.0f);
    out[n * 2 + 1] = (int16_t)(constrain(yR, -1.0f, 1.0f) * 32000.0f);
  }
}

// ==================== SECUENCIADOR ====================

// Un paso del secuenciador (semicorchea). Se llama desde el reloj, cada 6 ticks.
void seqStepFire() {
  seqStep = (seqStep + 1) % SEQ_STEPS;
  for (int s = 0; s < NUM_SLOTS; s++) {
    if (!seqPat[s][seqStep]) continue;
    // un slot latcheado en loop se deja en paz: si el secuenciador lo re-dispara,
    // corta la textura que estas sosteniendo a proposito.
    if (loopVoice[s] >= 0) continue;
    triggerSlot(s, false);
  }
  if (seqStep % 4 == 0) {                        // negra: aqui entran los loops en espera
    ledBeat = 1.0f;
    for (int s = 0; s < NUM_SLOTS; s++) {
      if (!pendingLoop[s]) continue;
      pendingLoop[s] = false;
      triggerSlot(s, true);
    }
  }
}

// ==================== RELOJ MIDI (MAESTRO) ====================
// Los envoltorios viven aqui y no arriba con los #include: ver la nota del bloque
// MIDI_CLOCK_OUT sobre los prototipos autogenerados de Arduino.
#if MIDI_CLOCK_OUT
static inline void midiBegin()          { MIDI.begin(); USB.begin(); }
static inline void midiByte(uint8_t b)  { MIDI.write(b); }
#else   // sin MIDI: los mismos puntos de llamada, pero no hacen nada
static inline void midiBegin()          {}
static inline void midiByte(uint8_t)    {}
#endif

// Se llama una vez por bloque (~2.9 ms) y es la UNICA fuente de tiempo: de aqui salen
// tanto los ticks 0xF8 que van al DAW como los pasos del secuenciador interno. Si cada
// uno contara por su cuenta, terminarian desfasados aunque partieran del mismo BPM.
//
// El reloj corre SIEMPRE, tambien con la secuencia parada, para que Ableton tenga tempo
// desde que lo enchufas; Start (0xFA) / Stop (0xFC) solo marcan el transporte.

void midiClockAdvance() {
  float samplesPerTick = (60.0f / g_bpm / 24.0f) * (float)OUT_RATE;   // 24 PPQ
  clkAcc += (float)BUF_SAMPLES;

  while (clkAcc >= samplesPerTick) {
    clkAcc -= samplesPerTick;
    // El paso se dispara ANTES de contar, no despues: por norma MIDI el primer 0xF8
    // tras un Start ya ES el primer pulso, asi que el paso 0 tiene que caer ahi mismo.
    // Contando primero, el patron entraba una semicorchea tarde respecto al DAW.
    if (seqPlaying && (clkTick % 6) == 0) seqStepFire();   // 6 ticks = una semicorchea
    midiByte(0xF8);                              // Timing Clock
    clkTick++;
  }
}

void midiTransport(bool play) {
  if (play) { clkTick = 0; clkAcc = 0.0f; }      // el proximo tick es el pulso 1
  midiByte(play ? 0xFA : 0xFC);                  // Start / Stop
}

// Graba un golpe en el paso mas cercano (cuantizado hacia adelante si vas tarde).
// Devuelve el paso grabado: el combo del mixer lo necesita para borrar el fantasma.
int seqRecord(int s) {
  // La posicion dentro del paso ahora se mide en ticks (6 por semicorchea): si vas
  // pasada la mitad, el golpe se cuantiza al paso siguiente en vez de atrasarse.
  int st = seqStep;
  if ((clkTick % 6) >= 3) st = (st + 1) % SEQ_STEPS;
  seqPat[s][st] = true;
  return st;
}

void seqClear() {
  for (int s = 0; s < NUM_SLOTS; s++)
    for (int i = 0; i < SEQ_STEPS; i++) seqPat[s][i] = false;
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

// ==================== GPT: pedido en espanol -> prompt de SFX ====================
// ElevenLabs responde mucho mejor a prompts cortos en ingles. GPT ademas decide la
// duracion y si el sonido conviene que loopee (textura) o sea un one-shot (golpe).

SfxSpec buildSfxSpec(String pedido) {
  SfxSpec spec;
  spec.prompt = pedido;
  spec.dur    = 2.0f;
  spec.loop   = false;

#if USE_GPT_PROMPT
  g_state = ST_PROCESSING;

  pedido.replace("\\", "\\\\"); pedido.replace("\"", "\\\"");
  pedido.replace("\n", " ");   pedido.replace("\r", "");

  String sys = "Convierte el pedido del usuario (en espanol) en un prompt de EFECTO DE SONIDO para ElevenLabs. ";
  sys += "Responde SOLO un JSON en una linea, sin explicaciones ni markdown, con esta forma exacta: ";
  sys += "{\\\"p\\\":\\\"...\\\",\\\"d\\\":2.0,\\\"l\\\":0}. ";
  sys += "p = prompt EN INGLES, corto y concreto, describiendo timbre, material y espacio (max 20 palabras). ";
  sys += "d = duracion en segundos entre 0.5 y 5.0. Si el usuario pide una duracion explicita, respetala ";
  sys += "(recortada a ese rango). Si no la pide: usa 0.5-1.5 para golpes percusivos y 3-5 para texturas, drones o voces cantadas. ";
  sys += "l = 1 si es una textura/ambiente que debe loopear sin costura, 0 si es un one-shot percusivo. ";
  sys += "El destino es un sampler musical: prioriza sonidos limpios. Las voces cantadas o habladas ";
  sys += "estan bien si el usuario las pide; solo evita musica con derechos de autor.";

  String body = "{\"model\":\"gpt-4o-mini\",\"messages\":[";
  body += "{\"role\":\"system\",\"content\":\"" + sys + "\"},";
  body += "{\"role\":\"user\",\"content\":\"" + pedido + "\"}";
  body += "],\"max_tokens\":120,\"temperature\":0.6}";

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
    // el JSON pedido viene escapado dentro de "content"
    int p = r.indexOf("\\\"p\\\":\\\"");
    if (p > 0) {
      p += 8;
      int e = r.indexOf("\\\"", p);
      if (e > p) spec.prompt = r.substring(p, e);
    }
    int d = r.indexOf("\\\"d\\\":");
    if (d > 0) { float v = r.substring(d + 6, d + 12).toFloat(); if (v >= 0.5f && v <= 5.0f) spec.dur = v; }
    int l = r.indexOf("\\\"l\\\":");
    if (l > 0) spec.loop = (r.charAt(l + 6) == '1');
  }
  http.end();
#endif

  spec.prompt.replace("\\", " "); spec.prompt.replace("\"", " ");
  if (spec.prompt.length() < 2) spec.prompt = "short metallic percussive hit";

  // Cuadrar el pedido con el reloj: la duracion se redondea a un numero ENTERO de pulsos
  // al BPM actual, y si es textura se le pide explicitamente un loop a ese tempo.
  // Esto solo ACERCA el material a la grilla: el recorte de silencio al cargarlo vuelve a
  // mover el largo, asi que el calce fino lo hace el motor al reproducir (ver triggerSlot).
  float beat = 60.0f / g_bpm;
  int   n    = (int)roundf(spec.dur / beat);
  if (n < 1) n = 1;
  while (n * beat < 0.5f) n++;                       // minimo de la API
  while (n * beat > 5.0f && n > 1) n--;              // maximo que guardamos por slot
  spec.dur = n * beat;

  if (spec.loop) {
    spec.prompt += " seamless loop at ";
    spec.prompt += String((int)g_bpm);
    spec.prompt += " BPM";
  }
  return spec;
}

// ==================== ELEVENLABS: prompt -> sample en el slot ====================

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
  body += ",\"prompt_influence\":0.45";
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
    // El motivo real viene en el CUERPO, no en la linea de estado: un 401 puede ser
    // "key invalida" o "a la key le falta el permiso sound_generation", que se arregla
    // distinto. Sin imprimirlo se depura a ciegas.
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

  // Igual que en asistente_ia: si viene chunked hay que quitar las cabeceras de tamano,
  // si no esos bytes ASCII entran al PCM y suenan como ruido fuerte.
  const size_t MAXB = MAX_SLOT_SAMPLES * 2;
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

// Recorta el silencio inicial y normaliza: sin esto el boton se siente "blando" y bajo.
void finishSlot(int s, uint32_t rawLen, uint32_t rate) {
  Slot& sl = slots[s];
  sl.rate = rate;
  if (rawLen < 8) { sl.len = 0; sl.ready = false; return; }

  // ---- 1) Quitar la continua -----------------------------------------------------
  // Va PRIMERO, antes de medir nada. Un sample con offset tiene un pico falso (el
  // offset se suma a la señal), se normaliza mal, y ademas cada arranque y cada corte
  // se convierten en un escalon de continua = el chasquido seco que se oia.
  int64_t suma = 0;
  for (uint32_t i = 0; i < rawLen; i++) suma += sl.data[i];
  int32_t dc = (int32_t)(suma / (int64_t)rawLen);
  if (dc > 40 || dc < -40) {                       // por debajo de eso no vale la pena
    for (uint32_t i = 0; i < rawLen; i++)
      sl.data[i] = (int16_t)constrain((int32_t)sl.data[i] - dc, -32768, 32767);
  }

  // ---- 2) Medir pico y RMS -------------------------------------------------------
  int32_t peak = 1;
  double  sq   = 0.0;
  for (uint32_t i = 0; i < rawLen; i++) {
    int32_t a = abs(sl.data[i]);
    if (a > peak) peak = a;
    sq += (double)sl.data[i] * (double)sl.data[i];
  }
  float rms = sqrtf((float)(sq / (double)rawLen));
  if (rms < 1.0f) rms = 1.0f;

  // primer cruce por encima de -40 dBFS respecto al pico, con 5 ms de pre-roll
  int32_t umbral = peak / 100;
  uint32_t start = 0;
  while (start < rawLen && abs(sl.data[start]) < umbral) start++;
  uint32_t pre = rate / 200;                       // 5 ms
  start = (start > pre) ? start - pre : 0;

  // ---- Anclar el arranque a un cruce por cero ------------------------------------
  // Esto es lo que permite que el fade de ENTRADA sea casi nulo. Si el sample ya
  // empieza donde la señal vale ~0, no hay escalon que suavizar y el transitorio
  // queda INTACTO. Se busca solo hacia ATRAS (hasta 2 ms): moverse hacia adelante
  // se comeria justo el ataque, que es lo que hay que proteger.
  uint32_t win   = rate / 500;                     // 2 ms
  uint32_t best  = start;
  int32_t  bestv = abs(sl.data[start]);
  for (uint32_t k = 1; k <= win && start >= k; k++) {
    int32_t a = abs(sl.data[start - k]);
    if (a < bestv) { bestv = a; best = start - k; }
  }
  start = best;

  uint32_t len = rawLen - start;
  if (start > 0) memmove(sl.data, sl.data + start, len * sizeof(int16_t));

  // ---- 3) Normalizar, pero sin subir el suelo de ruido ---------------------------
  // Con el pico solo, un sample casi vacio con un unico transitorio se multiplicaba
  // por 12 y lo que subia era el ruido de fondo (brutal en el fallback ulaw_8000, que
  // es 8 bits logaritmicos). Se toma la ganancia MAS CONSERVADORA de las dos: la que
  // deja el pico en ~0.9 FS y la que deja el RMS en ~0.25 FS.
  // El tope se deja en x12: quien evita amplificar ruido es el criterio de RMS, no un
  // techo bajo. Con x8 los golpes flojos (pico alto, RMS bajo = justo la percusion)
  // se quedaban cortos de nivel sin motivo.
  float gPeak = 29500.0f / (float)peak;
  float gRms  =  8000.0f / rms;
  float gain  = (gPeak < gRms) ? gPeak : gRms;
  if (gain > 12.0f) gain = 12.0f;
  if (gain > 1.0f) {
    for (uint32_t i = 0; i < len; i++) {
      int32_t v = (int32_t)(sl.data[i] * gain);
      sl.data[i] = (int16_t)constrain(v, -32768, 32767);
    }
  }

  // ---- 4) Fades de borde ASIMETRICOS, con curva de coseno ------------------------
  // Los dos bordes NO son el mismo problema y no pueden llevar el mismo fade:
  //
  //   SALIDA: 5 ms. Nadie oye que la cola se suavice, y es lo que mata el chasquido
  //           del final y de la costura del loop.
  //   ENTRADA: 0.5 ms. Aqui esta el ATAQUE, y en una bateria el ataque ES el sonido.
  //           Un fade de 5 ms se come entero el transitorio de un bombo o una caja:
  //           el golpe pierde el pegue y suena flojo y como si empezara a media
  //           altura (que era exactamente el sintoma). Con el arranque anclado a un
  //           cruce por cero, medio milisegundo basta para que no haya escalon.
  //
  // La curva es media campana de coseno y no una recta: el fade lineal llega a cero
  // pero su PENDIENTE cambia de golpe, y esa esquina ya se oye como un tic.
  uint32_t fadeOut = rate / 200;                   // 5 ms
  uint32_t fadeIn  = rate / 2000;                  // 0.5 ms
  if (fadeIn < 4) fadeIn = 4;

  if (fadeOut * 2 + fadeIn < len) {
    for (uint32_t i = 0; i < fadeIn; i++) {
      float g = 0.5f * (1.0f - cosf(PI * (float)i / (float)fadeIn));
      sl.data[i] = (int16_t)(sl.data[i] * g);
    }
    for (uint32_t i = 0; i < fadeOut; i++) {
      float g = 0.5f * (1.0f - cosf(PI * (float)i / (float)fadeOut));
      sl.data[len - 1 - i] = (int16_t)(sl.data[len - 1 - i] * g);
    }
  }

  sl.len   = len;
  sl.ready = len > 500;
  sl.hue   = random(0, 255);

  Serial.print("Slot "); Serial.print(s + 1);
  Serial.print(": "); Serial.print(len);
  Serial.print(" muestras @ "); Serial.print(rate);
  Serial.print(" Hz | dc="); Serial.print(dc);
  Serial.print(" pico="); Serial.print(peak);
  Serial.print(" rms="); Serial.print(rms, 0);
  Serial.print(" ganancia x"); Serial.print(gain, 2);
  Serial.print(" | arranque en |"); Serial.print(bestv);
  Serial.println(bestv < 300 ? "| (cruce por cero OK)" : "| (ATENCION: no encontro cruce)");
}

// Pide el sonido y lo deja cargado en el slot. true si quedo listo.
bool generateIntoSlot(int s, const SfxSpec& spec) {
  g_state = ST_PROCESSING;
  WiFi.setSleep(false);

  uint32_t rate = SLOT_RATE_PCM;
  size_t len = fetchSfx(spec, "pcm_22050");

  // pcm_22050 exige plan Pro. Si la key no lo permite, ulaw_8000 esta en todos los planes.
  if (len == 0) {
    Serial.println("Reintentando en ulaw_8000 (lo-fi, disponible en cualquier plan)...");
    rate = SLOT_RATE_ULAW;
    len  = fetchSfx(spec, "ulaw_8000");
  }

  WiFi.setSleep(true);
  if (len < 1000) return false;

  Slot& sl = slots[s];
  sl.ready = false;                                // por si estaba sonando

  uint32_t n = 0;
  if (rate == SLOT_RATE_ULAW) {
    n = min((uint32_t)len, (uint32_t)MAX_SLOT_SAMPLES);
    for (uint32_t i = 0; i < n; i++) sl.data[i] = ulaw2linear(netBuffer[i]);
  } else {
    n = min((uint32_t)(len / 2), (uint32_t)MAX_SLOT_SAMPLES);
    for (uint32_t i = 0; i < n; i++)               // s16 little-endian
      sl.data[i] = (int16_t)(netBuffer[i * 2] | (netBuffer[i * 2 + 1] << 8));
  }

  finishSlot(s, n, rate);
  if (sl.ready) lastLoaded = s;
  return sl.ready;
}

// ==================== CICLO COMPLETO DE GENERACION ====================

void generarSample() {
  int n = recordVoice();
  if (n < 1000) { g_state = ST_READY; return; }

  String pedido = transcribeAudio(n);
  Serial.print("Pedido: "); Serial.println(pedido);
  if (pedido.length() < 2) { g_state = ST_ERROR; g_errUntil = millis() + 1500; return; }

  SfxSpec spec = buildSfxSpec(pedido);
  Serial.print("SFX: "); Serial.print(spec.prompt);
  Serial.print(" | "); Serial.print(spec.dur, 1); Serial.print("s | loop=");
  Serial.println(spec.loop ? "si" : "no");

  if (generateIntoSlot(targetSlot, spec)) {
    triggerSlot(targetSlot, false);                // audicion inmediata del sample nuevo
    targetSlot = (targetSlot + 1) % NUM_SLOTS;     // el destino avanza solo
    g_state = ST_READY;
  } else {
    g_state = ST_ERROR;
    g_errUntil = millis() + 2000;
  }
}

// ==================== BOTONES ====================
// Regla del proyecto: el sonido sale en el FLANCO DE PRESION, sin ventanas de espera.
// El "mantener" solo AGREGA comportamiento (loop) sobre algo que ya sono.

Btn bS1 = { BTN_S1 }, bS2 = { BTN_S2 }, bS3 = { BTN_S3 }, bSeq = { BTN_SEQ };

// Entra o sale del modo mixer y re-arma el pickup de los pots.
void mixerToggle() {
  mixerMode = !mixerMode;
  potRef[0] = readPot(POT_VOL);
  potRef[1] = readPot(POT_PITCH);
  potRef[2] = readPot(POT_STUT);
  potLive[0] = potLive[1] = potLive[2] = false;
  Serial.println(mixerMode
    ? "MIXER ON: pots 2/3/4 = volumen de canal 1/2/3 (BTN2+BTN4 para salir)"
    : "MIXER OFF: pots 2/3/4 vuelven a volumen / pitch / stutter");
}

// Deshace lo que hizo la presion registrada del boton s: resulto ser parte del combo.
void comboUndo(int s) {
  PressInfo& pi = lastPress[s];
  if (pi.voice >= 0 && pi.voice < NUM_VOICES && voices[pi.voice].active) {
    voices[pi.voice].loop = false;               // a la cola de release: se apaga sin click
    voices[pi.voice].held = 0;
    if (loopVoice[s] == pi.voice) loopVoice[s] = -1;
  }
  if (pi.recStep >= 0 && pi.recStep < SEQ_STEPS)
    seqPat[s][pi.recStep] = false;               // borrar el golpe fantasma del overdub
  if (pi.wasLooping || pi.wasPending) {          // el toque habia apagado una textura:
    if (seqPlaying) pendingLoop[s] = true;       // se restaura (al pulso si hay reloj)
    else            triggerSlot(s, true);
  }
  pi = PressInfo();
}

void handleSlotButton(Btn& b, int s) {
  bool now = digitalRead(b.pin);

  if (now == LOW && b.last == HIGH) {              // presion
    b.down = millis();
    b.longDone = false;

    // ---- COMBO BTN2+BTN4 = entrar/salir del MIXER --------------------------------
    // El sonido salio igual en el flanco del primer boton (regla del proyecto); si el
    // otro boton del combo llego hace menos de COMBO_MS, esto no era tocar: se deshace
    // lo del primero, este no dispara nada, y se cambia de modo.
    if (s == 0 || s == 2) {
      int otro = (s == 0) ? 2 : 0;
      if (lastPress[otro].t && millis() - lastPress[otro].t < COMBO_MS) {
        comboUndo(otro);
        b.longDone = true;                         // mantener no debe enganchar loop
        bS1.longDone = bS3.longDone = true;
        Serial.println("COMBO BTN2+BTN4");
        mixerToggle();
        b.last = now;
        return;
      }
    }

    PressInfo pi;
    pi.t = millis();

    int r = resolveSlot(s);
    Serial.print("BTN slot "); Serial.print(s + 1);
    if (r < 0)            Serial.println(" -> vacio (aun no hay ningun sample)");
    else if (r == s)      Serial.println(" -> sample propio, tono original");
    else { Serial.print(" -> presta el slot "); Serial.print(r + 1);
           Serial.print(" x"); Serial.println(SLOT_RATIO[s], 6); }
    if (loopVoice[s] >= 0 || pendingLoop[s]) {
      pi.wasPending = pendingLoop[s];
      pi.wasLooping = (loopVoice[s] >= 0);
      pendingLoop[s] = false;                      // cancela el que estaba por entrar
      stopLoop(s);                                 // ya loopeaba -> este toque lo apaga
      b.longDone = true;                           // y no vuelve a engancharlo al mantener
    } else {
      pi.voice = (int8_t)triggerSlot(s, false);    // instantaneo
      if (seqPlaying) pi.recStep = (int8_t)seqRecord(s);   // overdub cuantizado
    }
    if (s == 0 || s == 2) lastPress[s] = pi;       // por si es la primera mitad del combo
  }

  if (now == LOW && !b.longDone && (millis() - b.down) > 600) {
    b.longDone = true;
    if (seqPlaying) {
      pendingLoop[s] = true;                       // entra en la proxima negra
      Serial.print("LOOP slot "); Serial.print(s + 1); Serial.println(" -> espera al pulso");
    } else {
      triggerSlot(s, true);                        // sin reloj no hay a que esperar
    }
  }

  b.last = now;
}

void handleSeqButton() {
  bool now = digitalRead(bSeq.pin);

  if (now == LOW && bSeq.last == HIGH) {
    bSeq.down = millis();
    bSeq.longDone = false;
  }

  if (now == LOW && !bSeq.longDone && (millis() - bSeq.down) > 1000) {
    bSeq.longDone = true;
    seqClear();
    ledBeat = 1.0f;
  }

  if (now == HIGH && bSeq.last == LOW && !bSeq.longDone) {
    seqPlaying = !seqPlaying;
    seqStep = SEQ_STEPS - 1;                       // el primer avance cae en el paso 0
    midiTransport(seqPlaying);                     // Start / Stop al DAW

    int golpes = 0;
    for (int s = 0; s < NUM_SLOTS; s++)
      for (int i = 0; i < SEQ_STEPS; i++) if (seqPat[s][i]) golpes++;

    Serial.print("SEQ ");
    Serial.print(seqPlaying ? "PLAY" : "STOP");
    Serial.print(" | "); Serial.print(g_bpm, 0); Serial.print(" BPM | ");
    Serial.print(SEQ_STEPS); Serial.print(" pasos = ");
    Serial.print((60.0f / g_bpm / 4.0f) * SEQ_STEPS, 1);   // duracion real del patron
    Serial.print(" s | golpes: "); Serial.println(golpes);
    if (seqPlaying && golpes == 0)
      Serial.println("  (patron vacio: toca BTN2/3/4 mientras corre para grabar golpes)");
  }

  bSeq.last = now;
}

// ==================== LEDs ====================

// ==================== TIRA DE 80 LEDs ====================
// La tira se reparte en 3 BANDAS, una por canal del mixer, y encima corre el cabezal del
// secuenciador. Todo lo que pinta sale de estado que YA existe (nada de animaciones que
// no signifiquen nada): brillo de fondo = volumen del canal, cometa = disparo, respiracion
// = loop enganchado, tono = filtro del IMU, y el stutter congela la imagen igual que al audio.

static inline void stripPix(int i, CRGB c) {          // i = 0..STRIP_LEDS-1
  if (i >= 0 && i < STRIP_LEDS) leds[STATUS_LEDS + i] += c;
}

void renderStrip() {
  // Los estados de red mandan sobre todo: si esta grabando o generando, se ve de lejos.
  if (g_state != ST_READY) {
    CRGB c = (g_state == ST_RECORDING)  ? CRGB(90, 0, 0)
           : (g_state == ST_PROCESSING) ? CRGB(70, 40, 0)
                                        : CRGB(80, 0, 80);
    // barrido de carga mientras procesa, para que no parezca colgado
    uint8_t fase = (uint8_t)((millis() / 6) % STRIP_LEDS);
    for (int i = 0; i < STRIP_LEDS; i++) {
      uint8_t d = abs((int)fase - i);
      leds[STATUS_LEDS + i] = (g_state == ST_PROCESSING && d < 10)
                              ? CRGB(140, 90, 0) : c;
    }
    return;
  }

  // El filtro del IMU tine toda la escena: cerrado = frio/violeta, abierto = calido.
  float ap  = constrain((g_cutoff - 200.0f) / 11800.0f, 0.0f, 1.0f);
  uint8_t hueBase = (uint8_t)(160.0f - ap * 130.0f);

  for (int i = 0; i < STRIP_LEDS; i++) leds[STATUS_LEDS + i] = CRGB::Black;

  for (int s = 0; s < NUM_SLOTS; s++) {
    int ini = s * BAND;
    bool hay = (resolveSlot(s) >= 0);
    if (!hay) continue;

    uint8_t hue = slots[resolveSlot(s)].hue;
    float   vol = slotVol[s];

    // fondo: el volumen del canal se VE; en modo mixer se exagera para leerlo de lejos
    uint8_t base = mixerMode ? (uint8_t)(8 + vol * 150.0f) : (uint8_t)(6 + vol * 42.0f);
    for (int i = 0; i < BAND; i++) stripPix(ini + i, CHSV(hue, 200, base));

    // loop enganchado: la banda respira; en espera del pulso, parpadea rapido
    if (loopVoice[s] >= 0) {
      uint8_t br = (uint8_t)(30 + 45 * (0.5f + 0.5f * sinf(millis() * 0.003f + s)));
      for (int i = 0; i < BAND; i++) stripPix(ini + i, CHSV(hue, 190, (uint8_t)(br * vol)));
    } else if (pendingLoop[s]) {
      if ((millis() / 90) % 2) for (int i = 0; i < BAND; i++) stripPix(ini + i, CRGB(35, 35, 35));
    }

    // disparo: cometa que sale del centro de la banda hacia los dos extremos
    if (ledFlash[s] > 0.02f) {
      float   av  = 1.0f - ledFlash[s];                  // 0 al golpear -> 1 al apagarse
      int     c   = ini + BAND / 2;
      int     ext = (int)(av * (BAND / 2 + 2));
      uint8_t v   = (uint8_t)(ledFlash[s] * 235.0f * (0.25f + 0.75f * vol));
      stripPix(c - ext, CHSV(hue + 24, 230, v));
      stripPix(c + ext, CHSV(hue + 24, 230, v));
      stripPix(c,       CHSV(hue,      200, (uint8_t)(v * 0.5f)));
    }
  }

  // cabezal del secuenciador: recorre la tira entera, un LED por paso
  if (seqPlaying) {
    int p = (seqStep * STRIP_LEDS) / SEQ_STEPS;
    for (int k = 0; k < 3; k++)
      stripPix(p + k, CHSV(hueBase + 40, 120, (uint8_t)(150 - k * 45)));
  }

  // stutter: la imagen se congela y estrobea al ritmo del grano, igual que el audio
  if (g_stutMix > 0.01f && g_stutLen > 0) {
    uint32_t perGrano = (g_stutLen * 1000) / OUT_RATE;    // ms por grano
    if (perGrano < 20) perGrano = 20;                     // mas rapido no se ve, solo molesta
    if (((millis() / perGrano) % 2) == 0) {
      for (int i = 0; i < STRIP_LEDS; i++)
        leds[STATUS_LEDS + i].nscale8((uint8_t)(255 - g_stutMix * 190));
    }
  }
}

void updateLeds() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  for (int s = 0; s < NUM_SLOTS; s++) {
    if (!slots[s].ready) {
      // vacio: si esta prestando el ultimo sample, se ve tenue con SU color (asi se
      // distingue "presto" de "no hay nada"); el destino de la proxima grabacion respira
      int pres = resolveSlot(s);
      if (pres >= 0) {
        uint8_t v = (uint8_t)min(255.0f, 14.0f + ledFlash[s] * 150.0f);
        leds[s] = CHSV(slots[pres].hue, 205, v);
      }
      if (s == targetSlot) {
        uint8_t br = (uint8_t)(18 + 16 * sinf(millis() * 0.005f));
        leds[s] += CRGB(br, br, br);
      }
      continue;
    }
    uint8_t v = 45;
    if (loopVoice[s] >= 0) v = (uint8_t)(90 + 50 * sinf(millis() * 0.004f));
    v = (uint8_t)min(255.0f, v + ledFlash[s] * 165.0f);
    leds[s] = CHSV(slots[s].hue, 205, v);
    if (s == targetSlot) leds[s] += CRGB(12, 12, 12);
  }

  // MIXER: los LEDs de canal pasan a ser faders visibles (brillo = volumen del canal)
  if (mixerMode)
    for (int s = 0; s < NUM_SLOTS; s++) {
      int r = resolveSlot(s);
      uint8_t v = (uint8_t)(8 + slotVol[s] * 190.0f);
      leds[s] = (r >= 0) ? CHSV(slots[r].hue, 160, v) : CHSV(0, 0, (uint8_t)(v / 3));
    }

  leds[3] = seqPlaying ? CHSV(96, 200, (uint8_t)(30 + ledBeat * 200)) : CRGB(0, 6, 0);

  uint8_t fh = (uint8_t)(160.0f - constrain((g_cutoff - 200.0f) / 11800.0f, 0.0f, 1.0f) * 130.0f);
  leds[4] = CHSV(fh, 190, (uint8_t)(30 + constrain((g_q - 0.7f) / 7.0f, 0.0f, 1.0f) * 180));

  CRGB st;
  switch (g_state) {
    case ST_READY:      st = CRGB(0, 40, 0);    break;
    case ST_RECORDING:  st = CRGB(130, 0, 0);   break;
    case ST_PROCESSING: st = CRGB(95, 55, 0);   break;
    case ST_ERROR:      st = CRGB(110, 0, 110); break;
  }
  leds[5] = st;

  // el LED del modulo resume el promedio de la tira
  uint16_t sr = 0, sg = 0, sb = 0;
  for (int i = 0; i < STATUS_LEDS; i++) { sr += leds[i].r; sg += leds[i].g; sb += leds[i].b; }
  onboard[0] = CRGB(sr / STATUS_LEDS, sg / STATUS_LEDS, sb / STATUS_LEDS);

  renderStrip();
  FastLED.show();

  for (int s = 0; s < NUM_SLOTS; s++) ledFlash[s] *= 0.82f;
  ledBeat *= 0.86f;
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(400);
  setCpuFrequencyMhz(240);

  pinMode(BTN_REC, INPUT_PULLUP);
  pinMode(BTN_S1,  INPUT_PULLUP);
  pinMode(BTN_S2,  INPUT_PULLUP);
  pinMode(BTN_S3,  INPUT_PULLUP);
  pinMode(BTN_SEQ, INPUT_PULLUP);

  analogReadResolution(12);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.addLeds<WS2812, RGB_PIN, GRB>(onboard, 1);
  FastLED.setBrightness(45);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // --- memoria en PSRAM ---
  micBuffer = (int16_t*)ps_malloc(MAX_MIC_SAMPLES * sizeof(int16_t));
  netBuffer = (uint8_t*)ps_malloc(MAX_SLOT_SAMPLES * 2);
  stutBuf   = (int16_t*)ps_malloc(STUT_SIZE * 2 * sizeof(int16_t));       // L,R intercalados
  if (stutBuf) memset(stutBuf, 0, STUT_SIZE * 2 * sizeof(int16_t));       // arrancar en silencio
  for (int s = 0; s < NUM_SLOTS; s++)
    slots[s].data = (int16_t*)ps_malloc(MAX_SLOT_SAMPLES * sizeof(int16_t));

  bool memOK = micBuffer && netBuffer && stutBuf;
  for (int s = 0; s < NUM_SLOTS; s++) if (!slots[s].data) memOK = false;
  if (!memOK) {
    Serial.println("Sin PSRAM suficiente. Activa 'PSRAM: OPI PSRAM' en el IDE.");
    while (1) {
      fill_solid(leds, NUM_LEDS, CRGB(110, 0, 110)); FastLED.show(); delay(300);
      fill_solid(leds, NUM_LEDS, CRGB::Black);       FastLED.show(); delay(300);
    }
  }

  // MIDI USB: el PercuSynth se ofrece al PC como dispositivo MIDI (maestro de reloj).
  // Va DESPUES del Serial.begin: USB.begin() levanta el compuesto CDC + MIDI de una vez.
  // Si MIDI_CLOCK_OUT es false, esto no hace nada y el firmware corre igual sin MIDI.
  midiBegin();

  seqClear();
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
    Serial.println("Sin WiFi: no se pueden generar samples nuevos (lo demas funciona).");
    g_state = ST_ERROR;
    g_errUntil = millis() + 3000;
  } else {
    WiFi.setSleep(true);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    g_state = ST_READY;
  }

  randomSeed(esp_random());
}

// ==================== LOOP ====================
// El ritmo lo marca el DMA: i2s_channel_write bloquea hasta que hay hueco, asi que
// todo el control se atiende una vez por bloque de 128 muestras (~2.9 ms).

void loop() {
  static int16_t out[BUF_SAMPLES * 2];
  static uint8_t ctrlDiv = 0;
  static unsigned long lastLed = 0;

  // --- control cada 4 bloques (~12 ms): pots e IMU no necesitan mas ---
  if (++ctrlDiv >= 4) {
    ctrlDiv = 0;
    g_bpm = 60.0f + readPot(POT_BPM) * 140.0f;               // POT1 no cambia de funcion

    // ---- POTS 2/3/4: dos modos, con pickup ----------------------------------------
    // La v1 del mixer (pots-fader mientras MANTENIAS el boton de disparo) era injugable:
    // mantener es tambien el gesto de enganchar un loop, asi que el panel cambiaba en
    // CADA pulsacion y los valores congelados quedaban desincronizados del pot fisico,
    // sin forma de saber en que estado estabas. La v2 entra y sale SOLO con BTN2+BTN4
    // juntos (ver handleSlotButton). Dentro: pots 2/3/4 = volumen de canal 1/2/3.
    // Fuera: volumen / pitch / stutter. El pickup (potLive/potRef) deja cada pot mudo
    // al cambiar de modo hasta que lo mueves: nada pega saltos.
    float p2 = readPot(POT_VOL), p3 = readPot(POT_PITCH), p4 = readPot(POT_STUT);

    if (!potLive[0] && fabsf(p2 - potRef[0]) > 0.04f) potLive[0] = true;
    if (!potLive[1] && fabsf(p3 - potRef[1]) > 0.04f) potLive[1] = true;
    if (!potLive[2] && fabsf(p4 - potRef[2]) > 0.04f) potLive[2] = true;

    if (mixerMode) {
      if (potLive[0]) slotVol[0] = p2 * p2;                  // misma curva cuadratica
      if (potLive[1]) slotVol[1] = p3 * p3;                  // que el volumen master
      if (potLive[2]) slotVol[2] = p4 * p4;
    } else {
      if (potLive[0]) g_vol = p2 * p2;                       // curva cuadratica

      // Pitch: -12..+12 semitonos con zona muerta en el centro (volver al tono original
      // a mano es imposible si no hay detente).
      if (potLive[1]) {
        float semis = p3 * 24.0f - 12.0f;
        if (fabsf(semis) < 0.8f) semis = 0.0f;
        g_pitch = powf(2.0f, semis / 12.0f);
      }

      // POT4 = stutter granular. El grano se acorta de forma EXPONENCIAL (500 ms -> 2 ms):
      // en lineal, toda la parte interesante se apelotona en el ultimo cuarto del recorrido.
      //   ~0     apagado        ~1/3  tartamudeo ritmico
      //   ~2/3   granos cortos   1.0  granos de 2 ms = zumbido afinado (~500 Hz)
      if (potLive[2]) {
        if (p4 < 0.03f) { g_stutMix = 0.0f; g_stutLen = 0; }
        else {
          float t = (p4 - 0.03f) / 0.97f;                    // 0..1
          g_stutLen = (uint32_t)(OUT_RATE * 0.5f * powf(0.004f, t));
          if (g_stutLen < 32) g_stutLen = 32;
          // entrada suave: sin rampa, pasar el umbral era un salto brusco a efecto total
          g_stutMix = (t < 0.12f) ? (t / 0.12f) : 1.0f;
        }
      }
    }

    // El pitch se aplica EN VIVO a las voces que estan sonando: sin esto mover el pot
    // solo afectaba al siguiente disparo y se sentia muerto (y las texturas en loop
    // no se podian barrer de tono, que es media gracia del sampler).
    for (int i = 0; i < NUM_VOICES; i++) {
      if (!voices[i].active) continue;
      voices[i].step = ((float)slots[voices[i].slot].rate / (float)OUT_RATE)
                       * g_pitch * voices[i].ratio;   // respeta la transposicion del slot
    }

    if (imuOK) imuRead();
    updateFilter();
  }

  // --- botones (flanco de presion = sonido inmediato) ---
  handleSlotButton(bS1, 0);
  handleSlotButton(bS2, 1);
  handleSlotButton(bS3, 2);
  handleSeqButton();

  // --- reloj (manda los ticks y avanza el secuenciador) y audio ---
  midiClockAdvance();
  renderBlock(out);
  size_t bw;
  i2s_channel_write(tx_chan, out, sizeof(out), &bw, portMAX_DELAY);

  // --- LEDs a ~30 FPS (show() bloquea, no conviene por bloque) ---
  if (millis() - lastLed > 33) {
    lastLed = millis();
    if (g_state == ST_ERROR && millis() > g_errUntil) g_state = ST_READY;
    updateLeds();
  }

  // --- BTN1: grabar pedido de voz (bloquea el audio mientras dura la generacion) ---
  if (digitalRead(BTN_REC) == LOW) {
    delay(40);
    if (digitalRead(BTN_REC) == LOW) {
      // La generacion bloquea el loop varios segundos (WiFi + Whisper + ElevenLabs), asi
      // que el reloj se congela: hay que avisarle al DAW con un Stop, o se queda esperando
      // ticks que no llegan y arrastra el tempo. Al volver se manda Start otra vez.
      bool seqEstaba = seqPlaying;
      if (seqEstaba) midiTransport(false);
      seqPlaying = false;
      for (int i = 0; i < NUM_VOICES; i++) { voices[i].active = false; voices[i].env = 0.0f; }
      for (int s = 0; s < NUM_SLOTS; s++) { loopVoice[s] = -1; pendingLoop[s] = false; }
      updateLeds();

      generarSample();

      seqPlaying = seqEstaba;
      if (seqEstaba) { seqStep = SEQ_STEPS - 1; midiTransport(true); }
      while (digitalRead(BTN_REC) == LOW) delay(10);   // no re-disparar al soltar
    }
  }
}
