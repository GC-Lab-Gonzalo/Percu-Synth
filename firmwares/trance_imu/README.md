# trance_imu — Secuenciador de Trance Polifónico (IMU)

Port del secuenciador de trance del **Proto-Synth v2** (`firmwares/trance_IMU`) al
**PercuSynth**, subiendo el motor de audio de otra liga y volviéndolo **polifónico**.

## Qué cambia respecto al original (Proto-Synth v2)

| | Proto-Synth v2 (original) | PercuSynth (este firmware) |
|---|---|---|
| Salida | DAC interno 8-bit, **8 kHz**, `delayMicroseconds` (bloqueante) | I2S → PCM5102 **44.1 kHz · 16-bit estéreo**, buffers DMA no bloqueantes |
| Timing del paso | `millis()` + reproducción bloqueante | exacto **a nivel de muestra** |
| Voces | monofónico (1 nota a la vez) | **polifónico** — pool de 16 voces |
| Por paso | 1 nota | **acorde** de 4 voces (voicing diatónico editable en vivo) |
| Oscilador | sierra cruda (aliasing) | **sierra / cuadrada / triangular** anti-aliasing (PolyBLEP) |
| Salida | directa (recorte duro) | **saturador suave tipo tanh** acotado, sin clipping digital |

Como las voces resuenan con su propia envolvente de decay, las notas consecutivas
**se solapan** → textura tipo *pluck de trance* con cola. Subir el POT de decay
densifica la polifonía.

> **Sin LEDs y sin Serial a propósito**: todo el presupuesto de CPU va al audio.

## Hardware

- ESP32-S3 + DAC **PCM5102** vía I2S (`LCK 39 · DIN 40 · BCK 41`)
- **MPU6050** por I2C (`SDA 21 · SCL 38`, dirección `0x68`) — controla el filtro
- 5 botones (pull-up interno) · 4 potenciómetros

## Controles — tres paneles

Cambio de panel por **combos** (el mismo combo te devuelve al Panel A):
- **BTN1 + BTN5** a la vez → **Panel B** (notas / tonalidad)
- **BTN2 + BTN4** a la vez → **Panel C** (timbre / síntesis)

(Las acciones individuales de los botones de combo se disparan al *soltar* y se ignoran
si fue combo, así presionar dos juntos no dispara su función por accidente.)

### Panel A (normal)

| Control | Pin | Función |
|---|---|---|
| BTN1 | 44 | Play / Stop (al detener, las voces decaen solas) |
| BTN2 | 42 | Cambiar escala (Mayor · Menor · Armónica · Árabe · Lidia · Frigia) |
| BTN3 | 0  | Cambiar patrón (16) |
| BTN4 | 45 | Largo de secuencia (4 → 8 → 16 pasos) |
| BTN5 | 47 | Octava (-1 → 0 → +1 → +2 respecto a C3) |
| BTN1 + BTN5 | 44 + 47 | **Ir a Panel B** (presionar juntos) |
| BTN2 + BTN4 | 42 + 45 | **Ir a Panel C** (presionar juntos) |
| POT1 | ADC1 | Ataque (0.5 ms – ~100 ms) |
| POT2 | ADC2 | Volumen master (curva cuadrática) |
| POT3 | ADC8 | Tempo (40 – 300 BPM, notas de 1/16) |
| POT4 | ADC10 | Decay/Release (~50 ms – ~2.5 s) |
| IMU X | — | Cutoff del filtro paso-bajo |
| IMU Y | — | Resonancia (Q) del filtro |

### Panel B (shift — notas / tonalidad)

Para armar canciones: **cada botón es una nota** (cambia la tonalidad de la canción) +
**reescritura del acorde en vivo**, todo **cuantizado a la escala** seleccionada. Cada botón
fija la transposición de la tónica en semitonos, con las **distancias tonales de Si mayor /
Sol# menor** (una pentatónica mayor):

| Control | Pin | Nota (distancia desde la tónica) |
|---|---|---|
| BTN1 | 44 | **F#** (+7) |
| BTN2 | 42 | **C#** (+2) |
| BTN3 | 0  | **B** — tónica (0) |
| BTN4 | 45 | **D#** (+4) |
| BTN5 | 47 | **G#** (+9) |
| BTN1 + BTN5 | 44 + 47 | **Volver a Panel A** (presionar juntos) |
| POT1–POT4 | ADC1/2/8/10 | Grado de escala (0–7) de cada una de las **4 voces** del acorde |

- Son **distancias tonales**: lo que importa son los intervalos entre las notas. Que cada
  acorde suene mayor o menor (p. ej. D#m, G#m) lo define la **escala activa** del Panel A.
- La nota/tonalidad elegida **persiste** al volver al Panel A (tu canción se queda en la nueva tonalidad).
- El **voicing del acorde que armas con los pots también persiste**: el motor usa siempre
  `chordOffset` (4 voces), así que el acorde que dejas en Panel B sigue sonando en Panel A
  (allí no se edita, pero no se "desarma"). Por defecto: fundamental · 3ª · 5ª · octava.

### Panel C (timbre / síntesis)

Da forma al sonido. Todo lo que ajustes aquí **persiste** al volver al Panel A.

| Control | Pin | Función |
|---|---|---|
| BTN1 | 44 | **Forma de onda**: Sierra → Cuadrada → Triangular |
| BTN2 | 42 | **Octava arriba** (capa +12, brillo) on/off |
| BTN3 | 0  | **Unison** (lead mono gordo) / **Poly** (acorde) |
| BTN4 | 45 | **Quinta** (capa de quinta justa +7, power-chord/órgano) on/off |
| BTN5 | 47 | **Sub-osc** (capa -12, cuerpo de bajo) on/off |
| BTN2 + BTN4 | 42 + 45 | **Volver a Panel A** (presionar juntos) |
| POT1 | ADC1 | **Detune** entre voces (coro en poly, *supersaw* en unison; 0–25 cents) |
| POT2 | ADC2 | **Drive** (saturación de salida: limpio → caliente) |
| POT3 | ADC8 | **Piso de cutoff** del filtro (carácter aunque no muevas el IMU; el IMU suma encima) |
| POT4 | ADC10 | **Resonancia base** (Q) del filtro (el IMU suma encima) |

- **Unison**: colapsa el acorde a la fundamental con todas las voces desafinadas → lead mono
  ancho tipo *supersaw* (sube el Detune para engordarlo). En Poly vuelve al acorde del Panel B.
- **Capas apilables**: sub-osc (−12), quinta (+7) y octava-arriba (+12) suman una voz cada una
  del pool de 16 (combínalas para power-chords y pads gruesos).
- Osciladores con anti-aliasing PolyBLEP. La **cuadrada** se genera como diferencia de dos
  sierras band-limited (50 %, sin offset DC → sin clip), a nivel parejo con la sierra.

### Congelado de controles entre paneles

Al cambiar de panel los parámetros **no se auto-actualizan**: quedan congelados tal cual se
dejaron. Al cambiar de panel se fija un *ancla* con la posición de cada pot y queda "apagado";
un pot **retoma el control solo cuando se mueve** ≥ 2 % desde el ancla (medir desde el ancla lo
hace inmune al ruido del ADC), y solo sobre el parámetro del panel activo. Los botones, al ser
por flanco, ya solo actúan al pulsarlos. Resultado: mover POT1 en Panel B reescribe el acorde
pero **no toca** el ataque del Panel A hasta que muevas POT1 estando en Panel A.

Mover el PercuSynth en el aire hace el clásico *filter sweep* del trance (activo en los tres paneles).

## Compilar y flashear (Arduino IDE)

- Board: **ESP32S3 Dev Module**
- USB CDC On Boot: **Enabled**
- Flash Mode: **DIO** (¡OPI rompe el I2S!)
- PSRAM: **OPI PSRAM**
- Librerías: ESP32 Arduino core ≥ 3.x (incluye `driver/i2s_std.h`) + `Wire.h` (incluida)

## Modo de uso

1. BTN1 para arrancar la secuencia.
2. POT3 ajusta el tempo; POT4 alarga la cola para texturas más densas.
3. BTN2 / BTN3 cambian escala y patrón en vivo.
4. Mueve el equipo: el IMU abre/cierra y resuena el filtro.
5. BTN5 lleva la línea de grave a lead cambiando de octava.
