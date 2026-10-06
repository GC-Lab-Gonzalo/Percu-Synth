# secuenciador_melodico — Secuenciador melódico de 16 pasos en modos griegos

Autor: **Nicolás Martínez Contreras**.

Secuenciador **monofónico** de 16 pasos donde cada paso es un **grado de la escala** (o un
silencio), así todo lo que toca queda dentro del modo elegido. Los 7 **modos griegos** (jónico,
dórico, frigio, lidio, mixolidio, eólico, locrio) se recorren con un botón. Trae una
**webapp** (`secuenciador_melodico webapp/`) que se conecta por **Web Serial** para editar la
secuencia con el mouse y ver el paso que suena.

## Controles

| Control | Qué hace |
|---|---|
| BTN1 | Play / Stop |
| BTN2 | Modo griego siguiente (7) |
| BTN3 | Reversa (la secuencia corre hacia atrás) |
| BTN4 | Secuencia al azar |
| BTN5 | Borrar la secuencia |
| POT1 | Volumen |
| POT2 | Tempo (los pasos son semicorcheas) |
| POT3 | Transposición −12…+12 semitonos |
| POT4 | Envolvente (ataque / caída) |

Los 6 LEDs de la placa muestran el estado.

## Webapp

Abre `secuenciador_melodico webapp/secuenciador melodico.html` en Chrome o Edge, conecta el
PercuSynth por Web Serial (115200) y edita los pasos en la pantalla. La placa le manda su
estado (`SCALE`, `DIR`, `PITCH`, `VOL`, `BPM`, `ENV`, `STEP`, `SYNC`) y recibe la secuencia con
`SEQ`.

## Librerías

- **ESP32Synth** (motor de síntesis que maneja el I2S)
- **FastLED**
