// ==============================================================================================================================================
// PERCUSYNTH - ASISTENTE DE VOZ IA (Whisper + GPT-4o-mini + TTS) - GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo Sandoval - GC Lab Chile
// Licencia de Software: MIT License (https://opensource.org/licenses/MIT)
// Licencia de Hardware: CERN Open Hardware Licence v2 - Permissive (CERN-OHL-P)
//
// Port del asistente del Proto-Synth v2 al PercuSynth (ESP32-S3). La diferencia de fondo:
// el Proto-Synth usaba microfono ANALOGICO (ADC) y DAC INTERNO de 8 bits por software.
// Aqui todo el audio es I2S REAL: micro INMP441 (entrada) + DAC PCM5102 (salida).
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
// - USB CDC On Boot: opcional
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
//   2) La transcribe con Whisper (whisper-1, idioma es).
//   3) Consulta a GPT-4o-mini con tu contexto personalizado embebido en FLASH.
//   4) Sintetiza la respuesta con TTS (tts-1, formato pcm 24 kHz) y la reproduce
//      por el DAC PCM5102 (I2S). Sin resamplear: el DAC se configura a 24 kHz.
// ==============================================================================================================================================
// FUNCIONAMIENTO
// ==============================================================================================================================================
//   LED VERDE  = LISTO       -> manten BTN1 y habla (max 5 s)
//   LED ROJO   = GRABANDO
//   LED AMBAR  = PROCESANDO   (Whisper / GPT)
//   LED CIAN   = HABLANDO     (reproduciendo respuesta)
//   LED MAGENTA (parpadeo) = ERROR (sin WiFi / sin RAM)
// ==============================================================================================================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "driver/i2s_std.h"
#include <FastLED.h>

// ==================== CONFIGURACION USUARIO ====================

// --- Credenciales (WiFi + OpenAI) ---
// No viven en este archivo. Copia secretos.example.h a secretos.h (misma carpeta del
// sketch) y escribe ahi tus claves. secretos.h esta en .gitignore: nunca se sube al repo.
#if !__has_include("secretos.h")
  #error "Falta secretos.h: copia secretos.example.h a secretos.h y pon tus claves."
#endif
#include "secretos.h"

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

#define MIC_RATE   16000       // grabacion + WAV para Whisper
#define TTS_RATE   24000       // reproduccion (formato pcm de OpenAI = 24 kHz mono s16le)
#define RECORD_SECONDS 5
#define MAX_SAMPLES (MIC_RATE * RECORD_SECONDS)

// El INMP441 es sensible pero la voz queda baja: ganancia digital (ajustable)
#define MIC_GAIN   6

// ==================== ESTADO ====================

i2s_chan_handle_t tx_chan = NULL;   // DAC
i2s_chan_handle_t rx_chan = NULL;   // mic
int16_t* audioBuffer = nullptr;
bool recording = false;

CRGB leds[NUM_LEDS];

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

// ==================== CONTEXTO PERSONALIZADO (en FLASH) ====================
//
// ESTO ES UNA PLANTILLA: reemplaza el texto de abajo por la informacion que quieras que
// el asistente sepa. Es lo que hace que responda como TU asistente y no como un GPT
// generico. Escribe en texto plano, en el idioma que vayas a usar.
//
// Ideas de que poner: quien eres o que es tu proyecto, que productos o servicios ofreces,
// como contactarte, y cualquier dato que quieras que repita bien (precios, horarios,
// direccion, nombres). Mientras mas concreto, mejor responde.
//
// Reglas practicas:
//   - No pongas nada privado: esto se compila dentro del firmware y viaja a la API de
//     OpenAI en cada pregunta.
//   - Sin comillas dobles sin escapar y sin acentos ni enes (el TTS los lee igual, pero
//     el JSON se arma a mano mas abajo y los caracteres raros lo pueden romper).
//   - Cabe holgado en ~2.5 KB de texto. Si lo agrandas, sube tambien `bufferSize` en
//     getCustomContext() y de paso el limite de tokens de la respuesta.
//   - Si lo dejas vacio, el asistente igual funciona: responde con el conocimiento
//     general del modelo.
//
// Cambia tambien el prompt de sistema en pedirGPT() (mas abajo) para que se presente
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

// ==================== I2S: DAC (salida) ====================

void i2s_dac_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;                       // silencio limpio cuando no escribimos
  chan_cfg.dma_desc_num = 8;                         // mas descriptores DMA = mas colchon
  chan_cfg.dma_frame_num = 300;                      // ~80 ms de buffer a 24 kHz (anti-underrun)
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(TTS_RATE),   // 24 kHz = el TTS de OpenAI
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
    int got = bytesRead / sizeof(int32_t);      // nº de int32 leidos (frames*2)
    for (int i = 0; i < got && n < MAX_SAMPLES; i += 2) {   // i += 2 -> solo canal IZQ
      int32_t s = raw[i] >> 16;                 // 32 -> 16 bits
      s *= MIC_GAIN;                            // ganancia de voz
      if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
      audioBuffer[n++] = (int16_t)s;
    }
  }

  return n;
}

// ==================== WHISPER (transcripcion) ====================

String transcribeAudio(int sampleCount) {
  if (sampleCount < 1000) return "";

  showState(ST_PROCESSING);

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
    yield();
  }

  client.print(model);
  client.print(language);
  client.print(tail);

  unsigned long timeout = millis() + 30000;
  while (!client.available() && millis() < timeout) delay(50);

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

  int textStart = response.indexOf("\"text\":\"");
  if (textStart > 0) {
    textStart += 8;
    int textEnd = response.indexOf("\"", textStart);
    return response.substring(textStart, textEnd);
  }
  return "";
}

// ==================== GPT (con el contexto personalizado) ====================

String askGPT(String question) {
  showState(ST_PROCESSING);

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
  sys += "\\n\\nResponde SIEMPRE en espanol latino de forma concisa (maximo 2-3 oraciones cortas), ";
  sys += "porque tu respuesta se reproducira por voz en el PercuSynth, un dispositivo fisico basado en ESP32. ";
  sys += "Si te preguntan algo que no esta en el contexto, responde con tu conocimiento general pero manten el estilo conciso.";

  String body = "{";
  body += "\"model\":\"gpt-4o-mini\",";
  body += "\"messages\":[";
  body += "{\"role\":\"system\",\"content\":\"" + sys + "\"},";
  body += "{\"role\":\"user\",\"content\":\"" + question + "\"}";
  body += "],\"max_tokens\":200,\"temperature\":0.7}";

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

// ==================== TTS -> DAC I2S (reproduccion) ====================

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

bool speakText(String text) {
  showState(ST_SPEAKING);
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

  // ---- Enviar request a mano ------------------------------------------------
  client.print("POST /v1/audio/speech HTTP/1.1\r\n");
  client.print("Host: api.openai.com\r\n");
  client.print("Authorization: Bearer " + String(OPENAI_API_KEY) + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: " + String(body.length()) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  // ---- Leer status + cabeceras (detectar chunked) ---------------------------
  unsigned long to = millis() + 30000;
  while (!client.available() && millis() < to) delay(10);

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

  // ---- FASE 1: descargar TODO el PCM a PSRAM (decodificando chunked) ---------
  // OpenAI entrega el audio con Transfer-Encoding: chunked. Hay que quitar las
  // cabeceras de tamano de cada chunk; si no, esos bytes ASCII se meten en el
  // PCM y suenan como ruido fuerte. (getStreamPtr NO hace esto por nosotros.)
  const size_t MAX_TTS_BYTES = 1200000;                     // ~25 s de voz a 24 kHz
  uint8_t* pcm = (uint8_t*)ps_malloc(MAX_TTS_BYTES);
  if (!pcm) pcm = (uint8_t*)malloc(MAX_TTS_BYTES);
  if (!pcm) { client.stop(); WiFi.setSleep(true); return false; }

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

  // ---- FASE 2: reproducir desde PSRAM, ritmo lo marca el DMA (sin underrun) --
  int16_t stereo[128 * 2];                                 // 128 frames L,R
  size_t nSamples = pcmLen / 2;                            // muestras mono s16le
  size_t idx = 0;
  int sframes = 0;
  while (idx < nSamples) {
    int16_t s = (int16_t)(pcm[idx * 2] | (pcm[idx * 2 + 1] << 8));
    idx++;
    stereo[sframes * 2]     = s;      // mono -> L
    stereo[sframes * 2 + 1] = s;      // mono -> R
    sframes++;
    if (sframes >= 128) {
      size_t bw;
      i2s_channel_write(tx_chan, stereo, sframes * 2 * sizeof(int16_t), &bw, portMAX_DELAY);
      sframes = 0;
    }
  }
  if (sframes > 0) {                                       // ultimo bloque parcial
    size_t bw;
    i2s_channel_write(tx_chan, stereo, sframes * 2 * sizeof(int16_t), &bw, portMAX_DELAY);
  }

  // cola de silencio para vaciar el DMA sin "click" final
  memset(stereo, 0, sizeof(stereo));
  for (int k = 0; k < 4; k++) {
    size_t bw;
    i2s_channel_write(tx_chan, stereo, sizeof(stereo), &bw, portMAX_DELAY);
  }

  free(pcm);
  return pcmLen > 4000;
}

// ==================== SETUP ====================

void setup() {
  // UART0 inicializado (algunas rutinas del core lo asumen) pero sin imprimir.
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  esp_log_level_set("*", ESP_LOG_NONE);
  delay(500);

  setCpuFrequencyMhz(240);

  pinMode(BTN_RECORD, INPUT_PULLUP);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(40);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // Buffer de grabacion: preferimos PSRAM (son ~160 KB), con fallback a RAM interna.
  audioBuffer = (int16_t*)ps_malloc(MAX_SAMPLES * sizeof(int16_t));
  if (!audioBuffer) audioBuffer = (int16_t*)malloc(MAX_SAMPLES * sizeof(int16_t));
  if (!audioBuffer) {
    while (1) { showState(ST_ERROR); delay(400); fill_solid(leds, NUM_LEDS, CRGB::Black); FastLED.show(); delay(400); }
  }

  i2s_dac_init();
  i2s_mic_init();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) { delay(500); attempts++; }

  if (WiFi.status() != WL_CONNECTED) {
    while (1) { showState(ST_ERROR); delay(400); fill_solid(leds, NUM_LEDS, CRGB::Black); FastLED.show(); delay(400); }
  }

  WiFi.setSleep(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  showState(ST_READY);
}

// ==================== LOOP ====================

void loop() {
  if (digitalRead(BTN_RECORD) == LOW && !recording) {
    delay(50);                                    // antirrebote
    if (digitalRead(BTN_RECORD) == LOW) {
      recording = true;

      int sampleCount = recordAudio();            // graba mientras BTN1 este presionado

      if (sampleCount > 1000) {
        String question = transcribeAudio(sampleCount);
        if (question.length() > 0) {
          String answer = askGPT(question);
          if (answer.length() > 0) speakText(answer);
        }
      }

      recording = false;
      showState(ST_READY);
    }
  }
  delay(10);
}
