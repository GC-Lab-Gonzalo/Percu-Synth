# oscilador_escalas — Dron de 4 osciladores por escala (IMU → filtro)

Port del **`Oscilador_4_escalas` del [Proto-Synth v2](https://github.com/GC-Lab-Gonzalo/proto-synth-v2)**
al hardware del Percu-Synth, **sin Mozzi**: motor de síntesis propio a **44.1 kHz ·
16 bit · estéreo** por I2S hacia el **PCM5102**.

La idea original queda tal cual — **cuatro potenciómetros = cuatro osciladores**, cada
uno cuantizado a la escala activa — y los controles son **directos**: un botón, una
función. Sin paneles ni combos. Lo que cambia es todo lo que hay debajo.

## Controles

| Control | Qué hace |
|---|---|
| **POT1..POT4** | Nota del oscilador 1..4, cuantizada a la escala (**4 octavas, 28 notas**). Al mínimo (< 2 % del recorrido) ese oscilador queda **en silencio**, igual que en el original |
| **BTN1** | **Escala** siguiente (ciclo de 10) |
| **BTN2** | **Octava** global (ciclo de 4: −1 · 0 · +1 · +2) |
| **BTN3** | **Intermitencia** ON/OFF — el sonido se corta y vuelve al tempo |
| **BTN4** | **Tap tempo** de la intermitencia (2 golpes o más). Reengancha la fase: el corte cae donde tú marcas. Sin tocarlo, 120 BPM |
| **BTN5** | Toque: **forma de onda** siguiente · Mantenido > 0.8 s: **tónica +1 semitono** |
| **IMU** | El **eje X** (fijo) abre y cierra el filtro pasa-bajos resonante |

Las 10 escalas, en orden de ciclo: **Jónico · Dórico · Frigio · Lidio · Mixolidio ·
Eólico · Locrio · Frigio dominante (flamenca) · Doble armónica (gitana) · Menor
armónica**. Arranca en Lidio, tónica **La**, como el original.

Formas de onda: **sierra** (la del original) · cuadrada · pulso 25 % · triangular · seno.

El tap tempo manda dos cosas a la vez: el corte de la intermitencia y el **tiempo del
delay** (corchea con puntillo del pulso), así el eco siempre queda en tiempo.

## Qué cambia respecto del original

| | Proto-Synth v2 (Mozzi) | Percu-Synth (este) |
|---|---|---|
| Salida | DAC interno 8 bit, ~16 kHz, **mono** | **PCM5102 I2S, 16 bit, 44.1 kHz, estéreo** |
| Oscilador | tabla `saw2048` (aliasing audible) | **sierra PolyBLEP** anti-aliasing |
| Por pot | 1 oscilador | **3 voces de unísono desafinadas** repartidas en el estéreo + **sub** una octava abajo |
| Formas de onda | sólo sierra | 5, en BTN5 |
| Notas | tablas de frecuencias fijas en La | **intervalos**: tónica movible (12) + octava global |
| Escalas | 4 | **10** (las que uso siempre), todas en un botón |
| Filtro | LPF de 8 bit de Mozzi, control por **LDR** | **biquad RBJ resonante estéreo**, control por el **eje X del IMU** |
| Cambio de nota | salto seco | **portamento de 40 ms** + envolvente anti-clic por oscilador |
| Extras | — | **intermitencia con tap tempo**, drive, **delay estéreo ping-pong** sincronizado al tap |
| Indicadores | 4 LEDs on/off | **6 LEDs WS2812** + LED RGB del módulo |

La cadena de audio es: `4 osciladores (3 unísono + sub) → mezcla → drive → LPF
resonante (IMU) → tono → intermitencia → delay estéreo → limitador`.

## Intermitencia

El reloj del tempo **corre siempre**, aunque la intermitencia esté apagada: así puedes
marcar el tempo con BTN4 antes de encenderla (el LED 5 late), y al activarla ya entra
en tiempo. Cada pulso suena la primera mitad y se corta la segunda (50 %), con rampas
de ~3 ms en cada flanco → corta seco pero sin chasquido.

El delay va **después** del corte, así que las colas siguen sonando dentro de los
silencios: eso es lo que hace que la intermitencia respire en vez de sonar a botón.

## LEDs

| LED | Muestra |
|---|---|
| 0–3 | Los 4 osciladores: **color = nota** (recorre el arcoíris a lo largo de las 4 octavas), **brillo = nivel**. Apagado = ese oscilador está en silencio |
| 4 | **Escala** activa (color) + **octava** (brillo: más octava, más brillo) |
| 5 | **Apertura del filtro** (IMU) + **latido del tempo** (fuerte si la intermitencia está activa, tenue si no) |

Flash blanco al cambiar escala, octava, onda o tónica. El **LED RGB del módulo**
(GPIO48) refleja el promedio de la tira, más tenue.

## Hardware

- ESP32-S3 + DAC **PCM5102** vía I2S (`LCK 39 · DIN 40 · BCK 41`)
- **MPU6050** por I2C (`SDA 21 · SCL 38`, dirección `0x68`) → filtro (eje X)
- **6 LEDs WS2812** internos (`DATA 46`) + LED RGB del módulo (`DATA 48`)
- 5 botones con pull-up (`44 · 42 · 0 · 45 · 47`) · 4 potenciómetros (`ADC 1 · 2 · 8 · 10`)

## Compilar y flashear (Arduino IDE)

- Board: **ESP32S3 Dev Module**
- USB CDC On Boot: **Enabled**
- **Flash Mode: DIO** (¡OPI rompe el I2S!)
- PSRAM: OPI PSRAM (no es obligatorio — el delay vive en RAM interna, ~80 KB)
- Librerías: **FastLED** (`Wire.h` y el driver I2S vienen en el core ESP32 ≥ 3.x)

## Notas de implementación

- **Timbre fijo**: como los 4 pots son notas (igual que en el original), el resto de la
  síntesis va con valores fijos elegidos a mano (desafinación 14 cents, ancho 0.6,
  drive 1.6, piso de cutoff 380 Hz, Q 2.2, delay 26 % con realimentación 0.36). Si algo
  suena corto o pasado, se toca en las constantes del bloque "Timbre".
- **Delay**: líneas `int16_t` de 20 000 muestras por canal en RAM interna (453 ms). La
  realimentación pasa por un one-pole que oscurece cada repetición y **cruza de canal**
  (ping-pong).
- **Al cambiar el tempo el delay NO se desafina**: las cabezas de lectura están quietas
  y el cambio de tiempo se hace con un **crossfade de 30 ms entre dos cabezas**. La
  primera versión deslizaba una sola cabeza (efecto cinta) y al marcar el tap el eco se
  iba de tono medio semitono largo — que sonaba, con razón, a que el sintetizador se
  desafinaba. Si alguna vez se quiere ese barrido *a propósito*, es mover `delayA` poco
  a poco en vez de llamar al crossfade.
- **Anti-clic**: cada oscilador tiene su propia envolvente (~25 ms) y, si estaba en
  silencio, la frecuencia se "teletransporta" a la nota nueva en vez de deslizarse
  desde la anterior.
- **Cuantización con histéresis**: el ruido del ADC no hace bailar la nota en los bordes.
- **IMU**: si no responde, se recupera solo — reintenta como mucho cada 3 s y con
  esperas mínimas, para no vaciar el DMA de audio mientras suena.
