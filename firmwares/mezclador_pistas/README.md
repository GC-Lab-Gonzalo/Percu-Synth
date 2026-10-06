# mezclador_pistas

El PercuSynth toca una canción separada en **4 pistas** (batería, bajo, voz, otros) desde la
microSD, todas a la vez y sincronizadas a la muestra, con volumen y filtro por pista. Al mismo
tiempo manda por USB lo que pasa en cada pista para que **Resonancia** (la app de visuales,
`web/public_html/resonancia/`) dibuje una capa por instrumento.

**El audio nunca pasa por el computador**: lo mezcla la placa y sale por el DAC. El navegador sólo
recibe datos (60 líneas por segundo) y dibuja.

## Preparar la tarjeta

1. Separa la canción en pistas (Moises, Demucs, lalal…) y exporta también el metrónomo si la
   herramienta lo da.
2. Convierte:

   ```bash
   python preparar_cancion.py "<carpeta con las pistas>" "<carpeta de salida>" "Título" "Artista"
   ```

3. Copia `cancion.wav` y `cancion.txt` a la **raíz** de una microSD FAT32 (clase 10 / A1).

**Varias canciones:** la segunda se prepara con `--nombre cancion2` (y así hasta `cancion9`). La placa
las encuentra solas al arrancar (`/cancion.wav`, `/cancion1.wav` … `/cancion9.wav`) y se pasa a la
siguiente manteniendo BTN1 2,5 s.

El BPM se mide con un **ajuste lineal sobre todos los clicks** del metrónomo, no con la mediana de los
intervalos: el de «Cosmic Consciousness» redondea cada click y alterna 480 y 460 ms, y la mediana daba
125 BPM cuando el tempo real es 126.

Por qué un solo archivo y no cuatro: la tarjeta lee **un** archivo de corrido a 353 KB/s sin
problema, pero cuatro archivos sueltos la obligan a saltar entre ellos, y esos saltos son los
que cortan el audio. Las pistas van en mono (cada una con su lugar en la mezcla estéreo) para dejar
margen de lectura; «otros» se abre en estéreo con un retardo de Haas de 11 ms.

La primera canción ya está preparada en `BRENNDV/para_microSD/` («Miro Hacia el Interior», Brenndv,
125 BPM, primer pulso a 61 ms medido sobre su metrónomo).

## Cableado

El mismo de `grabador_campo`: microSD por SPI en **SCK 14 · MOSI 15 · MISO 16 · CS 17**.

## Controles: bancos

Un botón elige el banco y las 4 perillas pasan a ese banco. Al cambiar de banco una perilla **no
salta**: toma el control recién cuando la mueves (un 4 %). Resonancia muestra en pantalla el banco y
qué hace cada perilla, y marca «mueve para tomar» en las que todavía no lo toman.

| Botón | Banco | POT1 | POT2 | POT3 | POT4 |
|---|---|---|---|---|---|
| **BTN1** toque | General | vol. bajo | vol. voz | vol. batería | vol. otros |
| **BTN2** | Bajo | filtro | resonancia | efecto rítmico | intensidad |
| **BTN3** | Voz | filtro | resonancia | efecto rítmico | intensidad |
| **BTN4** | Batería | filtro | resonancia | efecto rítmico | intensidad |
| **BTN5** | Otros | filtro | resonancia | efecto rítmico | intensidad |

- **BTN1 mantenido** medio segundo = play / stop; 2,5 s = **siguiente canción** desde el inicio (con una sola canción en la tarjeta, vuelve al inicio). Arranca sonando.
- **Filtro**: al centro abierto, a la izquierda pasa-bajos (hasta 110 Hz), a la derecha pasa-altos (hasta 5 kHz).
- **Resonancia** atada al corte: con el filtro casi abierto se permite poca, para que nunca quede un pico en los agudos.
- **Efecto rítmico** (zonas de la perilla, amarrado al pulso de la canción): apagado · gate 1/8 · gate 1/16 ·
  repeat 1/2 · 1/4 · 1/8 · 1/16. El gate corta en corcheas o semicorcheas; el repeat repite el comienzo de
  cada pulso. **Intensidad** = cuánto corta el gate o cuánto repeat se mezcla.
- **IMU**: no toca el sonido; inclinar gira y tiñe las visuales. **Piezos**: sólo visuales.
- Pines según el pinout fijo del repositorio: POT1–4 = GPIO 1, 2, 8, 10 · BTN1–5 = GPIO 44, 42, 0, 45, 47.

### Cada control también se ve

| Control | En la imagen |
|---|---|
| Volumen de una pista | opacidad de su capa |
| Filtro | pasa-bajos: la capa se oscurece y sus formas crecen · pasa-altos: quedan sólo líneas finas |
| Resonancia | las líneas se afinan y brillan |
| Gate | la capa se prende y apaga al mismo pulso |
| Repeat | el tiempo de la capa vuelve al comienzo del pulso: la imagen tartamudea igual que el sonido |
| Elegir un banco | la capa de ese instrumento da un destello |
| Inclinar la placa | gira y cambia de color todo |

## Lo que se mide y por qué así

- **Detector de golpes por pista** (y bombo / caja / platillo dentro de la batería): envolvente de
  1 ms de subida y 30 ms de caída contra su propia media de 300 ms, umbral relativo al golpe más
  fuerte reciente e **histéresis** (se re-arma sólo cuando la envolvente vuelve a bajar). Se ajustó
  simulándolo sobre la canción real contra la grilla de 125 BPM: la primera versión, sin
  histéresis y con umbral absoluto, contaba 2 a 4,5 «golpes» por pulso en el bajo y el bombo y
  nunca veía la caja; la actual da 0,5–1,2 por pulso, y en las secciones con batería la caja y el
  platillo caen en el tiempo y el medio tiempo.
- **El pulso no se detecta**: sale de la posición de la reproducción, el BPM y el primer pulso de
  `cancion.txt`. Es exacto.
- **Sin pasa-bajos de 13 kHz en el master**: esa regla del proyecto es para la síntesis de la
  placa, que genera aliasing. Acá las pistas ya vienen masterizadas y el techo sólo le quitaría aire.

## Arquitectura

Tres tareas, el patrón de `grabador_campo` al revés:

- **lectora** (core 0): lee la tarjeta en trozos de 16 KB a un colchón de **4 s en PSRAM**. Al
  final del archivo vuelve al principio: la canción queda en loop pegada a la muestra.
- **audio** (core 1): saca 128 cuadros del colchón, filtra, mezcla, limita y escribe al DAC. Si la
  tarjeta no alcanzó, toca silencio y el LED 0 se pone naranjo.
- **control** (core 0, 1 kHz): botones, pots, IMU, piezos, LEDs y la línea serial.

`powf` y las divisiones del filtro se calculan una vez por bloque, nunca por muestra (libm es caro
en el S3).

## Protocolo serial

Ver el encabezado del `.ino` («SALIDA SERIAL»). Una línea `R,…` cada 16 ms y una `I,…` (título,
artista, BPM, duración) cada 5 s. Los contadores de golpes sólo suben, así la app no pierde un
golpe aunque se salte una línea.

## Estado

Compila (arduino-cli, core ESP32 3.2.0: 36 % del flash, 9 % de RAM). La lógica de los detectores
se validó en el PC con la canción real. Probado en la placa: lee la tarjeta y suena, arranque verificado
por Serial (`[arranque 0..5]`).
