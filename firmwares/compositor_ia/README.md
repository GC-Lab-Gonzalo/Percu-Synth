# compositor_ia v2 — pide una canción con tu voz

Fusión de `asistente_ia` (mic → Whisper → GPT) + motor de audio de `cancion_aleatoria_leds`:
GPT responde un **JSON de canción** y el PercuSynth la toca. La v2 agrega lo que hace que los
estilos suenen **realmente distintos** (portado de `tools/generador_estilos`):

- **Bajo** con 5 patrones: fundamental-quinta / walking / contratiempo / octavas / riff
- **Comping** por golpes: pad sostenido / golpes en 2 y 4 / stabs / power chords en corcheas
- **5 gramáticas de melodía**: frases generativas / arpegio / hook / riff / lírica
- **Forma con secciones** (intro → verso → coro/drop, capas e intensidad por sección)
- **Swing real** en todo el grid

## Controles

| Control | Función |
|---|---|
| **BTN1 (mantener)** | Graba tu pedido: *"quiero un techno oscuro"* (máx 5 s) |
| **BTN2** | Play / Stop |
| **POT1 / POT2 / POT3** | Volumen pads+bajo / melodía / percusión |
| **Monitor Serie 115200** | Pega un JSON (una línea) + Enter = canción **sin WiFi** |

Antes de compilar: copia `secretos.example.h` a `secretos.h` y pon ahí `OPENAI_API_KEY` + WiFi
(ese archivo está en el `.gitignore`, nunca se sube) · Flash Mode **DIO** · **PSRAM OPI** · USB CDC On Boot.
El prompt completo que sabe GPT está en `COMPOSER_PROMPT` dentro del `.ino` (sirve para pedirle
el JSON a cualquier IA en el PC y pegarlo por Serial).

## Claves nuevas de la v2

- `comp`: 0 pad · 1 golpes 2y4 · 2 stabs · 3 power corcheas
- `bajo`: 0 sin · 1 fund-quinta · 2 walking · 3 contratiempo · 4 octavas · 5 riff (+ `bajowave`, `bajolvl`)
- `melmodo`: 0 frases · 1 arpegio · 2 hook · 3 riff · 4 lírica
- `forma`: secciones `"capas/compases/intensidad/gramatica"` — capas: `b`atería `j`=bajo `c`=acordes `m`elodía `t`odas; gramática opcional: `q a h r l s`(solo). La progresión se reinicia por sección.

## Pruebas rápidas por Serial (pegar una línea + Enter)

**Techno oscuro** (four-on-floor, bajo rolling, stabs, hook, build→drop):
```json
{"estilo":"techno","raiz":9,"modo":2,"bpm":128,"swing":0.5,"prog":"0,0,0,5","beats":"4,4,4,4","voicing":0,"padwave":1,"atk":0.05,"rel":0.6,"cutoff":900,"q":2.2,"lfor":0.25,"lfod":2500,"det":10,"tone":0.6,"padlvl":0.45,"gate":2,"gatedepth":0.6,"comp":2,"bajo":3,"bajowave":1,"bajolvl":0.9,"kick":"1000100010001000","snare":"0000100000001000","hatc":"0010001000100010","hato":"0000000000000010","ghost":0.08,"plvl":0.7,"kickdec":0.3,"sndec":0.1,"snoise":0.7,"melmodo":2,"melwave":1,"meldens":3,"melgain":0.8,"env":0,"sub":0,"forma":"b/8/0.5, bj/8/0.65, t/16/0.85, cm/8/0.5/h, t/16/1.0, bj/8/0.5"}
```

**Blues shuffle** (12 compases, walking bass, golpes en 2 y 4, frases con solo):
```json
{"estilo":"blues","raiz":4,"modo":4,"bpm":76,"swing":0.6,"prog":"0,0,0,0,3,3,0,0,4,3,0,4","beats":"4,4,4,4,4,4,4,4,4,4,4,4","voicing":2,"padwave":3,"atk":0.05,"rel":1.2,"cutoff":1400,"q":1.0,"lfor":0.08,"lfod":900,"det":8,"tone":0.65,"padlvl":0.5,"gate":0,"gatedepth":0,"comp":1,"bajo":2,"bajowave":0,"bajolvl":0.95,"kick":"1000000010000010","snare":"0000100000001000","hatc":"1010101010101010","hato":"0000000000000000","ghost":0.06,"plvl":0.5,"kickdec":0.2,"sndec":0.12,"snoise":0.75,"melmodo":0,"melwave":3,"meldens":2,"melgain":0.9,"env":2,"sub":0,"forma":"jb/4/0.4, t/12/0.6, t/12/0.75, t/12/0.9/s, t/12/0.6"}
```

Si estos dos suenan claramente distintos entre sí (y del espacial por defecto), la v2 cumple.
