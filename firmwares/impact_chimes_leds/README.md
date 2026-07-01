# impact_chimes_leds

Hermano de **impact_chimes** con **show de luces WS2812** y **3 timbres seleccionables**, todo en una sola escala mágica: **C Lydia** (Do mayor con la 4ª aumentada → sonido soñador, "de película").

Se apoya el PercuSynth en el **piso**: al golpear cerca, el acelerómetro detecta el impacto → dispara una nota de la escala (caminata melódica suave) **y** un efecto de luz reactivo sobre la tira de 68 LEDs.

## Controles

| Control | Acción |
|---|---|
| **Golpe en el piso** (IMU) | Dispara nota + luz. Más fuerte = más volumen y brillo. |
| **BTN1** (44) | Efecto de luz **anterior** |
| **BTN5** (47) | Efecto de luz **siguiente** |
| **BTN2** (42) | Timbre **CAMPANA** (el clásico de impact_chimes) |
| **BTN3** (0)  | Timbre **MARIMBA** (fundamental + 4º armónico, ataque seco, cola corta leñosa) |
| **BTN4** (45) | Timbre **GUITARRA ELÉCTRICA** (sierra + overdrive + vibrato, sostenido largo) |
| **POT1** (ADC1)  | Ataque |
| **POT2** (ADC2)  | Decay/cola (la marimba lo recorta, la guitarra lo alarga) |
| **POT3** (ADC8)  | Brillo (cutoff del filtro paso-bajos) |
| **POT4** (ADC10) | Timbre: morph (campana) · dureza del mazo (marimba) · drive (guitarra) |

## Efectos de luz (5, ciclables con BTN1/BTN5)

0. **Onda** — onda simétrica que se expande desde el centro a cada golpe (estilo de la referencia de Omar).
1. **Cometa** — cabezas brillantes que vuelan del centro a los extremos con estela.
2. **Pulso** — toda la tira late con el color de la nota; respiración suave en reposo.
3. **Chispas** — polvo mágico en posiciones aleatorias a cada golpe.
4. **Arcoíris** — arcoíris que se desplaza; brillo y velocidad pulsan con la energía.

El **color** de cada nota va por una paleta mágica (cian → violeta → magenta) según la altura tocada.

## Hardware

- ESP32-S3 + PCM5102 (I2S, audio estéreo 44.1 kHz / 16-bit) + MPU6050 (acelerómetro ±8g).
- **Tira WS2812 de 68 LEDs en el pin de datos 46.** Si tu tira tiene otro largo, cambia `NUM_LEDS`.
- FastLED corre por el periférico **RMT** (no choca con el I2S del audio). `FastLED.show()` está throttleado a ~30 FPS para no robarle tiempo a los buffers de audio.

## Ajustes finos (en el `.ino`)

- **Sensibilidad del golpe:** `HIT_HIGH` (↓ = más sensible).
- **Doble disparo por rebote:** sube `RETRIG_RATIO` (0.85) o `FLOOR_DECAY` (0.970).
- **Brillo de la tira:** `LED_BRIGHT` (150).
- **Largo de la tira:** `NUM_LEDS` (68) — el centro de los efectos es `NUM_LEDS/2`.

## Arduino IDE

Board **ESP32S3 Dev Module** · USB CDC On Boot **Enabled** · **Flash Mode DIO** (¡OPI rompe el I2S!) · PSRAM **OPI PSRAM**.
Librería extra: **FastLED** (gestor de librerías de Arduino).
