# step_sequencer_loader — Secuenciador + remote MIDI en vivo

Webapp con **doble personalidad**:

1. **Generador de firmware** — cargas 6 samples de drum kit, descargas el `.ino` listo para flashear con el secuenciador embebido.
2. **Control remoto en vivo** — una vez flasheado, te conectas al PercuSynth vía Web MIDI y editas el patrón, FX y transport en tiempo real. **El PercuSynth queda como MIDI CLOCK MASTER**, así que mientras editas desde la web también puedes enganchar un DAW como esclavo.

Hermana del firmware `firmwares/step_sequencer_midi/` — los dos son las dos mitades del mismo sistema.

## Cómo correrla

Doble-click sobre `index.html`. Requiere **Chrome o Edge** (Web MIDI no existe en Firefox/Safari).

```bash
# O servida por HTTP:
cd tools/step_sequencer_loader
python -m http.server 8000
```

## Cómo se comunica con la placa

**Web MIDI sobre USB** (no Web Serial). Cuando conectas el PercuSynth al PC aparece como dispositivo MIDI nativo.

| Dirección  | Canal | Mensaje                              | Significado                              |
|------------|-------|--------------------------------------|------------------------------------------|
| Web → ESP  | 16    | `NoteOn note=track*16+step vel`      | Editar celda del patrón (vel 0 = off)    |
| Web → ESP  | 16    | `NoteOn note=96..101 vel>0`          | Mute toggle de tracks T0..T5             |
| Web → ESP  | 16    | `NoteOn note=120/121/122`            | PLAY / STOP / RESET                      |
| Web → ESP  | 16    | `CC 20..30`                          | BPM, swing, pitch, HPF, vol, LPF, **delay**, **fill**, reverse, **beat-repeat**, **drop** |
| Web → ESP  | 10    | `NoteOn 36/38/42/46/49/51`           | Trigger manual de track (GM drum)        |
| ESP → Web  | -     | `0xF8` Clock 24 PPQ                  | Sincronización                           |
| ESP → Web  | -     | `0xFA / 0xFC` Start / Stop           | Transport                                |
| ESP → Web  | 16    | `NoteOn note=100+step`               | "Voy en este paso" → la webapp resalta   |
| ESP → Web  | 16    | `CC echo`                            | Pot local cambió → webapp actualiza slider |

Toda la lógica MIDI completa está documentada en el header de [`firmwares/step_sequencer_midi/step_sequencer_midi.ino`](../../firmwares/step_sequencer_midi/step_sequencer_midi.ino).

## Flujo típico

### Primera vez (cargar samples)

1. Abrir `index.html` en Chrome.
2. Pestaña **SAMPLES & FIRMWARE** → cargar 6 samples (MP3/WAV/OGG) en los 6 slots, **o grabarlos con `● REC`** desde el micrófono del PC (tope 8 s, se normaliza y se hornea igual que un archivo).
   - **`✂` Recortar**: abre un editor con la onda en grande y dos manijas (inicio/fin) para dejar solo la parte útil. Botón **AUTO SILENCIO** recorta el silencio de los extremos, **▶ ESCUCHAR** prueba el recorte antes de aplicar.
3. **⬇ DESCARGAR .INO** → guarda `step_sequencer_midi.ino`.
4. Arduino IDE → ESP32S3 Dev Module → **USB Mode: USB-OTG (TinyUSB)** · **USB CDC On Boot: Disabled** · **Flash Mode: DIO** (no OPI) → upload.

### Sesión en vivo

1. Conectar el PercuSynth por USB → aparece como dispositivo MIDI.
2. **▶ HABILITAR WEB MIDI** → seleccionar el PercuSynth en INPUT y OUTPUT → **CONECTAR**.
3. **↑ PUSH PATRÓN** envía el patrón local al ESP (la primera conexión).
4. Click en las celdas de la grilla 6×16 → edita el patrón **mientras suena**.
5. Sliders BPM / SWING / PITCH / HPF / LPF / VOL / **DELAY** + toggles FILL / BEAT REP / REVERSE / DROP → cambian FX en vivo.
6. **PLAY** desde la web manda `0xFA` → el ESP arranca el clock master → un DAW conectado al mismo puerto MIDI también arranca. Sin hardware, la app igual anima los pasos con su reloj interno.

### Efectos

- **Swing** — shuffle real: retrasa las semicorcheas impares.
- **Delay** — eco sincronizado a 1/8 del BPM; una sola perilla controla mezcla + feedback.
- **Fill** — roll de caja por encima del patrón (para rellenos).
- **Beat Repeat** — repite el paso actual rápido (glitch), congelando el avance.
- **Reverse** — reproduce los samples al revés.
- **Drop** — silencia el master mientras se mantiene (builds/bajadas).

### Mientras tanto, el ESP es autónomo

- Los pots locales también editan FX (con histéresis para no pisarse con la web): POT0=BPM · POT1=LPF · POT2=DELAY · POT3=VOL.
- **BTN1 = PLAY/STOP · BTN2 = Fill (hold) · BTN3 = Beat Repeat (hold) · BTN4 = Reverse (toggle) · BTN5 = Drop (hold)**.
- Si desconectas el USB el patrón sigue sonando hasta que cortas la corriente.

## Limitaciones

- Web MIDI **solo en Chrome y Edge**.
- 6 tracks × 16 steps fijos.
- Los samples se hornean en el firmware → cambiar el kit = regenerar y re-flashear.
- Patrón se puede exportar/importar como `.json` para backup.
