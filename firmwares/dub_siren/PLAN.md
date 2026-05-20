# dub_siren — Plan de implementación

Sintetizador "sirena dub" para PercuSynth (ESP32-S3). El sample se embebe en el firmware en **compile-time** mediante una **app web generadora de .ino** (patrón establecido por `synth_studio` y `loop_loader`). Una vez flasheado, el ESP32 corre standalone: la web app no se vuelve a usar hasta querer un nuevo sample.

**Repo:** `percusynth/tools/dub_siren_generator/` (web app · fuente de verdad del firmware)
**Hardware base:** PercuSynth ESP32-S3 WROOM-1 N8R8 (PCM5102 vía I2S)
**Última actualización:** 2026-05-15

---

## 1. Visión

Dub siren digital con dos personalidades superpuestas:

1. **Sample-based:** un sample largo (hasta ~30 s @ 44.1 kHz o ~60 s @ 22 kHz) embebido en flash. Reproducción normal al apretar PLAY. Al **mantener** CAPTURE, la cabeza lectora se "congela" en un microloop justo en la posición actual — beat-repeat/glitch clásico. Pitch y largo del loop modulables en vivo.
2. **Siren puro:** oscilador sine/square/saw con LFO modulando frecuencia. El sonido dub siren clásico, independiente del sample.

Ambas voces pasan por **filter LPF → delay tape-style con feedback (hasta auto-oscilación) → master**. IMU eje X modula `delay feedback` para el wobble gestual. Piezos disparan repeticiones rápidas del loop como "scratches percusivos".

**Flujo de uso:**
1. Abro `tools/dub_siren_generator/index.html` en Chrome
2. Drag-and-drop de un archivo de audio
3. Pre-procesa (mono · resample · normaliza · trim opcional)
4. Click "Generar .ino" → descarga `dub_siren_<nombre>.ino`
5. Abro en Arduino IDE, flasheo
6. **Ya no necesito la web app** hasta querer otro sample

---

## 2. Hardware (PercuSynth - recap relevante)

| Recurso | Detalle |
|---|---|
| MCU | ESP32-S3 WROOM-1 N8R8 — 240 MHz dual core, 8 MB PSRAM |
| DAC | PCM5102 vía I2S — 44.1 kHz · 16-bit · estéreo |
| I2S pines | BCK 41 · LCK 39 · DIN 40 |
| Botones | GPIO 44, 42, 0, 45, 47 |
| Potenciómetros | ADC 1, 2, 8, 10 |
| Piezos | ADC 4, 5, 6, 7 |
| IMU | MPU6050 vía I2C (SDA 21, SCL 38) |
| WS2812 | GPIO 46 (no usado en F1–F4) |
| MIDI DIN-5 TX | GPIO 43 (no usado en F1–F4) |

**Partition scheme requerido (en el .ino generado se incluye un comentario recordándolo):**
- **Tools → Partition Scheme → Huge APP (3MB No OTA / 1MB SPIFFS)**
- Esto da ~2.8 MB para sample embedido (después del código) = **~30 s @ 44.1 kHz mono o ~60 s @ 22 kHz mono**

**Otros Arduino IDE settings críticos:**
- USB CDC On Boot: **Enabled** (sólo si quieres Serial debug)
- Flash Mode: **DIO** (OPI rompe I2S — ya documentado en el repo)
- PSRAM: **OPI PSRAM** (no para el sample, sí para el delay largo opcional)

---

## 3. Arquitectura DSP del firmware generado

### Pipeline (mono interno, dup a estéreo en la salida)

```
[Sample Player] ─┐
                 ├→ [Mix] → [LPF one-pole] → [Delay tape] → [Master Vol] → I2S
[Siren Osc]   ──┘

[LFO] ──────────→ modula pitch de SamplePlayer y freq de SirenOsc
[IMU.x] ────────→ modula DelayFeedback (suavizado)
```

### Estructuras (mono int16_t, 44.1 kHz)

```c
// Sample data viene embebido en el .ino generado:
extern const int16_t SAMPLE[] PROGMEM;
extern const uint32_t SAMPLE_LEN;

struct SamplePlayer {
    double   phase;          // posición fraccionaria — DOUBLE para evitar drift en samples largos
    float    pitch;          // 0.25 – 4.0 (POT1)
    bool     playing;
    bool     reverse;
    bool     frozen;
    uint32_t freeze_pos;     // sample index donde arrancó el freeze
    uint32_t freeze_len;     // largo del microloop en samples (POT2)
};

struct SirenOsc {
    float phase;
    float freq;
    enum { WAVE_SINE, WAVE_SQUARE, WAVE_SAW } wave;
    bool  active;
    float env;               // envolvente exp decay
};

struct LFO { float phase, rate, depth; };

struct DelayTape {
    int16_t *buf;            // ~88 KB (1s) en heap interno · o hasta varios MB en PSRAM
    uint32_t size;
    uint32_t write_pos;
    float    time;           // 0 – 1 → 0 – size
    float    feedback;       // 0 – 1.1 (auto-osc en el tope)
};

struct FilterLPF { float a, state; };  // y[n] = y[n-1] + a*(x[n] - y[n-1])
```

### Sample render loop (resumen por sample)

1. Avanzar LFO, calcular `lfo_val = sinf(phase * 2π)`
2. **Sample player:** `pitch_eff = pitch * (1 + lfo.depth * lfo_val)` · avanza `phase` · si `frozen` wrap en `[freeze_pos, freeze_pos+freeze_len)`, si no en `[0, SAMPLE_LEN)` · lectura con **interpolación lineal**, leyendo `pgm_read_word_near(SAMPLE + i)` (es PROGMEM)
3. **Siren osc:** acumular fase con freq modulada por LFO, generar onda según `wave`, aplicar envolvente exp
4. **Mix:** `s = sample_out + siren_out`
5. **LPF:** `state += a * (s - state); s = state`
6. **Delay:** lee `delayed = buf[read_pos]` · `to_write = clip(s + delayed * feedback, ±32000)` con soft-clip · escribe · `s = s * dry + delayed * wet`
7. `s *= master_vol`, cast a int16_t, escribir L=R en `out_buf`

### Threading

- **Core 0:** audio loop (`i2s_channel_write` blocking) — ininterrumpido
- **Core 1:** lectura de pots/botones/piezos/IMU cada 5 ms, lógica de control
- Sincronización: variables `volatile` para parámetros simples. Sin protocolo de carga → sin necesidad de doble buffer del sample (vive en flash, inmutable).

### Carga de CPU

44.1 kHz · 128 samples por buffer = ~2.9 ms de cómputo. ESP32-S3 a 240 MHz tiene >100× headroom. Sin riesgo de underrun.

**Nota sobre PROGMEM:** lectura desde flash es ~3× más lenta que desde RAM. Para 44.1 kHz mono con interpolación lineal son 2 lecturas/sample × 44100 = 88k reads/s, completamente OK. Si en algún momento agregamos polifonía pesada de samples (no es el plan), se podría cachear bloques en RAM con DMA.

---

## 4. Mapeo de controles

### Capa normal

| Físico | Función | Rango / mapeo |
|---|---|---|
| **POT1** | Pitch sample/loop | 0.25× – 4×, mapeo exponencial centrado en 1× |
| **POT2** | Largo del loop frozen / posición lectura | 5 – 500 ms (frozen) · 0 – 100% del sample (no frozen) |
| **POT3** | Delay time | 10 – 1000 ms exp |
| **POT4** | Delay feedback | 0 – 110 % (auto-osc en el tope) |
| **BTN1** (44) | PLAY / STOP sample | toggle |
| **BTN2** (42) | **CAPTURE** (freeze) | **hold = freeze, release = libera** |
| **BTN3** (0)  | REVERSE | toggle |
| **BTN4** (45) | TRIGGER siren osc puro | nota corta con env decay |
| **BTN5** (47) | SHIFT | modificador para capa 2 |

### Capa SHIFT (BTN5 mantenido)

| Físico | Función |
|---|---|
| POT1 | LFO rate (0.1 – 20 Hz exp) |
| POT2 | LFO depth → pitch (0 – ±50 %) |
| POT3 | Filter cutoff (20 Hz – 20 kHz exp) |
| POT4 | Master volume |
| BTN1 | Cambiar wave del osc siren (sine → square → saw) |
| BTN2 | Reset posición a 0 |
| BTN3 | Toggle ping-pong del loop frozen |
| BTN4 | Reservado (futuro) |

### Sensores extra

- **IMU eje X (roll):** modula delay feedback en runtime. Smoothing exponencial (`α=0.05`) para evitar zipper noise.
- **Piezos (4):** triggers de scratch — al detectar pico, fuerza el sample a entrar en `frozen` en posición aleatoria con `freeze_len` corto (~30 ms), pitch derivado de la velocity. Auto-libera 200 ms después.

---

## 5. Serial debug (opcional, mínimo)

Siguiendo el patrón de `serial_test`, el `.ino` generado incluye logging básico por USB CDC (115200 baud) para debugging:

```
Heartbeat cada 2s: "HB play=1 frz=0 pitch=1.23 fb=85 mode=NORMAL"
Comandos opcionales:
  PING           → PONG
  INFO           → JSON con specs del sample (len, sr, duration)
  MUTE / UNMUTE  → silencia el output (útil para flashear sin parlante)
```

**No hay carga de samples por Serial.** El sample vive embebido en el `.ino`. Para cambiar el sample → re-generar el .ino con la web app y re-flashear.

---

## 6. Web app generadora (`tools/dub_siren_generator/index.html`)

HTML standalone sin build step. Sigue **exactamente** el patrón de `tools/loop_loader/index.html`.

### Pre-procesado de audio (Web Audio API)

```
file → arrayBuffer → audioCtx.decodeAudioData()
     → OfflineAudioContext(1ch, samples, targetSR).startRendering()  // mono + resample
     → normalizar por peak
     → trim opcional por zero-crossings (evita clicks al wrappear)
     → Int16Array con clipping a [-32768, 32767]
```

### Generación del .ino

Patrón idéntico al `generateFirmware()` de `loop_loader/index.html`:

```javascript
const L = [];
L.push(`// dub_siren — generado por dub_siren_generator`);
L.push(`// Sample: ${name} · ${duration}s · ${sr}Hz · mono`);
L.push(`// Partition: Huge APP (3MB No OTA / 1MB SPIFFS)`);
L.push(`#include <Arduino.h>`);
L.push(`#include "driver/i2s_std.h"`);
L.push(`#include <Wire.h>`);
// ... structs, constantes, render loop, setup, loop ...
L.push(int16ToC(int16Data, 'SAMPLE'));  // genera el const int16_t[] PROGMEM
L.push(`const uint32_t SAMPLE_LEN = ${int16Data.length}UL;`);
L.push(`const uint32_t SAMPLE_SR  = ${sr}UL;`);
// ... resto del firmware ...
const code = L.join('\n');
// blob + download como dub_siren_<nombre>.ino
```

**Importante:** el firmware completo (structs, setup, loop, render) vive como **strings en el código JS** del generador. No hay un `.ino` "manual" en el repo — la fuente de verdad es el JS que lo construye. Mismo modelo que `loop_loader` y `synth_studio`.

### Funcionalidades de la GUI

1. **Botón "Cargar audio"** + drag-and-drop
2. **Selector de sample rate target:** 44.1 kHz (calidad) · 22.05 kHz (doble duración, sonido lo-fi tipo cassette — perfecto para dub)
3. **Waveform canvas** con marcadores de inicio/fin para trim manual (igual que `loop_loader`)
4. **Indicador de tamaño:** muestra cuánto va a ocupar el sample en flash y avisa si excede el partition.
5. **Preview live con Web Audio API:** simula el dub siren en el navegador con los efectos para que puedas ajustar antes de generar — pitch/delay/LFO/freeze (mismo enfoque que `loop_loader` con su preview). Permite iterar sin re-flashear.
6. **Selector de defaults** que se hornean en el firmware: pitch inicial, delay time/feedback inicial, wave del osc.
7. **Botón "⬇ Descargar .ino"** → triggers download de `dub_siren_<nombre>.ino`
8. **Botón "⚡ FLASH FW" (opcional F5)** vía ESP Web Tools — pero esto requiere `.bin` pre-compilado, no `.ino`. Probablemente lo dejamos fuera porque el sample cambia entre cada generación; el flujo natural es descargar .ino y flashear con Arduino IDE.

### Log de actividad

Estilo `loop_loader`: panel inferior con mensajes `ok`/`warn`/`err` ("Sample cargado", "Trim aplicado", "Generado: 1.4 MB", "Aviso: cabe justo en Huge APP").

---

## 7. Plan de fases

### F1 — Generador con audio engine básico

**Objetivo:** que la web app genere un .ino que reproduzca y haga freeze del sample. Sin delay, sin LFO, sin osc puro todavía.

- [ ] Estructura inicial de `tools/dub_siren_generator/index.html`: layout copiado de `loop_loader`, file picker, waveform, log
- [ ] Pre-procesado: decode → mono → resample 44.1k → normalize → trim zero-crossings → Int16Array
- [ ] Generación del .ino con:
  - I2S setup
  - SamplePlayer con pitch lineal interp + wrap + freeze
  - Lectura POT1 (pitch) y POT2 (freeze_len)
  - Lectura BTN1 (PLAY/STOP), BTN2 (CAPTURE hold) con debounce 25 ms
- [ ] Download del .ino, abrir en Arduino IDE, flashear, verificar

**Criterio de éxito:** subo un mp3 a la web app, descargo el .ino, lo flasheo, y al apretar BTN1 reproduce. BTN2-hold congela en un microloop modulable por POT1/POT2.

### F2 — Voz dub siren completa en el firmware generado

- [ ] Delay tape-style en heap interno (1s · 88 KB) con feedback hasta auto-osc (soft-clip)
- [ ] LFO con rate/depth → modula pitch del sample y freq del osc
- [ ] Siren osc (sine/square/saw) con envolvente exp
- [ ] Filter LPF one-pole
- [ ] Lógica SHIFT con BTN5 mantenido para acceder a segunda capa de POTs
- [ ] Lectura BTN3 (reverse), BTN4 (trigger siren)
- [ ] Defaults configurables desde la GUI (pitch, delay, wave inicial)

**Criterio de éxito:** el sonido se reconoce como "dub siren" — auto-osc del delay funcional, osc siren puro disparable, LFO modula pitch.

### F3 — Preview en navegador

- [ ] Reimplementar el pipeline DSP en Web Audio API (AudioWorklet o nodos nativos)
- [ ] Controles virtuales espejando los del hardware (sliders + botones)
- [ ] Permite iterar sin re-flashear
- [ ] Botón "ajustar defaults" → los valores actuales del preview se hornean en el .ino al generar

**Criterio de éxito:** puedo escuchar el dub siren en el navegador sin tocar el hardware, ajustar el sound design, y exportar el .ino con esos defaults.

### F4 — IMU + piezos + waveform interactivo

- [ ] MPU6050 init + lectura periódica + smoothing exponencial
- [ ] IMU.x → delay feedback en runtime
- [ ] Peak detection en piezos (estilo `MIDI_Drum`) → triggers de scratch con `freeze_len` corto y pitch por velocity
- [ ] GUI: trim del sample con drag de marcadores en el waveform canvas
- [ ] Recordatorio en la GUI del partition scheme + check de tamaño

**Criterio de éxito:** instrumento "tocable" — gestos del cuerpo modulan el sonido, piezos disparan scratches percusivos sobre el sample.

### F5 — Polish y documentación

- [ ] README del generador con screenshots
- [ ] Sample bank curado en `samples/dub_siren_samples/` para demos rápidos
- [ ] Video de uso de 60 s
- [ ] Entrada en el README principal del PercuSynth

---

## 8. Estructura de archivos

```
percusynth/tools/dub_siren_generator/
└── index.html              ← HTML+CSS+JS standalone, ~1500 líneas
                              FUENTE DE VERDAD del firmware

percusynth/firmwares/dub_siren/
├── PLAN.md                 ← este archivo
├── README.md               ← descripción + link al generador (F5)
└── ejemplo_generado.ino    ← (opcional F5) un .ino de ejemplo
                              generado con un sample default,
                              para que un usuario pueda flashear
                              sin pasar por la web app si no quiere

percusynth/samples/dub_siren_samples/
└── (F5) banco curado de samples para demos
```

**Decisión:** No hay un `.ino` "manual" en `firmwares/dub_siren/`. El firmware se construye desde el JS del generador. Esto sigue la convención de `synth_studio` y `loop_loader`.

---

## 9. Tests / verificación

| Fase | Test manual | Métrica |
|---|---|---|
| F1 | Generar .ino con sample de 10 s, flashear, reproducir + freeze | Audio audible sin clicks; freeze sin discontinuidades |
| F1 | Generar con sample de 30 s a 44.1 kHz | Compila y cabe en Huge APP; reproduce sin glitches |
| F2 | Delay self-osc | Feedback al 110 % → oscilación sostenida sin saturar |
| F3 | Preview vs hardware A/B | El preview suena equivalente al hardware (mismo pipeline conceptual) |
| F4 | Inclinar dispositivo | Cambio audible en feedback, sin zipper |
| F4 | Golpe en piezo | Scratch corto disparado, vuelve a normal automáticamente |

---

## 10. Riesgos y mitigaciones

| Riesgo | Mitigación |
|---|---|
| **Sample supera el partition Huge APP** | La GUI calcula y muestra el tamaño en MB antes de generar; si excede, sugiere bajar a 22 kHz o trimear. Hard-cap a 28s @ 44.1 o 56s @ 22 kHz. |
| **PROGMEM read latency** | Cacheado de bloques sólo si se detecta underrun (probablemente innecesario para 1 voz mono) |
| **Pitch drift en samples largos por precision float** | Usar `double` para `phase` del SamplePlayer |
| **Zipper noise en pots** | Oversampling 8× (precedente en el repo) + slew limiting en parámetros |
| **Overflow en delay con feedback > 100 %** | Soft clipping antes de escribir al buffer |
| **Discontinuidad al wrappear el loop frozen** | Cross-fade automático de ~2 ms en el wrap-around |
| **El usuario olvida cambiar partition scheme** | Print por Serial al boot: "Sketch usa X bytes; partition Huge APP recomendado" + aviso prominente en la GUI con copy-paste del setting |
| **MPU6050 cuelga el I2C bus** | Watchdog en lectura (timeout 5 ms), si falla N veces deshabilitar IMU silenciosamente |
| **Re-generar .ino para cada nuevo sample es fricción** | Es la convención del ecosistema · el preview de F3 mitiga la mayoría del iterar (no hace falta re-flashear para tweakar params, sólo para cambiar sample) |

---

## 11. Open questions (a definir durante implementación)

1. **¿Cross-fade automático al cerrar el loop frozen?** — probable sí, ~2 ms con interpolación
2. **¿`freeze_len` mayor al sample restante desde `freeze_pos`?** — sí, wrap dentro del sample completo
3. **¿Reset del LFO al disparar SIREN o continuo?** — continuo (más expresivo)
4. **¿Sample stereo soportado o forzamos mono siempre?** — **mono siempre** (mitad de flash, simplifica)
5. **¿El preview de F3 reusa el código generado o reimplementa en Web Audio API?** — reimplementar en Web Audio (el firmware C no corre en browser); cuidar la equivalencia conceptual entre ambos
6. **¿Sample max 28 s o 30 s @ 44.1?** — depende del tamaño del código compilado, definir empíricamente en F1

---

## 12. Referencias en el repo

- `tools/loop_loader/index.html` — **referencia principal** del patrón (pre-procesado + generación de .ino + preview + waveform)
- `tools/synth_studio/` — otro generador, útil para la estructura de "ajustar params en GUI y hornear en .ino"
- `tools/percu_control/` — opcional para el botón "⚡ FLASH FW" (aunque probablemente no aplique acá)
- `firmwares/serial_test/serial_test.ino` — patrón base de I2S + Serial CDC mínimo (para el debug log opcional)
- `firmwares/drum_machine_basic/drum_machine_basic.ino` — biquad filter, voice struct, envelopes
- `firmwares/synth_basico/synth_basico.ino` — LFO sinusoidal, LPF one-pole
- `firmwares/MIDI_Drum/MIDI_Drum.ino` — MPU6050 + peak detection en piezos
