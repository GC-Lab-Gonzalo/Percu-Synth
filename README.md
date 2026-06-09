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
- Instrumentos por impacto y drones por vibración (el equipo apoyado en el piso)
- Reproducción de samples y loops cargados desde el navegador
- Experimentos con filtros, osciladores, envolventes y efectos digitales
- Visuales reactivas en tira o matriz LED WS2812 sincronizadas con sonido o gesto
- Síntesis audiovisual de video y hasta un videojuego controlado por el hardware
- Integración con software como Ableton, GarageBand, Pure Data, etc.

Los firmwares y herramientas del repositorio son ejemplos concretos de estas posibilidades. El proyecto crece con cada experimento nuevo.

---

## Hardware

<p align="center">
  <img src="Imagenes/percu-synth pinout.jpeg" alt="PercuSynth - Pinout" width="600"/>
</p>

> **Tip para usar con IA:** puedes subir directamente la imagen del pinout a cualquier asistente de IA (Claude, ChatGPT, Gemini, etc.) para que entienda el hardware y te ayude a crear nuevos firmwares. Mejor aún: pásale el [documento de contexto para IA](PROMPT_PARA_LA_IA.md) ([versión PDF](PROMPT_PARA_LA_IA.pdf)) — un documento madre con pinout, settings del Arduino IDE, librerías y patrones de código listos para que la IA genere firmware a la primera.

### Componentes principales

- **Microcontrolador:** ESP32-S3 (USB nativo, WiFi, Bluetooth)
- **DAC de audio:** PCM5102 vía I2S — salida estéreo 44.1 kHz · 16-bit
- **IMU:** MPU6050 — acelerómetro + giroscopio para control gestual (I2C)
- **Entradas:** 5 botones, 4 potenciómetros, 4 sensores piezoeléctricos, 2 sensores analógicos externos
- **Salidas:** Audio I2S · tira/matriz LED WS2812 · MIDI USB nativo · MIDI DIN-5

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

### Archivos de circuito (`Hardware/`)

El diseño electrónico está abierto para que puedas fabricar tu propia placa o estudiar las conexiones:

- [`Schematic_Percu-synth_V1.pdf`](Hardware/Schematic_Percu-synth_V1.pdf) — esquemático completo
- [`PCB_1-PCB_PCB_Percu-synth_V1.pdf`](Hardware/PCB_1-PCB_PCB_Percu-synth_V1.pdf) — vista del PCB
- [`Gerber_Percu-synth_1-PCB_PCB_Percu-synth_V1.zip`](Hardware/Gerber_Percu-synth_1-PCB_PCB_Percu-synth_V1.zip) — gerbers listos para enviar a fabricar

---

## Firmwares disponibles

Cada firmware es un sketch Arduino independiente (`.ino`). Se compila y se carga por separado — no hay sistema de build centralizado. Los siguientes son los firmwares desarrollados hasta la fecha; el proyecto está en desarrollo activo y se irán sumando más experimentos con el tiempo.

### Síntesis y secuenciadores de audio

#### `drum_machine_basic` — Drum Machine con Secuenciador
- 10 voces polifónicas: kick, snare, hi-hat, crash, click
- Síntesis por osciladores + ruido LCG + filtros biquad bandpass en cascada
- Secuenciador de 4 pistas × 16 pasos con grabación en tiempo real
- Control de tempo y timbre por potenciómetros

#### `synth_basico` — Sintetizador Polifónico
- 5 voces con morphing de forma de onda (senoidal → cuadrada → diente de sierra)
- Vibrato LFO (±1.2%, 0.2–8.2 Hz) y filtro pasa-bajos one-pole controlados por potenciómetro
- Notas mapeadas a los botones: C4, D4, E4, F4, G4

#### `trance_imu` — Secuenciador de Trance Polifónico (IMU)
- Port del secuenciador de trance del **Proto-Synth v2** a I2S 44.1 kHz / 16-bit estéreo (audio de otra liga, buffers DMA no bloqueantes)
- **Polifónico:** cada paso dispara un acorde de 4 voces sobre un pool de 16 voces → textura "pluck de trance" con cola
- Osciladores sierra anti-aliasing (PolyBLEP) + filtro pasa-bajos resonante biquad **controlado en vivo por el IMU** (eje X → cutoff, eje Y → resonancia)
- 3 paneles de control (combos de botones): normal / notas-tonalidad / timbre-síntesis. Sin LEDs ni Serial: todo el CPU va al audio

#### `trance_imu_leds` — Trance Polifónico + 6 LEDs de placa
- Igual que `trance_imu`, pero usa los **6 LEDs WS2812 SMD internos** de la placa como visualizador (VU polifónico + flash al beat + color según el filtro IMU + paleta por panel)

#### `impact_chimes` — Campanas por golpe en el piso
- Instrumento por **impacto**: se apoya el equipo en el piso y el **acelerómetro detecta los golpes** → dispara notas de una escala tipo campanas (eólica por defecto)
- Una escala distinta por cada botón; los potenciómetros controlan la síntesis

#### `seismic_drone` — Drones épicos por vibración de la tierra
- Hermano grave de `impact_chimes`: el MPU6050 en **±2g** detecta la vibración del suelo → genera un **dron épico** (sierra estéreo desafinada + sub-oscilador, filtro resonante que "respira")
- Escalas épicas; los potenciómetros controlan la textura

#### `dub_siren` — *(en desarrollo)*
- Sintetizador "sirena dub" con 3 samples polifónicos + oscilador siren con LFO + delay tape con feedback
- Generación del firmware (con samples embebidos) vía `tools/dub_siren_generator/`
- Plan completo y arquitectura en [`firmwares/dub_siren/PLAN.md`](firmwares/dub_siren/PLAN.md)

### Controladores MIDI y máquinas audiovisuales

#### `MIDI_Drum` — Controlador MIDI de Percusión
- Convierte golpes físicos y gestos en mensajes MIDI USB (canal 10 / GM drums)
- Tres modalidades de entrada: botones (cola circular), sensores piezoeléctricos (peak-detection 15 ms) e IMU (ventana 20 ms)
- Debounce de 50 ms por piezo y 25 ms por botón
- Compatible con cualquier software o hardware que reciba MIDI

#### `drum_midi_leds` — Drum Machine + MIDI + Luces
- Secuenciador de 16 pasos × 4 drums (kick, snare, hi-hat, crash) sincronizado con efectos full-strip cinematográficos en la tira WS2812
- Salida MIDI USB en canal 10 (notas 36, 38, 42, 49) — el PercuSynth queda como controlador de un DAW
- BTN5 alterna entre modo **GRAB** (grabación en tiempo real) y **PLAYBACK**
- Potes para brillo, tempo (60–240 BPM), color base del fondo y velocity MIDI (60–127)
- Cola de NoteOff diferidos para mantener los gates MIDI limpios

#### `trance_midi_leds` — Trance melódico MIDI + matriz 20×20
- El mismo motor de secuenciador de `trance_imu` pero **monofónico y melódico**: cada paso envía **una sola nota** por MIDI USB a tu DAW/sinte (true mono, sin notas solapadas)
- El IMU se traduce a **MIDI CC** (CC74 filtro / CC71 resonancia) para barrer el filtro del sinte moviendo el aparato
- En paralelo, la matriz 20×20 corre un show estilo **fiesta electrónica** reactivo al beat

#### `matrix_midi_anyma` — Máquina audiovisual electro (matriz 20×20)
- Máquina **estilo Anyma** que combina tres cosas: **secuenciador interno** de 16 pasos (drums ch10 + bajo *acid* ch1 por USB MIDI), **MIDI Clock Master** (24 PPQ) y un **motor visual 2D** de 5 escenas sobre la matriz WS2812 20×20
- Las visuales reaccionan tanto al secuenciador interno como a **notas MIDI entrantes** (un secuenciador externo también pinta la matriz)
- No genera audio: es controlador MIDI + visualizador

### Pruebas de hardware

Sketches mínimos de diagnóstico para verificar cada periférico. Varios evitan el USB/Serial a propósito (el CDC puede provocar reinicios durante las pruebas):

- **`test_leds`** — Test de la tira LED WS2812: 6 modos de animación (sólido, chase, rainbow, twinkle, pulso, meteor). LEDs 0-5 (SMD internos) siempre apagados; del 6 en adelante activos. Requiere **FastLED**
- **`test_imu`** — Comprobación mínima del IMU por **Monitor Serie**: WHO_AM_I, aceleración (g) y giro (°/s) de los 3 ejes
- **`test_imu_led`** — Comprueba el IMU **sin USB ni Serial**; el resultado se ve en la **tira LED**
- **`test_imu_sound`** — Comprueba el IMU por **sonido** (DAC), sin LEDs, USB ni Serial

---

## Herramientas web (`tools/`)

Páginas HTML standalone (sin build, sin npm) que cumplen distintos roles según la herramienta:

1. **Generadores de firmware** — drag & drop de audios → la webapp produce un `.ino` con los samples embebidos en flash como arrays `PROGMEM`. Lo flasheas con Arduino IDE y la web no se usa más hasta que cambies los samples. (`sample_loader`, `loop_loader`, `dub_siren_generator`, `loops/bpm_mono_44100`)
2. **Flasheo desde el navegador** — instala el firmware compilado directo al ESP32-S3 vía **ESP Web Tools**, sin abrir Arduino IDE. (`percu_control`)
3. **Control remoto en vivo** — se conecta al PercuSynth ya flasheado vía **Web MIDI** y edita patrón / FX / transport en tiempo real. (`step_sequencer_loader`)
4. **Instrumento en el navegador** — lee un controlador MIDI USB (**Web MIDI**) y suena al instante con **Web Audio**, además de generar su `.ino`. (`midi_sampler`)
5. **Síntesis audiovisual** — usa la PercuSynth (vía **Web Serial**) como controlador de visuales/sonido en el navegador. (`video_synth`)

La mayoría requiere **Chrome o Edge** (Firefox/Safari no soportan Web Serial ni Web MIDI). Cada herramienta tiene su propio `README.md` con detalles:

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

### [`midi_sampler/`](tools/midi_sampler/) — Sampler MIDI USB (1 sample · 4 pots)
Lee un controlador MIDI USB vía **Web MIDI** y reproduce **un solo sample** (ej. una campana) según la nota MIDI que entra, transpuesto desde una nota base configurable. 4 potenciómetros: volumen/attack/decay/cutoff (también por CC MIDI). Suena en el navegador con **Web Audio** (testeable sin hardware: teclado en pantalla + teclas del PC) y, sin sample cargado, usa un seno afinado. Pestaña aparte para **generar el `.ino`** que convierte al PercuSynth en un sampler MIDI físico de 1 sample.

### [`video_synth/`](tools/video_synth/) — Sintetizador audiovisual de video
Webapp de una página que **importa un video y lo sintetiza en imagen Y sonido** en tiempo real, controlado por la PercuSynth vía **Web Serial** (o el micrófono del PC). Minimalista: cada control hace una sola cosa obvia.

### [`loops/bpm_mono_44100/`](tools/loops/bpm_mono_44100/) — Editor BPM-aware de loops
Variante avanzada de `loop_loader` con tap-tempo, transporte master con beat dots, snap a compás y mono-switcher (un solo loop activo a la vez) + sampler polifónico paralelo. Para sesiones donde los loops tienen que estar sincronizados en BPM.

---

## Videojuego (`videogame/`)

### [`cyber_flight/`](videogame/cyber_flight/) — NEON STRIKE
Webapp de una página: un **shooter cyberpunk en primera persona** sobre una megaciudad distópica. Pilotas un caza y derribas naves enemigas. Controlado por la PercuSynth vía **Web Serial** (los **2 ejes del IMU** apuntan la mira, **BTN5/BTN1** disparan desde cada lado, **BTN2+BTN4** juntos sueltan una bomba). Totalmente jugable con **mouse + teclado** cuando no hay hardware conectado.

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

Luego abre <http://localhost:8000>, conecta el PercuSynth por USB y aprieta **⚡ FLASH FW**.

### Opción B — Desde Arduino IDE

1. Abrir el archivo `.ino` en **Arduino IDE**
2. Seleccionar placa: **ESP32S3 Dev Module** (o variante equivalente)
3. Configurar: **USB CDC On Boot: Enabled**, **Flash Mode: DIO** (crítico — OPI rompe I2S), **PSRAM: OPI PSRAM**
4. Compilar y cargar al microcontrolador
5. Monitor serie a **115200 baud** para diagnóstico

### Bibliotecas requeridas

- ESP32 Arduino core ≥ 3.x (incluye `driver/i2s_std.h`)
- `Wire.h` — I2C para el MPU6050 *(incluida en el core; la mayoría de los firmwares con IMU leen el sensor por registros crudos, sin librería extra)*
- `USB.h` / `USBMIDI.h` — MIDI USB *(incluidas en el core; MIDI_Drum, drum_midi_leds, trance_midi_leds, matrix_midi_anyma)*
- **FastLED** *(test_leds, drum_midi_leds, trance_imu_leds, trance_midi_leds, matrix_midi_anyma…)*
- Biblioteca `MPU6050` *(solo MIDI_Drum)*

---

## Estructura del repositorio

```
percusynth/
├── firmwares/                      # Sketches Arduino escritos a mano
│   ├── drum_machine_basic/         #   Drum machine con secuenciador de pasos
│   ├── synth_basico/               #   Sintetizador polifónico con morphing
│   ├── trance_imu/                 #   Secuenciador de trance polifónico (IMU→filtro)
│   ├── trance_imu_leds/            #   trance_imu + 6 LEDs SMD internos como visualizador
│   ├── impact_chimes/              #   Campanas por golpe en el piso (acelerómetro)
│   ├── seismic_drone/              #   Drones graves por vibración de la tierra
│   ├── dub_siren/                  #   Dub siren (en desarrollo · PLAN.md)
│   ├── MIDI_Drum/                  #   Controlador MIDI (piezo + IMU + botones)
│   ├── drum_midi_leds/             #   Drum machine + MIDI + LEDs sincronizadas
│   ├── trance_midi_leds/           #   Trance melódico mono por MIDI + matriz 20×20
│   ├── matrix_midi_anyma/          #   Máquina audiovisual electro + MIDI Clock Master (matriz 20×20)
│   ├── test_leds/                  #   Test de tira LED WS2812 — 6 modos
│   ├── test_imu/                   #   Test del IMU por Monitor Serie
│   ├── test_imu_led/               #   Test del IMU sin USB → resultado en LEDs
│   └── test_imu_sound/             #   Test del IMU sin USB → resultado por sonido
├── tools/                          # Webapps standalone (Chrome/Edge)
│   ├── percu_control/              #   Panel universal + flasheo desde el navegador
│   ├── sample_loader/              #   Genera .ino con samples one-shot
│   ├── loop_loader/                #   Genera .ino con loops + hits
│   ├── step_sequencer_loader/      #   Genera .ino + control remoto vía Web MIDI
│   ├── dub_siren_generator/        #   Genera .ino dub siren con samples
│   ├── midi_sampler/               #   Sampler MIDI USB de 1 sample + 4 pots (Web Audio + genera .ino)
│   ├── video_synth/                #   Sintetizador audiovisual de video (Web Serial)
│   └── loops/
│       └── bpm_mono_44100/         #   Editor BPM-aware de loops sincronizados
├── videogame/
│   └── cyber_flight/               # NEON STRIKE — shooter cyberpunk (Web Serial)
├── samples/                        # Firmwares generados por las webapps (ejemplos vivos)
├── Hardware/                       # Esquemático, PCB y gerbers del circuito
├── Imagenes/                       # Renders 3D y diagrama de pinout
├── Documentos/                     # Informe técnico (PDF)
├── PROMPT_PARA_LA_IA.md            # Documento de contexto para IA (+ versión .pdf)
└── Percu-Synth.mp4                 # Video del proyecto
```

Cada subcarpeta de `firmwares/` y `tools/` tiene su propio `README.md` (o `PLAN.md`) con los detalles del firmware o de la webapp.

---

## Proyecto hermano

Este laboratorio es parte del mismo ecosistema que el **[Proto-Synth v2](https://github.com/GC-Lab-Gonzalo/proto-synth-v2)**, otra plataforma de experimentación de GC Lab Chile, con hardware diferente y una colección más amplia de firmwares de ejemplo.

---

Desarrollado por **GC Lab Chile** — electrónica, arte y tecnología abierta.
