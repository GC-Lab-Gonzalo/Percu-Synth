# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Structure

```
percusynth/
├── firmwares/
│   ├── drum_machine_basic/   # Drum machine + step sequencer
│   ├── MIDI_Drum/            # MIDI controller (piezo + IMU)
│   └── synth_basico/         # Polyphonic synthesizer
├── Imagenes/                 # 3D renders and pinout diagrams
├── Documentos/               # Technical report (Word)
└── README.md                 # Spanish-language project README
```

## Project Overview

PercuSynth is an embedded electronic percussion synthesizer project by GC Lab Chile. It contains three independent Arduino/ESP32 firmware sketches:

- **drum_machine_basic** — 16-step sequencer drum machine with real-time synthesis
- **MIDI_Drum** — Hardware MIDI controller using piezo sensors, buttons, and IMU (MPU6050)
- **synth_basico** — 5-voice polyphonic synthesizer with waveform morphing

## Build & Flash

Each firmware is a standalone Arduino sketch (`.ino`). There is no centralized build system.

1. Open the desired `.ino` file in Arduino IDE (or PlatformIO)
2. Select board: **ESP32** (or ESP32-S3 depending on hardware revision)
3. Compile and upload to the board
4. Monitor via Serial at **115200 baud** for diagnostic output

**Required Libraries:**
- ESP32 Arduino core (includes `driver/i2s_std.h`)
- `Wire.h` — I2C for MPU6050 (MIDI_Drum only)
- `USB.h` and `USBMIDI.h` — USB MIDI (MIDI_Drum only)
- `MPU6050` library (MIDI_Drum only)

## Hardware Pinout (shared across audio firmwares)

| Signal | Pin |
|--------|-----|
| I2S LCK (LRCK) | 39 |
| I2S DIN (DATA) | 40 |
| I2S BCK (BCLK) | 41 |
| Buttons | 44, 42, 0, 45, 47 |
| Potentiometers | ADC 1, 2, 8, 10 |
| Piezo sensors | ADC 4, 5, 6, 7 |
| LED WS2812 data | 46 |
| MIDI DIN-5 TX | 43 |
| I2C SDA (MPU6050) | 21 |
| I2C SCL (MPU6050) | 38 |
| External sensor A | ADC 3 |
| External sensor B | ADC 9 |

Audio output targets a **PCM5102 DAC** via I2S at 44.1 kHz, 16-bit stereo.

## Architecture

All three firmwares follow the same pattern: a `setup()` that initializes hardware, and a `loop()` that processes inputs and fills I2S DMA buffers with 128-sample blocks (or sends MIDI events).

### drum_machine_basic
- **Voice struct** (10 polyphonic voices): dual-oscillator, LCG noise, 2-stage cascaded biquad filter state
- `triggerDrum()` — sets voice parameters per drum type (kick, snare, hihat, crash, click)
- `renderVoice()` — generates one sample: pitch sweep + noise mix + biquad filtering + envelope decay
- `biquadBP()` / `processBQ()` — Direct Form II Transposed biquad bandpass filter
- Pattern sequencer: `bool pattern[4][16]` stepped by BPM tempo from pot
- Main loop: fills 128-sample buffer → `i2s_channel_write()`

### MIDI_Drum
- No audio output — translates physical inputs to USB MIDI note events on channel 9
- **Three input modalities:**
  - Buttons → MIDI notes via 5-element circular queue (`btnQueue`)
  - Piezo sensors → 15 ms peak-detection window → velocity (40–127) → note on/off
  - MPU6050 IMU → 20 ms acceleration peak window → velocity → note on/off
- `accelMag()` reads MPU6050 via I2C and returns 3-axis acceleration magnitude
- Retrigger prevention: 50 ms debounce per piezo, 25 ms per button

### synth_basico
- **Voice struct** (5 voices): single phase accumulator, exponential envelope, active/pressed flags
- `mixedWave()` — morphs smoothly between sine, square, and sawtooth based on pot reading
- LFO vibrato: sinusoidal ±1.2% frequency deviation at 0.2–8.2 Hz
- One-pole lowpass filter on final mix output
- Buttons map to C4, D4, E4, F4, G4 (261.6, 293.7, 329.6, 349.2, 392 Hz)

## Key Constants

All audio firmwares use:
- `SAMPLE_RATE 44100`
- `BUF_SAMPLES 128`
- Pot reading uses 8× oversampling (`readPot()`)
- Biquad and filter coefficients computed at runtime from pot values each buffer cycle
