# pads_imu_leds — Pads Profundos + Arpegio (IMU) + 6 LEDs de placa

Igual que [`pads_imu`](../pads_imu/) (máquina de **pads profundos** polifónicos: cada
botón latchea un acorde sostenido estéreo, con capa de **arpegio** y filtro biquad
barrido por el IMU), **pero con un visualizador reactivo sobre los 6 LEDs WS2812
internos de la placa** (los que `test_leds` deja apagados a propósito).

> Mismo motor de audio que `pads_imu`, con todas sus optimizaciones anti-glitch
> (LUT de semitonos, robo de voz por menor amplitud, tope `ARP_MAX`, anti-denormal).
> El render de LEDs corre en el mismo `loop()` que el audio: como FastLED usa el
> periférico **RMT** (no el I2S) y se refresca throttled (~45 fps, ~180 µs por
> refresco), **no choca con la salida de audio**.

## Qué muestran los 6 LEDs

1. **Paleta = panel activo** (indicador de panel):
   - Panel A (interpretación) → **cian/azul**
   - Panel B (arpegio/armonía) → **violeta**
   - Panel C (timbre/síntesis) → **naranja**
   - Al **cambiar de panel**, un flash blanco breve lo confirma.
2. **Barra (VU)** → energía del **pad**: cuántos LEDs encienden = cuántas voces/cola
   del acorde. Sin acorde activo → respiración suave en el color del panel.
3. **Arpegio** → un **punto que corre** por los 6 LEDs al ritmo del arpegio (avanza
   una posición por nota), en color de contraste. Su velocidad = la del arpegio.

El filtro del IMU desplaza el tono (filter sweep ↔ color sweep).

## Diferencia con `pads_imu`

| | pads_imu | pads_imu_leds |
|---|---|---|
| LEDs | sin LEDs (todo CPU al audio) | **6 LEDs SMD de placa** como visualizador |
| Librería extra | — | **FastLED** |
| Controles / audio | — | **idénticos** |

## Hardware

- ESP32-S3 + DAC **PCM5102** vía I2S (`LCK 39 · DIN 40 · BCK 41`)
- **MPU6050** por I2C (`SDA 21 · SCL 38`, `0x68`) — controla el filtro
- **6 LEDs WS2812** internos (`DATA 46`)
- 5 botones (pull-up interno) · 4 potenciómetros

## Controles

Idénticos a [`pads_imu`](../pads_imu/README.md) — revisa ahí el detalle de los 3
paneles, los bancos de acordes y el arpegio. Resumen:

- **Panel A** (cian): BTN1..5 = los 5 acordes del banco (**BTN1+BTN3 = 6º acorde**) · POT1 ataque · POT2 **volumen del pad** · POT3 release · POT4 movimiento.
- **Panel B** (violeta, `BTN1+BTN5`): arpegio (POT1 volumen — 0 = off · POT2 velocidad · POT3 rango · POT4 gate) · BTN1/5 transpose · BTN2/4 octava · **BTN3 cambia banco**.
- **Panel C** (naranja, `BTN2+BTN4`, todo en vivo): BTN1 arpegio ◀ · BTN2 onda (Seno→Sierra→Cuadrada→Triangular) · BTN3 sub · BTN4 quinta · BTN5 arpegio ▶ · POT1 detune · POT2 tono · POT3 cutoff · POT4 Q. Tipos de arpegio: UP · DOWN · UP-DOWN · DOWN-UP · RANDOM · CHORD.

## Compilar y flashear (Arduino IDE)

- Board: **ESP32S3 Dev Module**
- USB CDC On Boot: **Enabled**
- Flash Mode: **DIO** (¡OPI rompe el I2S!)
- PSRAM: **OPI PSRAM**
- Librerías: ESP32 Arduino core ≥ 3.x + `Wire.h` (incluida) + **FastLED** (gestor de librerías)

## Ajustes rápidos (LEDs)

| Síntoma | Ajuste |
|---|---|
| Colores invertidos | `COLOR_ORDER` GRB → RGB |
| Brillo | `LED_BRIGHT` (0–255, def 250) |
| LEDs "lentos" o glitch de audio | sube `LED_REFRESH_MS` (def 22 ms) |
