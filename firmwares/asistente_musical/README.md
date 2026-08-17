# asistente_musical — pad generativo + voz IA con pasa-altos

Conversas con GPT por voz **mientras suena una cama armónica que nunca se detiene**. La voz
de GPT entra al mismo mezclador que el pad, con un **filtro pasa-altos al potenciómetro** y
sidechain sobre la música.

Es el cruce de dos firmwares que ya existían: la cadena de red de
[`asistente_ia`](../asistente_ia/) (Whisper → GPT → TTS) y el motor de voces estéreo de
[`pads_imu`](../pads_imu/).

---

## Lo que cambia respecto a `asistente_ia`

`asistente_ia` tenía el DAC clavado a **24 kHz** porque su única fuente de audio era el TTS, y
todo el flujo vivía en `loop()` de forma bloqueante: mientras el chip hablaba con OpenAI no
hacía absolutamente nada más. Aquí hay dos cambios de fondo:

**1. Un solo dueño del I2S.** Existe un mezclador a **44100 Hz** que es el único que llama a
`i2s_channel_write()`. El pad, el arpegio y la voz de GPT son tres fuentes que entran a ese
mezclador. La voz llega a 24 kHz y se **resamplea a 44.1 kHz** con interpolación lineal
(avance de fase `24000/44100 = 0.5442`).

**2. Dos tareas, dos cores.** El problema real nunca fue la CPU, fue el **bloqueo**: WiFi +
TLS + esperar a OpenAI son segundos enteros.

| Core | Tarea | Prioridad | Qué hace |
|---|---|---|---|
| 1 (APP_CPU) | `audioTask` | 10 | Lee pots y botones, avanza la progresión, sintetiza, mezcla y escribe al DMA. Nunca toca WiFi, ni `String`, ni `malloc`. |
| 0 (PRO_CPU) | `assistantTask` | 3 | Graba, Whisper, GPT, descarga del TTS. Es donde ya vive el stack WiFi. |
| 1 | `loop()` | 1 | Solo los 6 LEDs, a ~30 FPS. |

A favor: como el arpegio es autónomo y **ningún botón dispara sonido**, la latencia no
importa. Eso permite dejar la cola DMA en `12 × 256 ≈ 70 ms`, que es el colchón que aguanta
el handshake TLS sin un solo glitch.

---

## Controles

### Botones

| Botón | Pin | Acción |
|---|---|---|
| **BTN1** | 44 | **Mantener** = grabar tu voz (máx. 5 s). La música **no se corta**, solo baja ~5 dB |
| **BTN2** | 42 | **Canción nueva**: re-sortea tonalidad, modo y progresión |
| **BTN3** | 0 | Forma de onda del pad: Seno → Sierra → Cuadrada → Triangular |
| **BTN4** | 45 | Tipo de arpegio: UP / DOWN / UP-DOWN / DOWN-UP / RANDOM / CHORD |
| **BTN5** | 47 | **Mantener** = Panel B |

### Panel A — pots sueltos, es donde vives

| Pot | ADC | Función |
|---|---|---|
| POT1 | 1 | **Cutoff** del filtro del pad (200 Hz – 9 kHz) |
| POT2 | 2 | **Resonancia (Q)** del pad (0.7 – 12) |
| POT3 | 8 | **Pasa-altos de la voz de GPT** (20 Hz limpio → 2 kHz teléfono/megáfono) |
| POT4 | 10 | **Volumen del pad** |

### Panel B — manteniendo BTN5

| Pot | ADC | Función |
|---|---|---|
| POT1 | 1 | **Profundidad del ducking** (0 = sin sidechain → 1 = la música casi mutea) |
| POT2 | 2 | Volumen de la voz de GPT |
| POT3 | 8 | Velocidad del arpegio (2 – 16 notas/s) |
| POT4 | 10 | Volumen del arpegio (en 0 = solo queda el pad) |

**Pots congelados.** Al entrar o salir del Panel B los cuatro pots quedan congelados y cada
uno retoma el control solo cuando lo **mueves** (≥ 4 %, tres lecturas seguidas). Sin esto, al
soltar BTN5 el cutoff pegaría un salto a donde hubiera quedado el pot del panel B. Es el mismo
patrón de `pads_imu` / `cyber_kit` / `sampler_ia`.

### LEDs (6 SMD on-board)

| LED | Muestra |
|---|---|
| 0 | Estado: **verde** listo · **rojo** grabando · **ámbar** procesando · **cian** hablando · **magenta parpadeante** error |
| 1–5 | VU: nivel de la **música**, o de la **voz** mientras GPT habla |

---

## El pasa-altos de la voz

Biquad RBJ high-pass con Q = 0.707, coeficientes recalculados por buffer:

```
w0 = 2π·fc/fs        α = sin(w0)/(2Q)
b0 =  (1+cos w0)/2   a0 = 1+α
b1 = -(1+cos w0)     a1 = -2·cos w0
b2 =  (1+cos w0)/2   a2 = 1-α
```

POT3 lo barre de **20 Hz a 2 kHz en escala logarítmica** (`20 · 100^val`), que es como se
escucha lineal al oído. En el extremo izquierdo el filtro es transparente; hacia la derecha
pasa por radio y termina en megáfono.

Dos decisiones que importan:

- **La voz NO pasa por el filtro del pad.** Si pasara, cerrar el cutoff del pad también
  apagaría a GPT. Son cadenas separadas que se suman al final.
- El TTS ya viene a 24 kHz, o sea **no hay nada arriba de 12 kHz de partida**. Todo el
  carácter del efecto lo tiene que hacer el filtro por abajo.

## El ducking

Sidechain real: un seguidor de envolvente sobre la voz baja el bus de música.

- Ataque ~5 ms → la primera sílaba ya despeja.
- Release ~350 ms → la música no "bombea" entre palabra y palabra.
- La envolvente se mide sobre la voz **cruda, antes del pasa-altos**. Si se midiera después,
  subir el cutoff (que quita graves y baja el nivel) soltaría el ducking justo cuando la voz
  está más delgada y más sitio necesita.

## Grabar sin cortar la música

El INMP441 capta lo que sale del PCM5102, así que grabar con el pad sonando es pedirle a
Whisper que separe tu voz de la música. Se resuelve por dos lados a la vez:

**1. Pasa-altos de 250 Hz sobre el micrófono** (biquad RBJ calculado a 16 kHz, no a 44.1 —
usar la tasa equivocada correría el corte casi 3 octavas). El pad tiene casi toda su energía
**debajo** de ese corte: raíz, sub-octava y cuerpo del acorde. La voz apenas lo nota, porque
la inteligibilidad vive entre 300 Hz y 3.4 kHz — es exactamente por lo que la telefonía corta
en 300 Hz y se entiende perfecto. Va **antes** de `MIC_GAIN`, para que el pad no se coma el
headroom y sature la voz.

**2. La música baja a `REC_DUCK` (0.55 ≈ −5 dB)** con un fundido de 120 ms. Sigue sonando
claramente, pero le deja aire al micro.

```cpp
const float REC_DUCK = 0.55f;   // 1.0 = no tocar la música en absoluto
```

Si prefieres que **no cambie nada** al grabar, pon `REC_DUCK = 1.0f`. Si notas que Whisper se
confunde (transcribe frases raras o palabras que no dijiste), bájalo a `0.35f` antes de tocar
cualquier otra cosa. El fundido es de 120 ms y no de 50 como en la primera versión: al no
cortar del todo, un fundido brusco se nota como un tirón en medio del pad.

## Largo de la respuesta y contexto

`max_tokens` es **500** (eran 200, que cortaban a media frase) y el system prompt le pide a
GPT que **ajuste el largo a la pregunta**: 2–3 oraciones si es directa, hasta 10–12 si le
piden explicar o contar sobre el taller. También le prohíbe listas, viñetas y markdown —
suenan pésimo leídos en voz alta.

El techo real no lo pone `max_tokens` sino el buffer: **`MAX_TTS_BYTES` = 4.8 MB ≈ 100 s** de
voz a 24 kHz. Si se llena, la descarga descarta el resto y la frase se corta. Súbelo si te
pasa seguido (hay PSRAM de sobra: en total se usan ~5 MB de 8).

`CONTEXT_PERSONALIZADO` en FLASH es **una plantilla vacía que tienes que llenar**: ahí va lo que
quieras que el asistente sepa (quién eres, qué ofreces, fechas, precios, contacto). Es lo que lo
convierte en *tu* asistente y no en un GPT genérico. Junto con eso, edita el prompt de sistema
unas líneas más abajo, donde dice quién dice ser.

Dos cosas que conviene copiar del uso real: mientras **más concreto** el dato, menos se lo
inventa; y cerrar el contexto con una línea del tipo *"no inventes datos que no aparezcan aquí,
deriva al contacto"* evita que rellene fechas y precios de su cosecha.

> Nada privado ahí adentro: se compila dentro del firmware y viaja a la API de OpenAI en cada
> pregunta. Si publicas tu sketch, publicas ese texto.

> `getCustomContext()` dimensiona el buffer con `strlen_P()`, así que el contexto puede crecer sin
> tocar nada más. Antes era un `malloc(3072)` fijo: al agrandar el texto, `strcpy_P` habría pisado
> memoria en silencio.

---

## La música

Cama armónica generativa (el modo AUTO de `pads_imu`, siempre encendido):

- Sortea tonalidad (12), modo (mayor/menor) y una **progresión diatónica funcional** de 4–8
  acordes, con caminata por grados (`NEXTmaj` / `NEXTmin`) que empieza en la tónica y termina
  en **V** → al loopear cae la cadencia V→i.
- 4/4 a 80 BPM, todos los acordes con la misma duración (8 o 16 negras) → suena a canción.
- Pad estéreo de 32 voces con detune/paneo por voz + sub-octava, LPF biquad resonante.
- Arpegio encima del acorde que suena.

**Sin LFO en el filtro** y **sin IMU**: el cutoff y la resonancia se quedan exactamente donde
los dejas.

---

## Compilar y flashear

| Ajuste | Valor |
|---|---|
| Placa | ESP32S3 Dev Module |
| **Flash Mode** | **DIO** — obligatorio en este hardware para que el I2S suene bien |
| **PSRAM** | **OPI PSRAM** — obligatorio (~1.4 MB de buffers) |
| Upload/Monitor | 115200 baud |

Librerías: core ESP32 Arduino (WiFi, `driver/i2s_std.h`) + **FastLED**.

Antes de compilar, copia `secretos.example.h` a `secretos.h` (misma carpeta del sketch) y
rellena ahí tus claves:

```cpp
const char* WIFI_SSID      = "TU_RED_WIFI";
const char* WIFI_PASS      = "TU_CLAVE_WIFI";
const char* OPENAI_API_KEY = "sk-TU_API_KEY_AQUI";
```

> `secretos.h` está en el `.gitignore` del repositorio: nunca se sube a GitHub. Si falta, el
> sketch no compila y te avisa con un `#error` en vez de fallar en tiempo de ejecución.

### Hardware extra

Además del PercuSynth estándar necesitas el **micrófono INMP441** por I2S:

| INMP441 | GPIO |
|---|---|
| WS (LRCL) | 11 |
| SCK (BCLK) | 12 |
| SD (DOUT) | 13 |
| L/R | GND (dato en el slot izquierdo) |
| VDD | **3.3 V** (no 5 V) |

---

## Notas técnicas

- **Los 4 pots son de ADC1** (GPIO 1/2/8/10). Es lo que hace viable todo esto: con la radio
  encendida el **ADC2 queda inservible**, así que un firmware con WiFi *y* potenciómetros solo
  funciona si los pots caen en ADC1. En este pinout caen.
- **Buffers reservados una sola vez en `setup()`**: grabación (~160 KB) + TTS (4.8 MB). Pedir
  4.8 MB en caliente, con el mezclador corriendo, es buscarse un glitch. Requiere un módulo
  con **8 MB de PSRAM** (el OPI estándar); con 2 MB no arranca y parpadea magenta.
- **El TTS de OpenAI llega con `Transfer-Encoding: chunked`.** Hay que quitar la cabecera de
  tamaño de cada chunk a mano; si no, esos bytes ASCII se cuelan en el PCM y suenan como ruido
  fuerte. (`getStreamPtr` no lo hace por ti.) Heredado tal cual de `asistente_ia`.
- **Traspaso entre tareas sin mutex**: hay un solo productor y un solo consumidor por
  dirección. La tarea de red llena `ttsBuf`, escribe `ttsBytes` y recién al final levanta
  `ttsActive`; la de audio lo consume y baja la bandera al terminar. Un mutex dentro del render
  sería justo lo que no queremos.
- **Anti-denormal**: se suma un DC ínfimo (1e-18) antes de cada biquad. En la FPU del ESP32 los
  números denormales son lentísimos y aparecen como glitches cuando el pad se apaga.
- **Sin red la música igual arranca.** Si el WiFi falla, el LED 0 queda magenta y la tarea del
  asistente reintenta en segundo plano, en vez de bloquear el equipo entero como hacía
  `asistente_ia`.

## Pendientes / ideas

- **El contexto del taller caduca.** Después del 23 de agosto de 2026 hay que sacarlo o
  reemplazarlo por el siguiente: el firmware no tiene reloj en hora, así que no puede saber
  solo que la fecha ya pasó y seguiría anunciándolo como próximo.
- Reproducción en *streaming* (que el mezclador consuma mientras se descarga) en vez de
  descargar entero y luego sonar. Ahorraría 1–2 s de espera, a cambio de un ring buffer y de
  arriesgar cortes si la red se atasca.
- Cadena de radio completa sobre la voz (pasa-altos + pasa-bajos + saturación en un solo pot)
  si el HPF solo se queda corto.
