# trance_imu_leds — Secuenciador de Trance Polifónico (IMU) + 6 LEDs de placa

Versión de [`trance_imu`](../trance_imu/) que **reactiva los 6 LEDs WS2812 SMD internos
de la placa** (los que [`test_leds`](../test_leds/) mantiene apagados a propósito) como
**visualizador en vivo**. El motor de audio es idéntico: I2S → PCM5102 a 44.1 kHz /
16-bit estéreo, 16 voces, osciladores anti-aliasing PolyBLEP, filtro biquad resonante
controlado por el IMU y soft-limiter.

> Todo corre en un único `loop()` (audio y LEDs comparten hilo), así que **no hay
> concurrencia**. El único cuidado es **throttlear** `FastLED.show()` (~22 ms) para no
> robarle tiempo a los buffers de I2S. En el ESP32-S3 FastLED usa el periférico **RMT**
> (no el I2S) → **no choca** con la salida de audio. Son sólo 6 LEDs: cada refresco
> transmite en ~180 µs por hardware, impacto despreciable.

## Qué muestran los 6 LEDs

Cuatro cosas a la vez:

| Capa | Qué hace |
|---|---|
| **Paleta = panel** | Indica el panel activo: **A** = cian/azul · **B** = violeta · **C** = naranja (paleta de marca GC Lab). Doble función: visualizador **e** indicador de panel. |
| **Barra (VU)** | Cuántos LEDs encienden = **energía polifónica** (suma de las envolventes de las voces activas). Más acorde / más cola de decay → barra más llena. Independiente del volumen master. |
| **Color fino** | El filtro del IMU desplaza el tono: el clásico *filter sweep* del trance se vuelve también un *color sweep*. |
| **Beat** | Cada paso con nota da un destello blanco; el **downbeat** (pasos 0/4/8/12) pega más fuerte. Detenido (**Stop**) → respiración lenta en el color del panel. |

Al **cambiar de panel** un flash breve confirma el cambio.

## Diferencias respecto a `trance_imu`

- `+ #include <FastLED.h>` y init de los 6 LEDs en `setup()`.
- `triggerStep()` avisa a los LEDs cuando dispara nota (flag de beat + downbeat).
- En cada buffer se calcula un seguidor de **energía polifónica** (ataque instantáneo,
  release suave) a partir de las envolventes de las voces.
- `renderLEDs()` (throttled) se llama **después** de volcar el buffer al DMA de I2S.
- **Todo lo demás (controles, paneles, síntesis, IMU) es idéntico.**

## Hardware

- ESP32-S3 + DAC **PCM5102** vía I2S (`LCK 39 · DIN 40 · BCK 41`)
- **MPU6050** por I2C (`SDA 21 · SCL 38`, dirección `0x68`) — controla el filtro
- **6 LEDs WS2812** SMD internos de la placa en `DATA 46` (índices 0..5 de la tira)
- 5 botones (pull-up interno) · 4 potenciómetros

## Controles

Idénticos a [`trance_imu`](../trance_imu/README.md): tres paneles (A normal,
B notas/tonalidad por **BTN1+BTN5**, C timbre/síntesis por **BTN2+BTN4**), pots
congelados entre paneles, IMU → cutoff (X) / resonancia (Y) del filtro. Ver el README
de `trance_imu` para la tabla completa de botones y pots.

## Compilar y flashear (Arduino IDE)

- Board: **ESP32S3 Dev Module**
- USB CDC On Boot: **Enabled**
- Flash Mode: **DIO** (¡OPI rompe el I2S!)
- PSRAM: **OPI PSRAM**
- Librerías: ESP32 Arduino core ≥ 3.x (incluye `driver/i2s_std.h`) + `Wire.h` (incluida)
  + **FastLED** (gestor de librerías)

## Binario precompilado (flashear sin Arduino IDE)

En [`bin/`](bin/) está el firmware ya compilado para el ESP32-S3 con los ajustes de
arriba (ESP32S3 Dev Module · USB CDC On Boot · **DIO** · OPI PSRAM · 4 MB · partición
default · core ESP32 3.3.11 · FastLED 3.10.4):

| Archivo                        | Qué es                                                                 |
|--------------------------------|------------------------------------------------------------------------|
| `trance_imu_leds_esp32s3.bin`  | Imagen **merged** (bootloader + particiones + boot_app0 + app), ~550 KB |
| `manifest.json`                | Descriptor para ESP Web Tools (lo lee el flasheador de `percu_control`) |

Se graba **completa en la dirección `0x0`** — es un solo archivo, no hay que indicar
offsets por separado. Tres formas de cargarlo:

- **Desde el navegador (PC, Chrome/Edge):** copia `trance_imu_leds_esp32s3.bin` a
  `tools/percu_control/firmware/firmware.bin`, sirve esa carpeta
  (`python -m http.server 8000`) y aprieta **⚡ FLASH FW**. Web Serial no existe en
  Chrome para Android ni en iOS, así que desde el teléfono este camino no sirve.
- **Desde el teléfono (Android + cable OTG):** cualquier app de flasheo de ESP32 por USB
  OTG que acepte un `.bin` y una dirección: elige el archivo, dirección `0x0`
  (o `0x00000`), chip ESP32-S3, y graba. Si la placa no entra sola en modo descarga,
  mantén **BOOT** (BTN3 = GPIO 0) mientras conectas el USB.
- **Desde la terminal:** `esptool --chip esp32s3 write-flash 0x0 trance_imu_leds_esp32s3.bin`

Si tocas el `.ino`, hay que recompilar y volver a exportar (`Sketch → Export Compiled
Binary` deja el `*.merged.bin`, que es el mismo formato).

## Ajustes

- Si oyes glitches de audio, sube `LED_REFRESH_MS` (refresca menos seguido).
- `LED_BRIGHT` controla el brillo global de los 6 LEDs (bajo por defecto, 50/255).
- Si los LEDs internos están montados al revés en tu revisión de placa y el color sale
  raro, cambia `COLOR_ORDER` (p. ej. `GRB` → `RGB`).
