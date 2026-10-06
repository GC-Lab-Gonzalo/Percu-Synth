# oled_video_techno — Video vertical en la OLED + techno con bombo

Un video vertical (un short) corre en la **pantalla OLED puesta de lado**, **amarrado al tempo**,
mientras la placa toca un secuenciador techno: bombo en negras, línea ácida con el filtro en los
cuatro pots y hats. Es el primer firmware que usa el conector OLED de la placa V2.0.

## Hardware

- **OLED 0.96" SSD1306 128×64 I2C** en el conector OLED (SDA 21 / SCL 38, dirección 0x3C). Mismo bus
  que el MPU6050, que acá no se usa.
- DAC PCM5102, 5 botones y 4 pots como siempre. Sin IMU.
- Opcional: módulo **microSD** por SPI para grabar el audio (ver más abajo). El LED 0 de la placa
  muestra el estado de la grabación. Requiere **FastLED**.
- Arduino IDE: PSRAM en **OPI PSRAM** para tener 8 s de colchón de grabación.

La pantalla se usa **girada 90°**: queda en 64 de ancho × 128 de alto. Si al girar la placa la imagen
queda cabeza abajo, cambia `GIRO_180` a `true` y vuelve a flashear. La imagen se gira por hardware y
no hace falta regenerar el video.

## Controles

| | |
|---|---|
| **BTN1** | Play / Stop. Play arranca en el paso 1 **y en el primer cuadro del video** |
| **BTN2** | Patrón nuevo de la línea ácida (La menor natural; el paso 1 es siempre la tónica) |
| **BTN3** | Tap tempo (60–200 BPM). Una serie nueva no arrastra el tempo de la anterior |
| **BTN4** | Bombo sí / no, para armar quiebres. El video sigue corriendo |
| **BTN5** | Grabar / parar la grabación a la microSD (independiente de Play) |
| **POT1** | Corte del filtro (60 Hz – 7.7 kHz) |
| **POT2** | Resonancia. Va atada al corte: con el filtro muy abierto se modera sola |
| **POT3** | Envolvente: cuánto abre el filtro cada nota (0–5 octavas) |
| **POT4** | Decay de la envolvente de filtro (40 ms – 1 s) |

El volumen es fijo (`MASTER`): se ajusta en el parlante o en la mesa.

## Grabar el audio a la microSD (BTN5)

Graba **exactamente lo que sale por el DAC**: el mismo bloque de 128 muestras que va a los audífonos,
en estéreo, 16 bit y 44.1 kHz. No usa micrófono. Cada toma queda en `TECHNO_0001.WAV`,
`TECHNO_0002.WAV`… y la numeración sigue lo que ya haya en la tarjeta. Sirve para montar el sonido
limpio sobre el video que grabes con la cámara.

- **Módulo microSD por SPI**, el mismo cableado que `grabador_campo`: SCK 14 · MOSI 15 · MISO 16 ·
  CS 17. VCC a 5 V si el módulo trae regulador, si no a 3.3 V. Tarjeta en FAT32. Se puede meter
  después de encender: al apretar BTN5 se vuelve a montar.
- **LED 0 de la placa**:

  | Color | Significa |
  |---|---|
  | Apagado | Sin grabar |
  | Rojo | Grabando |
  | Naranjo | La tarjeta no alcanzó y se perdió un trozo; usa una más rápida |
  | Rojo parpadeando 3 s | No hay tarjeta o falló |

- La tarjeta **nunca corta el audio**. La tarea de audio deja cada bloque en un colchón: 8 s en PSRAM,
  o 1 s en RAM interna si la placa no tiene PSRAM. Una tarea en el core 0 lo vacía a la tarjeta en
  escrituras de 16 KB. La cabecera del WAV se reescribe cada 5 s, así que una toma cortada se abre igual.
- Consejo para el video: empieza a grabar **antes** del Play. Así queda el arranque entero y es fácil
  sincronizar con la cámara usando el primer bombo.

## Cómo va el video con la música

El video **no corre a sus fps**: avanza con el reloj del secuenciador. El loop entero dura
`VIDEO_PULSOS` negras, un número de compases enteros que calcula el conversor. El audio publica la
posición en cuadros (paso entero + fracción del paso en curso) y la tarea de pantalla dibuja el
cuadro que toque, saltándose los que no alcance a mandar. Como el tiempo lo marca el audio, la
imagen nunca se desfasa de la música. Al subir el tempo el baile se acelera, con Stop se congela y
cada Play lo reinicia junto con el "1".

En cada bombo el **contraste** de la pantalla salta al máximo y vuelve a bajar en ~70 ms: la imagen
late con el pulso (`CONTRASTE_BASE`, `CONTRASTE_GOLPE`, `TAU_DESTELLO_MS`).

Cada cuadro son 1024 bytes que la pantalla recibe tal cual (el conversor ya los deja en el orden de
memoria de la SSD1306), en ~12 ms a 800 kHz. A 126 BPM el video de ejemplo pide ~34 fps y el bus da
para ~70. Si ves basura en la imagen, baja `I2C_HZ` a 400000: llega a ~40 fps, que alcanza hasta
~150 BPM con este video.

## Cambiar el video

```bash
python convertir_video.py mi_video.mp4 --tapar 205,0,155,46 --pulsos 16
```

(Así se generó el `video.h` incluido: 4 compases exactos, el baile corre a 1.13× a 126 BPM.)

- Genera `video.h` (lo incluye el `.ino`) y `video_vista_previa.png` con una muestra de los cuadros.
  **Mira la vista previa antes de flashear.**
- `--modo silueta` (por defecto) sirve para **un personaje sobre fondo claro liso**: el fondo queda
  negro, la figura encendida y las líneas oscuras del dibujo se apagan dentro de ella. En una OLED es
  lo que mejor se lee. `--modo dither` (Bayer 4×4, no "hierve" entre cuadros) sirve para escenas con
  fondo; `--modo umbral` es blanco y negro duro.
- `--umbral` y `--fondo` ajustan qué cuenta como línea oscura y qué como fondo.
- `--tapar x,y,ancho,alto` tapa con blanco un rectángulo del video original (logos, carteles). El
  ejemplo tapa el cartel «Goku.» del short de muestra.
- `--bpm` tiene que ser el `BPM_INICIAL` del `.ino` (126). Con eso el conversor elige `VIDEO_PULSOS`
  como el múltiplo de 4 negras más cercano a la duración real; `--pulsos N` lo fuerza.
- `--fps 15` re-muestrea videos largos. Cada cuadro ocupa 1 KB de flash: con la partición por defecto
  entran ~550 cuadros; con **Huge APP (3 MB)**, unos 2500.

Requiere ffmpeg, numpy y Pillow. Para bajar un short: `pip install yt-dlp` y
`python -m yt_dlp -f "bv*[height<=720]" URL`.

## Sonido

- **Bombo** de tres bandas (el de `trance_pistas` / `drum_poder`): fundamental que aterriza en 46 Hz,
  2.º armónico para el pecho, mazo de ruido pasa-banda y click. Lleva saturación propia porque tiene una
  sola parcial fuerte. Dos instancias con fast-kill de 3 ms al retriggear.
- **Línea ácida**: dos sierras PolyBLEP (±6 cents) → SVF TPT pasa-bajos, con acentos (más volumen y
  el filtro abre más) y ligados a la 303 (portamento de 45 ms sin re-atacar). La nota no reinicia la
  fase y el ataque sube desde donde esté la envolvente, así que no hay clics. Coeficientes cada 16
  muestras con `tanRapido`/`exp2Rapido`, sin libm por muestra.
- **Hats**: ruido pasa-banda en 8 kHz (nunca pasa-altos pelado), abierto en el contratiempo y
  cerrados suaves entre medio, con choke.
- Sidechain del bombo sobre la línea, bloqueador de continua, limitador con lookahead de 64 muestras,
  pasa-bajos de 13 kHz **después** del limitador.

Arquitectura de siempre: audio en su tarea en el core 1. En el core 0 van el control (botones y pots
a 1 kHz) y la pantalla, con prioridad más baja. Ninguna toca el audio: dejan pedidos que el audio
aplica al empezar cada paso. Sin Serial.

## Verificado

- Compila con arduino-cli para ESP32-S3 (DIO): 646 KB con el video de ejemplo (258 cuadros).
- Simulado en PC con mocks de Arduino/Wire/I2S: el video avanza siempre hacia adelante y da la vuelta
  exactamente cada 16 negras; 43 bombos en 20 s a 126 BPM. En el peor caso (pots barridos de punta a
  punta, resonancia y envolvente al máximo, 60 y 200 BPM, patrones nuevos, bombo on/off, stop/play):
  pico −1.3 dBFS, 0 muestras al techo, 0 NaN, 0 saltos bruscos. Nivel normal: −12 dBFS RMS, factor de
  cresta 2.9.
- **No probado todavía en la placa con la pantalla.** Lo primero a mirar: la orientación
  (`GIRO_180`) y si el bus aguanta 800 kHz con el MPU6050 colgado.
