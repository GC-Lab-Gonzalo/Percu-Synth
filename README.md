# PercuSynth

**PercuSynth** es un sintetizador de percusión electrónica embebida desarrollado por **GC Lab Chile**. El proyecto consiste en tres firmwares independientes para ESP32 que permiten crear y controlar sonidos de batería y síntesis en tiempo real mediante hardware de bajo costo.

<p align="center">
  <img src="Imagenes/percu-synth modelo 3d isometrica.jpeg" alt="PercuSynth - modelo 3D" width="600"/>
</p>

---

## Firmwares

### 1. `drum_machine_basic` — Drum Machine con Secuenciador
Batería electrónica autónoma con síntesis de audio en tiempo real y secuenciador de 16 pasos.

- 10 voces polifónicas (kick, snare, hi-hat, crash, click)
- Síntesis por osciladores + ruido + filtros biquad en cascada
- Secuenciador 4 pistas × 16 pasos con grabación en tiempo real
- Control de tempo y timbre por potenciómetros
- Salida de audio I2S a DAC PCM5102 a 44.1 kHz

### 2. `MIDI_Drum` — Controlador MIDI de Percusión
Convierte golpes físicos y gestos en mensajes MIDI USB estándar.

- **3 modalidades de entrada:**
  - Botones digitales → notas MIDI directas
  - Sensores piezoeléctricos → detección de impacto + velocidad proporcional
  - IMU MPU6050 → detección de movimiento por aceleración
- Salida MIDI USB (canal 9 — batería estándar General MIDI)
- Anti-retrigger: debounce de 50 ms por piezo

### 3. `synth_basico` — Sintetizador Polifónico
Sintetizador de 5 voces con morphing de forma de onda y modulación LFO.

- Mezcla continua entre onda senoidal, cuadrada y diente de sierra
- Vibrato LFO senoidal (0.2–8.2 Hz)
- Filtro pasa-bajos de un polo con control por potenciómetro
- Notas: C4, D4, E4, F4, G4
- Salida de audio I2S a DAC PCM5102 a 44.1 kHz

---

## Hardware

<p align="center">
  <img src="Imagenes/percu-synth pinout.jpeg" alt="PercuSynth - Pinout" width="600"/>
</p>

### Componentes principales
- **Microcontrolador:** ESP32 (o ESP32-S3)
- **DAC de audio:** PCM5102 vía I2S
- **IMU:** MPU6050 vía I2C (solo MIDI_Drum)
- **Entradas:** 5 botones, 4 potenciómetros, 4 sensores piezoeléctricos

### Pinout (firmwares de audio)

| Señal | Pin ESP32 |
|-------|-----------|
| I2S LRCK | 39 |
| I2S DATA | 40 |
| I2S BCLK | 41 |
| Botones | 44, 42, 0, 45, 47 |
| Potenciómetros | ADC 1, 2, 8, 10 |
| Piezos (MIDI_Drum) | ADC 4, 5, 6, 7 |

---

## Configuración y Carga

1. Abrir el archivo `.ino` correspondiente en **Arduino IDE**
2. Seleccionar placa: **ESP32**
3. Compilar y cargar al microcontrolador
4. Monitor serie a **115200 baud** para diagnóstico

### Bibliotecas requeridas
- ESP32 Arduino core (`driver/i2s_std.h` incluido)
- `Wire.h` — I2C para MPU6050 *(solo MIDI_Drum)*
- `USB.h` / `USBMIDI.h` — MIDI USB *(solo MIDI_Drum)*
- Biblioteca `MPU6050` *(solo MIDI_Drum)*

---

## Estructura del Repositorio

```
percusynth/
├── firmwares/
│   ├── drum_machine_basic/   # Drum machine con secuenciador
│   ├── MIDI_Drum/            # Controlador MIDI por piezo e IMU
│   └── synth_basico/         # Sintetizador polifónico
├── Imagenes/                 # Renders 3D y pinout
└── Documentos/               # Informe técnico del proyecto
```

---

## Desarrollado por

**GC Lab Chile**
Laboratorio de desarrollo de hardware y síntesis de audio embebida.
