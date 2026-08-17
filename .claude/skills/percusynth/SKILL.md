---
name: percusynth
description: Crear, modificar y depurar firmware Arduino para el PercuSynth (ESP32-S3 + DAC PCM5102 + IMU MPU6050 + LEDs WS2812) y las webapps que lo controlan por Web Serial / Web MIDI. Úsala cuando el trabajo toque un .ino de este repositorio, el pinout de la placa, síntesis de audio a 44.1 kHz por I2S, o una herramienta de tools/ que hable con el hardware.
---

# PercuSynth — firmware y herramientas

Laboratorio de electrónica musical de GC Lab Chile: una placa ESP32-S3 con salida de audio I2S,
IMU, botones, pots, piezos y LEDs direccionables. El repositorio son **sketches Arduino
independientes** (`firmwares/`) y **webapps de un solo archivo** (`tools/`, `videogame/`).

## Antes de escribir código

Lee **[`PROMPT_PARA_LA_IA.md`](../../../PROMPT_PARA_LA_IA.md)** en la raíz del repositorio. Es el
documento madre: pinout fijo, ajustes del Arduino IDE, librerías, constantes de audio y los
patrones de código canónicos (init de I2S, botones con debounce, pots con oversampling, lectura
cruda del MPU6050, USB-MIDI, FastLED). Copia y adapta desde ahí en vez de improvisar.

Si vas a modificar un firmware que ya existe, **lee su `README.md` primero**: cada carpeta de
`firmwares/` y `tools/` tiene uno, y ahí están las decisiones de diseño que ya se tomaron y por
qué. Muchas son el resultado de un problema real que se midió — deshacerlas suele reintroducir el
bug.

## Lo que no se negocia

- **El pinout es fijo.** No inventes pines: están en `PROMPT_PARA_LA_IA.md` §2. Los botones son
  `{44, 42, 0, 45, 47}` en ese orden.
- **Flash Mode DIO**, nunca OPI: con OPI el audio se corta y cruje en este hardware.
- **Audio por I2S al PCM5102** a 44.1 kHz / 16-bit estéreo, bloques DMA de 128 muestras. Nada de
  `dacWrite()` ni del DAC interno.
- **Nunca `delay()` dentro del render de audio.**
- **LEDs:** los índices 0..5 son los SMD internos de la placa. Empieza a escribir en el 6 salvo
  que el firmware los use a propósito como indicadores de estado.
- **Con WiFi encendido el ADC2 no se puede leer.** Los 4 pots ya están en ADC1 (1, 2, 8, 10).
- **Credenciales fuera del `.ino`:** van en un `secretos.h` (ignorado por git) con su
  `secretos.example.h` al lado. Ver `PROMPT_PARA_LA_IA.md` §4.1.

## Convenciones del proyecto

- Comentarios y nombres de variables **en español**.
- Todo `.ino` empieza con el **encabezado formato proto-synth-v2**: bloques HARDWARE / ARDUINO IDE
  SETTINGS / LIBRERÍAS REQUERIDAS / DESCRIPCIÓN / FUNCIONAMIENTO separados por líneas de `====`.
  La plantilla exacta está en `PROMPT_PARA_LA_IA.md` §9.
- Las webapps son **HTML de un solo archivo**: CSS y JS inline, sin build, sin npm, sin CDN de
  librerías salvo fuentes.
- Cada firmware o herramienta nueva lleva su propio `README.md`.

## Diseño de controles

Estas reglas vienen de iteraciones con usuarios reales en talleres:

- **Un botón, una función**, y que se oiga **en el flanco de presión**. Nada de ventanas de espera
  para detectar combos: si necesitas combos, dispara igual al presionar y que el combo *deshaga*
  la acción individual con un fast-kill de ~4 ms.
- **Los pots significan siempre lo mismo** y su posición física *es* el valor. Los paneles
  congelados y el pick-up confunden; úsalos solo si el firmware ya los tiene.
- **No inventes paneles ni combos** que no se pidieron. Si hace falta un parámetro más, primero
  considera dejarlo como constante fija en el código.
- **Nada se mueve solo.** Los LFO que generan control autónomo sobre parámetros que el usuario
  cree estar manejando arruinan la sensación de tocar.

## Calidad de audio

- **Ruidoso ≠ distorsionado.** Un timbre sucio se hace con síntesis (parciales inarmónicos, ruido
  filtrado, saturación por bandas), no rompiendo la señal. El bit-crush, el diezmado y el drive en
  el bus producen aliasing: frecuencias que no pertenecen a ninguna nota.
- **Dos envolventes por golpe percusivo** — una para el tono y otra para el ruido. Un transitorio
  de ruido corto sobre un cuerpo largo es lo que separa un bombo de un "pfff".
- Decaimientos exponenciales: un golpe suena ~3·tau. Un tau de 0.4 s convierte el bombo en un
  drone que tapa todo lo demás.
- **PolyBLEP** en toda forma de onda con esquinas (sierra, cuadrada, pulso).
- Bloqueador de continua a la salida, y sidechain del bombo sobre el resto si hay percusión.

## Verificación

No mandes archivos de audio renderizados: la placa está al lado y se escucha ahí. Para validar
lógica de un firmware de audio antes de flashear, se compila el `.ino` en el PC con mocks de
`Arduino.h` / `Wire.h` / `i2s_std.h` y se renderiza a WAV local — eso encuentra NaN, clipping,
notas fuera de escala y voces trabadas que `arduino-cli` no ve.

## Flashear

- **Sin Arduino IDE:** sirve `tools/percu_control/` por HTTP y usa el botón ⚡ FLASH FW
  (ESP Web Tools, Chrome o Edge).
- **Con Arduino IDE:** placa *ESP32S3 Dev Module*, USB CDC On Boot *Enabled*, Flash Mode *DIO*,
  PSRAM *OPI*, monitor a 115200.
