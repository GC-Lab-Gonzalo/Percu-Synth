# bpm_mono_44100 — Editor BPM-aware de loops + sampler polifónico

App web especializada en preparar loops que tienen que **coincidir en BPM y cantidad de compases** antes de embeberse en un firmware. Setea un BPM master, ajusta cada loop (trim, time-stretch o pitch-shift) para que entren prolijos en N compases, y genera un `.ino` que sincroniza los loops automáticamente.

Variante "pro" de `loop_loader`: agrega tap-tempo, transporte master con beat dots, snap a compás y un mixer mono-switcher (solo un loop activo a la vez) + sampler polifónico paralelo.

## Cómo correrla

Doble-click sobre `index.html`. Recomendado **Chrome o Edge**.

```bash
# Si querés servirla por HTTP:
cd tools/loops/bpm_mono_44100
python -m http.server 8000
```

## Cómo se comunica con la placa

**No se comunica directamente** — flujo offline:

```
loops + samples → webapp ajusta BPM y trim → .ino → Arduino IDE → flash
```

## Mapa del firmware generado

### Layout

| Categoría    | Slots | Hardware                | Comportamiento                              |
|--------------|-------|-------------------------|---------------------------------------------|
| LOOPS        | Q W E R T (5) | BTN GPIO 44, 42, 0, 45, 47 | mono switcher — solo uno activo a la vez |
| SAMPLES      | Y A S D (4)   | Piezo ADC 4, 5, 6, 7        | polifónicos, paralelos al loop activo    |

### Controles en vivo

| Control            | Hardware             | Función                                      |
|--------------------|----------------------|----------------------------------------------|
| Pitch / Speed      | POT1 ADC1            | 0.5× – 2.0× (afecta loops + samples)         |
| HPF cutoff         | POT2 ADC2            | 20 – 4000 Hz                                 |
| LPF Q (resonancia) | POT3 ADC8            | 0.5 – 10.0                                   |
| Volumen master     | POT4 ADC10           | 0 – 100 %                                    |
| LPF frecuencia     | IMU AccelX (MPU6050) | 200 – 20 000 Hz                              |

## Flujo típico

1. Abrir `index.html` en Chrome.
2. Setear el **BPM master** (slider o **TAP** 3+ veces sobre el botón).
3. Cargar hasta 5 loops + 4 samples (drag & drop o file picker).
4. Por cada loop:
   - Marcar la cantidad de compases que ocupa.
   - Ajustar trim si arranca/termina off-beat.
   - El editor muestra cuántos beats sobran/faltan para encajar.
5. **▶ MASTER START** → escuchar todo sincronizado en el navegador.
6. Cuando suena bien, **⬇ GENERAR FIRMWARE** → descarga `.ino` listo para flashear.

## Cuándo usar esta vs loop_loader

| Usar `loop_loader`                    | Usar `bpm_mono_44100`                                  |
|---------------------------------------|--------------------------------------------------------|
| Loops de longitudes diversas          | Loops que tienen que estar sincronizados en BPM        |
| Setups improvisatorios                | Sesiones de dub/dance donde el groove pisa todo        |
| Hasta 3 loops                         | Hasta 5 loops con mono-switching                       |
| Sin necesidad de tap-tempo            | Querés ajustar el BPM en vivo antes de flashear        |

Para sets en vivo donde un loop entra/sale al apretar un botón sin solapamiento, este genera firmware más limpio.

## Output

`.ino` en el formato `PercuSynth — BPM Mono Player + Polyphonic Samples Firmware`. Ejemplo real en [`samples/Industrial/Industrial.ino`](../../../samples/Industrial/Industrial.ino).
