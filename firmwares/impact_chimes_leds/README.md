# impact_chimes_leds

Hermano de **impact_chimes** con **show de luces WS2812** y **3 timbres seleccionables**, todo en una sola escala mágica: **C Lydia** (Do mayor con la 4ª aumentada → sonido soñador, "de película").

Se apoya el PercuSynth en el **piso**: al golpear cerca, el acelerómetro detecta el impacto → dispara una nota de la escala (caminata melódica suave) **y** un efecto de luz reactivo sobre la tira de 68 LEDs.

## Controles

| Control | Acción |
|---|---|
| **Golpe en el piso** (IMU) | Dispara nota + luz. Más fuerte = más volumen y brillo. |
| **BTN1** (44) | Efecto de luz **anterior** (desde **Apagado** salta al último, *Barrido*) |
| **BTN5** (47) | Efecto de luz **siguiente** (desde *Barrido* vuelve a **Apagado**) |
| **BTN2** (42) | Timbre **CAMPANA** (el clásico de impact_chimes) |
| **BTN3** (0)  | Timbre **MARIMBA** (fundamental + 4º armónico, ataque seco, cola corta leñosa) |
| **BTN4** (45) | Timbre **GUITARRA ELÉCTRICA** (sierra + overdrive + vibrato, sostenido largo) |
| **POT1** (ADC1)  | Ataque |
| **POT2** (ADC2)  | Decay/cola (la marimba lo recorta, la guitarra lo alarga) |
| **POT3** (ADC8)  | Brillo (cutoff del filtro paso-bajos) |
| **POT4** (ADC10) | Timbre: morph (campana) · dureza del mazo (marimba) · drive (guitarra) |

## Efectos de luz (6 estados en ciclo circular, con BTN1 ◀ / BTN5 ▶)

0. **Apagado** — tira negra. **Es el estado inicial**: al encender no hay luces hasta que elijas un efecto.
1. **Onda** — onda simétrica que se expande desde el centro a cada golpe (estilo de la referencia de Omar).
2. **Cometa** — cabezas brillantes que vuelan del centro a los extremos con estela.
3. **Pulso** — toda la tira late a cada nota; respiración suave en reposo.
4. **Chispas** — polvo mágico en posiciones aleatorias a cada golpe (brillo aleatorio).
5. **Barrido** — onda de brillo que recorre la tira; nivel y velocidad pulsan con la energía.

El ciclo **rebota en los dos extremos**: `0 —BTN1→ 5` y `5 —BTN5→ 0`. Cada cambio confirma con un parpadeo magenta corto (también al entrar en Apagado, para saber que el botón se registró). Mientras está apagado, el sonido sigue funcionando normal; al salir del apagado no se descarga de golpe lo acumulado (las ondas y chispas pendientes se vacían).

Ojo: al **encender** sí hay un barrido de luz de ~0.4 s — es el diagnóstico de arranque (confirma tira + audio sin necesidad de USB) y termina en negro, dejando la tira en Apagado.

Toda la luz es **magenta puro**: no hay ningún otro color en el firmware. Los efectos varían **brillo**, posición y velocidad, nunca el tono — incluidos los flashes de confirmación (el de timbre se distingue por brillo: tenue/medio/fuerte) y el aviso de "IMU no detectado" (antes rojo, ahora magenta tenue).

El magenta se pinta en RGB directo con el helper `magenta(v)` → `CRGB(v, 0, v)`, **no** con `CHSV`: el mapa "rainbow" de FastLED desatura el magenta (lo baja a ~128,0,128). Con RGB directo también las mezclas aditivas (`leds[i] += c`) se mantienen exactamente en magenta.

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
