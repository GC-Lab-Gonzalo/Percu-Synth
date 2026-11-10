# Percu Hero — PercuSynth

Juego de ritmo para los 5 botones del PercuSynth: las notas bajan por cinco carriles y hay que
apretar el botón de cada una justo cuando cruza la barra ámbar.

Hay **tres canciones**, sintetizadas en el navegador con el idioma de tres estilos de
`firmwares/matriz_oscura`:

| Canción | Estilo | Tono | BPM |
|---|---|---|---|
| «Turno de noche» | industrial: martillos y pistones en 3+3+2, bajo sucio entre los bombos | Re hijaz, un acorde | 128 |
| «Cinturón de asteroides» | psytrance: bombo corto, bajo rodante, melodía ácida | Mi frigio, un acorde | 140 |
| «Nebulosa roja» | psycore: bajo que rueda entre la tónica y la b2, melodía FM | Fa# doble armónica, Fa# ×4 · Sol ×2 · Fa# ×2 | 150 |

**La base suena sola; la melodía la tocas tú**: sólo suena si aciertas.

## Cómo se juega

1. Abre `index.html` en **Chrome o Edge de escritorio** (Web Serial).
2. La primera vez, conecta la placa por el **conector derecho** y pulsa **Instalar firmware**: el
   juego carga el control en la placa desde la misma página (ESP Web Tools), sin Arduino IDE. Es el
   mismo firmware de telemetría de Tilt Maze y Arkanoid; si ya lo tienes, sáltate este paso.
3. Pulsa **Conectar PercuSynth**, elige el puerto USB y empieza con BTN3.

| Control | PercuSynth | Teclado |
|---|---|---|
| Carriles 1 a 5 | BTN1 … BTN5 | D F G H J (o 1–5) |
| Cambiar canción (en el menú) | BTN2 / BTN4 | ↑ ↓ |
| Cambiar dificultad (en el menú) | BTN1 / BTN5 | ← → |
| Empezar / otra vez | BTN3 | Enter |
| Pausa | — | Esc |

Las notas largas se mantienen apretadas hasta que termina la cola: mientras la sostienes, suena y
suma. Cada 10 aciertos seguidos sube el multiplicador (hasta ×4); fallar o apretar donde no hay
nota lo devuelve a ×1.

**Energía.** Partes con 7 de 10 bloques. Cada nota fallada quita energía (más en DURO) y apretar
donde no hay nota quita un poco; cada acierto devuelve, más con PERFECTO y con el multiplicador
alto, así que una racha larga la recupera. En cero se acaba la partida. Los valores son las
constantes `VIDA_…` y el `golpe` de cada dificultad en `DIFS`.

**El fondo** es un viaje por la galaxia en tres formas que se turnan y se mezclan cada 4 compases
(campo `fondo` de cada sección): **A** hiperespacio de trazos, **B** planetas recortados que pasan
por los lados, **C** túnel de anillos hexagonales. La velocidad es la energía de la sección y salta
con cada bombo; los anillos giran con los martillos. En el final van los tres juntos.

**Zona difícil.** El fondo sigue la densidad de notas de cada compás: mientras más notas, más
estrellas, aparecen tres colores que el resto del viaje no tiene (rojo, turquesa, magenta), el
hiperespacio se suma aunque no le toque y cruzan asteroides cerca de la pista. En lo más denso los
bordes de la pista se ponen rojos y la pantalla golpea con el bombo. En FÁCIL hay menos notas, así
que el viaje es más tranquilo.

**Calibrar** (en el menú) toca 12 bombos: sigue el pulso con cualquier botón y el juego mide cuánto
tarda en llegar tu toque. Se guarda por separado para la placa y para el teclado. Conviene hacerlo
una vez, sobre todo con parlantes Bluetooth.

## La música

Los carriles son un teclado, grave a la izquierda:

| Carril | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| Casi toda la canción | Re | Mi♭ | Fa# | La | Re |
| Sección aguda | La | Si♭ | Do | Re | Mi♭ |

Cada canción es una fila de `CANCIONES` (tempo, escala, tónicas, carriles, kit, patrones y chart);
agregar una cuarta es agregar una fila. Las dos nuevas usan `forma()`, que arma la misma estructura
de la primera con su propio chart. Todo está escrito como texto al principio del `<script>`:

- `BAT` — la batería, 7 líneas de 16 por patrón (bombo · hat · martillo · pistón · clap · abierto · tom).
- `BAJO` — `x` fundamental, `2` la b2, `5` la quinta. Nunca cae en una negra: parte en el
  contratiempo y se va llenando hasta el bajo rodante. Es **una sola voz** que no se apaga (cada
  nota cambia la altura y redispara las envolventes); con una voz nueva por nota las colas se
  pisaban y sonaba fuera de tiempo.
- `SECCIONES` — la forma de la canción y **el chart**: un string de 16 por compás, `1`–`5` = carril,
  `=` = la nota sigue sostenida. Un compás escrito como `[línea, línea]` lleva notas dobles en la
  segunda, que sólo entran en DURO. FÁCIL deja las negras y el «2» del 3+3+2.

**La canción crece por secciones.** Cada sección de `SECCIONES` trae su energía `e` (0 a 1, o
`[desde, hasta]` si crece adentro): abre el pasa-bajos de toda la base, sube su nivel y el del bombo,
sube los hats, abre y ensucia el bajo y hunde más el sidechain. Los quiebres llevan un dron grave
(`dron`), las entradas grandes un impacto (`golpe`), y la melodía cambia de timbre con `tm` (tabla
`TMEL`: STAB pasa-banda → ACERO → FM → QUINTAS en el final). El «1» de cada compás y las notas
dobles van acentuados: más nivel y el filtro más abierto. Martillo, pistón, clap y melodía mandan a
una sala oscura; el bombo va seco.

Los timbres siguen las reglas del repo: bombo en tres bandas con su propia saturación, todo lo que
suena arriba de 1 kHz es ruido pasa-banda, sidechain del bombo sobre el resto y techo de 13 kHz.

## Técnica

- Un solo `index.html` sin build step: Canvas 2D + Web Audio + Web Serial.
- El reloj es el del audio: los pasos se programan 160 ms antes con `AudioContext.currentTime` y la
  pantalla dibuja según lo que se está **oyendo** (se descuenta `outputLatency`).
- **La melodía también va en la grilla.** Mientras vienes acertando, cada nota se programa exacta
  sobre la base; si la fallas se corta y la melodía se calla hasta el próximo acierto. El toque
  decide *si* suena, el reloj decide *cuándo*. (La primera versión disparaba la nota al apretar y
  quedaba corrida respecto de la base por la latencia de salida.)
- Ventanas de acierto: ±45 ms perfecto, ±100 ms bien.
- Protocolo: `A,accelX,…,pot4,btn1,btn2,btn3,btn4,btn5` a 115200 baudios; los botones se toman en
  el flanco.
- **Instalador incrustado.** La imagen del firmware (gestor de arranque + particiones + programa, sin
  el relleno final, 427 KB) va en base64 dentro del propio `index.html`; al cargar, la página arma
  un manifiesto de ESP Web Tools como blob que apunta a esa imagen, así sigue siendo un solo archivo
  que se abre desde el disco. El código fuente del firmware también va dentro (**Código .ino**).
  Para regenerar la imagen: `arduino-cli compile --fqbn esp32:esp32:esp32s3-octal:CDCOnBoot=cdc,FlashMode=dio
  --output-dir build <carpeta del .ino>`, quitar los `0xFF` finales de `*.merged.bin` y pegarla en
  base64 en el bloque `firmwareBin`. ESP Web Tools se baja de unpkg, así que instalar pide internet.
- `index.html?auto` hace que el juego se toque solo (para probar o mostrar).
