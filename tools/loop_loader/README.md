# loop_loader — Cargador de loops + hits con preview en vivo

App web que genera un firmware `.ino` con **3 loops (hasta 8 s) + 6 hits one-shot (hasta 3 s)** embebidos en flash. Pensada para sets de drum + bass loops + percusiones disparables por piezos y botones, todo con preview en el navegador antes de comprometer la placa.

Versión más rica que `sample_loader`: agrega looping preciso sin click al wrappear, polifonía en los hits (hasta 3 voces), HPF + LPF biquad controlables por POT/IMU, y pitch shifting global.

## Cómo correrla

Doble-click sobre `index.html` (no necesita Web Serial). Recomendado **Chrome o Edge**.

```bash
# Si quieres servirla por HTTP:
cd tools/loop_loader
python -m http.server 8000
```

## Cómo se comunica con la placa

**No se comunica** — flujo offline: genera `.ino` y lo flasheas con Arduino IDE.

## Mapa del firmware generado

| Control                | Hardware                                | Función                          |
|------------------------|-----------------------------------------|----------------------------------|
| Loop A (toggle)        | BTN GPIO 44                             | start/stop loop A                |
| Loop B (toggle)        | BTN GPIO 42                             | start/stop loop B                |
| Loop C (toggle)        | BTN GPIO 0                              | start/stop loop C                |
| Hit 0                  | BTN GPIO 45                             | one-shot                         |
| Hit 1                  | BTN GPIO 47                             | one-shot                         |
| Hits 2–5               | Piezo ADC 4, 5, 6, 7                    | one-shots (3 voces simultáneas)  |
| Pitch / Speed          | POT1 ADC1                               | 0.5× – 2.0× (afecta loops y hits)|
| HPF cutoff             | POT2 ADC2                               | 20 – 4000 Hz                     |
| Sensibilidad piezo     | POT3 ADC8                               | umbral de disparo                |
| Volumen master         | POT4 ADC10                              | 0 – 100 %                        |
| LPF frecuencia         | IMU AccelX (MPU6050)                    | 200 – 20 000 Hz                  |
| LPF resonancia (Q)     | IMU AccelY (MPU6050)                    | 0.5 – 10.0                       |

## Flujo típico

1. Abrir `index.html` en Chrome.
2. Drag & drop 3 loops largos (8 s máx) sobre la grilla "LOOP SAMPLES".
3. Drag & drop hits/percusiones sobre la grilla "HIT SAMPLES".
4. Probar en el navegador con teclas Q W E (loops) y 1-6 (hits). Los sliders simulan POTs e IMU para escuchar antes de quemar firmware.
5. **GENERATE FIRMWARE (.ino)** → descarga `percusynth_loop_player.ino`.
6. Arduino IDE → ESP32S3 Dev Module → DIO → upload.

## Detalles técnicos

- Loops: lectura precisa con interpolación lineal, wrap sin click (último sample → primer sample).
- Hits: pool de 3 voces con voice stealing (roba la voz más avanzada cuando se llena).
- HPF one-pole + LPF biquad con Q variable + soft limiter.
- Mono 44.1 kHz · 16-bit, todo `PROGMEM`.

## Limitaciones

- Loops: 8 s máx cada uno → ~1.4 MB cada uno a 44.1 kHz mono.
- Hits: 3 s máx cada uno → ~0.5 MB cada uno.
- Total flash disponible para datos: ~3 MB (con Partition Scheme "Default 4MB with spiffs").

## Output

`.ino` generado en el formato `PercuSynth — Loop Player Firmware` (ver ejemplos vivos en [`samples/percusynth_loop_player/`](../../samples/percusynth_loop_player/)).
