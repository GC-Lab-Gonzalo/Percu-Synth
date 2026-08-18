// ==============================================================================================================================================
// PERCUSYNTH - ASISTENTE DE VOZ IA SOBRE NagaAI (api.naga.ac) - GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo Sandoval - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
//
// Hermano de asistente_ia, pero hablando con NagaAI en vez de con OpenAI. NagaAI es un
// agregador: una sola API compatible con OpenAI que enruta a modelos de muchos proveedores
// (OpenAI, Google, Meta, DeepSeek, ElevenLabs...) con una sola clave y un solo saldo.
//
// Los ENDPOINTS son identicos a los de OpenAI (/v1/audio/transcriptions, /v1/chat/completions,
// /v1/audio/speech); lo que cambia es el HOST, los NOMBRES DE MODELO y un parametro:
//
//     OpenAI                         NagaAI
//     ----------------------------   ------------------------------------------
//     api.openai.com                 api.naga.ac
//     whisper-1                      whisper-large-v3   (whisper-1 NO existe aqui)
//     tts-1                          gpt-4o-mini-tts    (tts-1 NO existe aqui)
//     gpt-4o-mini                    llama-3.3-70b-instruct:free, gemini-2.5-flash-lite...
//     max_tokens                     max_completion_tokens
//
// Los modelos con sufijo  :free  no consumen saldo. La configuracion de abajo viene con
// los tres gratis puestos por defecto, asi que el asistente funciona sin gastar credito.
// ==============================================================================================================================================
// HARDWARE
// ==============================================================================================================================================
// - Microcontrolador ESP32-S3 (PercuSynth). Se recomienda modulo CON PSRAM.
//
// - DAC PCM5102 por I2S  ->  I2S_NUM_0 (SALIDA / TX):
//       I2S LCK / LRCK ... GPIO 39
//       I2S DIN / DATA ... GPIO 40   (DIN del DAC = DOUT del ESP32)
//       I2S BCK / BCLK ... GPIO 41
//
// - Microfono INMP441 por I2S  ->  I2S_NUM_1 (ENTRADA / RX):
//       WS  (LRCL) ....... GPIO 11
//       SCK (BCLK) ....... GPIO 12
//       SD  (DOUT) ....... GPIO 13   (SD del micro = DIN del ESP32)
//       L/R .............. GND       (dato en el slot IZQUIERDO)
//       VDD .............. 3.3V      (NO 5V)
//       GND .............. GND
//
// - Boton grabar ...... BTN1 = GPIO 44 (INPUT_PULLUP, presionado = LOW)
// - Indicadores ....... 6 LEDs SMD WS2812 on-board (data GPIO 46), color = estado
// ==============================================================================================================================================
// ARDUINO IDE SETTINGS
// ==============================================================================================================================================
// - Placa:        ESP32S3 Dev Module
// - Flash Mode:    DIO            (IMPORTANTE en este hardware para que el I2S funcione bien)
// - PSRAM:         OPI PSRAM      (habilitar: el buffer de grabacion son ~160 KB)
// - USB CDC On Boot: habilitado si quieres ver el diagnostico por Serial
// - Upload/Monitor: 115200 baud
// ==============================================================================================================================================
// LIBRERIAS REQUERIDAS
// ==============================================================================================================================================
// - WiFi.h / WiFiClientSecure.h / HTTPClient.h   (core ESP32 Arduino)
// - driver/i2s_std.h                             (core ESP32 Arduino, nuevo driver I2S)
// - FastLED                                      (indicadores WS2812)
// ==============================================================================================================================================
// DESCRIPCION
// ==============================================================================================================================================
// Asistente de voz con CONTEXTO PERSONALIZADO (editable, ver CONTEXT_PERSONALIZADO):
//   1) Graba tu voz por el INMP441 (I2S, 16 kHz mono) mientras mantienes BTN1.
//   2) La transcribe con Whisper en NagaAI (/v1/audio/transcriptions, idioma es).
//   3) Consulta al modelo de chat de NagaAI con tu contexto embebido en FLASH.
//   4) Sintetiza la respuesta (/v1/audio/speech) y la reproduce por el PCM5102.
//
// Diferencia de fondo con asistente_ia en la reproduccion: aquel pedia  response_format=pcm
// y asumia 24 kHz fijos. Aqui se pide  wav  y se LEE LA CABECERA: sample rate, canales y
// bits vienen en el propio archivo, y el I2S se reconfigura a esa frecuencia. Asi el sketch
// sigue funcionando si cambias de modelo de voz (los de ElevenLabs no entregan 24 kHz).
// Si la respuesta no trae cabecera RIFF se reproduce como PCM crudo a TTS_RATE_POR_DEFECTO;
// si llega un MP3 se avisa por Serial en vez de reproducir ruido.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
//   LED VERDE  = LISTO       -> manten BTN1 y habla (max 5 s)
//   LED ROJO   = GRABANDO
//   LED AMBAR  = PROCESANDO   (transcripcion / chat)
//   LED CIAN   = HABLANDO     (reproduciendo respuesta)
//   LED MAGENTA (parpadeo) = ERROR (sin WiFi / sin RAM)
//
//   Con MOSTRAR_ESTADO en 1 se imprime por Serial (115200) todo el recorrido: lo que
//   entendio, lo que respondio, los codigos HTTP y el formato de audio recibido. Es la
//   forma de descubrir rapido si un modelo no existe o si la clave esta mal.
// ==============================================================================================================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "driver/i2s_std.h"
#include <FastLED.h>

// ==================== CONFIGURACION USUARIO ====================

// --- Credenciales (WiFi + NagaAI) ---
// No viven en este archivo. Copia secretos.example.h a secretos.h (misma carpeta del
// sketch) y escribe ahi tus claves. secretos.h esta en .gitignore: nunca se sube al repo.
#if !__has_include("secretos.h")
  #error "Falta secretos.h: copia secretos.example.h a secretos.h y pon tus claves."
#endif
#include "secretos.h"

// --- Diagnostico por Serial (115200). Ponlo en 0 para el uso normal ---
#define MOSTRAR_ESTADO 1

// --- Servidor ---
#define NAGA_HOST  "api.naga.ac"

// --- Modelos ---
// Lista completa y precios:  https://naga.ac/models   o   curl https://api.naga.ac/v1/models
// Los ":free" no consumen saldo. Comprobado en el catalogo de agosto de 2026.
//
//   Transcripcion : whisper-large-v3:free   whisper-large-v3   whisper-large-v3-turbo
//                   gpt-4o-transcribe       scribe-v1
//   Chat          : nemotron-3-ultra-550b-a55b:free   llama-3.3-70b-instruct:free
//                   llama-4-scout-17b-16e-instruct:free   gemini-2.5-flash-lite   gpt-5-nano
//   Voz           : gpt-4o-mini-tts:free    eleven-multilingual-v2:free   eleven-v3
//
// OJO 1: los nombres de OpenAI (whisper-1, tts-1, gpt-4o-mini) NO existen en NagaAI.
//        Si pones uno inexistente la API responde 404 y el sketch lo imprime por Serial.
// OJO 2: los modelos gratis dependen de la capacidad del proveedor y a ratos devuelven
//        503 ("upstream provider is temporarily unavailable"). No es un fallo del sketch:
//        cambia MODELO_CHAT por otro de la lista. Un 402 significa cuenta sin saldo, y
//        entonces solo funcionan los ":free".
// OJO 3: nemotron-3-super-120b-a12b:free devuelve su RAZONAMIENTO dentro de "content"
//        (el TTS lo leeria en voz alta). El :ultra no lo hace: por eso es el que va aqui.
#define MODELO_STT   "whisper-large-v3:free"
#define MODELO_CHAT  "nemotron-3-ultra-550b-a55b:free"
#define MODELO_TTS   "gpt-4o-mini-tts:free"

// Voz del TTS. Para los modelos de OpenAI: alloy, echo, fable, onyx, nova, shimmer, coral,
// sage, ash. Para los de ElevenLabs va el id de la voz en su catalogo.
#define VOZ_TTS      "alloy"

// El parametro speed solo lo aceptan los TTS de OpenAI. Pon 0 si usas ElevenLabs.
#define TTS_USAR_SPEED 1
#define TTS_SPEED      1.0

// Formato de audio pedido. "wav" trae cabecera y el sketch lee de ahi la frecuencia real
// (lo recomendado). "pcm" ahorra 44 bytes pero obliga a acertar TTS_RATE_POR_DEFECTO.
#define TTS_FORMATO  "wav"

// Idioma que se le pasa a Whisper como pista.
#define IDIOMA_STT   "es"

// ==================== PINES ====================

// I2S salida (DAC PCM5102) - I2S_NUM_0
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41

// I2S entrada (mic INMP441) - I2S_NUM_1
#define MIC_WS    11
#define MIC_SCK   12
#define MIC_SD    13

#define BTN_RECORD 44          // BTN1
#define LED_PIN    46          // WS2812 on-board
#define NUM_LEDS   6

// ==================== AUDIO ====================

#define MIC_RATE   16000       // grabacion + WAV para la transcripcion
#define TTS_RATE_POR_DEFECTO 24000   // solo se usa si la respuesta NO trae cabecera WAV
#define RECORD_SECONDS 5
#define MAX_SAMPLES (MIC_RATE * RECORD_SECONDS)

// Techo del audio de respuesta: ~30 s a 24 kHz mono, ~17 s a 44.1 kHz estereo.
#define MAX_TTS_BYTES 1500000

// El INMP441 es sensible pero la voz queda baja: ganancia digital (ajustable)
#define MIC_GAIN   6

// ==================== ESTADO ====================

i2s_chan_handle_t tx_chan = NULL;   // DAC
i2s_chan_handle_t rx_chan = NULL;   // mic
int16_t* audioBuffer = nullptr;
bool recording = false;
uint32_t dacRateActual = TTS_RATE_POR_DEFECTO;

CRGB leds[NUM_LEDS];

// Descripcion del audio que llego del servidor. Va aqui arriba a proposito: el IDE de
// Arduino genera los prototipos de las funciones ANTES del cuerpo del sketch, asi que un
// tipo propio usado como valor de retorno tiene que estar definido por encima de todo.
struct AudioInfo {
  size_t   offset   = 0;                      // donde empiezan las muestras
  size_t   bytes    = 0;                      // cuantos bytes de muestras hay
  uint32_t rate     = TTS_RATE_POR_DEFECTO;
  uint16_t canales  = 1;
  uint16_t bits     = 16;
  bool     valido   = false;
};

// El log sale por LOS DOS puertos serie a proposito. En la DevKitC-1 hay dos conectores
// USB y el ajuste "USB CDC On Boot" decide cual es `Serial`: con Enabled es el USB NATIVO,
// con Disabled es el UART0 (el del conversor USB-serie, el que se usa para flashear). Si
// imprimieras solo en `Serial` y tuvieras el cable en el otro conector, no verias nada —
// que es exactamente el sintoma facil de confundir con "el firmware no funciona".
#if MOSTRAR_ESTADO
  #if ARDUINO_USB_CDC_ON_BOOT
    #define LOGLN(x)   do { Serial.println(x); Serial0.println(x); } while (0)
  #else
    #define LOGLN(x)   Serial.println(x)
  #endif
#else
  #define LOGLN(x)
#endif


enum State { ST_READY, ST_RECORDING, ST_PROCESSING, ST_SPEAKING, ST_ERROR };

void showState(State s) {
  CRGB c;
  switch (s) {
    case ST_READY:      c = CRGB(0, 60, 0);    break;   // verde
    case ST_RECORDING:  c = CRGB(120, 0, 0);   break;   // rojo
    case ST_PROCESSING: c = CRGB(90, 55, 0);   break;   // ambar
    case ST_SPEAKING:   c = CRGB(0, 45, 80);   break;   // cian
    case ST_ERROR:      c = CRGB(110, 0, 110); break;   // magenta
  }
  fill_solid(leds, NUM_LEDS, c);
  FastLED.show();
}

// Parpadeo magenta permanente + el motivo REPETIDO por Serial. La version anterior
// imprimia el error una sola vez y se quedaba muda para siempre: con USB CDC el PC abre
// el puerto DESPUES de que arranca la placa, asi que ese unico mensaje se perdia justo
// cuando mas falta hacia.
void bloquearConError(const String& motivo) {
  while (1) {
    LOGLN("ERROR: " + motivo);
    for (int k = 0; k < 5; k++) {                  // ~2 s de parpadeo entre avisos
      showState(ST_ERROR);
      delay(200);
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      FastLED.show();
      delay(200);
    }
  }
}

// ==================== CONTEXTO PERSONALIZADO (en FLASH) ====================
//
// ESTO ES UNA PLANTILLA: reemplaza el texto de abajo por la informacion que quieras que
// el asistente sepa. Es lo que hace que responda como TU asistente y no como un modelo
// generico. Escribe en texto plano, en el idioma que vayas a usar.
//
// Ideas de que poner: quien eres o que es tu proyecto, que productos o servicios ofreces,
// como contactarte, y cualquier dato que quieras que repita bien (precios, horarios,
// direccion, nombres). Mientras mas concreto, mejor responde.
//
// Reglas practicas:
//   - No pongas nada privado: esto se compila dentro del firmware y viaja a la API de
//     NagaAI (y de ahi al proveedor del modelo) en cada pregunta.
//   - Sin comillas dobles sin escapar y sin acentos ni enes (el TTS los lee igual, pero
//     el JSON se arma a mano mas abajo y los caracteres raros lo pueden romper).
//   - Cabe holgado en ~2.5 KB de texto. Si lo agrandas, sube tambien `bufferSize` en
//     getCustomContext() y de paso el limite de tokens de la respuesta.
//   - Si lo dejas vacio, el asistente igual funciona: responde con el conocimiento
//     general del modelo.
//
// Cambia tambien el prompt de sistema en preguntarChat() (mas abajo) para que se presente
// como tu asistente y no como uno generico.

const char CONTEXT_PERSONALIZADO[] PROGMEM = R"(
CONTEXTO:

SOBRE MI / SOBRE EL PROYECTO:
[Describe aqui quien eres o que es tu proyecto, en dos o tres frases.]

PRODUCTOS O SERVICIOS:
1. [Nombre]
   - [Que es, en una linea.]
   - [Detalle util: precio, duracion, para quien es.]

2. [Nombre]
   - [Que es, en una linea.]

DATOS QUE DEBE SABER RESPONDER:
- [Horarios, direccion, formas de pago, lo que sea que te pregunten seguido.]

CONTACTO:
[Web, correo, redes.]
)";

String getCustomContext() {
  const int bufferSize = 3072;
  char* buffer = (char*)malloc(bufferSize);
  if (!buffer) return "";
  strcpy_P(buffer, CONTEXT_PERSONALIZADO);
  String context = String(buffer);
  free(buffer);
  return context;
}

// ==================== UTILIDADES JSON ====================
//
// El JSON se arma y se lee a mano (sin ArduinoJson) para no cargar una libreria mas en un
// sketch que ya se pelea la RAM con el audio. Son dos operaciones: escapar lo que sale y
// des-escapar lo que entra.

// Deja un texto listo para ir DENTRO de una cadena JSON.
void escaparJSON(String& s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", " ");
  s.replace("\r", "");
  s.replace("\t", " ");
}

// Convierte los escapes de una cadena JSON a texto normal, incluidos los \uXXXX.
// Sin esto, un proveedor que escape los acentos (á) haria que el TTS leyera
// literalmente "u00e1" en mitad de la frase.
String desescaparJSON(const String& s) {
  String out;
  out.reserve(s.length());
  for (int i = 0; i < (int)s.length(); i++) {
    char c = s.charAt(i);
    if (c != '\\' || i + 1 >= (int)s.length()) { out += c; continue; }
    char n = s.charAt(++i);
    switch (n) {
      case 'n': out += ' ';   break;
      case 'r':               break;
      case 't': out += ' ';   break;
      case 'b': case 'f':     break;
      case '"': out += '"';   break;
      case '\\': out += '\\'; break;
      case '/': out += '/';   break;
      case 'u': {
        if (i + 4 < (int)s.length()) {
          uint16_t cp = (uint16_t)strtol(s.substring(i + 1, i + 5).c_str(), NULL, 16);
          i += 4;
          if (cp < 0x80) {
            out += (char)cp;
          } else if (cp < 0x800) {                       // 2 bytes UTF-8
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
          } else {                                       // 3 bytes UTF-8
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
          }
        }
        break;
      }
      default: out += n; break;
    }
  }
  return out;
}

// Busca "clave":"valor" a partir de 'desde' y devuelve el valor ya des-escapado.
// Tolera espacios tras los dos puntos y respeta las comillas escapadas del interior.
String extraerCadenaJSON(const String& src, const String& clave, int desde = 0) {
  String pat = "\"" + clave + "\"";
  int k = src.indexOf(pat, desde);
  if (k < 0) return "";
  int i = k + pat.length();
  while (i < (int)src.length() && (src.charAt(i) == ' ' || src.charAt(i) == ':')) i++;
  if (i >= (int)src.length() || src.charAt(i) != '"') return "";   // null, numero u objeto
  i++;
  int ini = i;
  while (i < (int)src.length()) {
    char c = src.charAt(i);
    if (c == '\\') { i += 2; continue; }
    if (c == '"') break;
    i++;
  }
  return desescaparJSON(src.substring(ini, i));
}

// ==================== I2S: DAC (salida) ====================

void i2s_dac_init(uint32_t rate) {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;                       // silencio limpio cuando no escribimos
  chan_cfg.dma_desc_num = 8;                         // mas descriptores DMA = mas colchon
  chan_cfg.dma_frame_num = 300;                      // ~80 ms de buffer a 24 kHz (anti-underrun)
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(rate),
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
  dacRateActual = rate;
}

// Cambia la frecuencia del DAC en caliente. Hay que dejar el canal en READY (disable)
// antes de tocar el reloj; si no, i2s_channel_reconfig_std_clock devuelve error.
void setDacRate(uint32_t rate) {
  if (rate == dacRateActual || rate < 8000 || rate > 96000) return;
  i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(rate);
  i2s_channel_disable(tx_chan);
  if (i2s_channel_reconfig_std_clock(tx_chan, &clk) == ESP_OK) dacRateActual = rate;
  i2s_channel_enable(tx_chan);
}

// ==================== I2S: MIC INMP441 (entrada) ====================

void i2s_mic_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));   // solo RX

  // INMP441 = 24 bits dentro de slot de 32. Usamos STEREO 32-bit (64 BCLK/frame,
  // que es lo que el INMP441 espera) y luego nos quedamos con el canal IZQUIERDO.
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

// ==================== GRABAR AUDIO (I2S mic -> audioBuffer) ====================

int recordAudio() {
  showState(ST_RECORDING);

  int32_t raw[256];          // 128 frames estereo (L,R,L,R...)
  size_t bytesRead = 0;
  int n = 0;

  // Descartar ~80 ms iniciales para evitar el "pop" de arranque del DMA
  for (int k = 0; k < 10; k++) {
    i2s_channel_read(rx_chan, raw, sizeof(raw), &bytesRead, 50);
  }

  while (digitalRead(BTN_RECORD) == LOW && n < MAX_SAMPLES) {
    if (i2s_channel_read(rx_chan, raw, sizeof(raw), &bytesRead, 200) != ESP_OK) continue;
    int got = bytesRead / sizeof(int32_t);      // nro de int32 leidos (frames*2)
    for (int i = 0; i < got && n < MAX_SAMPLES; i += 2) {   // i += 2 -> solo canal IZQ
      int32_t s = raw[i] >> 16;                 // 32 -> 16 bits
      s *= MIC_GAIN;                            // ganancia de voz
      if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
      audioBuffer[n++] = (int16_t)s;
    }
  }

  return n;
}

// ==================== TRANSCRIPCION (NagaAI /v1/audio/transcriptions) ====================
//
// multipart/form-data armado a mano: HTTPClient no sabe subir un cuerpo por partes sin
// tenerlo entero en RAM, y el WAV son ~160 KB. Aqui se calcula el Content-Length primero
// y luego se va escribiendo el audio en trozos de 512 bytes.

String transcribirAudio(int sampleCount) {
  if (sampleCount < 1000) return "";

  showState(ST_PROCESSING);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(60);

  if (!client.connect(NAGA_HOST, 443)) {
    LOGLN("[STT] ERROR: no se pudo conectar a " NAGA_HOST);
    return "";
  }

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
  model += "Content-Disposition: form-data; name=\"model\"\r\n\r\n" MODELO_STT;

  String language = "\r\n--" + boundary + "\r\n";
  language += "Content-Disposition: form-data; name=\"language\"\r\n\r\n" IDIOMA_STT;

  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t totalLen = head.length() + 44 + dataSize + model.length() + language.length() + tail.length();

  client.println("POST /v1/audio/transcriptions HTTP/1.1");
  client.println("Host: " NAGA_HOST);
  client.println("Authorization: Bearer " + String(NAGA_API_KEY));
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
    yield();
  }

  client.print(model);
  client.print(language);
  client.print(tail);

  unsigned long timeout = millis() + 30000;
  while (!client.available() && millis() < timeout) delay(50);

  String status = client.readStringUntil('\n');            // "HTTP/1.1 200 OK"

  String response = "";
  bool bodyStarted = false;
  while (client.available() || client.connected()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (!bodyStarted) { if (line == "\r") bodyStarted = true; }
      else response += line;
    } else {
      delay(10);
      if (!client.available()) break;
    }
  }
  client.stop();

  if (status.indexOf("200") < 0) {
    LOGLN("[STT] ERROR HTTP: " + status);
    LOGLN("[STT] " + response.substring(0, 300));
    return "";
  }

  String texto = extraerCadenaJSON(response, "text");
  if (texto.length() == 0) {
    LOGLN("[STT] respuesta sin campo text: " + response.substring(0, 300));
  }
  return texto;
}

// ==================== CHAT (NagaAI /v1/chat/completions) ====================

String preguntarChat(String pregunta) {
  showState(ST_PROCESSING);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, "https://" NAGA_HOST "/v1/chat/completions");
  http.addHeader("Authorization", String("Bearer ") + NAGA_API_KEY);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(45000);

  escaparJSON(pregunta);

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
  sys += "\\n\\nResponde SIEMPRE en espanol latino de forma concisa (maximo 2-3 oraciones cortas), ";
  sys += "porque tu respuesta se reproducira por voz en el PercuSynth, un dispositivo fisico basado en ESP32. ";
  sys += "No uses markdown, listas ni emojis: el texto se lee en voz alta tal cual. ";
  sys += "Si te preguntan algo que no esta en el contexto, responde con tu conocimiento general pero manten el estilo conciso.";

  // NagaAI usa max_completion_tokens (el max_tokens de OpenAI no figura como parametro
  // soportado en su catalogo de modelos).
  String body = "{";
  body += "\"model\":\"" MODELO_CHAT "\",";
  body += "\"messages\":[";
  body += "{\"role\":\"system\",\"content\":\"" + sys + "\"},";
  body += "{\"role\":\"user\",\"content\":\"" + pregunta + "\"}";
  body += "],\"max_completion_tokens\":200,\"temperature\":0.7}";

  int httpCode = http.POST(body);
  String out = "";

  if (httpCode == 200) {
    String response = http.getString();
    // Se busca "content" DESPUES de "message" para no confundirse con campos previos
    // (algunos modelos devuelven razonamiento o anotaciones antes del texto).
    int m = response.indexOf("\"message\"");
    out = extraerCadenaJSON(response, "content", m > 0 ? m : 0);
    if (out.length() == 0) {
      LOGLN("[CHAT] respuesta sin content: " + response.substring(0, 300));
    }
  } else {
    LOGLN("[CHAT] ERROR HTTP " + String(httpCode));
    LOGLN("[CHAT] " + http.getString().substring(0, 300));
  }

  http.end();
  return out;
}

// ==================== TTS (NagaAI /v1/audio/speech) -> DAC I2S ====================

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
      delay(1);
    }
  }
  return got;
}

static uint32_t leerLE32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t leerLE16(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

// Mira los primeros bytes y decide que es. Un WAV se recorre por chunks (algunos
// servidores meten un chunk LIST antes del data) hasta encontrar "fmt " y "data".
AudioInfo analizarAudio(const uint8_t* buf, size_t len) {
  AudioInfo a;
  if (len < 16) return a;

  if (memcmp(buf, "RIFF", 4) == 0 && memcmp(buf + 8, "WAVE", 4) == 0) {
    size_t p = 12;
    bool fmtOk = false;
    while (p + 8 <= len) {
      uint32_t chunkSize = leerLE32(buf + p + 4);
      const uint8_t* id = buf + p;
      if (memcmp(id, "fmt ", 4) == 0 && p + 8 + 16 <= len) {
        a.canales = leerLE16(buf + p + 8 + 2);
        a.rate    = leerLE32(buf + p + 8 + 4);
        a.bits    = leerLE16(buf + p + 8 + 14);
        fmtOk = true;
      } else if (memcmp(id, "data", 4) == 0) {
        a.offset = p + 8;
        // Los TTS mandan el WAV en streaming: la cabecera suele traer un tamano
        // ficticio (0 o 0xFFFFFFFF). Manda lo que realmente se descargo.
        size_t disponible = len - a.offset;
        a.bytes  = (chunkSize == 0 || chunkSize > disponible) ? disponible : chunkSize;
        a.valido = fmtOk && a.bits == 16 && a.canales >= 1 && a.canales <= 2;
        return a;
      }
      p += 8 + chunkSize + (chunkSize & 1);              // los chunks van alineados a par
      if (chunkSize == 0) break;                          // cabecera corrupta: no girar en vano
    }
    return a;
  }

  // MP3: no hay decodificador en este sketch. Mejor avisar que reproducir ruido.
  if (memcmp(buf, "ID3", 3) == 0 || (buf[0] == 0xFF && (buf[1] & 0xE0) == 0xE0)) {
    a.bits = 0;                                          // marca de "formato comprimido"
    return a;
  }
  // OggS (opus) o flac: tampoco.
  if (memcmp(buf, "OggS", 4) == 0 || memcmp(buf, "fLaC", 4) == 0) {
    a.bits = 0;
    return a;
  }

  // Sin cabecera reconocible: se asume PCM crudo s16le mono, como el "pcm" de OpenAI.
  a.offset  = 0;
  a.bytes   = len;
  a.rate    = TTS_RATE_POR_DEFECTO;
  a.canales = 1;
  a.bits    = 16;
  a.valido  = true;
  return a;
}

// Vuelca las muestras al DAC. El ritmo lo marca el DMA (portMAX_DELAY), asi que no hay
// underruns aunque el WiFi este ocupado: para este punto el audio ya esta entero en RAM.
void reproducirPCM(const uint8_t* pcm, const AudioInfo& a) {
  setDacRate(a.rate);

  int16_t stereo[128 * 2];
  size_t frames = a.bytes / (2 * a.canales);
  size_t idx = 0;
  int sframes = 0;
  size_t bw;

  while (idx < frames) {
    const uint8_t* p = pcm + a.offset + idx * 2 * a.canales;
    int16_t l = (int16_t)(p[0] | (p[1] << 8));
    int16_t r = (a.canales == 2) ? (int16_t)(p[2] | (p[3] << 8)) : l;
    idx++;
    stereo[sframes * 2]     = l;
    stereo[sframes * 2 + 1] = r;
    sframes++;
    if (sframes >= 128) {
      i2s_channel_write(tx_chan, stereo, sframes * 2 * sizeof(int16_t), &bw, portMAX_DELAY);
      sframes = 0;
    }
  }
  if (sframes > 0) {
    i2s_channel_write(tx_chan, stereo, sframes * 2 * sizeof(int16_t), &bw, portMAX_DELAY);
  }

  // cola de silencio para vaciar el DMA sin "click" final
  memset(stereo, 0, sizeof(stereo));
  for (int k = 0; k < 4; k++) {
    i2s_channel_write(tx_chan, stereo, sizeof(stereo), &bw, portMAX_DELAY);
  }
}

bool hablarTexto(String text) {
  showState(ST_SPEAKING);
  WiFi.setSleep(false);

  escaparJSON(text);

  String body = "{\"model\":\"" MODELO_TTS "\",\"input\":\"" + text +
                "\",\"voice\":\"" VOZ_TTS "\",\"response_format\":\"" TTS_FORMATO "\"";
#if TTS_USAR_SPEED
  body += ",\"speed\":" + String(TTS_SPEED, 2);
#endif
  body += "}";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);
  if (!client.connect(NAGA_HOST, 443)) {
    LOGLN("[TTS] ERROR: no se pudo conectar a " NAGA_HOST);
    WiFi.setSleep(true);
    return false;
  }

  // ---- Enviar request a mano ------------------------------------------------
  client.print("POST /v1/audio/speech HTTP/1.1\r\n");
  client.print("Host: " NAGA_HOST "\r\n");
  client.print("Authorization: Bearer " + String(NAGA_API_KEY) + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: " + String(body.length()) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  // ---- Leer status + cabeceras (detectar chunked) ---------------------------
  unsigned long to = millis() + 30000;
  while (!client.available() && millis() < to) delay(10);

  String status = client.readStringUntil('\n');
  bool ok200 = status.indexOf("200") >= 0;

  bool chunked = false;
  String tipo = "";
  while (true) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() <= 1) break;         // fin de cabeceras
    String low = line; low.toLowerCase();
    if (low.indexOf("transfer-encoding") >= 0 && low.indexOf("chunked") >= 0) chunked = true;
    if (low.startsWith("content-type")) tipo = line;
  }

  if (!ok200) {                                            // error de API: el cuerpo es JSON
    String err = "";
    unsigned long t = millis();
    while ((client.connected() || client.available()) && err.length() < 400 && millis() - t < 5000) {
      if (client.available()) err += (char)client.read(); else delay(2);
    }
    client.stop();
    WiFi.setSleep(true);
    LOGLN("[TTS] ERROR HTTP: " + status);
    LOGLN("[TTS] " + err);
    return false;
  }

  // ---- FASE 1: descargar TODO el audio a PSRAM (decodificando chunked) ------
  // El servidor entrega el audio con Transfer-Encoding: chunked. Hay que quitar las
  // cabeceras de tamano de cada chunk; si no, esos bytes ASCII se meten en el audio
  // y suenan como ruido fuerte. (getStreamPtr NO hace esto por nosotros.)
  uint8_t* pcm = (uint8_t*)ps_malloc(MAX_TTS_BYTES);
  if (!pcm) pcm = (uint8_t*)malloc(MAX_TTS_BYTES);
  if (!pcm) {
    client.stop(); WiFi.setSleep(true);
    LOGLN("[TTS] ERROR: sin memoria para el audio");
    return false;
  }

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
      pcmLen += readExact(client, pcm + pcmLen, toRead);

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
    // sin chunked: leer hasta que cierre la conexion
    while (client.connected() || client.available()) {
      if (client.available()) {
        size_t room = MAX_TTS_BYTES - pcmLen;
        if (room == 0) break;
        int r = client.read(pcm + pcmLen, room);
        if (r > 0) pcmLen += r;
      } else delay(2);
    }
  }

  client.stop();
  WiFi.setSleep(true);

  // ---- FASE 2: identificar el formato y reproducir --------------------------
  AudioInfo a = analizarAudio(pcm, pcmLen);

  if (a.bits == 0) {
    LOGLN("[TTS] llego audio COMPRIMIDO (" + tipo + ") y este sketch no lo decodifica.");
    LOGLN("[TTS] revisa TTS_FORMATO (wav o pcm) y que el modelo lo soporte.");
    free(pcm);
    return false;
  }
  if (!a.valido || a.bytes < 2000) {
    LOGLN("[TTS] audio no reproducible: " + String(pcmLen) + " bytes, bits=" + String(a.bits) +
          ", canales=" + String(a.canales));
    free(pcm);
    return false;
  }

  LOGLN("[TTS] " + String(a.bytes) + " bytes @ " + String(a.rate) + " Hz, " +
        String(a.canales) + " canal(es)");

  reproducirPCM(pcm, a);
  free(pcm);
  return true;
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial0.begin(115200);                 // UART0: el conector del conversor USB-serie
#endif
  Serial.setDebugOutput(false);
  esp_log_level_set("*", ESP_LOG_NONE);

  // Con USB CDC el puerto lo abre el PC despues de que la placa arranca: si imprimimos de
  // inmediato, el arranque se pierde. Se espera al Monitor Serie hasta 3 s (si no hay
  // nadie escuchando, sigue igual: el firmware no depende del Serial para funcionar).
  unsigned long tSerie = millis();
  while (!Serial && millis() - tSerie < 3000) delay(10);
  delay(200);

  LOGLN("");
  LOGLN("PercuSynth - asistente NagaAI  [arrancando]");

  setCpuFrequencyMhz(240);

  pinMode(BTN_RECORD, INPUT_PULLUP);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(40);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // Buffer de grabacion: preferimos PSRAM (son ~160 KB), con fallback a RAM interna.
  audioBuffer = (int16_t*)ps_malloc(MAX_SAMPLES * sizeof(int16_t));
  if (!audioBuffer) audioBuffer = (int16_t*)malloc(MAX_SAMPLES * sizeof(int16_t));
  if (!audioBuffer) bloquearConError("sin memoria para el buffer de grabacion (habilita PSRAM: OPI)");

  i2s_dac_init(TTS_RATE_POR_DEFECTO);
  i2s_mic_init();

  // WiFi
  LOGLN("Conectando a WiFi: " + String(WIFI_SSID));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) { delay(500); attempts++; }

  if (WiFi.status() != WL_CONNECTED) {
    bloquearConError("no se pudo conectar al WiFi (status " + String(WiFi.status()) + ")");
  }

  WiFi.setSleep(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  LOGLN("");
  LOGLN("PercuSynth - asistente NagaAI");
  LOGLN("  IP   : " + WiFi.localIP().toString());
  LOGLN("  host : " NAGA_HOST);
  LOGLN("  STT  : " MODELO_STT);
  LOGLN("  chat : " MODELO_CHAT);
  LOGLN("  voz  : " MODELO_TTS " / " VOZ_TTS " / " TTS_FORMATO);
  LOGLN("Manten BTN1 y habla.");

  showState(ST_READY);
}

// ==================== LOOP ====================

void loop() {
  if (digitalRead(BTN_RECORD) == LOW && !recording) {
    delay(50);                                    // antirrebote
    if (digitalRead(BTN_RECORD) == LOW) {
      recording = true;

      int sampleCount = recordAudio();            // graba mientras BTN1 este presionado
      LOGLN("[MIC] " + String(sampleCount) + " muestras (" +
            String(sampleCount / (float)MIC_RATE, 1) + " s)");

      // Cada rama dice por que se detiene. Si no, un fallo se ve solo como "la luz
      // vuelve a verde" y no hay forma de saber en cual de los tres pasos se cayo.
      if (sampleCount <= 1000) {
        LOGLN("[MIC] grabacion demasiado corta: manten BTN1 mientras hablas");
      } else {
        String pregunta = transcribirAudio(sampleCount);
        if (pregunta.length() == 0) {
          LOGLN("[STT] sin transcripcion: no se sigue");
        } else {
          LOGLN("[YO] " + pregunta);
          String respuesta = preguntarChat(pregunta);
          if (respuesta.length() == 0) {
            LOGLN("[CHAT] sin respuesta: no se sigue");
          } else {
            LOGLN("[IA] " + respuesta);
            hablarTexto(respuesta);
          }
        }
      }

      recording = false;
      showState(ST_READY);
    }
  }
  delay(10);
}
