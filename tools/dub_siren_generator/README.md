# dub_siren_generator — Sintetizador dub siren con samples embebidos

App web que genera un firmware `.ino` con un **dub siren completo**: 3 samples polifónicos disparados por botones (hold-to-play), oscilador siren con LFO modulado por POT, delay tape-style con feedback (hasta auto-oscilación), y pitch global controlado por el IMU. Pensado para sets de dub/reggae/electrónica donde el dispositivo se mueve en la mano y "duberea" en vivo.

Hermano del firmware-en-desarrollo [`firmwares/dub_siren/`](../../firmwares/dub_siren/) (ver `PLAN.md` ahí para la visión completa).

## Cómo correrla

Doble-click sobre `index.html`. Recomendado **Chrome o Edge** (soporte de Web Audio API).

```bash
# Si querés servirla por HTTP:
cd tools/dub_siren_generator
python -m http.server 8000
```

## Cómo se comunica con la placa

**No se comunica.** Patrón "generate-and-flash":

```
3 audio files → webapp procesa (resample · trim · normaliza) → .ino → Arduino IDE → flash
```

Una vez flasheada la placa, el dub siren corre standalone — la webapp no se vuelve a usar hasta cambiar los samples.

## Sample rate

Elegís uno de dos modos antes de generar:

| SR        | Duración total máx (3 samples) | Carácter                              |
|-----------|--------------------------------|----------------------------------------|
| 44.1 kHz  | ~28 s                          | calidad estudio, alineado con el PCM5102 |
| 22.05 kHz | ~56 s                          | lo-fi tipo cassette, doble duración    |

## Mapa del firmware generado

### Controles continuos

| Control                 | Hardware              | Rango / Función                                     |
|-------------------------|-----------------------|------------------------------------------------------|
| LFO depth               | POT1 ADC1             | 0 – 100 % (profundidad del wobble)                  |
| LFO rate                | POT2 ADC2             | 0.1 – 20 Hz (curva exp · velocidad del wobble)      |
| Delay time              | POT3 ADC8             | 10 ms – 1 s (curva exp · pitch shift al mover)      |
| Delay feedback          | POT4 ADC10            | 0 – 100 % (auto-osc estable en el tope)             |
| Pitch global            | IMU AccelX (MPU6050)  | 0.25× – 4× exp · centro = 1× · afecta todo el mix   |

### Botones (todos HOLD → release = stop)

| Botón    | Hardware    | Acción                                                |
|----------|-------------|-------------------------------------------------------|
| BTN1     | GPIO 44     | TRIGGER sample 1                                      |
| BTN2     | GPIO 42     | TRIGGER sample 2                                      |
| BTN3     | GPIO 0      | TRIGGER sample 3                                      |
| BTN4     | GPIO 45     | SIREN gate (oscilador sirena con decay exp ~220 ms)   |
| BTN5     | GPIO 47     | RANDOM sample (elige uno de los 3 al azar)            |

Los 3 samples son mezclables — apretar varios botones a la vez genera la mezcla. Cada sample sale al ~40 % de su nivel para que la suma + siren no sature.

### Salidas

- **Audio**: I2S → PCM5102 (BCK 41 · LCK 39 · DIN 40 · 44.1 kHz · 16-bit estéreo).
- **LED on-board (GPIO 48 WS2812)**: ⚫ stop · 🔴 sample play · 🟢 random — requiere lib `FastLED`.

## Flujo típico

1. Abrir `index.html` en Chrome.
2. Drag & drop hasta 3 archivos de audio (WAV/MP3/OGG) sobre los slots.
3. (Opcional) Trim de cada sample con los handles de la waveform.
4. Elegir sample rate (44.1 kHz para calidad, 22.05 kHz para más duración).
5. **⬇ GENERAR FIRMWARE (.ino)** → descarga `dub_siren_<nombre>.ino`.
6. Arduino IDE → ESP32S3 Dev Module → **Flash Mode: DIO** + **PSRAM: OPI PSRAM** + **Partition Scheme: Huge APP (3MB)** → upload.

## Librerías Arduino requeridas

Instalar desde el Library Manager:

- **FastLED** (LED on-board)
- **MPU6050** — Electronic Cats fork (IMU)

## Output

Header del `.ino` generado (ejemplo real, v0.6 con 3 samples cargados):

```cpp
// PercuSynth — dub_siren  v0.6
// 3 samples polifónicos · Siren osc · LFO modulado por POT · Delay tape
// Generado por dub_siren_generator · 16/5/2026, 16:34:49
// SAMPLE 1: Ahhh · 0.663s · 44100 Hz mono · 57 KB
// ...
```
