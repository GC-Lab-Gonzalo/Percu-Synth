# industrial_matriz_v2

La segunda versión de [`industrial_matriz`](../industrial_matriz/). El motor de sonido es el mismo,
pero ordenado en **cuatro instrumentos que se tocan por separado**, cada uno con su panel:

- **Batería**: bombo y 6 pistas.
- **Bajo**.
- **Melodías**: keys y arpegio.
- **Oscilador**.

Sobre eso hay **5 estilos**, y la salida USB alimenta a **Resonancia**: cada instrumento mueve su capa.

Las secuencias corren siempre. Callar un instrumento no lo detiene, sólo lo silencia, así que
al volver entra en tiempo. Por eso puedes dejar sonando sólo la percusión rítmica, sin melodías
(o al revés).

## Conexión

Igual que el v1:

- **Matriz de 32×8** encadenada después de los 6 LEDs de la placa (GPIO 46).
- **Fuente de 5 V propia** para la matriz, con GND común.
- **Prueba de orientación** de 3 s al encender: rojo arriba a la izquierda, verde arriba a la
  derecha, azul abajo a la izquierda, y una columna blanca que barre. Si algo no calza, se ajusta
  con las cuatro constantes `MATRIZ_*`.

Por el **USB del Arduino IDE** salen los datos para Resonancia. Compila con
**USB CDC On Boot: Enabled**.

## Cambiar de panel

| Combo | Va a |
|---|---|
| **BTN4 + BTN5** | El siguiente panel: General → Bajo → Batería → Melodías → Oscilador → General |
| **BTN3 + BTN4** | Directo al General |

Cada botón actúa en el instante en que lo aprietas. Por eso el combo **deshace** lo que alcanzó a
hacer el primero de los dos, y el segundo no hace su función.

Al cambiar de panel, cada pot queda **suelto**: no cambia nada hasta que lo muevas un 4 %. Desde
ahí, su posición es el valor. Así cambiar de panel nunca hace saltar nada, y cada panel recuerda
dónde dejaste sus pots.

**LED 5 = el panel.** Al entrar, los seis LEDs destellan en su color.

| Panel | Color del LED 5 |
|---|---|
| General | Verde |
| Bajo | Rojo |
| Batería | Ámbar |
| Melodías | Cian |
| Oscilador | Azul |

## Los paneles

### General (verde)

| | |
|---|---|
| BTN1 | Play / Stop. Arranca en el "1" |
| BTN2 | **Estilo**: el siguiente de los 5. Trae sus patrones y sus timbres |
| BTN3 | **Fill** al final del compás. Reemplaza a la batería en los pasos 13–16 del compás que suena; si ya pasó el paso 12, en el siguiente. El "1" que sigue trae un plato |
| BTN4 (mantener) | **Beat repeat**. Repite el pulso donde lo apretaste: 2 vueltas de 4 semicorcheas, 4 vueltas de 2, y después una sola semicorchea, un redoble que se cierra. Repite todo: batería, bajo y melodías. El reloj nunca se detiene, así que al soltar todo vuelve a tiempo |
| BTN5 (mantener) | **Break**. Se callan bombo y bajo, y vuelven a tiempo al soltar. **Más de 2 s** empieza la **subida** del v1: 16 compases con ruido, oscilador que sube y caja que crece, hasta un golpe con cola. El próximo toque de BTN5 es el **drop** |
| POT1 | Volumen |
| POT2 | **Tempo**, de 70 a 170 BPM |
| POT3 | **Swing**: de recto (50 %) a 70 %. La semicorchea par se alarga y la impar se acorta lo mismo, así el pulso no se mueve |
| POT4 | **Pasa-bajos general**. Arriba del 92 % queda abierto del todo |

### Bajo (rojo)

| | |
|---|---|
| BTN1 | Suena / calla |
| BTN2 | Patrón: 4 por estilo |
| BTN3 | **Tipo de bajo** (ver la tabla de abajo) |
| BTN4 | Octava arriba sí / no |
| BTN5 | **Glide** sí / no: cada nota se desliza desde la anterior (60 ms) |
| POT1 | Volumen |
| POT2 | Filtro (55 Hz – 1.6 kHz). La resonancia va atada al corte |
| POT3 | Ataque (1–150 ms) |
| POT4 | Caída (40 ms – 1.2 s) |

| Tipo | Sonido |
|---|---|
| SIERRAS | El de `bajo_8_pasos` |
| SUB | Redondo, casi sin filtro: el peso |
| ÁCIDO | Una sierra con resonancia y envolvente a la 303. Con el glide queda ácido de verdad |
| REESE | Dos sierras a ±10 cents que baten |

### Batería (ámbar)

| | |
|---|---|
| BTN1 | Suena / calla |
| BTN2 | Patrón: 4 por estilo |
| BTN3 | **Kit** (ver la tabla de abajo) |
| BTN4 | Bombo sí / no. El resto sigue |
| BTN5 (mantener) | **Media velocidad**: cada paso del patrón dura dos. Al soltar vuelve a tiempo |
| POT1 | Volumen |
| POT2 | Filtro (pasa-bajos de toda la batería, bombo incluido) |
| POT3 | **Largo** de los golpes: de ×0.5 (secos) a ×1.5 |
| POT4 | **Densidad**, la del v1: abajo del todo la fábrica calla y al subir entran golpes sin cambiar los que ya sonaban. El bombo no depende de ella |

| Kit | Sonido |
|---|---|
| INDUSTRIAL | El del v1: martillo, pistón, clap de un golpe |
| NEXO | De `drum_poder`: bombo 808 con pecho, caja afinada con bordonera, todo lo de arriba corto |
| 909 | Bombo con pegada y click, caja de 909 |
| 808 | Bombo largo y redondo, la caja grande del synthwave, shaker |
| GLITCH | Todo cortísimo y apretado |

### Melodías (cian)

| | |
|---|---|
| BTN1 | Suena / calla. El eco sigue sonando: las colas no se cortan |
| BTN2 | Patrón: 4 por estilo. Cada uno es la melodía del keys y la figura de arpegio que va con ella |
| BTN3 | **Instrumento** (ver la tabla de abajo) |
| BTN4 | Arpegio sí / no |
| BTN5 | Octava: normal → +1 → −1 |
| POT1 | Volumen |
| POT2 | Filtro (×¼ a ×4 sobre el reposo de cada nota) |
| POT3 | Ataque (3–300 ms) |
| POT4 | Caída (60 ms – 1.5 s). El arpegio la sigue, más corta |

| Instrumento | Sonido |
|---|---|
| KEYS | El del v1: pulso con el ancho respirando |
| PLUCK | Cierra rápido: pulsado y oscuro |
| LEAD | Dos sierras a ±7 cents con el filtro abierto |
| VIDRIO | Casi un seno, limpio y frío |

### Oscilador (azul)

| | |
|---|---|
| BTN1 | Encender / apagar |
| BTN2 | **Timbre**: SIERRAS (el del v1) · CUADRADAS · SUAVE |
| BTN3 (mantener) | **Killswitch**: silencio mientras lo mantienes (corte de 2 ms, sin clic) |
| BTN4 | Baja un grado de la escala |
| BTN5 | Sube un grado de la escala |
| POT1 | Volumen |
| POT2 | Filtro: dónde reposa, de 1× a 16× la nota |
| POT3 | **LFO**: cuánto mueve el filtro el LFO libre de velocidad aleatoria del v1. 0 = quieto |
| POT4 | **Delay**: envío al eco ping-pong |

El oscilador ya no tiene la altura en un pot: ahora va por **grados de la escala del estilo**, de
una octava abajo a dos arriba, con un portamento corto entre grados. Siempre suena en tono, y al
cambiar de estilo se reacomoda solo.

### Lo que elegí para los botones libres (BTN4 y BTN5)

Pediste que nunca salieran de los tiempos ni de las notas generales. Estos lo cumplen todos:

- **Octava y glide** en el bajo: la octava es la misma nota, y el glide sólo cambia cómo llega a ella.
- **Bombo sí/no y media velocidad** en la batería: la media velocidad tiene ancla y vuelve a tiempo.
- **Arpegio sí/no y octava** en las melodías.
- **Grado abajo / arriba** en el oscilador: es la única forma de darle altura sin pot, y por
  construcción no puede salir de la escala.

## Los 5 estilos

| Estilo | Tonalidad | Progresión | Kit · bajo · melodía · oscilador |
|---|---|---|---|
| INDUSTRIAL | Mi eólico | Em ×6 · C ×2 | INDUSTRIAL · SIERRAS · KEYS · SIERRAS |
| TRANCE | La eólico | Am F C G, 2 compases cada uno | NEXO · SIERRAS · PLUCK · SIERRAS |
| TECHNO | Sol frigio | Gm ×6 · A♭ ×2 | 909 · ÁCIDO · KEYS · CUADRADAS |
| SYNTHWAVE | Re dórico | Dm G Dm C | 808 · REESE · LEAD · SUAVE |
| FUTURISMO | Do lidio | C ×2 · D ×2 | GLITCH · SUB · VIDRIO · CUADRADAS |

Algunas notas sobre los estilos:

- **SYNTHWAVE** está en dórico: la sexta mayor convierte el IV en mayor, que es el color de los 80.
- **FUTURISMO** está en lidio: la cuarta aumentada y el II mayor son el color de ciencia ficción.
- Al cambiar de estilo, cada instrumento vuelve a su patrón 1 y a los timbres del estilo. Después
  los cambias en cada panel.

Todo está escrito en **grados de la escala** y **relativo al acorde que suena**: el bajo en
fundamental, tercera, quinta, séptima y octava; las melodías en grados desde la fundamental. Por
construcción no hay nota fuera de tono. En la simulación: 0 de 3954.

Cada estilo es **una fila de la tabla `ESTILOS`**, con sus patrones escritos como texto. Se edita
sin tocar código:

- **Batería**: `X` fuerte · `x` normal · `.` fantasma · `-` nada. La letra es también la
  prioridad de la densidad.
- **Bajo**: `x` fundamental · `3` · `5` · `b` · `o` octava · `-` silencio.
- **Melodía**: `1`–`9` grado desde la fundamental del acorde.
- **Arpegio**: `0`–`5` nota del acorde.

## Resonancia

60 veces por segundo sale por el USB la línea **`R`** de `mezclador_pistas` y `trance_pistas`, y
cada 5 s (o al cambiar de estilo) la línea **`I`**. Así **Resonancia** trata a la placa como una
más, con 4 pistas: `batería|bajo|melodías|oscilador`.

**Los golpes no se detectan: los cuenta el secuenciador**, así que son exactos.

| Pista | Cuenta un golpe por |
|---|---|
| Batería | Cada golpe |
| Bajo | Cada nota |
| Melodías | Cada nota del keys o del arpegio |
| Oscilador | Cada cambio de grado o encendido |

Además van aparte el **bombo**, la **caja** (caja + clap) y los **hats** (cerrado + abierto).

El resto de los campos de la línea `R`:

- **Volumen** de cada pista: 0 si está callada.
- **Filtro** de cada pista: escalado para que 0.5 sea abierto, como lo lee Resonancia.
- **Nivel real** de cada bus: el pico por bloque de audio.
- **Fase del pulso**.
- **Botones apretados**.
- **Panel**, en el campo del banco.

`Serial.setTxTimeoutMs(0)`: si nadie escucha, se descarta sin bloquear. Se manda desde la tarea
de control en el core 0, nunca desde el audio.

### MIDI o serial

La elección es **serial**, por cuatro razones:

- **El firmware ya sabe todo con exactitud**, porque cada golpe sale del secuenciador. Por serial
  va todo, a 60 Hz y sin perder nada: los contadores sólo suben, así que una línea perdida no
  pierde golpes. MIDI tendría que traducirlo a notas y CC de 7 bits.
- **Es el idioma que Resonancia ya habla**.
- **USB-MIDI cuesta el puerto serie.** Obliga a cambiar el modo USB del S3, y entonces perderías
  el puerto serie y la carga desde el IDE.
- **El DIN-5 necesita una interfaz** en el PC.

MIDI conviene sólo si un día quieres manejar Resolume o TouchDesigner sin pasar por Resonancia.

### La línea `P` (opcional)

Con `LINEA_PROYECCION 1` sale además una línea con **cada pista por separado**, para cuando
Resonancia quiera más que bombo, caja y hats. Los campos, en orden:

| # | Campo |
|---|---|
| 1 | `P` |
| 2 | ms |
| 3 | estilo |
| 4 | panel |
| 5 | tocando |
| 6 | paso (semicorcheas desde el Play) |
| 7 | BPM×10 |
| 8 | grado del acorde |
| 9 | tónica MIDI |
| 10–20 | Contadores: bombo · hat · caja · perc · clap · hat abierto · tom · bajo · keys · arpegio · oscilador |
| 21–24 | Notas MIDI: bajo · keys · arpegio · oscilador (−1 = apagado) |
| 25–29 | Niveles ×1000: batería · bajo · melodías · oscilador · master |
| 30 | Envolvente del bajo ×1000 |
| 31 | Silencios (bits: 1 batería · 2 bombo · 4 bajo · 8 melodías · 16 oscilador) |
| 32 | Break: 0 no · 1 break · 2 subida · 3 esperando el drop |
| 33 | Beat repeat |
| 34 | Fill |
| 35 | Swing ×1000 |
| 36 | Pasa-bajos general ×1000 |
| 37 | Densidad ×1000 |
| 38–40 | kit · tipo de bajo · instrumento |

Va apagada por defecto porque no sé si Resonancia ignora las líneas que no conoce.

## LEDs de la placa

- **LED 0**: bombo.
- **LED 1**: bajo.
- **LED 2**: hats.
- **LED 3**: caja, perc, clap y tom.
- **LED 4**: melodías y oscilador.

Un instrumento callado se ve en **rojo tenue**. Con break, los LEDs 0 y 1 quedan en rojo fijo; en
la subida parpadean.

LED 5 es el panel, con el pulso de cada negra. Se pone **magenta** si un bloque de audio pasa el
80 % de su tiempo.

## Sonido

Los cambios respecto del v1:

- **Buses separados.** Cada instrumento tiene volumen y silencio con fundido (~12 ms, sin clic).
  - La batería pasa por su propio **pasa-bajos estéreo**, bombo incluido.
  - El eco tiene su propio bus, fuera del volumen de las melodías. Así las colas suenan al
    callarlas, y el envío del oscilador no depende del panel de melodías.
- **Cadena del master**: suma → **pasa-bajos general** → DC → limitador con lookahead → 13 kHz →
  techo suave. Toda saturación está antes del filtro general, como pide la regla.
- **Un solo motor de percusión con kits como tablas** (`struct Golpe`: capa tonal de hasta dos
  parciales + capa de ruido con su envolvente y su pasa-banda + pasa-altos de carril). Siguen las
  reglas de `drum_poder` y del v1:
  - Arriba de 1 kHz sólo ruido filtrado.
  - Ningún parcial metálico afinado.
  - El clap es de un golpe.
  - Las cajas no se saturan.
  - Un parche por instrumento con fast-kill de 3 ms.
- **El fill reemplaza a la batería, no se suma.** Sus golpes van más cortos: un redoble de golpes
  largos es barro.
- **El volumen** de cada instrumento llega a 1.3 con el pot arriba del todo. El nivel del v1 queda
  en el 88 % del recorrido. Con 1.56 y los cuatro instrumentos al máximo, el limitador bajaba 9 dB.

## Verificado

Compilado en el PC contra mocks de `Arduino.h`, `i2s_std.h` y `FastLED.h`, y renderizado. La
prueba fueron 383 s:

- Los 5 estilos.
- En cada uno: fill, beat repeat, break, todos los botones de los cinco paneles y los combos.
- Una subida completa con su drop.
- 40 s con todos los volúmenes al máximo y los pots barridos.

Resultados:

- **0 NaN**, **0 muestras en el techo duro**.
- **0 notas fuera de tono** de 3954.
- Los combos deshacen bien el primer botón: repeat, octava, bombo, arpegio, killswitch.
- En uso normal el limitador trabaja ~1.4 dB.
- Con el volumen general y los cuatro instrumentos al máximo llega a −8.6 dB, igual que el v1 con
  el volumen a fondo.

**Todavía no está probado en la placa**, y el `.ino` no se compiló con el core del ESP32 (no hay
`arduino-cli` en este entorno). Antes de darlo por bueno:

1. Compílalo con **`-DMEDIR=1`**: arranca tocando a densidad máxima con el oscilador prendido, y
   cada 2 s imprime por Serial0 el CPU por sección.
2. Mira el **LED 5**: si se pone magenta, el problema es de tiempo y no de señal.

Respecto del v1 se sumaron cuatro filtros de bus por muestra y dos timbres con dos osciladores
(LEAD y REESE). El v1 medía 53 % promedio y 75 % en el peor caso.
