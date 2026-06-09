# midi_sampler — Sampler MIDI USB (1 sample · 4 pots)

App web standalone que reproduce **un solo sample** (p. ej. una campana) según la **nota MIDI** que entra por USB. La nota toca el sample transpuesto; el tono lo da el MIDI. Cuatro potenciómetros: **volumen, attack, decay y cutoff**. Nada más.

Dos modos:

1. **Instrumento en el navegador** (Web MIDI + Web Audio): suena al instante, sin flashear. Funciona también **sin hardware** (teclado en pantalla / teclas del PC y 4 knobs).
2. **Generador de firmware** (`.ino`): hornea el sample como array `PROGMEM` y produce un sketch que convierte al PercuSynth en un **sampler MIDI USB** físico de 1 sample, con los 4 pots.

> Mismo patrón que el resto de `tools/`: HTML de un solo archivo, sin build.

## Cómo correrla

Abrir `index.html` con doble-click. Requiere **Chrome o Edge** (Web MIDI).

## Flujo típico

1. **HABILITAR WEB MIDI** → elige tu controlador en *INPUT*.
2. **CARGAR** un sample (o arrástralo al recuadro). Define su **nota base** (la nota MIDI a la que suena a velocidad normal; default C4 = 60).
3. Toca: cada nota reproduce el sample transpuesto. Mueve los 4 knobs (o sus CC).

Sin controlador: teclado en pantalla o teclas `A S D F G H J K` / `W E T Y U` del PC. Sin sample cargado, suena un seno afinado a la nota (para probar al toque).

## Los 4 potenciómetros (= 4 pots del hardware)

| Knob    | Parámetro        | Rango         | CC MIDI | Pot (firmware) |
|---------|------------------|---------------|---------|----------------|
| VOLUMEN | Volumen master   | 0–100 %       | CC 7    | POT0 · ADC1    |
| ATTACK  | Ataque (envolv.) | 0–200 ms      | CC 73   | POT1 · ADC2    |
| DECAY   | Decay / release  | 0.05–2.0 s    | CC 72   | POT2 · ADC8    |
| CUTOFF  | Filtro pasa-bajos| 200 Hz–18 kHz | CC 74   | POT3 · ADC10   |

`VOL` y `CUTOFF` actúan en vivo; `ATTACK`/`DECAY` afectan a las notas nuevas. `velocity` MIDI → ganancia.

## Una voz

- Pitch por resampling (interpolación lineal) según `nota − nota_base`.
- Envolvente AR (attack + decay/release).
- Filtro pasa-bajos de un polo con cutoff variable.

## Modo firmware (.ino)

Pestaña **FIRMWARE (.ino)** → **DESCARGAR .INO** → `midi_sampler.ino` con el sample embebido. Arduino IDE → **ESP32S3 Dev Module** → **USB Mode = USB-OTG (TinyUSB)** → **Flash Mode = DIO** → Upload. Monitor 115200.

## Límites

- Duración máx del sample: 3 s. Mono · 44.1 kHz · 16-bit (automático).
- Web MIDI sólo en Chrome/Edge.
