# asistente_naga — Asistente de voz sobre NagaAI (una sola clave, modelos gratis)

El mismo asistente de voz de [`asistente_ia`](../asistente_ia/) — mantienes BTN1, hablas, y te
responde por el parlante — pero hablando con **[NagaAI](https://naga.ac/)** en vez de con OpenAI.

NagaAI es un **agregador**: una sola API compatible con OpenAI que enruta a modelos de muchos
proveedores (OpenAI, Google, Meta, DeepSeek, ElevenLabs…) con una sola clave y un solo saldo. Para
este proyecto eso significa dos cosas concretas:

- **Una única `NAGA_API_KEY`** cubre transcripción, chat y voz. No hace falta cuenta de OpenAI ni de
  ElevenLabs aunque acabes usando sus modelos.
- Hay modelos con sufijo **`:free`** que no consumen saldo. El sketch viene con los tres puestos por
  defecto, así que **funciona sin gastar un peso** — que es justo lo que se necesita en un taller
  donde diez placas preguntan a la vez.

## Lo que cambia respecto de `asistente_ia`

Los *endpoints* son idénticos (`/v1/audio/transcriptions`, `/v1/chat/completions`,
`/v1/audio/speech`). Lo que cambia es el host, los nombres de modelo y un parámetro:

| | OpenAI | NagaAI |
|---|---|---|
| Host | `api.openai.com` | `api.naga.ac` |
| Transcripción | `whisper-1` | `whisper-large-v3` — **`whisper-1` no existe aquí** |
| Voz | `tts-1` | `gpt-4o-mini-tts` — **`tts-1` no existe aquí** |
| Chat | `gpt-4o-mini` | `llama-3.3-70b-instruct:free`, `gemini-2.5-flash-lite`, `gpt-5-nano`… — **`gpt-4o-mini` no existe aquí** |
| Límite de respuesta | `max_tokens` | `max_completion_tokens` |

Copiar los nombres de OpenAI tal cual es el error fácil de cometer: la API responde 404 y no suena
nada. El catálogo real siempre se puede consultar sin clave:

```bash
curl https://api.naga.ac/v1/models
```

## Cadena

1. Mantienes **BTN1** y hablas (máx. 5 s) → graba por el INMP441 a 16 kHz mono
2. `whisper-large-v3:free` transcribe (idioma `es`)
3. `llama-3.3-70b-instruct:free` responde, con tu contexto embebido en flash
4. `gpt-4o-mini-tts:free` sintetiza la respuesta y sale por el PCM5102

## La reproducción lee la cabecera, no la adivina

`asistente_ia` pide `response_format=pcm` y asume 24 kHz fijos, porque sabe que del otro lado hay
un solo proveedor. Aquí el modelo de voz es intercambiable — y los de ElevenLabs no entregan
24 kHz — así que el sketch pide **`wav`** y **lee la cabecera RIFF**: frecuencia, canales y bits
vienen en el propio archivo, y el I2S se reconfigura en caliente a esa frecuencia
(`i2s_channel_reconfig_std_clock`). Cambiar de voz no obliga a tocar ninguna constante.

El analizador cubre los tres casos que se dan en la práctica:

- **WAV**: recorre los *chunks* hasta `fmt ` y `data` (algunos servidores meten un `LIST` en medio)
  y, como el audio llega en *streaming*, ignora el tamaño declarado en la cabecera — que suele venir
  en 0 o `0xFFFFFFFF` — y usa lo que realmente se descargó.
- **Sin cabecera**: se asume PCM crudo s16le mono a `TTS_RATE_POR_DEFECTO` (el comportamiento de
  `asistente_ia`), por si prefieres `TTS_FORMATO "pcm"`.
- **MP3 / Ogg / FLAC**: se detectan y **se avisa por Serial** en vez de reproducirlos como ruido.
  Este sketch no lleva decodificador.

## Diagnóstico por Serial

`MOSTRAR_ESTADO 1` (por defecto) imprime a 115200 baud todo el recorrido: qué entendió, qué
respondió, los códigos HTTP con el cuerpo del error, y el formato de audio que llegó. Es la forma
rápida de descubrir que un modelo no existe o que la clave está mal. Ponlo en `0` para el uso normal.

> **El log sale por los dos puertos serie a propósito.** La DevKitC-1 tiene **dos conectores USB**
> y el ajuste *USB CDC On Boot* decide cuál es `Serial`: con **Enabled** es el **USB nativo**, con
> **Disabled** es el **UART0** (el del conversor USB-serie, el que se usa para flashear). Si el
> firmware imprimiera solo en `Serial` y tuvieras el cable en el otro conector, no verías nada —
> un silencio que se confunde con "el firmware no funciona". Por eso se escribe también en
> `Serial0` cuando CDC está habilitado. Ten en cuenta además que **con CDC el puerto se
> re-enumera en cada reset**: hay que reabrir el Monitor Serie después de flashear.
>
> Por lo mismo, el arranque espera hasta 3 s a que el PC abra el puerto antes de imprimir (con USB
> CDC el host abre el puerto *después* de que la placa arranca, así que el banner se perdía), y el
> parpadeo magenta de error **repite el motivo cada ~2 s** en vez de decirlo una sola vez.

```
PercuSynth - asistente NagaAI
  IP   : 192.168.1.42
  host : api.naga.ac
  STT  : whisper-large-v3:free
  chat : llama-3.3-70b-instruct:free
  voz  : gpt-4o-mini-tts:free / alloy / wav
Manten BTN1 y habla.
[YO] que talleres hacen
[IA] Hacemos talleres de electronica musical y de IA visual...
[TTS] 122880 bytes @ 24000 Hz, 1 canal(es)
```

## Estado por color (6 LEDs SMD de la placa)

| Color | Estado |
|---|---|
| Verde | Listo — mantén BTN1 y habla |
| Rojo | Grabando |
| Ámbar | Procesando (transcripción / chat) |
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
`NAGA_API_KEY` (se crea en el panel de [naga.ac](https://naga.ac/)). `secretos.h` está en el
`.gitignore`: nunca se sube al repositorio. Si falta, el sketch no compila y te avisa con un
`#error`.

Ajustes del Arduino IDE: placa **ESP32S3 Dev Module** · Flash Mode **DIO** (crítico para el I2S) ·
**PSRAM: OPI** (el buffer de grabación son ~160 KB y el de la respuesta hasta 1,5 MB) · 115200 baud.

Librerías: **FastLED** (el resto viene en el core ESP32 ≥ 3.x).

## Personalizarlo

`CONTEXT_PERSONALIZADO` (en flash, arriba del sketch) es **una plantilla vacía**: ahí va lo que
quieres que el asistente sepa — quién eres, qué ofreces, horarios, contacto. Cambia también el
prompt de sistema en `preguntarChat()` para que se presente como tu asistente y no como uno
genérico. No pongas nada privado: viaja a NagaAI y de ahí al proveedor del modelo en cada pregunta.

## Cambiar de modelo

Todo está en el bloque `CONFIGURACIÓN USUARIO`:

```cpp
#define MODELO_STT   "whisper-large-v3:free"
#define MODELO_CHAT  "nemotron-3-ultra-550b-a55b:free"
#define MODELO_TTS   "gpt-4o-mini-tts:free"
#define VOZ_TTS      "alloy"
```

Alternativas del catálogo (agosto de 2026):

- **Transcripción**: `whisper-large-v3`, `whisper-large-v3-turbo`, `gpt-4o-transcribe`, `scribe-v1`
- **Chat**: `llama-3.3-70b-instruct:free`, `llama-4-scout-17b-16e-instruct:free`,
  `gemini-2.5-flash-lite`, `gpt-5-nano`
- **Voz**: `eleven-multilingual-v2:free`, `eleven-v3` (voces de ElevenLabs, mejores en español).
  Con ElevenLabs, `VOZ_TTS` es el **id de la voz** en su catálogo (`alloy` da
  `Voice ID 'alloy' not found`) y hay que poner `TTS_USAR_SPEED 0`, porque el parámetro `speed`
  solo lo aceptan los TTS de OpenAI.

> **No uses `nemotron-3-super-120b-a12b:free`**: devuelve su *razonamiento* dentro de `content`
> ("Okay, the user asked…"), y el TTS lo leería en voz alta. El `:ultra` responde limpio.

## Cuando falla: qué significa cada código

Los modelos gratis dependen de la capacidad del proveedor y **se caen a ratos**. El Serial imprime
el código y el cuerpo del error, así que no hay que adivinar:

| Código | Qué es | Qué hacer |
|---|---|---|
| **503** | `The upstream provider is temporarily unavailable` — el modelo gratis está saturado | Cambia `MODELO_CHAT` por otro de la lista; no es un fallo del firmware |
| **402** | La cuenta no tiene saldo | Solo funcionarán los modelos `:free` |
| **404** | El modelo no existe en Naga | Casi siempre por copiar un nombre de OpenAI (`gpt-4o-mini`, `whisper-1`, `tts-1`) |
| **401** | Clave inválida | Revisa `NAGA_API_KEY` en `secretos.h` |

Comprobar el estado de un modelo desde el PC, sin flashear nada:

```bash
curl -s https://api.naga.ac/v1/chat/completions -H "Authorization: Bearer TU_API_KEY" -H "Content-Type: application/json" -d "{\"model\":\"nemotron-3-ultra-550b-a55b:free\",\"messages\":[{\"role\":\"user\",\"content\":\"hola\"}],\"max_completion_tokens\":20}"
```

## Verificación

La lógica de parseo (escapado y des-escapado de JSON, extracción de campos, análisis de la cabecera
de audio) se corrió en el PC antes de flashear, con las funciones **extraídas del propio `.ino`**
para que la prueba no se separe del código: 26 casos, incluidos `\uXXXX` a UTF-8, `content` dentro
de `message` sin confundirse con `reasoning_content`, WAV con `LIST` intermedio, WAV truncado, y
MP3/Ogg detectados como no reproducibles. El sketch compila para `esp32:esp32:esp32s3` con
`PSRAM=opi,FlashMode=dio`.

Un detalle de Arduino que vale recordar: `struct AudioInfo` está definida **arriba del todo** a
propósito. El IDE genera los prototipos de las funciones antes del cuerpo del sketch, así que un
tipo propio usado como valor de retorno tiene que existir por encima de todo o la compilación falla
con `'AudioInfo' does not name a type`.
