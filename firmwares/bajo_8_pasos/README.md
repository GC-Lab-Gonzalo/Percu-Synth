# bajo_8_pasos — Secuenciador de bajo de 8 pasos (sierra, pentatónica menor)

Secuenciador de **8 pasos** que toca un **bajo monofónico lleno de armónicos**: dos
sierras PolyBLEP desafinadas + una cuadrada una octava abajo, saturación suave y un
filtro pasa-bajos resonante con **envolvente de filtro** (cada nota abre el filtro y
se cierra con el decay). Todas las notas caen en **La pentatónica menor**.

Creado en conjunto con los participantes del **taller abierto de GC Lab en Hive Espacios**.

## Controles

| Control | Qué hace |
|---|---|
| **BTN1** | **Play / Stop**. Play arranca siempre desde el paso 1 y suena en el acto |
| **BTN2** | **Panel A** — POT1..POT4 = nota de los pasos **1..4** |
| **BTN3** | **Panel B** — POT1..POT4 = nota de los pasos **5..8** |
| **BTN4** | **Panel C** — POT1 **volumen** · POT2 **velocidad** · POT3 **decay** · POT4 **cutoff** |
| **BTN5** | **Octava** siguiente (ciclo de 3: La1 · La2 · La3). Cambia también la nota que está sonando |

- **Notas:** cada pot recorre 11 notas de la pentatónica menor (La · Do · Re · Mi · Sol,
  dos octavas más la tónica de arriba). Con el pot **al mínimo** (< 3 %) el paso queda
  **en silencio**.
- **Velocidad:** 60–200 BPM, cada paso es una semicorchea. Arranca en 120.
- **Decay:** 30 ms – 1.2 s. El filtro se cierra un poco antes que el volumen.
- **Cutoff:** 50 Hz – 6 kHz. La resonancia va atada al corte (más resonante abajo).

### Cómo se comportan los pots al cambiar de panel

Al entrar a un panel, **ningún pot cambia nada hasta que lo muevas** (unos 5 % de
recorrido). Desde ese momento la posición física del pot es el valor. Así saltar de
panel A a B o C nunca pisa las notas que ya dejaste en el otro panel.

## LEDs

| LED | Muestra |
|---|---|
| 0–3 | Panel A/B: los 4 pasos del panel (color = nota, apagado = silencio, **blanco = paso sonando**) · Panel C: nivel de cada parámetro |
| 4 | Panel activo: A cian · B violeta · C naranjo |
| 5 | Octava (brillo) + pulso en cada paso mientras suena (fuerte en los pasos 1 y 5). Rojo tenue = detenido |

## Tira externa de 121 LEDs — balas de luz

La tira WS2812 va **encadenada después de los 6 LEDs SMD de la placa, en el mismo pin
(GPIO 46)**: índices 6..126. Aliméntala con 5 V propios y GND común con la placa
(el firmware limita el consumo a 2 A con `LED_MAX_MA`).

- **Cada paso que suena dispara una bala** desde el inicio de la tira que avanza hacia
  el final con una estela que se apaga. Un paso en silencio no dispara.
- **Multicolor:** cada paso tiene su color (arcoíris de 8) y la nota lo corre un poco,
  así que al cambiar notas cambian los colores. La punta sale casi blanca.
- Los **pasos 1 y 5** salen más brillantes y con estela más larga (marcan el compás).
- **La velocidad sigue al tempo:** siempre quedan 15 LEDs entre bala y bala
  (`ESPACIO_BALAS`), o sea la secuencia entera de 8 pasos cabe volando en la tira.
- Movimiento con sub-píxel a ~60 fps, en la tarea de control (core 0): no le quita
  tiempo al audio.

Se ajusta en el `.ino`: `TIRA_LEDS`, `ESPACIO_BALAS`, `HUE_PASO[]` (colores por paso),
`LED_MAX_MA`.

## Cadena de audio

`sierra + sierra (+9 cents) + cuadrada −1 oct → saturación suave → SVF pasa-bajos
(cutoff pot × envolvente de filtro, Q atado al corte) → VCA (ataque 2 ms + decay) →
bloqueador de continua → volumen → limitador suave`

El timbre que no está en un pot vive en el bloque **TIMBRE (fijo)** del `.ino`:
desafinación, nivel de la sub, drive, cuánto abre la envolvente de filtro
(`ENV_FILTRO_OCT`), rango de resonancia.

## Notas de implementación

- **Dos núcleos**: audio en su tarea fijada al core 1, botones/pots/LEDs en el core 0
  a 1 kHz. El control sólo deja pedidos (notas, parámetros, play) que el audio lee en
  el borde del buffer o al disparar cada paso. Sin `Serial`.
- **Secuenciador preciso a la muestra**, y el largo de cada paso se fija al empezarlo:
  girar la velocidad a mitad de un paso no dispara notas de más.
- **Sin clics al encadenar notas**: la fase de los osciladores no se reinicia y el
  ataque sube desde donde esté la envolvente. Paso en silencio y Stop cierran la nota
  con un tau de 25 ms.
- **Filtro SVF (TPT de Zavalishin)**: estable aunque la envolvente mueva el corte muy
  rápido; los coeficientes se recalculan cada 8 muestras.
- DMA corto (4 × 128 ≈ 12 ms) para que Play responda al instante.
- Verificado en simulación en PC: 32 pasos en 4 s a 120 BPM, 53 a 200 BPM, sin NaN ni
  saturación (pico máx. −2.6 dBFS con todo al máximo).

## Hardware

- ESP32-S3 + DAC **PCM5102** vía I2S (`LCK 39 · DIN 40 · BCK 41`)
- **6 LEDs WS2812** internos (`DATA 46`) + **tira WS2812 de 121 LEDs** encadenada después (mismo pin)
- 5 botones con pull-up (`44 · 42 · 0 · 45 · 47`) · 4 potenciómetros (`ADC 1 · 2 · 8 · 10`)

## Compilar y flashear (Arduino IDE)

- Board: **ESP32S3 Dev Module**
- USB CDC On Boot: **Enabled**
- **Flash Mode: DIO** (¡OPI rompe el I2S!)
- Librerías: **FastLED** (el driver I2S viene en el core ESP32 ≥ 3.x)
