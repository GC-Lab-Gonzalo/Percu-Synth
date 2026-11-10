# triphop_viaje

Una máquina para hacer **trip hop**, y nada más que trip hop. Nace del `industrial_matriz_v2`, donde
el trip hop era uno de cinco estilos: acá los seis **dialectos** son seis variantes del mismo género
y todo el instrumento está puesto al servicio de ese sonido. La OLED es un monitor de los controles
y la matriz, efectos matemáticos que mutan al ritmo.

## Lo que lo hace trip hop y no una drum machine lenta

**El arrastre.** Cada kit trae un arrastre por pista, en milisegundos: la caja entra 8 a 30 ms
*después* del tiempo. Eso es lo que hace que un groove suene tocado por una persona. No cabe en la
rejilla de semicorcheas (a 86 BPM una semicorchea son 174 ms), así que hay un planificador de golpes
aparte: el bombo marca el tiempo exacto y el resto llega tarde, con precisión de muestra. El tempo
va de **58 a 110 BPM** (arranca en 86) y el swing llega al 72 %.

**La cinta.** Queda el **wow y flutter** (la altura derivando porque la cinta no corre perfecta) y
el **desgaste** — saturación suave más la banda recortada arriba y abajo —, los dos como constantes
fijas en el bloque `CINTA FIJA`, sin panel: son parte del timbre de la máquina. Hubo una capa de
ruido de disco y **se quitó**; por qué, más abajo.

**La armonía.** Sólo dos escalas, las que se eligieron en un test de diez (`test_escalas/`):
**menor armónica** y **frigio dominante**. La progresión no está escrita en el dialecto: **cada
cambio de dialecto (BTN2 del General) sortea una de 20 progresiones** (10 por escala), nunca la que
ya sonaba, y la OLED la muestra en grande. **Stop y Play NO cambian nada**: vuelven a lo mismo
desde el primer acorde, que es lo que hace falta para grabar una sesión otra vez (al principio
sorteaba en cada Play, y así no había cómo volver a lo que estaba sonando). Al encender sale una
al azar. Los acordes se arman apilando
terceras de la escala, con séptima, así que en la menor armónica salen Am(maj7) · Bø7 · Cmaj7#5 ·
Dm7 · E7 · Fmaj7 · G#°7, y la novena del arpegio sobre el V es la b9. Los disminuidos y el aumentado
**van a propósito**: son el color de estas dos escalas (antes se reemplazaban por la tríada
consonante más cercana, y eso las aplanaba). El pad toca el acorde completo (fundamental, tercera,
quinta y **séptima**) y las melodías son monofónicas y con mucho espacio.

| Menor armónica | | Frigio dominante | |
|---|---|---|---|
| NOIR | i · iv · ii° · V | HIJAZ | I · II · vii · I |
| LAMENTO | i · VI · iv · V | DESIERTO | I · iv · II · I |
| PEDAL | i (4) · iv · V | ESPEJISMO | I · VI+ · II · I |
| DESCENSO | i · VI · ii° · V | MEDIALUNA | I · vii · II · I |
| AGENTE | i · III+ · VI · V | OSCURO | I · v° · II · I |
| DISMINUIDO | i · vii° · i · V | CARAVANA | II · I · vii · I |
| SOMBRA | iv · V · i (4) | ZOCO | I · II (1) · I (1) · iv · II |
| VALS NEGRO | i · iv · VI · vii° | LETARGO | I (4) · II · I |
| SUSPENSO | VI · V · i (4) | ORACULO | iv · II · I (4) |
| QUIETA | i (6) · V | RITUAL | I · II · I · II (1 c/u) · vii · I |

Dos compases por acorde salvo donde dice otra cosa. NOIR y HIJAZ son las del test.

## Cantidad

8 kits de batería · 6 timbres de bajo · 6 instrumentos de armonía · 4 timbres de pad ·
6 dialectos × **12** patrones de batería, 12 de bajo y 12 de armonía con su arpegio · 20
progresiones. Todo son tablas escritas en **grados de la escala** y relativo al acorde que suena:
ningún botón puede sacar una nota de tono ni un golpe de tiempo. Los 6 patrones nuevos de cada
dialecto buscan lo que tienen las dos escalas: la sensible que vuelve a la tónica, la 2.ª aumentada,
la 2.ª menor que cae.

| Dialecto | De qué va |
|---|---|
| **BRISTOL** | Bombo pesado y quebrado, bajo sub larguísimo, Rhodes con tres notas por compás |
| **PORTIS** | El cine negro. Cinta gastada, contrabajo, clavecín, un vals roto |
| **JAZZ HOP** | Escobillas con swing, contrabajo caminando, vibráfono |
| **DUB** | 808 larguísimo, guitarra apagada al contratiempo, mucho eco, mucho silencio |
| **NOCTURNO** | Medio tiempo, bombo enorme, piano sin brillo |
| **CELESTE** | Escobillas, bajo redondo, vibráfono y coro: el que flota |

## Controles

Cinco paneles. En **todos**: **BTN4 = siguiente panel**, **BTN5 = break** (calla bombo y bajo) / vuelta
del break. Sin combos. El LED 5 muestra el panel. Al cambiar de panel cada pot queda suelto y no
cambia nada hasta que lo mueves un 4 %. Las secuencias corren siempre: el BTN1 de cada instrumento
sólo lo calla o lo deja sonar, así que al volver entra en tiempo.

Todo esto cabe en una plana para imprimir y llevar al taller: [`controles.html`](controles.html) y su [`controles_triphop_viaje.pdf`](controles_triphop_viaje.pdf).

| Panel | BTN1 | BTN2 | BTN3 | POT1 | POT2 | POT3 | POT4 |
|---|---|---|---|---|---|---|---|
| **General** (verde) | play/stop (vuelve a lo mismo) | dialecto (6) + progresión nueva | fill al final del compás | volumen | tempo 58–110 | swing (recto → 72 %) | pasa-bajos general |
| **Batería** (ámbar) | suena/calla | patrón (12) | kit (8) | volumen | filtro | largo de los golpes | densidad |
| **Bajo** (rojo) | suena/calla | patrón (12) | timbre (6) | volumen | filtro | ataque | caída |
| **Armonía** (cian) | suena/calla | patrón (12) | instrumento (6) | volumen | filtro | ataque | caída |
| **Pad** (azul) | suena/calla | timbre (4) | registro (normal · +1 · −1) | volumen | filtro | ataque y suelta | envío al eco |

- **Kits:** POLVO · BOOM BAP · 808 DUB · ESCOBILLAS · CINTA · PESADO · SECO · QUEBRADO
- **Bajos:** SUB · DUB · CONTRABAJO · PÚA · REESE · ÁCIDO
- **Armonía:** RHODES · VIBRÁFONO · PIANO POLVO · CLAVE · GUITARRA · CUERDAS
- **Pad:** CUERDAS · MELOTRÓN · CORO · VIDRIO

## Lo que se ve

**OLED 128×64 — monitor de controles.** La pantalla no adorna: dice qué está haciendo cada control.

- **Arriba**, invertida: el panel en que estás, los cinco paneles (el actual es el cuadrito lleno) y
  su estado — PLAY/STOP en el General, ON/OFF en cada instrumento, BRK parpadeando en el break.
- **Los 4 pots**, con su nombre, una barra y el valor en la unidad que de verdad mueven (BPM, %, ms,
  s, ×), calculado con las mismas fórmulas que lee el audio. Un pot **suelto** (recién cambiado de
  panel, todavía no lo tomaste) se ve **punteado** y con una **rayita donde está la perilla física**:
  hay que pasar por ahí para que mande. El pot que estás moviendo se marca.
- **Abajo**, qué hacen BTN2 y BTN3 en ese panel y qué tienen puesto (PATRÓN 3/6, KIT PESADO,
  DIALECTO PORTIS…), y los 16 pasos del compás.
- **Cada botón**, al apretarlo, muestra su valor nuevo en grande durante un segundo («KIT 6/8 ·
  PESADO», «TRANSPORTE · PLAY»).

**Matriz 32×8 — efectos matemáticos que mutan.** Ocho campos: interferencia de dos fuentes (elipses
o hipérbolas), anillos, espiral, rayos, rayas, rombos, cuadros y damero. Cada LED evalúa el campo en
su posición y los **niveles enteros** del campo son las bandas: color plano y negro entre una banda y
otra, sin degradados — por eso se ve definido.

- **Muta** cada 2 compases, siempre en el «1»: otro efecto (nunca el mismo), otra paleta (tríada ·
  complemento · vecinos · fuego y agua), otra simetría (ninguna · espejo · espejo doble).
- **Es rítmico**: el patrón no se desliza, **salta** con la rejilla (por negra, corchea o semicorchea,
  según el sorteo) y cada salto se asienta en ~35 ms. El bombo engorda las bandas, la caja gira la
  paleta un lugar y la destiñe un instante, y el acorde que suena pone el tono.
- Parado sigue andando con un reloj propio al tempo del POT2 del General.
- La energía se **normaliza en cada cuadro** (un efecto de bandas anchas no prende más que uno de
  bandas finas) y el bombo la sube un poco: así el limitador de corriente casi no actúa y el pulso
  del bombo se sigue viendo.
- Al encender, 3 s de prueba de orientación.

Antes las dos pantallas fueron un campo de estrellas y después un vuelo rasante sobre un planeta; y
en una ronda más, curvas de Lissajous, figuras de Chladni y autómatas celulares. Nada de eso quedó:
lo que se buscaba era **información** en la pantalla y en la matriz algo **rítmico, definido y que
cambie**. De todas esas pruebas sólo gustó la interferencia de ondas de la matriz, y de ahí salió
este motor.

## Grabar cada sesión

**Cada Play del General abre una toma en la microSD y su Stop la cierra**: `TRIPHOP_0001.WAV`,
`TRIPHOP_0002.WAV`… (estéreo, 16 bit, 44.1 kHz — exactamente lo que sale por el DAC), y al lado un
`TRIPHOP_0001.TXT` con el dialecto, la escala, la progresión y el tempo con que arrancó, para saber
cuál era la que salió bien. La numeración sigue lo que ya haya en la tarjeta.

- Módulo microSD por SPI con el cableado de `grabador_campo`: **SCK 14 · MOSI 15 · MISO 16 · CS 17**.
  Es opcional: sin tarjeta todo suena igual, sólo no graba, y el aviso del Play dice **SIN MICROSD**.
  La tarjeta se vuelve a probar en cada Play, así que se puede meter con la máquina encendida.
- Mientras graba, **un punto late** en la cabecera de la OLED. Si sale **hueco**, la tarjeta no
  alcanzó a escribir y se perdió un trozo de esa toma.
- **El audio nunca espera a la tarjeta**: copia cada bloque a un colchón de 8 s en PSRAM y una
  tarea en el core 0 lo vacía a la microSD. Sin PSRAM el colchón baja a 0.4 s y una tarjeta lenta
  puede perder trozos.
- El Stop graba **72 ms más**: lo que tarda el fundido de salida en llegar a silencio, así la toma
  no termina en un clic. Un Play dentro de esa cola cierra la toma anterior ahí mismo y abre la
  siguiente sin perder un bloque.
- La cabecera del WAV se reescribe cada 5 s: si se corta la luz, el archivo se abre igual.

Verificado en el simulador: dos tomas seguidas de 5 y 3 s, cabecera igual al tamaño real, audio en
las dos y la última muestra en cero exacto.

## Cargar

ESP32S3 Dev Module · **Flash Mode DIO** (con OPI/QIO el I2S suena desfasado y con chasquidos) ·
USB CDC On Boot Enabled · **PSRAM: OPI PSRAM** (el colchón de grabación). Partición: la de
siempre. Requiere FastLED.

```
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=dio,PSRAM=opi" --upload -p COM61 .
```

## Verificado por simulación (2026-10-20)

El `.ino` se compila en el PC contra mocks de `Arduino.h` / `Wire.h` / `i2s_std.h` y se mide la
señal (unos 400 s de audio: los 6 dialectos × sus 6 patrones de entonces, los 8 kits, los 22 timbres, todo al
máximo, los dos extremos de tempo y el stop):

- **0 NaN** en la salida y en los buses · **0 muestras en el techo** de 21.7 millones
- **0 notas fuera de tono** (bajo, armonía, arpegio y las cuatro voces del pad, revisadas golpe a golpe)
- Con todo al máximo: pico **1.06** antes del limitador, que baja hasta **0.77** — trabaja y no clipea
- Tempo dentro del 1 % en los dos extremos (58 y 110 BPM)
- Seis segundos después del stop: pico **0.0** — no queda nada colgado
- El arrastre entra donde tiene que entrar: 0 ms al tiro, 10/22/40 ms en el bloque que les toca
- **Prueba espectral** (la de `drum_poder`, promediando 8 golpes con distinta semilla): ninguna de
  las 56 voces de percusión tiene un tercio de octava más de **1.7 dB** sobre sus vecinos arriba de
  600 Hz — el límite de la casa son 12 dB. No hay silbidos ni metales afinados sonando a olla
- **Presupuesto de caída** por banda: las 56 voces dentro de la tabla, y los 8 bombos miden
  exactamente la tau que dice su kit (0.14 a 0.62 s, tope 0.70) con la fundamental entre 38 y 57 Hz
- La matriz entra justo en su tope de carga (9 %) con el limitador de corriente en **1.000** (no
  actúa) y la OLED dibuja entre 1009 y 1404 píxeles de los 8192

Dos bugs que la simulación encontró y que ya están arreglados, por si vuelven a aparecer en otro
firmware con pad:

- **Una voz del pad que cambia de nota no puede esperar la suelta del pot para apagarse.** Con la
  suelta larga tardaba segundos en llegar al silencio y el pad se quedaba tocando el acorde
  anterior: medido, andaba fuera de la armonía la mayor parte del tiempo. Ahora baja con un corte
  fijo de 12 ms, se reafina en el silencio y vuelve a entrar con el ataque (reafinar una voz sonando
  sería un portamento, y un pad que se desliza entre acordes suena a sirena).
- **Al cambiar de dialecto el pad se tiene que reacomodar en el acto**, no en el "1" del próximo
  compás: si no, se queda hasta 2.8 s en la tonalidad anterior mientras el bajo y la melodía ya
  cambiaron en su próxima nota.

## Dos bugs que lo hacían sonar "pegado" (2026-10-20)

Probado en el simulador, el motor corría a **la mitad del tiempo real** y se oía arrastrado. Eran dos
cosas distintas, las dos encontradas midiendo y las dos arregladas. Valen para cualquier firmware:

**1. Subnormales.** Un filtro al que le entra silencio decae hacia cero, y a mitad de camino sus
estados caen en el rango **subnormal** — ahí cada multiplicación cuesta del orden de cien veces más,
tanto en x86 como en el WASM del simulador (que no pone flush-to-zero). Medido por bloque:

| | antes | después |
|---|---|---|
| vinilo | 179 µs | **3.9 µs** |
| voz (NATURAL) | 330 µs | **3.5 µs** |
| voz (VOCODER) | 3137 µs | **17 µs** |
| suma de todas las voces | 534 µs | **30 µs** |

No era que el vocoder fuera caro: eran sus 24 biquads sentados en subnormales. Las voces musicales
no lo sufrían porque el motor que viene del v2 ya les inyecta `+1e-18f` a la entrada del filtro — lo
que faltaba era eso mismo en el vinilo, la voz y la reverb, que son código nuevo. Ahora la defensa
vive **dentro de las primitivas** (`pasoBQ` y `pasoFiltro` suman `DITHER` a su entrada) para que
ningún filtro que se agregue después se quede sin ella, y los suavizadores de un polo —que no tienen
entrada donde inyectar— se van a **cero exacto** con `sinSub()` (el cero no es subnormal).
`1e-18` es un float normal: el subnormal empieza en `1.2e-38`, y a la salida son −360 dB.

**2. El micrófono estaba en el camino crítico del audio.** El reloj de este firmware lo pone la
escritura al DAC, que bloquea mientras la cola del DMA está llena. Una lectura de I2S **también** se
marca el ritmo: no se puede leer más rápido que el tiempo real. Con las dos en serie dentro del mismo
bucle, cada bloque duraba el doble. Y encima `leerMic()` hacía **dos** lecturas por bloque, para
"descartar lo viejo si el micro va adelantado": eso tiraba a la basura audio del micrófono en la
placa y en el simulador duplicaba el problema. Ahora el micrófono tiene **su propia tarea en el core
0** que lee bloqueando y deja los bloques en un anillo de 8; la tarea de audio sólo copia y nunca
espera. Es la idea de `grabador_campo` al revés (allá la tarjeta nunca toca el audio). Medido en el
simulador después: **0 huecos de audio** (eran 200) y el peor bloque bajó de **515 %** a **97 %** de
su presupuesto, con un promedio del **9 %**.

Dos cosas que conviene saber al medir en el simulador: `ESP.getCycleCount()` ahí es reloj de pared a
240 MHz nominales, así que `-DMEDIR=1` da porcentajes reales del presupuesto **dentro del navegador**
(el camino más corto para encontrar esto); y **con el volumen en 0 el ratio audio/reloj baja a ~0.65×
en cualquier firmware** — medido en el `industrial_matriz_v2`, que va a 1.00× con volumen arriba. No
sacar conclusiones de ese número estando muteado.

## El ruido blanco que tapaba la música (2026-10-20)

Reportado: «suena más fuerte un ruido blanco de fondo que la música». No era que el disco estuviera
alto — iba **36 dB bajo la música en RMS**. Era el ruteo: el vinilo entraba **después** del
pasa-bajos general, «porque el ruido del disco está encima de la música». Con el POT4 del General a
medio camino —que es donde queda al arrancar, y donde lo deja el ADC— la música sale cortada en
~1.7 kHz, así que el disco se quedaba con **toda** la banda de arriba para él solo. Medido: la
música sola tiene −18.3 dB en 4–13 kHz y el disco le sumaba **+3 dB**; en esa zona es donde el oído
pone la presencia, y eso se oye exactamente como «el ruido está más fuerte». Ahora el disco pasa
**antes** del pasa-bajos general (que además es lo que hace un filtro sobre un disco de verdad) y
aporta **+0.0 dB** en esa banda. De paso, a niveles de disco y no de fritura: el crepitar disparaba
69 chasquidos por segundo con pico en −29 dBFS, y un vinilo con polvo tiene unos pocos ticks sueltos
sobre un siseo oscuro — densidades a un tercio, siseo de 1800–7000 Hz bajado a 900–4200, y la capa
entera de −60.8 a −69.2 dBFS. Con los pots del panel arriba vuelve a oírse como textura.

**La regla que queda:** cualquier ruido de fondo que no pase por el filtro del master termina siendo
dueño de los agudos en cuanto alguien cierra ese filtro, por muy bajo que esté en RMS.

## Por qué ya no hay capa de vinilo (2026-10-20)

El encargo original pedía «el típico sonido de vinilo, como crepitar». Se construyó, y **se quitó**:
el usuario la oía como chasquidos aleatorios y no se dejaba domar. Las cifras y el camino, porque la
lección sirve para la próxima:

- Reportó «chasquidos aleatorios; silencié instrumento por instrumento y sigue». **Ese dato era la
  respuesta**: el panel VINILO tenía su propio BTN1 y no es un instrumento, así que silenciar
  batería, bajo, armonía y pad lo dejaba sonando — y el crepitar son, por diseño, impulsos sueltos
  repartidos al azar.
- **El error de método, que es lo que hay que recordar:** se perdieron tres rondas midiendo
  **discontinuidades** (`|d2|` contra la cota del ancho de banda) y las tres dieron limpio, con
  razón: un tic de 0.2 ms salido de un pasa-banda es perfectamente suave, su `max|d2|` da 0.5× de
  la cota. Pero **«sin discontinuidad» no es «no suena a clic»**. Lo correcto es un detector de
  **transitorios audibles** — envolvente rápida de 1.5 ms contra lenta de 50 ms, contar lo que
  sobresale 4× — que es como oye el oído.
- Con ese detector se bajó de 0.60 tics/s y −49.8 dBFS a 0.20/s y −61.2 dBFS (pot cuadrático con
  recorrido real, tics como ráfagas de ~0.15 ms en vez de impulsos de una muestra, aguja apagada por
  omisión). **No alcanzó**: seguía oyéndolo. Se eliminó la capa entera.
- Medido después de quitarla, en su configuración (los cuatro pots del General al centro):
  **0.00 transitorios audibles por segundo** mientras suena, y con todo silenciado **cero absoluto**
  — 0 muestras distintas de cero en 793.800 pasados 12 s.

**Las dos reglas que quedan:** una capa cuyo trabajo es hacer chasquidos no puede ir encendida y
audible por omisión, y su control tiene que poder llevarla a inaudible (un pot con 10 dB de
recorrido no sirve). Y para juzgarla, medir transitorios audibles, nunca discontinuidades.

## Por qué ya no hay micrófono (2026-10-20)

El encargo incluía una capa de voz en vivo por el INMP441 (compuerta, eco de cinta, catedral, octava
abajo, armonizador, vocoder y teléfono, más un CONGELAR de 1.5 s), y **se quitó entera** a pedido
del usuario: «quitemos no más el mic, suena mal». Antes de sacarla se arreglaron los tres fallos
que la hacían sonar intermitente y el retraso (abajo queda el registro, porque las lecciones sirven
para cualquier firmware que lea el micrófono), pero en el simulador seguía oyéndose con un retraso
que no depende del firmware. Lo que se fue con ella: el panel VOZ (de 6 paneles a **5**), la reverb
de catedral, el vocoder de 12 bandas, el desplazador de altura, el anillo del micrófono y su tarea,
y el CONGELAR — por eso este firmware ya no necesita PSRAM y la RAM bajó de 244 KB a **188 KB**.

Si alguna vez vuelve: el código está en el historial de git, y lo que hay que respetar es lo de
abajo (el anillo nunca descarta, la compuerta con histéresis y el recorte de cola sólo en los
silencios), más la regla general: **el micrófono no puede estar en el camino crítico del audio**.

### Lo que se había arreglado: la voz sonaba intermitente

Reportado: «intenté usar el mic y suena pésimo, suena con intermitencia, con ruido». Eran tres
fallos, los tres en mi lado y los tres arreglados:

1. **El anillo del micrófono tiraba audio a la basura.** El consumidor hacía
   `if (w - r > MIC_ANILLO/2) usar = w - 1;` — en cuanto el micrófono se adelantaba cuatro bloques,
   saltaba al último y descartaba **hasta 20 ms de voz de un tajo**. Con un micrófono que entrega a
   ráfagas (el del navegador lo hace) eso salta todo el rato: **eso era la intermitencia**. Ahora es
   **un bloque por bloque, siempre, sin saltar nunca**, y el anillo pasó de 8 a 16. En una voz en
   vivo la continuidad vale más que la latencia; si el anillo se llena de verdad, el que suelta lo
   más viejo es el productor, y de a un bloque.
2. **La compuerta no tenía histéresis ni retención, y cerraba a cero.** Una voz que ronda el umbral
   la abre y la cierra sin parar, y cerrar del todo trocea entre sílabas. Ahora abre en un umbral y
   cierra en otro más bajo (0.45×), se queda abierta 220 ms tras la última sílaba, y cerrada baja a
   **−18 dB en vez de a silencio**: respira en lugar de trocear.
3. **El umbral era un número puesto a ojo** (`0.020`), y un número a ojo sólo funciona con el
   micrófono con el que se eligió. Ahora **sigue al ruido de la sala**: un seguidor de piso que baja
   rápido y sube lentísimo, y el umbral es 4× ese piso. Una pausa de un segundo ya lo recalibra.
   Además la ganancia de entrada pasa por una saturación suave, así que pasarse no rompe nada.

Verificado alimentando el micrófono **a ráfagas** (0, 1 o 2 bloques por bloque de audio, irregular),
25 s de voz:

| | |
|---|---|
| huecos dentro de las sílabas | **0** |
| cruces de la compuerta | **3.60/s** (las sílabas de la prueba van a 2.3 Hz: lo correcto es ~4.6) |
| recorrido de la compuerta | 0.146 … 1.000 (nunca llega a cero) |

Si aun así entra baja o distorsionada, el número a tocar es `GANANCIA_MIC` arriba del sketch.

### El retraso: de dónde sale cada milisegundo

Arreglar la intermitencia («nunca descartar») dejó que el anillo acumulara, y eso es **latencia**: se
habla y se oye después. El recorte va donde no se puede oír — **en los silencios**: si la compuerta
no está del todo abierta y hay cola de sobra, se consume de a dos bloques hasta dejar un colchón de
dos; y si la cola se va de 8 bloques se recorta igual, porque 25 ms de retraso molestan más que un
hueco de 3 ms. Medido con el micrófono entregando a ráfagas, 25 s:

| | antes | ahora |
|---|---|---|
| cola del anillo, media | 6.05 bloques (17.6 ms) | **2.31 bloques (6.7 ms)** |
| cola máxima | 15 (43.5 ms) | **7 (20.3 ms)** |
| huecos dentro de las sílabas | 0 | **0** |

**El presupuesto completo en la placa** ≈ 2.9–5.8 ms (DMA de entrada) + 6.7 (el anillo) + 2.9 (el
bloque) + ~8.7 (cola del DMA de salida) = **unos 21–24 ms**. Eso está en el borde de lo perceptible
y es usable para monitorear.

**En el simulador es otra cosa y no depende del firmware:** el propio HAL deja **20 ms de colchón**
en la lectura del micrófono (`mic_pos = w − rate*0.02`), y encima van el `getUserMedia` del navegador
y la salida de Web Audio — en Windows, decenas o cientos de ms cada uno. Ahí es normal oírse con
mucho retraso. La capa de voz se juzga en la placa con el INMP441, no en el navegador.

## Pendiente

- **Medir el CPU en la placa**: `-DMEDIR=1` arranca tocando al peor caso (densidad al máximo) e
  imprime por `Serial0` (el conector UART) el porcentaje del presupuesto por sección
  cada 2 s. El LED 5 se pone **magenta** medio segundo si un bloque usa más del 80 % de su tiempo.
  Es lo único que la simulación del PC no puede ver, y es justo lo que produce los chasquidos: el
  render tiene que entregar 128 muestras cada 2.9 ms o el driver mete ceros. Si va apretado, lo
  primero que sobra es el pad en CUERDAS (es el único timbre con dos sierras PolyBLEP por voz, y
  son cuatro voces).
- Probarlo en la placa con la matriz y con la pantalla puestas.
