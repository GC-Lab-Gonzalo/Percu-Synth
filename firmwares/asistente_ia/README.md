# asistente_ia — Asistente de voz (Whisper → GPT → TTS)

El PercuSynth como **asistente de voz**: mantienes BTN1, hablas, y te responde por el parlante.
Es el firmware más simple de la familia con IA — el que conviene leer primero si quieres entender
cómo se conecta el hardware a una API de OpenAI.

Port del asistente del **Proto-Synth v2**. La diferencia de fondo: allá el micrófono era analógico
(ADC) y el DAC interno de 8 bits por software; aquí **todo el audio es I2S real** — micrófono
INMP441 a la entrada, DAC PCM5102 a la salida.

> Si lo que quieres es conversar con GPT **mientras suena música que nunca se detiene**, ese es
> [`asistente_musical`](../asistente_musical/), que corre el audio en el core 1 y la red en el core 0.

## Cadena

1. Mantienes **BTN1** y hablas (máx. 5 s) → graba por el INMP441 a 16 kHz mono
2. **Whisper** (`whisper-1`, español) transcribe
3. **GPT-4o-mini** responde, con el contexto de GC Lab Chile embebido en flash
4. **TTS** (`tts-1`, PCM 24 kHz) sintetiza la respuesta y sale por el PCM5102

El DAC se configura directamente a 24 kHz, así que no hay resampleo.

## Estado por color (6 LEDs SMD de la placa)

| Color | Estado |
|---|---|
| Verde | Listo — mantén BTN1 y habla |
| Rojo | Grabando |
| Ámbar | Procesando (Whisper / GPT) |
| Cian | Hablando (reproduciendo la respuesta) |
| Magenta parpadeando | Error (sin WiFi / sin RAM) |

## Hardware extra

Además de la placa base necesitas un **micrófono INMP441** por I2S (`I2S_NUM_1`, entrada):

| Pin del INMP441 | GPIO |
|---|---|
| WS (LRCL) | 11 |
| SCK (BCLK) | 12 |
| SD (DOUT) | 13 |
| L/R | GND (dato en el slot izquierdo) |
| VDD | **3.3 V** (no 5 V) |

## Antes de compilar

Copia `secretos.example.h` a `secretos.h` (esta misma carpeta) y pon ahí tu WiFi y tu
`OPENAI_API_KEY`. `secretos.h` está en el `.gitignore`: nunca se sube al repositorio. Si falta,
el sketch no compila y te avisa con un `#error`.

Ajustes del Arduino IDE: placa **ESP32S3 Dev Module** · Flash Mode **DIO** (crítico para el I2S) ·
**PSRAM: OPI** (el buffer de grabación son ~160 KB) · 115200 baud.

Librerías: **FastLED** (el resto viene en el core ESP32 ≥ 3.x).
