# PercuSynth

**PercuSynth** es un laboratorio portátil de experimentación con electrónica, programación y síntesis de audio, desarrollado por **GC Lab Chile**.

La percusión y los sintetizadores son la excusa: el arte y la música son la puerta de entrada a un proceso de aprendizaje más amplio, donde explorar electrónica, código y tecnología se vuelve natural, entretenido y con sentido. Es un proyecto diseñado bajo la metodología **STEAM** — donde la ciencia, la tecnología, la ingeniería y las matemáticas se integran con el arte como motor creativo.

<p align="center">
  <img src="Imagenes/percu-synth modelo 3d isometrica.jpeg" alt="PercuSynth - modelo 3D" width="600"/>
</p>

El hardware es compacto, portátil y basado en el microcontrolador **ESP32**. Desde él se puede experimentar con síntesis de sonido, secuenciadores, controladores MIDI, sensores de movimiento y mucho más. Los firmwares disponibles son solo el punto de partida — el proyecto está en desarrollo activo y las posibilidades son abiertas.

---

## ¿Qué se puede hacer con PercuSynth?

El laboratorio permite explorar una amplia variedad de ideas, por ejemplo:

- Sintetizadores y máquinas de ritmo con síntesis de audio en tiempo real
- Controladores MIDI para software de producción musical
- Secuenciadores de pasos con patrones grabables
- Detección de golpes y gestos con sensores piezoeléctricos e IMU
- Experimentos con filtros, osciladores, envolventes y efectos digitales
- Integración con software como Ableton, GarageBand, Pure Data, etc.

Los firmwares del repositorio son ejemplos concretos de estas posibilidades. El proyecto crece con cada experimento nuevo.

---

## Hardware

<p align="center">
  <img src="Imagenes/percu-synth pinout.jpeg" alt="PercuSynth - Pinout" width="600"/>
</p>

> **Tip para usar con IA:** puedes subir directamente la imagen del pinout a cualquier asistente de IA (Claude, ChatGPT, Gemini, etc.) para que entienda el hardware y te ayude a crear nuevos firmwares. Ver también el [prompt listo para copiar](Documentos/PROMPT_IA.md).

### Componentes principales

- **Microcontrolador:** ESP32 (o ESP32-S3)
- **DAC de audio:** PCM5102 vía I2S (salida estéreo de alta calidad)
- **IMU:** MPU6050 — acelerómetro + giroscopio para control gestual (I2C)
- **Entradas:** 5 botones, 4 potenciómetros, 4 sensores piezoeléctricos

### Pinout

| Señal | Pin ESP32 |
|-------|-----------|
| I2S LRCK | 39 |
| I2S DATA | 40 |
| I2S BCLK | 41 |
| Botones | 44, 42, 0, 45, 47 |
| Potenciómetros | ADC 1, 2, 8, 10 |
| Piezos | ADC 4, 5, 6, 7 |

Todos los firmwares de audio generan señal a **44.1 kHz, 16-bit estéreo** a través del DAC PCM5102.

---

## Firmwares disponibles

Los siguientes firmwares son los primeros desarrollados para el laboratorio. El proyecto está en etapa temprana y se irán sumando más experimentos con el tiempo.

### `drum_machine_basic` — Drum Machine con Secuenciador
- 10 voces polifónicas: kick, snare, hi-hat, crash, click
- Síntesis por osciladores + ruido + filtros biquad en cascada
- Secuenciador de 4 pistas × 16 pasos con grabación en tiempo real
- Control de tempo y timbre por potenciómetros

### `MIDI_Drum` — Controlador MIDI de Percusión
- Convierte golpes físicos y gestos en mensajes MIDI USB
- Tres modalidades de entrada: botones, sensores piezoeléctricos e IMU
- Compatible con cualquier software o hardware que reciba MIDI (canal 9)

### `synth_basico` — Sintetizador Polifónico
- 5 voces con morphing de forma de onda (senoidal → cuadrada → diente de sierra)
- Vibrato LFO y filtro pasa-bajos controlados por potenciómetro
- Notas: C4, D4, E4, F4, G4

---

## Cómo cargar un firmware

1. Abrir el archivo `.ino` en **Arduino IDE**
2. Seleccionar placa: **ESP32**
3. Compilar y cargar al microcontrolador
4. Monitor serie a **115200 baud** para diagnóstico

### Bibliotecas requeridas

- ESP32 Arduino core (incluye `driver/i2s_std.h`)
- `Wire.h` — I2C para MPU6050 *(solo MIDI_Drum)*
- `USB.h` / `USBMIDI.h` — MIDI USB *(solo MIDI_Drum)*
- Biblioteca `MPU6050` *(solo MIDI_Drum)*

---

## Estructura del repositorio

```
percusynth/
├── firmwares/
│   ├── drum_machine_basic/   # Drum machine con secuenciador de pasos
│   ├── MIDI_Drum/            # Controlador MIDI (piezo + IMU + botones)
│   └── synth_basico/         # Sintetizador polifónico con morphing
├── Imagenes/                 # Renders 3D y diagrama de pinout
└── Documentos/               # Informe técnico y prompt para IA
```

---

## Proyecto hermano

Este laboratorio es parte del mismo ecosistema que el **[Proto-Synth v2](https://github.com/GC-Lab-Chile/proto-synth-v2)**, otra plataforma de experimentación de GC Lab Chile, con hardware diferente y una colección más amplia de firmwares de ejemplo.

---

Desarrollado por **GC Lab Chile** — electrónica, arte y tecnología abierta.
