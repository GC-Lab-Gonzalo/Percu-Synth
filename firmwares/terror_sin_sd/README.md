# terror_sin_sd

Música de tráiler de terror que se toca en vivo, en 4 paneles, **sin microSD**: las voces van dentro
del firmware, en la flash del ESP32. Cada Play sortea un tema nuevo y arranca con una **intro
lenta**: un latido que se va acelerando y un viento que crece durante 16 s, hasta que la percusión
entra con un golpe. La **tensión** (POT3 del General) va sumando el bajo, un tic, un arpegio que
sube y los tambores. Encima se agregan **capas que crecen de a poco** y **voces** (risas, lamentos,
susurros, niños, bebés) con pitch, reverb, eco y reversa. Sin acelerómetro. Luces todas rojas.

**La hoja de controles para imprimir** está en `controles_terror_sin_sd.pdf` (y su fuente,
`controles.html`).

## Paneles

**BTN4 = siguiente panel**, en todos. Al cambiar de panel cada pot queda suelto hasta que lo
muevas un 4 %, así que nunca salta un valor. El LED 5 dice el panel.

| Panel (LED 5) | BTN1 | BTN2 | BTN3 | BTN5 | POT1 | POT2 | POT3 | POT4 |
|---|---|---|---|---|---|---|---|---|
| **General** (rojo) | play/stop: tema nuevo + intro | tema siguiente sin parar | suspenso (otra vez = entra ya) | — | volumen | tempo | **tensión** | filtro general |
| **Bajo** (naranjo) | suena/calla | patrón (8) | — | — | volumen | filtro | caída 40 ms–1.2 s | resonancia |
| **Capas** (ámbar) | cuerdas que suben | coro oscuro | tono de tensión | caja de música | vol. cuerdas | vol. coro | vol. tono | vol. caja |
| **Voces** (violeta) | dispara una voz | categoría | reversa sí/no | — | volumen | pitch | reverb | eco |

Cada tema es una progresión y **cada acorde trae su propia escala**: bajo, arpegio, braam, cuerdas,
coro y caja de música tocan sólo notas de la escala del acorde que suena.

## Las voces

Trae **2 por categoría**, 10 en total (25.4 s, mono, 16 bit, 44.1 kHz), todas CC0 o de dominio público
(las fuentes están en [`fuentes_voces.txt`](fuentes_voces.txt)):

| Categoría | Voz 1 | Voz 2 |
|---|---|---|
| Risas | risa maníaca (2.7 s) | carcajada fantasmal (2.8 s) |
| Lamentos | quejido de fantasma (1.7 s) | gemido de fantasma (3.0 s) |
| Susurros | «behind you» (2.8 s) | susurro siniestro (2.2 s) |
| Niños | niña diciendo «game over» (2.0 s) | niña tarareando (2.7 s) |
| Bebés | recién nacido llorando (2.5 s) | llanto en un pasillo (3.0 s) |

Se leen directo de la flash, al derecho o al revés, con el pitch, la reverb y el eco del panel
Voces. Ocupan 2.24 MB, por eso **hay que cargar con la partición Huge APP**.

**Para tocarlo no hace falta Python**: `voces.h` ya viene hecho y basta con abrir el `.ino` (Arduino
lo muestra como pestaña). Al compartirlo, manda la carpeta entera, o como mínimo `terror_sin_sd.ino` +
`voces.h` juntos. Si llega solo el `.ino`, compila igual, pero todas las voces suenan como el susurro
sintetizado.

Para cambiarlas, `generar_voces_h.py` arma un `voces.h` nuevo con hasta 4 por categoría, desde
cualquier audio que lea ffmpeg (recorta los silencios, corta en 3 s por el punto más quieto,
normaliza a −1 dBFS y avisa si se pasa de lo que cabe, ~2.4 MB):

```
python generar_voces_h.py --risas a.wav b.mp3 --lamentos c.wav d.wav --susurros e.wav f.wav --ninos g.wav h.wav --bebes i.wav j.wav
```

## Cargar

ESP32S3 Dev Module · **Flash Mode DIO** · USB CDC On Boot Enabled · PSRAM OPI · **Partition Scheme:
Huge APP (3MB No OTA/1MB SPIFFS)**. Requiere FastLED. La PSRAM no es obligatoria: sin ella el glitch
usa una memoria más corta.

```
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=dio,PSRAM=opi,PartitionScheme=huge_app" --upload -p COMx .
```

## Verificado (2026-10-27)

Simulado en el PC con un guion de 322 s: 0 NaN, 0 muestras en el techo y 0 de
2692 notas fuera de la escala de su acorde. Las 10 voces suenan al derecho, al revés y con pitch.
No está probado en la placa.
