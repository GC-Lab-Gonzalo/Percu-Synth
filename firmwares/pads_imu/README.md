# pads_imu — Pads Profundos Polifónicos + Arpegio (IMU)

Hermano **ambient** del [`trance_imu`](../trance_imu/): el **mismo motor de audio**
(I2S 44.1 kHz / 16-bit, osciladores PolyBLEP anti-aliasing, filtro biquad resonante
barrido por el IMU, soft-limiter tipo *tanh*) pero **sin secuenciador ni patrones**.

En su lugar es una **máquina de pads profundos**: cada botón dispara un **acorde
sostenido** (drone) con ataque y release lentos sobre un pool polifónico. Las voces
se reparten en el campo **estéreo** con detune (ensemble) → texturas anchas y
cinematográficas. Encima del pad hay una capa opcional de **arpegio** que recorre
las notas del acorde activo. Mueve el equipo para hacer el clásico *filter sweep*
mientras el pad se sostiene.

## Qué cambia respecto a `trance_imu`

| | trance_imu | pads_imu (este firmware) |
|---|---|---|
| Disparo | secuenciador interno (16 pasos, 16 patrones) | **botones tocan acordes** (latch sostenido) |
| Notas | grados de escala diatónica | **acordes absolutos** (raíz + mayor/menor) |
| Envolvente | attack + decay (la nota se va sola) | **attack → sustain (hold) → release** (pad) |
| Salida | mono | **estéreo** (2 biquads, voces paneadas con detune) |
| Tempo | POT3 = BPM | POT3 = **Release**; el movimiento lo da un **LFO lento** |
| Extra | — | capa de **arpegio** sobre el acorde (Panel B) + **3 bancos** de acordes |

> **Sin LEDs y sin Serial a propósito**: todo el presupuesto de CPU va al audio.

## Bancos de acordes (BTN1..BTN5 en Panel A)

Hay **3 bancos** de 5 acordes. **BTN3 del Panel B** cicla entre ellos:

| Botón | Pin | Banco 0 | Banco 1 | Banco 2 |
|---|---|---|---|---|
| BTN1 | 44 | Do Mayor (C E G) | Do Mayor | Do Mayor |
| BTN2 | 42 | Sol Mayor (G B D) | La menor (A C E) | La menor |
| BTN3 | 0  | La menor (A C E) | Mi menor (E G B) | **Mi7** (E G# B D) |
| BTN4 | 45 | Fa Mayor (F A C) | Fa Mayor | Fa Mayor |
| BTN5 | 47 | Re menor (D F A) | Sol Mayor (G B D) | Sol Mayor |

Son acordes **absolutos** (raíz + intervalos reales), no grados de escala. Tocar un
botón **latchea** su acorde como pad sostenido; **tocar el mismo botón otra vez lo
apaga** (release con cola). Cambiar de acorde hace **cross-fade**.

## Hardware

- ESP32-S3 + DAC **PCM5102** vía I2S (`LCK 39 · DIN 40 · BCK 41`)
- **MPU6050** por I2C (`SDA 21 · SCL 38`, dirección `0x68`) — controla el filtro
- 5 botones (pull-up interno) · 4 potenciómetros

## Controles — tres paneles

Cambio de panel por **combos** (el mismo combo te devuelve al Panel A):
- **BTN1 + BTN5** a la vez → **Panel B** (arpegio / armonía)
- **BTN2 + BTN4** a la vez → **Panel C** (timbre / síntesis)

(Las acciones individuales de los botones de combo se disparan al *soltar* y se ignoran
si fue combo, así presionar dos juntos no dispara su acorde por accidente.)

### Panel A (interpretación)

| Control | Pin | Función |
|---|---|---|
| BTN1..BTN5 | 44/42/0/45/47 | Los **5 acordes del banco activo** (latch: el mismo botón apaga) |
| POT1 | ADC1 | **Ataque** (5 ms percusivo → ~4 s de fundido lento) |
| POT2 | ADC2 | **Volumen del pad** (solo los acordes; el arpegio tiene su propio volumen en Panel B) |
| POT3 | ADC8 | **Release** (~0.2 s corto → ~8 s de cola enorme) |
| POT4 | ADC10 | **Movimiento** (LFO lento: respira el filtro + tremolo) |
| IMU X | — | Cutoff del filtro paso-bajo |
| IMU Y | — | Resonancia (Q) del filtro |

### Panel B (arpegio / armonía) — BTN1+BTN5

El **arpegio** recorre las notas del acorde que esté sonando (queda amarrado a la
armonía) y suena una octava sobre el pad, pasando por el mismo filtro. **POT1 lo
enciende** (en 0 está apagado); el resto de los pots dan forma SOLO al arpegio.

| Control | Pin | Función |
|---|---|---|
| BTN1 | 44 | Transponer **−1 semitono** |
| BTN5 | 47 | Transponer **+1 semitono** |
| BTN2 | 42 | **Octava global −1** |
| BTN4 | 45 | **Octava global +1** |
| BTN3 | 0  | **Cambiar banco** de acordes (0 → 1 → 2) |
| POT1 | ADC1 | **Volumen del arpegio** (en 0 = arpegio APAGADO; independiente del volumen del pad) |
| POT2 | ADC2 | **Velocidad** del arpegio (2 → 16 notas/s) |
| POT3 | ADC8 | **Rango de octavas** del arpegio (1 → 3) |
| POT4 | ADC10 | **Gate** (largo de cada nota: 30 ms staccato → 480 ms casi ligado) |

- Todos los controles del arpegio actúan **en vivo**. Los botones re-disparan el
  acorde activo para oír el cambio de tono/banco al instante.

### Panel C (timbre / síntesis) — BTN2+BTN4

| Control | Pin | Función |
|---|---|---|
| BTN1 | 44 | **Forma de onda**: Sierra → Cuadrada → Triangular (en vivo, sin re-disparar) |
| BTN2 | 42 | **Octava arriba** (+12, brillo) on/off |
| BTN3 | 0  | **Sub-osc** (−12, cuerpo de bajo) on/off *(ON por defecto → pad profundo)* |
| BTN4 | 45 | **Quinta** (+7, power/órgano) on/off |
| BTN5 | 47 | **Shimmer** (+24, capa etérea ancha) on/off |
| POT1 | ADC1 | **Detune/ensemble** — duplica cada nota en 2 voces desafinadas → coro ancho audible (0–40 cents) |
| POT2 | ADC2 | **Tono** (oscuro → brillante: filtro suave, **no satura**) |
| POT3 | ADC8 | **Piso de cutoff** del filtro (el IMU suma encima) |
| POT4 | ADC10 | **Resonancia base** (Q) del filtro (el IMU suma encima) |

- Los **toggles de capa** re-disparan el acorde activo para oír el cambio. La forma
  de onda se lee en vivo, así que afecta a todas las voces al instante.

### Congelado de controles entre paneles

Igual que en `trance_imu`: al cambiar de panel los pots quedan **congelados**; cada
pot **retoma el control solo cuando se mueve** ≥ 2 % desde su posición (inmune al
ruido del ADC) y solo sobre el parámetro del panel activo. El acorde, la armonía
(Panel B) y el timbre (Panel C) **persisten** al volver al Panel A.

## Compilar y flashear (Arduino IDE)

- Board: **ESP32S3 Dev Module**
- USB CDC On Boot: **Enabled**
- Flash Mode: **DIO** (¡OPI rompe el I2S!)
- PSRAM: **OPI PSRAM**
- Librerías: ESP32 Arduino core ≥ 3.x (incluye `driver/i2s_std.h`) + `Wire.h` (incluida)

## Modo de uso

1. Toca un botón (Panel A) → suena su acorde como pad sostenido.
2. Mueve el PercuSynth: el IMU abre/cierra y resuena el filtro.
3. Sube POT3 (Release) para colas largas y POT4 (Movimiento) para que respire.
4. Encadena botones para una progresión (DoM → SolM → Lam → FaM → Rem...).
5. BTN1+BTN5 → Panel B: sube **POT1** para encender el **arpegio** (POT2 velocidad,
   POT3 rango, POT4 gate); transpón / octava con los botones y cambia de **banco** con BTN3.
6. BTN2+BTN4 → Panel C: forma de onda, apila capas (sub, octava, quinta, shimmer) y
   ajusta detune / tono / cutoff / Q.
