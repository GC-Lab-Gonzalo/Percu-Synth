# secuenciador_melodico_cromatico — Secuenciador polifónico de 16 pasos, 12 notas

Autor: **Nicolás Martínez Contreras**.

Secuenciador **polifónico** de 16 pasos sobre una **octava cromática** (12 notas, de Do a Si):
cada paso puede tener cualquier combinación de notas sonando a la vez, así se escriben acordes
además de melodías. Hay 12 voces, una por nota. Trae una **webapp**
(`secuenciador melodico cromatico webapp/`) que se conecta por **Web Serial** para editar la
grilla de 16 × 12 con el mouse.

## Controles

| Control | Qué hace |
|---|---|
| BTN1 | Play / Stop |
| BTN2 | Forma de onda siguiente: pulso → sierra → triángulo → seno |
| BTN3 | Reversa |
| BTN4 | Secuencia al azar |
| BTN5 | Borrar la secuencia |
| POT1 | Volumen |
| POT2 | Tempo (los pasos son semicorcheas) |
| POT3 | Transposición −12…+12 semitonos |
| POT4 | Envolvente (ataque / caída, desde el centro) |

Los 6 LEDs de la placa muestran el estado.

## Webapp

Abre `secuenciador melodico cromatico webapp/secuenciador melodico cromatico.html` en Chrome o
Edge y conecta el PercuSynth por Web Serial (115200). La placa le manda su estado (`PLAYING`,
`DIR`, `PITCH`, `WAVE`, `VOL`, `BPM`, `ENV`, `SYNC`) y recibe la secuencia con `SEQ`; desde la
webapp también se puede dar play y stop.

## Librerías

- **ESP32Synth** (motor de síntesis que maneja el I2S)
- **FastLED**
