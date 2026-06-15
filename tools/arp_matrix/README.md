# ArpMatrix — Arpegiador polifónico + matriz LED horizontal 64×32

Webapp standalone (un solo `index.html`, sin build step) que convierte el **PercuSynth** en
un **arpegiador polifónico** con una **matriz LED horizontal 64×32** (LEDs separados) reactiva al sonido.
Proyecto colectivo creado en el taller del PercuSynth · **GC Lab Chile**.

El audio suena en el navegador (Web Audio API) y, si conectas el PercuSynth por **Web Serial**,
la webapp **espeja su telemetría** (pots / botones / IMU) en pantalla. El botón **Generar .ino**
descarga un firmware con el **mismo motor de síntesis** → el hardware suena igual que la webapp.

> Funciona 100 % con **mouse + teclado**, sin hardware.

---

## Cómo se usa

1. Abre `index.html` en Chrome o Edge (Web Audio + Web Serial).
2. Pulsa **PLAY** (o la barra espaciadora) y empieza a sonar el arpegio.
3. Opcional: **CONECTAR** → elige el puerto del PercuSynth para que la matriz reaccione al hardware.
4. **⬇ Generar .ino** → descarga `arp_matrix_percusynth.ino` y flashéalo desde el Arduino IDE.

### Teclado (modo sin hardware)
`espacio` Play/Stop · `1-5` botones BTN1–BTN5 · `W` tipo de onda · `A` menú ADSR · `M` cambia escena visual.

---

## Motor de sonido

- **Nota raíz:** C, octava seleccionable (C2 · C3 · C4 · C5).
- **8 modos griegos:** Jónico, Dórico, Frigio, Lidio, Mixolidio, Eólico, Locrio + Pentatónica.
- **6 patrones:** UP · DOWN · UP-DOWN · DOWN-UP · RANDOM · CHORD (acorde 1-3-5-7 simultáneo).
- **Polifonía con overlap:** cada nota tiene su propia envolvente → las notas resuenan y se solapan.
- **4 tipos de onda:** sine · square · saw · triangle.
- **Ruido blanco** (POT1): es **una voz más que acompaña al arpegio** — se dispara con cada nota y comparte su envolvente (no es un fondo constante).
- **Saturación** waveshaper tanh (POT2) · **Volumen master** (POT4).
- **Filtro resonante** LPF/HPF/BPF (Panel 3) con cutoff + resonancia.
- **PWM** (ancho de pulso de la onda cuadrada) + **LFO** que modula el cutoff (movimiento/dinámica) + **sub-osc** (−1 octava). Todo en el Panel 3.
- **ADSR** en Panel 2 con *pot pickup* (los pots no saltan al cambiar de panel).
- **Beat repeat** (BTN4 mantenido): repite la nota actual rápido (flam).

### Mapeo de controles — TRES paneles

Combos (estilo `trance_imu_leds`): **BTN1+BTN5** ↔ Panel 2 (ADSR) · **BTN2+BTN4** ↔ Panel 3 (TIMBRE).

**Panel 1 — PRINCIPAL**

| Control | Función |
|---|---|
| BTN1 | Cicla octava (C2→C3→C4→C5) |
| BTN2 | Cicla modo griego |
| BTN3 | Cicla patrón de arpegio |
| BTN4 (hold) | Beat repeat · BTN5 Play/Stop |
| POT1 Ruido · POT2 Saturación · POT3 Velocidad · **POT4 Volumen** |

**Panel 2 — ADSR**

| Control | Función |
|---|---|
| BTN1 tipo de onda · BTN2 visualización de la matriz · BTN3 rango de octavas (1→2→3) |
| POT1 Attack · POT2 Decay · POT3 Sustain · POT4 Release |

**Panel 3 — TIMBRE** (filtro + dinámica)

| Control | Función |
|---|---|
| BTN1 | Tipo de filtro: LPF / HPF / BPF |
| BTN2 | Velocidad del LFO: lento / medio / rápido |
| BTN3 | Sub-oscilador on/off (−1 octava, da cuerpo) |
| POT1 Cutoff · POT2 Resonancia · POT3 PWM (ancho de pulso) · POT4 LFO (modula el cutoff → movimiento) |

> **Pot pickup:** al cambiar de panel los pots quedan **congelados** y sólo retoman el control
> cuando se mueven (>2% del recorrido), igual que `trance_imu_leds` — así los valores del panel
> anterior no saltan.

---

## Matriz visual (horizontal · 64×32)

Matriz LED **horizontal** de 64×32, elemento central dominante. Cada LED se dibuja como un
**círculo** (aro apagado + halo/bloom + brillo especular) sobre fondo oscuro, para que se vea
como una matriz LED real. Las escenas se renderizan en un buffer lógico 64×32 y `presentLEDs()`
las muestra como la rejilla de círculos. Reactiva al audio (FFT/RMS vía `AnalyserNode`), con 4
escenas seleccionables (BTN2 del Panel 2 o chips en pantalla):

- **NEXUS** — campo de partículas desviado por el IMU (acelerómetro X/Y).
- **LISSAJOUS** — `x = A·sin(aω+δ)`, `y = B·sin(bω)`, modulado por modo/patrón/IMU.
- **PLASMA** — campo sinusoidal modulado por graves + IMU.
- **FRACTAL** — conjunto de Julia animado; `c` modulado por audio (graves/RMS) + IMU, color por modo griego.

---

## Firmware embebido (`arp_matrix_percusynth.ino`)

Instrumento **autónomo** en ESP32-S3: corre el mismo arpegiador y la misma síntesis por
**I2S → PCM5102** (44.1 kHz / 16-bit), y transmite por USB Serial @115200 a **50 Hz**:

```
p0,p1,p2,p3,b1,b2,b3,b4,b5,imuX,imuY
```

(pots 0..4095, botones 0/1, IMU 0..4095 centro 2048) para que la webapp lo espeje.

**Arduino IDE:** Board `ESP32S3 Dev Module` · USB CDC On Boot **Enabled** · **Flash Mode `DIO`**
(OPI rompe el I2S) · PSRAM `OPI PSRAM`. Librerías: ESP32 core ≥ 3.x (`driver/i2s_std.h`) + `Wire.h`.

---

## Créditos del taller

| Participante | Contribución |
|---|---|
| Juan León | Estética HTML percu-control, play/stop, pot white noise, botón octavas |
| Alfredo Romero | Matriz LED visual, pot saturación, motor arpegiador, visuales por ecuaciones, beat repeat |
| Francisco Castro | IMU → partículas en la matriz, patrones Lissajous |
| Pablo Zabal | Distorsión CSS de la interfaz cuando el sonido es fuerte |
| Manuel Martínez | LEDs RGB en webapp para imágenes/video |
| Nicolás Martínez | 8 modos griegos, pot velocidad del arpegio |
| Gonzalo Sandoval | 6 patrones de arpegio, menú ADSR (BTN1+5), botón tipos de onda |
