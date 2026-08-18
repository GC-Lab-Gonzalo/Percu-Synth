# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Structure

```
percusynth/
├── firmwares/                      # Hand-written Arduino sketches
│   ├── drum_machine_basic/         #   16-step drum machine + real-time synth
│   ├── drum_ruido/                 #   Noisy-timbre drum machine with a CLEAN output chain: random 32-step patterns, 5 kits, tap tempo, half-time, fill, pot beat-repeat
│   ├── synth_basico/               #   5-voice polyphonic synth
│   ├── trance_imu/                 #   Polyphonic trance sequencer (IMU → filter)
│   ├── trance_imu_leds/            #   trance_imu + 6 on-board SMD LEDs as visualizer
│   ├── pads_imu/                   #   Deep ambient pads: 5 buttons play sustained chords (no sequencer, stereo)
│   ├── pads_imu_leds/              #   pads_imu + 6 on-board SMD LEDs as visualizer (panel palette + arp running dot)
│   ├── cancion_aleatoria_leds/     #   Autonomous random-song machine (Play/Stop + 3 volume pots): generative key/mode/progression/tempo + random monophonic melody + random synthesized percussion + 6 LEDs
│   ├── paisajes_relax_leds/        #   Relaxing soundscape machine: 10 combinable synthesized layers (wind/sea/chimes/drops+echo/…) + 2 WS2812 rings of 30 LEDs
│   ├── cyber_kit/                  #   Cyber texture/FX/lead sequencer (no drums): 4 sound banks tuned to 10 scales, 4 pot panels, PERC/SEQ modes with full transport (play/stop/reverse/beat-repeat/speed/chaos), POTS/IMU filter
│   ├── asistente_ia/               #   Voice assistant (Whisper → GPT-4o-mini → TTS); the simplest of the AI family — read it first
│   ├── asistente_naga/             #   Same assistant over NagaAI (api.naga.ac): one key for STT/chat/TTS, free models, WAV-header-driven playback
│   ├── sampler_ia/                 #   Voice-prompted AI sampler: Whisper → GPT → ElevenLabs SFX → PSRAM slot, 3 trigger buttons + 16-step sequencer
│   ├── asistente_musical/          #   Voice chat with GPT over a never-stopping generative pad (voice high-pass on a pot + sidechain ducking; audio on core 1, network on core 0)
│   ├── oscilador_ia/               #   The AI sample IS the oscillator: ElevenLabs note @A2 → pitch-detect → tuned keyboard/arps/infinite drones/generative seq (10 scales)
│   ├── compositor_ia/              #   Ask for a song out loud: Whisper → GPT returns a compact song JSON → the cancion_aleatoria engine plays it (real swing, sections, 5 bass patterns); JSON can also be pasted over Serial with no WiFi
│   ├── oscilador_escalas/          #   Drone: 4 pots = 4 oscillators quantized to a scale (port of Proto-Synth v2's Oscilador_4_escalas without Mozzi) — unison+sub, 10 scales on one button, IMU X → resonant LPF, tempo gate + tap tempo, tape delay
│   ├── espacio_modular/            #   MONOPHONIC film-ambience machine (6-osc bank + continuous tonic drone): 24 curated 4-bar THEMES over a per-mode non-functional progression, one chord per 4-bar cycle, fixed-time ping-pong delay + reverb; single panel (5 buttons, 4 pots), Play rolls the mode, BTN2 rolls the theme, IMU X → cutoff
│   ├── impact_chimes/              #   Floor-impact chimes (accelerometer triggers scale notes)
│   ├── impact_chimes_leds/         #   impact_chimes + 68-LED WS2812 show + 3 timbres (chime/marimba/e-guitar), C Lydian
│   ├── seismic_drone/              #   Deep drones from ground vibration (±2g)
│   ├── MIDI_Drum/                  #   MIDI controller (buttons + piezos + IMU)
│   ├── drum_midi_leds/             #   Drum MIDI + cinematic LED show
│   ├── trance_midi_leds/           #   Mono melodic trance over MIDI + 20×20 matrix show
│   ├── matrix_midi_anyma/          #   20×20 matrix electro sequencer + MIDI Clock Master + 2D visual engine
│   ├── dub_siren/                  #   Dub siren (in development — PLAN.md only)
│   ├── test_system/                #   System monitor: live state of every peripheral over Serial + audio/LED self-tests
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
│   ├── scale_osc/                  #   Quantized pitch engine in the browser: 4 pots = 4 scale-quantized oscillators (browser sibling of oscilador_escalas)
│   ├── generador_estilos/          #   Musical-style engine where a style is DATA (JSON), not code — blues/techno/synthwave/grunge over Web Audio (in development)
│   └── loops/bpm_mono_44100/       #   BPM-aware loop editor
├── videogame/
│   ├── cyber_flight/               # NEON STRIKE: cyberpunk first-person shooter (Web Serial)
│   ├── tilt_maze/                  # Ball-maze game steered by tilting the IMU
│   └── nebula_gp/                  # NEBULA GP: FPV drone racing vs 4 bots (Web Serial)
├── samples/                        # Auto-generated firmware examples from the loaders
├── Hardware/                       # Schematic, PCB and gerbers of the circuit
├── Imagenes/                       # 3D renders and pinout diagrams
├── Documentos/                     # Technical report (PDF)
└── .claude/skills/percusynth/      # Installable skill: clone the repo and it loads itself
```

A single master context document for AIs lives at the repo root: `PROMPT_PARA_LA_IA.md` (+ `.pdf`).
It is the source of truth for pinout, IDE settings and canonical code patterns; `.claude/skills/percusynth/SKILL.md`
is the condensed version that Claude Code loads automatically and points back to it.

Most subdirectories of `firmwares/` and `tools/` have their own `README.md` (or `PLAN.md` for
work-in-progress). The more direct sketches — `drum_machine_basic`, `synth_basico`, `MIDI_Drum`,
`drum_midi_leds`, `trance_midi_leds` and the `test_*` ones — document themselves through the
header block of the `.ino`. New folders get a README.

## Project Overview

PercuSynth is an embedded electronic percussion synthesizer project by GC Lab Chile. It contains multiple independent Arduino/ESP32-S3 firmware sketches plus a collection of webapps that **generate `.ino` files with samples embedded as `PROGMEM` arrays** (compile-time samples — no runtime loading).

### Hand-written firmwares (under `firmwares/`)

- **drum_machine_basic** — 16-step sequencer drum machine with real-time synthesis (no samples)
- **drum_ruido** — Drum machine with deliberately **noisy timbres but a clean output chain** — the guiding rule after v1 was rejected for sounding bad: the dirt is made with *synthesis*, never by breaking the audio (no bit-crush, no sample-rate decimation, no output distortion; those produce aliasing, i.e. "frequencies that belong to no note"). **Patterns are random**: no factory rhythms — each BTN1 rolls a fresh 32-step pattern (2 bars of 16ths) with the active kit's feel (`posWeight()` decides where each track may fall; the step-0 kick is forced, since a 100 % random pattern with no anchor turns to mush). **5 kits** (CYBER · DUBSTEP half-time · GLITCH · INDUSTRIAL · CAOS) live in one `KITS[5]` table where each row is a whole timbre *and* its pattern densities. The 7 tracks are kick · snare · hats · **clank** (short iron) · **metal** (anvil) · **blast** (steam/air) · bass (clean sub). Metals are built from **inharmonic partials** (1.41, 2.37, 3.14…) — that's why they read as sheet metal and not as a note — and fast pitch sweeps are deliberately avoided: a short sweep through the mid range sounds like a bird chirp, not industrial percussion (a real complaint against an earlier version). Controls are flat — **one button, one function, on the press edge; every pot always means the same thing**: BTN1 pattern · BTN2 timbre · BTN3 tap tempo · BTN4 hold = **half-time** (each pattern step lasts two master-clock steps; the master keeps running so releasing lands back in time) · BTN5 hold = fill. POT1 volume · POT2 filter · POT3 dirt · POT4 **beat repeat** OFF/×2/×4/×8/×16 (loop lengths 8/4/2/1 steps, hysteresis on the zones, anchor searches backwards for a block that actually has hits or the break becomes silence) over a free-running master clock so it never drifts. There is no play/stop — the machine always runs; POT1 to zero is the mute. The sound rules, each from a measurement: **two envelopes per hit — one for the tone, one for the noise** (a 6 ms noise transient over a 200 ms body is what separates a kick from a "pfff"); the per-voice filter touches **only the noise layer** so the kick's fundamental survives; **decays are exponential taus**, so a hit rings ~3·tau — v1's kick had tau 0.42 and rang for nearly 2 s, becoming a bass drone that masked everything (now 0.13–0.30); **PolyBLEP on every waveform with corners**; the kick lands at ~50 Hz (not 36, which no small speaker reproduces) with its own soft saturation for audible harmonics; **sidechain** ducking the rest 3 dB per kick so the kick is *felt* rather than competing; `TRACK_GAIN[]` keeps the bass at 0.62 since it shares the kick's band; filter curve `pFilt^0.55` because a pure exponential put the pot's midpoint at 1.4 kHz and made the hats (7–12 kHz) vanish, plus a **filter envelope** (each hit opens the cutoff, deeper the more closed the pot is) and resonance up to 3.2 with `Q^0.35` makeup; **two-band saturation** (split at 2.2 kHz, lows up to ×4.35, highs barely touched — saturating the highs equally is what makes the aliasing screech); global filter coefficients interpolated **per sample**; DC blocker on the output. Measured with the filter open: crest factor 2.8–3.7, peak 0.76–0.83, zero NaN, zero samples at full scale; sweeping POT3 on an isolated kick lifts sub-100 Hz by 8–9 dB while the >12 kHz band moves 1–4 dB and stays 35 dB down. 10 one-shot voices, short DMA queue (4×128 ≈ 12 ms). 6 SMD LEDs as status only. No IMU/Serial. Requires FastLED
- **synth_basico** — 5-voice polyphonic synthesizer with waveform morphing (sine → square → saw)
- **trance_imu** — Polyphonic trance sequencer ported from Proto-Synth v2 to I2S 44.1 kHz/16-bit; each step fires a 4-voice chord over a 16-voice pool; PolyBLEP saw + resonant biquad LPF driven live by the IMU (X → cutoff, Y → resonance). No LEDs/Serial (all CPU to audio)
- **trance_imu_leds** — Same as trance_imu but uses the 6 on-board SMD WS2812 LEDs as a visualizer (poly VU + beat flash + filter-driven color)
- **pads_imu** — Ambient sibling of trance_imu: same audio engine but **no sequencer/patterns**. The 5 buttons latch sustained **absolute chords** as deep pads (attack→sustain→release over a 32-voice pool, **stereo** with per-voice detune/pan + dual biquad). **5 chord banks** of 6 chords (BTN1..5 + a 6th via BTN1+BTN3), cycled by BTN3 in Panel B (without retriggering the pad). Optional **arpeggio** layer (4 notes, 6 types: up/down/up-down/down-up/random/chord) over the active chord (Panel B pots: volume/speed/range/gate). Optional **AUTO mode** (BTN4 Panel C): seed-fixed generative chord bed (diatonic functional progression that loops in 4/4, random-but-fixed chord durations) + arpeggio playing random in-scale notes. 3 panels: A (chords + attack/volume/release/movement-LFO), B (transpose/octave/bank + arp), C (waveform sine/saw/sq/tri + sub live layer + arp-type + AUTO, detune/tone/cutoff/Q). IMU → filter. No LEDs/Serial (all CPU to audio)
- **pads_imu_leds** — Same as pads_imu but uses the 6 on-board SMD WS2812 LEDs as a visualizer: per-panel palette (A cyan / B violet / C orange) + panel-change flash, pad-energy VU bar, and a **running dot that advances with each arpeggio note** (the "beat" of trance_imu_leds adapted to the arp). Also drives the ESP32-S3 module's addressable **RGB LED (GPIO48)** mirroring the strip's average color/energy. Same audio engine + anti-glitch work. Requires FastLED
- **cancion_aleatoria_leds** — Autonomous sibling of pads_imu_leds: reuses its stereo voice engine + 6-LED visualizer but strips **all** manual control down to **BTN1 = Play/Stop** plus **POT1/POT2/POT3 = pad / melody / percussion volume** (no IMU). Each Play generates a whole coherent **random song**, anchored by one of **5 archetypes** (AMBIENT/CINEMATIC/PULSE/PLUCK/DRIVE) that fix a correlated global character so songs sound categorically different. Randomizes key + mode (7 scales) → diatonic **chord progression** (functional walk, I→…→V loop), **BPM 46–128**, **pad voicing** (triad/triad+oct/seventh/open) and **pad rhythm** (sustained or a clock-synced amplitude **gate** for PULSE/DRIVE), plus synthesis (pad waveform, sub, detune, per-voice brightness LPF, cutoff floor, resonance, autonomous filter LFO, pad attack/release). The arpeggio is **replaced by a true generative monophonic melody** organized in **phrases** (3–9 notes): each phrase rolls its own rhythm style (LONG/FLOW/RHYTHM/SPARSE/FAST — so long notes really happen), waveform + **brightness**, register (±octave), envelope (pluck/sustain/staccato) and optional sub-octave body; each note walks within the scale (+ chord-tone snapping = always in key) with on-grid random length. **Random synthesized percussion** (own 6-voice one-shot engine, no samples: pitch-sweep kick / tone+noise snare-rim / high-passed hats): a 16-step pattern, timbres and level are rolled per song by archetype (AMBIENT often has none; PULSE/DRIVE get four-on-floor + backbeat + hats), with beat accents, ghost hats and a snare mini-fill every 4 bars — always on the clock grid, mixed dry (bypasses the pad filter). Stop→Play = a brand-new song. Requires FastLED
- **paisajes_relax_leds** — Relaxing soundscape machine: **10 real-time synthesized layers** (no samples) that can be toggled and **freely combined** — wind (with calm moments), sea waves, wind chimes (C pentatonic, 3 inharmonic partials), brief water drops, crickets, distant campfire crackle, calm rain (pure hiss texture), deep Tibetan bowl drone (C2–G2, fundamental+fifth+octave), gentle stream, bird trills. **All dry and natural — no delay/reverb effects.** Each button toggles one layer (short press = primary, long press >0.6 s = alternate). Global pots: master volume / density / tonal color (LPF) / background↔events balance. Each active layer paints its own additive light effect on **two 30-LED WS2812 rings** (60-LED strip split in two circles: ring A outward = events/effects, ring B inward = ambient glow). The 6 on-board SMD LEDs are **status indicators** (LED 0-4 = per-button active-layer color, LED 5 = breathing when anything plays). Starts silent. Cross-interaction: wind gusts make the chimes tinkle more. No IMU/Serial. Requires FastLED
- **cyber_kit** — Cyber **texture/FX/lead sequencer** (NOT a drum machine, no samples): 4 banks × 5 sounds (LEADS/TEXTURAS/FX/BAJOS — neon leads, FM bells, hoovers, risers, granular clouds, dark drones, zaps, downlifters, glitch bursts, growls, reeses, subs) built from a parametric voice (pitch-sweep osc + detuned 2nd osc + FM + LCG noise through swept per-voice biquad BP + AM gate with per-grain random pitch + square pitch-LFO). Tuned sounds live **inside a selectable scale** (10: 7 Greek modes + Phrygian dominant flamenco, double harmonic gypsy, harmonic minor) with variable root. **Instant triggering**: buttons fire on the press edge (no wait); two-button combos within a 50 ms window **undo** each button's action (triggered voices get a 4 ms fast-kill fade) — instant play AND reliable combos; short DMA queue (4×128 ≈ 12 ms). **2 play modes** (BTN1+BTN3): PERC (buttons trigger sounds; tuned ones walk the note pattern) and SEQ with **full transport**: BTN1 play/stop, BTN2 reverse, BTN3 hold = beat repeat (2-step loop, master position keeps running so release stays on-grid), BTN4 speed ×1/×2/×½, BTN5 hold = chaos (random in-scale hits). **2 filter modes** (BTN1+BTN5 held >1 s): POTS (cutoff/res) or IMU (X → cutoff, Y → Q). Chain: textures↔leads mixer → drive → resonant LPF with wobble (LFO→cutoff) → master. **4 pot panels** (A master/mixer/drive/tempo · B filter+LFO · C **notable synthesis macros**: attack 0.5 ms–0.8 s (hit→swell), decay ×0.1–×8, texture (FM+white noise), global pitch ±12 semi · D root/scale/rhythm-pattern/note-pattern) with the pads_imu frozen-pot engine (all values persist). BTN2+BTN4 = bank, BTN3+BTN5 = panel D, BTN4+BTN5 = cycle A/B/C. 6 SMD LEDs = panel palette + per-slot hit flashes + beat/VU + transport feedback. Requires FastLED
- **asistente_ia** — Voice assistant and the **simplest member of the AI family** — the one to read first to see how the board talks to an API. Hold BTN1 and speak (max 5 s) → INMP441 records at 16 kHz mono → **Whisper** (`whisper-1`, Spanish) transcribes → **GPT-4o-mini** answers with GC Lab's context baked into flash → **TTS** (`tts-1`, PCM 24 kHz) plays back through the PCM5102 with no resampling (the DAC is pinned to 24 kHz — which is exactly why `asistente_musical` had to be re-architected). Port of the Proto-Synth v2 assistant: there the mic was analog (ADC) and the DAC was 8-bit software; here the whole path is real I2S. The 6 SMD LEDs are the state machine (green ready / red recording / amber processing / cyan speaking / blinking magenta error). Requires FastLED + INMP441
- **asistente_naga** — Same assistant as `asistente_ia` but talking to **NagaAI** (`api.naga.ac`), an aggregator that exposes many providers behind one OpenAI-compatible API, one key and one balance. The endpoints are identical (`/v1/audio/transcriptions`, `/v1/chat/completions`, `/v1/audio/speech`); what changes is the host, the **model names** and one parameter — and that is the trap: **`whisper-1`, `tts-1` and `gpt-4o-mini` do not exist in Naga's catalog** (it has `whisper-large-v3`, `gpt-4o-mini-tts`, `llama-3.3-70b-instruct:free`, `gemini-2.5-flash-lite`, ElevenLabs voices…), and the limit parameter is **`max_completion_tokens`**, not `max_tokens`. Copying OpenAI's names verbatim gets a 404 and silence. The sketch ships the three **`:free`** models by default, so it runs without spending credit — which is what a workshop with ten boards needs. Second difference, in playback: `asistente_ia` asks for `pcm` and assumes a fixed 24 kHz because there is one provider behind it; here the voice model is swappable (ElevenLabs does not deliver 24 kHz), so it asks for **`wav` and reads the RIFF header** — rate/channels/bits come in the file and the I2S is retuned live with `i2s_channel_reconfig_std_clock`. The parser walks the chunks to `fmt `/`data` (some servers insert a `LIST`), **ignores the declared data size** (streamed WAVs send 0 or `0xFFFFFFFF`) and uses what actually downloaded; headerless bodies are treated as raw s16le mono, and MP3/Ogg/FLAC are **detected and reported over Serial** instead of played as noise. `MOSTRAR_ESTADO` prints the whole round trip (transcript, answer, HTTP codes with error body, audio format) — the fast way to find a wrong model name or key. JSON is parsed by hand with a helper that decodes `\uXXXX` to UTF-8 and looks for `content` *after* `message` so it can't grab a `reasoning_content`. Requires FastLED + INMP441
- **compositor_ia** — `asistente_ia`'s network chain (mic → Whisper → GPT) driving `cancion_aleatoria_leds`' audio engine: **you ask for a song out loud and GPT answers with a compact song JSON** that the board plays. v2 added what actually makes styles sound different (ported from `tools/generador_estilos`): 5 bass patterns (root-fifth / walking / offbeat / octaves / riff), comping by hit type (sustained pad / hits on 2 and 4 / stabs / eighth-note power chords), 5 melody grammars, **sections** (intro → verse → chorus/drop with layers and intensity per section) and **real swing** across the whole grid. The full prompt lives in `COMPOSER_PROMPT` inside the sketch, so you can ask any AI on the PC for the JSON and **paste it over Serial with no WiFi at all** — which is also how it gets demoed without a network. Requires PSRAM + FastLED + INMP441
- **sampler_ia** — **Voice-prompted AI sampler**: you *say* the sound you want and the PercuSynth generates it, loads it into a slot and lets you trigger it. Chain: **BTN1 hold** records voice (INMP441) → **Whisper** transcribes → **GPT-4o-mini** turns the Spanish request into a short English SFX prompt + duration + loop flag → **ElevenLabs `/v1/sound-generation`** returns raw audio → auto-trim of leading silence + normalize + 2 ms edge fades → **PSRAM slot**. 3 slots (round-robin destination). **BTN2/3/4** trigger slots 1/2/3 on the press edge (instant, 128-frame DMA); **holding >0.6 s** turns that slot into a sustained **loop/texture** (a new tap kills it). **BTN5** = 16-step sequencer play/stop, and while it plays each tap on BTN2/3/4 **overdubs** that hit into the nearest step (quantized); hold >1 s clears. Empty slots **borrow the last loaded sample transposed** (frequency ratios ×1.0 / ×1.189207 = +3 semi / ×1.498307 = +7 semi → a minor triad from a single sample). Pots: **BPM / master volume / live pitch (±12 semi, centre detent) / granular stutter** (freezes a slice of the mix and repeats it; grain shrinks exponentially 500 ms → 2 ms, so the top of the travel turns the grain into a pitched tone; re-captures every ~200 ms and windows the grain edges; sits before the filter); **IMU X → cutoff, Y → resonance** (RBJ biquad LPF). 32-step sequencer (two bars) that skips slots currently latched in loop; loop latches quantize to the next beat and wrap at a whole number of beats (never drift). Holding any trigger button turns POT2/3/4 into **per-channel volume faders** (frozen-pot panel). LEDs: the 6 on-board SMD stay as status indicators and an **80-LED WS2812 strip** chains after them on the same data line (86 total, GPIO 46), showing three channel bands (background = channel volume, comet = hit, breathing = latched loop), the sequencer playhead, IMU filter colour, and a strobe that freezes in step with the stutter. Format handling: requests `pcm_22050` and **auto-falls back to `ulaw_8000`** (decoded inline, no library) since PCM needs ElevenLabs Pro tier while µ-law works on Starter. Requires PSRAM + FastLED
- **asistente_musical** — Cross of `asistente_ia`'s network chain (Whisper → GPT-4o-mini → TTS) with `pads_imu`'s stereo voice engine: you talk to GPT by voice **while a generative harmonic bed never stops playing**. The architectural change that makes it work: a **single 44.1 kHz mixer owns `i2s_channel_write`** and runs in `audioTask` pinned to **core 1** (prio 10), while the assistant lives in `assistantTask` on **core 0** alongside the WiFi stack — so TLS and GPT (seconds of blocking) never cut the music. `asistente_ia` couldn't: its DAC was pinned to 24 kHz and the whole flow blocked in `loop()`. The TTS arrives at 24 kHz and is **resampled to 44.1 kHz** by linear interpolation (phase step 0.5442), then goes through a **RBJ high-pass biquad with cutoff on a pot** (20 Hz clean → 2 kHz megaphone) and is summed **after** the pad's filter (otherwise closing the pad cutoff would mute GPT too). A real **sidechain duck** (envelope follower on the *raw* voice, before the HPF) lowers the music bed; **BTN1 fades the music to silence while recording** because the INMP441 hears the speaker and Whisper would transcribe the pad. Music = `pads_imu`'s AUTO mode always on (random key/mode + functional diatonic progression ending on V, 4/4 at 80 BPM, 32-voice stereo pad + arp) but **no filter LFO and no IMU** — cutoff/Q stay where you leave them. Panel A: cutoff / resonance / voice HPF / pad volume. Panel B (hold BTN5, frozen pots): duck depth / voice volume / arp speed / arp volume. All 4 pots are on **ADC1** — mandatory, since WiFi makes ADC2 unusable. Requires PSRAM + FastLED + INMP441
- **oscilador_ia** — Melodic sibling of sampler_ia where **the AI sample IS the oscillator**: you ask for a *sound* by voice and the prompt sent to ElevenLabs is **just that sound, translated plainly to English** — no tuning, no "sustained/steady/loop" riders (ElevenLabs doesn't understand musical concepts and riders confuse it; duration + loop go as API parameters). Instead, on load the **real fundamental is measured by normalized autocorrelation** (octave-error guard + parabolic refinement → cent-level tuning; fallback = assume 110 Hz, intervals stay correct), so every note is computed against the *measured* pitch — wherever the sound landed, and a **sustain loop with a baked crossfade** (45%–92% zone, zero-crossing anchors) lets held notes ring **forever** (drones = the sample as a continuous oscillator). One instrument at a time (each generation replaces it). Musical brain: **10 scales** (7 Greek modes + harmonic minor + Phrygian dominant + double harmonic) with per-mode progression vocabulary + modal cadence; **POT3 = root** (C2..B2, 12 zones with hysteresis), **BTN1 tap = next scale** (hold = record). **POT4 = mode** (6 zones): TECLADO (BTN2–5 = chord degrees 1/3/5/8; tap = note, hold >0.6 s = infinite drone latch), 4 arpeggiator modes (up/down/up-down/random at 16ths; BTN2–5 latch chords on degrees I/IV/V/VI, switching arp zones keeps the arp running), and SEQ (4-bar generative melody with chord-tone snapping on strong steps; BTN2 play/stop, BTN3 new melody, BTN4 new progression, BTN5 root drone pedal; changing scale reinterprets the same degrees = instant modal recolor). POT1 = BPM, POT2 = volume, IMU → biquad filter. 6 SMD LEDs as status (instrument/mode/scale/beat/filter/network). Requires PSRAM + FastLED
- **oscilador_escalas** — Port of the Proto-Synth v2 sketch `Oscilador_4_escalas` **without Mozzi**, onto the Percu-Synth's own 44.1 kHz/16-bit **stereo** I2S engine. The original idea survives untouched — **4 pots = 4 oscillators**, each quantized to the active scale — and the controls stay **direct: one button, one function, no panels or combos**. What changed is everything underneath: each pot drives a **3-voice detuned unison stack** panned across the stereo field (equal-power) plus a **sub-osc**, **PolyBLEP** anti-aliased waveforms (saw/square/25 % pulse/triangle/sine on BTN5), notes stored as **intervals** so tonic and octave transpose everything, the **10 house scales** (7 Greek modes + Phrygian dominant + double harmonic + harmonic minor) cycling on **BTN1**, octave cycle on **BTN2**, quantization with **hysteresis** (ADC noise can't jitter the note), 40 ms **portamento** + a ~25 ms per-oscillator envelope (no clicks), **drive** before a **resonant RBJ biquad LPF swept by the IMU's X axis** (fixed; the LDR did this on the original), plus a **tempo gate** ("intermitencia", BTN3) with **tap tempo** (BTN4) whose clock also sets the **stereo ping-pong delay** time (dotted eighth, damped feedback, 80 KB of internal RAM; delay-time changes **crossfade between two fixed read heads** — a sliding head pitch-shifts the echoes and made the synth sound out of tune every time you tapped). The gate sits **before** the delay so tails ring through the silences. BTN5 long-press = tonic +1. 6 SMD LEDs = per-oscillator note/level, scale + octave, filter opening + tempo pulse. Requires FastLED
- **espacio_modular** — MONOPHONIC **film-ambience machine**, space sibling of `trance_imu`. **Three design rules everything follows from.** (1) *One panel, no combos, no hidden pages* — 5 buttons and 4 pots, one function each; the timbre is fixed in a `TIMBRE FIJO` block (saw, all layers, mid drone, pad envelope, mid Q) and is not exposed at all. (2) *Pots never change meaning and never freeze* — POT1 volume, POT2 tempo, POT3 filter, POT4 espacio; the knob's physical position **is** the value. (3) *Every button is audible the instant it's pressed* (measured: all 5 fire within the first 2.9 ms buffer). **The 24 patterns are THEMES, not figures** — that is the difference between film music and an arpeggio. A theme is defined as much by its **long-short rhythm** as by its pitches, and uses intervals that mean something: the 1→5→8 rise (the horn call), the 8→7→6→5 descent (the lament), the 6→5 sigh, the insisting pedal. Above all, **each note lasts until the next one**: a 0 in the pattern is not a rest, it is the continuation of the previous note, so the spacing *is* the duration (21 of 24 themes span ×2 to ×7 between their shortest and longest note). Grouped as LLAMADAS · LAMENTOS · PEDALES · CIMIENTOS · ARCOS · MOVIDOS. **Why it reads as ambience, not a song**: (a) a **continuous drone** on the tonic (root + fifth + octave-down) through the same filter — the pedal a scene rests on; because it is there the themes need no bass of their own and the voice is free to sing; (b) **very slow harmonic rhythm** — 4 progression slots, *each lasting a whole 4-bar cycle* (9–38 s per chord); (c) **non-functional progressions** — no `i-VI-III-VII` and no `V-i` cadences; the tonic holds half the cycle and the motion is a *modal shift*. The **8 modes are a deliberately cinematic palette** (Aeolian · Phrygian · NÓRDICO = Dorian, the Viking/Celtic colour · Mixolydian · Lydian · ÁRABE = Phrygian dominant/hijaz · BIZANTINO = double harmonic · harmonic minor); plain major and neutral Dorian were dropped. Every progression uses **only consonant triads** of its mode (exotic scales throw up diminished and augmented triads that read as mistakes), and the themes deliberately hit degrees **2, 4, 6 and 7**, not just chord tones — a mode's defining note is almost never in the chord (Phrygian's b2, Lydian's #4, Mixolydian's b7, double harmonic's natural 7), so a 1-3-5-8-only pattern makes all eight modes sound identical (a real bug caught in simulation). Everything is computed in *scale degrees*, so every note is in key by construction (verified: 0 out of key across 5376 notes). **Randomness sits on the two main buttons**: every Play rolls a new mode (the "feeling"), BTN2 rolls a new theme; neither repeats what was just sounding, and each roll mixes `micros()` (the exact instant of the press is the best entropy source on a clockless board). BTN3 steps the modes in order, BTN4 the key by fourths, BTN5 the octave. **IMU: a single axis** — X multiplies the cutoff up to ×8; the filter is otherwise 100% the user's (POT3 only, no LFO). Prints mode/theme/key over **Serial (115200) on every button press** so the user can identify a sound they liked (`MOSTRAR_ESTADO 0` disables it). Audio hygiene worth preserving: **the step length is latched when the step starts** (if tempo can shorten the *current* step, raising it retro-fires a note — sweeping the knob produced dozens of trampled notes), **the delay time is fixed and NOT tempo-synced** (syncing forces the read pointer to move on every BPM change, heard as a pitch sweep while turning the knob), soft saturation *inside* the delay feedback loop, **smoothed panning** (applying `stepPan` directly makes the L/R gains jump mid-note — an audible click), smoothed cutoff/espacio, and gain normalized by the oscillator level sum and by `Q^0.30`. No LEDs (all CPU to audio)
- **impact_chimes_leds** — Sibling of impact_chimes with a **WS2812 LED show** and **3 selectable timbres**, in a single magical scale (**C Lydian**). Floor impacts trigger a note (melodic walk) + a reactive light effect on a 68-LED strip. **BTN1/BTN5** cycle 5 LED effects (Onda/Cometa/Pulso/Chispas/Arcoíris); **BTN2/BTN3/BTN4** pick timbre (campana / marimba / guitarra eléctrica). Pots = attack/decay/brightness/timbre. Dynamic-threshold anti-double-trigger. FastLED via RMT (no I2S clash), `show()` throttled to ~30 FPS. Requires FastLED
- **seismic_drone** — Deep ambient sibling of impact_chimes: MPU6050 at ±2g senses ground vibration → epic drone (detuned stereo saw + sub, breathing resonant filter)
- **MIDI_Drum** — Hardware MIDI USB controller using piezo sensors, buttons, and IMU (MPU6050); velocity-by-IMU-motion for button hits
- **drum_midi_leds** — Drum machine MIDI controller with cinematic full-strip LED effects (one effect per drum type, additive mixing, 8-event pool)
- **trance_midi_leds** — Same sequencer engine as trance_imu but **monophonic & melodic**: one note per step over USB MIDI (true mono), IMU → MIDI CC (CC74/CC71), plus a "fiesta electrónica" show on the 20×20 matrix
- **matrix_midi_anyma** — Anyma-style electro audiovisual machine for the 20×20 WS2812 matrix: internal 16-step sequencer (4 patterns) sends drums (ch10) + bass (ch1) over USB MIDI, acts as **MIDI Clock Master**, and drives a 2D visual engine (5 scenes: NEXUS/TUNNEL/SPECTRUM/STORM/GRID) reacting to both the internal sequencer and incoming MIDI notes. No audio (visual + MIDI controller only)
- **dub_siren** — Work-in-progress dub siren firmware (`.ino` generated by `tools/dub_siren_generator/`)
- **test_system / test_leds / test_imu / test_imu_led / test_imu_sound** — Hardware-diagnostic sketches. `test_system` is the one to reach for on a freshly assembled board: it dumps the live state of *every* peripheral in one Serial screen and takes typed commands to launch audio and LED self-tests. The rest are minimal single-peripheral checks (LED strip; IMU over Serial; IMU shown on LEDs; IMU rendered as sound); the IMU tests without USB avoid CDC-induced resets

### Tool-generated firmwares (under `samples/`)

These `.ino` files are **artifacts** produced by the webapps in `tools/` — not hand-written. They're real working examples of what the loaders generate (with samples already embedded as `PROGMEM` arrays). When working on these files, prefer regenerating from the webapp over editing them by hand.

### Webapps (under `tools/`)

All are standalone HTML files (no build step). Pattern: drop audio files in the browser → app generates a `.ino` → user flashes it via Arduino IDE. Exception: `percu_control` flashes a pre-built `firmware.bin` via ESP Web Tools, and `step_sequencer_loader` also acts as a **live Web MIDI remote** for the running PercuSynth.

- **midi_sampler** — Reads a USB MIDI controller via **Web MIDI** and plays **one** loaded sample (e.g. a bell) as a live **Web Audio** instrument, pitched by the incoming MIDI note relative to a configurable base note (per-voice: linear-interp resampling, AR envelope, one-pole LPF; velocity→gain; sine fallback when no sample is loaded). **4 on-screen knobs (= the 4 hardware pots, ADC 1/2/8/10)** for volume/attack/decay/cutoff, also movable via MIDI CC (7/73/72/74). Testable with no hardware (on-screen keyboard + PC keys). A second tab **generates the `.ino`** that turns the PercuSynth itself into a 1-sample MIDI-USB sampler with the same synthesis mapped to the 4 pots.
- **scale_osc** — Browser sibling of the `oscilador_escalas` firmware: **each pot is an oscillator quantized to a scale**, plus filter, envelope and reverb. Connects to the PercuSynth over **Web Serial** (the IMU's tilt sweeps the filter) and accepts **Web MIDI**; fully playable with mouse and keyboard when nothing is plugged in. Useful for trying the idea without flashing anything.
- **generador_estilos** *(in development)* — Engine where **a musical style is DATA, not code**: blues, techno, synthwave and grunge described in JSON and played over Web Audio. The intended flow is that an AI writes the song JSON and it gets pasted in. Design lives in `PLAN.md`, and `PROMPT_IA.md` is the prompt to hand an AI. Its style vocabulary is what got ported into `compositor_ia` v2.
- **video_synth** — Single-page webapp that imports a video and synthesizes it into image **and** sound in real time, driven by the PercuSynth over **Web Serial** (or the PC mic). Minimalist: each control does one obvious thing.

### Games (under `videogame/`)

- **nebula_gp** — **NEBULA GP**, an FPV drone-racing simulator: 3 laps vs **4 AI bot drones** on a closed neon circuit (Catmull-Rom spline with hills, 16 glowing gates) under a galactic sky (pre-rendered panorama: nebulas, galaxy band, ringed planet, twinkling stars) with drifting smoke banks and a synthwave floor grid — all on a hand-rolled Canvas-2D 3D projection (no WebGL). Reads the PercuSynth over **Web Serial** (same protocol/firmware as NEON STRIKE): **IMU** = yaw rotation + pitch (climb/descend), **BTN5** forward, **BTN1** reverse/brake, **BTN2/BTN4** strafe left/right, **BTN3** recenter IMU/start, **POT1** volume, **POT2** IMU sensitivity. Off-track energy corridor slows the drone; lap progress is arc-length continuous (reversal-safe); bots rubber-band. Web Audio engine hum + wind + race SFX. Fully playable with mouse+keyboard.
- **cyber_flight** — **NEON STRIKE**, a cyberpunk first-person flight-shooter game (not a generator). Reads the PercuSynth over **Web Serial** (`p0..p3,b1..b5,imuX,imuY` @ 50 Hz): the **two IMU axes** steer a center-locked reticle, **BTN5** fires from the right edge, **BTN1** fires from the left edge, **BTN2+BTN4 together** drop a screen-clearing bomb. Canvas-2D synthwave city with Web Audio SFX and a wave/score/multiplier system. Ships its own firmware (`neon_strike_control_percusynth.ino`, embedded download — sends X **and** Y accel). Fully playable with **mouse+keyboard** when no hardware is connected.
- **tilt_maze** — Ball-maze game steered by **tilting the board** (IMU MPU6050), like the wooden toys but with 10 hand-curated levels (all verified solvable), lives, portals, holes and lethal zones. Keyboard fallback when no hardware is connected.

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
- **FastLED** — the only library to install by hand; every LED firmware needs it

### Credentials

**No sketch may carry a WiFi password or an API key in its `.ino`.** The six AI firmwares
(`asistente_ia`, `asistente_naga`, `asistente_musical`, `sampler_ia`, `oscilador_ia`,
`compositor_ia`) each ship a
`secretos.example.h`; the user copies it to `secretos.h` in the same sketch folder. `secretos.h`
is gitignored, `secretos.example.h` is committed. The `.ino` guards the include so a missing file
fails at compile time with a readable message rather than at runtime:

```cpp
#if !__has_include("secretos.h")
  #error "Falta secretos.h: copia secretos.example.h a secretos.h y pon tus claves."
#endif
#include "secretos.h"
```

When adding a firmware that needs credentials, follow the same pattern — never a fallback default
that silently compiles. Note also that **WiFi makes ADC2 unreadable**; the four pots are already
on ADC1 (1, 2, 8, 10), so keep them there.

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
- Every new firmware or tool gets its own `README.md` in its folder (or `PLAN.md` while it is still a design). The root `README.md`, this file and `PROMPT_PARA_LA_IA.md` are the three index documents — keep them in sync when adding or removing a folder
- Credentials never live in a sketch: see **Build & Flash → Credentials**

## Control Design

How an instrument in this project is expected to feel under the hands. These are design rules,
not preferences — follow them unless a firmware states otherwise:

- **One button, one function**, audible **on the press edge**. No waiting windows to disambiguate
  combos: fire immediately and let a combo *undo* the individual action with a ~4 ms fast-kill
- **Pots always mean the same thing** and the knob's physical position *is* the value. Frozen-pot
  panels and pick-up exist in some firmwares, but do not add them to new ones by default
- **Do not invent panels or combos** that were not asked for; prefer a fixed constant in the code
- **Nothing moves on its own** — autonomous LFOs writing to parameters the user believes they are
  holding destroy the sense of playing the instrument
- **Noisy ≠ distorted**: dirty timbres come from synthesis (inharmonic partials, filtered noise,
  band-split saturation), never from bit-crush, decimation or bus drive — those are aliasing
- Percussion checklist: **two envelopes per hit** (tone + noise), exponential decay taus (a hit
  rings ~3·tau), PolyBLEP on any waveform with corners, DC blocker on the output, kick sidechain
- Do not render and send audio files of a firmware — the board is right there and gets listened to.
  To validate audio logic before flashing, compile the `.ino` on the PC against mocks of
  `Arduino.h` / `Wire.h` / `i2s_std.h` and render to a local WAV: that catches NaN, clipping,
  out-of-scale notes and stuck voices that `arduino-cli` cannot see
