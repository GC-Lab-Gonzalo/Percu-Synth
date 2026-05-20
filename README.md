# PercuSynth

**PercuSynth** es un laboratorio portátil de experimentación con electrónica, programación y síntesis de audio, desarrollado por **GC Lab Chile**.

La percusión y los sintetizadores son la excusa: el arte y la música son la puerta de entrada a un proceso de aprendizaje más amplio, donde explorar electrónica, código y tecnología se vuelve natural, entretenido y con sentido. Es un proyecto diseñado bajo la metodología **STEAM** — donde la ciencia, la tecnología, la ingeniería y las matemáticas se integran con el arte como motor creativo.

<p align="center">
  <img src="Imagenes/percu-synth modelo 3d isometrica.jpeg" alt="PercuSynth - modelo 3D" width="600"/>
</p>

El hardware es compacto, portátil y basado en el microcontrolador **ESP32-S3**. Desde él se puede experimentar con síntesis de sonido, secuenciadores, controladores MIDI, sensores de movimiento, sampleo, luces reactivas y mucho más. Los firmwares y las herramientas web disponibles son solo el punto de partida — **el proyecto está en desarrollo activo** y las posibilidades son abiertas.

---

## ¿Qué se puede hacer con PercuSynth?

El laboratorio permite explorar una amplia variedad de ideas, por ejemplo:

- Sintetizadores y máquinas de ritmo con síntesis de audio en tiempo real
- Controladores MIDI para software de producción musical
- Secuenciadores de pasos con patrones grabables
- Detección de golpes y gestos con sensores piezoeléctricos e IMU
- Theremin wavetable con control gestual por IMU
- Reproducción de samples y loops cargados desde el navegador
- Experimentos con filtros, osciladores, envolventes y efectos digitales
- Visuales reactivas en tira LED WS2812 sincronizadas con sonido o gesto
- Integración con software como Ableton, GarageBand, Pure Data, etc.

Los firmwares y herramientas del repositorio son ejemplos concretos de estas posibilidades. El proyecto crece con cada experimento nuevo.

---

## Hardware

<p align="center">
  <img src="Imagenes/percu-synth pinout.jpeg" alt="PercuSynth - Pinout" width="600"/>
</p>

> **Tip para usar con IA:** puedes subir directamente la imagen del pinout a cualquier asistente de IA (Claude, ChatGPT, Gemini, etc.) para que entienda el hardware y te ayude a crear nuevos firmwares. Ver también el [prompt listo para copiar](Documentos/PROMPT_IA.md).

### Componentes principales

- **Microcontrolador:** ESP32-S3 (USB nativo, WiFi, Bluetooth)
- **DAC de audio:** PCM5102 vía I2S — salida estéreo 44.1 kHz · 16-bit
- **IMU:** MPU6050 — acelerómetro + giroscopio para control gestual (I2C)
- **Entradas:** 5 botones, 4 potenciómetros, 4 sensores piezoeléctricos, 2 sensores analógicos externos
- **Salidas:** Audio I2S · tira LED WS2812 · MIDI USB nativo · MIDI DIN-5

### Pinout

| Señal | Pin ESP32-S3 |
|-------|--------------|
| I2S LRCK | 39 |
| I2S DATA | 40 |
| I2S BCLK | 41 |
| Botones | 44, 42, 0, 45, 47 |
| Potenciómetros | ADC 1, 2, 8, 10 |
| Piezos | ADC 4, 5, 6, 7 |
| LED WS2812 (datos) | 46 |
| MIDI DIN-5 TX | 43 |
| I2C SDA (MPU6050) | 21 |
| I2C SCL (MPU6050) | 38 |
| Sensor externo A | ADC 3 |
| Sensor externo B | ADC 9 |

Todos los firmwares de audio generan señal a **44.1 kHz, 16-bit estéreo** a través del DAC PCM5102, con buffers DMA de 128 muestras.

---

## Firmwares disponibles

Cada firmware es un sketch Arduino independiente (`.ino`). Se compila y se carga por separado — no hay sistema de build centralizado. Los siguientes son los firmwares desarrollados hasta la fecha; el proyecto está en etapa temprana y se irán sumando más experimentos con el tiempo.

### `drum_machine_basic` — Drum Machine con Secuenciador
- 10 voces polifónicas: kick, snare, hi-hat, crash, click
- Síntesis por osciladores + ruido LCG + filtros biquad bandpass en cascada
- Secuenciador de 4 pistas × 16 pasos con grabación en tiempo real
- Control de tempo y timbre por potenciómetros

### `MIDI_Drum` — Controlador MIDI de Percusión
- Convierte golpes físicos y gestos en mensajes MIDI USB (canal 9)
- Tres modalidades de entrada: botones (cola circular), sensores piezoeléctricos (peak-detection 15 ms) e IMU (ventana 20 ms)
- Debounce de 50 ms por piezo y 25 ms por botón
- Compatible con cualquier software o hardware que reciba MIDI

### `synth_basico` — Sintetizador Polifónico
- 5 voces con morphing de forma de onda (senoidal → cuadrada → diente de sierra)
- Vibrato LFO (±1.2%, 0.2–8.2 Hz) y filtro pasa-bajos one-pole controlados por potenciómetro
- Notas mapeadas a los botones: C4, D4, E4, F4, G4

### `test_leds` — Prueba de Tira LED WS2812
- 6 modos de animación: sólido, chase, rainbow, twinkle, pulso, meteor
- LEDs 0-5 (SMD internos) siempre apagados — LEDs 6 en adelante activos (144 LEDs)
- Botones cambian modo y dirección; potes controlan brillo, color, velocidad y parámetro extra
- Requiere librería **FastLED**

### `drum_midi_leds` — Drum Machine + MIDI + Luces
- Secuenciador de 16 pasos × 4 drums (kick, snare, hi-hat, crash) sincronizado con efectos full-strip cinematográficos en la tira WS2812
- Salida MIDI USB en canal 10 (notas 36, 38, 42, 49) — el Percu-Synth queda como controlador de un DAW
- BTN5 alterna entre modo **GRAB** (grabación en tiempo real) y **PLAYBACK**
- Potes para brillo, tempo (60–240 BPM), color base del fondo y velocity MIDI (60–127)
- Cola de NoteOff diferidos para mantener los gates MIDI limpios
- Requiere **FastLED**, `USB.h` y `USBMIDI.h`

### `step_sequencer_midi` — Secuenciador de samples + MIDI Clock Master
- Secuenciador de 16 pasos × 6 pistas con samples embebidos en flash
- **MIDI Clock Master por USB** (24 PPQ) — sincronizá un DAW al PercuSynth
- Control remoto en vivo desde la webapp `tools/step_sequencer_loader/` vía Web MIDI: editar patrón, FX y transport sin re-flashear
- FX globales: HPF/LPF biquad, master pitch (cinta), bitcrush, stutter, reverse
- El `.ino` con samples se regenera con la webapp; este `.ino` del repo trae placeholders vacíos

### `dub_siren` — *(en desarrollo)*
- Sintetizador "sirena dub" con 3 samples polifónicos + oscilador siren con LFO + delay tape con feedback
- Generación del firmware (con samples embebidos) vía `tools/dub_siren_generator/`
- Plan completo y arquitectura en [`firmwares/dub_siren/PLAN.md`](firmwares/dub_siren/PLAN.md)

---

## Herramientas web (`tools/`)

Páginas HTML standalone (sin build, sin npm) que cumplen tres roles distintos según la herramienta:

1. **Generadores de firmware** — drag & drop de audios → la webapp produce un `.ino` con los samples embebidos en flash como arrays `PROGMEM`. Lo flasheas con Arduino IDE y la web no se usa más hasta que cambies los samples. (`sample_loader`, `loop_loader`, `dub_siren_generator`, `loops/bpm_mono_44100`)
2. **Flasheo desde el navegador** — instala el firmware compilado directo al ESP32-S3 vía **ESP Web Tools**, sin abrir Arduino IDE. (`percu_control`)
3. **Control remoto en vivo** — se conecta al PercuSynth ya flasheado vía **Web MIDI** y edita patrón / FX / transport en tiempo real. (`step_sequencer_loader`)

Todas requieren **Chrome o Edge** (Firefox/Safari no soportan Web Serial ni Web MIDI). Cada herramienta tiene su propio `README.md` con detalles:

### [`percu_control/`](tools/percu_control/) — Panel de control universal + flasheo
Interfaz visual completa para configurar el PercuSynth: osciladores con 5 formas de onda, octave shift, mixer, ruido (white/pink), drive multimodo (off/soft/fold/bit), filtro, LFO y master. **Incluye un botón "⚡ FLASH FW" que instala el firmware directo al ESP32-S3 desde el navegador** usando ESP Web Tools — no requiere Arduino IDE para usuarios finales.

### [`sample_loader/`](tools/sample_loader/) — Cargador de samples one-shot
Drag & drop de hasta 5 archivos de audio (máx 2 s c/u). **Genera un `.ino` con los samples embebidos en flash como arrays `PROGMEM`**. El firmware generado convierte al PercuSynth en un sampler polifónico con pitch shift cuantizado a escala frigia.

### [`loop_loader/`](tools/loop_loader/) — Cargador de loops + hits con preview
Genera firmware con 3 loops (hasta 8 s, wrap sin click) + 6 hits one-shot (hasta 3 s, polifonía 3 voces). Preview en navegador con sliders que simulan los pots y el IMU.

### [`step_sequencer_loader/`](tools/step_sequencer_loader/) — Secuenciador remoto en vivo
Doble personalidad: **(1)** genera firmware con 6 samples embebidos y secuenciador 6×16; **(2)** una vez flasheado, controla el PercuSynth en vivo vía **Web MIDI** — editar patrón, FX y transport sin re-flashear. El PercuSynth queda como MIDI Clock Master.

### [`dub_siren_generator/`](tools/dub_siren_generator/) — Generador de dub siren
Genera firmware con 3 samples polifónicos + oscilador siren con LFO modulado por POT + delay tape con feedback (hasta auto-oscilación) + pitch global por IMU. Sample rate seleccionable (44.1 kHz / 22 kHz lo-fi).

### [`loops/bpm_mono_44100/`](tools/loops/bpm_mono_44100/) — Editor BPM-aware de loops
Variante avanzada de `loop_loader` con tap-tempo, transporte master con beat dots, snap a compás y mono-switcher (un solo loop activo a la vez) + sampler polifónico paralelo. Para sesiones donde los loops tienen que estar sincronizados en BPM.

---

## Samples (`samples/`)

Esta carpeta es una **biblioteca viva de firmwares generados por las webapps de `tools/`**. Cada subcarpeta tiene un `.ino` que ya viene con samples reales embebidos, listos para abrir en Arduino IDE y flashear como ejemplo de qué se puede armar:

- **`Industrial/` · `industrial_dos/`** — kits industriales (generados con `bpm_mono_44100`)
- **`percusynth_samples/` · `percusynth_samples2/`** — bancos propios (generados con `sample_loader`)
- **`percusynth_loop_player/`** — set dub/dance (generado con `loop_loader`)
- **`sonidos/`** — archivos de audio crudos (WAV) listos para alimentar las webapps

> Estos `.ino` son **artefactos generados**, no escritos a mano. Para hacer tus propios kits, abre la webapp correspondiente, carga tus audios y genera un nuevo `.ino`.

---

## Cómo cargar un firmware

### Opción A — Desde el navegador (sin Arduino IDE)

Sirve `tools/percu_control/` con un mini-servidor HTTP y abre la página en Chrome o Edge:

```bash
cd tools/percu_control
python -m http.server 8000
```

Luego abre <http://localhost:8000>, conecta el Percu-Synth por USB y aprieta **⚡ FLASH FW**.

### Opción B — Desde Arduino IDE

1. Abrir el archivo `.ino` en **Arduino IDE**
2. Seleccionar placa: **ESP32S3 Dev Module** (o variante equivalente)
3. Configurar: **USB CDC On Boot: Enabled**, **Flash Mode: DIO** (crítico — OPI rompe I2S)
4. Compilar y cargar al microcontrolador
5. Monitor serie a **115200 baud** para diagnóstico

### Bibliotecas requeridas

- ESP32 Arduino core (incluye `driver/i2s_std.h`)
- `Wire.h` — I2C para MPU6050 *(MIDI_Drum y firmwares que usan IMU)*
- `USB.h` / `USBMIDI.h` — MIDI USB *(MIDI_Drum, drum_midi_leds)*
- Biblioteca `MPU6050` *(MIDI_Drum y firmwares que usan IMU)*
- **FastLED** *(test_leds, drum_midi_leds)*

---

## Estructura del repositorio

```
percusynth/
├── firmwares/                      # Sketches Arduino escritos a mano
│   ├── drum_machine_basic/         #   Drum machine con secuenciador de pasos
│   ├── MIDI_Drum/                  #   Controlador MIDI (piezo + IMU + botones)
│   ├── synth_basico/               #   Sintetizador polifónico con morphing
│   ├── test_leds/                  #   Test de tira LED WS2812 — 6 modos
│   ├── drum_midi_leds/             #   Drum machine + MIDI + LEDs sincronizadas
│   ├── step_sequencer_midi/        #   Secuenciador 6×16 + MIDI Clock Master
│   └── dub_siren/                  #   Dub siren (en desarrollo · PLAN.md)
├── tools/                          # Webapps standalone (Chrome/Edge)
│   ├── percu_control/              #   Panel universal + flasheo desde el navegador
│   ├── sample_loader/              #   Genera .ino con samples one-shot
│   ├── loop_loader/                #   Genera .ino con loops + hits
│   ├── step_sequencer_loader/      #   Genera .ino + control remoto vía Web MIDI
│   ├── dub_siren_generator/        #   Genera .ino dub siren con samples
│   └── loops/
│       └── bpm_mono_44100/         #   Editor BPM-aware de loops sincronizados
├── samples/                        # Firmwares generados por las webapps (ejemplos vivos)
├── Imagenes/                       # Renders 3D y diagrama de pinout
└── Documentos/                     # Informe técnico y prompt para IA
```

Cada subcarpeta de `firmwares/` y `tools/` tiene su propio `README.md` (o `PLAN.md`) con los detalles del firmware o de la webapp.

---

## Proyecto hermano

Este laboratorio es parte del mismo ecosistema que el **[Proto-Synth v2](https://github.com/GC-Lab-Gonzalo/proto-synth-v2)**, otra plataforma de experimentación de GC Lab Chile, con hardware diferente y una colección más amplia de firmwares de ejemplo.

---

Desarrollado por **GC Lab Chile** — electrónica, arte y tecnología abierta.
