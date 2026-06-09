# video_synth — Sintetizador audiovisual (web)

Webapp de una página (`index.html`, sin build) que **importa un video y lo
sintetiza en imagen Y sonido** en tiempo real, controlado por la **PercuSynth**.
Minimalista: cada control hace **una** cosa obvia, usando solo lo que tiene la
PercuSynth (4 pots, 5 botones, IMU, piezos).

## Uso

1. Abre `index.html` en **Chrome o Edge** (Web Serial).
2. Arrastra un video o pulsa **Abrir video** (`O`).
3. (Opcional) **Conectar** (`C`) y elige el puerto del **USB nativo** del ESP32-S3.
   Sin hardware: usa los **pots y pads en pantalla** del panel.

> Servir local: `python -m http.server 8770 --directory tools/video_synth`.

## Mapeo (fijo) — cada control afecta imagen Y sonido

| Control | Imagen | Sonido |
|---------|--------|--------|
| **POT1** | Velocidad del video | **Pitch** (sube/baja con la velocidad) |
| **POT2** | Mosaico / pixelado | **Vibrato** (modulación de frecuencia) |
| **POT3** | Glitch RGB (aberración) | **Chorus** (modulación) |
| **POT4** | Lo-fi (scanlines + desaturación) | **Filtro** lowpass (cierra) |
| **IMU** (inclinar) | Warp / ondas | **Eco** (delay con feedback) |
| **PIEZO** (golpe) | Flash + zoom | — |
| **BTN1** | **Play / Stop** (imagen **y** audio juntos) | |
| **BTN2** | Escena: ciclar paleta (6) | **Auto-wah distinto por escena** (cada paleta = otro barrido) |
| **BTN3** | Multi-pantalla x2 → x3 → x4 | **Ring mod distinto por nivel** (x2=150, x3=400, x4=900 Hz) |
| **BTN4** | Caleidoscopio x2 → x3 → x4 → … | **Flanger distinto por nivel** (feedback+velocidad suben) |
| **BTN5** | Strobe (parpadeo) | **Tremolo** (corte rítmico) |

## Audio

El sonido es el **audio ORIGINAL del video**, procesado por una cadena Web Audio
de **modulaciones** (sin distorsión):
`fuente → auto-wah → vibrato → chorus → flanger → filtro → tremolo → ring-mod → (seco + eco) → salida`.
Cada control mueve un nodo, y **cada pulsación de BTN2/BTN3/BTN4 genera un cambio
distinto** (no on/off constante). Necesita un clic/tecla para arrancar — pulsa Play.

## Firmware PercuSynth (incluido)

Botón **⬇ Firmware** → `video_synth_control_percusynth.ino` (embebido). Protocolo
USB-Serial 115200, 50 Hz:

```
ENVIA:  pot1,pot2,pot3,pot4,btn1,btn2,btn3,btn4,btn5,piezo,imu
RECIBE: L:l1,l2,l3,l4   (play / audio / invertir / caleido)
```

Pins: pots ADC 1/2/8/10 · **botones 44/42/0/45/47** (orden físico BTN1..BTN5) ·
piezos ADC 4–7 · IMU SDA21/SCL38 · LED WS2812 pin 46. El IMU se lee por **I2C crudo
(`Wire`), sin librería externa** (igual que `firmwares/test_imu`), con autodetección
de dirección 0x68/0x69. Flashea con **ESP32S3 Dev Module**, **USB CDC On Boot =
Enabled**, **Flash Mode = DIO**.

### ⚠️ "Conectado pero SIN datos"

La PercuSynth tiene **dos puertos USB-C**: el del **chip UART** (para flashear) y el
**USB nativo** del ESP32-S3 (por donde sale el Serial). **Conecta la webapp al USB
nativo** (es un COM distinto al de programar). El monitor RX del panel se pone verde
cuando el puerto es el correcto.

## Teclado

`SPACE` play · `2` paleta · `3`/`I` invertir · `4` caleido · `5`/`A` audio ·
`O` abrir · `C` conectar · `H` panel.
