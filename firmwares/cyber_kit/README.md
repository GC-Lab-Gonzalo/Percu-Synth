# CYBER KIT — secuenciador de texturas, FX rítmicos y leads cyber

**No es una drum machine**: es un secuenciador/instrumento de **texturas, efectos rítmicos y melodías cyber** 100% sintetizado en tiempo real (sin samples) para el PercuSynth (ESP32-S3 + PCM5102): leads neón, campanas FM, hoovers, risers, nubes granulares, drones oscuros, zaps, downlifters, glitch bursts, growls, reeses, subs.

**Respuesta instantánea**: los botones disparan en el flanco de presión (0 ms de espera) y la cola DMA es corta (~12 ms). Si un combo de dos botones se forma dentro de 50 ms, la acción de cada botón se **deshace** (los golpes se apagan con un fade de 4 ms inaudible) → tocar es inmediato y los combos siguen siendo confiables.

Motor de paneles/pots congelados de `pads_imu_leds`: todos los valores **persisten** al cambiar de panel.

## Bancos (BTN2+BTN4 cicla)

| Banco | Sonidos |
|---|---|
| **LEADS** | NeonLead (sierra desafinada) · CampanaFM · AcidSq · Hoover · Chip (trino 8-bit) |
| **TEXTURAS** | Riser (ruido BP barrido↑) · Nube granular · Dron oscuro (swell) · Brillo FM · Polvo |
| **FX** | Zap láser · Reversa (swell→corte) · Downlifter · Beam/alarma · GlitchBurst |
| **BAJOS** | Growl FM · Reese · SubPunch · Talk · Stab |

Los sonidos afinados viven **dentro de la escala elegida** (Panel D).

## Modos de tocar (BTN1+BTN3)

- **PERC**: BTN1..BTN5 disparan los 5 sonidos del banco. Los afinados caminan por el patrón de notas en cada golpe.
- **SEQ**: transporte de performance sobre el secuenciador (16 pasos × 5 slots):
  - **BTN1** → Play/Stop (play reinicia en el paso 0)
  - **BTN2** → Reversa
  - **BTN3** → Beat Repeat (mantener: loopea 2 pasos; al soltar sigue en tiempo)
  - **BTN4** → Velocidad ×1 → ×2 → ×½
  - **BTN5** → Caos (mantener: golpes/notas aleatorias en escala)

## Modos de filtro (BTN1+BTN5 sostenido >1 s)

- **POTS**: cutoff + resonancia (Panel B POT1/POT2)
- **IMU**: aceleración X → cutoff · Y → resonancia

## Paneles de potenciómetros

| Panel | Color | POT1 | POT2 | POT3 | POT4 |
|---|---|---|---|---|---|
| **A — Mezcla** (inicial) | cian | Volumen master | Mixer texturas↔leads | Drive | Tempo (60–180 BPM) |
| **B — Filtro/LFO** (BTN4+BTN5) | violeta | Cutoff | Resonancia | Velocidad LFO | Wobble |
| **C — Síntesis** (BTN4+BTN5 ×2) | naranja | Attack (0.5 ms–0.8 s) | Decay (×0.1–×8) | Textura (FM+ruido) | Pitch global ±12 semi |
| **D — Escala/Seq** (BTN3+BTN5) | verde | Fundamental | Escala (10) | Patrón rítmico (8) | Patrón de notas (8) |

El **Panel C transforma los sonidos de forma notable**: attack alto convierte cualquier golpe en swell/reverse, decay va de click a cola larga, textura mete FM y ruido a todo, pitch mueve ±1 octava.

## Escalas y patrones (Panel D)

- **Escalas**: Jónico · Dórico · Frigio · Lidio · Mixolidio · Eólico · Locrio · **Frigio dominante (flamenca)** · **Doble armónica (gitana oscura)** · Menor armónica.
- **Rítmicos**: TECHNO · DUBSTEP · BREAKBEAT · TRAP · ELECTRO · DNB · TRIBAL · MINIMAL.
- **De notas**: PEDAL · OCTAVAS · QUINTAS · SUBE-BAJA · RIFF OSCURO · SALTOS · PEDAL CAE · CAOS.

## LEDs

Paleta = panel activo (A cian / B violeta / C naranja / D verde) + flash blanco al cambiar. LEDs 0-4 flashean por sonido; LED 5 = beat (SEQ, rojo si Beat Repeat) o VU (PERC); Caos = chispas blancas. Mensajes: banco (N LEDs amarillos), modo (verde/azul), filtro (violeta/blanco), velocidad (1/2/4 LEDs celestes).

## Arduino IDE

- Board: **ESP32S3 Dev Module** · USB CDC On Boot: Enabled · **Flash Mode: DIO** (OPI rompe I2S) · PSRAM: OPI PSRAM.
- Librerías: ESP32 core ≥ 3.x, Wire, **FastLED**.
