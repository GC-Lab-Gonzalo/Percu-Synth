# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Structure

```
percusynth/
├── firmwares/                      # Hand-written Arduino sketches
│   ├── drum_machine_basic/         #   16-step drum machine + real-time synth
│   ├── synth_basico/               #   5-voice polyphonic synth
│   ├── trance_imu/                 #   Polyphonic trance sequencer (IMU → filter)
│   ├── trance_imu_leds/            #   trance_imu + 6 on-board SMD LEDs as visualizer
│   ├── pads_imu/                   #   Deep ambient pads: 5 buttons play sustained chords (no sequencer, stereo)
│   ├── pads_imu_leds/              #   pads_imu + 6 on-board SMD LEDs as visualizer (panel palette + arp running dot)
│   ├── impact_chimes/              #   Floor-impact chimes (accelerometer triggers scale notes)
│   ├── seismic_drone/              #   Deep drones from ground vibration (±2g)
│   ├── MIDI_Drum/                  #   MIDI controller (buttons + piezos + IMU)
│   ├── drum_midi_leds/             #   Drum MIDI + cinematic LED show
│   ├── trance_midi_leds/           #   Mono melodic trance over MIDI + 20×20 matrix show
│   ├── matrix_midi_anyma/          #   20×20 matrix electro sequencer + MIDI Clock Master + 2D visual engine
│   ├── dub_siren/                  #   Dub siren (in development — PLAN.md only)
│   ├── test_leds/                  #   WS2812 strip test (6 animation modes)
│   ├── test_imu/                   #   IMU test over Serial Monitor
│   ├── test_imu_led/               #   IMU test without USB → output on LEDs
│   └── test_imu_sound/             #   IMU test without USB → output via DAC sound
├── tools/                          # Standalone webapps (Chrome/Edge)
│   ├── percu_control/              #   Universal panel + browser-side firmware flashing
│   ├── sample_loader/              #   Generates .ino with embedded one-shot samples
│   ├── loop_loader/                #   Generates .ino with loops + hits
│   ├── step_sequencer_loader/      #   Generates sample-sequencer .ino + live Web MIDI remote
│   ├── dub_siren_generator/        #   Generates dub siren .ino with embedded samples
│   ├── midi_sampler/               #   MIDI-USB sampler: live Web Audio instrument + generates .ino
│   ├── video_synth/                #   Audiovisual video synth driven over Web Serial
│   ├── arp_matrix/                 #   Polyphonic arpeggiator + horizontal 64×32 round-LED matrix, 3 control panels (Web Audio + Web Serial) + generates .ino
│   └── loops/bpm_mono_44100/       #   BPM-aware loop editor
├── videogame/
│   └── cyber_flight/               # NEON STRIKE: cyberpunk first-person shooter (Web Serial)
├── samples/                        # Auto-generated firmware examples from the loaders
├── Hardware/                       # Schematic, PCB and gerbers of the circuit
├── Imagenes/                       # 3D renders and pinout diagrams
└── Documentos/                     # Technical report (PDF)
```

A single master context document for AIs lives at the repo root: `PROMPT_PARA_LA_IA.md` (+ `.pdf`).

Each subdirectory of `firmwares/` and `tools/` has its own `README.md` (or `PLAN.md` for work-in-progress).

## Project Overview

PercuSynth is an embedded electronic percussion synthesizer project by GC Lab Chile. It contains multiple independent Arduino/ESP32-S3 firmware sketches plus a collection of webapps that **generate `.ino` files with samples embedded as `PROGMEM` arrays** (compile-time samples — no runtime loading).

### Hand-written firmwares (under `firmwares/`)

- **drum_machine_basic** — 16-step sequencer drum machine with real-time synthesis (no samples)
- **synth_basico** — 5-voice polyphonic synthesizer with waveform morphing (sine → square → saw)
- **trance_imu** — Polyphonic trance sequencer ported from Proto-Synth v2 to I2S 44.1 kHz/16-bit; each step fires a 4-voice chord over a 16-voice pool; PolyBLEP saw + resonant biquad LPF driven live by the IMU (X → cutoff, Y → resonance). No LEDs/Serial (all CPU to audio)
- **trance_imu_leds** — Same as trance_imu but uses the 6 on-board SMD WS2812 LEDs as a visualizer (poly VU + beat flash + filter-driven color)
- **pads_imu** — Ambient sibling of trance_imu: same audio engine but **no sequencer/patterns**. The 5 buttons latch sustained **absolute chords** as deep pads (attack→sustain→release over a 32-voice pool, **stereo** with per-voice detune/pan + dual biquad). **5 chord banks** of 6 chords (BTN1..5 + a 6th via BTN1+BTN3), cycled by BTN3 in Panel B (without retriggering the pad). Optional **arpeggio** layer (4 notes, 6 types: up/down/up-down/down-up/random/chord) over the active chord (Panel B pots: volume/speed/range/gate). Optional **AUTO mode** (BTN4 Panel C): seed-fixed generative chord bed (diatonic functional progression that loops in 4/4, random-but-fixed chord durations) + arpeggio playing random in-scale notes. 3 panels: A (chords + attack/volume/release/movement-LFO), B (transpose/octave/bank + arp), C (waveform sine/saw/sq/tri + sub live layer + arp-type + AUTO, detune/tone/cutoff/Q). IMU → filter. No LEDs/Serial (all CPU to audio)
- **pads_imu_leds** — Same as pads_imu but uses the 6 on-board SMD WS2812 LEDs as a visualizer: per-panel palette (A cyan / B violet / C orange) + panel-change flash, pad-energy VU bar, and a **running dot that advances with each arpeggio note** (the "beat" of trance_imu_leds adapted to the arp). Also drives the ESP32-S3 module's addressable **RGB LED (GPIO48)** mirroring the strip's average color/energy. Same audio engine + anti-glitch work. Requires FastLED
- **impact_chimes** — Floor-impact instrument: accelerometer detects hits on the floor → triggers scale notes (chimes); one scale per button, pots = synthesis
- **seismic_drone** — Deep ambient sibling of impact_chimes: MPU6050 at ±2g senses ground vibration → epic drone (detuned stereo saw + sub, breathing resonant filter)
- **MIDI_Drum** — Hardware MIDI USB controller using piezo sensors, buttons, and IMU (MPU6050); velocity-by-IMU-motion for button hits
- **drum_midi_leds** — Drum machine MIDI controller with cinematic full-strip LED effects (one effect per drum type, additive mixing, 8-event pool)
- **trance_midi_leds** — Same sequencer engine as trance_imu but **monophonic & melodic**: one note per step over USB MIDI (true mono), IMU → MIDI CC (CC74/CC71), plus a "fiesta electrónica" show on the 20×20 matrix
- **matrix_midi_anyma** — Anyma-style electro audiovisual machine for the 20×20 WS2812 matrix: internal 16-step sequencer (4 patterns) sends drums (ch10) + bass (ch1) over USB MIDI, acts as **MIDI Clock Master**, and drives a 2D visual engine (5 scenes: NEXUS/TUNNEL/SPECTRUM/STORM/GRID) reacting to both the internal sequencer and incoming MIDI notes. No audio (visual + MIDI controller only)
- **dub_siren** — Work-in-progress dub siren firmware (`.ino` generated by `tools/dub_siren_generator/`)
- **test_leds / test_imu / test_imu_led / test_imu_sound** — Minimal hardware-diagnostic sketches (LED strip; IMU over Serial; IMU shown on LEDs; IMU rendered as sound). The IMU tests without USB avoid CDC-induced resets

### Tool-generated firmwares (under `samples/`)

These `.ino` files are **artifacts** produced by the webapps in `tools/` — not hand-written. They're real working examples of what the loaders generate (with samples already embedded as `PROGMEM` arrays). When working on these files, prefer regenerating from the webapp over editing them by hand.

### Webapps (under `tools/`)

All are standalone HTML files (no build step). Pattern: drop audio files in the browser → app generates a `.ino` → user flashes it via Arduino IDE. Exception: `percu_control` flashes a pre-built `firmware.bin` via ESP Web Tools, and `step_sequencer_loader` also acts as a **live Web MIDI remote** for the running PercuSynth.

- **midi_sampler** — Reads a USB MIDI controller via **Web MIDI** and plays **one** loaded sample (e.g. a bell) as a live **Web Audio** instrument, pitched by the incoming MIDI note relative to a configurable base note (per-voice: linear-interp resampling, AR envelope, one-pole LPF; velocity→gain; sine fallback when no sample is loaded). **4 on-screen knobs (= the 4 hardware pots, ADC 1/2/8/10)** for volume/attack/decay/cutoff, also movable via MIDI CC (7/73/72/74). Testable with no hardware (on-screen keyboard + PC keys). A second tab **generates the `.ino`** that turns the PercuSynth itself into a 1-sample MIDI-USB sampler with the same synthesis mapped to the 4 pots.
- **video_synth** — Single-page webapp that imports a video and synthesizes it into image **and** sound in real time, driven by the PercuSynth over **Web Serial** (or the PC mic). Minimalist: each control does one obvious thing.

### Game (under `videogame/`)

- **cyber_flight** — **NEON STRIKE**, a cyberpunk first-person flight-shooter game (not a generator). Reads the PercuSynth over **Web Serial** (`p0..p3,b1..b5,imuX,imuY` @ 50 Hz): the **two IMU axes** steer a center-locked reticle, **BTN5** fires from the right edge, **BTN1** fires from the left edge, **BTN2+BTN4 together** drop a screen-clearing bomb. Canvas-2D synthwave city with Web Audio SFX and a wave/score/multiplier system. Ships its own firmware (`neon_strike_control_percusynth.ino`, embedded download — sends X **and** Y accel). Fully playable with **mouse+keyboard** when no hardware is connected.

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

All audio firmwares follow the same pattern: a `setup()` that initializes hardware, and a `loop()` that processes inputs and fills I2S DMA buffers with 128-sample blocks (or sends MIDI events).

### drum_machine_basic
- **Voice struct** (10 polyphonic voices): dual-oscillator, LCG noise, 2-stage cascaded biquad filter state
- `triggerDrum()` — sets voice parameters per drum type (kick, snare, hihat, crash, click)
- `renderVoice()` — generates one sample: pitch sweep + noise mix + biquad filtering + envelope decay
- `biquadBP()` / `processBQ()` — Direct Form II Transposed biquad bandpass filter
- Pattern sequencer: `bool pattern[4][16]` stepped by BPM tempo from pot
- Main loop: fills 128-sample buffer → `i2s_channel_write()`

### MIDI_Drum
- No audio output — translates physical inputs to USB MIDI note events on channel 10 (GM drums)
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

### test_leds
- Pure FastLED demo — no audio
- 6 animation modes: SOLID, CHASE, RAINBOW, TWINKLE, PULSE (breathing), METEOR
- LEDs 0–5 are SMD internals on the PCB → always kept off via `apagarInternos()`
- POT4 changes meaning per mode (tail length, density, sparkles, etc.)

### drum_midi_leds
- Combines MIDI USB output (ch 10 GM drums) with cinematic full-strip LED effects
- Event pool (`MAX_EVENTS 8`) with additive mixing — each drum hit spawns one event
- Per-drum effect: `fxKick` (shockwave), `fxSnare` (flash + sparks), `fxHihat` (alternating-direction cyan bolt), `fxCrash` (supernova fading to rainbow)
- Background ambient: sinusoidal hue wave with warm orange pulse on strong beats
- Step indicator: comet trail moving across the strip in sync with the 16-step sequencer

### dub_siren *(in development)*
- See `firmwares/dub_siren/PLAN.md` for full architecture
- Generated by `tools/dub_siren_generator/` — `.ino` already exists with 3 placeholder samples baked in
- 3 polyphonic samples (hold-to-play) + siren oscillator + tape-style delay with feedback + IMU pitch global

## Key Constants (audio firmwares)

- `SAMPLE_RATE 44100`
- `BUF_SAMPLES 128`
- Pot reading uses 8× oversampling (`readPot()`)
- Biquad and filter coefficients computed at runtime from pot values each buffer cycle

## Code Conventions

- Headers in `.ino` files follow the **proto-synth-v2 format**: HARDWARE / ARDUINO IDE SETTINGS / LIBRERÍAS REQUERIDAS / DESCRIPCIÓN / FUNCIONAMIENTO blocks separated by `====...` lines
- Comments and variable names are in Spanish (project convention)
- Standard Arduino `.ino` format (C++11)
- Webapps (`tools/`) are single-file HTML — no build step, no npm. Inline CSS + JS, all assets self-contained per tool
